/* Network message conversion: every row of the Event, Warning and Status
 * mapping, the anti-pinch fixed unsupported pattern, activation metadata
 * format 0 / 1, saturated age 65535, per-message TX order and DOMAIN restart
 * through the adapter, the Network stand-in and the millisecond clock. */

#include <string.h>

#include "test_util.h"

#include "../../src/service/input/vss_input_net.h"
#include "../../src/net_if/net_stub.h"
#include "../../src/infra/vss_time.h"

static uint32_t s_ticks;

static uint32_t ReadTicks(void)
{
    return s_ticks;
}

/* ms must be a multiple of 5 (4 ticks = 5 ms). */
static void SetNowMs(uint64_t ms)
{
    s_ticks = (uint32_t)((ms * 4u) / 5u);
}

static void Reset(uint64_t nowMs)
{
    Input_TestReset();
    Store_TestReset();
    VssInputNet_ResetForTest();
    s_ticks = 0u;
    VssTime_ResetForTest();
    (void)VssTime_Init(ReadTicks);
    SetNowMs(nowMs);
}

static NetCanRxMeta Meta(uint32_t boot, uint32_t tx, uint32_t rxMs)
{
    NetCanRxMeta m;
    m.source.boot_id = boot;
    m.source.tx_sequence = tx;
    m.received_at_ms = rxMs;
    return m;
}

static NetVssEvent Event(NetVssEventKind kind, uint32_t occurrence)
{
    NetVssEvent e;
    memset(&e, 0, sizeof e);
    e.event_kind = kind;
    e.occurrence_id = occurrence;
    e.original_age_ms = 0u;
    e.use_limit_ms = 500u;
    e.value_quality = NET_QUALITY_OK;
    e.input_availability = NET_AVAILABILITY_AVAILABLE;
    e.related_command_id = 0u;
    return e;
}

/* All lanes valid and CLEAR / activation ENABLED, format 1. */
static NetVssWarningState Warning(uint32_t sequence)
{
    NetVssWarningState w;
    memset(&w, 0, sizeof w);
    w.antipinch_source_boot = 21u;
    w.antipinch_source_sequence = sequence;
    w.occupant_hazard_source_boot = 22u;
    w.occupant_hazard_source_sequence = sequence;
    w.rear_obstacle_source_boot = 23u;
    w.rear_obstacle_source_sequence = sequence;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_sequence = sequence;
    return w;
}

static VssFactTime Rx(VssOrigin origin, uint64_t ms)
{
    VssFactTime t;
    t.domain = origin;
    t.epoch = 1u;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

static bool SameTime(const VssFactTime *a, const VssFactTime *b)
{
    return (a->domain == b->domain) && (a->epoch == b->epoch) && (a->earliest == b->earliest) &&
           (a->latest == b->latest) && (a->continuity == b->continuity);
}

static VssOccurrenceKey Key(VssOrigin origin, uint32_t boot, uint32_t occurrence)
{
    VssOccurrenceKey k;
    k.origin = origin;
    k.source = VSS_SOURCE_DOMAIN;
    k.generation = boot;
    k.occurrenceId = occurrence;
    return k;
}

static VssInputNetObservation Obs(VssOrigin origin)
{
    VssInputNetObservation o;
    memset(&o, 0, sizeof o);
    CHECK(VssInputNet_Read(origin, &o));
    return o;
}

/* ---- Event rows ---------------------------------------------------------- */

static void test_event_kind_row(void)
{
    static const VssInputSignal expected[5] = {
        VSS_SIGNAL_WELCOME, VSS_SIGNAL_GOODBYE, VSS_SIGNAL_LOCK_COMPLETE,
        VSS_SIGNAL_UNLOCK_COMPLETE, VSS_SIGNAL_LOCK_ERROR
    };
    static const NetVssEventKind rejected[3] = { 0u, 6u, 255u };
    NetCanRxMeta meta = Meta(1u, 1u, 1000u);
    VssFactTime rx = Rx(VSS_TEST, 1000u);
    VssOccurrenceKey key;
    VssInputEvidence e;
    NetVssEvent m;
    unsigned i;

    for (i = 0u; i < 5u; ++i)
    {
        m = Event((NetVssEventKind)(NET_VSS_EVENT_WELCOME + i), 1u);
        CHECK(VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
        CHECK_EQ_U(e.signal, expected[i]);
        CHECK_EQ_U(e.value, VSS_VALUE_EVENT);
    }
    for (i = 0u; i < 3u; ++i)
    {
        m = Event(rejected[i], 1u);
        CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
    }
    m = Event(NET_VSS_EVENT_WELCOME, 1u);
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, NULL, &meta, &rx, &key, &e));
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, NULL, &rx, &key, &e));
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, NULL, &key, &e));
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, NULL, &e));
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, NULL));
    CHECK(!VssInputNet_ConvertEvent((VssOrigin)7, &m, &meta, &rx, &key, &e));

    /* A discarded kind never reaches INPUT and does not consume its order. */
    Reset(2000u);
    m = Event(6u, 1u);
    meta = Meta(1u, 9u, 2000u);
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).eventsDiscarded, 1u);
    CHECK_EQ_U(Obs(VSS_TEST).eventsForwarded, 0u);
    CHECK(!Obs(VSS_TEST).generationOpen);
    m = Event(NET_VSS_EVENT_WELCOME, 1u);
    meta = Meta(1u, 1u, 2000u);
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    VssInputNet_OnEvent(VSS_TEST, NULL, &meta);
    VssInputNet_OnEvent(VSS_TEST, &m, NULL);
    CHECK_EQ_U(Obs(VSS_TEST).eventsDiscarded, 3u);
}

