#include "domain_network_admission_bridge_host.h"

#include <stdio.h>
#include <string.h>

#define TEST_NETWORK_BOOT 301U
#define TEST_DEVICE 10U
#define TEST_SESSION 20U
#define TEST_PORT_EPOCH 4U
#define TEST_VALIDITY_MS 64U
#define TEST_SLOT_COUNT 8U

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)fprintf(stderr, "FAIL %s:%d: %s\n",                       \
                          __FILE__, __LINE__, #condition);                     \
            return 1;                                                          \
        }                                                                      \
    } while (0)

typedef struct
{
    uint32_t nowMs;
    bool p6Available;
    bool mapAvailable;
} TestEvidenceProvider_t;

static DomainRequestCoordinator_t g_coordinator;
static CommandManager_Slot_t g_commandSlots[TEST_SLOT_COUNT];
static ResultManager_Slot_t g_resultSlots[TEST_SLOT_COUNT];
static DomainRequestCoordinator_Track_t g_tracks[TEST_SLOT_COUNT];
static DomainNetworkAdmissionBridgeHost_t g_bridge;
static NetUpper g_service;
static NetDomainC2 g_adapter;
static TestEvidenceProvider_t g_provider;
static NetUpperOwner g_owner;

static bool TestPolicyContext(
    void *context,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_AuthContext_t *outAuth,
    RequestManager_Context_t *outVehicleContext,
    RequestManager_Policy_t *outPolicy)
{
    (void)context;
    (void)request;
    (void)nowMs;
    (void)outAuth;
    (void)outVehicleContext;
    (void)outPolicy;
    return false;
}

static bool TestContextEvidence(
    void *context,
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    DomainRequestCoordinator_ContextSnapshot_t *outSnapshot)
{
    TestEvidenceProvider_t *provider = context;

    if ((provider == NULL) || (outSnapshot == NULL) ||
        !provider->p6Available || (deviceContextId == 0U) ||
        (sessionId == 0U))
    {
        return false;
    }
    (void)memset(outSnapshot, 0, sizeof(*outSnapshot));
    outSnapshot->linkUp = true;
    outSnapshot->registrationVerified = true;
    outSnapshot->registered = true;
    outSnapshot->connectionVerified = true;
    outSnapshot->connected = true;
    outSnapshot->sessionVerified = true;
    outSnapshot->p6AuthorizationVerified = true;
    return true;
}

static bool TestMapTime(
    void *context,
    uint64_t networkMs,
    uint32_t *outDomainMs)
{
    TestEvidenceProvider_t *provider = context;

    if ((provider == NULL) || (outDomainMs == NULL) ||
        !provider->mapAvailable)
    {
        return false;
    }
    /* Explicit Host fixture contract: both clocks are the same wrapping ms32
     * timebase. Production must supply its own reviewed mapping. */
    *outDomainMs = (uint32_t)networkMs;
    return true;
}

static bool TestNow(void *context, uint32_t *outNowMs)
{
    TestEvidenceProvider_t *provider = context;

    if ((provider == NULL) || (outNowMs == NULL))
    {
        return false;
    }
    *outNowMs = provider->nowMs;
    return true;
}

static bool TestC2Initialized(void)
{
    return true;
}

