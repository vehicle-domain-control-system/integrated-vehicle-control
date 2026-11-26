#include "network/net_domain_c2.h"
#include "network/net_upper_publish.h"

#include "domain_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

#define DOMAIN_BOOT 301U
#define CIS_BOOT 401U
#define WARNING_PERIOD_MS 50U
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

static NetStatus Submit(
    void *context,
    const NetUpperPacket *packet,
    NetTxKey key)
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

static const NetUpperPortOps g_portOps = {
    NetworkClock,
    Submit,
    Poll
};

static void SettleCaptured(void)
{
    size_t i;
    for (i = 0U; i < g_capturedCount; ++i)
    {
        NetUpperEvent event;
        if (!g_captured[i].open)
        {
            continue;
        }
        (void)memset(&event, 0, sizeof(event));
        event.kind = NET_UPPER_TX_EVENT;
        event.link = g_captured[i].packet.link;
        event.capture.port_epoch = 1U;
        event.capture.timestamp_known = true;
        event.capture.captured_at_ms = g_networkNowMs;
        event.tx.key = g_captured[i].key;
        event.tx.admission_observed = true;
        event.tx.accepted = true;
        event.tx.outcome = NET_TX_SUCCEEDED;
        event.tx.release_observed = true;
        (void)net_upper_capture(&g_network, &event);
        g_captured[i].open = false;
    }
}

static void NetworkPass(void)
{
    SettleCaptured();
    (void)net_upper_step(&g_network, NET_UPPER_POLL_BUDGET);
    (void)net_domain_c2_step(&g_adapter);
    (void)net_upper_publish_service(&g_publisher);
    (void)net_upper_step(&g_network, NET_UPPER_POLL_BUDGET);
}

static int Start(uint32_t runtimeNowMs)
{
    static const NetUpperPublishEntry entries[] = {
        { NET_WIRE_CAN, 0x0C0U, WARNING_PERIOD_MS,
          { true, UINT16_MAX + 1000U, true } }
    };
    DomainRuntime_Config_t runtimeConfig;
    NetDomainC2Api api;
    NetDomainC2Config adapterConfig;
    NetUpperConfig networkConfig;
    NetUpperPublishConfig publishConfig;
    size_t i;

    (void)memset(&g_network, 0, sizeof(g_network));
    (void)memset(&g_publisher, 0, sizeof(g_publisher));
    (void)memset(&g_adapter, 0, sizeof(g_adapter));
    (void)memset(&g_runtime, 0, sizeof(g_runtime));
    (void)memset(g_captured, 0, sizeof(g_captured));
    g_capturedCount = 0U;
    g_networkNowMs = 1000U;
    g_runtimeNowMs = runtimeNowMs;

    (void)memset(&runtimeConfig, 0, sizeof(runtimeConfig));
    runtimeConfig.timeProvider = RuntimeClock;
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
    CHECK(DomainRuntime_IsReady(&g_runtime));

    net_domain_c2_pinned_owner_api(&api);
    (void)memset(&adapterConfig, 0, sizeof(adapterConfig));
    adapterConfig.domain_boot = DOMAIN_BOOT;
    adapterConfig.first_can_sequence = 1U;
    adapterConfig.value_quality = net_domain_c2_value_quality_provisional;
    adapterConfig.publisher = &g_publisher;
    adapterConfig.transmit_policy.bound = true;
    adapterConfig.transmit_policy.maximum_age_ms = UINT16_MAX + 1000U;
    adapterConfig.transmit_policy.expires_at_equality = true;

    (void)memset(&networkConfig, 0, sizeof(networkConfig));
    networkConfig.node = NET_WIRE_NODE_DOMAIN;
    networkConfig.local_epoch = 1U;
    networkConfig.profile = NET_WIRE_CANDIDATE_ONLY;
    networkConfig.port.ops = &g_portOps;
    networkConfig.owner = net_domain_c2_owner(&g_adapter);
    for (i = 0U; i < 3U; ++i)
    {
        networkConfig.port_epoch[i] = 1U;
    }
    for (i = 0U; i < NET_UPPER_ROUTE_COUNT; ++i)
    {
        networkConfig.rx_policy[i].bound = true;
        networkConfig.rx_policy[i].maximum_age_ms = 2000U;
        networkConfig.rx_policy[i].expires_at_equality = true;
    }
    CHECK(net_upper_init(&g_network, &networkConfig) == NET_OK);

    (void)memset(&publishConfig, 0, sizeof(publishConfig));
    publishConfig.can_boot_id = DOMAIN_BOOT;
    publishConfig.entries = entries;
    publishConfig.entry_count = sizeof(entries) / sizeof(entries[0]);
    CHECK(net_upper_publish_init(
              &g_publisher, &g_network, &publishConfig) == NET_OK);
    CHECK(net_domain_c2_init(
              &g_adapter, &g_network, NULL, &api, &adapterConfig) == NET_OK);
    CHECK(net_domain_c2_register(&g_adapter) == NET_OK);
    return 0;
}

