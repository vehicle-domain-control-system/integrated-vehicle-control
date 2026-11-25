#include "domain_runtime.h"
#include "network/net_domain_c2.h"

#include <stdio.h>
#include <string.h>

#define TEST_DOMAIN_BOOT 301U
#define TEST_BCM_BOOT 201U
#define TEST_DEVICE 10U
#define TEST_SESSION 20U
#define TEST_PORT_EPOCH 4U
#define TEST_DOOR_TTL_MS 500U
#define TEST_RESULT_TIMEOUT_MS 100U
#define TEST_PACKET_CAPACITY 8U

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n",                        \
                          __FILE__, __LINE__, #condition);                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

typedef struct
{
    uint32_t nowMs;
    NetUpperPacket packets[TEST_PACKET_CAPACITY];
    size_t packetCount;
} TestPort_t;

static DomainRuntime_t g_runtime;
static NetUpper g_network;
static NetUpperQuery g_query;
static NetDomainC2 g_adapter;
static TestPort_t g_port;

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
{
    TestPort_t *port = (TestPort_t *)context;

    if ((port == NULL) || (outNowMs == NULL))
    {
        return false;
    }
    *outNowMs = port->nowMs;
    return true;
}

static NetStatus TestPort_GetNow(void *context, uint64_t *outNowMs)
{
    TestPort_t *port = (TestPort_t *)context;

    if ((port == NULL) || (outNowMs == NULL))
    {
        return NET_INVALID_ARGUMENT;
    }
    *outNowMs = port->nowMs;
    return NET_OK;
}

static NetStatus TestPort_Submit(
    void *context,
    const NetUpperPacket *packet,
    NetTxKey key)
{
    TestPort_t *port = (TestPort_t *)context;

    (void)key;
    if ((port == NULL) || (packet == NULL))
    {
        return NET_INVALID_ARGUMENT;
    }
    if (port->packetCount >= TEST_PACKET_CAPACITY)
    {
        return NET_NO_CAPACITY;
    }
    port->packets[port->packetCount++] = *packet;
    return NET_OK;
}

static NetStatus TestPort_Poll(void *context, NetUpperEvent *outEvent)
{
    (void)context;
    (void)outEvent;
    return NET_NO_WORK;
}

static const NetUpperPortOps g_portOps = {
    TestPort_GetNow,
    TestPort_Submit,
    TestPort_Poll
};

static bool TestAuthorize(
    void *context,
    const DomainIf_MobileRequest_t *request,
    RequestManager_AuthContext_t *outAuth,
    RequestManager_FunctionGate_t *outDoorGate)
{
    (void)context;
    if ((request == NULL) || (outAuth == NULL) || (outDoorGate == NULL))
    {
        return false;
    }
    (void)memset(outAuth, 0, sizeof(*outAuth));
    (void)memset(outDoorGate, 0, sizeof(*outDoorGate));
    outAuth->registrationVerified = true;
    outAuth->registered = true;
    outAuth->connectionVerified = true;
    outAuth->connected = true;
    outAuth->sessionVerified = true;
    outAuth->currentDeviceContextId = request->deviceContextId;
    outAuth->currentSessionId = request->context.sessionId;
    outDoorGate->supportKnown = true;
    outDoorGate->supported = true;
    outDoorGate->permissionKnown = true;
    outDoorGate->operationAllowed = true;
    return true;
}

