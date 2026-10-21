#include "GatewayLifecycleManager.h"

#include <string.h>

#include "DeviceRegistrationManager.h"
#include "LinkStateManager.h"
#include "MessageContextManager.h"

static GatewayLifecycle_Snapshot_t s_snapshot;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void GatewayLifecycleManager_SetState(
    GatewayLifecycle_State_t state,
    Gateway_TimeMs_t now_ms)
{
    if (s_snapshot.state != state)
    {
        s_snapshot.state = state;
        s_snapshot.state_since_ms = now_ms;
        ++s_snapshot.transition_count;
    }
}

static bool GatewayLifecycleManager_AreLinksAvailable(void)
{
    return LinkStateManager_IsAvailable(GATEWAY_LINK_BLUETOOTH)
        && LinkStateManager_IsAvailable(GATEWAY_LINK_DOMAIN_UART);
}

static bool GatewayLifecycleManager_AreReadyPrerequisitesMet(void)
{
    return GatewayLifecycleManager_AreLinksAvailable()
        && DeviceRegistrationManager_IsCurrentPeerAuthenticated()
        && MessageContextManager_HasActiveSession();
}

static Gateway_Status_t GatewayLifecycleManager_InvalidateSessionForRecovery(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    if (!MessageContextManager_HasActiveSession())
    {
        s_snapshot.session_reconfirmation_required = true;
        return GATEWAY_STATUS_OK;
    }

    status = MessageContextManager_InvalidateSessionAt(now_ms);

    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    s_snapshot.session_reconfirmation_required = true;

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t GatewayLifecycleManager_EnterRecoveryState(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    s_snapshot.synchronization_required = true;

    status = GatewayLifecycleManager_InvalidateSessionForRecovery(now_ms);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    GatewayLifecycleManager_SetState(
        s_snapshot.has_reached_ready
            ? GATEWAY_LIFECYCLE_DEGRADED
            : GATEWAY_LIFECYCLE_LINK_WAIT,
        now_ms);

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t GatewayLifecycleManager_CheckDependencies(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!DeviceRegistrationManager_IsInitialized()
        || !LinkStateManager_IsInitialized()
        || !MessageContextManager_IsInitialized())
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayLifecycleManager_Init(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));

    s_snapshot.initialized = true;
    s_snapshot.started = false;
    s_snapshot.state = GATEWAY_LIFECYCLE_STARTUP;
    s_snapshot.state_since_ms = Gateway_Time_GetMs();

    /*
     * READY에 들어가기 전에는 한 번의 현재 Context synchronization이 필요하다.
     */
    s_snapshot.synchronization_required = true;
    s_snapshot.session_reconfirmation_required = false;
    s_snapshot.has_reached_ready = false;
    s_snapshot.transition_count = 0U;

    return GATEWAY_STATUS_OK;
}

void GatewayLifecycleManager_Reset(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
}

bool GatewayLifecycleManager_IsInitialized(void)
{
    return s_snapshot.initialized;
}

Gateway_Status_t GatewayLifecycleManager_StartAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = GatewayLifecycleManager_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (s_snapshot.started)
    {
        return GATEWAY_STATUS_OK;
    }

    s_snapshot.started = true;

    GatewayLifecycleManager_SetState(
        GATEWAY_LIFECYCLE_LINK_WAIT,
        now_ms);

    return GatewayLifecycleManager_UpdateAt(now_ms);
}

