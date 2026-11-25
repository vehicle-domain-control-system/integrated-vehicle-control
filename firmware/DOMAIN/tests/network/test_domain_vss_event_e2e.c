#include "network/net_domain_c2.h"
#include "network/net_upper_publish.h"

#include "domain_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

#define DOMAIN_BOOT 301U
#define BCM_BOOT 501U
#define MAX_CAPTURED 32U

typedef struct
{
    NetTxKey key;
    NetUpperPacket packet;
    bool open;
} CapturedPacket_t;

static NetUpper g_network;
static NetUpperPublisher g_publisher;
static NetDomainC2 g_adapter;
static DomainRuntime_t g_runtime;
static uint64_t g_networkNowMs;
static uint32_t g_runtimeNowMs;
static CapturedPacket_t g_captured[MAX_CAPTURED];
static size_t g_capturedCount;

static NetStatus NetworkClock(void *context, uint64_t *outNowMs)
{
    (void)context;
    *outNowMs = g_networkNowMs;
    return NET_OK;
}

static bool RuntimeClock(void *context, uint32_t *outNowMs)
{
    (void)context;
    *outNowMs = g_runtimeNowMs;
    return true;
}

static bool EventGeneration(void *context,
                            SourceGeneration_t *outGeneration)
{
    (void)context;
    *outGeneration = DOMAIN_BOOT;
    return true;
}

static NetStatus Submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    (void)context;
    if ((packet == NULL) || (g_capturedCount >= MAX_CAPTURED))
    {
        return NET_NO_CAPACITY;
    }
    g_captured[g_capturedCount].key = key;
    g_captured[g_capturedCount].packet = *packet;
    g_captured[g_capturedCount].open = true;
    ++g_capturedCount;
    return NET_OK;
}

static NetStatus Poll(void *context, NetUpperEvent *event)
{
    (void)context;
    (void)event;
    return NET_NO_WORK;
}

static const NetUpperPortOps g_portOps = { NetworkClock, Submit, Poll };

