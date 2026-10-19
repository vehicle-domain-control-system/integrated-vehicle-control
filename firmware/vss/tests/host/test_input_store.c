#include "../../src/service/input/vss_input_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A failed check stops the program: later checks read state that the failed
 * one guarded. The RESULT line is what host_test.py collects. */
static unsigned checks;
#define CHECK(c) do { ++checks; if (!(c)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    printf("RESULT passed=%u failed=1\n", checks - 1u); exit(1); } } while (0)
#define RUN(f) do { f(); printf("PASS %s\n", #f); } while (0)

static VssFactTime at(VssOrigin origin, uint64_t t)
{
    VssFactTime r = {VSS_PRODUCT, 1u, 0u, 0u, VSS_CONTINUITY_COMPARABLE};
    r.domain = origin; r.earliest = t; r.latest = t;
    return r;
}

static VssOccurrenceKey key(uint32_t id)
{
    VssOccurrenceKey k = {VSS_PRODUCT, VSS_SOURCE_DOMAIN, 7u, 0u};
    k.occurrenceId = id;
    return k;
}

static VssInputEvidence event(const VssOccurrenceKey *k, uint32_t tx, uint64_t rx, uint32_t age)
{
    VssInputEvidence e = {0};
    e.signal = VSS_SIGNAL_LOCK_COMPLETE;
    e.value = VSS_VALUE_EVENT;
    e.quality = VSS_INPUT_VALID;
    e.occurrence = k;
    e.deliveryOrder = tx;
    e.receivedAt = at(k->origin, rx);
    e.ageLowMs = age; e.ageHighMs = age; e.ageComplete = true;
    e.useLimitMs = 2000u;
    e.relatedCommand = 90u;
    e.availability = VSS_AVAILABLE;
    return e;
}

static VssInputApplication submit(const VssInputEvidence *e, uint64_t now)
{
    VssTimeEvidence t = at(e->occurrence == NULL ? VSS_PRODUCT : e->occurrence->origin, now);
    return Input_Process(e, &t, NULL, 0u, NULL);
}

static void reset(void)
{
    VssTimeEvidence t = at(VSS_PRODUCT, 0u);
    Input_TestReset(); Store_TestReset();
    CHECK(Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &t));
    t.domain = VSS_TEST;
    CHECK(Input_OpenAfterSynchronization(VSS_TEST, 7u, &t));
}

static VssOccurrenceObservation read_record(const VssOccurrenceKey *k)
{
    VssOccurrenceObservation r = {0};
    CHECK(Store_ReadOccurrence(k, &r));
    return r;
}

static void normal_and_borrow_lifetime(void)
{
    VssOccurrenceKey k = key(1u), savedKey = k;
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssInputApplication a;
    VssOccurrenceObservation r;
    reset(); a = submit(&e, 1150u);
    CHECK(a.validation == VSS_VALIDATION_APPLICABLE && a.contextApplied);
    CHECK(a.storeAdmission == VSS_STORE_ACCEPTED && Store_Count() == 1u);
    memset(&e, 0xa5, sizeof e); memset(&k, 0xa5, sizeof k);
    r = read_record(&savedKey);
    CHECK(r.event.meta.original.earliest == 1000u && r.event.meta.original.latest == 1000u);
    CHECK(r.event.meta.useLimit == 500u && r.event.meta.validUntil == 1500u);
    CHECK(r.event.meta.relatedCommand == 90u && r.state == VSS_OCCURRENCE_PENDING);
    CHECK(r.replayAllowed && !r.firstStartFact.present && !r.finalFact.present);
}

static void duplicates_conflicts_and_order(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssOccurrenceObservation before, after;
    VssInputApplication a;
    reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    before = read_record(&k);
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_NO_CHANGE); /* same TX */
    e.relatedCommand++;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED); /* same TX conflict */
    e.relatedCommand--; e.deliveryOrder = 2u;
    e.receivedAt = at(VSS_PRODUCT, 1200u); e.ageLowMs = 200u; e.ageHighMs = 200u;
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_DUPLICATE);
    e.deliveryOrder = 1u; e.quality = VSS_INPUT_INVALID;
    CHECK(submit(&e, 1200u).validation == VSS_VALIDATION_REJECTED);
    e.quality = VSS_INPUT_VALID; e.deliveryOrder = 3u; e.signal = VSS_SIGNAL_WELCOME;
    a = submit(&e, 1200u);
    CHECK(a.contextApplied && a.storeAdmission == VSS_STORE_REJECTED);
    e.signal = VSS_SIGNAL_LOCK_COMPLETE; e.deliveryOrder = 4u; e.relatedCommand++;
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_REJECTED);
    e.relatedCommand--; e.deliveryOrder = 5u; e.ageLowMs = 199u; e.ageHighMs = 199u;
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_REJECTED); /* younger original */
    e.deliveryOrder = 6u; e.ageLowMs = 201u; e.ageHighMs = 201u;
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_REJECTED); /* inconsistent older */
    e.deliveryOrder = 7u; e.ageLowMs = 200u; e.ageHighMs = 200u; e.useLimitMs = 400u;
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_REJECTED);
    after = read_record(&k);
    CHECK(memcmp(&before, &after, sizeof before) == 0);
}