Gateway_Status_t GatewayLifecycleManager_Start(void)
{
    return GatewayLifecycleManager_StartAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Periodic evaluation                                                        */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayLifecycleManager_UpdateAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;
    const bool links_available = GatewayLifecycleManager_AreLinksAvailable();
    const bool authenticated_peer =
        DeviceRegistrationManager_IsCurrentPeerAuthenticated();
    const bool active_session = MessageContextManager_HasActiveSession();
    const bool prerequisites_met =
        links_available && authenticated_peer && active_session;

    status = GatewayLifecycleManager_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (!s_snapshot.started)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    switch (s_snapshot.state)
    {
        case GATEWAY_LIFECYCLE_STARTUP:
            GatewayLifecycleManager_SetState(
                GATEWAY_LIFECYCLE_LINK_WAIT,
                now_ms);
            break;

        case GATEWAY_LIFECYCLE_LINK_WAIT:
            if (prerequisites_met)
            {
                s_snapshot.session_reconfirmation_required = false;
                s_snapshot.synchronization_required = true;

                GatewayLifecycleManager_SetState(
                    GATEWAY_LIFECYCLE_SYNCING,
                    now_ms);
            }
            break;

        case GATEWAY_LIFECYCLE_SYNCING:
            if (!links_available || !authenticated_peer)
            {
                return GatewayLifecycleManager_EnterRecoveryState(now_ms);
            }

            if (!active_session)
            {
                s_snapshot.synchronization_required = true;
                s_snapshot.session_reconfirmation_required = true;

                GatewayLifecycleManager_SetState(
                    s_snapshot.has_reached_ready
                        ? GATEWAY_LIFECYCLE_DEGRADED
                        : GATEWAY_LIFECYCLE_LINK_WAIT,
                    now_ms);
            }
            break;

        case GATEWAY_LIFECYCLE_READY:
            if (!links_available || !authenticated_peer || !active_session)
            {
                return GatewayLifecycleManager_EnterRecoveryState(now_ms);
            }
            break;

        case GATEWAY_LIFECYCLE_DEGRADED:
            /*
             * Recovery 후에는 기존 Session을 그대로 신뢰하지 않는다.
             * 외부 Connection/Registration logic이 Session을 다시 Activate해야 한다.
             */
            if (prerequisites_met)
            {
                s_snapshot.session_reconfirmation_required = false;
                s_snapshot.synchronization_required = true;

                GatewayLifecycleManager_SetState(
                    GATEWAY_LIFECYCLE_SYNCING,
                    now_ms);
            }
            break;

        default:
            return GATEWAY_STATUS_INTERNAL_ERROR;
    }

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t GatewayLifecycleManager_UpdateNow(void)
{
    return GatewayLifecycleManager_UpdateAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Synchronization                                                            */
/* -------------------------------------------------------------------------- */

Gateway_Status_t GatewayLifecycleManager_MarkSynchronizationCompleteAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = GatewayLifecycleManager_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (!s_snapshot.started
        || (s_snapshot.state != GATEWAY_LIFECYCLE_SYNCING))
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!GatewayLifecycleManager_AreReadyPrerequisitesMet())
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    s_snapshot.synchronization_required = false;
    s_snapshot.session_reconfirmation_required = false;
    s_snapshot.has_reached_ready = true;

    GatewayLifecycleManager_SetState(
        GATEWAY_LIFECYCLE_READY,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t GatewayLifecycleManager_MarkSynchronizationComplete(void)
{
    return GatewayLifecycleManager_MarkSynchronizationCompleteAt(
        Gateway_Time_GetMs());
}

Gateway_Status_t GatewayLifecycleManager_RequestResynchronizationAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = GatewayLifecycleManager_CheckDependencies();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (!s_snapshot.started)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    s_snapshot.synchronization_required = true;

    status = GatewayLifecycleManager_InvalidateSessionForRecovery(now_ms);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    GatewayLifecycleManager_SetState(
        s_snapshot.has_reached_ready
            ? GATEWAY_LIFECYCLE_DEGRADED
            : GATEWAY_LIFECYCLE_LINK_WAIT,
        now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t GatewayLifecycleManager_RequestResynchronization(void)
{
    return GatewayLifecycleManager_RequestResynchronizationAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

GatewayLifecycle_State_t GatewayLifecycleManager_GetState(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_LIFECYCLE_STARTUP;
    }

    return s_snapshot.state;
}

bool GatewayLifecycleManager_IsReady(void)
{
    return s_snapshot.initialized
        && s_snapshot.started
        && (s_snapshot.state == GATEWAY_LIFECYCLE_READY);
}

bool GatewayLifecycleManager_CanRelayNewControlRequest(void)
{
    return GatewayLifecycleManager_IsReady();
}

Gateway_Status_t GatewayLifecycleManager_GetSnapshot(
    GatewayLifecycle_Snapshot_t *snapshot)
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