static DomainIf_Status_t TestSetTxPort(const DomainIf_TxPort_t *port)
{
    (void)port;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t TestRxConnection(
    DeviceContextId_t deviceContextId,
    const DigitalKeyConnectionState_t *connection)
{
    return ((deviceContextId != 0U) && (connection != NULL))
               ? DOMAIN_IF_OK
               : DOMAIN_IF_INVALID_ARGUMENT;
}

static DomainIf_Status_t TestRxProximity(
    DeviceContextId_t deviceContextId,
    const ProximityInput_t *proximity)
{
    return ((deviceContextId != 0U) && (proximity != NULL))
               ? DOMAIN_IF_OK
               : DOMAIN_IF_INVALID_ARGUMENT;
}

static NetStatus TestClock(void *context, uint64_t *outNowMs)
{
    TestEvidenceProvider_t *provider = context;

    if ((provider == NULL) || (outNowMs == NULL))
    {
        return NET_INVALID_ARGUMENT;
    }
    *outNowMs = provider->nowMs;
    return NET_OK;
}

static NetStatus TestSubmit(
    void *context,
    const NetUpperPacket *packet,
    NetTxKey key)
{
    (void)context;
    (void)packet;
    (void)key;
    return NET_OK;
}

static NetStatus TestPoll(void *context, NetUpperEvent *outEvent)
{
    (void)context;
    (void)outEvent;
    return NET_NO_WORK;
}

static const NetUpperPortOps g_portOps = {
    TestClock,
    TestSubmit,
    TestPoll
};

static int TestStart(bool withP6, uint32_t nowMs)
{
    DomainRequestCoordinator_Ports_t ports;
    ResultManager_Policy_t resultPolicy;
    NetDomainC2Api api;
    NetDomainC2Config adapterConfig;
    NetUpperConfig networkConfig;
    NetUpperOwner owner;
    NetStatus initStatus;
    size_t index;

    (void)memset(&g_coordinator, 0, sizeof(g_coordinator));
    (void)memset(g_commandSlots, 0, sizeof(g_commandSlots));
    (void)memset(g_resultSlots, 0, sizeof(g_resultSlots));
    (void)memset(g_tracks, 0, sizeof(g_tracks));
    (void)memset(&g_bridge, 0, sizeof(g_bridge));
    (void)memset(&g_service, 0, sizeof(g_service));
    (void)memset(&g_adapter, 0, sizeof(g_adapter));
    (void)memset(&g_provider, 0, sizeof(g_provider));
    (void)memset(&g_owner, 0, sizeof(g_owner));
    (void)memset(&ports, 0, sizeof(ports));
    (void)memset(&resultPolicy, 0, sizeof(resultPolicy));
    (void)memset(&api, 0, sizeof(api));
    (void)memset(&adapterConfig, 0, sizeof(adapterConfig));
    (void)memset(&networkConfig, 0, sizeof(networkConfig));
    DomainNetworkAdmissionBridgeHost_ResetBinding();

    g_provider.nowMs = nowMs;
    g_provider.p6Available = withP6;
    g_provider.mapAvailable = true;
    ports.getContext = TestPolicyContext;
    CHECK(DomainRequestCoordinator_Init(
              &g_coordinator,
              g_commandSlots,
              TEST_SLOT_COUNT,
              g_resultSlots,
              TEST_SLOT_COUNT,
              g_tracks,
              TEST_SLOT_COUNT,
              1U,
              &resultPolicy,
              &ports) == DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainRequestCoordinator_ConfigureAdmissionEvidence(
              &g_coordinator, TEST_VALIDITY_MS) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainNetworkAdmissionBridgeHost_Init(
              &g_bridge,
              &g_coordinator,
              TEST_NETWORK_BOOT,
              TestContextEvidence,
              TestMapTime,
              TestNow,
              &g_provider) == DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK);

    api.IsInitialized = TestC2Initialized;
    api.SetTxPort = TestSetTxPort;
    api.RxDigitalKeyConnection = TestRxConnection;
    api.RxProximity = TestRxProximity;
    CHECK(DomainNetworkAdmissionBridgeHost_BindApi(&g_bridge, &api) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK);

    adapterConfig.domain_boot = TEST_NETWORK_BOOT;
    adapterConfig.bcm_boot = 201U;
    adapterConfig.first_can_sequence = 1U;
    adapterConfig.command_accept_limit_ms = 200U;
    adapterConfig.event_use_limit_ms = 200U;
    adapterConfig.transmit_policy.bound = true;
    adapterConfig.transmit_policy.maximum_age_ms = 1000U;
    adapterConfig.transmit_policy.expires_at_equality = true;
    adapterConfig.value_quality = net_domain_c2_value_quality_provisional;
    initStatus = net_domain_c2_init(
        &g_adapter, &g_service, NULL, &api, &adapterConfig);
    if (initStatus != NET_OK)
    {
        (void)fprintf(stderr, "net_domain_c2_init status=%d\n",
                      (int)initStatus);
        return 1;
    }

    networkConfig.node = NET_WIRE_NODE_DOMAIN;
    networkConfig.local_epoch = 3U;
    networkConfig.profile = NET_WIRE_CANDIDATE_ONLY;
    networkConfig.port.ops = &g_portOps;
    networkConfig.port.context = &g_provider;
    owner = DomainNetworkAdmissionBridgeHost_WrapOwner(
        &g_bridge, net_domain_c2_owner(&g_adapter));
    CHECK(owner.uart != NULL);
    g_owner = owner;
    networkConfig.owner = owner;
    for (index = 0U; index < 3U; ++index)
    {
        networkConfig.port_epoch[index] = TEST_PORT_EPOCH;
    }
    for (index = 0U; index < NET_UPPER_ROUTE_COUNT; ++index)
    {
        networkConfig.rx_policy[index].bound = true;
        networkConfig.rx_policy[index].maximum_age_ms = 1000U;
    }
    for (index = 0U; index < 2U; ++index)
    {
        networkConfig.uart_policy[index].candidate_time_policy_selected = true;
        networkConfig.uart_policy[index].assembly_limit_ms = 100U;
        networkConfig.uart_policy[index].expires_at_equality = true;
        networkConfig.uart_sequence_bound[index] = true;
        networkConfig.uart_initial_sequence[index] = 1U;
    }
    CHECK(net_upper_init(&g_service, &networkConfig) == NET_OK);
    CHECK(net_domain_c2_register(&g_adapter) == NET_OK);
    return 0;
}

static NetUpperDelivery TestMetadata(
    uint64_t portEpoch,
    uint64_t capturedAtMs,
    bool timestampKnown)
{
    NetUpperDelivery metadata;

    (void)memset(&metadata, 0, sizeof(metadata));
    metadata.capture.port_epoch = portEpoch;
    metadata.capture.captured_at_ms = capturedAtMs;
    metadata.capture.timestamp_known = timestampKnown;
    metadata.delivered_at_ms = g_provider.nowMs;
    metadata.receipt_id = 1U;
    return metadata;
}

static NetUpperUartDelivery TestContextDelivery(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint64_t portEpoch,
    uint16_t sequence)
{
    NetUpperUartDelivery delivery;

    (void)memset(&delivery, 0, sizeof(delivery));
    delivery.port_epoch = portEpoch;
    delivery.message.link = NET_WIRE_UART_M;
    delivery.message.type = 0x01U;
    delivery.message.sequence = sequence;
    delivery.message.data.m_context.device_context_id = deviceContextId;
    delivery.message.data.m_context.session_id = sessionId;
    delivery.message.data.m_context.device_registration_state = 1U;
    delivery.message.data.m_context.bt_connection_state = 1U;
    delivery.message.data.m_context.app_active_state = 1U;
    delivery.message.data.m_context.proximity_validity = 1U;
    return delivery;
}

static NetUpperUartDelivery TestRequestDelivery(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    RequestId_t requestId,
    uint64_t portEpoch,
    uint16_t sequence)
{
    NetUpperUartDelivery delivery;

    (void)memset(&delivery, 0, sizeof(delivery));
    delivery.port_epoch = portEpoch;
    delivery.message.link = NET_WIRE_UART_M;
    delivery.message.type = 0x10U;
    delivery.message.sequence = sequence;
    delivery.message.data.m_request.device_context_id = deviceContextId;
    delivery.message.data.m_request.session_id = sessionId;
    delivery.message.data.m_request.request_id = requestId;
    delivery.message.data.m_request.kind = 1U;
    delivery.message.data.m_request.operation = 0U;
    return delivery;
}

static NetStatus TestDeliverContext(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint64_t portEpoch,
    uint16_t sequence,
    uint64_t capturedAtMs,
    bool timestampKnown)
{
    NetUpperUartDelivery delivery = TestContextDelivery(
        deviceContextId, sessionId, portEpoch, sequence);
    NetUpperDelivery metadata = TestMetadata(
        portEpoch, capturedAtMs, timestampKnown);
    NetStatus status;

    status = g_owner.uart(g_owner.context, &delivery, &metadata);
    if (status != NET_OK)
    {
        (void)fprintf(stderr, "context delivery status=%d\n", (int)status);
    }
    return status;
}

static NetStatus TestDeliverRequest(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    RequestId_t requestId,
    uint64_t portEpoch,
    uint16_t sequence,
    uint64_t capturedAtMs,
    bool timestampKnown)
{
    NetUpperUartDelivery delivery = TestRequestDelivery(
        deviceContextId, sessionId, requestId, portEpoch, sequence);
    NetUpperDelivery metadata = TestMetadata(
        portEpoch, capturedAtMs, timestampKnown);

    return g_owner.uart(g_owner.context, &delivery, &metadata);
}

static int TestValidCopy(void)
{
    NetUpperUartDelivery delivery;
    NetUpperDelivery metadata;

    CHECK(TestStart(true, 100U) == 0);
    CHECK(g_bridge.initialized);
    CHECK(!g_bridge.inOwnerCall);
    CHECK(g_bridge.delegate.uart != NULL);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_OK);
    delivery = TestRequestDelivery(
        TEST_DEVICE, TEST_SESSION, 40U, TEST_PORT_EPOCH, 2U);
    metadata = TestMetadata(TEST_PORT_EPOCH, 101U, true);
    g_provider.nowMs = 101U;
    CHECK(g_owner.uart(g_owner.context, &delivery, &metadata) == NET_OK);
    CHECK(g_coordinator.inboxCount == 1U);
    CHECK(g_coordinator.inbox[0].request.context.requestId == 40U);
    CHECK(g_coordinator.inbox[0].request.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_coordinator.inbox[0].request.meta.ageMs == UINT32_MAX);
    CHECK(g_coordinator.inbox[0].admission.portEpoch == TEST_PORT_EPOCH);
    CHECK(g_coordinator.inbox[0].admission.linkSequence == 2U);
    CHECK(g_coordinator.inbox[0].admission.capturedAtMs == 101U);
    delivery.message.data.m_request.request_id = 99U;
    metadata.capture.captured_at_ms = 999U;
    CHECK(g_coordinator.inbox[0].request.context.requestId == 40U);
    CHECK(g_coordinator.inbox[0].admission.capturedAtMs == 101U);
    return 0;
}