static void distinct_occurrences_same_signal(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    k.occurrenceId = 2u; e.deliveryOrder = 2u;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(Store_Count() == 2u);
}

/* Host PB double: arithmetic classification ONLY. No physical observation,
 * Store_ApplyPlaybackFact or production PB implementation is present. */
static int mock_first_actual(const VssInputMeta *m, const VssFactTime *actual,
                             bool physicalCoverage, bool authorityKnown, bool allowed)
{
    if (!physicalCoverage || !authorityKnown || actual->continuity != VSS_CONTINUITY_COMPARABLE ||
        actual->domain != m->original.domain || actual->epoch != m->original.epoch ||
        actual->earliest > actual->latest) { return 0; } /* uncertain */
    if (!allowed) { return -1; }
    if (actual->earliest >= m->original.latest &&
        actual->latest - m->original.earliest < m->useLimit) { return 1; }
    if (actual->earliest >= m->original.latest &&
        actual->earliest - m->original.latest >= m->useLimit) { return -1; }
    return 0;
}

static void first_start_499_500_501_and_actual_three_way(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1300u, 300u);
    VssOccurrenceObservation r, after;
    VssFactTime a;
    uint32_t elapsed;
    reset(); CHECK(submit(&e, 1300u).storeAdmission == VSS_STORE_ACCEPTED);
    r = read_record(&k);
    for (elapsed = 0u; elapsed <= 700u; ++elapsed) {
        a = at(VSS_PRODUCT, 1000u + elapsed);
        CHECK(Store_FirstStartAllowed(&k, &a) == (elapsed < 500u));
        CHECK(mock_first_actual(&r.event.meta, &a, true, true, true) == (elapsed < 500u ? 1 : -1));
    }
    a = at(VSS_PRODUCT, 1499u); a.latest = 1500u;
    CHECK(!Store_FirstStartAllowed(&k, &a));
    CHECK(mock_first_actual(&r.event.meta, &a, true, true, true) == 0);
    a = at(VSS_PRODUCT, 1499u);
    CHECK(mock_first_actual(&r.event.meta, &a, false, true, true) == 0);
    CHECK(mock_first_actual(&r.event.meta, &a, true, false, true) == 0);
    CHECK(mock_first_actual(&r.event.meta, &a, true, true, false) == -1);
    a.epoch = 2u; CHECK(mock_first_actual(&r.event.meta, &a, true, true, true) == 0);
    after = read_record(&k);
    CHECK(memcmp(&r, &after, sizeof r) == 0); /* no actual/final generated */
}

static void uncertain_original_and_age_once(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1300u, 300u);
    VssOccurrenceObservation r;
    VssTimeEvidence t;
    reset(); e.receivedAt.latest = 1302u; e.ageLowMs = 299u; e.ageHighMs = 301u;
    CHECK(submit(&e, 1400u).storeAdmission == VSS_STORE_ACCEPTED);
    r = read_record(&k);
    CHECK(r.event.meta.original.earliest == 999u && r.event.meta.original.latest == 1003u);
    CHECK(r.event.meta.validUntil == 1499u); /* RX->consume not subtracted twice */
    t = at(VSS_PRODUCT, 1498u); CHECK(Store_FirstStartAllowed(&k, &t));
    t.latest = 1499u; CHECK(!Store_FirstStartAllowed(&k, &t));
    t = at(VSS_PRODUCT, 1500u);
    CHECK(mock_first_actual(&r.event.meta, &t, true, true, true) == 0);
    t = at(VSS_PRODUCT, 1503u);
    CHECK(mock_first_actual(&r.event.meta, &t, true, true, true) == -1);
    t = at(VSS_PRODUCT, 1002u); CHECK(!Store_FirstStartAllowed(&k, &t));
}

static void zero_short_long_limit_and_tombstones(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssTimeEvidence t;
    VssOccurrenceObservation r;
    reset(); e.useLimitMs = 0u;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    r = read_record(&k); CHECK(r.rejectedTombstone && !r.replayAllowed);
    e.useLimitMs = 2000u; e.deliveryOrder = 2u;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_REJECTED);
    reset(); e.deliveryOrder = 1u; e.useLimitMs = 200u;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    r = read_record(&k); CHECK(r.event.meta.useLimit == 200u);
    t = at(VSS_PRODUCT, 1199u); CHECK(Store_FirstStartAllowed(&k, &t));
    t = at(VSS_PRODUCT, 1200u); CHECK(!Store_FirstStartAllowed(&k, &t));
    reset(); e.useLimitMs = UINT32_MAX;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(read_record(&k).event.meta.useLimit == 500u);
}