static void test_event_field_rows(void)
{
    static const VssInputQuality quality[4] = {
        VSS_INPUT_VALID, VSS_INPUT_STALE, VSS_INPUT_INVALID, VSS_INPUT_UNASSESSED
    };
    static const VssAvailability availability[3] = { VSS_AVAILABLE, VSS_LIMITED, VSS_UNAVAILABLE };
    NetCanRxMeta meta = Meta(3u, 12u, 1995u);
    VssFactTime rx = Rx(VSS_PRODUCT, 1995u);
    VssOccurrenceKey key;
    VssInputEvidence e;
    NetVssEvent m = Event(NET_VSS_EVENT_LOCK_COMPLETE, 77u);
    unsigned i;

    m.original_age_ms = 100u;
    m.use_limit_ms = 400u;
    m.input_availability = NET_AVAILABILITY_LIMITED;
    m.related_command_id = 9u;
    CHECK(VssInputNet_ConvertEvent(VSS_PRODUCT, &m, &meta, &rx, &key, &e));
    CHECK_EQ_U(key.origin, VSS_PRODUCT);
    CHECK_EQ_U(key.source, VSS_SOURCE_DOMAIN);
    CHECK_EQ_U(key.generation, 3u);
    CHECK_EQ_U(key.occurrenceId, 77u);
    CHECK(e.occurrence == &key);
    CHECK_EQ_U(e.deliveryOrder, 12u);
    CHECK(SameTime(&e.receivedAt, &rx));
    CHECK_EQ_U(e.ageLowMs, 100u);
    CHECK_EQ_U(e.ageHighMs, 100u + VSS_CFG_RX_GAP_BUDGET_MS);
    CHECK(e.ageComplete);
    CHECK_EQ_U(e.useLimitMs, 400u);
    CHECK_EQ_U(e.relatedCommand, 9u);
    CHECK_EQ_U(e.availability, VSS_LIMITED);
    CHECK_EQ_U(e.lossAt.epoch, 0u); /* normal quality: no loss time */

    m.original_age_ms = 65535u;
    CHECK(VssInputNet_ConvertEvent(VSS_PRODUCT, &m, &meta, &rx, &key, &e));
    CHECK(!e.ageComplete);
    CHECK_EQ_U(e.ageLowMs, 65535u);

    m = Event(NET_VSS_EVENT_WELCOME, 1u);
    for (i = 0u; i < 4u; ++i)
    {
        m.value_quality = (NetValueQuality)i;
        CHECK(VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
        CHECK_EQ_U(e.quality, quality[i]);
        /* abnormal quality: loss at the receive time */
        CHECK(SameTime(&e.lossAt, &rx) == (i != 0u));
    }
    m.value_quality = 4u;
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
    m.value_quality = NET_QUALITY_OK;
    for (i = 0u; i < 3u; ++i)
    {
        m.input_availability = (NetAvailability)i;
        CHECK(VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
        CHECK_EQ_U(e.availability, availability[i]);
        CHECK(SameTime(&e.lossAt, &rx) == (i == 2u));
    }
    m.input_availability = 3u;
    CHECK(!VssInputNet_ConvertEvent(VSS_TEST, &m, &meta, &rx, &key, &e));
    CHECK_EQ_U(key.origin, VSS_TEST);
}

/* The same rows observed after INPUT and STORE accepted the event. */
static void test_event_rows_reach_store(void)
{
    NetVssEvent m = Event(NET_VSS_EVENT_LOCK_COMPLETE, 77u);
    NetCanRxMeta meta = Meta(3u, 12u, 1995u);
    VssOccurrenceKey key = Key(VSS_TEST, 3u, 77u);
    VssOccurrenceObservation r;
    VssInputContextObservation c;

    Reset(2000u);
    m.original_age_ms = 100u;
    m.use_limit_ms = 400u;
    m.input_availability = NET_AVAILABILITY_LIMITED;
    m.related_command_id = 9u;
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK(Obs(VSS_TEST).generationOpen);
    CHECK_EQ_U(Obs(VSS_TEST).acceptedBoot, 3u);
    memset(&r, 0, sizeof r);
    CHECK(Store_ReadOccurrence(&key, &r));
    CHECK_EQ_U(r.event.signal, VSS_SIGNAL_LOCK_COMPLETE);
    CHECK_EQ_U(r.event.meta.deliveryOrder, 12u);
    CHECK_EQ_U(r.event.meta.original.domain, VSS_TEST);
    CHECK_EQ_U(r.event.meta.original.earliest, 1995u - 100u - VSS_CFG_RX_GAP_BUDGET_MS);
    CHECK_EQ_U(r.event.meta.original.latest, 1995u - 100u);
    CHECK_EQ_U(r.event.meta.useLimit, 400u);
    CHECK_EQ_U(r.event.meta.validUntil, 1995u - 100u - VSS_CFG_RX_GAP_BUDGET_MS + 400u);
    CHECK_EQ_U(r.event.meta.relatedCommand, 9u);
    CHECK_EQ_U(r.event.meta.availability, VSS_LIMITED);
    CHECK(r.replayAllowed);

    /* STALE quality on the same signal is a quality change. */
    m = Event(NET_VSS_EVENT_LOCK_COMPLETE, 78u);
    m.value_quality = NET_QUALITY_STALE;
    meta = Meta(3u, 13u, 2000u);
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_QUALITY_CHANGE);
    CHECK(Input_ReadContext(VSS_TEST, VSS_SIGNAL_LOCK_COMPLETE, &c));
    CHECK_EQ_U(c.quality, VSS_INPUT_STALE);
}

/* received_at_ms is on the VssTime clock: extended across the 32-bit wrap,
 * and unusable when it lies in the future. */
static void test_receive_time_row(void)
{
    NetVssEvent m = Event(NET_VSS_EVENT_WELCOME, 1u);
    NetCanRxMeta meta;
    VssOccurrenceKey key = Key(VSS_TEST, 1u, 1u);
    VssOccurrenceObservation r;

    Reset(2000u);
    meta = Meta(1u, 1u, 1990u);
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK(Store_ReadOccurrence(&key, &r));
    CHECK_EQ_U(r.event.meta.original.latest, 1990u);
    CHECK_EQ_U(r.event.meta.original.epoch, 1u);

    m.occurrence_id = 2u;
    meta = Meta(1u, 2u, 2010u); /* received after now */
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_REJECTED);
    key.occurrenceId = 2u;
    CHECK(!Store_ReadOccurrence(&key, &r));

    /* now = 4 294 967 500 ms (204 ms past 2^32), received 400 ms earlier,
     * before the wrap */
    Input_TestReset();
    Store_TestReset();
    VssInputNet_ResetForTest();
    s_ticks = 3435974000u;
    VssTime_ResetForTest();
    CHECK(VssTime_Init(ReadTicks));
    CHECK_EQ_U(VssTime_NowMs(), 4294967500ull);
    CHECK_EQ_U(VssTime_NowMs32(), 204u);
    m.occurrence_id = 3u;
    meta = Meta(1u, 1u, 4294967100u);
    VssInputNet_OnEvent(VSS_TEST, &m, &meta);
    key.occurrenceId = 3u;
    CHECK(Store_ReadOccurrence(&key, &r));
    CHECK_EQ_U(r.event.meta.original.latest, 4294967100ull);
}

/* ---- Warning rows -------------------------------------------------------- */