static int Start(uint32_t nowMs)
{
    static const NetUpperPublishEntry entries[] = {
        { NET_WIRE_CAN, 0x0C0U, 50U,
          { true, UINT16_MAX + 1000U, true } }
    };
    DomainRuntime_Config_t runtimeConfig;
    NetDomainC2Api api;
    NetDomainC2Config adapterConfig;
    NetUpperConfig networkConfig;
    NetUpperPublishConfig publishConfig;
    size_t index;

    (void)memset(&g_network, 0, sizeof(g_network));
    (void)memset(&g_publisher, 0, sizeof(g_publisher));
    (void)memset(&g_adapter, 0, sizeof(g_adapter));
    (void)memset(&g_runtime, 0, sizeof(g_runtime));
    (void)memset(g_captured, 0, sizeof(g_captured));
    g_capturedCount = 0U;
    g_networkNowMs = 1000U;
    g_runtimeNowMs = nowMs;

    (void)memset(&runtimeConfig, 0, sizeof(runtimeConfig));
    runtimeConfig.timeProvider = RuntimeClock;
    runtimeConfig.vssEventGenerationProvider = EventGeneration;
    runtimeConfig.firstCommandId = 1U;
    runtimeConfig.resultPolicy.firstResponseTimeoutEnabled = true;
    runtimeConfig.resultPolicy.firstResponseTimeoutMs = 100U;
    runtimeConfig.resultPolicy.finalResultTimeoutEnabled = true;
    runtimeConfig.resultPolicy.finalResultTimeoutMs = 1000U;
    runtimeConfig.resultPolicy.doorEvidenceTtlEnabled = true;
    runtimeConfig.resultPolicy.doorEvidenceTtlMs = 500U;
    runtimeConfig.permissionPolicy.reevaluationPeriodMs = 200U;
    runtimeConfig.permissionPolicy.sourceValidityMs = 500U;
    runtimeConfig.stateQueryPolicy.bcmDoorTtlMs = 500U;
    runtimeConfig.stateQueryPolicy.bcmStatusTtlMs = 500U;
    runtimeConfig.stateQueryPolicy.cisRearTtlMs = 150U;
    runtimeConfig.stateQueryPolicy.cisStatusTtlMs = 500U;
    CHECK(DomainRuntime_Init(&g_runtime, &runtimeConfig) == DOMAIN_RUNTIME_OK);

    net_domain_c2_pinned_owner_api(&api);
    (void)memset(&adapterConfig, 0, sizeof(adapterConfig));
    adapterConfig.domain_boot = DOMAIN_BOOT;
    adapterConfig.first_can_sequence = 1U;
    adapterConfig.value_quality = net_domain_c2_value_quality_provisional;
    adapterConfig.local_failure_reason =
        net_domain_c2_local_failure_reason_provisional;
    adapterConfig.failure_sink_ready = true;
    adapterConfig.event_use_limit_ms = 500U;
    adapterConfig.publisher = &g_publisher;
    adapterConfig.transmit_policy.bound = true;
    adapterConfig.transmit_policy.maximum_age_ms = 500U;
    adapterConfig.transmit_policy.expires_at_equality = true;

    (void)memset(&networkConfig, 0, sizeof(networkConfig));
    networkConfig.node = NET_WIRE_NODE_DOMAIN;
    networkConfig.local_epoch = 1U;
    networkConfig.profile = NET_WIRE_CANDIDATE_ONLY;
    networkConfig.port.ops = &g_portOps;
    networkConfig.owner = net_domain_c2_owner(&g_adapter);
    for (index = 0U; index < 3U; ++index)
    {
        networkConfig.port_epoch[index] = 1U;
    }
    for (index = 0U; index < NET_UPPER_ROUTE_COUNT; ++index)
    {
        networkConfig.rx_policy[index].bound = true;
        networkConfig.rx_policy[index].maximum_age_ms = 2000U;
        networkConfig.rx_policy[index].expires_at_equality = true;
    }
    CHECK(net_upper_init(&g_network, &networkConfig) == NET_OK);

    (void)memset(&publishConfig, 0, sizeof(publishConfig));
    publishConfig.can_boot_id = DOMAIN_BOOT;
    publishConfig.entries = entries;
    publishConfig.entry_count = sizeof(entries) / sizeof(entries[0]);
    CHECK(net_upper_publish_init(&g_publisher, &g_network, &publishConfig) ==
          NET_OK);
    CHECK(net_domain_c2_init(&g_adapter, &g_network, NULL, &api,
                             &adapterConfig) == NET_OK);
    CHECK(net_domain_c2_register(&g_adapter) == NET_OK);
    return 0;
}

static int TrackDoor(DomainCommandId_t commandId, DoorLockTarget_t target)
{
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    CommandManager_Intent_t intent;
    ResultManager_t *manager = DomainRuntime_GetResultManager(&g_runtime);

    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = 11U;
    request.context.sessionId = 22U;
    request.context.requestId = commandId + 100U;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = target;
    request.meta.quality = DATA_QUALITY_OK;
    request.meta.updateSequence = commandId;
    key.deviceContextId = request.deviceContextId;
    key.context = request.context;

    (void)memset(&intent, 0, sizeof(intent));
    intent.commandId = commandId;
    intent.decisionSequence = commandId;
    intent.targetEcu = COMMAND_MANAGER_ECU_BCM;
    intent.kind = COMMAND_MANAGER_KIND_DOOR;
    intent.target.door.target = (target == DOOR_TARGET_LOCK)
                                    ? COMMAND_MANAGER_DOOR_TARGET_LOCK
                                    : COMMAND_MANAGER_DOOR_TARGET_UNLOCK;
    intent.request.deviceContextId = request.deviceContextId;
    intent.request.sessionId = request.context.sessionId;
    intent.request.requestId = request.context.requestId;

    CHECK(manager != NULL);
    CHECK(ResultManager_RegisterRequest(manager, &request, g_runtimeNowMs) ==
          RESULT_MANAGER_OK);
    CHECK(ResultManager_RecordDomainResult(
              manager, &key, REQUEST_RESULT_ACCEPTED, RESULT_REASON_NONE,
              false, g_runtimeNowMs) == RESULT_MANAGER_OK);
    CHECK(ResultManager_BindCommand(manager, &key, &intent) ==
          RESULT_MANAGER_OK);
    return 0;
}

