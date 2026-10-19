/* DOMAIN generation changes through the adapter: late frames of replaced
 * boots, the rear fence across restarts, a reused boot, the receive monitor
 * of a new generation, the minimum frame a restart needs, events that
 * originated before INPUT opened, and input that arrives before the clock is
 * registered. */

#include <string.h>

#include "test_util.h"

#include "../../src/service/input/vss_input_net.h"
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

static void ResetInput(void)
{
    Input_TestReset();
    Store_TestReset();
    VssInputNet_ResetForTest();
}

static void Reset(uint64_t nowMs)
{
    ResetInput();
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

static NetVssEvent Event(uint32_t occurrence)
{
    NetVssEvent e;
    memset(&e, 0, sizeof e);
    e.event_kind = NET_VSS_EVENT_WELCOME;
    e.occurrence_id = occurrence;
    e.use_limit_ms = 500u;
    return e;
}

/* All lanes valid, rear EMERGENCY, others CLEAR, activation ENABLED. */
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
    w.rear_obstacle_state = NET_REAR_WARNING_EMERGENCY;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_sequence = sequence;
    return w;
}

static VssOccurrenceKey Key(uint32_t boot, uint32_t occurrence)
{
    VssOccurrenceKey k;
    k.origin = VSS_PRODUCT;
    k.source = VSS_SOURCE_DOMAIN;
    k.generation = boot;
    k.occurrenceId = occurrence;
    return k;
}