static void test_warning_frame_and_value_rows(void)
{
    NetVssWarningState m = Warning(5u);
    NetCanRxMeta meta = Meta(4u, 33u, 1500u);
    VssFactTime rx = Rx(VSS_TEST, 1500u);
    VssWarningFrame f;

    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK_EQ_U(f.origin, VSS_TEST);
    CHECK_EQ_U(f.source, VSS_SOURCE_DOMAIN);
    CHECK_EQ_U(f.domainBoot, 4u);
    CHECK_EQ_U(f.deliveryOrder, 33u);
    CHECK(SameTime(&f.receivedAt, &rx));
    CHECK(f.structureVerified);

    /* lane order: 0 activation, 1 rear risk, 2 occupant, 3 anti-pinch */
    CHECK_EQ_U(f.lanes[0].value, VSS_VALUE_ENABLED);
    CHECK_EQ_U(f.lanes[1].value, VSS_VALUE_CLEAR);
    CHECK_EQ_U(f.lanes[2].value, VSS_VALUE_CLEAR);
    CHECK_EQ_U(f.lanes[3].value, VSS_VALUE_CLEAR);
    m.rear_detection_activation = NET_FLAG_NO;
    m.rear_obstacle_state = NET_REAR_WARNING_CAUTION;
    m.occupant_hazard_state = NET_FLAG_YES;
    m.antipinch_state = NET_FLAG_YES;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK_EQ_U(f.lanes[0].value, VSS_VALUE_DISABLED);
    CHECK_EQ_U(f.lanes[1].value, VSS_VALUE_CAUTION);
    CHECK_EQ_U(f.lanes[2].value, VSS_VALUE_ACTIVE);
    CHECK_EQ_U(f.lanes[3].value, VSS_VALUE_ACTIVE);
    m.rear_obstacle_state = NET_REAR_WARNING_EMERGENCY;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK_EQ_U(f.lanes[1].value, VSS_VALUE_EMERGENCY);

    CHECK(!VssInputNet_ConvertWarning(VSS_TEST, NULL, &meta, &rx, &f));
    CHECK(!VssInputNet_ConvertWarning(VSS_TEST, &m, NULL, &rx, &f));
    CHECK(!VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, NULL, &f));
    CHECK(!VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, NULL));
}

static void test_warning_quality_reason_meaning_rows(void)
{
    static const VssInputQuality quality[4] = {
        VSS_INPUT_VALID, VSS_INPUT_STALE, VSS_INPUT_INVALID, VSS_INPUT_UNASSESSED
    };
    static const VssWarningReason reason[10] = {
        VSS_REASON_NONE,         /* NONE */
        VSS_REASON_DATA_INVALID, /* NOT_READY */
        VSS_REASON_DATA_INVALID, /* OUT_OF_RANGE */
        VSS_REASON_DATA_INVALID, /* SENSOR_FAULT */
        VSS_REASON_DATA_INVALID, /* VISION_FAULT */
        VSS_REASON_STALE,        /* STALE */
        VSS_REASON_NO_DATA,      /* NO_DATA */
        VSS_REASON_DATA_INVALID, /* COMMUNICATION_FAULT */
        VSS_REASON_DATA_INVALID, /* RECOVERING */
        VSS_REASON_DATA_INVALID  /* DATA_INVALID */
    };
    NetVssWarningState m;
    NetCanRxMeta meta = Meta(4u, 1u, 1500u);
    VssFactTime rx = Rx(VSS_TEST, 1500u);
    VssWarningFrame f;
    unsigned i;

    for (i = 0u; i < 4u; ++i)
    {
        m = Warning(1u);
        m.rear_activation_quality = (NetValueQuality)i;
        m.rear_obstacle_quality = (NetValueQuality)i;
        m.occupant_hazard_quality = (NetValueQuality)i;
        m.antipinch_quality = (NetValueQuality)i;
        CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
        CHECK_EQ_U(f.lanes[0].quality, quality[i]);
        CHECK_EQ_U(f.lanes[1].quality, quality[i]);
        CHECK_EQ_U(f.lanes[2].quality, quality[i]);
        CHECK_EQ_U(f.lanes[3].quality, quality[i]);
        /* meaning: CONFIRMED only for OK with reason NONE */
        CHECK_EQ_U(f.lanes[1].meaning, (i == 0u) ? VSS_MEANING_CONFIRMED : VSS_MEANING_UNCONFIRMED);
        CHECK_EQ_U(f.lanes[0].meaning, (i == 0u) ? VSS_MEANING_CONFIRMED : VSS_MEANING_UNCONFIRMED);
    }
    for (i = 0u; i < 10u; ++i)
    {
        m = Warning(1u);
        m.rear_activation_reason = (NetQualityReason)i;
        m.rear_obstacle_reason = (NetQualityReason)i;
        m.occupant_hazard_reason = (NetQualityReason)i;
        m.antipinch_reason = (NetQualityReason)i;
        CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
        CHECK_EQ_U(f.lanes[0].reason, reason[i]);
        CHECK_EQ_U(f.lanes[1].reason, reason[i]);
        CHECK_EQ_U(f.lanes[2].reason, reason[i]);
        CHECK_EQ_U(f.lanes[3].reason, reason[i]);
        CHECK_EQ_U(f.lanes[2].meaning, (i == 0u) ? VSS_MEANING_CONFIRMED : VSS_MEANING_UNCONFIRMED);
    }
}