static DomainIf_BcmCommandResult_t DoorResult(
    DomainCommandId_t commandId,
    DoorLockTarget_t target,
    DomainIf_BcmDoorEvidenceType_t evidence,
    RequestResult_t result,
    ResultReason_t reason,
    uint32_t sourceOccurrence,
    uint32_t ageMs)
{
    DomainIf_BcmCommandResult_t value;
    (void)memset(&value, 0, sizeof(value));
    value.commandId = commandId;
    value.kind = BCM_COMMAND_KIND_DOOR;
    value.result = result;
    value.reason = reason;
    value.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    value.hasDoorEvidence = true;
    value.doorEvidence.type = evidence;
    value.doorEvidence.occurrenceId = sourceOccurrence;
    value.doorEvidence.target = target;
    value.doorEvidence.quality = DATA_QUALITY_OK;
    value.doorEvidence.ageMs = ageMs;
    return value;
}

static int RunDomainPoll(void)
{
    NetStatus status;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    status = net_upper_step(&g_network, NET_UPPER_POLL_BUDGET);
    CHECK((status == NET_OK) || (status == NET_NO_WORK));
    return 0;
}

static int EventAt(size_t ordinal, NetWireVssEventRaw *out, size_t *outIndex)
{
    NetUpperCanMessage decoded;
    size_t index;
    size_t found = 0U;
    for (index = 0U; index < g_capturedCount; ++index)
    {
        if (g_captured[index].packet.link != NET_WIRE_CAN)
        {
            continue;
        }
        if (net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY,
                                 &g_captured[index].packet.can,
                                 &decoded) != NET_WIRE_OK)
        {
            continue;
        }
        if (decoded.id != 0x160U)
        {
            continue;
        }
        if (found == ordinal)
        {
            *out = decoded.data.vss_event;
            if (outIndex != NULL)
            {
                *outIndex = index;
            }
            return 0;
        }
        ++found;
    }
    return 1;
}

static size_t EventCount(void)
{
    NetWireVssEventRaw event;
    size_t count = 0U;
    while (EventAt(count, &event, NULL) == 0)
    {
        ++count;
    }
    return count;
}

static int DoorEvents(void)
{
    DomainIf_BcmCommandResult_t result;
    NetWireVssEventRaw wire;

    CHECK(Start(100U) == 0);
    CHECK(TrackDoor(10U, DOOR_TARGET_LOCK) == 0);
    result = DoorResult(10U, DOOR_TARGET_LOCK,
                        BCM_DOOR_EVIDENCE_LOCK_COMPLETED,
                        REQUEST_RESULT_DONE, RESULT_REASON_NONE, 50U, 4U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(0U, &wire, NULL) == 0);
    CHECK(wire.event_kind == 3U);
    CHECK(wire.occurrence_id == 1U);
    CHECK(wire.original_age_ms == 4U);
    CHECK(wire.use_limit_ms == 500U);
    CHECK(wire.related_command_id == 10U);

    CHECK(TrackDoor(11U, DOOR_TARGET_UNLOCK) == 0);
    result = DoorResult(11U, DOOR_TARGET_UNLOCK,
                        BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED,
                        REQUEST_RESULT_DONE, RESULT_REASON_NONE, 51U, 0U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(1U, &wire, NULL) == 0);
    CHECK(wire.event_kind == 4U && wire.occurrence_id == 2U);

    CHECK(TrackDoor(12U, DOOR_TARGET_LOCK) == 0);
    result = DoorResult(12U, DOOR_TARGET_LOCK,
                        BCM_DOOR_EVIDENCE_REQUEST_FAILED,
                        REQUEST_RESULT_FAILED, RESULT_REASON_NO_FEEDBACK,
                        52U, 0U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(2U, &wire, NULL) == 0);
    CHECK(wire.event_kind == 5U && wire.occurrence_id == 3U);

    /* A duplicate committed result is absorbed before event creation. */
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventCount() == 3U);
    return 0;
}