static void invalid_time_evidence_and_arithmetic(void)
{
    unsigned variant;
    for (variant = 0u; variant < 10u; ++variant) {
        VssOccurrenceKey k = key(1u);
        VssInputEvidence e = event(&k, 1u, 1100u, 100u);
        VssTimeEvidence t = at(VSS_PRODUCT, 1200u);
        reset();
        switch (variant) {
        case 0u: e.ageComplete = false; break;
        case 1u: e.ageHighMs = 65535u; break;
        case 2u: e.ageLowMs = 101u; break;
        case 3u: e.ageLowMs = 1200u; e.ageHighMs = 1200u; break;
        case 4u: e.receivedAt.epoch = 2u; break;
        case 5u: e.receivedAt.continuity = VSS_CONTINUITY_UNKNOWN; break;
        case 6u: e.receivedAt.latest = 1300u; break;
        case 7u: t.latest = t.earliest - 1u; break;
        case 8u: e.receivedAt = at(VSS_PRODUCT, UINT64_MAX - 10u);
                 e.ageLowMs = 0u; e.ageHighMs = 0u; t = e.receivedAt; break;
        default: t.continuity = VSS_CONTINUITY_UNKNOWN; break;
        }
        CHECK(Input_Process(&e, &t, NULL, 0u, NULL).storeAdmission == VSS_STORE_NOT_SUBMITTED);
        CHECK(!Store_FirstStartAllowed(&k, &t));
    }
}

static void boot_unknown_restart_and_test_isolation(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssTimeEvidence t = at(VSS_PRODUCT, 0u);
    VssOccurrenceObservation r;
    Input_TestReset(); Store_TestReset();
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED); /* first Boot insufficient */
    CHECK(Store_Count() == 0u);
    CHECK(!Input_OpenAfterSynchronization(VSS_PRODUCT, 0u, &t));
    CHECK(Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &t));
    CHECK(!Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &t));
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    k.generation = 0u; e.deliveryOrder = 2u;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    k.generation = 7u;
    r = read_record(&k); CHECK(r.replayAllowed);
    k.generation = 8u;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_QUALITY_CHANGE);
    k.generation = 7u; r = read_record(&k);
    CHECK(!r.replayAllowed && r.inputLossAt.continuity == VSS_CONTINUITY_UNKNOWN);
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    CHECK(!Input_OpenAfterSynchronization(VSS_PRODUCT, 8u, &t));
    /* Simulated VSS restart loses RAM ledger: first old Boot stays closed. */
    Input_TestReset(); Store_TestReset();
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    CHECK(Input_OpenAfterSynchronization(VSS_PRODUCT, 8u, &t)); /* explicit test proof */
    k.generation = 8u; CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    reset(); k = key(1u); e = event(&k, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    k.origin = VSS_TEST; e.receivedAt.domain = VSS_TEST;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(Store_Count() == 2u);
    e.quality = VSS_INPUT_INVALID; e.deliveryOrder = 2u; e.lossAt = at(VSS_TEST, 1110u);
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED);
    CHECK(!read_record(&k).replayAllowed);
    k.origin = VSS_PRODUCT; CHECK(read_record(&k).replayAllowed);
}

static void quality_loss_monotonic_and_existing_facts(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssOccurrenceObservation first, after;
    VssReservedPlaybackFact start = {0}, final = {0};
    start.present = true; start.kind = 1u; start.fact = 51u;
    start.occurrence = k; start.session = 31u; start.attempt = 41u;
    start.occurredAt = at(VSS_PRODUCT, 1050u); start.basisRevision = 61u;
    final = start; final.kind = 2u; final.fact = 52u;
    final.occurredAt = at(VSS_PRODUCT, 1080u); final.basisRevision = 62u;
    reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    e.quality = VSS_INPUT_INVALID; e.deliveryOrder = 2u; e.lossAt = at(VSS_PRODUCT, 1110u);
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED);
    first = read_record(&k);
    CHECK(!first.replayAllowed && first.state == VSS_OCCURRENCE_PENDING);
    e.deliveryOrder = 3u; e.lossAt = at(VSS_PRODUCT, 1120u);
    CHECK(submit(&e, 1120u).validation == VSS_VALIDATION_NO_CHANGE);
    after = read_record(&k); CHECK(memcmp(&first, &after, sizeof first) == 0);
    e.deliveryOrder = 4u; e.quality = VSS_INPUT_VALID;
    CHECK(submit(&e, 1120u).storeAdmission == VSS_STORE_DUPLICATE);
    CHECK(!read_record(&k).replayAllowed);
    e.deliveryOrder = 5u; e.quality = VSS_INPUT_STALE;
    CHECK(submit(&e, 1120u).storeAdmission == VSS_STORE_UPDATED);
    CHECK(read_record(&k).inputLossAt.earliest == 1110u);
    Store_TestSeedFacts(&k, VSS_OCCURRENCE_COMPLETED, &start, &final);
    first = read_record(&k);
    e.deliveryOrder = 6u; e.quality = VSS_INPUT_VALID;
    CHECK(submit(&e, 1120u).storeAdmission == VSS_STORE_DUPLICATE);
    after = read_record(&k); CHECK(memcmp(&first, &after, sizeof first) == 0);
    e.deliveryOrder = 7u; e.quality = VSS_INPUT_INVALID;
    CHECK(submit(&e, 1120u).storeAdmission == VSS_STORE_UPDATED);
    after = read_record(&k); CHECK(memcmp(&first, &after, sizeof first) == 0);
}