static int TestContextMismatch(void)
{
    CHECK(TestStart(true, 100U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_OK);
    g_provider.nowMs = 101U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE + 1U, TEST_SESSION, 41U, TEST_PORT_EPOCH,
              2U, 101U, true) == NET_HOLD);
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION + 1U, 42U, TEST_PORT_EPOCH,
              3U, 101U, true) == NET_HOLD);
    CHECK(g_coordinator.inboxCount == 0U);
    CHECK(g_bridge.requestDeliveryCount == 0U);
    return 0;
}

static int TestContextChange(void)
{
    CHECK(TestStart(true, 100U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_OK);
    g_provider.nowMs = 101U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 43U, TEST_PORT_EPOCH,
              2U, 101U, true) == NET_OK);
    CHECK(g_coordinator.inboxCount == 1U);
    g_provider.nowMs = 102U;
    CHECK(TestDeliverContext(
              TEST_DEVICE + 1U, TEST_SESSION + 1U, TEST_PORT_EPOCH,
              3U, 102U, true) == NET_OK);
    CHECK(g_coordinator.inboxCount == 0U);
    CHECK(g_coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_CONTEXT_CHANGED);
    CHECK(g_bridge.currentContext.contextRevision == 2U);
    return 0;
}

static int TestLifecycleInvalidation(void)
{
    CHECK(TestStart(true, 100U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_OK);
    g_provider.nowMs = 101U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 44U, TEST_PORT_EPOCH,
              2U, 101U, true) == NET_OK);
    CHECK(DomainNetworkAdmissionBridgeHost_OnLinkDown(&g_bridge) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK);
    CHECK(g_coordinator.inboxCount == 0U);
    CHECK(g_coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN);

    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 3U, 102U, true) ==
          NET_OK);
    g_provider.nowMs = 103U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 45U, TEST_PORT_EPOCH,
              4U, 103U, true) == NET_OK);
    CHECK(DomainNetworkAdmissionBridgeHost_OnPortEpochChanged(
              &g_bridge, TEST_PORT_EPOCH + 1U) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK);
    CHECK(g_coordinator.inboxCount == 0U);
    CHECK(g_coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED);

    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 5U, 104U, true) ==
          NET_OK);
    g_provider.nowMs = 105U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 46U, TEST_PORT_EPOCH,
              6U, 105U, true) == NET_OK);
    CHECK(DomainNetworkAdmissionBridgeHost_OnNetworkBootChanged(
              &g_bridge, TEST_NETWORK_BOOT + 1U) ==
          DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK);
    CHECK(g_coordinator.inboxCount == 0U);
    CHECK(g_coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED);
    return 0;
}