static void test_warning_source_and_time_rows(void)
{
    static const VssAvailability availability[3] = { VSS_AVAILABLE, VSS_LIMITED, VSS_UNAVAILABLE };
    NetVssWarningState m = Warning(5u);
    NetCanRxMeta meta = Meta(4u, 1u, 1500u);
    VssFactTime rx = Rx(VSS_TEST, 1500u);
    VssWarningFrame f;
    unsigned i;

    m.rear_activation_sequence = 41u;
    m.rear_obstacle_source_sequence = 42u;
    m.occupant_hazard_source_sequence = 43u;
    m.antipinch_source_sequence = 44u;
    m.rear_activation_age_ms = 10u;
    m.rear_obstacle_original_age_ms = 20u;
    m.occupant_hazard_original_age_ms = 30u;
    m.antipinch_original_age_ms = 40u;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    /* evidence source per lane */
    CHECK_EQ_U(f.lanes[0].evidenceSource, VSS_EVIDENCE_DOMAIN_ACTIVATION);
    CHECK_EQ_U(f.lanes[1].evidenceSource, VSS_EVIDENCE_CIS_REAR);
    CHECK_EQ_U(f.lanes[2].evidenceSource, VSS_EVIDENCE_PI_VIA_CIS);
    CHECK_EQ_U(f.lanes[3].evidenceSource, VSS_EVIDENCE_WINDOW);
    /* evidence boot / sequence: activation uses the frame boot */
    CHECK_EQ_U(f.lanes[0].evidenceBoot, 4u);
    CHECK_EQ_U(f.lanes[0].evidenceSequence, 41u);
    CHECK_EQ_U(f.lanes[1].evidenceBoot, 23u);
    CHECK_EQ_U(f.lanes[1].evidenceSequence, 42u);
    CHECK_EQ_U(f.lanes[2].evidenceBoot, 22u);
    CHECK_EQ_U(f.lanes[2].evidenceSequence, 43u);
    CHECK_EQ_U(f.lanes[3].evidenceBoot, 21u);
    CHECK_EQ_U(f.lanes[3].evidenceSequence, 44u);
    /* age interval */
    CHECK_EQ_U(f.lanes[0].ageLowMs, 10u);
    CHECK_EQ_U(f.lanes[0].ageHighMs, 10u + VSS_CFG_RX_GAP_BUDGET_MS);
    CHECK_EQ_U(f.lanes[1].ageLowMs, 20u);
    CHECK_EQ_U(f.lanes[2].ageHighMs, 30u + VSS_CFG_RX_GAP_BUDGET_MS);
    CHECK_EQ_U(f.lanes[3].ageLowMs, 40u);
    for (i = 0u; i < 4u; ++i)
    {
        CHECK(f.lanes[i].ageComplete);
        CHECK(f.lanes[i].provenanceProven);
        CHECK(SameTime(&f.lanes[i].lossAt, &rx));
    }
    /* availability: lane values; activation is not provided */
    CHECK_EQ_U(f.lanes[0].availability, VSS_AVAILABILITY_NOT_PROVIDED);
    for (i = 0u; i < 3u; ++i)
    {
        m.rear_obstacle_availability = (NetAvailability)i;
        m.occupant_hazard_availability = (NetAvailability)i;
        m.antipinch_availability = (NetAvailability)i;
        CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
        CHECK_EQ_U(f.lanes[1].availability, availability[i]);
        CHECK_EQ_U(f.lanes[2].availability, availability[i]);
        CHECK_EQ_U(f.lanes[3].availability, availability[i]);
        CHECK_EQ_U(f.lanes[0].availability, VSS_AVAILABILITY_NOT_PROVIDED);
    }
    /* provenance needs boot and sequence */
    m = Warning(5u);
    m.rear_obstacle_source_boot = 0u;
    m.occupant_hazard_source_sequence = 0u;
    m.rear_activation_sequence = 0u;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK(!f.lanes[0].provenanceProven);
    CHECK(!f.lanes[1].provenanceProven);
    CHECK(!f.lanes[2].provenanceProven);
    CHECK(f.lanes[3].provenanceProven);
    meta.source.boot_id = 0u;
    m = Warning(5u);
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK(!f.lanes[0].provenanceProven);
}

static void test_activation_format_rows(void)
{
    NetVssWarningState m = Warning(1u);
    NetCanRxMeta meta = Meta(4u, 1u, 2000u);
    VssFactTime rx = Rx(VSS_TEST, 2000u);
    VssWarningFrame f;
    VssWarningContextObservation c;
    unsigned i;

    /* format 1: activation evaluation and basis present */
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK(f.lanes[0].evaluationPresent);
    CHECK(f.lanes[0].activationBasisProven);
    for (i = 1u; i < 4u; ++i)
    {
        CHECK(f.lanes[i].evaluationPresent);
        CHECK(!f.lanes[i].activationBasisProven);
    }
    /* format 0: canonical absence, all activation fields zero */
    m.activation_meta_format = 0u;
    m.rear_detection_activation = 0u;
    m.rear_activation_sequence = 0u;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &m, &meta, &rx, &f));
    CHECK(!f.lanes[0].evaluationPresent);
    CHECK(!f.lanes[0].activationBasisProven);
    CHECK(f.lanes[2].evaluationPresent);
    /* format 0 with any activation field set, and unknown formats, fail */
    for (i = 0u; i < 6u; ++i)
    {
        NetVssWarningState bad = m;
        switch (i)
        {
        case 0u: bad.rear_detection_activation = 1u; break;
        case 1u: bad.rear_activation_quality = NET_QUALITY_STALE; break;
        case 2u: bad.rear_activation_reason = NET_QUALITY_REASON_STALE; break;
        case 3u: bad.rear_activation_sequence = 1u; break;
        case 4u: bad.rear_activation_age_ms = 1u; break;
        default: bad.activation_meta_format = 2u; break;
        }
        CHECK(!VssInputNet_ConvertWarning(VSS_TEST, &bad, &meta, &rx, &f));
    }

    /* Through INPUT: format 0 leaves the activation unassessed (never
     * DISABLED/OK) while the other lanes apply. */
    Reset(2000u);
    m.occupant_hazard_state = NET_FLAG_YES;
    VssInputNet_OnWarningState(VSS_TEST, &m, &meta);
    CHECK(Obs(VSS_TEST).lastWarning.complete);
    CHECK(Input_ReadWarningContext(VSS_TEST, VSS_SIGNAL_REAR_ACTIVATION, &c));
    CHECK_EQ_U(c.quality, VSS_INPUT_UNASSESSED);
    CHECK(!c.lastValidPresent);
    CHECK(Input_ReadWarningContext(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &c));
    CHECK_EQ_U(c.quality, VSS_INPUT_VALID);
    CHECK_EQ_U(Obs(VSS_TEST).lastWarning.lanes[2].storeAdmission, VSS_STORE_UPDATED);
}

