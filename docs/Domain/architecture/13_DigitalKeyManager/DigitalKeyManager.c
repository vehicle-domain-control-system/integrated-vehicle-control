/**
 * @file DigitalKeyManager.c
 */

#include "DigitalKeyManager.h"

#include <stddef.h>
#include <string.h>

#include "DomainLifecycleManager.h"
#include "Domain_PolicyConfig.h"
#include "GatewaySessionManager.h"
#include "SettingsManager.h"
#include "VehicleStateManager.h"

typedef struct
{
    bool initialized;
    DigitalKeyManager_Config_t config;
    DigitalKeyManager_Snapshot_t snapshot;
} DigitalKeyManager_Context_t;

static DigitalKeyManager_Context_t g_digitalKey;

static void Dkm_ClearProcessOutput(
    DigitalKeyManager_ProcessOutput_t *outResult)
{
    (void)memset(outResult, 0, sizeof(*outResult));
    outResult->reason = DIGITAL_KEY_DECISION_NONE;
    outResult->txStatus = DOMAIN_IF_NOT_AVAILABLE;
    outResult->permission.decision = PERMISSION_DECISION_DENY;
    outResult->permission.reason = PERMISSION_REASON_NONE;
}

static void Dkm_ClearResultOutput(
    DigitalKeyManager_ResultOutput_t *outResult)
{
    (void)memset(outResult, 0, sizeof(*outResult));
    outResult->mobileResult.confirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;
    outResult->mobileResult.origin =
        UNLOCK_ORIGIN_PROXIMITY_AUTO;
}

static void Dkm_ResetApproach(void)
{
    g_digitalKey.snapshot.approachState =
        DIGITAL_KEY_APPROACH_WAIT_FAR;
}

static bool Dkm_ProximityIsTrusted(
    const ProximityInput_t *proximity)
{
    return ((proximity != NULL) &&
            (proximity->meta.quality == DATA_QUALITY_OK) &&
            (proximity->state != PROXIMITY_UNKNOWN));
}

static bool Dkm_DoorIsTrusted(
    const DoorState_t *door)
{
    return ((door != NULL) &&
            (door->meta.quality == DATA_QUALITY_OK) &&
            (door->lockState != DOOR_LOCK_STATE_UNKNOWN) &&
            (door->compositeState == DOOR_COMPOSITE_STATE_NORMAL));
}

static DataQuality_t Dkm_CombineRequiredQuality(
    const DoorState_t *door,
    const ProximityInput_t *proximity)
{
    if ((door == NULL) || (proximity == NULL))
    {
        return DATA_QUALITY_NO_DATA;
    }

    if ((door->meta.quality == DATA_QUALITY_NO_DATA) ||
        (proximity->meta.quality == DATA_QUALITY_NO_DATA))
    {
        return DATA_QUALITY_NO_DATA;
    }

    if ((door->meta.quality == DATA_QUALITY_INVALID) ||
        (proximity->meta.quality == DATA_QUALITY_INVALID))
    {
        return DATA_QUALITY_INVALID;
    }

    if ((door->meta.quality == DATA_QUALITY_STALE) ||
        (proximity->meta.quality == DATA_QUALITY_STALE))
    {
        return DATA_QUALITY_STALE;
    }

    return DATA_QUALITY_OK;
}

static void Dkm_UpdateVehicleState(
    DigitalKeySetting_t setting)
{
    DigitalKeyState_t state;

    state.setting = setting;
    state.availability = g_digitalKey.snapshot.availability;
    state.lastUnlockOrigin =
        g_digitalKey.snapshot.hasActualUnlockHistory ?
        g_digitalKey.snapshot.lastUnlockOrigin :
        UNLOCK_ORIGIN_USER_REQUEST;

    (void)VehicleStateManager_UpdateDigitalKeyState(&state);
}

