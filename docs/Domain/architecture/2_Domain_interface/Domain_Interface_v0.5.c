/**
 * @file Domain_Interface.c
 * @brief Domain logical interface Core Routing implementation
 *
 * ============================================================================
 * 역할
 * ============================================================================
 *
 * - Raw CAN/UART frame을 다루지 않는다.
 * - Rx logical information을 이미 존재하는 Core Manager에 반영한다.
 * - Feature/Diagnostic가 아직 소비해야 하는 Event는 Rx Observer로 전달한다.
 * - Tx는 등록된 Tx Port(Adapter/Queue)에만 전달한다.
 * - Tx Port가 없거나 enqueue가 실패한 경우 성공/완료를 꾸며내지 않는다.
 *
 * 호출 문맥:
 *   ISR에서 직접 호출하지 않는다.
 *   Domain Task 또는 serialization이 보장된 task context를 전제로 한다.
 */

#include "Domain_Interface.h"

#include <stddef.h>
#include <string.h>

#include "CommandManager.h"
#include "DomainLifecycleManager.h"
#include "Domain_Time.h"
#include "GatewaySessionManager.h"
#include "RequestManager.h"
#include "ResultManager.h"
#include "SettingsManager.h"
#include "VehicleStateManager.h"

typedef struct
{
    bool initialized;
    bool hasRxObserver;
    bool hasTxPort;

    DomainIf_RxObserver_t rxObserver;
    DomainIf_TxPort_t txPort;
} DomainIf_Context_t;

static DomainIf_Context_t g_domainIf;

/* ============================================================================
 * Internal helpers
 * ============================================================================
 */