static void test_antipinch_unsupported_pattern(void)
{
    NetVssWarningState pattern = Warning(1u);
    NetCanRxMeta meta = Meta(4u, 1u, 2000u);
    VssFactTime rx = Rx(VSS_TEST, 2000u);
    VssWarningFrame f;
    VssWarningObservation w;
    unsigned i;

    pattern.antipinch_quality = NET_QUALITY_NO_DATA;
    pattern.antipinch_reason = NET_QUALITY_REASON_NO_DATA;
    pattern.antipinch_availability = NET_AVAILABILITY_UNAVAILABLE;
    pattern.antipinch_source_boot = 0u;
    pattern.antipinch_source_sequence = 0u;
    pattern.antipinch_original_age_ms = 65535u;
    CHECK(VssInputNet_ConvertWarning(VSS_TEST, &pattern, &meta, &rx, &f));
    CHECK_EQ_U(f.lanes[3].meaning, VSS_MEANING_UNSUPPORTED);
    CHECK_EQ_U(f.lanes[3].quality, VSS_INPUT_UNASSESSED);
    CHECK(!f.lanes[3].provenanceProven);
    CHECK(!f.lanes[3].ageComplete);
    CHECK_EQ_U(f.lanes[2].meaning, VSS_MEANING_CONFIRMED);
    /* any one field off the pattern is not the unsupported pattern */
    for (i = 0u; i < 6u; ++i)
    {
        NetVssWarningState near = pattern;
        switch (i)
        {
        case 0u: near.antipinch_quality = NET_QUALITY_INVALID; break;
        case 1u: near.antipinch_reason = NET_QUALITY_REASON_DATA_INVALID; break;
        case 2u: near.antipinch_availability = NET_AVAILABILITY_LIMITED; break;
        case 3u: near.antipinch_source_boot = 1u; break;
        case 4u: near.antipinch_source_sequence = 1u; break;
        default: near.antipinch_original_age_ms = 65534u; break;
        }
        CHECK(VssInputNet_ConvertWarning(VSS_TEST, &near, &meta, &rx, &f));
        CHECK_EQ_U(f.lanes[3].meaning, VSS_MEANING_UNCONFIRMED);
    }

    /* Through INPUT: the anti-pinch lane never becomes a candidate, the
     * other lanes of the same frame apply. */
    Reset(2000u);
    pattern.occupant_hazard_state = NET_FLAG_YES;
    VssInputNet_OnWarningState(VSS_TEST, &pattern, &meta);
    CHECK(Obs(VSS_TEST).lastWarning.complete);
    CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_ANTI_PINCH, &w));
    CHECK(!w.lastValidPresent);
    CHECK(!w.newSessionAllowed);
    CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &w));
    CHECK(w.newSessionAllowed);
}

/* structureVerified = every code in range. A frame that fails it is
 * rejected by INPUT without consuming its TX or changing the generation. */
static void test_structure_verified_row(void)
{
    NetCanRxMeta meta = Meta(4u, 1u, 2000u);
    VssFactTime rx = Rx(VSS_TEST, 2000u);
    VssWarningFrame f;
    VssWarningObservation w;
    unsigned i;

    for (i = 0u; i < 19u; ++i)
    {
        NetVssWarningState bad = Warning(1u);
        switch (i)
        {
        case 0u:  bad.antipinch_state = 2u; break;
        case 1u:  bad.occupant_hazard_state = 2u; break;
        case 2u:  bad.rear_obstacle_state = 3u; break;
        case 3u:  bad.rear_detection_activation = 2u; break;
        case 4u:  bad.antipinch_quality = 4u; break;
        case 5u:  bad.occupant_hazard_quality = 4u; break;
        case 6u:  bad.rear_obstacle_quality = 4u; break;
        case 7u:  bad.rear_activation_quality = 4u; break;
        case 8u:  bad.antipinch_reason = 10u; break;
        case 9u:  bad.occupant_hazard_reason = 10u; break;
        case 10u: bad.rear_obstacle_reason = 10u; break;
        case 11u: bad.rear_activation_reason = 10u; break;
        case 12u: bad.antipinch_availability = 3u; break;
        case 13u: bad.occupant_hazard_availability = 3u; break;
        case 14u: bad.rear_obstacle_availability = 3u; break;
        case 15u: bad.activation_meta_format = 2u; break;
        case 16u: bad.activation_meta_format = 255u; break;
        case 17u: bad.rear_obstacle_state = 255u; break;
        default:  bad.antipinch_reason = 255u; break;
        }
        CHECK(!VssInputNet_ConvertWarning(VSS_TEST, &bad, &meta, &rx, &f));
        CHECK(!f.structureVerified);
    }

    Reset(2000u);
    {
        NetVssWarningState good = Warning(1u), bad = Warning(1u);
        good.occupant_hazard_state = NET_FLAG_YES;
        VssInputNet_OnWarningState(VSS_TEST, &good, &meta);
        CHECK(Obs(VSS_TEST).lastWarning.complete);
        bad.occupant_hazard_state = 7u;
        meta = Meta(5u, 2u, 2000u); /* new boot on an unverified frame */
        VssInputNet_OnWarningState(VSS_TEST, &bad, &meta);
        CHECK_EQ_U(Obs(VSS_TEST).lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
        CHECK_EQ_U(Obs(VSS_TEST).warningFramesUnverified, 1u);
        CHECK_EQ_U(Obs(VSS_TEST).acceptedBoot, 4u);
        CHECK_EQ_U(Obs(VSS_TEST).generationChanges, 0u);
        CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &w));
        CHECK(w.newSessionAllowed && !w.provenanceLost);
        good = Warning(2u);
        meta = Meta(4u, 2u, 2000u); /* TX 2 was not consumed */
        VssInputNet_OnWarningState(VSS_TEST, &good, &meta);
        CHECK_EQ_U(Obs(VSS_TEST).lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
        CHECK_EQ_U(Obs(VSS_TEST).warningFramesForwarded, 3u);
    }
}

static void test_saturated_age_65535(void)
{
    NetVssEvent e = Event(NET_VSS_EVENT_WELCOME, 1u);
    NetVssWarningState w = Warning(1u);
    NetCanRxMeta meta = Meta(4u, 1u, 2000u);
    VssOccurrenceKey key = Key(VSS_TEST, 4u, 1u);
    VssOccurrenceObservation r;
    VssWarningObservation ws;
    VssWarningContextObservation c;

    Reset(2000u);
    e.original_age_ms = 65535u;
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_REJECTED);
    /* the occurrence is protected as rejected, never replayable */
    CHECK(Store_ReadOccurrence(&key, &r));
    CHECK(r.rejectedTombstone && !r.replayAllowed);

    w.occupant_hazard_state = NET_FLAG_YES;
    w.occupant_hazard_original_age_ms = 65535u;
    VssInputNet_OnWarningState(VSS_TEST, &w, &meta);
    CHECK(Obs(VSS_TEST).lastWarning.complete);
    CHECK(Input_ReadWarningContext(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &c));
    CHECK_EQ_U(c.quality, VSS_INPUT_INVALID);
    CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &ws));
    CHECK(!ws.lastValidPresent && !ws.newSessionAllowed);
    /* 65534 is a measured age */
    w = Warning(2u);
    w.occupant_hazard_state = NET_FLAG_YES;
    w.occupant_hazard_original_age_ms = 65534u;
    meta = Meta(4u, 2u, 2000u);
    VssInputNet_OnWarningState(VSS_TEST, &w, &meta);
    CHECK(Input_ReadWarningContext(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD, &c));
    CHECK(c.latestEvidence.ageComplete);
}

