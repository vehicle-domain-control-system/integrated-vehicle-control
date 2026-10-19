/* Selection between events of the same rank whose original time intervals
 * overlap. Strictly ordered intervals play in time order. Overlapping
 * intervals from the same DOMAIN run (origin, source, generation) play in
 * occurrence-id order, because the DOMAIN issues the ids in order; without
 * that rule two nearly simultaneous events of one kind would both expire
 * while the selection kept asking for a fresh collection. Overlapping
 * intervals from different runs have no proved order and still do. */

#include "test_util.h"
#include "../../src/service/policy/vss_policy_store.h"
#include "../../src/service/asset/audio_asset.h"

#include <string.h>

static VssTimeEvidence At(uint64_t ms)
{
    VssTimeEvidence t;

    t.domain = VSS_TEST;
    t.epoch = 1u;
    t.earliest = ms;
    t.latest = ms;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

static void Reset(void)
{
    Store_TestReset();
    StorePlayback_TestReset();
}

static VssOccurrenceKey AddEvent(uint32_t generation, uint32_t id, VssInputSignal signal,
                                 uint64_t earliest, uint64_t latest)
{
    VssOneShotEvent e;

    memset(&e, 0, sizeof e);
    e.signal = signal;
    e.meta.key.origin = VSS_TEST;
    e.meta.key.source = VSS_SOURCE_DOMAIN;
    e.meta.key.generation = generation;
    e.meta.key.occurrenceId = id;
    e.meta.deliveryOrder = id;
    e.meta.original = At(earliest);
    e.meta.original.latest = latest;
    e.meta.receivedUseLimit = 500u;
    e.meta.useLimit = 500u;
    e.meta.validUntil = earliest + 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK_EQ_U(Store_ApplyInput(&e, NULL, NULL), VSS_STORE_ACCEPTED);
    return e.meta.key;
}

static VssPlanDescriptor Plan(VssInputSignal signal, uint32_t asset)
{
    VssPlanDescriptor p;

    memset(&p, 0, sizeof p);
    p.key = 1u;
    p.signal = signal;
    p.value = VSS_VALUE_EVENT;
    p.image = 1u;
    p.defined = true;
    p.available = true;
    p.cueCount = 1u;
    p.cues[0].asset = asset;
    p.repeatCount = 1u;
    return p;
}

static VssCandidateObservation s_candidates[VSS_CANDIDATE_CAPACITY];

/* Collects the candidates at now and asks for a selection with no session. */
static VssSelectionDecision Choose(uint64_t now, const VssPlanDescriptor *plan)
{
    VssTimeEvidence t = At(now);
    VssPolicyCurrent current;
    VssReadBasis basis;
    size_t count = 0u;

    CHECK(Store_ReadCandidates(&t, VSS_TEST, s_candidates, VSS_CANDIDATE_CAPACITY, &count));
    memset(&current, 0, sizeof current);
    memset(&basis, 0, sizeof basis);
    basis.origin = VSS_TEST;
    basis.time = t;
    basis.validity = VSS_READ_VALID;
    basis.storeRevision = Store_ReadRevision(VSS_TEST);
    basis.pbRevision = 1u;
    basis.startsAllowed = true;
    return Select_Choose(s_candidates, count, &current, plan, 1u, &basis);
}

static uint64_t s_nextFact = 1u;

/* Plays the occurrence to completion: bound, started at startMs, completed
 * at endMs, binding retired. */
static void Play(const VssOccurrenceKey *key, uint64_t session, uint64_t startMs, uint64_t endMs)
{
    VssOccurrenceObservation r;
    VssPlaybackFact f;

    CHECK(Store_ReadOccurrence(key, &r));
    CHECK(Store_BindPlayback(key, session, session, r.revision));
    CHECK(Store_MarkPlaybackAttempted(key, session, session));
    memset(&f, 0, sizeof f);
    f.present = true;
    f.fact = s_nextFact++;
    f.occurrence = *key;
    f.session = session;
    f.attempt = session;
    f.kind = (uint32_t)VSS_FACT_FIRST_ACTUAL_START;
    f.occurredAt = At(startMs);
    f.basisRevision = Store_ReadRevision(VSS_TEST);
    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    f.fact = s_nextFact++;
    f.kind = (uint32_t)VSS_FACT_WHOLE_COMPLETED;
    f.occurredAt = At(endMs);
    f.basisRevision = Store_ReadRevision(VSS_TEST);
    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    CHECK(Store_MarkPlaybackRetired(key, session, session, false));
}

static void test_overlapping_same_run_plays_in_occurrence_order(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_LOCK_COMPLETE, AUDIO_ASSET_ID_LOCK);
    VssOccurrenceKey first;
    VssOccurrenceKey second;
    VssSelectionDecision d;

    Reset();
    s_nextFact = 1u;
    /* The later id is accepted first and begins earlier, but overlaps. */
    second = AddEvent(3u, 2u, VSS_SIGNAL_LOCK_COMPLETE, 5u, 25u);
    first = AddEvent(3u, 1u, VSS_SIGNAL_LOCK_COMPLETE, 10u, 20u);
    d = Choose(100u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_CHOOSE);
    CHECK(d.candidatePresent);
    CHECK_EQ_U(d.candidate.key.occurrenceId, first.occurrenceId);

    /* Once the first is complete the second follows while still in its use
     * limit; after the limit nothing is left to choose. */
    Play(&first, 1u, 120u, 300u);
    d = Choose(320u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(d.candidate.key.occurrenceId, second.occurrenceId);
    d = Choose(505u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_WAIT);
    CHECK(!d.candidatePresent);
}

static void test_three_overlapping_events_in_order(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_WELCOME, AUDIO_ASSET_ID_WELCOME);
    VssOccurrenceKey k[3];
    VssSelectionDecision d;
    size_t i;

    Reset();
    s_nextFact = 1u;
    k[2] = AddEvent(3u, 30u, VSS_SIGNAL_WELCOME, 0u, 40u);
    k[0] = AddEvent(3u, 10u, VSS_SIGNAL_WELCOME, 20u, 30u);
    k[1] = AddEvent(3u, 20u, VSS_SIGNAL_WELCOME, 10u, 50u);
    for (i = 0u; i < 3u; ++i) {
        d = Choose(100u + 20u * i, &plan);
        CHECK_EQ_U(d.action, VSS_SELECTION_CHOOSE);
        CHECK_EQ_U(d.candidate.key.occurrenceId, k[i].occurrenceId);
        Play(&k[i], i + 1u, 101u + 20u * i, 110u + 20u * i);
    }
}