static int FeedRear(
    uint32_t txSequence,
    uint32_t updateSequence,
    uint16_t distanceMm,
    uint16_t sourceAgeMs,
    uint8_t validity,
    uint8_t reason,
    uint8_t measurementState)
{
    NetUpperCanMessage message;
    NetUpperEvent event;

    (void)memset(&message, 0, sizeof(message));
    message.id = 0x0A0U;
    message.data.cis_rear.boot_id = CIS_BOOT;
    message.data.cis_rear.tx_sequence = txSequence;
    message.data.cis_rear.update_sequence = updateSequence;
    message.data.cis_rear.rear_distance_deci_cm = distanceMm;
    message.data.cis_rear.source_age_ms = sourceAgeMs;
    message.data.cis_rear.validity = validity;
    message.data.cis_rear.quality_reason = reason;
    message.data.cis_rear.proximity_status = measurementState;

    (void)memset(&event, 0, sizeof(event));
    event.kind = NET_UPPER_CAN_RX;
    event.link = NET_WIRE_CAN;
    event.capture.port_epoch = 1U;
    event.capture.timestamp_known = true;
    event.capture.captured_at_ms = g_networkNowMs;
    CHECK(net_upper_can_encode(
              NET_WIRE_CANDIDATE_ONLY, &message, &event.can) == NET_WIRE_OK);
    CHECK(net_upper_capture(&g_network, &event) == NET_OK);
    CHECK(net_upper_step(&g_network, NET_UPPER_POLL_BUDGET) == NET_OK);
    (void)net_domain_c2_step(&g_adapter);
    return 0;
}

static int PollAndSend(void)
{
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    NetworkPass();
    CHECK(g_runtime.lastVssWarningStatus == VSS_WARNING_MANAGER_OK);
    return 0;
}

static int WarningAt(size_t index, NetWireVssWarningStateRaw *out)
{
    NetUpperCanMessage decoded;
    CHECK(index < g_capturedCount);
    CHECK(g_captured[index].packet.link == NET_WIRE_CAN);
    CHECK(net_upper_can_decode(
              NET_WIRE_CANDIDATE_ONLY,
              &g_captured[index].packet.can,
              &decoded) == NET_WIRE_OK);
    CHECK(decoded.id == 0x0C0U);
    *out = decoded.data.vss_warning_state;
    return 0;
}