static void no_rx_clock_reset_wrap_and_unknown(void)
{
    unsigned variant;
    for (variant = 0u; variant < 4u; ++variant) {
        VssOccurrenceKey k = key(1u);
        VssInputEvidence e = event(&k, 1u, 1100u, 100u);
        VssTimeEvidence t = at(VSS_PRODUCT, 2000u);
        VssOccurrenceObservation r;
        reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        CHECK(Input_Process(NULL, &t, NULL, 0u, NULL).validation == VSS_VALIDATION_NO_CHANGE);
        r = read_record(&k); CHECK(r.replayAllowed && r.state == VSS_OCCURRENCE_PENDING);
        CHECK(!Store_FirstStartAllowed(&k, &t)); /* expiry is opportunity only */
        if (variant == 0u) { t.epoch = 2u; }
        if (variant == 1u) { t.continuity = VSS_CONTINUITY_LOST; }
        if (variant == 2u) { t = at(VSS_PRODUCT, 1u); }
        if (variant == 3u) { t.continuity = VSS_CONTINUITY_UNKNOWN; }
        (void)Input_Process(NULL, &t, NULL, 0u, NULL);
        r = read_record(&k);
        CHECK(r.replayAllowed == (variant == 3u));
        CHECK(!Store_FirstStartAllowed(&k, &t));
        CHECK(r.state == VSS_OCCURRENCE_PENDING && !r.firstStartFact.present && !r.finalFact.present);
    }
}

static void ledger_full_preserves_all_and_context(void)
{
    VssOccurrenceObservation before[VSS_LEDGER_CAPACITY];
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssInputApplication a;
    size_t i;
    reset();
    for (i = 0u; i < VSS_LEDGER_CAPACITY; ++i) {
        k.occurrenceId = (uint32_t)i + 1u; e.deliveryOrder = (uint32_t)i + 1u;
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        before[i] = read_record(&k);
    }
    k.occurrenceId = 129u; e.deliveryOrder = 129u;
    a = submit(&e, 1100u);
    CHECK(a.contextApplied && a.validation == VSS_VALIDATION_APPLICABLE && a.storeAdmission == VSS_STORE_REJECTED);
    CHECK(Store_Count() == VSS_LEDGER_CAPACITY);
    e.deliveryOrder = 128u; e.quality = VSS_INPUT_INVALID;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED); /* no rollback */
    for (i = 0u; i < VSS_LEDGER_CAPACITY; ++i) {
        VssOccurrenceObservation r;
        k.occurrenceId = (uint32_t)i + 1u; r = read_record(&k);
        CHECK(memcmp(&before[i], &r, sizeof r) == 0);
    }
    k.occurrenceId = 1u; e.quality = VSS_INPUT_VALID; e.deliveryOrder = 130u;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_DUPLICATE);
    e.quality = VSS_INPUT_INVALID; e.deliveryOrder = 131u; e.lossAt = at(VSS_PRODUCT, 1110u);
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED); /* no new slot */
    for (i = 0u; i < VSS_LEDGER_CAPACITY; ++i) {
        k.occurrenceId = (uint32_t)i + 1u; CHECK(!read_record(&k).replayAllowed);
    }
}

static void invalid_arguments_diagnostic_capacity_and_protection(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    VssTimeEvidence t = at(VSS_PRODUCT, 1100u);
    VssDiagnosticEvidence out[VSS_DIAGNOSTIC_CAPACITY];
    VssDiagnosticObservation d;
    VssOccurrenceObservation r;
    VssQualityChange q = {0};
    VssStateUpdate invalidState = {0};
    size_t written = 77u;
    unsigned i;
    reset();
    CHECK(!Input_OpenAfterSynchronization((VssOrigin)99, 7u, &t));
    CHECK(!Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, NULL));
    CHECK(!Input_ReadDiagnostics(NULL));
    CHECK(!Store_ReadOccurrence(NULL, &r) && !Store_ReadOccurrence(&k, NULL));
    CHECK(!Store_FirstStartAllowed(NULL, &t) && !Store_FirstStartAllowed(&k, NULL));
    CHECK(Store_ApplyInput(NULL, NULL, NULL) == VSS_STORE_REJECTED);
    CHECK(Store_ApplyInput(NULL, NULL, &q) == VSS_STORE_REJECTED);
    CHECK(Input_Process(&e, &t, NULL, 1u, &written).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(written == 77u); /* invalid OUT does not block safe input */
    r = read_record(&k); q.basis = r.event.meta; q.signal = e.signal; q.quality = VSS_INPUT_INVALID;
    CHECK(Store_ApplyInput(&r.event, NULL, &q) == VSS_STORE_REJECTED);
    CHECK(Store_ApplyInput(NULL, &invalidState, NULL) == VSS_STORE_REJECTED); /* STORE reads the typed state */
    r.event.meta.useLimit = 0u;
    CHECK(Store_ApplyInput(&r.event, NULL, NULL) == VSS_STORE_REJECTED);
    e.occurrence = NULL;
    CHECK(Input_Process(&e, &t, NULL, 0u, &written).validation == VSS_VALIDATION_REJECTED);
    CHECK(written == 0u);
    written = 77u;
    (void)Input_Process(NULL, NULL, out, VSS_DIAGNOSTIC_CAPACITY + 1u, &written);
    CHECK(written == 77u);
    e.occurrence = &k; e.value = VSS_VALUE_UNSUPPORTED;
    for (i = 0u; i < 20u; ++i) {
        k.occurrenceId = 100u + i; e.deliveryOrder = 100u + i;
        (void)Input_Process(&e, &t, NULL, 0u, NULL);
    }
    CHECK(Input_ReadDiagnostics(&d));
    CHECK(d.pending == 16u && d.lossLatched && d.lossCount == 6u);
    CHECK(d.summaryPresent[2u] && d.firstLost[2u].key.occurrenceId == 114u);
    CHECK(d.latestLost[2u].key.occurrenceId == 119u);
    memset(&e, 0, sizeof e); memset(&k, 0, sizeof k);
    (void)Input_Process(NULL, &t, out, 16u, &written);
    CHECK(written == 16u && out[0].code == VSS_DIAG_ARGUMENT);
    CHECK(out[2].key.occurrenceId == 100u && out[2].receivedUseLimit == 2000u && out[2].useLimit == 500u);
    CHECK(Input_ReadDiagnostics(&d) && d.pending == 0u && d.lossCount == 6u);
    CHECK(out[15].key.occurrenceId == 113u);
}

