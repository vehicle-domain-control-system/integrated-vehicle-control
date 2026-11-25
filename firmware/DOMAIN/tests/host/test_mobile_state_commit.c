#include "domain_runtime.h"
#include "domain_network_transition_binding.h"
#include "domain_snapshot_commit_private.h"
#include "net_domain_snapshot_private.h"
#include <stdio.h>
#include <string.h>
#include "uart_m_codec.h"
#include "uart_m_frame.h"
#include "gateway_mapper.h"
#include "domain_network_admission_bridge_private.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static DomainRuntime_t runtime;
static NetUpper upper;
static NetUpperQuery query;
static NetDomainC2 adapter;
static uint32_t nowMs = 100U;
static unsigned txCount;
static NetUpperPacket packet;
static DomainNetworkAdmissionBridgePrivate_t monitoring;

static bool clock32(void *c, uint32_t *out) { (void)c; *out = nowMs; return true; }
static NetStatus clock64(void *c, uint64_t *out) { (void)c; *out = nowMs; return NET_OK; }
static NetStatus submit(void *c, const NetUpperPacket *p, NetTxKey key)
{ (void)c; (void)key; packet = *p; ++txCount; return NET_OK; }
static NetStatus poll_port(void *c, NetUpperEvent *e) { (void)c; (void)e; return NET_NO_WORK; }
static const NetUpperPortOps driverOps = { clock64, submit, poll_port };
static NetStatus guardedSubmit(void *c, const NetUpperPacket *p, NetTxKey key)
{ return NetDomainSnapshot_Submit(&driverOps, c, p, key, 42U); }
static const NetUpperPortOps ops = { clock64, guardedSubmit, poll_port };

static int feedMonitoring(void)
{
    uart_m_message_t message = {0};
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE], frame[UART_M_MAX_FRAME_SIZE];
    size_t length, frameLength, i;
    message.type = UART_M_TYPE_CONTEXT;
    message.body.context.context.device_context_id = 3U;
    message.body.context.context.session_id = 4U;
    message.body.context.registration = UART_M_TRI_YES;
    message.body.context.connection = UART_M_TRI_YES;
    message.body.context.app_active = UART_M_TRI_YES;
    message.body.context.registration_connection_update_sequence = 1U;
    message.body.context.app_update_sequence = 1U;
    message.body.context.proximity = UART_M_TRI_UNKNOWN;
    message.body.context.proximity_validity = UART_M_VALIDITY_INVALID;
    message.body.context.proximity_reason = UART_M_QUALITY_REASON_NO_DATA;
    message.body.context.proximity_age_ms = UINT16_MAX;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) == UART_M_CODEC_OK);
    CHECK(uart_m_frame_encode(UART_M_TYPE_CONTEXT, 1U, payload, length,
          frame, sizeof(frame), &frameLength) == UART_M_FRAME_STATUS_OK);
    for (i = 0U; i < frameLength; ++i) {
        NetUpperEvent event = {0};
        event.kind = NET_UPPER_UART_BYTE; event.link = NET_WIRE_UART_M;
        event.capture.port_epoch = 1U; event.capture.timestamp_known = true;
        event.capture.captured_at_ms = nowMs; event.byte = frame[i];
        CHECK(net_upper_capture(&upper, &event) == NET_OK);
        (void)net_upper_step(&upper, 1U);
    }
    if (!monitoring.contextValid) printf("Context reject: seen=%d captured=%d/%d adapter=%d q=%d/%d age=%u/%u seq=%u/%u\n",
        upper.context_seen, monitoring.connectionCaptured, monitoring.proximityCaptured,
        adapter.last_status, monitoring.receivedConnection.meta.quality,
        monitoring.receivedConnection.appMeta.quality, monitoring.receivedConnection.meta.ageMs,
        monitoring.receivedConnection.appMeta.ageMs, monitoring.receivedConnection.meta.updateSequence,
        monitoring.receivedConnection.appMeta.updateSequence);
    CHECK(monitoring.contextValid && monitoring.currentContext.monitoringSessionReady);
    CHECK(!monitoring.currentContext.p6AuthorizationVerified);
    return 0;
}
static DomainIf_Status_t no_commit(const CisCabinEnvironmentState_t *v)
{ (void)v; return DOMAIN_IF_OK; }