static int Exclusions(void)
{
    DomainIf_BcmCommandResult_t result;

    CHECK(Start(200U) == 0);
    /* Pre-policy rejection has no BCM execution evidence and creates no event. */
    CHECK(RunDomainPoll() == 0);
    CHECK(EventCount() == 0U);

    CHECK(TrackDoor(20U, DOOR_TARGET_UNLOCK) == 0);
    result = DoorResult(20U, DOOR_TARGET_UNLOCK,
                        BCM_DOOR_EVIDENCE_REQUEST_FAILED,
                        REQUEST_RESULT_FAILED, RESULT_REASON_NO_FEEDBACK,
                        60U, 0U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventCount() == 0U);

    CHECK(TrackDoor(21U, DOOR_TARGET_LOCK) == 0);
    result = DoorResult(21U, DOOR_TARGET_LOCK,
                        BCM_DOOR_EVIDENCE_REQUEST_REJECTED,
                        REQUEST_RESULT_REJECTED, RESULT_REASON_NOT_ALLOWED,
                        61U, 0U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventCount() == 0U);
    return 0;
}

static int RejectCaptured(size_t index)
{
    NetUpperEvent event;
    CHECK(index < g_capturedCount && g_captured[index].open);
    (void)memset(&event, 0, sizeof(event));
    event.kind = NET_UPPER_TX_EVENT;
    event.link = g_captured[index].packet.link;
    event.capture.port_epoch = 1U;
    event.capture.timestamp_known = true;
    event.capture.captured_at_ms = g_networkNowMs;
    event.tx.key = g_captured[index].key;
    event.tx.admission_observed = true;
    event.tx.accepted = false;
    event.tx.refusal = NET_TX_REFUSAL_DRIVER;
    event.tx.release_observed = true;
    CHECK(net_upper_capture(&g_network, &event) == NET_OK);
    CHECK(net_upper_step(&g_network, NET_UPPER_POLL_BUDGET) == NET_OK);
    (void)net_domain_c2_step(&g_adapter);
    g_captured[index].open = false;
    return 0;
}

static int RetryExpiry(void)
{
    DomainIf_BcmCommandResult_t result;
    NetWireVssEventRaw first;
    NetWireVssEventRaw retry;
    size_t firstIndex;
    size_t retryIndex;

    CHECK(Start(300U) == 0);
    CHECK(TrackDoor(30U, DOOR_TARGET_LOCK) == 0);
    result = DoorResult(30U, DOOR_TARGET_LOCK,
                        BCM_DOOR_EVIDENCE_LOCK_COMPLETED,
                        REQUEST_RESULT_DONE, RESULT_REASON_NONE, 70U, 10U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(0U, &first, &firstIndex) == 0);
    CHECK(RejectCaptured(firstIndex) == 0);

    g_runtimeNowMs += 20U;
    g_networkNowMs += 20U;
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(1U, &retry, &retryIndex) == 0);
    CHECK(retry.occurrence_id == first.occurrence_id);
    CHECK(retry.event_kind == first.event_kind);
    CHECK(retry.related_command_id == first.related_command_id);
    CHECK(retry.original_age_ms == first.original_age_ms + 20U);

    /* A later failure cannot revive the occurrence at its 500 ms limit. */
    CHECK(RejectCaptured(retryIndex) == 0);
    g_runtimeNowMs += 470U;
    g_networkNowMs += 470U;
    CHECK(RunDomainPoll() == 0);
    CHECK(EventCount() == 2U);
    return 0;
}