static void tx_wrap_and_stale_retransmission(void)
{
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, UINT32_MAX, 1100u, 100u);
    VssOccurrenceObservation before, after;
    reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    before = read_record(&k);
    e.deliveryOrder = 1u;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    after = read_record(&k); CHECK(memcmp(&before, &after, sizeof before) == 0);
    reset(); e.deliveryOrder = 1u;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    before = read_record(&k);
    e.deliveryOrder = 2u; e.receivedAt = at(VSS_PRODUCT, 1700u);
    e.ageLowMs = 700u; e.ageHighMs = 700u;
    CHECK(submit(&e, 1700u).validation == VSS_VALIDATION_REJECTED);
    after = read_record(&k); CHECK(memcmp(&before, &after, sizeof before) == 0);
}

static void all_signals_input_boundaries_and_revision(void)
{
    unsigned i;
    VssInputContextObservation before, after;
    VssOccurrenceKey k = key(1u);
    VssInputEvidence e = event(&k, 1u, 1100u, 100u);
    reset();
    CHECK(!Input_ReadContext(VSS_PRODUCT, VSS_SIGNAL_WELCOME, &before));
    CHECK(!Input_ReadContext((VssOrigin)99, VSS_SIGNAL_WELCOME, &before));
    CHECK(!Input_ReadContext(VSS_PRODUCT, (VssInputSignal)99, &before));
    CHECK(!Input_ReadContext(VSS_PRODUCT, VSS_SIGNAL_WELCOME, NULL));
    for (i = 0u; i < 5u; ++i) {
        e.signal = (VssInputSignal)i; e.deliveryOrder = i + 1u; k.occurrenceId = i + 1u;
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        CHECK(Input_ReadContext(VSS_PRODUCT, e.signal, &before));
        CHECK(before.revision == 1u && before.quality == VSS_INPUT_VALID);
    }
    e.deliveryOrder = 6u; e.quality = VSS_INPUT_INVALID; e.lossAt = at(VSS_PRODUCT, 1110u);
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED);
    CHECK(Input_ReadContext(VSS_PRODUCT, e.signal, &after));
    CHECK(after.revision == 2u && after.quality == VSS_INPUT_INVALID);
    CHECK(after.appliedMeta.original.earliest == before.appliedMeta.original.earliest);
    e.deliveryOrder = 4u;
    CHECK(submit(&e, 1110u).validation == VSS_VALIDATION_REJECTED);
    CHECK(Input_ReadContext(VSS_PRODUCT, e.signal, &before));
    CHECK(before.revision == after.revision && before.quality == after.quality);
    for (i = 499u; i <= 501u; ++i) {
        reset(); k = key(1u); e = event(&k, 1u, 1000u + i, i);
        if (i == 499u) { CHECK(submit(&e, 1000u + i).storeAdmission == VSS_STORE_ACCEPTED); }
        else {
            CHECK(submit(&e, 1000u + i).validation == VSS_VALIDATION_REJECTED);
            CHECK(read_record(&k).rejectedTombstone);
        }
    }
    /* Independent latest explicit quality evidence still works with bad age;
     * old INVALID and normal EVENT never take this exception. */
    reset(); e = event(&k, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    e.deliveryOrder = 2u; e.quality = VSS_INPUT_INVALID; e.ageComplete = false;
    e.lossAt.continuity = VSS_CONTINUITY_UNKNOWN;
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED);
    CHECK(!read_record(&k).replayAllowed);
    CHECK(read_record(&k).inputLossAt.continuity == VSS_CONTINUITY_UNKNOWN);
}

