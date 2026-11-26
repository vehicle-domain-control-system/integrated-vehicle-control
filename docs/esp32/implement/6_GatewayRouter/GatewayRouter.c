#include "GatewayRouter.h"

#include <string.h>

#include "DeviceRegistrationManager.h"
#include "GatewayLifecycleManager.h"
#include "LinkStateManager.h"
#include "MessageContextManager.h"

static GatewayRouter_Snapshot_t s_snapshot;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static Gateway_Status_t GatewayRouter_CheckDependencies(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!DeviceRegistrationManager_IsInitialized()
        || !GatewayLifecycleManager_IsInitialized()
        || !LinkStateManager_IsInitialized()
        || !MessageContextManager_IsInitialized()
        || !Gateway_Interface_IsInitialized())
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    return GATEWAY_STATUS_OK;
}

static void GatewayRouter_RecordInvalid(void)
{
    ++s_snapshot.rejected_invalid;
}

static void GatewayRouter_RecordNotReady(void)
{
    ++s_snapshot.rejected_not_ready;
}

static Gateway_Status_t GatewayRouter_CaptureCurrentContext(
    const Gateway_MessageContextView_t *context,
    MessageContextManager_OwnedContext_t *owned,
    Gateway_MessageContextView_t *owned_view)
{
    Gateway_Status_t status;

    status = MessageContextManager_CaptureForCurrentSession(
        context,
        owned);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = MessageContextManager_MakeView(
        owned,
        owned_view);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayRouter_Init(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.initialized = true;

    return GATEWAY_STATUS_OK;
}

void GatewayRouter_Reset(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
}

bool GatewayRouter_IsInitialized(void)
{
    return s_snapshot.initialized;
}

Gateway_Status_t GatewayRouter_GetSnapshot(
    GatewayRouter_Snapshot_t *snapshot)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (snapshot == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    *snapshot = s_snapshot;

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* MOBILE -> Domain                                                           */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayRouter_OnMobileMessage(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    MessageContextManager_OwnedContext_t owned_context;
    Gateway_MessageContextView_t owned_view;
    Gateway_RelayMessageView_t forwarded_message;
    Gateway_Status_t status;

    (void)user_context;

    status = GatewayRouter_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if ((message == NULL)
        || !Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_MOBILE_TO_DOMAIN))
    {
        GatewayRouter_RecordInvalid();
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    /*
     * MOBILE request/query/ack는 현재 Vehicle/Device/Session Context와
     * 일치해야 한다.
     *
     * Router가 Session을 자동 생성하지 않는다.
     */
    if (!MessageContextManager_IsViewCurrentSession(&message->context))
    {
        GatewayRouter_RecordNotReady();
        return GATEWAY_STATUS_NOT_READY;
    }

    /*
     * 실제 차량 제어 Request는 Gateway Lifecycle이 READY일 때만 허용한다.
     *
     * STATE_QUERY / WARNING_ACK 같은 비제어 논리 메시지는 Sync/Recovery에
     * 필요할 수 있으므로 여기서 READY gate를 강제하지 않는다.
     */
    if (message->type == GATEWAY_LOGICAL_MSG_MOBILE_REQUEST)
    {
        /*
         * "등록된 peer + 현재 연결"은 차량 수준의 실행 허용 판단이 아니라
         * MOBILE 출처/현재 연결을 확인하기 위한 Gateway-side gate다.
         *
         * Domain은 이후 기능 지원, 차량 상태, 실행 허용 조건을 별도로
         * 판단한다.
         */
        if (!DeviceRegistrationManager_IsCurrentPeerAuthenticated()
            || !GatewayLifecycleManager_CanRelayNewControlRequest())
        {
            GatewayRouter_RecordNotReady();
            return GATEWAY_STATUS_NOT_READY;
        }
    }

    /*
     * UART Path가 현재 사용 가능하지 않다면 메시지를 보관하지 않는다.
     * 복구 후 자동 Replay도 하지 않는다.
     */
    if (!LinkStateManager_IsAvailable(GATEWAY_LINK_DOMAIN_UART))
    {
        GatewayRouter_RecordNotReady();
        return GATEWAY_STATUS_NOT_READY;
    }

    status = GatewayRouter_CaptureCurrentContext(
        &message->context,
        &owned_context,
        &owned_view);
    if (status != GATEWAY_STATUS_OK)
    {
        if (status == GATEWAY_STATUS_INVALID_ARGUMENT)
        {
            GatewayRouter_RecordInvalid();
        }
        else
        {
            GatewayRouter_RecordNotReady();
        }

        return status;
    }

    forwarded_message = *message;
    forwarded_message.context = owned_view;

    status = Gateway_Interface_SendToDomain(&forwarded_message);

    if (status == GATEWAY_STATUS_OK)
    {
        ++s_snapshot.mobile_to_domain_forwarded;
    }
    else if (status == GATEWAY_STATUS_INVALID_ARGUMENT)
    {
        GatewayRouter_RecordInvalid();
    }
    else
    {
        GatewayRouter_RecordNotReady();
    }

    return status;
}


/* -------------------------------------------------------------------------- */
/* Domain -> MOBILE                                                           */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayRouter_OnDomainMessage(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    MessageContextManager_OwnedContext_t owned_context;
    Gateway_MessageContextView_t owned_view;
    Gateway_RelayMessageView_t forwarded_message;
    Gateway_Status_t status;

    (void)user_context;

    status = GatewayRouter_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if ((message == NULL)
        || !Gateway_Interface_IsValidRelayMessage(message)
        || (message->direction != GATEWAY_DIRECTION_DOMAIN_TO_MOBILE))
    {
        GatewayRouter_RecordInvalid();
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    /*
     * 이전 Session/연결 구간의 지연 Result/State/Warning을 현재 MOBILE
     * 메시지로 그대로 중계하지 않는다.
     */
    if (!MessageContextManager_IsViewCurrentSession(&message->context))
    {
        GatewayRouter_RecordNotReady();
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!LinkStateManager_IsAvailable(GATEWAY_LINK_BLUETOOTH))
    {
        GatewayRouter_RecordNotReady();
        return GATEWAY_STATUS_NOT_READY;
    }

    status = GatewayRouter_CaptureCurrentContext(
        &message->context,
        &owned_context,
        &owned_view);
    if (status != GATEWAY_STATUS_OK)
    {
        if (status == GATEWAY_STATUS_INVALID_ARGUMENT)
        {
            GatewayRouter_RecordInvalid();
        }
        else
        {
            GatewayRouter_RecordNotReady();
        }

        return status;
    }

    forwarded_message = *message;
    forwarded_message.context = owned_view;

    status = Gateway_Interface_SendToMobile(&forwarded_message);

    if (status == GATEWAY_STATUS_OK)
    {
        ++s_snapshot.domain_to_mobile_forwarded;
    }
    else if (status == GATEWAY_STATUS_INVALID_ARGUMENT)
    {
        GatewayRouter_RecordInvalid();
    }
    else
    {
        GatewayRouter_RecordNotReady();
    }

    return status;
}


/* -------------------------------------------------------------------------- */
/* Helper                                                                     */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayRouter_BuildCoreHandlers(
    Gateway_InterfaceCoreHandlers_t *handlers)
{
    if (handlers == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    handlers->on_mobile_message = GatewayRouter_OnMobileMessage;
    handlers->on_domain_message = GatewayRouter_OnDomainMessage;
    handlers->user_context = NULL;

    return GATEWAY_STATUS_OK;
}