/* ---- Order and generation through the adapter --------------------------- */

/* Event and Warning are different CAN messages with their own TX_SEQUENCE. */
static void test_per_message_tx_order(void)
{
    NetVssWarningState w = Warning(1u);
    NetVssEvent e = Event(NET_VSS_EVENT_WELCOME, 1u);
    NetCanRxMeta meta;

    Reset(2000u);
    meta = Meta(1u, 300u, 2000u);
    VssApp_OnDomainWarningState(&w, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
    meta = Meta(1u, 5u, 2000u);
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    w = Warning(2u);
    meta = Meta(1u, 301u, 2000u);
    VssApp_OnDomainWarningState(&w, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
    e.occurrence_id = 2u;
    meta = Meta(1u, 6u, 2000u);
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK_EQ_U(Store_Count(), 2u);
}

/* A sender's retransmission of one occurrence: same occurrence_id, the next
 * TX, a new receive time and the age grown by the time since the first
 * delivery. Within the use limit it describes the same original interval
 * and STORE keeps it as a duplicate; once the use limit has passed since the
 * occurrence INPUT refuses it on time. A byte-identical frame delivered again
 * (same TX, same receive time) changes nothing. None of them adds a record. */
static void test_event_retransmission(void)
{
    NetVssEvent e = Event(NET_VSS_EVENT_WELCOME, 1u);
    NetCanRxMeta meta = Meta(1u, 5u, 2000u);
    VssOccurrenceKey key = Key(VSS_TEST, 1u, 1u);
    VssOccurrenceObservation r;

    Reset(2000u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);

    /* Retransmitted 200 ms later. */
    SetNowMs(2200u);
    e.original_age_ms = 200u;
    meta = Meta(1u, 6u, 2200u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_APPLICABLE);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_DUPLICATE);

    /* The same frame again, byte for byte. */
    SetNowMs(2205u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_NO_CHANGE);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_NOT_SUBMITTED);

    /* Last retransmission inside the use limit: the interval starts at 1995,
     * 2490 - 1995 = 495 < 500. */
    SetNowMs(2490u);
    e.original_age_ms = 490u;
    meta = Meta(1u, 7u, 2490u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_DUPLICATE);

    /* From the use limit on: refused on time, not a duplicate. */
    SetNowMs(2495u);
    e.original_age_ms = 495u;
    meta = Meta(1u, 8u, 2495u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.storeAdmission, VSS_STORE_NOT_SUBMITTED);
    SetNowMs(3000u);
    e.original_age_ms = 1000u;
    meta = Meta(1u, 9u, 3000u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK_EQ_U(Obs(VSS_TEST).lastEvent.validation, VSS_VALIDATION_REJECTED);

    CHECK_EQ_U(Store_Count(), 1u);
    memset(&r, 0, sizeof r);
    CHECK(Store_ReadOccurrence(&key, &r));
    CHECK_EQ_U(r.event.meta.deliveryOrder, 5u);
}

/* boot 1 event plays; a boot 2 frame takes the new generation over; a boot 2
 * event plays; the boot 1 event delivered again does not. */
static void test_domain_restart(void)
{
    NetVssEvent e = Event(NET_VSS_EVENT_LOCK_COMPLETE, 10u);
    NetVssWarningState w = Warning(1u);
    NetCanRxMeta meta;
    VssOccurrenceKey old = Key(VSS_PRODUCT, 1u, 10u), fresh = Key(VSS_PRODUCT, 2u, 10u);
    VssOccurrenceObservation before, r;
    VssTimeEvidence now;

    Reset(2000u);
    meta = Meta(1u, 7u, 2000u);
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    now = Rx(VSS_PRODUCT, 2000u);
    CHECK(Store_FirstStartAllowed(&old, &now));

    SetNowMs(2050u);
    meta = Meta(2u, 1u, 2050u);
    VssApp_OnDomainWarningState(&w, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).acceptedBoot, 2u);
    CHECK_EQ_U(Obs(VSS_PRODUCT).generationChanges, 1u);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
    CHECK(Store_ReadOccurrence(&old, &r));
    CHECK(!r.replayAllowed);

    meta = Meta(2u, 1u, 2050u); /* the new generation restarts its event TX */
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK(Store_ReadOccurrence(&fresh, &r));
    CHECK(r.replayAllowed);

    CHECK(Store_ReadOccurrence(&old, &before));
    meta = Meta(1u, 8u, 2050u); /* boot 1 event again */
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs(VSS_PRODUCT).acceptedBoot, 2u);
    CHECK(Store_ReadOccurrence(&old, &r));
    CHECK(memcmp(&before, &r, sizeof r) == 0);
    CHECK_EQ_U(Store_Count(), 2u);

    e.occurrence_id = 11u; /* boot 2 is still open */
    meta = Meta(2u, 2u, 2050u);
    VssApp_OnDomainEvent(&e, &meta);
    CHECK_EQ_U(Obs(VSS_PRODUCT).lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
}

/* The Network callbacks use the PRODUCT origin; TEST stays untouched. */
static void test_product_callbacks(void)
{
    NetVssEvent e = Event(NET_VSS_EVENT_WELCOME, 5u);
    NetCanRxMeta meta = Meta(9u, 1u, 2000u);
    VssOccurrenceKey product = Key(VSS_PRODUCT, 9u, 5u), test = Key(VSS_TEST, 9u, 5u);
    VssOccurrenceObservation r;
    VssInputNetObservation o;

    Reset(2000u);
    VssApp_OnDomainEvent(&e, &meta);
    CHECK(Store_ReadOccurrence(&product, &r));
    CHECK(!Store_ReadOccurrence(&test, &r));
    CHECK_EQ_U(Obs(VSS_PRODUCT).eventsForwarded, 1u);
    CHECK_EQ_U(Obs(VSS_TEST).eventsForwarded, 0u);
    VssInputNet_OnEvent(VSS_TEST, &e, &meta);
    CHECK(Store_ReadOccurrence(&test, &r));
    CHECK(!VssInputNet_Read((VssOrigin)5, &o));
    CHECK(!VssInputNet_Read(VSS_TEST, NULL));
}

/* ---- Status row ---------------------------------------------------------- */