static void initial_invalid_preserves_transport_order(void)
{
    VssOccurrenceKey k = key(123u);
    VssInputEvidence e = event(&k, 2u, 1100u, 50u);
    VssInputContextObservation context;
    VssInputApplication r;
    reset(); e.signal = VSS_SIGNAL_WELCOME;
    e.quality = VSS_INPUT_INVALID; e.availability = VSS_UNAVAILABLE;
    e.lossAt = at(VSS_PRODUCT, 1100u);
    r = submit(&e, 1100u);
    CHECK(r.validation == VSS_VALIDATION_NO_CHANGE && !r.contextApplied);
    CHECK(r.storeAdmission == VSS_STORE_NOT_SUBMITTED && Store_Count() == 0u);
    CHECK(!Input_ReadContext(VSS_PRODUCT, e.signal, &context));
    CHECK(submit(&e, 1150u).validation == VSS_VALIDATION_NO_CHANGE);
    k.occurrenceId = 124u; /* caller mutation must not change the saved delivery */
    CHECK(submit(&e, 1150u).validation == VSS_VALIDATION_REJECTED);
    k.occurrenceId = 123u;
    CHECK(submit(&e, 1150u).validation == VSS_VALIDATION_NO_CHANGE);
    e.quality = VSS_INPUT_VALID; e.availability = VSS_AVAILABLE;
    CHECK(submit(&e, 1150u).validation == VSS_VALIDATION_REJECTED); /* same TX conflict */
    e.deliveryOrder = 1u; e.receivedAt = at(VSS_PRODUCT, 1150u);
    r = submit(&e, 1150u);
    CHECK(r.validation == VSS_VALIDATION_REJECTED && !r.contextApplied);
    CHECK(r.storeAdmission == VSS_STORE_NOT_SUBMITTED && Store_Count() == 0u);
    e.deliveryOrder = 3u; k.occurrenceId = 125u;
    CHECK(submit(&e, 1150u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(Store_Count() == 1u);
}

static void semantic_rejection_preserves_transport_order(void)
{
    unsigned variant;
    for (variant = 0u; variant < 5u; ++variant) {
        VssOccurrenceKey k = key(100u), rejectedKey = k;
        VssInputEvidence e = event(&k, 2u, 1100u, 100u);
        VssOccurrenceObservation before, after;
        VssInputContextObservation context;
        VssInputApplication r;
        uint64_t now = 1100u;
        reset();
        if (variant == 0u) { e.ageComplete = false; }
        if (variant == 1u) { e.useLimitMs = 0u; }
        if (variant == 2u) { e.ageHighMs = 65535u; }
        if (variant == 3u) { e = event(&k, 2u, 1500u, 500u); now = 1500u; }
        if (variant == 4u) { e.value = VSS_VALUE_UNSUPPORTED; }
        r = submit(&e, now);
        CHECK(r.validation == VSS_VALIDATION_REJECTED && !r.contextApplied);
        CHECK(r.storeAdmission == VSS_STORE_NOT_SUBMITTED && Store_Count() == 1u);
        CHECK(!Input_ReadContext(VSS_PRODUCT, e.signal, &context));
        before = read_record(&rejectedKey); CHECK(before.rejectedTombstone);
        CHECK(submit(&e, now).validation == VSS_VALIDATION_NO_CHANGE);
        e.relatedCommand++;
        CHECK(submit(&e, now).validation == VSS_VALIDATION_REJECTED); /* same TX conflict */
        k.occurrenceId = 200u; e = event(&k, 1u, now + 50u, 100u);
        r = submit(&e, now + 50u);
        CHECK(r.validation == VSS_VALIDATION_REJECTED && !r.contextApplied);
        CHECK(r.storeAdmission == VSS_STORE_NOT_SUBMITTED && Store_Count() == 1u);
        k.occurrenceId = 201u; e.deliveryOrder = 3u;
        CHECK(submit(&e, now + 50u).storeAdmission == VSS_STORE_ACCEPTED);
        after = read_record(&rejectedKey);
        CHECK(memcmp(&before, &after, sizeof before) == 0);
        k = rejectedKey; e.deliveryOrder = 4u;
        CHECK(submit(&e, now + 50u).storeAdmission == VSS_STORE_REJECTED);
        CHECK(Store_Count() == 2u); /* original tombstone is never reaccepted */
    }
}

static void untrusted_delivery_cannot_advance_order(void)
{
    unsigned variant;
    for (variant = 0u; variant < 21u; ++variant) {
        VssOccurrenceKey k = key(100u);
        VssInputEvidence e = event(&k, 100u, 1100u, 100u);
        VssTimeEvidence now = at(VSS_PRODUCT, 1100u);
        VssInputApplication r;
        reset();
        switch (variant) {
        case 0u: k.source = VSS_SOURCE_OTHER; break;
        case 1u: k.generation = 0u; break;
        case 2u: k.origin = (VssOrigin)99; break;
        case 3u: k.origin = VSS_TEST; break;
        case 4u: e.signal = (VssInputSignal)99; break;
        case 5u: e.quality = (VssInputQuality)99; break;
        case 6u: e.availability = (VssAvailability)99; break;
        case 7u: e.value = (VssInputValue)99; break;
        case 8u: e.deliveryOrder = 0u; break;
        case 9u: k.occurrenceId = 0u; break;
        case 10u: e.occurrence = NULL; break;
        case 11u: e.receivedAt.domain = VSS_TEST; break;
        case 12u: e.receivedAt.epoch = 2u; break;
        case 13u: e.receivedAt.epoch = 0u; break;
        case 14u: e.receivedAt.continuity = VSS_CONTINUITY_UNKNOWN; break;
        case 15u: e.receivedAt.continuity = VSS_CONTINUITY_LOST; break;
        case 16u: e.receivedAt.earliest = 1200u; break;
        case 17u: e.receivedAt = at(VSS_PRODUCT, 1200u); break;
        case 18u: now.continuity = VSS_CONTINUITY_UNKNOWN; break;
        case 19u: now.earliest = 1200u; break;
        default: now.domain = (VssOrigin)99; break;
        }
        r = Input_Process(&e, &now, NULL, 0u, NULL);
        CHECK(r.validation == VSS_VALIDATION_REJECTED && !r.contextApplied);
        CHECK(r.storeAdmission == VSS_STORE_NOT_SUBMITTED && Store_Count() == 0u);
        /* The unverified TX100 must not poison the valid TX1 high-water. */
        k = key(1u); e = event(&k, 1u, 1100u, 100u);
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        CHECK(Store_Count() == 1u);
    }
}

static void generation_clock_fences_and_origin_isolation(void)
{
    unsigned variant;
    for (variant = 0u; variant < 3u; ++variant) {
        VssOccurrenceKey product = key(1u), test = key(1u), bad = key(2u);
        VssInputEvidence e = event(&product, 1u, 1100u, 100u);
        VssOccurrenceObservation pBefore, pAfter, tBefore, tAfter;
        VssTimeEvidence now = at(VSS_PRODUCT, 1150u);
        reset(); test.origin = VSS_TEST;
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        e = event(&test, 1u, 1100u, 100u);
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
        pBefore = read_record(&product); tBefore = read_record(&test);
        e = event(&bad, 100u, 1150u, 100u);
        if (variant == 0u) { bad.generation = 8u; }
        if (variant == 1u) { now.epoch = 2u; }
        if (variant == 2u) { now.continuity = VSS_CONTINUITY_LOST; }
        CHECK(Input_Process(&e, &now, NULL, 0u, NULL).storeAdmission != VSS_STORE_ACCEPTED);
        CHECK(Store_Count() == 2u);
        pAfter = read_record(&product); tAfter = read_record(&test);
        CHECK(!pAfter.replayAllowed && pAfter.inputLossPresent);
        CHECK(pAfter.state == pBefore.state && !pAfter.firstStartFact.present && !pAfter.finalFact.present);
        CHECK(pAfter.event.meta.original.earliest == pBefore.event.meta.original.earliest);
        CHECK(pAfter.event.meta.validUntil == pBefore.event.meta.validUntil);
        CHECK(memcmp(&tBefore, &tAfter, sizeof tBefore) == 0);
        bad = key(2u); e = event(&bad, 2u, 1200u, 100u);
        CHECK(submit(&e, 1200u).validation == VSS_VALIDATION_REJECTED);
        CHECK(!Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &now));
        test.occurrenceId = 2u; e = event(&test, 2u, 1200u, 100u);
        CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_ACCEPTED);
    }
    /* A closed bootstrap does not consume untrusted TX100 before proof. */
    {
        VssOccurrenceKey k = key(1u);
        VssInputEvidence e = event(&k, 100u, 1100u, 100u);
        VssTimeEvidence sync = at(VSS_PRODUCT, 1100u);
        Input_TestReset(); Store_TestReset();
        CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
        CHECK(Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &sync));
        e.deliveryOrder = 1u;
        CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    }
}