static int TestP6Absent(void)
{
    CHECK(TestStart(false, 100U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_HOLD);
    CHECK(!g_coordinator.admissionContextValid);
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 47U, TEST_PORT_EPOCH,
              2U, 101U, true) == NET_HOLD);
    CHECK(g_coordinator.inboxCount == 0U);
    return 0;
}

static int TestTimestampAndExpiry(void)
{
    CHECK(TestStart(true, 100U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH, 1U, 100U, true) ==
          NET_OK);
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 48U, TEST_PORT_EPOCH,
              2U, 101U, false) == NET_HOLD);
    CHECK(g_coordinator.inboxCount == 0U);
    g_provider.nowMs = 200U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 49U, TEST_PORT_EPOCH,
              3U, 100U, true) == NET_HOLD);
    CHECK(g_coordinator.inboxCount == 0U);
    return 0;
}

static int TestCapacityReplayAndRollover(void)
{
    uint16_t sequences[] = {UINT16_MAX - 1U, UINT16_MAX, 0U, 1U};
    size_t index;

    CHECK(TestStart(true, UINT32_MAX - 15U) == 0);
    CHECK(TestDeliverContext(
              TEST_DEVICE, TEST_SESSION, TEST_PORT_EPOCH,
              UINT16_MAX - 2U, UINT32_MAX - 15U, true) == NET_OK);
    for (index = 0U; index < DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
         ++index)
    {
        g_provider.nowMs = (uint32_t)(UINT32_MAX - 14U + (uint32_t)index);
        CHECK(TestDeliverRequest(
                  TEST_DEVICE, TEST_SESSION, (RequestId_t)(50U + index),
                  TEST_PORT_EPOCH, sequences[index],
                  (uint32_t)(UINT32_MAX - 14U + (uint32_t)index), true) ==
              NET_OK);
    }
    CHECK(g_coordinator.inboxCount ==
          DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY);
    g_provider.nowMs = 2U;
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 54U, TEST_PORT_EPOCH,
              2U, 2U, true) == NET_NO_CAPACITY);
    CHECK(TestDeliverRequest(
              TEST_DEVICE, TEST_SESSION, 55U, TEST_PORT_EPOCH,
              1U, 2U, true) != NET_OK);
    CHECK(g_coordinator.inboxCount ==
          DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY);
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2)
    {
        return 2;
    }
    if (strcmp(argv[1], "valid_copy") == 0)
    {
        result = TestValidCopy();
    }
    else if (strcmp(argv[1], "context_mismatch") == 0)
    {
        result = TestContextMismatch();
    }
    else if (strcmp(argv[1], "context_change") == 0)
    {
        result = TestContextChange();
    }
    else if (strcmp(argv[1], "lifecycle_invalidation") == 0)
    {
        result = TestLifecycleInvalidation();
    }
    else if (strcmp(argv[1], "p6_absent") == 0)
    {
        result = TestP6Absent();
    }
    else if (strcmp(argv[1], "timestamp_expiry") == 0)
    {
        result = TestTimestampAndExpiry();
    }
    else if (strcmp(argv[1], "capacity_replay_rollover") == 0)
    {
        result = TestCapacityReplayAndRollover();
    }
    else
    {
        return 2;
    }
    if (result == 0)
    {
        (void)printf("Domain admission bridge %s PASS\n", argv[1]);
    }
    DomainNetworkAdmissionBridgeHost_ResetBinding();
    return result;
}