static int RangesAndRepeat(void)
{
    static const struct
    {
        uint16_t distanceMm;
        uint8_t measurementState;
        uint8_t expectedState;
    } samples[] = {
        {100U, 0U, 2U}, {400U, 0U, 2U},
        {401U, 0U, 1U}, {700U, 0U, 1U},
        {701U, 0U, 0U}, {1000U, 0U, 0U},
        {UINT16_MAX, 1U, 0U}
    };
    NetWireVssWarningStateRaw warning;
    size_t i;
    size_t before;

    CHECK(Start(1000U) == 0);
    for (i = 0U; i < sizeof(samples) / sizeof(samples[0]); ++i)
    {
        if (i != 0U)
        {
            g_networkNowMs += 80U;
            g_runtimeNowMs += 80U;
        }
        CHECK(FeedRear(
                  (uint32_t)i + 1U,
                  (uint32_t)i + 11U,
                  samples[i].distanceMm,
                  10U,
                  1U,
                  0U,
                  samples[i].measurementState) == 0);
        CHECK(PollAndSend() == 0);
        CHECK(WarningAt(i, &warning) == 0);
        CHECK(warning.rear_obstacle_state == samples[i].expectedState);
        CHECK(warning.rear_obstacle_quality == 0U);
        CHECK(warning.rear_obstacle_source_boot == CIS_BOOT);
        CHECK(warning.rear_obstacle_source_sequence == (uint32_t)i + 11U);
        CHECK(warning.rear_obstacle_original_age_ms == 10U);
    }

    before = g_capturedCount;
    g_networkNowMs += WARNING_PERIOD_MS - 1U;
    g_runtimeNowMs += WARNING_PERIOD_MS - 1U;
    NetworkPass();
    CHECK(g_capturedCount == before);
    ++g_networkNowMs;
    ++g_runtimeNowMs;
    NetworkPass();
    CHECK(g_capturedCount == before + 1U);
    CHECK(WarningAt(before, &warning) == 0);
    CHECK(warning.rear_obstacle_source_sequence == 17U);
    CHECK(warning.rear_obstacle_original_age_ms == 60U);
    return 0;
}

static int QualityAndFault(void)
{
    NetWireVssWarningStateRaw first;
    NetWireVssWarningStateRaw changed;

    CHECK(Start(100U) == 0);
    CHECK(FeedRear(1U, 20U, 300U, 0U, 1U, 0U, 0U) == 0);
    CHECK(PollAndSend() == 0);
    CHECK(WarningAt(0U, &first) == 0);
    CHECK(first.rear_obstacle_state == 2U);
    CHECK(first.rear_obstacle_quality == 0U);
    CHECK(first.occupant_hazard_source_boot == 0U);
    CHECK(first.occupant_hazard_source_sequence == 0U);
    CHECK(first.occupant_hazard_original_age_ms == UINT16_MAX);
    CHECK(first.occupant_hazard_quality == 3U);
    CHECK(first.occupant_hazard_reason == 6U);
    CHECK(first.occupant_hazard_availability == 2U);

    g_networkNowMs += 80U;
    g_runtimeNowMs += 80U;
    CHECK(FeedRear(2U, 20U, 300U, 80U, 0U, 3U, 0U) == 0);
    CHECK(PollAndSend() == 0);
    CHECK(WarningAt(1U, &changed) == 0);
    CHECK(changed.rear_obstacle_quality != 0U);
    CHECK(changed.rear_obstacle_availability != 0U);
    CHECK(changed.antipinch_quality == first.antipinch_quality);
    CHECK(changed.occupant_hazard_quality == first.occupant_hazard_quality);

    g_networkNowMs += 80U;
    g_runtimeNowMs += 80U;
    CHECK(FeedRear(3U, 21U, UINT16_MAX, 0U, 0U, 3U, 3U) == 0);
    CHECK(PollAndSend() == 0);
    CHECK(WarningAt(2U, &changed) == 0);
    CHECK(changed.rear_obstacle_quality != 0U);
    CHECK(changed.rear_obstacle_availability != 0U);
    return 0;
}

