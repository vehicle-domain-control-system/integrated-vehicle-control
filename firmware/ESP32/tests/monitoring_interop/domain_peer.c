/* Existing Domain fixture, with real Provider/Runtime/Publisher linked read-only. */
#include "domain_network_admission_bridge_private.h"
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_H
#define DomainNetworkAdmissionBridgeHost_t DomainNetworkAdmissionBridgePrivate_t
#define DomainNetworkAdmissionBridgeHost_Init DomainNetworkAdmissionBridgePrivate_Init
#define DomainNetworkAdmissionBridgeHost_BindApi DomainNetworkAdmissionBridgePrivate_BindApi
#define DomainNetworkAdmissionBridgeHost_WrapOwner DomainNetworkAdmissionBridgePrivate_WrapOwner
#define DomainNetworkAdmissionBridgeHost_OnLinkDown DomainNetworkAdmissionBridgePrivate_OnLinkDown
#define DomainNetworkAdmissionBridgeHost_OnPortEpochChanged DomainNetworkAdmissionBridgePrivate_OnPortEpochChanged
#define DomainNetworkAdmissionBridgeHost_OnNetworkBootChanged DomainNetworkAdmissionBridgePrivate_OnNetworkBootChanged
#define DomainNetworkAdmissionBridgeHost_ResetBinding DomainNetworkAdmissionBridgePrivate_ResetBinding
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK
#define main ExistingDomainFixtureMain
#include "test_domain_network_admission_bridge.c"
#undef main
#include "domain_runtime.h"
#include "net_domain_snapshot_private.h"
#include "domain_snapshot_commit_private.h"
#include "net_upper_private.h"
#include "net_domain_board.h"
#include "domain_network_transition_binding.h"
static DomainRuntime_t runtime;
static bool (*esp_receive)(const uint8_t *, size_t);
static unsigned submissions;
static bool now_read(void *unused, uint32_t *out)
{ (void)unused; *out = g_provider.nowMs; return true; }
static NetStatus capture_submit(void *unused, const NetUpperPacket *packet, NetTxKey key)
{
    (void)unused; (void)key;
    if (packet->link != NET_WIRE_UART_M || !esp_receive(packet->uart.bytes, packet->uart.length)) return NET_REJECTED;
    submissions++;
    return NET_OK;
}
static const NetUpperPortOps capture_ops = {TestClock, capture_submit, TestPoll};
static NetStatus guarded_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{ return NetDomainSnapshot_Submit(&capture_ops, context, packet, key, UINT32_C(0x12345678)); }
static const NetUpperPortOps guarded_ops = {TestClock, guarded_submit, TestPoll};
static int capture_can(NetUpperCanMessage *message)
{
    NetUpperEvent event = {0};
    event.kind = NET_UPPER_CAN_RX; event.link = NET_WIRE_CAN;
    event.capture.port_epoch = TEST_PORT_EPOCH;
    event.capture.captured_at_ms = g_provider.nowMs; event.capture.timestamp_known = true;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY, message, &event.can) == NET_WIRE_OK);
    CHECK(net_upper_capture(&g_service, &event) == NET_OK);
    return (int)net_upper_step(&g_service, 1U);
}
int monitoring_peer_exchange(const uint8_t *bytes, size_t length, uint32_t now,
    bool (*receive)(const uint8_t *, size_t))
{
    DomainRuntime_Config_t config = {0};
    NetDomainC2Api api;
    NetDomainC2Config board_config;
    NetDomainC2TransitionBinding transition;
    NetUpperCanMessage can = {0};
    size_t i;
    CHECK(TestStart(false, now) == 0);
    CHECK(net_domain_c2_unregister(&g_adapter) == NET_OK);
    config.timeProvider = now_read; config.firstCommandId = 1U;
    CHECK(DomainRuntime_Init(&runtime, &config) == DOMAIN_RUNTIME_OK);
    /* Same real Domain-owned transition installation as Production main.c. */
    transition = domain_network_transition_binding(&runtime);
    NetDomainBoard_SetTransitionBinding(&transition);
    CHECK(NetDomainBoard_C2Config(&api, &board_config));
    board_config.domain_boot = UINT32_C(0x12345678);
    board_config.bcm_boot = 201U;
    board_config.first_can_sequence = 1U;
    g_adapter.config = board_config;
    DomainNetworkAdmissionBridgePrivate_ResetBinding();
    g_adapter.config.domain_boot = UINT32_C(0x12345678);
    CHECK(DomainNetworkAdmissionBridgePrivate_InitMonitoring(&g_bridge, &g_adapter) == DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    CHECK(DomainNetworkAdmissionBridgePrivate_BindApi(&g_bridge, &api) == DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    g_adapter.api = api;
    g_service.config.owner = DomainNetworkAdmissionBridgePrivate_WrapOwner(&g_bridge, net_domain_c2_owner(&g_adapter));
    CHECK(net_domain_c2_register(&g_adapter) == NET_OK);
    esp_receive = receive; g_service.config.port.ops = &guarded_ops;
    /* Same optional-Proximity sentinel allowance as real net_node_domain.upper.
     * Provider independently enforces required Source TTL 600/1000. */
    for (i = 0U; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        if (r != NULL && r->link == NET_WIRE_UART_M && r->id_type == 0x01U)
            g_service.config.rx_policy[net_upper_admission_index(i)].maximum_age_ms = 2000U + UINT16_MAX;
    }
    for (i = 0U; i < length; ++i) {
        NetUpperEvent event = {0};
        event.kind = NET_UPPER_UART_BYTE; event.link = NET_WIRE_UART_M;
        event.capture.port_epoch = TEST_PORT_EPOCH;
        event.capture.captured_at_ms = now; event.capture.timestamp_known = true;
        event.byte = bytes[i];
        CHECK(net_upper_capture(&g_service, &event) == NET_OK);
        (void)net_upper_step(&g_service, 1U);
    }
    CHECK(g_bridge.contextValid && g_bridge.currentContext.monitoringSessionReady);
    CHECK(!g_bridge.currentContext.registrationVerified && !g_bridge.currentContext.sessionVerified && !g_bridge.currentContext.p6AuthorizationVerified);
    CHECK(!runtime.requestCoordinatorEnabled);
    CHECK(g_service.context_seen);
    CHECK(DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(g_service.context.device_context_id, g_service.context.session_id) == DOMAIN_IF_OK);
    /* Simulated CIS observations only; never product sensor producers. */
    can.id = 0x300U;
    can.data.cis_environment.boot_id = 201U; can.data.cis_environment.tx_sequence = 1U;
    can.data.cis_environment.temperature_centi_c = 2345;
    can.data.cis_environment.temperature_sequence = 1U;
    can.data.cis_environment.temperature_validity = 1U;
    can.data.cis_environment.humidity_centi_percent = 4567U;
    can.data.cis_environment.humidity_sequence = 1U;
    can.data.cis_environment.humidity_validity = 1U;
    can.data.cis_environment.illuminance_lux = 123U;
    can.data.cis_environment.illuminance_sequence = 1U;
    can.data.cis_environment.illuminance_validity = 1U;
    /* First frame waits for the real Domain transition outside the callback.
     * A subsequent CIS report keeps the observation but advances TX sequence. */
    CHECK(capture_can(&can) == NET_HOLD);
    (void)net_domain_c2_step(&g_adapter);
    CHECK(g_adapter.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].open);
    can.data.cis_environment.tx_sequence = 2U;
    CHECK(capture_can(&can) == NET_OK);
    memset(&can, 0, sizeof(can)); can.id = 0x0a0U;
    can.data.cis_rear.boot_id = 201U; can.data.cis_rear.tx_sequence = 1U;
    can.data.cis_rear.update_sequence = 1U; can.data.cis_rear.rear_distance_deci_cm = 1234U;
    can.data.cis_rear.validity = 1U;
    CHECK(capture_can(&can) == NET_OK);
    {
        CisRearState_t rear;
        CHECK(DomainIf_QueryGetCisRear(now, &rear) == DOMAIN_IF_OK);
    }
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    for (i = 0U; i < NET_UPPER_TX_CAPACITY; ++i) (void)net_upper_dispatch(&g_service);
    CHECK(submissions == 2U);
    return 0;
}
int monitoring_peer_expire(uint32_t now)
{
    g_provider.nowMs = now;
    CHECK(DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(g_service.context.device_context_id, g_service.context.session_id) != DOMAIN_IF_OK);
    CHECK(!g_bridge.contextValid && submissions == 2U);
    return 0;
}