static int TimeRollover(void)
{
    DomainIf_BcmCommandResult_t result;
    NetWireVssEventRaw wire;
    size_t firstIndex;

    CHECK(Start(UINT32_MAX - 20U) == 0);
    CHECK(TrackDoor(40U, DOOR_TARGET_LOCK) == 0);
    result = DoorResult(40U, DOOR_TARGET_LOCK,
                        BCM_DOOR_EVIDENCE_LOCK_COMPLETED,
                        REQUEST_RESULT_DONE, RESULT_REASON_NONE, 80U, 5U);
    CHECK(DomainIf_RxBcmCommandResult(&result) == DOMAIN_IF_OK);
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(0U, &wire, &firstIndex) == 0);
    CHECK(RejectCaptured(firstIndex) == 0);
    g_runtimeNowMs = 9U;
    g_networkNowMs += 30U;
    CHECK(RunDomainPoll() == 0);
    CHECK(EventAt(1U, &wire, NULL) == 0);
    CHECK(wire.original_age_ms == 35U);
    return 0;
}

static int FeedWireUnconfirmed(void)
{
    NetUpperCanMessage message;
    NetUpperEvent event;

    (void)memset(&message, 0, sizeof(message));
    message.id = 0x140U;
    message.data.bcm_result.boot_id = BCM_BOOT;
    message.data.bcm_result.tx_sequence = 1U;
    message.data.bcm_result.command_domain_boot = DOMAIN_BOOT;
    message.data.bcm_result.command_id = 50U;
    message.data.bcm_result.command_kind = 1U;
    message.data.bcm_result.result = 2U;
    message.data.bcm_result.result_reason = 0U;
    message.data.bcm_result.door_event = 1U;
    message.data.bcm_result.occurrence_id = 90U;
    message.data.bcm_result.event_age_ms = 0U;
    message.data.bcm_result.event_quality = 0U;
    message.data.bcm_result.door_target = 0U;

    (void)memset(&event, 0, sizeof(event));
    event.kind = NET_UPPER_CAN_RX;
    event.link = NET_WIRE_CAN;
    event.capture.port_epoch = 1U;
    event.capture.timestamp_known = true;
    event.capture.captured_at_ms = g_networkNowMs;
    CHECK(net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY, &message, &event.can) ==
          NET_WIRE_OK);
    CHECK(net_upper_capture(&g_network, &event) == NET_OK);
    CHECK(net_upper_step(&g_network, NET_UPPER_POLL_BUDGET) == NET_OK);
    (void)net_domain_c2_step(&g_adapter);
    return 0;
}

static int WireUnconfirmed(void)
{
    CHECK(Start(400U) == 0);
    CHECK(TrackDoor(50U, DOOR_TARGET_LOCK) == 0);
    CHECK(FeedWireUnconfirmed() == 0);
    CHECK(RunDomainPoll() == 0);
    /* CAN 0x140 has no confirmation bit.  The current adapter correctly
     * hands it to DOMAIN as UNCONFIRMED, so it cannot create a VSS event. */
    CHECK(EventCount() == 0U);
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2)
    {
        return 2;
    }
    if (strcmp(argv[1], "door_events") == 0)
    {
        result = DoorEvents();
    }
    else if (strcmp(argv[1], "exclusions") == 0)
    {
        result = Exclusions();
    }
    else if (strcmp(argv[1], "retry_expiry") == 0)
    {
        result = RetryExpiry();
    }
    else if (strcmp(argv[1], "time_rollover") == 0)
    {
        result = TimeRollover();
    }
    else if (strcmp(argv[1], "wire_unconfirmed") == 0)
    {
        result = WireUnconfirmed();
    }
    else
    {
        return 2;
    }
    if (result == 0)
    {
        (void)printf("BCM result -> Domain -> VSS_EVENT 0x160 %s PASS\n",
                     argv[1]);
    }
    return result;
}