static DomainRuntime_Config_t TestRuntimeConfig(void)
{
    DomainRuntime_Config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.timeProvider = TestClock_GetNow;
    config.timeProviderContext = &g_port;
    config.firstCommandId = 1U;
    config.resultPolicy.firstResponseTimeoutEnabled = true;
    config.resultPolicy.firstResponseTimeoutMs = TEST_RESULT_TIMEOUT_MS;
    config.resultPolicy.finalResultTimeoutEnabled = true;
    config.resultPolicy.finalResultTimeoutMs = TEST_RESULT_TIMEOUT_MS;
    config.resultPolicy.doorEvidenceTtlEnabled = true;
    config.resultPolicy.doorEvidenceTtlMs = TEST_DOOR_TTL_MS;
    config.requestCoordinatorEnabled = true;
    config.bcmDoorStateTtlMs = TEST_DOOR_TTL_MS;
    config.mobileRequestSourceValidityMs = TEST_DOOR_TTL_MS;
    config.requestAuthorizationProvider = TestAuthorize;
    config.permissionCoordinatorEnabled = false;
    config.cisOccupantEnabled = false;
    config.stateQueryPolicy.bcmDoorTtlMs = TEST_DOOR_TTL_MS;
    config.stateQueryPolicy.bcmStatusTtlMs = TEST_DOOR_TTL_MS;
    config.stateQueryPolicy.cisRearTtlMs = TEST_DOOR_TTL_MS;
    config.stateQueryPolicy.cisStatusTtlMs = TEST_DOOR_TTL_MS;
    return config;
}

static NetStatus TestLocalFailureReason(
    void *context,
    const NetUpperTxSnapshot *snapshot,
    NetworkLocalFailureReason_t *outReason)
{
    (void)context;
    if ((snapshot == NULL) || (outReason == NULL))
    {
        return NET_INVALID_ARGUMENT;
    }
    if (snapshot->local_status != NET_REJECTED)
    {
        return NET_HOLD;
    }
    *outReason = NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;
    return NET_OK;
}

static int TestStart(bool bindMobileIngress)
{
    DomainRuntime_Config_t runtimeConfig = TestRuntimeConfig();
    NetDomainC2Api api;
    NetDomainC2Config adapterConfig;
    NetUpperConfig networkConfig;
    size_t index;

    (void)memset(&g_runtime, 0, sizeof(g_runtime));
    (void)memset(&g_network, 0, sizeof(g_network));
    (void)memset(&g_query, 0, sizeof(g_query));
    (void)memset(&g_adapter, 0, sizeof(g_adapter));
    (void)memset(&g_port, 0, sizeof(g_port));
    (void)memset(&api, 0, sizeof(api));
    (void)memset(&adapterConfig, 0, sizeof(adapterConfig));
    (void)memset(&networkConfig, 0, sizeof(networkConfig));
    g_port.nowMs = 10U;

    CHECK(DomainRuntime_Init(&g_runtime, &runtimeConfig) == DOMAIN_RUNTIME_OK);
    CHECK(DomainRuntime_IsReady(&g_runtime));

    net_domain_c2_pinned_owner_api(&api);
    if (bindMobileIngress)
    {
        /* Test-only exposure of the existing public C2 ingress. The product
         * pinned owner intentionally leaves this callback NULL today. */
        api.RxMobileRequest = DomainIf_RxMobileRequest;
    }

    adapterConfig.domain_boot = TEST_DOMAIN_BOOT;
    adapterConfig.bcm_boot = TEST_BCM_BOOT;
    adapterConfig.first_can_sequence = 1U;
    adapterConfig.command_accept_limit_ms = 200U;
    adapterConfig.event_use_limit_ms = 200U;
    adapterConfig.transmit_policy.bound = true;
    adapterConfig.transmit_policy.maximum_age_ms = 1000U;
    adapterConfig.transmit_policy.expires_at_equality = true;
    adapterConfig.value_quality = net_domain_c2_value_quality_provisional;
    adapterConfig.failure_sink_ready = true;
    adapterConfig.local_failure_reason = TestLocalFailureReason;
    CHECK(net_domain_c2_init(
              &g_adapter, &g_network, &g_query, &api, &adapterConfig) == NET_OK);

    networkConfig.node = NET_WIRE_NODE_DOMAIN;
    networkConfig.local_epoch = 3U;
    networkConfig.profile = NET_WIRE_CANDIDATE_ONLY;
    networkConfig.port.ops = &g_portOps;
    networkConfig.port.context = &g_port;
    networkConfig.owner = net_domain_c2_owner(&g_adapter);
    for (index = 0U; index < 3U; ++index)
    {
        networkConfig.port_epoch[index] = TEST_PORT_EPOCH;
    }
    for (index = 0U; index < NET_UPPER_ROUTE_COUNT; ++index)
    {
        networkConfig.rx_policy[index].bound = true;
        networkConfig.rx_policy[index].maximum_age_ms = 10000U;
    }
    for (index = 0U; index < 2U; ++index)
    {
        networkConfig.uart_policy[index].candidate_time_policy_selected = true;
        networkConfig.uart_policy[index].assembly_limit_ms = 100U;
        networkConfig.uart_policy[index].expires_at_equality = true;
        networkConfig.uart_sequence_bound[index] = true;
        networkConfig.uart_initial_sequence[index] = 1U;
    }
    CHECK(net_upper_init(&g_network, &networkConfig) == NET_OK);

    /* Test fixture for a previously admitted UART-M peer. This is not an
     * authentication substitute; TestAuthorize remains the policy evidence. */
    g_network.context_seen = true;
    g_network.context_port_epoch = TEST_PORT_EPOCH;
    g_network.context.device_context_id = TEST_DEVICE;
    g_network.context.session_id = TEST_SESSION;
    CHECK(net_domain_c2_register(&g_adapter) == NET_OK);
    return 0;
}

