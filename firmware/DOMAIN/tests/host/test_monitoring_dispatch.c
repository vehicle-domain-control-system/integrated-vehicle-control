/* Reuse the existing real Network decoder/Domain inbox fixture. */
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
#define main ExistingPrivateBridgeMain
#include "test_domain_network_admission_bridge.c"
#undef main
#include "uart_m_codec.h"
#include "uart_m_frame.h"
#include "domain_snapshot_commit_private.h"
#include "net_domain_snapshot_private.h"
#include "net_domain_board.h"
#include "domain_runtime.h"
#include "net_upper_private.h"

static DomainRuntime_t dispatchRuntime;
static bool monitoringReady;
static bool invalidRequiredSource;
static bool oldRequiredSource;
static bool trustedMonitoring;
static bool MonitoringEvidence(void *context, DeviceContextId_t device,
    SessionId_t session, DomainRequestCoordinator_ContextSnapshot_t *out)
{
    if (!TestContextEvidence(context, device, session, out)) return false;
    out->monitoringSessionReady = monitoringReady; /* Host Provider only. */
    return true;
}
static bool DispatchNow(void *context, uint32_t *out)
{ (void)context; *out = g_provider.nowMs; return true; }

static unsigned driverCalls;
static NetStatus DriverSubmit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    (void)context; (void)packet; (void)key;
    ++driverCalls;
    return NET_OK;
}
static const NetUpperPortOps driverOps = { TestClock, DriverSubmit, TestPoll };
static NetStatus GuardedSubmit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    return NetDomainSnapshot_Submit(&driverOps, context, packet, key, TEST_NETWORK_BOOT);
}
static const NetUpperPortOps guardedOps = { TestClock, GuardedSubmit, TestPoll };

static int FeedContext(const NetUpperUartDelivery *context, size_t begin, size_t end)
{
    size_t i;
    for (i = begin; i < end; ++i) {
        NetUpperEvent event = {0};
        event.kind = NET_UPPER_UART_BYTE;
        event.link = NET_WIRE_UART_M;
        event.capture.port_epoch = TEST_PORT_EPOCH;
        event.capture.captured_at_ms = g_provider.nowMs;
        event.capture.timestamp_known = true;
        event.byte = context->frame.bytes[i];
        CHECK(net_upper_capture(&g_service, &event) == NET_OK);
        (void)net_upper_step(&g_service, 1U);
    }
    return 0;
}

static int EspContext(NetUpperUartDelivery *out)
{
    uart_m_message_t message = {0};
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t size;
    memset(out, 0, sizeof(*out));
    message.type = UART_M_TYPE_CONTEXT;
    message.body.context.context.device_context_id = TEST_DEVICE;
    message.body.context.context.session_id = TEST_SESSION;
    message.body.context.registration = UART_M_TRI_YES;
    message.body.context.connection = UART_M_TRI_YES;
    message.body.context.app_active = UART_M_TRI_YES;
    message.body.context.registration_connection_age_ms = 17U;
    message.body.context.registration_connection_update_sequence = 8U;
    message.body.context.proximity_validity = UART_M_VALIDITY_VALID;
    if (trustedMonitoring) message.body.context.app_update_sequence = 1U;
    if (invalidRequiredSource)
        message.body.context.app_quality = UART_M_QUALITY_NO_DATA;
    if (oldRequiredSource)
        message.body.context.registration_connection_age_ms = 2001U;
    if (monitoringReady) {
        /* The existing ESP32 Monitoring producer has no proximity source. */
        message.body.context.proximity = UART_M_TRI_UNKNOWN;
        message.body.context.proximity_validity = UART_M_VALIDITY_INVALID;
        message.body.context.proximity_reason = UART_M_QUALITY_REASON_NO_DATA;
        message.body.context.proximity_age_ms = UINT16_MAX;
    }
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &size) == UART_M_CODEC_OK);
    CHECK(uart_m_frame_encode(UART_M_TYPE_CONTEXT, 1U, payload, size,
        out->frame.bytes, sizeof(out->frame.bytes), &out->frame.length) == UART_M_FRAME_STATUS_OK);
    CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M,
        NET_WIRE_UPLINK, &out->frame, &out->message) == NET_WIRE_OK);
    out->port_epoch = TEST_PORT_EPOCH;
    return 0;
}