static VssStatusSnapshot Snapshot(void)
{
    VssStatusSnapshot s;
    memset(&s, 0, sizeof s);
    s.state = VSS_SERVICE_PLAYING;
    s.availability = VSS_SERVICE_DEGRADED;
    s.accepting = true;
    s.currentMask = (uint8_t)((1u << VSS_FAULT_ASSET) | (1u << VSS_FAULT_STATE));
    s.lastFaultPresent = true;
    s.lastFaultKind = VSS_FAULT_STATE;
    s.lastFaultOccurrence = 3u;
    s.lastFaultAtMs = 1000u;
    s.reportSequence = 17u;
    s.evaluatedAtMs = 1900u;
    return s;
}

static void test_status_row(void)
{
    VssStatusSnapshot s = Snapshot();
    VssTimeEvidence now = Rx(VSS_PRODUCT, 2000u);
    NetVssStatus out;
    unsigned k;

    CHECK(VssInputNet_ToNetStatus(&s, &now, &out));
    CHECK_EQ_U(out.vss_state, NET_VSS_STATE_PLAYING);
    CHECK_EQ_U(out.vss_availability, NET_VSS_AVAILABILITY_DEGRADED);
    CHECK_EQ_U(out.vss_accepting_events, NET_FLAG_YES);
    CHECK_EQ_U(out.vss_fault_active, NET_FLAG_YES);
    CHECK_EQ_U(out.fault_mask, NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE | NET_VSS_FAULT_PLAYBACK_STATE_FAILURE);
    CHECK_EQ_U(out.vss_last_fault, NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE);
    CHECK_EQ_U(out.last_fault_occurrence, 3u);
    CHECK_EQ_U(out.last_fault_age_ms, 1000u);
    CHECK_EQ_U(out.last_fault_quality, NET_QUALITY_OK);
    CHECK_EQ_U(out.update_sequence, 17u);
    CHECK_EQ_U(out.source_age_ms, 100u);

    for (k = 0u; k < (unsigned)VSS_FAULT_KINDS; ++k)
    {
        s.lastFaultKind = (VssFaultKind)k;
        s.currentMask = (uint8_t)(1u << k);
        CHECK(VssInputNet_ToNetStatus(&s, &now, &out));
        CHECK_EQ_U(out.vss_last_fault, k + 1u);
        CHECK_EQ_U(out.fault_mask, 1u << k);
        CHECK_EQ_U(out.vss_fault_active, NET_FLAG_YES); /* every fault kind is an output fault */
    }
    /* ages saturate at 65535 */
    s = Snapshot();
    now = Rx(VSS_PRODUCT, 200000u);
    CHECK(VssInputNet_ToNetStatus(&s, &now, &out));
    CHECK_EQ_U(out.last_fault_age_ms, 65535u);
    CHECK_EQ_U(out.source_age_ms, 65535u);
    /* no fault recorded: code 0, occurrence 0, age 0, NO_DATA; current clear */
    s = Snapshot();
    s.currentMask = 0u;
    s.lastFaultPresent = false;
    s.lastFaultKind = (VssFaultKind)99; /* not read */
    s.lastFaultAtMs = 999999u;
    s.state = VSS_SERVICE_READY;
    s.availability = VSS_SERVICE_FULL;
    s.accepting = false;
    now = Rx(VSS_PRODUCT, 2000u);
    CHECK(VssInputNet_ToNetStatus(&s, &now, &out));
    CHECK_EQ_U(out.vss_fault_active, NET_FLAG_NO);
    CHECK_EQ_U(out.fault_mask, 0u);
    CHECK_EQ_U(out.vss_accepting_events, NET_FLAG_NO);
    CHECK_EQ_U(out.vss_last_fault, NET_VSS_FAULT_CODE_NONE);
    CHECK_EQ_U(out.last_fault_occurrence, 0u);
    CHECK_EQ_U(out.last_fault_age_ms, 0u);
    CHECK_EQ_U(out.last_fault_quality, NET_QUALITY_NO_DATA);
    CHECK_EQ_U(out.vss_state, NET_VSS_STATE_READY);
    CHECK_EQ_U(out.vss_availability, NET_VSS_AVAILABILITY_FULL);

    /* reports that cannot be converted leave out untouched */
    for (k = 0u; k < 11u; ++k)
    {
        VssStatusSnapshot bad = Snapshot();
        VssTimeEvidence t = Rx(VSS_PRODUCT, 2000u);
        memset(&out, 0xA5, sizeof out);
        switch (k)
        {
        case 0u:  bad.state = 4u; break;
        case 1u:  bad.availability = 3u; break;
        case 2u:  bad.currentMask = 0x20u; break;
        case 3u:  bad.reportSequence = 0u; break;
        case 4u:  bad.lastFaultKind = VSS_FAULT_KINDS; break;
        case 5u:  bad.lastFaultAtMs = 2001u; break;
        case 6u:  bad.evaluatedAtMs = 2001u; break;
        case 7u:  t.continuity = VSS_CONTINUITY_UNKNOWN; break;
        case 8u:  t.epoch = 0u; break;
        case 9u:  t.earliest = 2001u; break;
        default:  break;
        }
        if (k == 10u)
        {
            CHECK(!VssInputNet_ToNetStatus(NULL, &t, &out));
            CHECK(!VssInputNet_ToNetStatus(&bad, NULL, &out));
            CHECK(!VssInputNet_ToNetStatus(&bad, &t, NULL));
        }
        else
        {
            CHECK(!VssInputNet_ToNetStatus(&bad, &t, &out));
        }
        CHECK_EQ_U(out.update_sequence, 0xA5A5A5A5u);
    }
}

/* ---- Network stand-in and clock ----------------------------------------- */

static void test_net_stub(void)
{
    NetVssStatus s, read;
    VssStatusSnapshot snap = Snapshot();
    VssTimeEvidence now = Rx(VSS_PRODUCT, 2000u);

    Net_Init();
    Net_MainFunction(1234u);
    CHECK(!NetStub_ReadLastStatus(&read));
    CHECK_EQ_U(NetStub_PublishCount(), 0u);
    CHECK_EQ_U(VssNet_PublishStatus(NULL), NET_SUBMIT_INVALID_ARGUMENT);
    CHECK_EQ_U(NetStub_PublishCount(), 0u);
    CHECK(VssInputNet_ToNetStatus(&snap, &now, &s));
    CHECK_EQ_U(VssNet_PublishStatus(&s), NET_SUBMIT_OK);
    memset(&s, 0, sizeof s); /* the stand-in keeps its own copy */
    CHECK(NetStub_ReadLastStatus(&read));
    CHECK_EQ_U(read.update_sequence, 17u);
    CHECK_EQ_U(read.vss_last_fault, NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE);
    snap.reportSequence = 18u;
    CHECK(VssInputNet_ToNetStatus(&snap, &now, &s));
    CHECK_EQ_U(VssNet_PublishStatus(&s), NET_SUBMIT_OK);
    CHECK(NetStub_ReadLastStatus(&read));
    CHECK_EQ_U(read.update_sequence, 18u);
    CHECK_EQ_U(NetStub_PublishCount(), 2u);
    CHECK(!NetStub_ReadLastStatus(NULL));
}

