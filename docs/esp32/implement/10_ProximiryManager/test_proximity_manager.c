#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "DeviceRegistrationManager.h"
#include "Gateway_Interface.h"
#include "Gateway_PolicyConfig.h"
#include "LinkStateManager.h"
#include "ProximityManager.h"

typedef struct
{
    int publish_calls;
    Gateway_ProximityState_t state;
    Gateway_DataQuality_t quality;
    Gateway_TimeMs_t age_ms;
    bool is_new_update;

    uint8_t device_ref[DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES];
    size_t device_ref_length;
} ProximityTestTransport_t;

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

static Gateway_Status_t unused_relay_sender(
    const Gateway_RelayMessageView_t *message,
    void *user_context)
{
    (void)message;
    (void)user_context;
    return GATEWAY_STATUS_OK;
}

static Gateway_Status_t unused_registration_sender(
    const Gateway_RegistrationConnectionUpdate_t *update,
    void *user_context)
{
    (void)update;
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

static Gateway_Status_t capture_proximity(
    const Gateway_ProximityUpdate_t *update,
    void *user_context)
{
    ProximityTestTransport_t *transport =
        (ProximityTestTransport_t *)user_context;

    assert(update != NULL);

    transport->publish_calls++;
    transport->state = update->proximity.state;
    transport->quality = update->proximity.update.quality;
    transport->age_ms = update->proximity.update.age_ms;
    transport->is_new_update =
        update->proximity.update.is_new_update;

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

static void configure_default_policy(void)
{
    const Gateway_ProximityPolicyConfig_t config = {
        .near_enter_rssi_dbm = -60,
        .far_exit_rssi_dbm = -70,
        .stable_sample_count = 2U,
        .proximity_expiry_ms = 500U
    };

    assert(Gateway_PolicyConfig_Init() == GATEWAY_STATUS_OK);

    assert(
        Gateway_PolicyConfig_SetProximity(&config)
        == GATEWAY_STATUS_OK);
}

static void authenticate_peer(
    const Gateway_ByteView_t *peer)
{
    assert(DeviceRegistrationManager_Init() == GATEWAY_STATUS_OK);
    assert(LinkStateManager_Init() == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnRegistrationConfirmedAt(
            peer,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        DeviceRegistrationManager_OnPeerConnectedAt(
            peer,
            100U)
        == GATEWAY_STATUS_OK);

    assert(
        LinkStateManager_MarkAvailableAt(
            GATEWAY_LINK_BLUETOOTH,
            100U)
        == GATEWAY_STATUS_OK);

    assert(DeviceRegistrationManager_IsCurrentPeerAuthenticated());
}

static void setup(
    const Gateway_ByteView_t *peer)
{
    Gateway_PolicyConfig_Reset();
    DeviceRegistrationManager_Reset();
    LinkStateManager_Reset();
    ProximityManager_Reset();

    configure_default_policy();
    authenticate_peer(peer);

    assert(ProximityManager_Init() == GATEWAY_STATUS_OK);
}

static void setup_interface(
    ProximityTestTransport_t *transport)
{
    Gateway_InterfaceCoreHandlers_t core_handlers = {0};

    Gateway_InterfaceTransportPorts_t ports = {
        .send_to_domain = unused_relay_sender,
        .send_to_mobile = unused_relay_sender,
        .send_registration_to_domain = unused_registration_sender,
        .send_app_activity_to_domain = unused_app_sender,
        .send_proximity_to_domain = capture_proximity,
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

static void test_initial_unknown(void)
{
    static const uint8_t peer_data[] = {0x01U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_UNKNOWN);

    assert(ProximityManager_UpdateAt(120U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_NO_DATA);
}

static void test_near_requires_stable_samples(void)
{
    static const uint8_t peer_data[] = {0x02U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);

    assert(ProximityManager_OnRssiSampleAt(&peer, -56, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_VALID);
}

static void test_far_requires_stable_samples(void)
{
    static const uint8_t peer_data[] = {0x03U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -80, true, 120U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);

    assert(ProximityManager_OnRssiSampleAt(&peer, -82, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_FAR);
}

static void test_hysteresis_holds_near_in_middle_band(void)
{
    static const uint8_t peer_data[] = {0x04U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(ProximityManager_OnRssiSampleAt(&peer, -65, true, 140U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);
}

static void test_near_to_far_requires_stability(void)
{
    static const uint8_t peer_data[] = {0x05U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(ProximityManager_OnRssiSampleAt(&peer, -75, true, 140U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(ProximityManager_OnRssiSampleAt(&peer, -76, true, 150U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_FAR);
}

static void test_invalid_sample_is_unknown_not_far(void)
{
    static const uint8_t peer_data[] = {0x06U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -50, false, 120U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_INVALID);
}

static void test_disconnect_is_unknown_not_far(void)
{
    static const uint8_t peer_data[] = {0x07U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(LinkStateManager_MarkUnavailableAt(
        GATEWAY_LINK_BLUETOOTH, 140U) == GATEWAY_STATUS_OK);

    assert(DeviceRegistrationManager_OnPeerDisconnectedAt(
        140U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_UpdateAt(140U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_NO_DATA);
}

static void test_expiry_is_stale_unknown_not_far(void)
{
    static const uint8_t peer_data[] = {0x08U};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    ProximityManager_Snapshot_t snapshot;

    setup(&peer);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(ProximityManager_UpdateAt(629U) == GATEWAY_STATUS_OK);
    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(ProximityManager_UpdateAt(630U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_STALE);

    assert(ProximityManager_GetSnapshot(&snapshot) == GATEWAY_STATUS_OK);
    assert(snapshot.last_observed_ms == 130U);
}

static void test_other_peer_sample_is_rejected(void)
{
    static const uint8_t peer_a_data[] = {0x09U};
    static const uint8_t peer_b_data[] = {0x0AU};

    const Gateway_ByteView_t peer_a =
        make_view(peer_a_data, sizeof(peer_a_data));
    const Gateway_ByteView_t peer_b =
        make_view(peer_b_data, sizeof(peer_b_data));

    setup(&peer_a);

    assert(ProximityManager_OnRssiSampleAt(&peer_b, -50, true, 120U)
        == GATEWAY_STATUS_NOT_READY);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
}

static void test_peer_change_does_not_reuse_old_near(void)
{
    static const uint8_t peer_a_data[] = {0x0BU};
    static const uint8_t peer_b_data[] = {0x0CU};

    const Gateway_ByteView_t peer_a =
        make_view(peer_a_data, sizeof(peer_a_data));
    const Gateway_ByteView_t peer_b =
        make_view(peer_b_data, sizeof(peer_b_data));

    setup(&peer_a);

    assert(ProximityManager_OnRssiSampleAt(&peer_a, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer_a, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_NEAR);

    assert(DeviceRegistrationManager_OnPeerDisconnectedAt(140U)
        == GATEWAY_STATUS_OK);

    assert(DeviceRegistrationManager_OnRegistrationConfirmedAt(
        &peer_b, 150U) == GATEWAY_STATUS_OK);

    assert(DeviceRegistrationManager_OnPeerConnectedAt(
        &peer_b, 150U) == GATEWAY_STATUS_OK);

    assert(LinkStateManager_MarkAvailableAt(
        GATEWAY_LINK_BLUETOOTH, 150U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_UpdateAt(150U) == GATEWAY_STATUS_OK);

    assert(ProximityManager_GetState() == GATEWAY_PROXIMITY_UNKNOWN);
    assert(ProximityManager_GetQuality() == GATEWAY_DATA_QUALITY_NO_DATA);
}

static void test_publish_preserves_new_update_semantics(void)
{
    static const uint8_t peer_data[] = {0x0DU};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    const Gateway_MessageContextView_t base = empty_context();
    ProximityTestTransport_t transport;

    setup(&peer);
    setup_interface(&transport);

    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 120U)
        == GATEWAY_STATUS_OK);
    assert(ProximityManager_OnRssiSampleAt(&peer, -55, true, 130U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_PublishToDomainAt(
        &base, 140U) == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 1);
    assert(transport.state == GATEWAY_PROXIMITY_NEAR);
    assert(transport.quality == GATEWAY_DATA_QUALITY_VALID);
    assert(transport.age_ms == 10U);
    assert(transport.is_new_update);
    assert(transport.device_ref_length == sizeof(peer_data));
    assert(transport.device_ref[0] == peer_data[0]);

    assert(ProximityManager_PublishToDomainAt(
        &base, 150U) == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 2);
    assert(!transport.is_new_update);

    assert(ProximityManager_OnRssiSampleAt(&peer, -58, true, 160U)
        == GATEWAY_STATUS_OK);

    assert(ProximityManager_PublishToDomainAt(
        &base, 165U) == GATEWAY_STATUS_OK);

    assert(transport.publish_calls == 3);
    assert(transport.is_new_update);
    assert(transport.age_ms == 5U);
}

static void test_config_required(void)
{
    static const uint8_t peer_data[] = {0x0EU};
    const Gateway_ByteView_t peer =
        make_view(peer_data, sizeof(peer_data));

    Gateway_PolicyConfig_Reset();
    DeviceRegistrationManager_Reset();
    LinkStateManager_Reset();
    ProximityManager_Reset();

    assert(Gateway_PolicyConfig_Init() == GATEWAY_STATUS_OK);
    authenticate_peer(&peer);
    assert(ProximityManager_Init() == GATEWAY_STATUS_OK);

    assert(ProximityManager_OnRssiSampleAt(&peer, -50, true, 120U)
        == GATEWAY_STATUS_NOT_READY);
}

int main(void)
{
    test_initial_unknown();
    test_near_requires_stable_samples();
    test_far_requires_stable_samples();
    test_hysteresis_holds_near_in_middle_band();
    test_near_to_far_requires_stability();
    test_invalid_sample_is_unknown_not_far();
    test_disconnect_is_unknown_not_far();
    test_expiry_is_stale_unknown_not_far();
    test_other_peer_sample_is_rejected();
    test_peer_change_does_not_reuse_old_near();
    test_publish_preserves_new_update_semantics();
    test_config_required();

    return 0;
}