int main(int argc, char **argv)
{
    NetDomainC2Api api;
    NetUpperUartDelivery context;
    NetUpperDelivery metadata;
    bool p6 = !(argc > 1 && strcmp(argv[1], "p6_absent") == 0);
    if (argc == 2 && strncmp(argv[1], "dispatch_", 9U) == 0)
    {
        NetUpperUartMessage message = {0};
        NetSourceStamp stamp = {0};
        NetAgePolicy policy = {true, 1000U, true};
        NetTxKey key;
        NetUpperTxSnapshot tx;
        uart_m_frame_t parsed;
        bool stateMode = strstr(argv[1], "dispatch_state_") == argv[1];
        trustedMonitoring = strstr(argv[1], "_gateway_") != NULL;
        CHECK(TestStart(!trustedMonitoring, 100U) == 0);
        monitoringReady = stateMode && strcmp(argv[1], "dispatch_state_not_ready") != 0;
        invalidRequiredSource = strcmp(argv[1], "dispatch_state_invalid_source") == 0;
        oldRequiredSource = strcmp(argv[1], "dispatch_state_old_source") == 0;
        g_bridge.getContextEvidence = MonitoringEvidence;
        if (strcmp(argv[1], "dispatch_alive") == 0) {
            NetUpperPacket alive = {0};
            NetTxKey aliveKey = {1U, 1U};
            message.link = NET_WIRE_UART_M;
            message.type = 0x02U;
            message.data.m_domain_alive.domain_boot_id = TEST_NETWORK_BOOT;
            message.data.m_domain_alive.value_quality = 3U;
            message.data.m_domain_alive.source_age_ms = UINT16_MAX;
            alive.link = NET_WIRE_UART_M;
            CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY, &message, &alive.uart) == NET_WIRE_OK);
            CHECK(NetDomainSnapshot_Submit(&driverOps, &g_provider, &alive,
                aliveKey, TEST_NETWORK_BOOT) == NET_OK);
            CHECK(driverCalls == 1U && !g_bridge.contextValid);
            puts("dispatch_alive PASS (existing context-free Alive preserved)");
            return 0;
        }
        if (stateMode) {
            DomainRuntime_Config_t config = {0};
            config.timeProvider = DispatchNow;
            config.firstCommandId = 1U;
            CHECK(DomainRuntime_Init(&dispatchRuntime, &config) == DOMAIN_RUNTIME_OK);
            CHECK(!dispatchRuntime.requestCoordinatorEnabled);
            g_adapter.api.RxBcmDoorState = DomainIf_RxBcmDoorState;
            CHECK(DomainNetworkAdmissionBridgePrivate_BindApi(&g_bridge, &g_adapter.api) ==
                  DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
        }
        if (trustedMonitoring) {
            NetUpperOwner delegate = g_bridge.delegate;
            DomainNetworkAdmissionBridgePrivate_ResetBinding();
            CHECK(DomainNetworkAdmissionBridgePrivate_InitMonitoring(&g_bridge, &g_adapter) ==
                  DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
            CHECK(DomainNetworkAdmissionBridgePrivate_BindApi(&g_bridge, &g_adapter.api) ==
                  DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
            g_owner = DomainNetworkAdmissionBridgePrivate_WrapOwner(&g_bridge, delegate);
            g_service.config.owner = g_owner;
        }
        g_service.config.port.ops = &guardedOps;
        if (monitoringReady) {
            size_t route;
            for (route = 0U; route < NET_UPPER_ROUTE_COUNT; ++route) {
                const NetWireRegistryEntry *entry = net_wire_registry_at(route);
                if (entry != NULL && entry->link == NET_WIRE_UART_M && entry->id_type == 0x01U) {
                    size_t index = net_upper_admission_index(route);
                    g_service.config.rx_policy[index].maximum_age_ms += UINT16_MAX;
                }
            }
        }
        CHECK(EspContext(&context) == 0);
        CHECK(FeedContext(&context, 0U, context.frame.length) == 0);
        if (invalidRequiredSource || oldRequiredSource) {
            CHECK(!g_bridge.contextValid && driverCalls == 0U);
            printf("%s PASS\n", argv[1]);
            return 0;
        }
        CHECK(g_bridge.contextValid && g_service.context_seen);
        if (trustedMonitoring) {
            CHECK(g_bridge.currentContext.monitoringSessionReady);
            CHECK(!g_bridge.currentContext.p6AuthorizationVerified);
            CHECK(!g_bridge.currentContext.sessionVerified);
            CHECK(!dispatchRuntime.requestCoordinatorEnabled);
            CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) != DOMAIN_IF_OK);
            CHECK(DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(TEST_DEVICE + 1U, TEST_SESSION) != DOMAIN_IF_OK);
            CHECK(DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(TEST_DEVICE, TEST_SESSION + 1U) != DOMAIN_IF_OK);
            CHECK(TestDeliverRequest(TEST_DEVICE, TEST_SESSION, 77U, TEST_PORT_EPOCH,
                                     2U, 100U, true) == NET_HOLD);
            CHECK(g_coordinator.inboxCount == 0U);
        }
        if (monitoringReady) {
            CHECK(g_bridge.receivedProximity.meta.quality == DATA_QUALITY_NO_DATA);
            CHECK(g_bridge.receivedProximity.state == PROXIMITY_UNKNOWN);
        }
        CHECK(driverCalls == 0U);
        if (strcmp(argv[1], "dispatch_state_gateway_repeat") == 0) {
            /* Same source sequence and old wire AGE cannot refresh source life. */
            g_provider.nowMs += 200U;
            CHECK(uart_m_frame_decode(context.frame.bytes, context.frame.length, &parsed) == UART_M_FRAME_STATUS_OK);
            CHECK(uart_m_frame_encode(UART_M_TYPE_CONTEXT, 2U, parsed.payload,
                parsed.payload_length, context.frame.bytes, sizeof(context.frame.bytes),
                &context.frame.length) == UART_M_FRAME_STATUS_OK);
            CHECK(FeedContext(&context, 0U, context.frame.length) == 0);
            CHECK(g_bridge.contextValid);
            CHECK((uint64_t)g_bridge.monitoringConnectionAge +
                  (uint32_t)(g_provider.nowMs - g_bridge.monitoringDeliveredAtMs) == 217U);
            g_provider.nowMs = 100U + DOMAIN_MONITORING_CONNECTION_TTL_MS - 17U;
            CHECK(DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(TEST_DEVICE, TEST_SESSION) != DOMAIN_IF_OK);
            CHECK(!g_bridge.contextValid && driverCalls == 0U);
            puts("dispatch_state_gateway_repeat PASS");
            return 0;
        }
        message.link = NET_WIRE_UART_M;
        message.type = 0x40U;
        message.data.m_query_end.device_context_id = TEST_DEVICE;
        message.data.m_query_end.session_id = TEST_SESSION;
        message.data.m_query_end.domain_boot_id = TEST_NETWORK_BOOT;
        message.data.m_query_end.query_id = 7U;
        message.data.m_query_end.query_status = 2U; /* Host PARTIAL fixture. */
        stamp.observation_id = 1U;
        stamp.observed_at_ms = g_provider.nowMs;
        if (strcmp(argv[1], "dispatch_same_step") == 0 ||
            strcmp(argv[1], "dispatch_state_same_step") == 0) {
            CHECK(uart_m_frame_decode(context.frame.bytes, context.frame.length, &parsed) == UART_M_FRAME_STATUS_OK);
            CHECK(uart_m_frame_encode(UART_M_TYPE_CONTEXT, 2U, parsed.payload,
                parsed.payload_length, context.frame.bytes, sizeof(context.frame.bytes),
                &context.frame.length) == UART_M_FRAME_STATUS_OK);
            CHECK(FeedContext(&context, 0U, context.frame.length - 1U) == 0);
        }
        if (stateMode) {
            NetUpperCanMessage can = {0}, received;
            NetUpperEvent event = {0};
            NetUpperDelivery delivery;
            DoorState_t committed;
            can.id = 0x200U;
            can.data.bcm_door_state.boot_id = 201U;
            can.data.bcm_door_state.tx_sequence = 1U;
            can.data.bcm_door_state.update_sequence = 1U;
            event.kind = NET_UPPER_CAN_RX; event.link = NET_WIRE_CAN;
            event.capture.port_epoch = TEST_PORT_EPOCH;
            event.capture.captured_at_ms = g_provider.nowMs;
            event.capture.timestamp_known = true;
            CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY, &can, &event.can) == NET_WIRE_OK);
            CHECK(net_upper_capture(&g_service, &event) == NET_OK);
            (void)net_upper_step(&g_service, 1U);
            CHECK(DomainIf_QueryGetBcmDoorState(g_provider.nowMs, &committed) == DOMAIN_IF_OK);
            CHECK(committed.sourceGeneration == 201U);
            CHECK(net_upper_received_can(&g_service, 0x200U, &received, &delivery, &stamp) == NET_OK);
            CHECK(net_upper_uart_project_can(NET_WIRE_CANDIDATE_ONLY, &received,
                TEST_DEVICE, TEST_SESSION, TEST_NETWORK_BOOT, 0U, 1U, &message) == NET_WIRE_OK);
        }
        CHECK(net_upper_submit_uart_auto(&g_service, &message, &stamp, &policy, &key) == NET_OK);
        if (strcmp(argv[1], "dispatch_normal") == 0 ||
            strcmp(argv[1], "dispatch_state_ready") == 0 ||
            strcmp(argv[1], "dispatch_state_gateway_ready") == 0 ||
            strcmp(argv[1], "dispatch_borrowed") == 0) {
            (void)net_upper_dispatch(&g_service);
            CHECK(driverCalls == 1U);
            if (strcmp(argv[1], "dispatch_borrowed") == 0) {
                g_provider.p6Available = false;
                DomainSnapshotCommit_Poll(g_provider.nowMs);
                CHECK(net_upper_tx_snapshot(&g_service, key, &tx) == NET_OK);
                CHECK(tx.submitted && !tx.reference_released);
                CHECK(driverCalls == 1U);
            }
        } else {
            if (strcmp(argv[1], "dispatch_state_gateway_expired") == 0) {
                g_provider.nowMs += DOMAIN_MONITORING_CONTEXT_VALIDITY_MS;
            }
            else if (strcmp(argv[1], "dispatch_state_gateway_source_expired") == 0) {
                g_provider.nowMs += DOMAIN_MONITORING_CONNECTION_TTL_MS - 17U;
            }
            else if (strcmp(argv[1], "dispatch_state_gateway_revoked") == 0) {
                CHECK(DomainNetworkAdmissionBridgePrivate_OnLinkDown(&g_bridge) == DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
            }
            else if (strcmp(argv[1], "dispatch_expired") == 0)
                g_provider.nowMs += g_coordinator.admissionEvidenceValidityMs;
            else if (strcmp(argv[1], "dispatch_state_not_ready") == 0) {
                CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) == DOMAIN_IF_OK);
            }
            else if (strcmp(argv[1], "dispatch_state_ready_revoked") == 0 ||
                     strcmp(argv[1], "dispatch_state_poll_revoked") == 0)
                monitoringReady = false;
            else g_provider.p6Available = false;
            if (strcmp(argv[1], "dispatch_state_poll_revoked") == 0) {
                DomainSnapshotCommit_Poll(g_provider.nowMs);
                CHECK(!g_bridge.contextValid);
            }
            if (strcmp(argv[1], "dispatch_same_step") == 0 ||
                strcmp(argv[1], "dispatch_state_same_step") == 0) {
                CHECK(FeedContext(&context, context.frame.length - 1U, context.frame.length) == 0);
                CHECK(!g_bridge.contextValid); /* Revoked by real RX Owner before dispatch. */
            } else (void)net_upper_dispatch(&g_service);
            CHECK(driverCalls == 0U);
            CHECK(net_upper_tx_snapshot(&g_service, key, &tx) == NET_OK);
            CHECK(!tx.submitted && !tx.queued && tx.local_status == NET_REJECTED);
        }
        printf("%s PASS\n", argv[1]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "query_plan") == 0) {
        NetDomainC2Api pinned;
        NetDomainC2Config config;
        NetWireMQueryRaw request = {0};
        NetUpperQueryPlan plan;
        CHECK(NetDomainBoard_C2Config(&pinned, &config));
        CHECK(config.select_query_plan(NULL, &request, &plan) == NET_OK);
        CHECK(plan.count == 7U && plan.selected[6].getter == NET_QUERY_VSS_STATUS);
        request.query_scope = 1U;
        CHECK(config.select_query_plan(NULL, &request, &plan) == NET_OK);
        CHECK(plan.count == 1U && plan.selected[0].getter == NET_QUERY_RECENT_RESULTS);
        request.query_scope = 3U;
        CHECK(config.select_query_plan(NULL, &request, &plan) == NET_OK);
        CHECK(plan.count == 9U && plan.selected[8].getter == NET_QUERY_AVAILABILITY);
        request.query_scope = 2U;
        CHECK(config.select_query_plan(NULL, &request, &plan) == NET_UNSUPPORTED);
        puts("query_plan VSS/STATE/RESULT/ALL/WARNING PASS");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "copy") != 0 &&
        strcmp(argv[1], "p6_absent") != 0 &&
        strcmp(argv[1], "context_expiry") != 0 &&
        strcmp(argv[1], "zero_ttl") != 0 &&
        strcmp(argv[1], "p6_revoked") != 0)
        return ExistingPrivateBridgeMain(argc, argv);
    CHECK(TestStart(p6, 100U) == 0);
    api = g_adapter.api;
    api.RxDigitalKeyConnection = NULL;
    api.RxProximity = NULL;
    CHECK(DomainNetworkAdmissionBridgePrivate_BindApi(&g_bridge, &api) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    g_adapter.api = api;
    CHECK(api.RxDigitalKeyConnection != NULL && api.RxProximity != NULL);
    CHECK(EspContext(&context) == 0);
    metadata = TestMetadata(TEST_PORT_EPOCH, 100U, true);
    if (!p6)
    {
        CHECK(g_owner.uart(g_owner.context, &context, &metadata) == NET_HOLD);
        CHECK(!g_bridge.contextValid);
        CHECK(g_coordinator.inboxCount == 0U);
        puts("context_owner_without_p6: PASS");
        return 0;
    }
    CHECK(g_owner.uart(g_owner.context, &context, &metadata) == NET_OK);
    CHECK(g_bridge.connectionCaptured && g_bridge.proximityCaptured);
    CHECK(g_bridge.receivedConnection.sessionId == TEST_SESSION);
    CHECK(g_bridge.receivedConnection.meta.ageMs == 17U);
    CHECK(g_bridge.receivedConnection.meta.updateSequence == 8U);
    CHECK(g_bridge.contextValid);
    CHECK(TestDeliverRequest(TEST_DEVICE, TEST_SESSION, 77U, TEST_PORT_EPOCH,
                             2U, 100U, true) == NET_OK);
    CHECK(g_coordinator.inboxCount == 1U);
    CHECK(g_coordinator.inbox[0].request.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_coordinator.inbox[0].request.meta.ageMs == UINT32_MAX);
    if (argc > 1 && (strcmp(argv[1], "context_expiry") == 0 ||
        strcmp(argv[1], "zero_ttl") == 0 || strcmp(argv[1], "p6_revoked") == 0)) {
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE + 1U, TEST_SESSION) ==
              DOMAIN_IF_REJECTED);
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) == DOMAIN_IF_OK);
        if (strcmp(argv[1], "context_expiry") == 0) {
            g_provider.nowMs = 100U + g_coordinator.admissionEvidenceValidityMs - 1U;
            CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) == DOMAIN_IF_OK);
            ++g_provider.nowMs;
        }
        if (strcmp(argv[1], "zero_ttl") == 0) g_coordinator.admissionEvidenceValidityMs = 0U;
        if (strcmp(argv[1], "p6_revoked") == 0) g_provider.p6Available = false;
        DomainSnapshotCommit_Poll(g_provider.nowMs);
        CHECK(!g_bridge.contextValid && g_coordinator.inboxCount == 0U);
        CHECK(DomainIf_ValidatePassiveQueryContext(TEST_DEVICE, TEST_SESSION) != DOMAIN_IF_OK);
        printf("%s: PASS (Poll revokes context, inbox and passive Query)\n", argv[1]);
        return 0;
    }
    CHECK(DomainNetworkAdmissionBridgePrivate_OnLinkDown(&g_bridge) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK);
    CHECK(!g_bridge.contextValid && !g_bridge.connectionCaptured);
    CHECK(g_coordinator.inboxCount == 0U);
    puts("context_owner_copy_request_handoff_linkdown: PASS");
    return 0;
}