static int RearTtl(void)
{
    NetWireVssWarningStateRaw stale;
    size_t before;

    CHECK(Start(500U) == 0);
    CHECK(FeedRear(1U, 30U, 500U, 0U, 1U, 0U, 0U) == 0);
    CHECK(PollAndSend() == 0);
    before = g_capturedCount;
    g_networkNowMs += 149U;
    g_runtimeNowMs += 149U;
    CHECK(DomainRuntime_Poll(&g_runtime) == DOMAIN_RUNTIME_OK);
    NetworkPass();
    CHECK(g_capturedCount == before + 1U);
    CHECK(WarningAt(before, &stale) == 0);
    CHECK(stale.rear_obstacle_quality == (uint8_t)DATA_QUALITY_OK);
    CHECK(stale.rear_obstacle_original_age_ms == 149U);
    ++g_networkNowMs;
    ++g_runtimeNowMs;
    CHECK(PollAndSend() == 0);
    CHECK(g_capturedCount == before + 2U);
    CHECK(WarningAt(before + 1U, &stale) == 0);
    CHECK(stale.rear_obstacle_quality == (uint8_t)DATA_QUALITY_STALE);
    CHECK(stale.rear_obstacle_original_age_ms == 150U);
    return 0;
}

static int TimeRollover(void)
{
    NetWireVssWarningStateRaw warning;
    size_t before;

    CHECK(Start(UINT32_MAX - 50U) == 0);
    CHECK(FeedRear(1U, 40U, 500U, 5U, 1U, 0U, 0U) == 0);
    CHECK(PollAndSend() == 0);
    before = g_capturedCount;

    g_networkNowMs += 145U;
    g_runtimeNowMs = 94U;
    CHECK(PollAndSend() == 0);
    CHECK(g_capturedCount == before + 1U);
    CHECK(WarningAt(before, &warning) == 0);
    CHECK(warning.rear_obstacle_quality == (uint8_t)DATA_QUALITY_STALE);
    CHECK(warning.rear_obstacle_original_age_ms == 150U);
    CHECK(warning.rear_obstacle_source_boot == CIS_BOOT);
    CHECK(warning.rear_obstacle_source_sequence == 40U);
    return 0;
}

static DomainIf_VssWarningState_t SaturationWarning(
    uint32_t sequence,
    uint32_t ageMs,
    DataQuality_t quality,
    QualityReason_t reason)
{
    DomainIf_VssWarningState_t warning;

    (void)memset(&warning, 0, sizeof(warning));
    warning.antiPinchEvidence.availability =
        FUNCTION_AVAILABILITY_UNSUPPORTED;
    warning.occupantHazardState = OCCUPANT_HAZARD_UNCONFIRMED;
    warning.occupantEvidence.quality = DATA_QUALITY_NO_DATA;
    warning.occupantEvidence.reason = QUALITY_REASON_NO_DATA;
    warning.occupantEvidence.availability =
        FUNCTION_AVAILABILITY_UNAVAILABLE;
    warning.occupantEvidence.source.ageMs = UINT32_MAX;
    warning.rearObstacleState = REAR_OBSTACLE_STATE_EMERGENCY;
    warning.rearEvidence.quality = DATA_QUALITY_OK;
    warning.rearEvidence.reason = QUALITY_REASON_NONE;
    warning.rearEvidence.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    warning.rearEvidence.source.generation = CIS_BOOT;
    warning.rearEvidence.source.sequence = sequence;
    warning.rearEvidence.source.ageMs = ageMs;
    warning.rearEvidence.quality = quality;
    warning.rearEvidence.reason = reason;
    warning.rearDetectionActivation = REAR_DETECTION_ACTIVE;
    warning.rearActivationMeta.quality = DATA_QUALITY_OK;
    warning.rearActivationMeta.updateSequence = sequence;
    return warning;
}