int main(int argc, char **argv)
{
    DomainRuntime_Config_t rc = {0};
    NetUpperConfig uc = {0};
    NetDomainC2Config cc = {0};
    NetDomainC2Api api;
    NetUpperCanMessage m = {0}, raw;
    NetUpperEvent e = {0};
    NetUpperDelivery delivery;
    NetSourceStamp source;
    NetWireUartPayload decoded;
    uart_m_frame_t espFrame;
    uart_m_message_t espMessage;
    gateway_ble_message_t bleCandidate;
    uint32_t boot, seq;
    uint16_t id = 0x300U;
    uint8_t uartType = 0x34U;
    size_t i;
    bool missing = argc > 1 && strcmp(argv[1], "no_commit") == 0;
    bool expired = argc > 1 && strcmp(argv[1], "environment_timeout") == 0;
    bool noSession = argc > 1 && strcmp(argv[1], "missing_session") == 0;
    if (argc > 1 && strcmp(argv[1], "door") == 0) { id = 0x200U; uartType = 0x30U; }
    if (argc > 1 && strcmp(argv[1], "status") == 0) { id = 0x230U; uartType = 0x33U; }
    if (argc > 1 && strcmp(argv[1], "rear") == 0) { id = 0x0a0U; uartType = 0x36U; }
    if (argc > 1 && strncmp(argv[1], "occupant", 8U) == 0) { id = 0x310U; uartType = 0x35U; }
    if (argc > 1 && strcmp(argv[1], "cis_status") == 0) { id = 0x320U; uartType = 0x37U; }
    if (argc > 1 && strcmp(argv[1], "vss_status") == 0) { id = 0x340U; uartType = 0x3aU; }
    rc.timeProvider = clock32;
    rc.firstCommandId = 1U;
    rc.cisOccupantStorageEnabled = id == 0x310U && strcmp(argv[1], "occupant_disabled") != 0;
    rc.cisOccupantPolicy.presenceTtlMs = VSS_WARNING_MANAGER_OCCUPANT_TTL_MS;
    rc.cisOccupantPolicy.countTtlMs = VSS_WARNING_MANAGER_OCCUPANT_TTL_MS;
    /* Test policy only; no Production TTL is chosen. */
    rc.stateQueryPolicy.bcmDoorTtlMs = 500U;
    rc.stateQueryPolicy.bcmStatusTtlMs = 500U;
    rc.stateQueryPolicy.cisStatusTtlMs = 500U;
    rc.stateQueryPolicy.vssStatusTtlMs = 500U;
    CHECK(DomainRuntime_Init(&runtime, &rc) == DOMAIN_RUNTIME_OK);
    net_domain_c2_pinned_owner_api(&api);
    if (missing) api.RxCisCabinEnvironment = no_commit;
    cc.domain_boot = 42U;
    cc.first_can_sequence = 1U;
    cc.value_quality = net_domain_c2_value_quality_provisional;
    cc.status_quality = net_domain_c2_status_quality_provisional;
    cc.confirm_snapshot = NetDomainSnapshot_Confirm;
    cc.transmit_policy.bound = true;
    cc.transmit_policy.maximum_age_ms = 2000U;
    cc.transition = domain_network_transition_binding(&runtime);
    cc.cis_transition_barriers = true;
    CHECK(net_domain_c2_init(&adapter, &upper, &query, &api, &cc) == NET_OK);
    uc.node = NET_WIRE_NODE_DOMAIN;
    uc.local_epoch = 1U;
    uc.profile = NET_WIRE_CANDIDATE_ONLY;
    uc.port.ops = &ops;
    uc.owner = net_domain_c2_owner(&adapter);
    for (i = 0; i < 3U; ++i) uc.port_epoch[i] = 1U;
    for (i = 0; i < NET_UPPER_ROUTE_COUNT; ++i) {
        uc.rx_policy[i].bound = true;
        uc.rx_policy[i].maximum_age_ms = 5000U;
        {
            const NetWireRegistryEntry *route = net_wire_registry_at(i);
            if (route != NULL && route->link == NET_WIRE_UART_M && route->id_type == 0x01U)
                uc.rx_policy[i].maximum_age_ms += UINT16_MAX;
        }
    }
    for (i = 0; i < 2U; ++i) {
        uc.uart_policy[i].candidate_time_policy_selected = true;
        uc.uart_policy[i].assembly_limit_ms = 100U;
        uc.uart_sequence_bound[i] = true;
        uc.uart_initial_sequence[i] = 1U;
    }
    CHECK(net_upper_init(&upper, &uc) == NET_OK);
    {
        NetUpperQueryOwner owner = net_domain_c2_query_owner(&adapter);
        CHECK(net_upper_query_init(&query, &upper, 42U, &owner) == NET_OK);
    }
    CHECK(DomainNetworkAdmissionBridgePrivate_InitMonitoring(&monitoring, &adapter) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    CHECK(DomainNetworkAdmissionBridgePrivate_BindApi(&monitoring, &adapter.api) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    upper.config.owner = DomainNetworkAdmissionBridgePrivate_WrapOwner(&monitoring, upper.config.owner);
    CHECK(net_domain_c2_register(&adapter) == NET_OK);
    if (!noSession) CHECK(feedMonitoring() == 0);
    CHECK(!runtime.requestCoordinatorEnabled && !runtime.cisOccupantEnabled);
    m.id = id;
    if (id == 0x300U) {
        NetWireCisEnvironmentRaw *v = &m.data.cis_environment;
        v->boot_id = 9U; v->tx_sequence = 1U;
        v->temperature_centi_c = 2350; v->humidity_centi_percent = 5500U;
        v->illuminance_lux = 300U;
        v->temperature_sequence = v->humidity_sequence = v->illuminance_sequence = 1U;
        v->temperature_validity = v->humidity_validity = v->illuminance_validity = 1U;
    } else if (id == 0x310U) {
        m.data.cis_occupant.boot_id = 9U;
        m.data.cis_occupant.tx_sequence = 1U;
        m.data.cis_occupant.pi_boot_id = 17U;
        m.data.cis_occupant.vision_sequence = 1U;
        m.data.cis_occupant.occupant_presence = 1U;
        m.data.cis_occupant.presence_validity = 1U;
        m.data.cis_occupant.occupant_count = 1U;
        m.data.cis_occupant.count_validity = 1U;
    } else if (id == 0x200U) {
        m.data.bcm_door_state.boot_id = 9U;
        m.data.bcm_door_state.tx_sequence = 1U;
        m.data.bcm_door_state.update_sequence = 1U;
    } else if (id == 0x230U) {
        m.data.bcm_status.boot_id = 9U;
        m.data.bcm_status.tx_sequence = 1U;
        m.data.bcm_status.update_sequence = 1U;
        m.data.bcm_status.bcm_ecu_state = 1U;
    } else if (id == 0x0a0U) {
        m.data.cis_rear.boot_id = 9U;
        m.data.cis_rear.tx_sequence = 1U;
        m.data.cis_rear.update_sequence = 1U;
        m.data.cis_rear.rear_distance_deci_cm = 300U;
        m.data.cis_rear.validity = 1U;
    } else if (id == 0x320U) {
        m.data.cis_status.boot_id = 9U;
        m.data.cis_status.tx_sequence = 1U;
        m.data.cis_status.update_sequence = 1U;
    } else {
        m.data.vss_status.boot_id = 9U;
        m.data.vss_status.tx_sequence = 1U;
        m.data.vss_status.update_sequence = 1U;
        m.data.vss_status.last_fault_quality = 3U;
    }
    e.kind = NET_UPPER_CAN_RX; e.link = NET_WIRE_CAN;
    e.capture.port_epoch = 1U; e.capture.captured_at_ms = nowMs;
    e.capture.timestamp_known = true;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY, &m, &e.can) == NET_WIRE_OK);
    CHECK(net_upper_capture(&upper, &e) == NET_OK);
    (void)net_upper_step(&upper, 1U);
    if (id == 0x300U || id == 0x310U) {
        (void)net_domain_c2_step(&adapter);
        CHECK(net_upper_capture(&upper, &e) == NET_OK);
        (void)net_upper_step(&upper, 1U);
    }
    if (argc > 1 && strcmp(argv[1], "occupant_disabled") == 0) {
        CisOccupantState_t state;
        CHECK(DomainIf_QueryGetCisOccupant(nowMs, &state) == DOMAIN_IF_NOT_AVAILABLE);
        CHECK(net_upper_received_can(&upper, id, &raw, &delivery, &source) != NET_OK);
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        CHECK(txCount == 0U);
        puts("occupant_disabled PASS"); return 0;
    }
    CHECK(net_upper_received_can(&upper, id, &raw, &delivery, &source) == NET_OK);
    CHECK(net_upper_can_identity(&raw, &boot, &seq) == NET_WIRE_OK);
    CHECK(txCount == 0U); /* Callback never transmits. */
    if (missing) {
        CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        (void)net_upper_dispatch(&upper);
        CHECK(txCount == 0U);
        puts("no_commit: PASS"); return 0;
    }
    CHECK(NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
    CHECK(!NetDomainSnapshot_Confirm(NULL, id ^ 1U, delivery.receipt_id, boot, seq));
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id + 1U, boot, seq));
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot + 1U, seq));
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq + 1U));
    if (noSession || (argc > 1 && (strcmp(argv[1], "session_change") == 0 ||
        strcmp(argv[1], "uart_link_down") == 0 || strcmp(argv[1], "uart_epoch") == 0))) {
        if (strcmp(argv[1], "session_change") == 0) upper.context.session_id = 5U;
        if (strcmp(argv[1], "uart_link_down") == 0) upper.link_failed[NET_WIRE_UART_M] = true;
        if (strcmp(argv[1], "uart_epoch") == 0) ++upper.config.port_epoch[NET_WIRE_UART_M];
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        (void)net_upper_dispatch(&upper);
        CHECK(txCount == 0U);
        upper.context_seen = true;
        upper.context.session_id = 4U;
        upper.link_failed[NET_WIRE_UART_M] = false;
        upper.config.port_epoch[NET_WIRE_UART_M] = 1U;
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        (void)net_upper_dispatch(&upper);
        CHECK(txCount == 0U); /* Restoring a tuple never replays dropped work. */
        printf("%s: PASS (old pending state discarded)\n", argv[1]);
        return 0;
    }
    if (expired) {
        CisCabinEnvironmentState_t stale;
        nowMs += CIS_ENVIRONMENT_TEMPERATURE_TTL_MS;
        CHECK(DomainIf_QueryGetCisEnvironment(nowMs, &stale) == DOMAIN_IF_OK);
        CHECK(stale.temperatureQuality.meta.quality == DATA_QUALITY_STALE);
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        (void)net_upper_dispatch(&upper);
        CHECK(txCount == 0U);
        puts("environment_timeout: PASS (stale snapshot never promoted)");
        return 0;
    }
    adapter.in_call = true;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    CHECK(txCount == 0U);
    adapter.in_call = false;
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    if (argc > 1 && strcmp(argv[1], "queued_revoke") == 0) {
        CHECK(txCount == 0U); /* Queue accepted, device has not borrowed. */
        DomainSnapshotCommit_Notify(0U, NULL, 0U);
        CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
        (void)net_upper_dispatch(&upper);
        CHECK(txCount == 0U);
        puts("queued_revoke: PASS (existing Network cancellation, no device submit)");
        return 0;
    }
    (void)net_upper_dispatch(&upper);
    if (txCount != 1U) printf("publisher last NetStatus=%d C2=%d\n",
                              (int)adapter.last_status, (int)adapter.last_c2_status);
    CHECK(txCount == 1U);
    CHECK(packet.link == NET_WIRE_UART_M);
    CHECK(net_wire_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M,
        NET_WIRE_DOWNLINK, &packet.uart, &decoded) == NET_WIRE_OK);
    CHECK(decoded.type == uartType);
    CHECK(uart_m_frame_decode(packet.uart.bytes, packet.uart.length, &espFrame) ==
          UART_M_FRAME_STATUS_OK);
    CHECK(uart_m_codec_decode(espFrame.message_type, espFrame.payload,
        espFrame.payload_length, &espMessage) == UART_M_CODEC_OK);
    CHECK(gateway_mapper_uart_downlink_to_ble(&espMessage, &bleCandidate) ==
          GATEWAY_MAPPER_OK);
    CHECK(bleCandidate.message_type == uartType);
    CHECK(bleCandidate.payload_length == espFrame.payload_length);
    if (argc > 1 && strcmp(argv[1], "occupant_timeout") == 0) {
        CisOccupantState_t state;
        CHECK(DomainIf_QueryGetCisOccupant(nowMs, &state) == DOMAIN_IF_OK);
        CHECK(state.presenceQuality.meta.quality == DATA_QUALITY_OK);
        nowMs += VSS_WARNING_MANAGER_OCCUPANT_TTL_MS;
        CHECK(DomainIf_QueryGetCisOccupant(nowMs, &state) == DOMAIN_IF_OK);
        CHECK(state.presenceQuality.meta.quality == DATA_QUALITY_STALE);
        CHECK(state.countQuality.meta.quality == DATA_QUALITY_STALE);
        CHECK(!runtime.cisOccupantEnabled && runtime.cisOccupantStorageEnabled);
    }
    CHECK(DomainRuntime_Poll(&runtime) == DOMAIN_RUNTIME_OK);
    (void)net_upper_dispatch(&upper);
    CHECK(txCount == 1U); /* Accepted handoff is not submitted twice. */
    upper.link_failed[NET_WIRE_CAN] = true;
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
    upper.link_failed[NET_WIRE_CAN] = false;
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
    DomainSnapshotCommit_Notify(id, NULL, 0U);
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
    CHECK(net_upper_capture(&upper, &e) == NET_OK);
    (void)net_upper_step(&upper, 1U);
    CHECK(!NetDomainSnapshot_Confirm(NULL, id, delivery.receipt_id, boot, seq));
    printf("%s: PASS (CAN RX -> Commit -> Runtime -> UART -> ESP32 Codec/BLE Mapper)\n",
           argc > 1 ? argv[1] : "environment");
    return 0;
}