static void rejected_delivery_preserves_accepted_facts(void)
{
    VssOccurrenceKey accepted = key(1u), rejected = key(2u), old = key(3u);
    VssInputEvidence e = event(&accepted, 1u, 1100u, 100u);
    VssOccurrenceObservation before, after;
    VssInputContextObservation contextBefore = {0}, contextAfter = {0};
    VssReservedPlaybackFact first = {0}, final = {0};
    reset(); CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    e.quality = VSS_INPUT_INVALID; e.deliveryOrder = 2u; e.lossAt = at(VSS_PRODUCT, 1110u);
    CHECK(submit(&e, 1110u).storeAdmission == VSS_STORE_UPDATED);
    first.present = true; first.kind = 1u; first.fact = 10u; first.occurrence = accepted;
    first.session = 20u; first.attempt = 30u; first.occurredAt = at(VSS_PRODUCT, 1050u);
    first.basisRevision = 40u;
    final = first; final.kind = 2u; final.fact = 11u;
    final.occurredAt = at(VSS_PRODUCT, 1080u); final.basisRevision = 41u;
    Store_TestSeedFacts(&accepted, VSS_OCCURRENCE_COMPLETED, &first, &final);
    before = read_record(&accepted);
    CHECK(Input_ReadContext(VSS_PRODUCT, e.signal, &contextBefore));
    e = event(&rejected, 4u, 1120u, 100u); e.ageComplete = false;
    CHECK(submit(&e, 1120u).validation == VSS_VALIDATION_REJECTED);
    CHECK(Input_ReadContext(VSS_PRODUCT, e.signal, &contextAfter));
    CHECK(memcmp(&contextBefore, &contextAfter, sizeof contextBefore) == 0);
    e = event(&old, 3u, 1130u, 100u);
    CHECK(submit(&e, 1130u).storeAdmission == VSS_STORE_NOT_SUBMITTED);
    CHECK(Store_Count() == 2u);
    e = event(&accepted, 5u, 1100u, 100u);
    CHECK(submit(&e, 1130u).storeAdmission == VSS_STORE_DUPLICATE);
    after = read_record(&accepted);
    CHECK(memcmp(&before, &after, sizeof before) == 0);
    CHECK(!after.replayAllowed && after.inputLossAt.earliest == 1110u);
}

