#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "GatewayLifecycleManager.h"
#include "Gateway_Interface.h"
#include "LinkStateManager.h"
#include "MessageContextManager.h"

static Gateway_ByteView_t make_view(
    const uint8_t *data,
    size_t length)
{
    Gateway_ByteView_t view = {
        .data = (length > 0U) ? data : NULL,
        .length = length
    };

    return view;
}

static Gateway_MessageContextView_t make_session(
    const uint8_t *vehicle,
    size_t vehicle_len,
    const uint8_t *device,
    size_t device_len,
    const uint8_t *session,
    size_t session_len)
{
    Gateway_MessageContextView_t context = {
        .vehicle_id = make_view(vehicle, vehicle_len),
        .device_context_id = make_view(device, device_len),
        .session_id = make_view(session, session_len),
        .request_id = { .data = NULL, .length = 0U }
    };

    return context;
}

static void reset_dependencies(void)
{
    GatewayLifecycleManager_Reset();
    LinkStateManager_Reset();
    MessageContextManager_Reset();

    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);
    assert(MessageContextManager_Init() == GATEWAY_STATUS_OK);
    assert(GatewayLifecycleManager_Init() == GATEWAY_STATUS_OK);
}

static void make_links_available(void)
{
    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            100U)
        == GATEWAY_STATUS_OK);
}

static void test_initial_state(void)
{
    GatewayLifecycle_Snapshot_t snapshot;

    GatewayLifecycleManager_Reset();

    assert(!GatewayLifecycleManager_IsInitialized());

    assert(
        GatewayLifecycleManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_NOT_READY);

    reset_dependencies();

    assert(GatewayLifecycleManager_IsInitialized());
    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_STARTUP);
    assert(!GatewayLifecycleManager_IsReady());
    assert(!GatewayLifecycleManager_CanRelayNewControlRequest());
}

static void test_start_without_links_waits(void)
{
    reset_dependencies();

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_LINK_WAIT);
}

static void test_links_and_session_enter_syncing(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_SYNCING);

    assert(!GatewayLifecycleManager_IsReady());
}

static void test_sync_complete_enters_ready(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_READY);

    assert(GatewayLifecycleManager_IsReady());
    assert(GatewayLifecycleManager_CanRelayNewControlRequest());
}

static void test_ready_link_loss_invalidates_session(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    GatewayLifecycle_Snapshot_t snapshot;

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkUnavailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            130U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(130U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_DEGRADED);

    assert(!MessageContextManager_HasActiveSession());
    assert(!GatewayLifecycleManager_CanRelayNewControlRequest());

    assert(
        GatewayLifecycleManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.synchronization_required);
    assert(snapshot.session_reconfirmation_required);
    assert(snapshot.has_reached_ready);
}

static void test_link_recovery_requires_session_reconfirmation(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkUnavailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            130U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(130U)
        == GATEWAY_STATUS_OK);

    /* UART만 복구해도 Session은 invalidated 상태라 READY/SYNCING 복귀 안 함 */
    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_DOMAIN_UART,
            200U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(200U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_DEGRADED);

    /* 현재 Connection/Session을 다시 확인한 뒤 Activate */
    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            210U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(210U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_SYNCING);

    assert(!GatewayLifecycleManager_IsReady());

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            220U)
        == GATEWAY_STATUS_OK);

    assert(GatewayLifecycleManager_IsReady());
}

static void test_resync_request_invalidates_current_session(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_RequestResynchronizationAt(
            130U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_DEGRADED);

    assert(!MessageContextManager_HasActiveSession());
    assert(!GatewayLifecycleManager_IsReady());
}

static void test_sync_complete_requires_syncing_and_prerequisites(void)
{
    reset_dependencies();

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_MarkSynchronizationCompleteAt(
            110U)
        == GATEWAY_STATUS_NOT_READY);
}

static void test_loss_during_initial_sync_returns_link_wait(void)
{
    static const uint8_t vehicle[] = {0x01U};
    static const uint8_t session[] = {0x10U};

    const Gateway_MessageContextView_t context = make_session(
        vehicle, sizeof(vehicle),
        NULL, 0U,
        session, sizeof(session));

    reset_dependencies();
    make_links_available();

    assert(
        MessageContextManager_ActivateSessionAt(
            &context,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_StartAt(110U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_SYNCING);

    assert(
        LinkStateManager_MarkUnavailableAt(
            GATEWAY_LINK_BLUETOOTH,
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_UpdateAt(120U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetState()
        == GATEWAY_LIFECYCLE_LINK_WAIT);

    assert(!MessageContextManager_HasActiveSession());
}

static void test_snapshot_transition_count(void)
{
    GatewayLifecycle_Snapshot_t snapshot;

    reset_dependencies();

    assert(
        GatewayLifecycleManager_StartAt(100U)
        == GATEWAY_STATUS_OK);

    assert(
        GatewayLifecycleManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.started);
    assert(snapshot.transition_count >= 1U);
}

int main(void)
{
    test_initial_state();
    test_start_without_links_waits();
    test_links_and_session_enter_syncing();
    test_sync_complete_enters_ready();
    test_ready_link_loss_invalidates_session();
    test_link_recovery_requires_session_reconfirmation();
    test_resync_request_invalidates_current_session();
    test_sync_complete_requires_syncing_and_prerequisites();
    test_loss_during_initial_sync_returns_link_wait();
    test_snapshot_transition_count();

    return 0;
}