static int AgeSaturation(void)
{
    static const struct
    {
        uint32_t ageMs;
        DataQuality_t quality;
        QualityReason_t reason;
        uint16_t expectedAge;
    } cases[] = {
        {UINT16_MAX - 1U, DATA_QUALITY_OK, QUALITY_REASON_NONE,
         UINT16_MAX - 1U},
        {UINT16_MAX, DATA_QUALITY_OK, QUALITY_REASON_NONE, UINT16_MAX},
        {UINT16_MAX + 1U, DATA_QUALITY_OK, QUALITY_REASON_NONE, UINT16_MAX},
        {UINT32_MAX, DATA_QUALITY_OK, QUALITY_REASON_NONE, UINT16_MAX},
        {UINT32_MAX, DATA_QUALITY_STALE, QUALITY_REASON_STALE, UINT16_MAX},
        {UINT32_MAX, DATA_QUALITY_INVALID, QUALITY_REASON_DATA_INVALID,
         UINT16_MAX},
        {UINT32_MAX, DATA_QUALITY_NO_DATA, QUALITY_REASON_NO_DATA, UINT16_MAX}
    };
    DomainIf_VssWarningState_t warning;
    NetWireVssWarningStateRaw wire;
    size_t index;

    CHECK(Start(700U) == 0);
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        warning = SaturationWarning((uint32_t)index + 1U, cases[index].ageMs,
                                    cases[index].quality,
                                    cases[index].reason);
        CHECK(DomainIf_TxVssWarningState(&warning) == DOMAIN_IF_OK);
        NetworkPass();
        CHECK(WarningAt(index, &wire) == 0);
        CHECK(wire.rear_obstacle_original_age_ms == cases[index].expectedAge);
        CHECK(wire.rear_obstacle_source_boot == CIS_BOOT);
        CHECK(wire.rear_obstacle_source_sequence == (uint32_t)index + 1U);
        CHECK(wire.rear_obstacle_quality == (uint8_t)cases[index].quality);
        CHECK(wire.rear_obstacle_reason == (uint8_t)cases[index].reason);
        g_networkNowMs += WARNING_PERIOD_MS;
        g_runtimeNowMs += WARNING_PERIOD_MS;
    }

    /* An old STALE occupant lane must not block a fresh rear lane. */
    warning = SaturationWarning(20U, 10U, DATA_QUALITY_OK,
                                QUALITY_REASON_NONE);
    warning.occupantEvidence.source.generation = 900U;
    warning.occupantEvidence.source.sequence = 21U;
    warning.occupantEvidence.source.ageMs = UINT16_MAX + 1U;
    warning.occupantEvidence.quality = DATA_QUALITY_STALE;
    warning.occupantEvidence.reason = QUALITY_REASON_STALE;
    warning.occupantEvidence.availability = FUNCTION_AVAILABILITY_DEGRADED;
    CHECK(DomainIf_TxVssWarningState(&warning) == DOMAIN_IF_OK);
    NetworkPass();
    CHECK(WarningAt(index, &wire) == 0);
    CHECK(wire.rear_obstacle_original_age_ms == 10U);
    CHECK(wire.rear_obstacle_quality == 0U);
    CHECK(wire.occupant_hazard_original_age_ms == UINT16_MAX);
    CHECK(wire.occupant_hazard_quality == 1U);
    CHECK(wire.occupant_hazard_reason == 5U);
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2)
    {
        return 2;
    }
    if (strcmp(argv[1], "ranges_repeat") == 0)
    {
        result = RangesAndRepeat();
    }
    else if (strcmp(argv[1], "quality_fault") == 0)
    {
        result = QualityAndFault();
    }
    else if (strcmp(argv[1], "ttl") == 0)
    {
        result = RearTtl();
    }
    else if (strcmp(argv[1], "time_rollover") == 0)
    {
        result = TimeRollover();
    }
    else if (strcmp(argv[1], "age_saturation") == 0)
    {
        result = AgeSaturation();
    }
    else
    {
        return 2;
    }
    if (result == 0)
    {
        (void)printf("CIS_REAR -> Domain Runtime -> VSS_WARNING_STATE %s PASS\n",
                     argv[1]);
    }
    return result;
}
