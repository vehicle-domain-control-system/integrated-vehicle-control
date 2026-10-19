/* Ledger, binding and fact reclaim. The capacities are 128 records, 32
 * bindings and 64 facts per run; without reclaim a run can play 32 one-shot
 * events and the 33rd binding fails. Reclaim frees every record whose
 * original interval ended VSS_CFG_STORE_RECLAIM_MS or more ago and whose
 * bindings are all retired: terminated records, tombstones, records that
 * lost replay in a DOMAIN generation change, and still-pending ones.
 * Store_ReadRevision must never decrease across a reclaim. */

#include "test_util.h"
#include "../../src/service/policy/vss_policy_store.h"

#include <string.h>

#define RECLAIM_MS ((uint64_t)VSS_CFG_STORE_RECLAIM_MS)

static VssTimeEvidence At(VssOrigin origin, uint64_t ms)
{
    VssTimeEvidence t;

    t.domain = origin;
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

static VssOccurrenceKey Key(VssOrigin origin, uint32_t generation, uint32_t id)
{
    VssOccurrenceKey k;

    k.origin = origin;
    k.source = VSS_SOURCE_DOMAIN;
    k.generation = generation;
    k.occurrenceId = id;
    return k;
}

/* Accepts a one-shot event whose original interval is [original, original]. */
static VssOccurrenceKey AddEvent(VssOrigin origin, uint32_t generation, uint32_t id,
                                 VssInputSignal signal, uint64_t original)
{
    VssOneShotEvent e;

    memset(&e, 0, sizeof e);
    e.signal = signal;
    e.meta.key = Key(origin, generation, id);
    e.meta.deliveryOrder = id;
    e.meta.original = At(origin, original);
    e.meta.receivedUseLimit = 500u;
    e.meta.useLimit = 500u;
    e.meta.validUntil = original + 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK_EQ_U(Store_ApplyInput(&e, NULL, NULL), VSS_STORE_ACCEPTED);
    return e.meta.key;
}

static VssPlaybackFact Fact(const VssOccurrenceKey *key, uint64_t id, uint64_t session,
                            VssPlaybackFactKind kind, uint64_t when)
{
    VssPlaybackFact f;

    memset(&f, 0, sizeof f);
    f.present = true;
    f.fact = id;
    f.occurrence = *key;
    f.session = session;
    f.attempt = session;
    f.kind = (uint32_t)kind;
    f.occurredAt = At(key->origin, when);
    f.occurredAt.latest = when + 3u;
    f.basisRevision = Store_ReadRevision(key->origin);
    return f;
}

static uint64_t s_nextFact = 1u;

static bool Present(VssOrigin origin, uint32_t generation, uint32_t id)
{
    VssOccurrenceKey k = Key(origin, generation, id);
    VssOccurrenceObservation r;

    return Store_ReadOccurrence(&k, &r);
}

/* Binds session to the record; returns false when the binding was refused. */
static bool Bind(const VssOccurrenceKey *key, uint64_t session)
{
    VssOccurrenceObservation r;

    if (!Store_ReadOccurrence(key, &r)) {
        return false;
    }
    if (!Store_BindPlayback(key, session, session, r.revision)) {
        return false;
    }
    CHECK(Store_MarkPlaybackAttempted(key, session, session));
    return true;
}

/* Plays the bound session to completion: first start at original + 10,
 * completed at original + 100, binding retired. */
static void Finish(const VssOccurrenceKey *key, uint64_t session, uint64_t original)
{
    VssPlaybackFact f = Fact(key, s_nextFact++, session, VSS_FACT_FIRST_ACTUAL_START, original + 10u);

    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    f = Fact(key, s_nextFact++, session, VSS_FACT_WHOLE_COMPLETED, original + 100u);
    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    CHECK(Store_MarkPlaybackRetired(key, session, session, false));
}

/* 200 different events 100 ms apart, each played to completion. With reclaim
 * after each one the ledger stays small; without it the 33rd binding fails. */
static uint32_t PlayMany(VssOrigin origin, bool reclaim, uint32_t total)
{
    uint64_t revisionBefore = 0u;
    size_t maxCount = 0u;
    uint32_t i;
    uint32_t bound = 0u;

    for (i = 1u; i <= total; ++i) {
        uint64_t original = (uint64_t)i * 100u;
        VssOccurrenceKey k = AddEvent(origin, 1u, i, VSS_SIGNAL_WELCOME, original);
        uint64_t revision;
        VssTimeEvidence now;

        if (!Bind(&k, i)) {
            break;
        }
        ++bound;
        Finish(&k, i, original);
        revision = Store_ReadRevision(origin);
        CHECK(revision >= revisionBefore);
        revisionBefore = revision;
        if (reclaim) {
            now = At(origin, original + 150u);
            (void)Store_Reclaim(&now);
            /* A reclaim changes nothing a reader could act on. */
            CHECK_EQ_U(Store_ReadRevision(origin), revision);
        }
        if (Store_Count() > maxCount) {
            maxCount = Store_Count();
        }
    }
    if (reclaim) {
        /* 2500 ms / 100 ms spacing: about 26 records alive at a time. */
        CHECK(maxCount <= 30u);
    }
    return bound;
}

static void test_without_reclaim_the_33rd_binding_fails(void)
{
    Reset();
    s_nextFact = 1u;
    CHECK_EQ_U(PlayMany(VSS_TEST, false, 200u), VSS_PLAYBACK_BINDINGS_CAPACITY);
}

static void test_two_hundred_events_with_reclaim(void)
{
    Reset();
    s_nextFact = 1u;
    CHECK_EQ_U(PlayMany(VSS_TEST, true, 200u), 200u);
}

static void test_reclaim_age_boundary(void)
{
    VssOccurrenceKey k;
    VssTimeEvidence now;
    uint64_t revision;

    Reset();
    s_nextFact = 1u;
    k = AddEvent(VSS_TEST, 1u, 1u, VSS_SIGNAL_GOODBYE, 1000u);
    CHECK(Bind(&k, 1u));
    Finish(&k, 1u, 1000u);
    revision = Store_ReadRevision(VSS_TEST);
    now = At(VSS_TEST, 1000u + RECLAIM_MS - 1u);
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    CHECK_EQ_U(Store_Count(), 1u);
    now = At(VSS_TEST, 1000u + RECLAIM_MS);
    CHECK_EQ_U(Store_Reclaim(&now), 1u);
    CHECK_EQ_U(Store_Count(), 0u);
    CHECK_EQ_U(Store_ReadRevision(VSS_TEST), revision);
    /* The same key is a new record afterwards; INPUT decides its admission. */
    k = AddEvent(VSS_TEST, 1u, 1u, VSS_SIGNAL_GOODBYE, 5000u);
    CHECK(Store_ReadRevision(VSS_TEST) > revision);
    CHECK(Bind(&k, 2u));
}

static void test_reclaim_ignores_unusable_time(void)
{
    VssOccurrenceKey k;
    VssTimeEvidence now;

    Reset();
    s_nextFact = 1u;
    k = AddEvent(VSS_TEST, 1u, 1u, VSS_SIGNAL_GOODBYE, 0u);
    CHECK(Bind(&k, 1u));
    Finish(&k, 1u, 0u);
    CHECK_EQ_U(Store_Reclaim(NULL), 0u);
    now = At(VSS_TEST, 10000u);
    now.continuity = VSS_CONTINUITY_UNKNOWN;
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    now = At(VSS_TEST, 10000u);
    now.epoch = 2u;
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    CHECK_EQ_U(Store_Count(), 1u);
    now = At(VSS_TEST, 10000u);
    CHECK_EQ_U(Store_Reclaim(&now), 1u);
}

static void test_tombstone_is_reclaimed_from_its_rejection_time(void)
{
    VssOccurrenceKey k = Key(VSS_TEST, 1u, 7u);
    VssTimeEvidence at = At(VSS_TEST, 1000u);
    VssTimeEvidence now;
    VssOccurrenceObservation r;
    VssOneShotEvent e;

    Reset();
    Store_ProtectRejectedEvent(&k, VSS_SIGNAL_LOCK_ERROR, &at);
    CHECK_EQ_U(Store_Count(), 1u);
    CHECK(Store_ReadOccurrence(&k, &r));
    CHECK(r.rejectedTombstone);
    CHECK_EQ_U(r.event.meta.original.latest, 1000u);
    now = At(VSS_TEST, 1000u + RECLAIM_MS - 1u);
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    now = At(VSS_TEST, 1000u + RECLAIM_MS);
    CHECK_EQ_U(Store_Reclaim(&now), 1u);
    CHECK_EQ_U(Store_Count(), 0u);
    CHECK(!Store_ReadOccurrence(&k, &r));
    /* Only INPUT protects against the occurrence coming back: STORE accepts. */
    memset(&e, 0, sizeof e);
    e.signal = VSS_SIGNAL_LOCK_ERROR;
    e.meta.key = k;
    e.meta.deliveryOrder = 9u;
    e.meta.original = At(VSS_TEST, 4000u);
    e.meta.receivedUseLimit = 500u;
    e.meta.useLimit = 500u;
    e.meta.validUntil = 4500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK_EQ_U(Store_ApplyInput(&e, NULL, NULL), VSS_STORE_ACCEPTED);

    /* A tombstone without a time is never reclaimed. */
    Reset();
    Store_ProtectRejectedEvent(&k, VSS_SIGNAL_LOCK_ERROR, NULL);
    now = At(VSS_TEST, 100000u);
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    CHECK_EQ_U(Store_Count(), 1u);
}

static void test_live_binding_keeps_the_record(void)
{
    VssOccurrenceKey k;
    VssTimeEvidence now = At(VSS_TEST, 100000u);
    VssPlaybackFact f;

    Reset();
    s_nextFact = 1u;
    k = AddEvent(VSS_TEST, 1u, 1u, VSS_SIGNAL_WELCOME, 0u);
    CHECK(Bind(&k, 1u));
    f = Fact(&k, s_nextFact++, 1u, VSS_FACT_FIRST_ACTUAL_START, 10u);
    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    CHECK_EQ_U(Store_Count(), 1u);
    f = Fact(&k, s_nextFact++, 1u, VSS_FACT_WHOLE_INTERRUPTED, 50u);
    CHECK_EQ_U(Store_ApplyPlaybackFact(&f), VSS_FACT_APPLIED);
    CHECK(Store_MarkPlaybackRetired(&k, 1u, 1u, false));
    CHECK_EQ_U(Store_Reclaim(&now), 1u);
    CHECK_EQ_U(Store_Count(), 0u);
    /* The freed binding and facts are usable again. */
    k = AddEvent(VSS_TEST, 1u, 2u, VSS_SIGNAL_WELCOME, 100000u);
    CHECK(Bind(&k, 2u));
}

/* A DOMAIN generation change makes INPUT report every open event lane of the
 * old generation as lost (INVALID, loss time of unknown continuity). STORE
 * then withdraws replay from the old records; those slots must come back. */
static void LoseGeneration(VssOrigin origin, uint32_t generation, VssInputSignal signal,
                           uint64_t at)
{
    VssQualityChange q;

    memset(&q, 0, sizeof q);
    q.signal = signal;
    q.quality = VSS_INPUT_INVALID;
    q.basis.key = Key(origin, generation, 1u);
    q.lossAt = At(origin, at);
    q.lossAt.continuity = VSS_CONTINUITY_UNKNOWN;
    CHECK_EQ_U(Store_ApplyInput(NULL, NULL, &q), VSS_STORE_UPDATED);
}

static void test_generation_change_frees_old_records(void)
{
    uint32_t i;
    VssOccurrenceKey k;
    VssOccurrenceObservation r;
    VssTimeEvidence now;
    uint64_t revision;

    Reset();
    s_nextFact = 1u;
    /* Generation 1: 32 events played (bindings full), 8 pending ones never
     * selected. */
    for (i = 1u; i <= VSS_PLAYBACK_BINDINGS_CAPACITY; ++i) {
        k = AddEvent(VSS_TEST, 1u, i, VSS_SIGNAL_WELCOME, (uint64_t)i * 10u);
        CHECK(Bind(&k, i));
        Finish(&k, i, (uint64_t)i * 10u);
    }
    for (i = 33u; i <= 40u; ++i) {
        (void)AddEvent(VSS_TEST, 1u, i, VSS_SIGNAL_WELCOME, 1000u);
    }
    CHECK_EQ_U(Store_Count(), 40u);
    LoseGeneration(VSS_TEST, 1u, VSS_SIGNAL_WELCOME, 1200u);
    k = Key(VSS_TEST, 1u, 40u);
    CHECK(Store_ReadOccurrence(&k, &r));
    CHECK(!r.replayAllowed);
    CHECK_EQ_U(r.state, VSS_OCCURRENCE_PENDING);
    revision = Store_ReadRevision(VSS_TEST);

    /* Generation 2 begins; its first event is the 33rd binding of the run. */
    k = AddEvent(VSS_TEST, 2u, 1u, VSS_SIGNAL_WELCOME, 1500u);
    CHECK(!Bind(&k, 41u));
    now = At(VSS_TEST, 1000u + RECLAIM_MS);
    CHECK_EQ_U(Store_Reclaim(&now), 40u);
    CHECK_EQ_U(Store_Count(), 1u);
    CHECK(Store_ReadRevision(VSS_TEST) >= revision);
    CHECK(!Present(VSS_TEST, 1u, 40u));
    CHECK(Store_ReadOccurrence(&k, &r));
    /* The pending generation-2 record is kept: its interval is recent. */
    CHECK(Bind(&k, 41u));
    Finish(&k, 41u, 1500u);
    for (i = 2u; i <= 40u; ++i) {
        k = AddEvent(VSS_TEST, 2u, i, VSS_SIGNAL_WELCOME, 3500u + (uint64_t)i * 100u);
        CHECK(Bind(&k, 40u + i));
        Finish(&k, 40u + i, 3500u + (uint64_t)i * 100u);
        now = At(VSS_TEST, 3500u + (uint64_t)i * 100u + 150u);
        (void)Store_Reclaim(&now);
    }
}

static void test_pending_record_past_the_limit_is_reclaimed(void)
{
    VssOccurrenceKey k;
    VssTimeEvidence now;

    Reset();
    k = AddEvent(VSS_TEST, 1u, 1u, VSS_SIGNAL_WELCOME, 0u);
    now = At(VSS_TEST, RECLAIM_MS - 1u);
    CHECK_EQ_U(Store_Reclaim(&now), 0u);
    now = At(VSS_TEST, RECLAIM_MS);
    CHECK(!Store_FirstStartAllowed(&k, &now));
    CHECK_EQ_U(Store_Reclaim(&now), 1u);
    CHECK_EQ_U(Store_Count(), 0u);
}

static void test_both_origins_are_reclaimed_in_one_call(void)
{
    uint32_t i;
    VssOccurrenceKey k;
    VssTimeEvidence now;
    uint64_t revisionTest;
    uint64_t revisionProduct;

    Reset();
    s_nextFact = 1u;
    /* 16 TEST and 16 PRODUCT events fill the 32 bindings. */
    for (i = 1u; i <= 16u; ++i) {
        k = AddEvent(VSS_TEST, 1u, i, VSS_SIGNAL_GOODBYE, (uint64_t)i * 20u);
        CHECK(Bind(&k, 2u * i - 1u));
        Finish(&k, 2u * i - 1u, (uint64_t)i * 20u);
        k = AddEvent(VSS_PRODUCT, 1u, i, VSS_SIGNAL_GOODBYE, (uint64_t)i * 20u);
        CHECK(Bind(&k, 2u * i));
        Finish(&k, 2u * i, (uint64_t)i * 20u);
    }
    k = AddEvent(VSS_TEST, 1u, 17u, VSS_SIGNAL_GOODBYE, 400u);
    CHECK(!Bind(&k, 33u));
    revisionTest = Store_ReadRevision(VSS_TEST);
    revisionProduct = Store_ReadRevision(VSS_PRODUCT);
    /* One call with TEST time sweeps the PRODUCT records as well. */
    now = At(VSS_TEST, 320u + RECLAIM_MS);
    CHECK_EQ_U(Store_Reclaim(&now), 32u);
    CHECK_EQ_U(Store_Count(), 1u);
    CHECK_EQ_U(Store_ReadRevision(VSS_TEST), revisionTest);
    CHECK_EQ_U(Store_ReadRevision(VSS_PRODUCT), revisionProduct);
    for (i = 1u; i <= 16u; ++i) {
        CHECK(!Present(VSS_PRODUCT, 1u, i));
        CHECK(!Present(VSS_TEST, 1u, i));
    }
    /* The 33rd and later bindings of both origins succeed. */
    CHECK(Bind(&k, 33u));
    Finish(&k, 33u, 400u);
    k = AddEvent(VSS_PRODUCT, 1u, 17u, VSS_SIGNAL_GOODBYE, 3000u);
    CHECK(Bind(&k, 34u));
    Finish(&k, 34u, 3000u);
    CHECK(Store_ReadRevision(VSS_TEST) > revisionTest);
    CHECK(Store_ReadRevision(VSS_PRODUCT) > revisionProduct);
}

int main(void)
{
    test_without_reclaim_the_33rd_binding_fails();
    test_two_hundred_events_with_reclaim();
    test_reclaim_age_boundary();
    test_reclaim_ignores_unusable_time();
    test_tombstone_is_reclaimed_from_its_rejection_time();
    test_live_binding_keeps_the_record();
    test_generation_change_frees_old_records();
    test_pending_record_past_the_limit_is_reclaimed();
    test_both_origins_are_reclaimed_in_one_call();
    TEST_FINISH();
}