static void test_overlapping_different_runs_still_recollect(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_LOCK_COMPLETE, AUDIO_ASSET_ID_LOCK);
    VssSelectionDecision d;

    Reset();
    (void)AddEvent(3u, 1u, VSS_SIGNAL_LOCK_COMPLETE, 10u, 20u);
    (void)AddEvent(4u, 2u, VSS_SIGNAL_LOCK_COMPLETE, 15u, 25u);
    d = Choose(100u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_RECOLLECT);
}

static void test_strict_time_order_wins_over_occurrence_id(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_GOODBYE, AUDIO_ASSET_ID_GOODBYE);
    VssSelectionDecision d;

    Reset();
    (void)AddEvent(3u, 1u, VSS_SIGNAL_GOODBYE, 30u, 40u);
    (void)AddEvent(3u, 2u, VSS_SIGNAL_GOODBYE, 0u, 10u);
    d = Choose(100u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(d.candidate.key.occurrenceId, 2u);
}

static void test_different_ranks_are_not_affected(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_LOCK_ERROR, AUDIO_ASSET_ID_LOCK_ERROR);
    VssSelectionDecision d;

    Reset();
    (void)AddEvent(3u, 1u, VSS_SIGNAL_WELCOME, 10u, 20u);
    (void)AddEvent(3u, 2u, VSS_SIGNAL_LOCK_ERROR, 15u, 25u);
    d = Choose(100u, &plan);
    CHECK_EQ_U(d.action, VSS_SELECTION_CHOOSE);
    CHECK_EQ_U(d.candidate.signal, VSS_SIGNAL_LOCK_ERROR);
}

static void test_plan_assets_cover_the_nine_cues(void)
{
    VssPlanDescriptor plan = Plan(VSS_SIGNAL_REAR_RISK, AUDIO_ASSET_ID_REAR_EMERGENCY);

    plan.value = VSS_VALUE_EMERGENCY;
    CHECK(VssPolicy_ValidPlan(&plan));
    plan.cues[0].asset = AUDIO_ASSET_COUNT - 1u;
    CHECK(VssPolicy_ValidPlan(&plan));
    plan.cues[0].asset = AUDIO_ASSET_COUNT;
    CHECK(!VssPolicy_ValidPlan(&plan));
    plan.cues[0].asset = 0u;
    CHECK(VssPolicy_ValidPlan(&plan));
}

int main(void)
{
    test_overlapping_same_run_plays_in_occurrence_order();
    test_three_overlapping_events_in_order();
    test_overlapping_different_runs_still_recollect();
    test_strict_time_order_wins_over_occurrence_id();
    test_different_ranks_are_not_affected();
    test_plan_assets_cover_the_nine_cues();
    TEST_FINISH();
}