static VssTimeEvidence Now(uint64_t ms)
{
    VssTimeEvidence t;
    t.domain = VSS_PRODUCT;
    t.epoch = 1u;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

static VssInputNetObservation Obs(void)
{
    VssInputNetObservation o;
    memset(&o, 0, sizeof o);
    CHECK(VssInputNet_Read(VSS_PRODUCT, &o));
    return o;
}

static void SendWarning(uint32_t boot, uint32_t tx, uint64_t atMs, const NetVssWarningState *w)
{
    NetCanRxMeta m = Meta(boot, tx, (uint32_t)atMs);
    SetNowMs(atMs);
    VssApp_OnDomainWarningState(w, &m);
}

/* boot 1 -> 2 -> 3, then a late boot 1 frame: it neither takes the
 * generation back nor changes a warning, and boot 3 keeps working. */
static void late_frame_two_generations_old(void)
{
    NetVssWarningState w;
    NetVssEvent e = Event(10u);
    NetCanRxMeta m;
    VssOccurrenceKey late = Key(1u, 10u), live = Key(3u, 10u);
    VssOccurrenceObservation r;
    VssWarningObservation occupant;

    Reset(2000u);
    w = Warning(1u); SendWarning(1u, 1u, 2000u, &w);
    w = Warning(2u); SendWarning(2u, 1u, 2050u, &w);
    w = Warning(3u); SendWarning(3u, 1u, 2100u, &w);
    CHECK_EQ_U(Obs().acceptedBoot, 3u);
    CHECK_EQ_U(Obs().generationChanges, 2u);

    /* late boot 1 frame reporting an occupant hazard */
    w = Warning(9u);
    w.occupant_hazard_state = NET_FLAG_YES;
    m = Meta(1u, 2u, 2105u);
    SetNowMs(2110u);
    VssApp_OnDomainWarningState(&w, &m);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs().acceptedBoot, 3u);
    CHECK_EQ_U(Obs().generationChanges, 2u);
    CHECK(Store_ReadWarning(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD, &occupant));
    CHECK_EQ_U(occupant.lastValidValue, VSS_VALUE_CLEAR);
    CHECK_EQ_U(occupant.lastValidMeta.domainBoot, 3u);
    CHECK_EQ_U(occupant.lastValidMeta.evidenceSequence, 3u);

    /* late boot 1 event */
    m = Meta(1u, 5u, 2110u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK(!Store_ReadOccurrence(&late, &r));

    /* the live DOMAIN continues */
    w = Warning(4u); SendWarning(3u, 2u, 2120u, &w);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
    m = Meta(3u, 1u, 2120u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK(Store_ReadOccurrence(&live, &r));
    CHECK(r.replayAllowed);
    CHECK_EQ_U(Obs().acceptedBoot, 3u);
}

/* The last four replaced boots are all refused. */
static void four_replaced_boots_are_refused(void)
{
    NetVssWarningState w;
    uint32_t boot;

    Reset(2000u);
    for (boot = 1u; boot <= 5u; ++boot)
    {
        w = Warning(boot);
        SendWarning(boot, 1u, 2000u + 10u * boot, &w);
    }
    CHECK_EQ_U(Obs().acceptedBoot, 5u);
    CHECK_EQ_U(Obs().generationChanges, 4u);
    for (boot = 4u; boot >= 1u; --boot)
    {
        w = Warning(20u + boot);
        SendWarning(boot, 2u, 2060u + 5u * (5u - boot), &w);
        CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
        CHECK_EQ_U(Obs().acceptedBoot, 5u);
        CHECK_EQ_U(Obs().generationChanges, 4u);
    }
    w = Warning(30u); SendWarning(5u, 2u, 2085u, &w);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
}

/* Each restart fences the rear evidence of the generation it replaces. */
static void rear_fence_follows_each_restart(void)
{
    NetVssWarningState w;
    VssRearObservation g;

    Reset(2000u);
    w = Warning(5u); SendWarning(1u, 1u, 2000u, &w);
    CHECK(Store_ReadRear(VSS_PRODUCT, &g) && g.riskApplicable);

    /* restart 1: the same rear evidence 5 stays fenced */
    w = Warning(6u); w.rear_obstacle_source_sequence = 5u; SendWarning(2u, 1u, 2050u, &w);
    CHECK(Store_ReadRear(VSS_PRODUCT, &g) && !g.riskApplicable);

    /* new rear evidence 6 in boot 2 */
    w = Warning(7u); w.rear_obstacle_source_sequence = 6u; SendWarning(2u, 2u, 2060u, &w);
    CHECK(Store_ReadRear(VSS_PRODUCT, &g) && g.riskApplicable);

    /* restart 2: the same rear evidence 6 stays fenced as well */
    w = Warning(8u); w.rear_obstacle_source_sequence = 6u; SendWarning(3u, 1u, 2100u, &w);
    CHECK(Store_ReadRear(VSS_PRODUCT, &g));
    CHECK(!g.riskApplicable);
    CHECK_EQ_U(g.riskFence.domainBoot, 2u);
    CHECK_EQ_U(g.riskFence.evidenceSequence, 6u);

    /* a newer rear evaluation passes */
    w = Warning(9u); w.rear_obstacle_source_sequence = 7u; SendWarning(3u, 2u, 2110u, &w);
    CHECK(Store_ReadRear(VSS_PRODUCT, &g) && g.riskApplicable);
}

/* A boot id the DOMAIN uses again (1 -> 2 -> 1) is a replaced boot: its
 * frames are refused. Boot id uniqueness is the sender's obligation. */
static void reused_boot_is_refused(void)
{
    NetVssWarningState w;
    NetVssEvent e = Event(1u);
    NetCanRxMeta m;

    Reset(2000u);
    w = Warning(1u); SendWarning(1u, 1u, 2000u, &w);
    w = Warning(2u); SendWarning(2u, 1u, 2050u, &w);
    w = Warning(3u); SendWarning(1u, 1u, 3000u, &w);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
    m = Meta(1u, 1u, 3000u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs().acceptedBoot, 2u);
    CHECK_EQ_U(Obs().generationChanges, 1u);
}

/* The warning stream stops during a DOMAIN restart long enough to lose the
 * receive monitor; an event of the old boot meanwhile latches the loss and is
 * rejected. The new generation starts with a fresh monitor and takes its
 * first event. */
static void new_generation_starts_with_fresh_monitor(void)
{
    NetVssWarningState w = Warning(1u);
    NetVssEvent e = Event(1u);
    NetCanRxMeta m;

    Reset(2000u);
    SendWarning(1u, 1u, 2000u, &w);
    SetNowMs(2400u);
    m = Meta(1u, 1u, 2400u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    m = Meta(2u, 1u, 2400u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().generationChanges, 1u);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_APPLICABLE);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
}

/* A frame INPUT would refuse anyway (TX 0, occurrence 0, a receive time it
 * cannot place) neither opens INPUT nor takes a new generation over. */
static void restart_needs_a_usable_frame(void)
{
    NetVssWarningState w = Warning(1u);
    NetVssEvent e = Event(1u);
    NetCanRxMeta m;
    VssOccurrenceKey old = Key(1u, 1u);
    VssOccurrenceObservation r;
    unsigned k;

    for (k = 0u; k < 3u; ++k)
    {
        Reset(2000u);
        e = Event(k == 1u ? 0u : 1u);
        m = Meta(1u, k == 0u ? 0u : 1u, k == 2u ? 2100u : 2000u);
        VssApp_OnDomainEvent(&e, &m);
        CHECK(!Obs().generationOpen);
    }
    Reset(2000u);
    m = Meta(1u, 0u, 2000u);
    VssApp_OnDomainWarningState(&w, &m);
    CHECK(!Obs().generationOpen);
    m = Meta(1u, 1u, 2100u);
    VssApp_OnDomainWarningState(&w, &m);
    CHECK(!Obs().generationOpen);

    Reset(2000u);
    e = Event(1u);
    m = Meta(1u, 1u, 2000u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    for (k = 0u; k < 4u; ++k)
    {
        e = Event(k == 1u ? 0u : 2u);
        m = Meta(2u, k == 0u ? 0u : 1u, k == 2u ? 2100u : 2000u);
        if (k == 3u)
        {
            m = Meta(2u, 0u, 2000u);
            VssApp_OnDomainWarningState(&w, &m);
        }
        else
        {
            VssApp_OnDomainEvent(&e, &m);
        }
        CHECK_EQ_U(Obs().acceptedBoot, 1u);
        CHECK_EQ_U(Obs().generationChanges, 0u);
        CHECK(Store_ReadOccurrence(&old, &r) && r.replayAllowed);
    }
    m = Meta(2u, 1u, 2000u);
    VssApp_OnDomainWarningState(&w, &m);
    CHECK_EQ_U(Obs().acceptedBoot, 2u);
    CHECK_EQ_U(Obs().generationChanges, 1u);
}

/* After a VSS start the old ledger is gone: an event that originated before
 * INPUT opened may already have played and is not replayed. */
static void event_older_than_opening_is_not_replayed(void)
{
    NetVssWarningState w = Warning(1u);
    NetVssEvent e;
    NetCanRxMeta m;
    VssOccurrenceKey before = Key(1u, 1u), after = Key(1u, 2u);
    VssTimeEvidence now = Now(2010u);

    Reset(2000u);
    SendWarning(1u, 1u, 2000u, &w); /* opens at 2000 */
    SetNowMs(2010u);
    e = Event(1u);
    e.original_age_ms = 200u; /* originated by 1810 */
    m = Meta(1u, 1u, 2010u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK(!Store_FirstStartAllowed(&before, &now));
    e = Event(2u);
    e.original_age_ms = 5u; /* originated by 2005 */
    m = Meta(1u, 2u, 2010u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK(Store_FirstStartAllowed(&after, &now));

    /* the event that opens INPUT itself */
    Reset(2000u);
    e = Event(1u);
    m = Meta(1u, 1u, 1995u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK(Obs().generationOpen);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    Reset(2000u);
    m = Meta(1u, 1u, 2000u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
}

/* Input before the clock is registered would open INPUT at time 0: dropped. */
static void input_before_clock_registration_is_dropped(void)
{
    NetVssEvent e = Event(1u);
    NetVssWarningState w = Warning(1u);
    NetCanRxMeta m = Meta(1u, 1u, 0u);

    ResetInput();
    VssTime_ResetForTest();
    CHECK(!VssTime_Init(NULL));
    VssApp_OnDomainEvent(&e, &m);
    VssApp_OnDomainWarningState(&w, &m);
    CHECK(!Obs().generationOpen);
    CHECK_EQ_U(Obs().eventsForwarded, 0u);
    CHECK_EQ_U(Obs().warningFramesForwarded, 0u);
    CHECK_EQ_U(Obs().inputsWithoutClock, 2u);
    CHECK_EQ_U(Obs().eventsDiscarded, 0u);

    /* registered: the same event opens INPUT */
    SetNowMs(2000u);
    CHECK(VssTime_Init(ReadTicks));
    m = Meta(1u, 1u, 2000u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK(Obs().generationOpen);
    CHECK_EQ_U(Obs().lastEvent.storeAdmission, VSS_STORE_ACCEPTED);
    CHECK_EQ_U(Obs().inputsWithoutClock, 2u);
}

/* TX back to 1 within the accepted boot: the sender restarted without a new
 * boot id. INPUT keeps rejecting the stream; the adapter counts the cause. */
static void tx_back_to_one_in_same_boot_is_counted(void)
{
    NetVssWarningState w;
    NetVssEvent e = Event(1u);
    NetCanRxMeta m;
    uint32_t tx;

    Reset(2000u);
    for (tx = 1u; tx <= 3u; ++tx)
    {
        w = Warning(tx);
        SendWarning(1u, tx, 2000u + 10u * tx, &w);
    }
    m = Meta(1u, 1u, 2030u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().txRestartsSameBoot, 0u);
    w = Warning(50u);
    SendWarning(1u, 1u, 2050u, &w);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs().txRestartsSameBoot, 1u);
    w = Warning(51u);
    SendWarning(1u, 2u, 2060u, &w); /* still below the old TX: not counted again */
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs().txRestartsSameBoot, 1u);
    e = Event(2u);
    m = Meta(1u, 2u, 2060u);
    VssApp_OnDomainEvent(&e, &m);
    e = Event(3u);
    m = Meta(1u, 1u, 2060u);
    VssApp_OnDomainEvent(&e, &m);
    CHECK_EQ_U(Obs().lastEvent.validation, VSS_VALIDATION_REJECTED);
    CHECK_EQ_U(Obs().txRestartsSameBoot, 2u);

    /* a new boot starts its own TX at 1 */
    w = Warning(60u);
    SendWarning(2u, 1u, 2070u, &w);
    CHECK_EQ_U(Obs().lastWarning.frameValidation, VSS_VALIDATION_APPLICABLE);
    CHECK_EQ_U(Obs().txRestartsSameBoot, 2u);
}

/* The adapter keeps the raw quality and reason INPUT receives reduced. */
static void raw_lane_quality_and_reason_are_kept(void)
{
    NetVssWarningState w = Warning(1u);
    VssWarningContextObservation c;

    Reset(2000u);
    w.rear_activation_quality = NET_QUALITY_INVALID;
    w.rear_activation_reason = NET_QUALITY_REASON_NOT_READY;
    w.rear_obstacle_quality = NET_QUALITY_STALE;
    w.rear_obstacle_reason = NET_QUALITY_REASON_STALE;
    w.occupant_hazard_quality = NET_QUALITY_INVALID;
    w.occupant_hazard_reason = NET_QUALITY_REASON_VISION_FAULT;
    w.antipinch_quality = NET_QUALITY_INVALID;
    w.antipinch_reason = NET_QUALITY_REASON_RECOVERING;
    SendWarning(1u, 1u, 2000u, &w);
    CHECK_EQ_U(Obs().warningRawQuality[0], NET_QUALITY_INVALID);
    CHECK_EQ_U(Obs().warningRawReason[0], NET_QUALITY_REASON_NOT_READY);
    CHECK_EQ_U(Obs().warningRawQuality[1], NET_QUALITY_STALE);
    CHECK_EQ_U(Obs().warningRawReason[1], NET_QUALITY_REASON_STALE);
    CHECK_EQ_U(Obs().warningRawQuality[2], NET_QUALITY_INVALID);
    CHECK_EQ_U(Obs().warningRawReason[2], NET_QUALITY_REASON_VISION_FAULT);
    CHECK_EQ_U(Obs().warningRawQuality[3], NET_QUALITY_INVALID);
    CHECK_EQ_U(Obs().warningRawReason[3], NET_QUALITY_REASON_RECOVERING);
    /* INPUT still sees the reduced reason */
    CHECK(Input_ReadWarningContext(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD, &c));
    CHECK_EQ_U(c.latestEvidence.reason, VSS_REASON_DATA_INVALID);

    /* an unverified frame is kept as received too */
    w = Warning(2u);
    w.rear_obstacle_reason = 200u;
    SendWarning(1u, 2u, 2010u, &w);
    CHECK_EQ_U(Obs().warningFramesUnverified, 1u);
    CHECK_EQ_U(Obs().warningRawReason[1], 200u);
    CHECK_EQ_U(Obs().warningRawReason[2], NET_QUALITY_REASON_NONE);
}

/* The 32-bit millisecond value wraps with the 64-bit clock. */
static void clock_32_bit_wrap(void)
{
    s_ticks = 3435973836u; /* 4294967295 ms */
    VssTime_ResetForTest();
    CHECK(VssTime_Init(ReadTicks));
    CHECK_EQ_U(VssTime_NowMs(), 4294967295ull);
    CHECK_EQ_U(VssTime_NowMs32(), 4294967295u);
    s_ticks = 3435973838u;
    CHECK_EQ_U(VssTime_NowMs(), 4294967297ull);
    CHECK_EQ_U(VssTime_NowMs32(), 1u);
}

int main(void)
{
    late_frame_two_generations_old();
    four_replaced_boots_are_refused();
    rear_fence_follows_each_restart();
    reused_boot_is_refused();
    new_generation_starts_with_fresh_monitor();
    restart_needs_a_usable_frame();
    event_older_than_opening_is_not_replayed();
    input_before_clock_registration_is_dropped();
    tx_back_to_one_in_same_boot_is_counted();
    raw_lane_quality_and_reason_are_kept();
    clock_32_bit_wrap();
    TEST_FINISH();
}
