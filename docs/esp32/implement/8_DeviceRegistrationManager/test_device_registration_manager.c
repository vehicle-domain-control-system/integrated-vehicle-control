#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "DeviceRegistrationManager.h"

typedef struct
{
    int publish_calls;

    Gateway_RegistrationState_t registration;
    Gateway_ConnectionState_t connection;
    Gateway_DataQuality_t quality;
    Gateway_TimeMs_t age_ms;
    bool is_new_update;

    uint8_t device_ref[DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES];
    size_t device_ref_length;
} RegistrationTestTransport_t;

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

static Gateway_MessageContextView_t empty_context(void)
{
    Gateway_MessageContextView_t context = {
        .vehicle_id = { .data = NULL, .length = 0U },
        .device_context_id = { .data = NULL, .length = 0U },
        .session_id = { .data = NULL, .length = 0U },
        .request_id = { .data = NULL, .length = 0U }
    };

    return context;
}

static Gateway_Status_t capture_registration(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context)
{
    RegistrationTestTransport_t *transport =
        (RegistrationTestTransport_t *)user_context;

    assert(update != NULL);

    transport->publish_calls++;
    transport->registration = update->registration;
    transport->connection = update->connection;
    transport->quality = update->update.quality;
    transport->age_ms = update->update.age_ms;
    transport->is_new_update = update->update.is_new_update;

    transport->device_ref_length =
        update->context.device_context_id.length;

    if (transport->device_ref_length > 0U)
    {
        assert(
            transport->device_ref_length
            <= sizeof(transport->device_ref));

        memcpy(
            transport->device_ref,
            update->context.device_context_id.data,
            transport->device_ref_length);
    }

    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_relay_sender(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    (void)message;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_app_sender(
    const Gateway_AppActivityUpdate_t *update,
    void *user_context)
{
    (void)update;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_proximity_sender(
    const Gateway_ProximityUpdate_t *update,
    void *user_context)
{
    (void)update;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static void setup_interface(
    RegistrationTestTransport_t *transport)
{
    Gateway_InterfaceCoreHandlers_t core_handlers = {0};
    Gateway_InterfaceTransportPorts_t ports = {
        .send_to_domain = unused_relay_sender,
        .send_to_mobile = unused_relay_sender,
        .send_registration_to_domain = capture_registration,
        .send_app_activity_to_domain = unused_app_sender,
        .send_proximity_to_domain = unused_proximity_sender,
        .user_context = transport
    };

    Gateway_Interface_Reset();
    memset(transport, 0, sizeof(*transport));

    assert(
        Gateway_Interface_Init(
            &core_handlers,
            &ports)
        == GATEWAY_STATUS_OK);
}

static void test_initial_state(void)
{
    DeviceRegistrationManager_Snapshot_t snapshot;

    DeviceRegistrationManager_Reset();

    assert(!DeviceRegistrationManager_IsInitialized());

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_NOT_READY);

    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetCurrentRegistrationState()
        == GATEWAY_REGISTRATION_UNKNOWN);

    assert(
        DeviceRegistrationManager_GetConnectionState()
        == GATEWAY_CONNECTION_UNKNOWN);

    assert(!DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_registered_but_disconnected(void)
{
    uint8_t peer[] = {0x10U, 0x11U};
    Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Snapshot_t snapshot;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.has_registered_peer);
    assert(snapshot.registered_peer.length == sizeof(peer));
    assert(snapshot.registered_peer.data[0] == 0x10U);

    assert(
        snapshot.current_registration
        == GATEWAY_REGISTERED);

    /*
     * 등록 이벤트만으로 현재 Bluetooth 연결을 만들지 않는다.
     */
    assert(
        snapshot.connection
        == GATEWAY_CONNECTION_UNKNOWN);

    assert(!DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_registered_peer_connected_is_authenticated(void)
{
    static const uint8_t peer[] = {0x20U, 0x21U};
    const Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &peer_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetCurrentRegistrationState()
        == GATEWAY_REGISTERED);

    assert(
        DeviceRegistrationManager_GetConnectionState()
        == GATEWAY_CONNECTED);

    assert(DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_other_connected_peer_is_not_registered(void)
{
    static const uint8_t registered_peer[] = {0x30U};
    static const uint8_t other_peer[] = {0x40U};

    const Gateway_ByteView_t registered_view =
        make_view(registered_peer, sizeof(registered_peer));

    const Gateway_ByteView_t other_view =
        make_view(other_peer, sizeof(other_peer));

    DeviceRegistrationManager_Snapshot_t snapshot;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &registered_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &other_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.has_registered_peer);
    assert(snapshot.has_current_peer);

    assert(
        snapshot.current_registration
        == GATEWAY_NOT_REGISTERED);

    assert(snapshot.connection == GATEWAY_CONNECTED);
    assert(!DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_input_buffer_is_copied(void)
{
    uint8_t peer[] = {0x50U, 0x51U};
    Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Snapshot_t snapshot;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    peer[0] = 0xFFU;

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.registered_peer.data[0] == 0x50U);
}

static void test_disconnect_does_not_delete_registration(void)
{
    static const uint8_t peer[] = {0x60U};
    const Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Snapshot_t snapshot;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &peer_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerDisconnectedAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.has_registered_peer);
    assert(!snapshot.has_current_peer);

    assert(
        snapshot.current_registration
        == GATEWAY_REGISTERED);

    assert(
        snapshot.connection
        == GATEWAY_DISCONNECTED);

    assert(!DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_unregister_while_connected_stops_authentication(void)
{
    static const uint8_t peer[] = {0x70U};
    const Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &peer_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(DeviceRegistrationManager_IsCurrentPeerAuthenticated());

    assert(
        DeviceRegistrationManager_OnRegistrationRemovedAt(
            120U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetCurrentRegistrationState()
        == GATEWAY_NOT_REGISTERED);

    assert(
        DeviceRegistrationManager_GetConnectionState()
        == GATEWAY_CONNECTED);

    assert(!DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void test_build_update_uses_current_peer(void)
{
    static const uint8_t registered_peer[] = {0x80U};
    static const uint8_t current_peer[] = {0x81U};
    const Gateway_ByteView_t registered_view =
        make_view(registered_peer, sizeof(registered_peer));
    const Gateway_ByteView_t current_view =
        make_view(current_peer, sizeof(current_peer));

    const Gateway_MessageContextView_t base = empty_context();
    Gateway_RegistrationConnectionUpdate_t update;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &registered_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &current_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_BuildDomainUpdateAt(
            &base, 150U, &update)
        == GATEWAY_STATUS_OK);

    assert(
        update.registration
        == GATEWAY_NOT_REGISTERED);

    assert(update.connection == GATEWAY_CONNECTED);
    assert(update.context.device_context_id.length == 1U);
    assert(update.context.device_context_id.data[0] == 0x81U);
    assert(update.update.quality == GATEWAY_DATA_QUALITY_VALID);
    assert(update.update.age_ms == 40U);
    assert(update.update.is_new_update);
}

static void test_publish_new_observation_semantics(void)
{
    static const uint8_t peer[] = {0x90U};
    const Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    const Gateway_MessageContextView_t base = empty_context();
    RegistrationTestTransport_t transport;

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);
    setup_interface(&transport);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &peer_view, 110U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_PublishToDomainAt(
            &base, 120U)
        == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 1);
    assert(transport.is_new_update);
    assert(transport.registration == GATEWAY_REGISTERED);
    assert(transport.connection == GATEWAY_CONNECTED);

    /*
     * 새 관측 없이 같은 상태를 다시 Publish하면 new update가 아니다.
     */
    assert(
        DeviceRegistrationManager_PublishToDomainAt(
            &base, 130U)
        == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 2);
    assert(!transport.is_new_update);

    /*
     * 값이 동일해도 Bluetooth 계층이 실제로 다시 확인했다면 새 관측이다.
     */
    assert(
        DeviceRegistrationManager_MarkCurrentStateObservedAt(
            140U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_PublishToDomainAt(
            &base, 145U)
        == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 3);
    assert(transport.is_new_update);
    assert(transport.age_ms == 5U);
}

static void test_oversize_ref_is_rejected_without_partial_replace(void)
{
    static const uint8_t valid_peer[] = {0xA0U};
    uint8_t too_large[DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES + 1U];

    const Gateway_ByteView_t valid_view =
        make_view(valid_peer, sizeof(valid_peer));

    Gateway_ByteView_t large_view;
    DeviceRegistrationManager_Snapshot_t snapshot;

    memset(too_large, 0xBB, sizeof(too_large));
    large_view = make_view(too_large, sizeof(too_large));

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &valid_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            &large_view, 110U)
        == GATEWAY_STATUS_UNSUPPORTED);

    assert(
        DeviceRegistrationManager_GetSnapshot(&snapshot)
        == GATEWAY_STATUS_OK);

    assert(snapshot.has_registered_peer);
    assert(snapshot.registered_peer.length == 1U);
    assert(snapshot.registered_peer.data[0] == 0xA0U);
}

static void test_connected_does_not_infer_app_or_proximity(void)
{
    /*
     * 이 Manager API에 App Activity / Proximity setter가 존재하지 않는 것
     * 자체가 책임 경계다. 여기서는 연결 상태만 검증한다.
     */
    static const uint8_t peer[] = {0xB0U};
    const Gateway_ByteView_t peer_view =
        make_view(peer, sizeof(peer));

    DeviceRegistrationManager_Reset();
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            &peer_view, 100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_GetConnectionState()
        == GATEWAY_CONNECTED);

    assert(
        DeviceRegistrationManager_GetCurrentRegistrationState()
        == GATEWAY_NOT_REGISTERED);
}

int main(void)
{
    test_initial_state();
    test_registered_but_disconnected();
    test_registered_peer_connected_is_authenticated();
    test_other_connected_peer_is_not_registered();
    test_input_buffer_is_copied();
    test_disconnect_does_not_delete_registration();
    test_unregister_while_connected_stops_authentication();
    test_build_update_uses_current_peer();
    test_publish_new_observation_semantics();
    test_oversize_ref_is_rejected_without_partial_replace();
    test_connected_does_not_infer_app_or_proximity();

    return 0;
}