static uint32_t OtherTicks(void)
{
    return 0u;
}

/* One registration per run: NULL and a second registration are refused and
 * leave the clock running on. */
static void test_clock(void)
{
    VssTime_ResetForTest();
    CHECK(!VssTime_Init(NULL));
    CHECK(!VssTime_IsRegistered());
    CHECK_EQ_U(VssTime_NowMs(), 0u);
    CHECK_EQ_U(VssTime_NowMs32(), 0u);
    s_ticks = 800u;
    CHECK(VssTime_Init(ReadTicks));
    CHECK(VssTime_IsRegistered());
    CHECK_EQ_U(VssTime_NowMs(), 1000u);
    s_ticks = 801u;
    CHECK_EQ_U(VssTime_NowMs(), 1001u); /* 1001.25 rounded down */
    s_ticks = 0xFFFFFF00u;
    CHECK_EQ_U(VssTime_NowMs(), (0xFFFFFF00ull * 5u) / 4u);
    s_ticks = 0x100u; /* tick counter wrapped */
    CHECK_EQ_U(VssTime_NowMs(), ((0x100000000ull + 0x100u) * 5u) / 4u);
    CHECK_EQ_U(VssTime_NowMs32(), (uint32_t)(((0x100000000ull + 0x100u) * 5u) / 4u));
    CHECK(!VssTime_Init(OtherTicks));
    CHECK(!VssTime_Init(ReadTicks));
    s_ticks = 0x200u;
    CHECK_EQ_U(VssTime_NowMs(), ((0x100000000ull + 0x200u) * 5u) / 4u);
}

static uint32_t Ms32OfTicks(uint64_t ticks)
{
    return (uint32_t)((ticks * 5u) / 4u);
}

/* A tick recorded earlier is placed on the VssTime_NowMs32 axis; a tick that
 * cannot be placed gives the current time and is counted. */
static void test_tick_to_ms32(void)
{
    VssTime_ResetForTest();
    CHECK_EQ_U(VssTime_UnplacedTicks(), 0u);
    CHECK_EQ_U(VssTime_TickToMs32(5u), 0u); /* no clock yet */
    CHECK_EQ_U(VssTime_UnplacedTicks(), 1u);

    s_ticks = 800u;
    CHECK(VssTime_Init(ReadTicks));
    CHECK_EQ_U(VssTime_TickToMs32(800u), VssTime_NowMs32());
    CHECK_EQ_U(VssTime_TickToMs32(800u), 1000u);
    CHECK_EQ_U(VssTime_TickToMs32(797u), 996u); /* 996.25 rounded down */
    CHECK_EQ_U(VssTime_TickToMs32(0u), 0u);
    CHECK_EQ_U(VssTime_UnplacedTicks(), 1u);
    CHECK_EQ_U(VssTime_TickToMs32(801u), 1000u); /* future */
    CHECK_EQ_U(VssTime_UnplacedTicks(), 2u);
    CHECK_EQ_U(VssTime_TickToMs32(0xFFFFFFF0u), 1000u); /* before tick 0 */
    CHECK_EQ_U(VssTime_UnplacedTicks(), 3u);

    /* tick counter wrapped: now 0x100 ticks past 2^32 */
    s_ticks = 0xFFFFFF00u;
    (void)VssTime_NowMs();
    s_ticks = 0x100u;
    CHECK_EQ_U(VssTime_TickToMs32(0x100u), VssTime_NowMs32());
    CHECK_EQ_U(VssTime_TickToMs32(0xFFFFFFF0u), Ms32OfTicks(0xFFFFFFF0ull));
    CHECK_EQ_U(VssTime_TickToMs32(0xFFFFFFFFu), Ms32OfTicks(0xFFFFFFFFull));
    CHECK_EQ_U(VssTime_TickToMs32(0u), Ms32OfTicks(0x100000000ull));
    CHECK_EQ_U(VssTime_TickToMs32(0x80u), Ms32OfTicks(0x100000080ull));
    /* 2^31 - 1 ticks back is placed, 2^31 is not */
    CHECK_EQ_U(VssTime_TickToMs32(0x100u - 0x7FFFFFFFu), Ms32OfTicks(0x100000100ull - 0x7FFFFFFFull));
    CHECK_EQ_U(VssTime_UnplacedTicks(), 3u);
    CHECK_EQ_U(VssTime_TickToMs32(0x100u - 0x80000000u), VssTime_NowMs32());
    CHECK_EQ_U(VssTime_UnplacedTicks(), 4u);
    CHECK_EQ_U(VssTime_TickToMs32(0x101u), VssTime_NowMs32()); /* future */
    CHECK_EQ_U(VssTime_UnplacedTicks(), 5u);

    /* around 2^32 ms the 32-bit value wraps with VssTime_NowMs32 */
    VssTime_ResetForTest();
    s_ticks = 3435973836u; /* 4294967295 ms */
    CHECK(VssTime_Init(ReadTicks));
    s_ticks = 3435973838u; /* 4294967297 ms */
    CHECK_EQ_U(VssTime_NowMs32(), 1u);
    CHECK_EQ_U(VssTime_TickToMs32(3435973838u), 1u);
    CHECK_EQ_U(VssTime_TickToMs32(3435973836u), 4294967295u);
    CHECK_EQ_U(VssTime_UnplacedTicks(), 0u);
}

int main(void)
{
    test_event_kind_row();
    test_event_field_rows();
    test_event_rows_reach_store();
    test_receive_time_row();
    test_warning_frame_and_value_rows();
    test_warning_quality_reason_meaning_rows();
    test_warning_source_and_time_rows();
    test_activation_format_rows();
    test_antipinch_unsupported_pattern();
    test_structure_verified_row();
    test_saturated_age_65535();
    test_per_message_tx_order();
    test_event_retransmission();
    test_domain_restart();
    test_product_callbacks();
    test_status_row();
    test_net_stub();
    test_clock();
    test_tick_to_ms32();
    TEST_FINISH();
}