static void rejected_max_tx_is_nonwrapping_and_isolated(void)
{
    VssOccurrenceKey k = key(1u), newer = key(2u);
    VssInputEvidence e = event(&k, UINT32_MAX, 1100u, 100u);
    reset(); e.ageComplete = false;
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_NO_CHANGE);
    e = event(&newer, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).validation == VSS_VALIDATION_REJECTED);
    CHECK(Store_Count() == 1u && read_record(&k).rejectedTombstone);
    newer.origin = VSS_TEST; e = event(&newer, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(Store_Count() == 2u);
}

/* A DOMAIN restart: the old generation is closed (its pending events never
 * replay), the new one is accepted from its first TX, and a late delivery of
 * the replaced boot is refused without closing the new generation. */
static void new_domain_generation_resumes_events(void)
{
    VssOccurrenceKey old = key(1u), fresh = key(1u), other = key(1u);
    VssInputEvidence e = event(&old, 40u, 1100u, 100u);
    VssTimeEvidence t = at(VSS_PRODUCT, 1200u);
    VssOccurrenceObservation r, before;
    reset(); other.origin = VSS_TEST;
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    e = event(&other, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 7u, &t)); /* current boot */
    CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 0u, &t));
    CHECK(Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    r = read_record(&old);
    CHECK(!r.replayAllowed && r.inputLossPresent && r.state == VSS_OCCURRENCE_PENDING);
    CHECK(read_record(&other).replayAllowed); /* the other origin is untouched */
    fresh.generation = 8u; e = event(&fresh, 1u, 1200u, 0u);
    CHECK(submit(&e, 1200u).storeAdmission == VSS_STORE_ACCEPTED);
    before = read_record(&old);
    e = event(&old, 41u, 1210u, 110u);
    CHECK(submit(&e, 1210u).validation == VSS_VALIDATION_REJECTED);
    r = read_record(&old); CHECK(memcmp(&before, &r, sizeof r) == 0);
    fresh.occurrenceId = 2u; e = event(&fresh, 2u, 1220u, 0u);
    CHECK(submit(&e, 1220u).storeAdmission == VSS_STORE_ACCEPTED);
    t = at(VSS_PRODUCT, 1230u);
    CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 7u, &t)); /* the replaced boot */
    CHECK(Input_AcceptNewGeneration(VSS_PRODUCT, 9u, &t));
    CHECK(!read_record(&fresh).replayAllowed && Store_Count() == 4u);
    fresh.generation = 9u; fresh.occurrenceId = 1u; e = event(&fresh, 1u, 1230u, 0u);
    CHECK(submit(&e, 1230u).storeAdmission == VSS_STORE_ACCEPTED);
    /* Refused before the first opening, on bad or discontinuous time. */
    Input_TestReset(); Store_TestReset();
    CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    reset(); old = key(1u); e = event(&old, 1u, 1100u, 100u);
    CHECK(submit(&e, 1100u).storeAdmission == VSS_STORE_ACCEPTED);
    t = at(VSS_PRODUCT, 1200u); t.continuity = VSS_CONTINUITY_UNKNOWN;
    CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    t = at(VSS_TEST, 1200u); CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    t = at(VSS_PRODUCT, 1200u); t.epoch = 2u; CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    t = at(VSS_PRODUCT, 1000u); CHECK(!Input_AcceptNewGeneration(VSS_PRODUCT, 8u, &t));
    CHECK(!Input_AcceptNewGeneration((VssOrigin)99, 8u, &t) && !Input_AcceptNewGeneration(VSS_PRODUCT, 8u, NULL));
    CHECK(read_record(&old).replayAllowed); /* refusals change nothing */
}

int main(void)
{
    RUN(normal_and_borrow_lifetime);
    RUN(duplicates_conflicts_and_order);
    RUN(distinct_occurrences_same_signal);
    RUN(first_start_499_500_501_and_actual_three_way);
    RUN(uncertain_original_and_age_once);
    RUN(zero_short_long_limit_and_tombstones);
    RUN(invalid_time_evidence_and_arithmetic);
    RUN(boot_unknown_restart_and_test_isolation);
    RUN(quality_loss_monotonic_and_existing_facts);
    RUN(no_rx_clock_reset_wrap_and_unknown);
    RUN(ledger_full_preserves_all_and_context);
    RUN(invalid_arguments_diagnostic_capacity_and_protection);
    RUN(tx_wrap_and_stale_retransmission);
    RUN(all_signals_input_boundaries_and_revision);
    RUN(initial_invalid_preserves_transport_order);
    RUN(semantic_rejection_preserves_transport_order);
    RUN(untrusted_delivery_cannot_advance_order);
    RUN(generation_clock_fences_and_origin_isolation);
    RUN(rejected_delivery_preserves_accepted_facts);
    RUN(rejected_max_tx_is_nonwrapping_and_isolated);
    RUN(new_domain_generation_resumes_events);
    printf("RESULT passed=%u failed=0\n", checks);
    return 0;
}