static DomainIf_Status_t DomainIf_GetNow(
    uint32_t *outNowMs)
{
    DomainTimeStatus_t status;

    if (outNowMs == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    status = DomainTime_GetNowMs(outNowMs);

    if (status == DOMAIN_TIME_STATUS_NOT_INITIALIZED)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if (status != DOMAIN_TIME_STATUS_OK)
    {
        return DOMAIN_IF_REJECTED;
    }

    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIf_FromVsmStatus(
    VsmStatus_t status)
{
    switch (status)
    {
        case VSM_STATUS_OK:
            return DOMAIN_IF_OK;

        case VSM_STATUS_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;

        case VSM_STATUS_NOT_INITIALIZED:
        default:
            return DOMAIN_IF_NOT_INITIALIZED;
    }
}

static DomainIf_Status_t DomainIf_FromGsmStatus(
    GatewaySessionStatus_t status)
{
    switch (status)
    {
        case GSM_STATUS_OK:
            return DOMAIN_IF_OK;

        case GSM_STATUS_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;

        case GSM_STATUS_NOT_INITIALIZED:
            return DOMAIN_IF_NOT_INITIALIZED;

        case GSM_STATUS_CAPACITY_FULL:
            return DOMAIN_IF_QUEUE_FULL;

        case GSM_STATUS_NOT_FOUND:
        case GSM_STATUS_SESSION_REUSE_BLOCKED:
        default:
            return DOMAIN_IF_REJECTED;
    }
}

static DomainIf_Status_t DomainIf_FromResultStatus(
    ResultManager_Status_t status)
{
    switch (status)
    {
        case RESULT_MANAGER_STATUS_OK:
            return DOMAIN_IF_OK;

        case RESULT_MANAGER_STATUS_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;

        case RESULT_MANAGER_STATUS_NOT_INITIALIZED:
            return DOMAIN_IF_NOT_INITIALIZED;

        case RESULT_MANAGER_STATUS_COMMAND_NOT_FOUND:
        case RESULT_MANAGER_STATUS_UNTRUSTED_REPORT:
        case RESULT_MANAGER_STATUS_INCONSISTENT_REPORT:
        case RESULT_MANAGER_STATUS_INVALID_TRANSITION:
        case RESULT_MANAGER_STATUS_REQUEST_LINK_ERROR:
        default:
            return DOMAIN_IF_REJECTED;
    }
}

static bool DomainIf_MetaUsable(
    const SignalMeta_t *meta)
{
    return ((meta != NULL) &&
            (meta->quality == DATA_QUALITY_OK));
}

static bool DomainIf_ValueQualityUsable(
    const ValueQuality_t *quality)
{
    return ((quality != NULL) &&
            (quality->validity == VALUE_VALIDITY_VALID) &&
            (quality->meta.quality == DATA_QUALITY_OK));
}

static void DomainIf_UpdateLifecycleByMeta(
    DomainLifecycleSyncMask_t item,
    const SignalMeta_t *meta,
    uint32_t nowMs)
{
    if (!DomainLifecycle_IsInitialized())
    {
        return;
    }

    if (DomainIf_MetaUsable(meta))
    {
        (void)DomainLifecycle_MarkHealthy(
            item,
            nowMs);
    }
    else
    {
        (void)DomainLifecycle_MarkUnavailable(
            item,
            nowMs);
    }
}

static void DomainIf_UpdateLifecycleByQuality(
    DomainLifecycleSyncMask_t item,
    bool usable,
    uint32_t nowMs)
{
    if (!DomainLifecycle_IsInitialized())
    {
        return;
    }

    if (usable)
    {
        (void)DomainLifecycle_MarkHealthy(
            item,
            nowMs);
    }
    else
    {
        (void)DomainLifecycle_MarkUnavailable(
            item,
            nowMs);
    }
}

static DomainIf_Status_t DomainIf_NotifyMobileRequest(
    const DomainIf_MobileRequest_t *request)
{
    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onMobileRequest == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onMobileRequest(request);
}

static DomainIf_Status_t DomainIf_HandleSessionTransition(
    const GatewaySessionTransition_t *transition,
    uint32_t nowMs)
{
    RequestManager_Status_t requestStatus;
    SettingsManager_Status_t settingsStatus;

    if ((transition == NULL) ||
        (transition->type == GSM_TRANSITION_NONE))
    {
        return DOMAIN_IF_OK;
    }

    switch (transition->type)
    {
        case GSM_TRANSITION_SESSION_ACTIVATED:
            settingsStatus =
                SettingsManager_OnSessionActivated(
                    transition->deviceContextId,
                    transition->currentSessionId,
                    transition->sessionGeneration,
                    nowMs);

            if (settingsStatus != SETTINGS_STATUS_OK)
            {
                return DOMAIN_IF_REJECTED;
            }
            break;

        case GSM_TRANSITION_SESSION_CHANGED:
            /*
             * RequestManager는 old session을 execution context에서 제외.
             * SettingsManager는 new context 활성화 시 Digital Key Setting을 OFF로
             * 초기화하며 이전 context를 덮어쓴다.
             */
            if (transition->hadPreviousSession)
            {
                requestStatus =
                    RequestManager_DeactivateSession(
                        transition->deviceContextId,
                        transition->previousSessionId,
                        nowMs);

                if (requestStatus != RM_STATUS_OK)
                {
                    return DOMAIN_IF_REJECTED;
                }
            }

            settingsStatus =
                SettingsManager_OnSessionActivated(
                    transition->deviceContextId,
                    transition->currentSessionId,
                    transition->sessionGeneration,
                    nowMs);

            if (settingsStatus != SETTINGS_STATUS_OK)
            {
                return DOMAIN_IF_REJECTED;
            }
            break;

        case GSM_TRANSITION_SESSION_INVALIDATED:
            if (transition->hadPreviousSession)
            {
                requestStatus =
                    RequestManager_DeactivateSession(
                        transition->deviceContextId,
                        transition->previousSessionId,
                        nowMs);

                if (requestStatus != RM_STATUS_OK)
                {
                    return DOMAIN_IF_REJECTED;
                }

                settingsStatus =
                    SettingsManager_OnSessionInvalidated(
                        transition->deviceContextId,
                        transition->previousSessionId,
                        transition->sessionGeneration,
                        nowMs);

                if ((settingsStatus != SETTINGS_STATUS_OK) &&
                    (settingsStatus != SETTINGS_STATUS_NOT_FOUND))
                {
                    return DOMAIN_IF_REJECTED;
                }
            }
            break;

        case GSM_TRANSITION_SESSION_REUSE_REJECTED:
            /*
             * 새 context를 만들지 않는다.
             * retired session을 새 실행 context로 복원하지 않음.
             */
            break;

        case GSM_TRANSITION_NONE:
        default:
            break;
    }

    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIf_ValidatePassiveSession(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId)
{
    RequestContext_t context;
    GatewayRequestValidation_t validation;

    context.sessionId = sessionId;
    context.requestId = 0U;

    validation = GatewaySessionManager_ValidateRequest(
        deviceContextId,
        &context);

    if (validation != GSM_REQUEST_VALID)
    {
        return DOMAIN_IF_REJECTED;
    }

    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIf_MarkCommandDispatched(
    DomainCommandId_t commandId,
    DomainIf_Status_t txStatus,
    uint32_t nowMs)
{
    CommandManager_Status_t commandStatus;

    if (txStatus != DOMAIN_IF_OK)
    {
        return txStatus;
    }

    commandStatus = CommandManager_MarkDispatched(
        commandId,
        nowMs);

    if (commandStatus == COMMAND_MANAGER_STATUS_OK)
    {
        return DOMAIN_IF_OK;
    }

    if (commandStatus ==
        COMMAND_MANAGER_STATUS_NOT_INITIALIZED)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    return DOMAIN_IF_REJECTED;
}

/* ============================================================================
 * Init / Port registration
 * ============================================================================
 */

DomainIf_Status_t DomainIf_Init(void)
{
    /*
     * Domain_Interface는 Core manager 위의 routing layer.
     * 이 시점에 필수 dependency가 초기화되어 있어야 한다.
     */
    if ((!DomainTime_IsInitialized()) ||
        (!VehicleStateManager_IsInitialized()) ||
        (!GatewaySessionManager_IsInitialized()) ||
        (!RequestManager_IsInitialized()) ||
        (!SettingsManager_IsInitialized()) ||
        (!CommandManager_IsInitialized()) ||
        (!ResultManager_IsInitialized()))
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    (void)memset(
        &g_domainIf,
        0,
        sizeof(g_domainIf));

    g_domainIf.initialized = true;

    return DOMAIN_IF_OK;
}

bool DomainIf_IsInitialized(void)
{
    return g_domainIf.initialized;
}

DomainIf_Status_t DomainIf_SetRxObserver(
    const DomainIf_RxObserver_t *observer)
{
    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if (observer == NULL)
    {
        (void)memset(
            &g_domainIf.rxObserver,
            0,
            sizeof(g_domainIf.rxObserver));
        g_domainIf.hasRxObserver = false;
        return DOMAIN_IF_OK;
    }

    g_domainIf.rxObserver = *observer;
    g_domainIf.hasRxObserver = true;

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_SetTxPort(
    const DomainIf_TxPort_t *port)
{
    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if (port == NULL)
    {
        (void)memset(
            &g_domainIf.txPort,
            0,
            sizeof(g_domainIf.txPort));
        g_domainIf.hasTxPort = false;
        return DOMAIN_IF_OK;
    }

    g_domainIf.txPort = *port;
    g_domainIf.hasTxPort = true;

    return DOMAIN_IF_OK;
}

/* ============================================================================
 * Rx: ESP32
 * ============================================================================
 */

DomainIf_Status_t DomainIf_RxDigitalKeyConnection(
    DeviceContextId_t deviceContextId,
    const DigitalKeyConnectionState_t *connection)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;
    GatewaySessionStatus_t gsmStatus;
    GatewaySessionTransition_t transition;

    if (connection == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateDigitalKeyConnection(
            connection,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    gsmStatus =
        GatewaySessionManager_UpdateConnection(
            deviceContextId,
            connection,
            nowMs,
            &transition);

    /*
     * GSM이 동일 retired session 재사용을 차단하더라도
     * Connection snapshot 자체는 VSM에 보존된다.
     */
    if ((gsmStatus != GSM_STATUS_OK) &&
        (gsmStatus != GSM_STATUS_SESSION_REUSE_BLOCKED))
    {
        return DomainIf_FromGsmStatus(gsmStatus);
    }

    status = DomainIf_HandleSessionTransition(
        &transition,
        nowMs);

    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_ESP32_CONNECTION,
        &connection->meta,
        nowMs);

    if (gsmStatus == GSM_STATUS_SESSION_REUSE_BLOCKED)
    {
        return DOMAIN_IF_REJECTED;
    }

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxProximity(
    DeviceContextId_t deviceContextId,
    const ProximityInput_t *proximity)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    (void)deviceContextId;

    if (proximity == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateProximity(
            proximity,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_ESP32_PROXIMITY,
        &proximity->meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxMobileRequest(
    const DomainIf_MobileRequest_t *request)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    GatewayRequestValidation_t validation;
    RequestManager_RegisterInfo_t registerInfo;
    RequestManager_Status_t requestStatus;

    if (request == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    validation =
        GatewaySessionManager_ValidateRequest(
            request->deviceContextId,
            &request->context);

    if (validation != GSM_REQUEST_VALID)
    {
        return DOMAIN_IF_REJECTED;
    }

    requestStatus =
        RequestManager_Register(
            request,
            nowMs,
            &registerInfo);

    if (requestStatus == RM_STATUS_CAPACITY_FULL)
    {
        return DOMAIN_IF_QUEUE_FULL;
    }

    if ((requestStatus == RM_STATUS_ID_CONFLICT) ||
        (requestStatus == RM_STATUS_STALE_REQUEST))
    {
        return DOMAIN_IF_REJECTED;
    }

    if (requestStatus != RM_STATUS_OK)
    {
        return DOMAIN_IF_REJECTED;
    }

    /*
     * 동일 Request 재전달은 새 실행으로 전달하지 않는다.
     * 기존 상태/결과 재응답은 향후 MobileStateMapper/DomainTask가 수행한다.
     */
    if (registerInfo.registerResult ==
        RM_REGISTER_DUPLICATE_SAME)
    {
        return DOMAIN_IF_OK;
    }

    if (registerInfo.registerResult !=
        RM_REGISTER_NEW)
    {
        return DOMAIN_IF_REJECTED;
    }

    return DomainIf_NotifyMobileRequest(request);
}

DomainIf_Status_t DomainIf_RxStateQuery(
    const DomainIf_StateQuery_t *query)
{
    DomainIf_Status_t status;

    if (query == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_ValidatePassiveSession(
        query->deviceContextId,
        query->sessionId);

    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onStateQuery == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onStateQuery(query);
}

DomainIf_Status_t DomainIf_RxWarningAck(
    const DomainIf_WarningAck_t *ack)
{
    DomainIf_Status_t status;

    if (ack == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_ValidatePassiveSession(
        ack->deviceContextId,
        ack->sessionId);

    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onWarningAck == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onWarningAck(ack);
}

/* ============================================================================
 * Rx: BCM
 * ============================================================================
 */

DomainIf_Status_t DomainIf_RxBcmEcuState(
    CommonEcuState_t state,
    const SignalMeta_t *meta)
{
    if (meta == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onBcmEcuState == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onBcmEcuState(
        state,
        meta);
}

DomainIf_Status_t DomainIf_RxBcmDoorState(
    const DoorState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateDoor(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_BCM_DOOR,
        &state->meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxBcmClimateState(
    const ClimateState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateClimate(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_BCM_CLIMATE,
        &state->meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxBcmInteriorLightState(
    const InteriorLightState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateInteriorLight(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_BCM_INTERIOR_LIGHT,
        &state->meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxBcmEvent(
    const DomainIf_BcmEvent_t *event)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    ResultManager_Output_t resultOutput;
    ResultManager_Status_t resultStatus;
    bool isCommandResultEvent = false;

    if (event == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    switch (event->eventType)
    {
        case BCM_EVENT_LOCK_COMPLETED:
        case BCM_EVENT_UNLOCK_COMPLETED:
        case BCM_EVENT_ALREADY_AT_TARGET:
        case BCM_EVENT_REQUEST_REJECTED:
        case BCM_EVENT_REQUEST_FAILED:
            isCommandResultEvent = true;
            break;

        case BCM_EVENT_OVERHEAT_DETECTED:
        case BCM_EVENT_FAN_MISMATCH_DETECTED:
        case BCM_EVENT_RECOVERY_CONFIRMED:
        default:
            break;
    }

    if (isCommandResultEvent)
    {
        status = DomainIf_GetNow(&nowMs);
        if (status != DOMAIN_IF_OK)
        {
            return status;
        }

        resultStatus =
            ResultManager_ProcessBcmEvent(
                event,
                nowMs,
                &resultOutput);

        if (resultStatus != RESULT_MANAGER_STATUS_OK)
        {
            return DomainIf_FromResultStatus(resultStatus);
        }
    }

    /*
     * ResultManager에서 core 상태를 반영한 뒤 Feature/Diagnostic observer에도
     * 원 event를 전달한다.
     *
     * DigitalKeyManager는 ALREADY_AT_TARGET와 실제 UNLOCK_COMPLETED를
     * eventType으로 구분할 수 있다.
     */
    if (g_domainIf.hasRxObserver &&
        (g_domainIf.rxObserver.onBcmEvent != NULL))
    {
        return g_domainIf.rxObserver.onBcmEvent(event);
    }

    return isCommandResultEvent ?
        DOMAIN_IF_OK :
        DOMAIN_IF_NOT_AVAILABLE;
}

DomainIf_Status_t DomainIf_RxBcmFault(
    const FaultInfo_t *fault)
{
    if (fault == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onBcmFault == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onBcmFault(fault);
}

/* ============================================================================
 * Rx: CIS
 * ============================================================================
 */

DomainIf_Status_t DomainIf_RxCisStatus(
    const DomainIf_CisStatusReport_t *status)
{
    if (status == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onCisStatus == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onCisStatus(status);
}

DomainIf_Status_t DomainIf_RxCisOccupant(
    const CisOccupantState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateCisOccupant(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByQuality(
        DLM_SYNC_CIS_OCCUPANT,
        DomainIf_ValueQualityUsable(
            &state->quality),
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxCisCabinEnvironment(
    const CisCabinEnvironmentState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;
    bool usable;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateCisCabin(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    usable =
        DomainIf_ValueQualityUsable(
            &state->temperatureQuality) &&
        DomainIf_ValueQualityUsable(
            &state->humidityQuality) &&
        DomainIf_ValueQualityUsable(
            &state->illuminanceQuality);

    DomainIf_UpdateLifecycleByQuality(
        DLM_SYNC_CIS_CABIN,
        usable,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxCisRear(
    const CisRearState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateCisRear(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByQuality(
        DLM_SYNC_CIS_REAR,
        DomainIf_ValueQualityUsable(
            &state->distanceQuality),
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxCisFault(
    const FaultInfo_t *fault)
{
    if (fault == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onCisFault == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onCisFault(fault);
}

/* ============================================================================
 * Rx: WINDOW
 * ============================================================================
 */

DomainIf_Status_t DomainIf_RxWindowState(
    const WindowState_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateWindow(
            state,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_WINDOW_STATE,
        &state->meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxWindowCommandResult(
    const DomainIf_WindowCommandResult_t *result)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    ResultManager_Output_t resultOutput;
    ResultManager_Status_t resultStatus;

    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    resultStatus =
        ResultManager_ProcessWindowResult(
            result,
            nowMs,
            &resultOutput);

    if (resultStatus != RESULT_MANAGER_STATUS_OK)
    {
        return DomainIf_FromResultStatus(resultStatus);
    }

    if (g_domainIf.hasRxObserver &&
        (g_domainIf.rxObserver.onWindowCommandResult != NULL))
    {
        return g_domainIf.rxObserver.onWindowCommandResult(
            result);
    }

    /*
     * Request-linked result는 ResultManager가 이미 RequestManager까지 반영한다.
     * Job-linked result는 Feature가 polling 또는 추후 observer로 소비 가능.
     */
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxWindowAntiPinch(
    const DomainIf_AntiPinchReport_t *report)
{
    if (report == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onWindowAntiPinch == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onWindowAntiPinch(
        report);
}

DomainIf_Status_t DomainIf_RxWindowFault(
    const FaultInfo_t *fault)
{
    if (fault == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onWindowFault == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onWindowFault(fault);
}

/* ============================================================================
 * Rx: VSS
 * ============================================================================
 */

DomainIf_Status_t DomainIf_RxVssState(
    const DomainIf_VssStateReport_t *state)
{
    uint32_t nowMs;
    DomainIf_Status_t status;
    VsmStatus_t vsmStatus;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIf_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    vsmStatus =
        VehicleStateManager_UpdateVss(
            &state->service,
            nowMs);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DomainIf_FromVsmStatus(vsmStatus);
    }

    DomainIf_UpdateLifecycleByMeta(
        DLM_SYNC_VSS_STATE,
        &state->service.meta,
        nowMs);

    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxVssFault(
    const FaultInfo_t *fault)
{
    if (fault == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasRxObserver) ||
        (g_domainIf.rxObserver.onVssFault == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.rxObserver.onVssFault(fault);
}

/* ============================================================================
 * Tx: Domain -> External
 * ============================================================================
 */

DomainIf_Status_t DomainIf_TxBcmDoorCommand(
    const DomainIf_BcmDoorCommand_t *command)
{
    uint32_t nowMs;
    DomainIf_Status_t status;

    if (command == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txBcmDoorCommand == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    status = g_domainIf.txPort.txBcmDoorCommand(command);

    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if (DomainIf_GetNow(&nowMs) != DOMAIN_IF_OK)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    return DomainIf_MarkCommandDispatched(
        command->context.commandId,
        DOMAIN_IF_OK,
        nowMs);
}

DomainIf_Status_t DomainIf_TxBcmClimateCommand(
    const DomainIf_BcmClimateCommand_t *command)
{
    uint32_t nowMs;
    DomainIf_Status_t status;

    if (command == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txBcmClimateCommand == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    status = g_domainIf.txPort.txBcmClimateCommand(command);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if (DomainIf_GetNow(&nowMs) != DOMAIN_IF_OK)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    return DomainIf_MarkCommandDispatched(
        command->context.commandId,
        DOMAIN_IF_OK,
        nowMs);
}

DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand(
    const DomainIf_BcmInteriorLightCommand_t *command)
{
    uint32_t nowMs;
    DomainIf_Status_t status;

    if (command == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txBcmInteriorLightCommand == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    status =
        g_domainIf.txPort.txBcmInteriorLightCommand(command);

    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if (DomainIf_GetNow(&nowMs) != DOMAIN_IF_OK)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    return DomainIf_MarkCommandDispatched(
        command->context.commandId,
        DOMAIN_IF_OK,
        nowMs);
}

DomainIf_Status_t DomainIf_TxCisPowerPermission(
    const DomainIf_CisPowerPermission_t *permission)
{
    if (permission == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txCisPowerPermission == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txCisPowerPermission(
        permission);
}

DomainIf_Status_t DomainIf_TxWindowCommand(
    const DomainIf_WindowCommand_t *command)
{
    uint32_t nowMs;
    DomainIf_Status_t status;

    if (command == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txWindowCommand == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    status = g_domainIf.txPort.txWindowCommand(command);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if (DomainIf_GetNow(&nowMs) != DOMAIN_IF_OK)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    return DomainIf_MarkCommandDispatched(
        command->context.commandId,
        DOMAIN_IF_OK,
        nowMs);
}

DomainIf_Status_t DomainIf_TxWindowOperationPermission(
    const DomainIf_WindowOperationPermission_t *permission)
{
    if (permission == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txWindowOperationPermission == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txWindowOperationPermission(
        permission);
}

DomainIf_Status_t DomainIf_TxWindowDomainAlive(
    const DomainIf_WindowDomainAlive_t *alive)
{
    if (alive == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txWindowDomainAlive == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txWindowDomainAlive(alive);
}

DomainIf_Status_t DomainIf_TxVssOneShotEvent(
    const DomainIf_VssOneShotEvent_t *event)
{
    if (event == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txVssOneShotEvent == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txVssOneShotEvent(event);
}

DomainIf_Status_t DomainIf_TxVssWarningState(
    const DomainIf_VssWarningState_t *state)
{
    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txVssWarningState == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txVssWarningState(state);
}

DomainIf_Status_t DomainIf_TxMobileRequestResult(
    const RequestResultInfo_t *result)
{
    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileRequestResult == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileRequestResult(result);
}

DomainIf_Status_t DomainIf_TxMobileVehicleState(
    const VehicleState_t *state)
{
    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileVehicleState == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileVehicleState(state);
}

DomainIf_Status_t DomainIf_TxMobileWarning(
    const DomainIf_WarningInfo_t *warning)
{
    if (warning == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileWarning == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileWarning(warning);
}

DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(
    const DomainIf_FunctionAvailability_t *availability)
{
    if (availability == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileFunctionAvailability == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileFunctionAvailability(
        availability);
}

DomainIf_Status_t DomainIf_TxMobileDigitalKeyState(
    const DigitalKeyState_t *state)
{
    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileDigitalKeyState == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileDigitalKeyState(state);
}

DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult(
    const DomainIf_DigitalKeyResult_t *result)
{
    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }

    if (!g_domainIf.initialized)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    if ((!g_domainIf.hasTxPort) ||
        (g_domainIf.txPort.txMobileDigitalKeyResult == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    return g_domainIf.txPort.txMobileDigitalKeyResult(result);
}