static DigitalKeyManager_Status_t Dkm_LoadActiveContext(
    DeviceContextId_t deviceContextId,
    GatewaySessionRecord_t *outSession,
    DigitalKeyUserSetting_t *outSetting,
    DigitalKeyInput_t *outInput,
    DoorState_t *outDoor)
{
    GatewaySessionStatus_t gsmStatus;
    SettingsManager_Status_t settingStatus;
    VsmStatus_t vsmStatus;

    gsmStatus =
        GatewaySessionManager_GetRecord(
            deviceContextId,
            outSession);

    if (gsmStatus != GSM_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    settingStatus =
        SettingsManager_GetDigitalKey(
            deviceContextId,
            outSetting);

    if (settingStatus != SETTINGS_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    vsmStatus =
        VehicleStateManager_GetDigitalKeyInput(
            outInput);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    vsmStatus =
        VehicleStateManager_GetDoor(outDoor);

    if (vsmStatus != VSM_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

static bool Dkm_CurrentContextMatches(
    DeviceContextId_t deviceContextId,
    const GatewaySessionRecord_t *session,
    const DigitalKeyUserSetting_t *setting)
{
    return ((session != NULL) &&
            (setting != NULL) &&
            session->hasActiveSession &&
            (session->sessionState == GSM_SESSION_STATE_ACTIVE) &&
            setting->contextActive &&
            (setting->deviceContextId == deviceContextId) &&
            (setting->sessionId == session->activeSessionId) &&
            (setting->sessionGeneration == session->sessionGeneration));
}

static void Dkm_AdoptContext(
    DeviceContextId_t deviceContextId,
    const GatewaySessionRecord_t *session,
    const DigitalKeyUserSetting_t *setting)
{
    bool changed;

    changed =
        (!g_digitalKey.snapshot.contextKnown) ||
        (g_digitalKey.snapshot.deviceContextId != deviceContextId) ||
        (g_digitalKey.snapshot.sessionId != session->activeSessionId) ||
        (g_digitalKey.snapshot.sessionGeneration !=
         session->sessionGeneration);

    if (changed)
    {
        g_digitalKey.snapshot.contextKnown = true;
        g_digitalKey.snapshot.deviceContextId = deviceContextId;
        g_digitalKey.snapshot.sessionId = session->activeSessionId;
        g_digitalKey.snapshot.sessionGeneration =
            session->sessionGeneration;
        g_digitalKey.snapshot.observedSettingRevision =
            setting->revision;
        Dkm_ResetApproach();
    }
}

static DigitalKeyManager_Status_t Dkm_HandlePendingTimeout(
    uint32_t nowMs,
    DigitalKeyManager_ProcessOutput_t *outResult)
{
    const DomainPolicyConfig_t *policy;
    CommandRecord_t command;
    CommandManager_Status_t commandStatus;
    ResultManager_Output_t unconfirmed;

    if (!g_digitalKey.snapshot.hasPendingCommand)
    {
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    policy = DomainPolicyConfig_Get();

    if (policy == NULL)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    if ((uint32_t)(nowMs -
        g_digitalKey.snapshot.commandIssuedAtMs) <
        policy->doorResultWaitMs.value)
    {
        outResult->reason =
            DIGITAL_KEY_DECISION_COMMAND_ALREADY_PENDING;
        outResult->commandId =
            g_digitalKey.snapshot.pendingCommandId;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    commandStatus = CommandManager_GetRecord(
        g_digitalKey.snapshot.pendingCommandId,
        &command);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_COMMAND_ERROR;
    }

    if (command.lifecycle == COMMAND_LIFECYCLE_FINAL)
    {
        /*
         * 정상 final event가 observer에 도달하기 전 Process가 먼저 불린 경우.
         * Event consumer가 pending을 정리하도록 여기서는 유지한다.
         */
        outResult->reason =
            DIGITAL_KEY_DECISION_COMMAND_ALREADY_PENDING;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    (void)ResultManager_MarkCommandUnconfirmed(
        g_digitalKey.snapshot.pendingCommandId,
        nowMs,
        &unconfirmed);

    g_digitalKey.snapshot.hasPendingCommand = false;
    outResult->reason =
        DIGITAL_KEY_DECISION_RESULT_TIMEOUT;
    outResult->commandId = command.commandId;

    /*
     * 같은 접근에서는 새 자동 요청하지 않는다.
     * approachState는 CONSUMED 유지.
     */

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

static DigitalKeyManager_Status_t Dkm_CreateUnlockCommand(
    const ProximityInput_t *proximity,
    uint32_t nowMs,
    DigitalKeyManager_ProcessOutput_t *outResult)
{
    CommandCreateRequest_t create;
    CommandCreateResult_t created;
    DomainIf_BcmDoorCommand_t command;
    CommandManager_Status_t commandStatus;
    DomainIf_Status_t txStatus;

    (void)memset(&create, 0, sizeof(create));
    (void)memset(&created, 0, sizeof(created));
    (void)memset(&command, 0, sizeof(command));

    create.target = COMMAND_TARGET_BCM;
    create.functionId = DOMAIN_FUNCTION_DOOR;
    create.type = COMMAND_TYPE_BCM_DOOR;
    create.origin = COMMAND_ORIGIN_DIGITAL_KEY;
    create.sourceMeta = proximity->meta;

    commandStatus =
        CommandManager_Create(
            &create,
            nowMs,
            &created);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_COMMAND_ERROR;
    }

    outResult->commandCreated = true;
    outResult->commandId = created.commandId;

    command.context = created.interfaceContext;
    command.target = DOOR_TARGET_UNLOCK;

    /*
     * Command를 만들기로 결정한 순간 이 접근은 소비한다.
     * Tx/BCM 결과가 실패해도 같은 접근에서 새 Command를 생성하지 않는다.
     */
    g_digitalKey.snapshot.approachState =
        DIGITAL_KEY_APPROACH_CONSUMED;

    txStatus = DomainIf_TxBcmDoorCommand(&command);
    outResult->txStatus = txStatus;

    if (txStatus != DOMAIN_IF_OK)
    {
        (void)CommandManager_AbortBeforeDispatch(
            created.commandId,
            REQUEST_RESULT_FAILED,
            RESULT_REASON_OTHER,
            nowMs);

        outResult->reason =
            DIGITAL_KEY_DECISION_UNLOCK_COMMAND_DISPATCH_FAILED;
        outResult->commandDispatched = false;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    g_digitalKey.snapshot.hasPendingCommand = true;
    g_digitalKey.snapshot.pendingCommandId =
        created.commandId;
    g_digitalKey.snapshot.commandIssuedAtMs = nowMs;

    outResult->reason =
        DIGITAL_KEY_DECISION_UNLOCK_COMMAND_DISPATCHED;
    outResult->commandDispatched = true;

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

void DigitalKeyManager_LoadCurrentProjectDefaults(
    DigitalKeyManager_Config_t *outConfig)
{
    if (outConfig == NULL)
    {
        return;
    }

    outConfig->supported = true;

    /*
     * DiagnosticManager 구현 전 초기 통합 기본값.
     * Session/State/Quality gate가 별도로 존재한다.
     */
    outConfig->initialAvailability =
        FUNCTION_AVAILABILITY_AVAILABLE;

    outConfig->degradedPolicy =
        PERMISSION_DEGRADED_DENY;
}

DigitalKeyManager_Status_t DigitalKeyManager_Init(
    const DigitalKeyManager_Config_t *config)
{
    DigitalKeyManager_Config_t defaults;

    if ((!VehicleStateManager_IsInitialized()) ||
        (!GatewaySessionManager_IsInitialized()) ||
        (!SettingsManager_IsInitialized()) ||
        (!PermissionManager_IsInitialized()) ||
        (!CommandManager_IsInitialized()) ||
        (!ResultManager_IsInitialized()) ||
        (DomainPolicyConfig_Get() == NULL))
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    (void)memset(
        &g_digitalKey,
        0,
        sizeof(g_digitalKey));

    if (config == NULL)
    {
        DigitalKeyManager_LoadCurrentProjectDefaults(
            &defaults);
        config = &defaults;
    }

    g_digitalKey.config = *config;
    g_digitalKey.snapshot.availability =
        config->initialAvailability;
    g_digitalKey.snapshot.setting =
        DIGITAL_KEY_SETTING_OFF;
    g_digitalKey.snapshot.appActive =
        APP_ACTIVE_UNKNOWN;
    g_digitalKey.snapshot.lastProximity =
        PROXIMITY_UNKNOWN;
    g_digitalKey.snapshot.approachState =
        DIGITAL_KEY_APPROACH_WAIT_FAR;

    g_digitalKey.initialized = true;

    Dkm_UpdateVehicleState(
        DIGITAL_KEY_SETTING_OFF);

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

bool DigitalKeyManager_IsInitialized(void)
{
    return g_digitalKey.initialized;
}

DigitalKeyManager_Status_t DigitalKeyManager_SetAvailability(
    FunctionAvailability_t availability)
{
    if (!g_digitalKey.initialized)
    {
        return DIGITAL_KEY_MANAGER_STATUS_NOT_INITIALIZED;
    }

    g_digitalKey.snapshot.availability = availability;

    /*
     * 기능이 사용할 수 없는 동안 관측한 proximity를
     * 새 접근 baseline으로 재사용하지 않는다.
     */
    if ((availability != FUNCTION_AVAILABILITY_AVAILABLE) &&
        !((availability == FUNCTION_AVAILABILITY_DEGRADED) &&
          (g_digitalKey.config.degradedPolicy ==
           PERMISSION_DEGRADED_ALLOW)))
    {
        Dkm_ResetApproach();
    }

    Dkm_UpdateVehicleState(
        g_digitalKey.snapshot.setting);

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

DigitalKeyManager_Status_t DigitalKeyManager_Process(
    DeviceContextId_t deviceContextId,
    uint32_t nowMs,
    DigitalKeyManager_ProcessOutput_t *outResult)
{
    GatewaySessionRecord_t session;
    DigitalKeyUserSetting_t setting;
    DigitalKeyInput_t input;
    DoorState_t door;
    DigitalKeyManager_Status_t loadStatus;
    PermissionEvaluation_t evaluation;
    PermissionResult_t permission;
    bool sessionValid;
    bool featureAvailableForBaseline;
    bool settingRevisionChanged;

    if (outResult == NULL)
    {
        return DIGITAL_KEY_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_digitalKey.initialized)
    {
        return DIGITAL_KEY_MANAGER_STATUS_NOT_INITIALIZED;
    }

    Dkm_ClearProcessOutput(outResult);

    loadStatus = Dkm_LoadActiveContext(
        deviceContextId,
        &session,
        &setting,
        &input,
        &door);

    if (loadStatus != DIGITAL_KEY_MANAGER_STATUS_OK)
    {
        g_digitalKey.snapshot.setting =
            DIGITAL_KEY_SETTING_OFF;
        g_digitalKey.snapshot.appActive =
            APP_ACTIVE_UNKNOWN;
        Dkm_ResetApproach();
        Dkm_UpdateVehicleState(
            DIGITAL_KEY_SETTING_OFF);

        outResult->reason =
            DIGITAL_KEY_DECISION_NO_ACTIVE_SESSION;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    sessionValid =
        Dkm_CurrentContextMatches(
            deviceContextId,
            &session,
            &setting);

    if (!sessionValid)
    {
        g_digitalKey.snapshot.setting =
            DIGITAL_KEY_SETTING_OFF;
        g_digitalKey.snapshot.appActive =
            session.connection.appActive;
        Dkm_ResetApproach();
        Dkm_UpdateVehicleState(
            DIGITAL_KEY_SETTING_OFF);

        outResult->reason =
            DIGITAL_KEY_DECISION_NO_ACTIVE_SESSION;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    Dkm_AdoptContext(
        deviceContextId,
        &session,
        &setting);

    settingRevisionChanged =
        (g_digitalKey.snapshot.observedSettingRevision !=
         setting.revision);

    if (settingRevisionChanged)
    {
        /*
         * OFF->ON, ON->OFF 모두 새 baseline 필요.
         * 특히 ON 직후 이미 NEAR인 상태에서 unlock하지 않는다.
         */
        g_digitalKey.snapshot.observedSettingRevision =
            setting.revision;
        Dkm_ResetApproach();
    }

    g_digitalKey.snapshot.setting =
        setting.setting;
    g_digitalKey.snapshot.appActive =
        session.connection.appActive;
    g_digitalKey.snapshot.lastProximity =
        input.proximity.state;

    Dkm_UpdateVehicleState(setting.setting);

    /* Pending command timeout is independent from whether current scope remains active. */
    if (g_digitalKey.snapshot.hasPendingCommand)
    {
        DigitalKeyManager_Status_t timeoutStatus =
            Dkm_HandlePendingTimeout(
                nowMs,
                outResult);

        if ((timeoutStatus != DIGITAL_KEY_MANAGER_STATUS_OK) ||
            (outResult->reason ==
             DIGITAL_KEY_DECISION_RESULT_TIMEOUT) ||
            g_digitalKey.snapshot.hasPendingCommand)
        {
            return timeoutStatus;
        }
    }

    if (setting.setting != DIGITAL_KEY_SETTING_ON)
    {
        Dkm_ResetApproach();
        outResult->reason =
            DIGITAL_KEY_DECISION_SETTING_OFF;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (session.connection.appActive !=
        APP_ACTIVE_ACTIVE)
    {
        Dkm_ResetApproach();
        outResult->reason =
            DIGITAL_KEY_DECISION_APP_NOT_ACTIVE;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    featureAvailableForBaseline =
        (g_digitalKey.snapshot.availability ==
         FUNCTION_AVAILABILITY_AVAILABLE) ||
        ((g_digitalKey.snapshot.availability ==
          FUNCTION_AVAILABILITY_DEGRADED) &&
         (g_digitalKey.config.degradedPolicy ==
          PERMISSION_DEGRADED_ALLOW));

    if ((!g_digitalKey.config.supported) ||
        (!featureAvailableForBaseline))
    {
        Dkm_ResetApproach();
        outResult->reason =
            DIGITAL_KEY_DECISION_FEATURE_UNAVAILABLE;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (!Dkm_ProximityIsTrusted(
            &input.proximity))
    {
        /*
         * STALE / INVALID / NO_DATA / UNKNOWN은 FAR가 아니다.
         * 이전 FAR baseline도 보수적으로 재사용하지 않는다.
         */
        Dkm_ResetApproach();
        outResult->reason =
            DIGITAL_KEY_DECISION_PROXIMITY_UNTRUSTED;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (input.proximity.state == PROXIMITY_FAR)
    {
        g_digitalKey.snapshot.approachState =
            DIGITAL_KEY_APPROACH_ARMED_FAR;

        outResult->reason =
            DIGITAL_KEY_DECISION_FAR_ARMED;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    /* From here proximity is trusted NEAR. */
    if (g_digitalKey.snapshot.approachState ==
        DIGITAL_KEY_APPROACH_WAIT_FAR)
    {
        outResult->reason =
            DIGITAL_KEY_DECISION_WAITING_VALID_FAR;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (g_digitalKey.snapshot.approachState ==
        DIGITAL_KEY_APPROACH_ARMED_FAR)
    {
        g_digitalKey.snapshot.approachState =
            DIGITAL_KEY_APPROACH_ACTIVE;
        outResult->reason =
            DIGITAL_KEY_DECISION_NEW_APPROACH;
        /*
         * 같은 call에서 아래 실행 조건까지 계속 평가한다.
         */
    }

    if (g_digitalKey.snapshot.approachState ==
        DIGITAL_KEY_APPROACH_CONSUMED)
    {
        outResult->reason =
            DIGITAL_KEY_DECISION_COMMAND_ALREADY_PENDING;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (!Dkm_DoorIsTrusted(&door))
    {
        outResult->reason =
            DIGITAL_KEY_DECISION_DOOR_UNTRUSTED;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    if (door.lockState == DOOR_LOCK_STATE_UNLOCKED)
    {
        /*
         * 이 접근은 이미 unlocked 상태에서 시작.
         * 이후 같은 접근에서 수동 lock되어도 자동 재해제하지 않는다.
         */
        g_digitalKey.snapshot.approachState =
            DIGITAL_KEY_APPROACH_CONSUMED;

        outResult->reason =
            DIGITAL_KEY_DECISION_ALREADY_UNLOCKED;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    (void)memset(
        &evaluation,
        0,
        sizeof(evaluation));

    evaluation.requirements =
        PERMISSION_REQUIRE_DOMAIN_OPERATIONAL |
        PERMISSION_REQUIRE_SESSION_VALID |
        PERMISSION_REQUIRE_FUNCTION_SUPPORTED |
        PERMISSION_REQUIRE_FUNCTION_AVAILABLE |
        PERMISSION_REQUIRE_INPUT_QUALITY;

    evaluation.facts.lifecycleState =
        DOMAIN_LIFECYCLE_READY;

    {
        DomainLifecycleSnapshot_t lifecycle;

        if (DomainLifecycle_GetSnapshot(
                &lifecycle) == DLM_STATUS_OK)
        {
            evaluation.facts.lifecycleState =
                lifecycle.state;
        }
    }

    evaluation.facts.sessionValid = true;
    evaluation.facts.functionSupported =
        g_digitalKey.config.supported;
    evaluation.facts.functionAvailability =
        g_digitalKey.snapshot.availability;
    evaluation.facts.degradedPolicy =
        g_digitalKey.config.degradedPolicy;
    evaluation.facts.requiredInputQuality =
        Dkm_CombineRequiredQuality(
            &door,
            &input.proximity);

    if (PermissionManager_Evaluate(
            &evaluation,
            &permission) != PERMISSION_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_DEPENDENCY_NOT_READY;
    }

    outResult->permission = permission;

    if (permission.decision !=
        PERMISSION_DECISION_ALLOW)
    {
        outResult->reason =
            DIGITAL_KEY_DECISION_PERMISSION_DENIED;
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    return Dkm_CreateUnlockCommand(
        &input.proximity,
        nowMs,
        outResult);
}

DigitalKeyManager_Status_t DigitalKeyManager_OnBcmEvent(
    const DomainIf_BcmEvent_t *event,
    uint32_t nowMs,
    DigitalKeyManager_ResultOutput_t *outResult)
{
    CommandRecord_t command;
    CommandManager_Status_t commandStatus;

    (void)nowMs;

    if ((event == NULL) || (outResult == NULL))
    {
        return DIGITAL_KEY_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_digitalKey.initialized)
    {
        return DIGITAL_KEY_MANAGER_STATUS_NOT_INITIALIZED;
    }

    Dkm_ClearResultOutput(outResult);

    /*
     * 같은 NEAR 접근 중 어떤 LOCK 완료가 발생해도
     * 실제 FAR를 다시 보기 전 자동 재해제하지 않는다.
     */
    if ((event->eventType == BCM_EVENT_LOCK_COMPLETED) &&
        (g_digitalKey.snapshot.lastProximity ==
         PROXIMITY_NEAR))
    {
        g_digitalKey.snapshot.approachState =
            DIGITAL_KEY_APPROACH_CONSUMED;
    }

    if ((!g_digitalKey.snapshot.hasPendingCommand) ||
        (event->commandId !=
         g_digitalKey.snapshot.pendingCommandId))
    {
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    commandStatus =
        CommandManager_GetRecord(
            event->commandId,
            &command);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return DIGITAL_KEY_MANAGER_STATUS_COMMAND_ERROR;
    }

    if (command.origin != COMMAND_ORIGIN_DIGITAL_KEY)
    {
        return DIGITAL_KEY_MANAGER_STATUS_OK;
    }

    switch (event->eventType)
    {
        case BCM_EVENT_UNLOCK_COMPLETED:
            if (event->result == REQUEST_RESULT_DONE)
            {
                outResult->relevant = true;
                outResult->mobileResult.hasVehicleResult = true;
                outResult->mobileResult.result =
                    REQUEST_RESULT_DONE;
                outResult->mobileResult.reason =
                    event->reason;
                outResult->mobileResult.confirmation =
                    RESULT_CONFIRMATION_CONFIRMED;
                outResult->mobileResult.relatedDoorCommandId =
                    event->commandId;
                outResult->mobileResult.actualUnlockTransition =
                    true;

                g_digitalKey.snapshot.hasActualUnlockHistory = true;
                g_digitalKey.snapshot.lastUnlockOrigin =
                    UNLOCK_ORIGIN_PROXIMITY_AUTO;
                g_digitalKey.snapshot.lastCompletedCommandId =
                    event->commandId;
                g_digitalKey.snapshot.hasPendingCommand = false;

                Dkm_UpdateVehicleState(
                    g_digitalKey.snapshot.setting);
            }
            break;

        case BCM_EVENT_ALREADY_AT_TARGET:
            if (event->result == REQUEST_RESULT_DONE)
            {
                outResult->relevant = true;
                outResult->mobileResult.hasVehicleResult = true;
                outResult->mobileResult.result =
                    REQUEST_RESULT_DONE;
                outResult->mobileResult.reason =
                    event->reason;
                outResult->mobileResult.confirmation =
                    RESULT_CONFIRMATION_CONFIRMED;
                outResult->mobileResult.relatedDoorCommandId =
                    event->commandId;
                outResult->mobileResult.actualUnlockTransition =
                    false;

                /*
                 * 실제 unlock history/origin은 갱신하지 않는다.
                 */
                g_digitalKey.snapshot.lastCompletedCommandId =
                    event->commandId;
                g_digitalKey.snapshot.hasPendingCommand = false;
            }
            break;

        case BCM_EVENT_REQUEST_REJECTED:
        case BCM_EVENT_REQUEST_FAILED:
            outResult->relevant = true;
            outResult->mobileResult.hasVehicleResult = true;
            outResult->mobileResult.result =
                event->result;
            outResult->mobileResult.reason =
                event->reason;
            outResult->mobileResult.confirmation =
                RESULT_CONFIRMATION_CONFIRMED;
            outResult->mobileResult.relatedDoorCommandId =
                event->commandId;
            outResult->mobileResult.actualUnlockTransition =
                false;

            g_digitalKey.snapshot.lastCompletedCommandId =
                event->commandId;
            g_digitalKey.snapshot.hasPendingCommand = false;
            break;

        case BCM_EVENT_LOCK_COMPLETED:
        case BCM_EVENT_OVERHEAT_DETECTED:
        case BCM_EVENT_FAN_MISMATCH_DETECTED:
        case BCM_EVENT_RECOVERY_CONFIRMED:
        default:
            break;
    }

    return DIGITAL_KEY_MANAGER_STATUS_OK;
}

DigitalKeyManager_Status_t DigitalKeyManager_GetSnapshot(
    DigitalKeyManager_Snapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
    {
        return DIGITAL_KEY_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_digitalKey.initialized)
    {
        return DIGITAL_KEY_MANAGER_STATUS_NOT_INITIALIZED;
    }

    *outSnapshot = g_digitalKey.snapshot;
    return DIGITAL_KEY_MANAGER_STATUS_OK;
}