static DoorState_t TestDoorState(DoorLockState_t lockState)
{
    DoorState_t state;

    (void)memset(&state, 0, sizeof(state));
    state.lockState = lockState;
    state.openState = DOOR_OPEN_STATE_CLOSED;
    state.compositeState = DOOR_COMPOSITE_STATE_NORMAL;
    state.lockQuality = DATA_QUALITY_OK;
    state.openQuality = DATA_QUALITY_OK;
    state.compositeQuality = DATA_QUALITY_OK;
    state.sourceGeneration = TEST_BCM_BOOT;
    state.meta.quality = DATA_QUALITY_OK;
    state.meta.updateSequence = 1U;
    return state;
}

static DomainIf_MobileRequest_t TestDoorRequest(
    RequestId_t requestId,
    DoorLockTarget_t target)
{
    DomainIf_MobileRequest_t request;

    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = TEST_DEVICE;
    request.context.sessionId = TEST_SESSION;
    request.context.requestId = requestId;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = target;
    request.meta.quality = DATA_QUALITY_OK;
    request.meta.updateSequence = requestId;
    return request;
}

static const NetUpperPacket *TestFindPacket(NetWireLink link, size_t ordinal)
{
    size_t index;

    for (index = 0U; index < g_port.packetCount; ++index)
    {
        if (g_port.packets[index].link == link)
        {
            if (ordinal == 0U)
            {
                return &g_port.packets[index];
            }
            --ordinal;
        }
    }
    return NULL;
}

static int TestCommandAndAcceptedResult(DoorLockTarget_t target)
{
    DoorState_t door = TestDoorState(
        (target == DOOR_TARGET_LOCK) ? DOOR_LOCK_STATE_UNLOCKED
                                     : DOOR_LOCK_STATE_LOCKED);
    DomainIf_MobileRequest_t request = TestDoorRequest(40U, target);
    NetWireBcmDoorCommandRaw command;
    NetWireMResult result;
    NetWireUartPayload uartPayload;
    uint16_t sequence;
    const NetUpperPacket *packet;

    CHECK(TestStart(false) == 0);
    CHECK(DomainIf_RxBcmDoorState(&door) == DOMAIN_IF_OK);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(g_port.packetCount == 0U);
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    (void)net_upper_step(&g_network, 0U);
    CHECK(g_port.packetCount == 2U);

    packet = TestFindPacket(NET_WIRE_CAN, 0U);
    CHECK(packet != NULL);
    CHECK(packet->can.standard_id == 0x100U);
    CHECK(net_wire_can_decode_bcm_door_command(
              NET_WIRE_CANDIDATE_ONLY, &packet->can, &command) == NET_WIRE_OK);
    CHECK(command.command_id == 1U);
    CHECK(command.target_boot_id == TEST_BCM_BOOT);
    CHECK(command.door_lock_target == (uint8_t)target);

    packet = TestFindPacket(NET_WIRE_UART_M, 0U);
    CHECK(packet != NULL);
    CHECK(net_wire_uart_decode(
              NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
              &packet->uart, &uartPayload) == NET_WIRE_OK);
    CHECK(uartPayload.type == 0x20U);
    CHECK(net_wire_m_result_decode(
              NET_WIRE_CANDIDATE_ONLY, &packet->uart, &result, &sequence) ==
          NET_WIRE_OK);
    CHECK(result.device_context_id == TEST_DEVICE);
    CHECK(result.request_session == TEST_SESSION);
    CHECK(result.request_id == request.context.requestId);
    CHECK(result.command_id == command.command_id);
    CHECK(result.command_target == 1U);
    return 0;
}

