#include "../../src/service/policy/vss_policy_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
/* The first failure ends the program: later checks read state the failed
 * one guarded. The RESULT line is what host_test.py collects. */
#define CHECK(x) do { ++checks; if (!(x)) { \
    printf("policy/store FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    printf("RESULT passed=%u failed=1\n", checks - 1u); exit(1); } } while (0)
static VssTimeEvidence at(uint64_t n)
{
    VssTimeEvidence t = { VSS_TEST, 1u, 0u, 0u, VSS_CONTINUITY_COMPARABLE };
    t.earliest = n; t.latest = n; return t;
}
static void reset(void) { Store_TestReset(); StorePlayback_TestReset(); }
static VssOccurrenceKey add_event(uint32_t id, VssInputSignal signal, uint64_t original)
{
    VssOneShotEvent e;
    memset(&e, 0, sizeof e);
    e.signal = signal; e.meta.key.origin = VSS_TEST; e.meta.key.source = VSS_SOURCE_DOMAIN;
    e.meta.key.generation = 7u; e.meta.key.occurrenceId = id;
    e.meta.deliveryOrder = id; e.meta.original = at(original);
    e.meta.receivedUseLimit = 500u; e.meta.useLimit = 500u; e.meta.validUntil = original + 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK(Store_ApplyInput(&e, NULL, NULL) == VSS_STORE_ACCEPTED);
    return e.meta.key;
}
static VssPlanDescriptor plan(VssInputSignal signal, VssInputValue value, uint64_t key)
{
    VssPlanDescriptor p;
    memset(&p, 0, sizeof p); p.key = key; p.signal = signal; p.value = value;
    p.image = 1u; p.defined = true; p.available = true; p.cueCount = 1u;
    p.cues[0].asset = (uint32_t)((key - 1u) % 8u); p.repeatCount = 1u; return p;
}
static VssReadBasis basis(uint64_t now)
{
    VssReadBasis b;
    memset(&b, 0, sizeof b); b.origin = VSS_TEST; b.time = at(now);
    b.validity = VSS_READ_VALID; b.storeRevision = Store_ReadRevision(VSS_TEST);
    b.pbRevision = 1u; b.startsAllowed = true; return b;
}
static VssPlaybackFact fact(VssOccurrenceKey key, uint64_t id, uint64_t session,
                           uint64_t attempt, VssPlaybackFactKind kind, uint64_t when)
{
    VssPlaybackFact f;
    memset(&f, 0, sizeof f); f.present = true; f.fact = id; f.occurrence = key;
    f.session = session; f.attempt = attempt; f.kind = (uint32_t)kind;
    f.occurredAt = at(when); f.basisRevision = 1u; return f;
}
static VssStorePlaybackSnapshot snapshot(void)
{
    VssStorePlaybackSnapshot p;
    memset(&p, 0, sizeof p); p.origin = VSS_TEST; p.complete = true;
    p.revision = 1u; p.storeRevision = Store_ReadRevision(VSS_TEST); return p;
}
static void ranks_and_plans(void)
{
    static const VssInputSignal signals[9] = { VSS_SIGNAL_ANTI_PINCH, VSS_SIGNAL_REAR_RISK,
        VSS_SIGNAL_OCCUPANT_HAZARD, VSS_SIGNAL_REAR_RISK, VSS_SIGNAL_LOCK_ERROR,
        VSS_SIGNAL_UNLOCK_COMPLETE, VSS_SIGNAL_LOCK_COMPLETE, VSS_SIGNAL_GOODBYE, VSS_SIGNAL_WELCOME };
    static const VssInputValue values[9] = { VSS_VALUE_ACTIVE, VSS_VALUE_EMERGENCY,
        VSS_VALUE_ACTIVE, VSS_VALUE_CAUTION, VSS_VALUE_EVENT, VSS_VALUE_EVENT,
        VSS_VALUE_EVENT, VSS_VALUE_EVENT, VSS_VALUE_EVENT };
    VssCandidateObservation c[9]; VssPlanDescriptor plans[9];
    VssPolicyCurrent current; VssReadBasis b; VssSelectionDecision d;
    size_t i, j;
    reset(); memset(&current, 0, sizeof current); memset(c, 0, sizeof c); b = basis(100u);
    for (i = 0u; i < 9u; ++i) {
        c[i].origin = VSS_TEST; c[i].mode = i < 4u ? VSS_MODE_STATEFUL : VSS_MODE_EVENT;
        c[i].signal = signals[i]; c[i].value = values[i]; c[i].revision = 1u;
        c[i].newSessionAllowed = true; c[i].validity = VSS_CANDIDATE_APPLICABLE;
        plans[i] = plan(signals[i], values[i], (uint64_t)i + 1u);
        CHECK(VssPolicy_Priority(signals[i], values[i]) == 9u - (unsigned)i);
    }
    for (i = 0u; i < 9u; ++i) {
        d = Select_Choose(c + i, 9u - i, &current, plans, 9u, &b);
        CHECK(d.action == VSS_SELECTION_CHOOSE); CHECK(d.candidate.signal == signals[i]);
        for (j = i + 1u; j < 9u; ++j) { CHECK(VssPolicy_Priority(d.candidate.signal, d.candidate.value) > VssPolicy_Priority(c[j].signal, c[j].value)); }
    }
    plans[0].defined = false;
    CHECK(Select_Choose(c, 9u, &current, plans, 9u, &b).action == VSS_SELECTION_WAIT);
    plans[0].defined = true; plans[0].available = false;
    d = Select_Choose(c, 9u, &current, plans, 9u, &b);
    CHECK(d.action == VSS_SELECTION_WAIT); CHECK(d.candidate.signal == VSS_SIGNAL_ANTI_PINCH);
    plans[0].available = true; b.startsAllowed = false;
    CHECK(Select_Choose(c, 9u, &current, plans, 9u, &b).action == VSS_SELECTION_WAIT);
    b.startsAllowed = true; b.validity = VSS_READ_CHANGED;
    CHECK(Select_Choose(c, 9u, &current, plans, 9u, &b).action == VSS_SELECTION_RECOLLECT);
    CHECK(VssPolicy_Priority(VSS_SIGNAL_REAR_ACTIVATION, VSS_VALUE_ENABLED) == 0u);
    plans[0].cueCount = 5u; CHECK(!VssPolicy_ValidPlan(&plans[0]));
    plans[0].cueCount = 1u; plans[0].repeatCount = 0u; CHECK(!VssPolicy_ValidPlan(&plans[0]));
}
static void occurrence_order_and_keep(void)
{
    VssCandidateObservation c[4], swap; VssPlanDescriptor p;
    VssPolicyCurrent current; VssReadBasis b; VssSelectionDecision d;
    VssTimeEvidence t = at(100u); size_t written;
    reset(); (void)add_event(2u, VSS_SIGNAL_WELCOME, 20u); (void)add_event(1u, VSS_SIGNAL_WELCOME, 10u);
    CHECK(Store_ReadCandidates(&t, VSS_TEST, c, 4u, &written)); CHECK(written == 2u);
    p = plan(VSS_SIGNAL_WELCOME, VSS_VALUE_EVENT, 1u); memset(&current, 0, sizeof current); b = basis(100u);
    d = Select_Choose(c, written, &current, &p, 1u, &b); CHECK(d.candidate.key.occurrenceId == 1u);
    swap = c[0]; c[0] = c[1]; c[1] = swap;
    d = Select_Choose(c, written, &current, &p, 1u, &b); CHECK(d.candidate.key.occurrenceId == 1u);
    c[1].eventMeta.original = c[0].eventMeta.original;
    CHECK(Select_Choose(c, written, &current, &p, 1u, &b).candidate.key.occurrenceId == 1u);
    /* Overlapping intervals: the same DOMAIN run orders them by occurrence
     * id; different runs have no proved order. */
    c[1].eventMeta.original.earliest = 5u; c[1].eventMeta.original.latest = 15u;
    CHECK(Select_Choose(c, written, &current, &p, 1u, &b).candidate.key.occurrenceId == 1u);
    c[1].key.generation = 8u;
    CHECK(Select_Choose(c, written, &current, &p, 1u, &b).action == VSS_SELECTION_RECOLLECT);
    c[1].key.generation = 7u;
    current.present = true; current.valid = true; current.session = 1u; current.candidate = c[0]; current.plan = p;
    CHECK(Select_Choose(NULL, 0u, &current, &p, 1u, &b).action == VSS_SELECTION_KEEP);
    current.stopping = true;
    CHECK(Select_Choose(NULL, 0u, &current, &p, 1u, &b).action == VSS_SELECTION_WAIT);
    current.stopping = false; current.plan.defined = false;
    CHECK(Select_Choose(NULL, 0u, &current, &p, 1u, &b).action == VSS_SELECTION_RECOLLECT);
    CHECK(!Store_ReadCandidates(&t, VSS_TEST, c, 1u, &written)); CHECK(written == 1u);
    b.storeRevision = 9999u;
    CHECK(Select_Choose(NULL, 0u, &current, &p, 1u, &b).action == VSS_SELECTION_RECOLLECT);
}
static void fact_identity_and_lifetime(void)
{
    VssOccurrenceKey k; VssOccurrenceObservation r; VssPlaybackFact f, changed, last;
    reset(); k = add_event(1u, VSS_SIGNAL_LOCK_COMPLETE, 0u);
    f = fact(k, 1u, 10u, 20u, VSS_FACT_FIRST_ACTUAL_START, 499u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_NOT_APPLIED);
    CHECK(Store_BindPlayback(&k, 10u, 20u, 1u));
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_NOT_APPLIED);
    CHECK(Store_MarkPlaybackAttempted(&k, 10u, 20u));
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_APPLIED);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_ALREADY_APPLIED);
    changed = f; changed.occurredAt.latest = 500u;
    CHECK(Store_ApplyPlaybackFact(&changed) == VSS_FACT_NOT_APPLIED);
    changed = f; changed.fact = 2u;
    CHECK(Store_ApplyPlaybackFact(&changed) == VSS_FACT_NOT_APPLIED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.state == VSS_OCCURRENCE_STARTED);
    CHECK(Store_MarkPlaybackRetired(&k, 10u, 20u, false));
    CHECK(!Store_BindPlayback(&k, 11u, 21u, r.revision));
    CHECK(Store_BindPlayback(&k, 10u, 21u, r.revision));
    CHECK(Store_MarkPlaybackAttempted(&k, 10u, 21u));
    last = fact(k, 2u, 10u, 21u, VSS_FACT_WHOLE_COMPLETED, 800u);
    CHECK(Store_ApplyPlaybackFact(&last) == VSS_FACT_APPLIED);
    CHECK(Store_ApplyPlaybackFact(&last) == VSS_FACT_ALREADY_APPLIED);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_ALREADY_APPLIED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.firstStartFact.fact == 1u); CHECK(r.finalFact.fact == 2u);
    CHECK(r.state == VSS_OCCURRENCE_COMPLETED); CHECK(!r.replayAllowed);
    changed = f; changed.fact = 3u; CHECK(Store_ApplyPlaybackFact(&changed) == VSS_FACT_NOT_APPLIED);
}
static void time_and_loss(void)
{
    uint64_t when;
    for (when = 499u; when <= 501u; ++when) {
        VssOccurrenceKey k; VssPlaybackFact f;
        reset(); k = add_event(1u, VSS_SIGNAL_GOODBYE, 0u);
        CHECK(Store_BindPlayback(&k, 1u, 1u, 1u)); CHECK(Store_MarkPlaybackAttempted(&k, 1u, 1u));
        f = fact(k, 1u, 1u, 1u, VSS_FACT_FIRST_ACTUAL_START, when);
        CHECK(Store_ApplyPlaybackFact(&f) == (when == 499u ? VSS_FACT_APPLIED : VSS_FACT_NOT_APPLIED));
    }
    {
        VssOccurrenceKey k; VssPlaybackFact f; VssQualityChange q; VssOccurrenceObservation r;
        reset(); k = add_event(1u, VSS_SIGNAL_GOODBYE, 0u);
        CHECK(Store_BindPlayback(&k, 1u, 1u, 1u)); CHECK(Store_MarkPlaybackAttempted(&k, 1u, 1u));
        memset(&q, 0, sizeof q); q.signal = VSS_SIGNAL_GOODBYE; q.quality = VSS_INPUT_INVALID;
        q.basis.key = k; q.lossAt = at(300u); CHECK(Store_ApplyInput(NULL, NULL, &q) == VSS_STORE_UPDATED);
        f = fact(k, 1u, 1u, 1u, VSS_FACT_FIRST_ACTUAL_START, 299u);
        CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_APPLIED);
        CHECK(Store_ReadOccurrence(&k, &r)); CHECK(!r.replayAllowed); CHECK(r.inputLossAt.earliest == 300u);
    }
}
static void no_start_and_deadlines(void)
{
    VssOccurrenceKey k; VssPlaybackFact f; VssStorePlaybackSnapshot p;
    VssTimeEvidence t; VssOccurrenceObservation r; VssCandidateObservation c[2]; size_t n;
    reset(); k = add_event(1u, VSS_SIGNAL_WELCOME, 0u);
    CHECK(Store_BindPlayback(&k, 1u, 1u, 1u)); CHECK(Store_MarkPlaybackAttempted(&k, 1u, 1u));
    p = snapshot(); t = at(500u);
    CHECK(Store_AdvanceDeadlines(&t, &p) == VSS_STORE_RECONFIRM_REQUIRED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.state == VSS_OCCURRENCE_PENDING);
    f = fact(k, 1u, 1u, 1u, VSS_FACT_NO_START_REEVALUATE, 100u);
    CHECK(!Store_MarkPlaybackRetired(&k, 1u, 1u, true));
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_APPLIED);
    CHECK(Store_MarkPlaybackRetired(&k, 1u, 1u, true));
    t = at(200u); CHECK(Store_ReadCandidates(&t, VSS_TEST, c, 2u, &n)); CHECK(c[0].newSessionAllowed);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(Store_BindPlayback(&k, 2u, 2u, r.revision));
    CHECK(Store_MarkPlaybackAttempted(&k, 2u, 2u));
    f = fact(k, 2u, 2u, 2u, VSS_FACT_NO_START_REEVALUATE, 250u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_APPLIED); CHECK(Store_MarkPlaybackRetired(&k, 2u, 2u, true));
    f = fact(k, 1u, 1u, 1u, VSS_FACT_NO_START_REEVALUATE, 100u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_ALREADY_APPLIED);
    p = snapshot(); p.notificationPending = true; t = at(500u);
    CHECK(Store_AdvanceDeadlines(&t, &p) == VSS_STORE_RECONFIRM_REQUIRED);
    p.notificationPending = false;
    CHECK(Store_AdvanceDeadlines(&t, &p) == VSS_STORE_CHANGED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.state == VSS_OCCURRENCE_EXPIRED);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_ALREADY_APPLIED);
    reset(); k = add_event(1u, VSS_SIGNAL_WELCOME, 0u);
    CHECK(Store_BindPlayback(&k, 1u, 1u, 1u)); CHECK(Store_MarkPlaybackAttempted(&k, 1u, 1u));
    CHECK(Store_MarkPlaybackRetired(&k, 1u, 1u, false)); p = snapshot(); t = at(600u);
    CHECK(Store_AdvanceDeadlines(&t, &p) == VSS_STORE_RECONFIRM_REQUIRED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.state == VSS_OCCURRENCE_PENDING);
    CHECK(!Store_BindPlayback(&k, 2u, 2u, r.revision));
    f = fact(k, 3u, 1u, 1u, VSS_FACT_FIRST_ACTUAL_START, 100u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_NOT_APPLIED);
}
static void bounded_history_and_uncertain(void)
{
    size_t i; VssOccurrenceKey k; VssOccurrenceObservation r; VssPlaybackFact f;
    reset();
    for (i = 0u; i < VSS_PLAYBACK_BINDINGS_CAPACITY; ++i) {
        k = add_event((uint32_t)i + 1u, VSS_SIGNAL_WELCOME, 0u);
        CHECK(Store_BindPlayback(&k, (uint64_t)i + 1u, (uint64_t)i + 1u, 1u));
    }
    k = add_event(100u, VSS_SIGNAL_WELCOME, 0u);
    CHECK(!Store_BindPlayback(&k, 100u, 100u, 1u));
    reset(); k = add_event(1u, VSS_SIGNAL_WELCOME, 0u);
    CHECK(Store_BindPlayback(&k, 1u, 1u, 1u)); CHECK(Store_MarkPlaybackAttempted(&k, 1u, 1u));
    f = fact(k, 1u, 1u, 1u, VSS_FACT_START_OUTCOME_UNCERTAIN, 100u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_APPLIED);
    f = fact(k, 2u, 1u, 1u, VSS_FACT_FIRST_ACTUAL_START, 50u);
    CHECK(Store_ApplyPlaybackFact(&f) == VSS_FACT_NOT_APPLIED);
    CHECK(Store_ReadOccurrence(&k, &r)); CHECK(r.state == VSS_OCCURRENCE_UNCERTAIN_FINAL); CHECK(!r.replayAllowed);
}
int main(void)
{
    ranks_and_plans(); occurrence_order_and_keep(); fact_identity_and_lifetime();
    time_and_loss(); no_start_and_deadlines(); bounded_history_and_uncertain();
    printf("6 groups / %u checks\n", checks);
    printf("RESULT passed=%u failed=0\n", checks);
    return 0;
}