static int TestBcmResultDecoderAndDuplicate(void)
{
    DoorState_t door = TestDoorState(DOOR_LOCK_STATE_UNLOCKED);
    DomainIf_MobileRequest_t request = TestDoorRequest(41U, DOOR_TARGET_LOCK);
    NetUpperCanMessage message;
    NetUpperEvent event;
    size_t packetCountAfterResult;
    ResultManager_RequestKey_t key;
    ResultManager_Snapshot_t resultSnapshot;

    CHECK(TestStart(false) == 0);
    CHECK(DomainIf_RxBcmDoorState(&door) == DOMAIN_IF_OK);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(g_port.packetCount == 0U);
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    (void)net_upper_step(&g_network, 0U);

    (void)memset(&message, 0, sizeof(message));
    message.id = 0x140U;
    message.data.bcm_result.boot_id = TEST_BCM_BOOT;
    message.data.bcm_result.tx_sequence = 1U;
    message.data.bcm_result.command_domain_boot = TEST_DOMAIN_BOOT;
    message.data.bcm_result.command_id = 1U;
    message.data.bcm_result.command_kind = 1U;
    message.data.bcm_result.result = 2U;
    message.data.bcm_result.result_reason = 0U;
    message.data.bcm_result.door_event = 1U;
    message.data.bcm_result.occurrence_id = 1U;
    message.data.bcm_result.event_age_ms = 1U;
    message.data.bcm_result.event_quality = 0U;
    message.data.bcm_result.door_target = 0U;
    (void)memset(&event, 0, sizeof(event));
    event.kind = NET_UPPER_CAN_RX;
    event.link = NET_WIRE_CAN;
    event.capture.port_epoch = TEST_PORT_EPOCH;
    event.capture.captured_at_ms = g_port.nowMs;
    event.capture.timestamp_known = true;
    CHECK(net_upper_can_encode(
              NET_WIRE_CANDIDATE_ONLY, &message, &event.can) == NET_WIRE_OK);
    CHECK(net_upper_capture(&g_network, &event) == NET_OK);
    CHECK(net_upper_step(&g_network, 1U) == NET_OK);
    (void)net_upper_step(&g_network, 0U);
    packetCountAfterResult = g_port.packetCount;
    CHECK(packetCountAfterResult == 2U);
    key.deviceContextId = request.deviceContextId;
    key.context = request.context;
    CHECK(ResultManager_GetSnapshot(
              &g_runtime.resultManager, &key, &resultSnapshot) ==
          RESULT_MANAGER_OK);
    CHECK(resultSnapshot.hasUnconfirmedBcmResult);
    CHECK(resultSnapshot.outcomeUnconfirmed);
    /* The real adapter rejects a nested TX while its RX callback owns the
     * service. The result is committed, but no fake UART success is claimed. */
    CHECK(g_runtime.requestCoordinator.lastPublishStatus ==
          DOMAIN_IF_NOT_AVAILABLE);

    CHECK(net_upper_capture(&g_network, &event) == NET_OK);
    (void)net_upper_step(&g_network, 1U);
    (void)net_upper_step(&g_network, 0U);
    CHECK(g_port.packetCount == packetCountAfterResult);
    return 0;
}

static int TestTimeoutAndUnavailable(void)
{
    DoorState_t door = TestDoorState(DOOR_LOCK_STATE_UNLOCKED);
    DomainIf_MobileRequest_t request = TestDoorRequest(42U, DOOR_TARGET_LOCK);
    const NetUpperPacket *packet;
    NetWireMResult result;
    uint16_t sequence;

    CHECK(TestStart(false) == 0);
    CHECK(DomainIf_RxBcmDoorState(&door) == DOMAIN_IF_OK);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(g_port.packetCount == 0U);
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(net_upper_step(&g_network, 0U) == NET_OK);
    g_port.nowMs += TEST_RESULT_TIMEOUT_MS;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(net_upper_step(&g_network, 0U) == NET_OK);
    CHECK(g_port.packetCount == 3U);
    packet = TestFindPacket(NET_WIRE_UART_M, 1U);
    CHECK(packet != NULL);
    CHECK(net_wire_m_result_decode(
              NET_WIRE_CANDIDATE_ONLY, &packet->uart, &result, &sequence) ==
          NET_WIRE_OK);
    CHECK(result.request_id == request.context.requestId);
    CHECK(result.result == 255U);

    CHECK(net_domain_c2_unregister(&g_adapter) == NET_BUSY);
    return 0;
}

static int TestUnavailableBeforeSubmission(void)
{
    DoorState_t door = TestDoorState(DOOR_LOCK_STATE_UNLOCKED);
    DomainIf_MobileRequest_t request = TestDoorRequest(43U, DOOR_TARGET_LOCK);

    CHECK(TestStart(false) == 0);
    CHECK(net_domain_c2_unregister(&g_adapter) == NET_OK);
    CHECK(DomainIf_RxBcmDoorState(&door) == DOMAIN_IF_OK);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    CHECK(g_port.packetCount == 0U);
    return 0;
}

static int TestUartRequestFailsClosedWithoutSourceMeta(void)
{
    DoorState_t door = TestDoorState(DOOR_LOCK_STATE_UNLOCKED);
    NetUpperOwner owner;
    NetUpperUartDelivery delivery;
    NetUpperDelivery metadata;

    CHECK(TestStart(true) == 0);
    CHECK(DomainIf_RxBcmDoorState(&door) == DOMAIN_IF_OK);
    owner = net_domain_c2_owner(&g_adapter);
    (void)memset(&delivery, 0, sizeof(delivery));
    (void)memset(&metadata, 0, sizeof(metadata));
    delivery.port_epoch = TEST_PORT_EPOCH;
    delivery.message.link = NET_WIRE_UART_M;
    delivery.message.type = 0x10U;
    delivery.message.sequence = 1U;
    delivery.message.data.m_request.device_context_id = TEST_DEVICE;
    delivery.message.data.m_request.session_id = TEST_SESSION;
    delivery.message.data.m_request.request_id = 44U;
    delivery.message.data.m_request.kind = 1U;
    delivery.message.data.m_request.operation = 0U;
    metadata.capture.port_epoch = TEST_PORT_EPOCH;
    metadata.capture.captured_at_ms = g_port.nowMs;
    metadata.capture.timestamp_known = true;
    metadata.receipt_id = 1U;
    CHECK(owner.uart(owner.context, &delivery, &metadata) != NET_OK);
    CHECK(g_port.packetCount == 0U);
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2)
    {
        return 2;
    }
    if (strcmp(argv[1], "lock") == 0)
    {
        result = TestCommandAndAcceptedResult(DOOR_TARGET_LOCK);
    }
    else if (strcmp(argv[1], "unlock") == 0)
    {
        result = TestCommandAndAcceptedResult(DOOR_TARGET_UNLOCK);
    }
    else if (strcmp(argv[1], "bcm_result_duplicate") == 0)
    {
        result = TestBcmResultDecoderAndDuplicate();
    }
    else if (strcmp(argv[1], "timeout") == 0)
    {
        result = TestTimeoutAndUnavailable();
    }
    else if (strcmp(argv[1], "unavailable") == 0)
    {
        result = TestUnavailableBeforeSubmission();
    }
    else if (strcmp(argv[1], "uart_missing_source_meta") == 0)
    {
        result = TestUartRequestFailsClosedWithoutSourceMeta();
    }
    else
    {
        return 2;
    }
    if (result == 0)
    {
        (void)printf("Domain Network E2E %s PASS\n", argv[1]);
    }
    return result;
}
