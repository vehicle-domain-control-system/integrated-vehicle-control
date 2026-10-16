#include "vss_playback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* All lower facts below are SYNTHETIC_TEST_EVIDENCE, never board results. */
static unsigned checks;
static VssCandidateObservation candidates[VSS_CANDIDATE_CAPACITY];
static size_t candidateCount;
static VssReadBasis currentBasis;
static VssPlaybackFact published[2];
static VssFactApplication applications[2];
static size_t factsWritten, applicationsWritten;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "PB/FLOW FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static VssTimeEvidence at(uint64_t n)
{
    VssTimeEvidence t = { VSS_TEST, 1u, 0u, 0u, VSS_CONTINUITY_COMPARABLE };
    t.earliest = n; t.latest = n; return t;
}
static VssPlaybackObservation view(void)
{
    VssPlaybackObservation p; CHECK(Playback_Read(&p)); return p;
}
static void reset(void)
{
    Input_TestReset(); Store_TestReset(); StorePlayback_TestReset(); Playback_TestReset();
    VssAudio_TestReset(); VssAudio_TestEnable(true);
    Flow_TestReset();
    factsWritten = 0u; applicationsWritten = 0u;
}
static VssOccurrenceKey add_event(uint32_t id, VssInputSignal signal)
{
    VssOneShotEvent e;
    memset(&e, 0, sizeof e); e.signal = signal;
    e.meta.key.origin = VSS_TEST; e.meta.key.source = VSS_SOURCE_DOMAIN;
    e.meta.key.generation = 7u; e.meta.key.occurrenceId = id;
    e.meta.deliveryOrder = id; e.meta.original = at(0u);
    e.meta.receivedUseLimit = 500u; e.meta.useLimit = 500u; e.meta.validUntil = 500u;
    e.meta.availability = VSS_AVAILABLE;
    CHECK(Store_ApplyInput(&e, NULL, NULL) == VSS_STORE_ACCEPTED);
    return e.meta.key;
}
static VssPlanDescriptor plan(VssInputSignal signal, size_t cues)
{
    VssPlanDescriptor p;
    memset(&p, 0, sizeof p); p.key = (uint64_t)signal + 1u; p.signal = signal;
    p.value = VSS_VALUE_EVENT; p.image = 1u; p.defined = true; p.available = true;
    p.cueCount = cues; p.cues[0].asset = 1u; p.cues[1].asset = 2u; p.repeatCount = 1u;
    return p;
}
static void collect(const VssTimeEvidence *now)
{
    VssPlaybackObservation p = view();
    Store_AdvanceWarningDeadlines(now);
    CHECK(Store_ReadCandidates(now, VSS_TEST, candidates, VSS_CANDIDATE_CAPACITY, &candidateCount));
    memset(&currentBasis, 0, sizeof currentBasis); currentBasis.origin = VSS_TEST;
    currentBasis.time = *now; currentBasis.validity = VSS_READ_VALID;
    currentBasis.startsAllowed = true; currentBasis.storeRevision = Store_ReadRevision(VSS_TEST);
    currentBasis.pbRevision = p.revision;
}
static VssSelectionDecision select_plan(VssPlanDescriptor *p, const VssTimeEvidence *now)
{
    VssPolicyCurrent current;
    collect(now); Playback_ReadPolicyCurrent(&current);
    return Select_Choose(candidates, candidateCount, &current, p, 1u, &currentBasis);
}
static VssAudioKey adopt(VssInputSignal signal, size_t cues)
{
    VssTimeEvidence now = at(1u); VssPlanDescriptor p = plan(signal, cues);
    VssSelectionDecision d = select_plan(&p, &now);
    CHECK(d.action == VSS_SELECTION_CHOOSE);
    CHECK(Playback_RequestTransition(&d, NULL, VSS_STOP_NONE, candidates, candidateCount,
                                    &currentBasis, &now) == VSS_REQUEST_ADOPTED);
    d.plan.cues[0].asset = 7u; p.cues[0].asset = 6u;
    CHECK(view().plan.cues[0].asset == 1u); /* caller borrow not retained */
    CHECK(view().phase == VSS_PB_PREPARING);
    return view().attempt;
}
static void cycle(uint64_t n, size_t factCapacity, size_t applicationCapacity)
{
    VssTimeEvidence now = at(n); VssAudioOutput output[VSS_AUDIO_OUTPUT_CAPACITY];
    VssAudioObservation lower; VssPlaybackObservation observed, p = view();
    VssPlaybackDiagnostic diagnostics[VSS_PB_DIAGNOSTICS];
    size_t outputCount = 0u, lowerCount = 0u, observationWritten = 0u, diagnosticsWritten = 0u;
    collect(&now); VssAudio_Poll(output, VSS_AUDIO_OUTPUT_CAPACITY, &outputCount);
    if (p.present && VssAudio_Read(&p.attempt, &lower)) { lowerCount = 1u; }
    Playback_Advance(&lower, lowerCount, output, outputCount, &lower, lowerCount,
        candidates, candidateCount, &currentBasis, &now, &observed, 1u, &observationWritten,
        published, factCapacity, &factsWritten, applications, applicationCapacity,
        &applicationsWritten, diagnostics, VSS_PB_DIAGNOSTICS, &diagnosticsWritten);
    CHECK(factsWritten == applicationsWritten);
}
static VssAudioKey start(VssInputSignal signal, size_t cues)
{
    VssAudioKey key = adopt(signal, cues); VssTimeEvidence activated = at(2u);
    cycle(2u, 2u, 2u); CHECK(view().phase == VSS_PB_START_IN_FLIGHT);
    CHECK(VssAudio_TestActivate(&key, &activated));
    CHECK(view().startKnowledge != VSS_START_ACTUAL_CONFIRMED);
    return key;
}
static void emit(VssAudioKey key, VssAudioOutputKind kind, uint64_t id,
                 uint64_t lo, uint64_t hi)
{
    VssAudioTestEvidence e; VssAudioObservation lower;
    CHECK(VssAudio_Read(&key, &lower)); memset(&e, 0, sizeof e);
    e.key = key; e.kind = kind; e.fact = id; e.occurredAt = at(lo); e.occurredAt.latest = hi;
    e.activationAt = lower.activationAt; e.coverage = VSS_AUDIO_COVERAGE_SUFFICIENT;
    e.originalScopeMatched = true; e.observedAfterActivation = true;
    e.pastNoOutputProven = true; e.residualOutputEnded = true;
    e.futureOutputBlocked = kind != VSS_AUDIO_OUTPUT_START;
    CHECK(VssAudio_TestObserve(&e));
}
static void request_stop(VssAudioKey key, uint64_t n)
{
    VssTimeEvidence now = at(n);
    collect(&now);
    CHECK(Playback_RequestTransition(NULL, &key.session, VSS_STOP_POLICY,
        candidates, candidateCount, &currentBasis, &now) == VSS_REQUEST_ADOPTED);
    CHECK(!view().startAllowed);
}
static void request_lanes_and_prestart_loss(void)
{
    VssOccurrenceKey occurrence; VssAudioKey key; VssQualityChange q;
    VssSelectionDecision d; VssTimeEvidence now = at(1u); uint64_t wrong = 99u;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME);
    key = adopt(VSS_SIGNAL_WELCOME, 1u); memset(&d, 0, sizeof d);
    CHECK(Playback_RequestTransition(NULL, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_REJECTED);
    CHECK(Playback_RequestTransition(&d, &key.session, VSS_STOP_POLICY, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_REJECTED);
    CHECK(Playback_RequestTransition(NULL, &wrong, VSS_STOP_POLICY, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_REJECTED);
    CHECK(view().phase == VSS_PB_PREPARING);
    memset(&q, 0, sizeof q); q.signal = VSS_SIGNAL_WELCOME; q.basis.key = occurrence;
    q.quality = VSS_INPUT_INVALID; q.lossAt = at(2u);
    CHECK(Store_ApplyInput(NULL, NULL, &q) == VSS_STORE_UPDATED);
    cycle(2u, 2u, 2u); CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().startAllowed);
    now = at(2u); CHECK(!VssAudio_TestActivate(&key, &now));
}
static void first_actual_boundaries(void)
{
    uint64_t n;
    for (n = 499u; n <= 501u; ++n) {
        VssOccurrenceKey occurrence; VssAudioKey key; VssOccurrenceObservation record;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_LOCK_COMPLETE); key = start(VSS_SIGNAL_LOCK_COMPLETE, 1u);
        emit(key, VSS_AUDIO_OUTPUT_START, 1u, n, n); cycle(n, 2u, 2u);
        CHECK(Store_ReadOccurrence(&occurrence, &record));
        if (n == 499u) {
            CHECK(record.state == VSS_OCCURRENCE_STARTED); CHECK(view().phase == VSS_PB_ACTIVE);
            CHECK(factsWritten == 1u); CHECK(applications[0] == VSS_FACT_APPLIED);
        } else {
            CHECK(record.state == VSS_OCCURRENCE_PENDING); CHECK(view().phase == VSS_PB_ABORTING);
            CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN); CHECK(factsWritten == 0u);
        }
    }
    {
        VssAudioKey key; VssOccurrenceObservation record; VssOccurrenceKey occurrence;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
        emit(key, VSS_AUDIO_OUTPUT_START, 1u, 499u, 500u); cycle(501u, 2u, 2u);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
        CHECK(view().phase == VSS_PB_ABORTING); CHECK(!view().notificationPending);
    }
}
static void late_actual_phase_and_final(void)
{
    VssOccurrenceKey occurrence; VssAudioKey key; VssOccurrenceObservation record;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_GOODBYE); key = start(VSS_SIGNAL_GOODBYE, 1u);
    request_stop(key, 100u); emit(key, VSS_AUDIO_OUTPUT_START, 1u, 50u, 50u); cycle(110u, 2u, 2u);
    CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().startAllowed);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_STARTED);
    emit(key, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 120u, 120u);
    CHECK(VssAudio_TestCleanup(&key, true, false, true)); cycle(121u, 2u, 2u);
    CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().wholeSafe);
    CHECK(VssAudio_TestCleanup(&key, true, true, true)); cycle(122u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().wholeSafe);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_INTERRUPTED);
    CHECK(record.firstStartFact.occurredAt.earliest == 50u);
    CHECK(record.finalFact.occurredAt.earliest == 120u);
}
static void publication_pairs_and_no_start(void)
{
    VssOccurrenceKey occurrence; VssAudioKey key; VssOccurrenceObservation record;
    VssStorePlaybackSnapshot storeSnapshot; VssTimeEvidence now;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
    emit(key, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 0u, 2u);
    CHECK(factsWritten == 0u); CHECK(view().publicationPending); CHECK(!view().notificationPending);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_STARTED);
    cycle(12u, 1u, 0u); CHECK(factsWritten == 0u); CHECK(view().publicationPending);
    cycle(13u, 1u, 1u); CHECK(factsWritten == 1u); CHECK(!view().publicationPending);
    CHECK(published[0].occurredAt.earliest == 10u); CHECK(applications[0] == VSS_FACT_APPLIED);
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
    request_stop(key, 100u); emit(key, VSS_AUDIO_OUTPUT_NO_START, 1u, 101u, 101u);
    CHECK(VssAudio_TestCleanup(&key, true, true, true)); cycle(102u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().startKnowledge == VSS_START_NO_START_CONFIRMED);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
    now = at(103u); collect(&now); CHECK(candidates[0].newSessionAllowed);
    Playback_ReadStoreSnapshot(VSS_TEST, &storeSnapshot); now = at(500u);
    CHECK(Store_AdvanceDeadlines(&now, &storeSnapshot) == VSS_STORE_CHANGED);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_EXPIRED);
}
static void unknown_and_qualified_uncertain(void)
{
    VssOccurrenceKey occurrence; VssAudioKey key; VssOccurrenceObservation record;
    VssTimeEvidence now; VssStorePlaybackSnapshot storeSnapshot;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
    cycle(600u, 2u, 2u); CHECK(view().phase == VSS_PB_STOPPING);
    CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN);
    now = at(600u); Playback_ReadStoreSnapshot(VSS_TEST, &storeSnapshot);
    CHECK(Store_AdvanceDeadlines(&now, &storeSnapshot) == VSS_STORE_RECONFIRM_REQUIRED);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
    now = at(601u); CHECK(Playback_TestQualifiedUncertainFinal(key.session, &now));
    CHECK(view().phase == VSS_PB_QUARANTINED); CHECK(view().startKnowledge == VSS_START_UNCERTAIN_FINAL);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_UNCERTAIN_FINAL);
    emit(key, VSS_AUDIO_OUTPUT_START, 1u, 100u, 100u); cycle(602u, 2u, 2u);
    CHECK(view().phase == VSS_PB_QUARANTINED); CHECK(!view().startAllowed);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(!record.firstStartFact.present);
    emit(key, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 603u, 603u);
    CHECK(VssAudio_TestCleanup(&key, true, true, true)); cycle(604u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().wholeSafe);
    CHECK(view().startKnowledge == VSS_START_UNCERTAIN_FINAL);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_UNCERTAIN_FINAL);
    CHECK(!record.firstStartFact.present); CHECK(!record.replayAllowed);
}
static void normal_second_cue_after_first_limit(void)
{
    VssOccurrenceKey occurrence; VssAudioKey first, second; VssTimeEvidence now;
    VssOccurrenceObservation record;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_LOCK_COMPLETE); first = start(VSS_SIGNAL_LOCK_COMPLETE, 2u);
    emit(first, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 2u, 2u);
    now = at(600u); CHECK(VssAudio_TestEndSegment(&first));
    emit(first, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 600u, 600u);
    CHECK(VssAudio_TestCleanup(&first, true, true, true)); cycle(601u, 2u, 2u);
    CHECK(view().phase != VSS_PB_RETIRED); cycle(602u, 2u, 2u);
    CHECK(view().attempts == 2u); CHECK(view().session == first.session); second = view().attempt;
    CHECK(second.attempt != first.attempt); cycle(603u, 2u, 2u);
    CHECK(view().phase == VSS_PB_START_IN_FLIGHT); now = at(603u);
    CHECK(VssAudio_TestActivate(&second, &now)); emit(second, VSS_AUDIO_OUTPUT_START, 3u, 700u, 700u);
    cycle(701u, 2u, 2u); CHECK(view().phase == VSS_PB_ACTIVE);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.firstStartFact.occurredAt.earliest == 10u);
    now = at(800u); CHECK(VssAudio_TestEndSegment(&second));
    emit(second, VSS_AUDIO_OUTPUT_TERMINATED, 4u, 800u, 800u);
    CHECK(VssAudio_TestCleanup(&second, true, true, true)); cycle(801u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_COMPLETED);
    CHECK(record.finalFact.attempt == second.attempt);
}
static void invalid_out_does_not_lose_notice(void)
{
    VssAudioKey key; VssTimeEvidence now; VssPlaybackFact f[2]; VssFactApplication a[2];
    size_t invalidWritten = 91u, validWritten = 92u;
    reset(); (void)add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
    emit(key, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 0u, 0u);
    now = at(12u); collect(&now);
    Playback_Advance(NULL, 0u, NULL, 0u, NULL, 0u, candidates, candidateCount,
        &currentBasis, &now, NULL, 0u, NULL, NULL, 1u, &invalidWritten,
        a, 2u, &validWritten, NULL, 0u, NULL);
    CHECK(invalidWritten == 91u); CHECK(validWritten == 0u); CHECK(view().publicationPending);
    now = at(13u); collect(&now);
    Playback_Advance(NULL, 0u, NULL, 0u, NULL, 0u, candidates, candidateCount,
        &currentBasis, &now, NULL, 0u, NULL, f, 2u, &invalidWritten,
        a, 1u, &validWritten, NULL, 0u, NULL);
    CHECK(invalidWritten == 1u); CHECK(validWritten == 1u);
    CHECK(a[0] == VSS_FACT_APPLIED); CHECK(f[0].occurredAt.earliest == 10u);
    CHECK(!view().publicationPending);
}
static VssWarningFrame warning_frame(uint32_t tx, uint32_t sequence, uint64_t n)
{
    VssWarningFrame f; size_t i;
    memset(&f, 0, sizeof f); f.origin = VSS_TEST; f.source = VSS_SOURCE_DOMAIN;
    f.domainBoot = 7u; f.deliveryOrder = tx; f.receivedAt = at(n); f.structureVerified = true;
    for (i = 0u; i < VSS_WARNING_LANES; ++i) {
        VssWarningEvidence *e = &f.lanes[i];
        e->value = i == 0u ? VSS_VALUE_ENABLED : (i == 1u ? VSS_VALUE_EMERGENCY : VSS_VALUE_ACTIVE);
        e->quality = VSS_INPUT_VALID; e->reason = VSS_REASON_NONE;
        e->availability = i == 0u ? VSS_AVAILABILITY_NOT_PROVIDED : VSS_AVAILABLE;
        e->meaning = VSS_MEANING_CONFIRMED; e->evidenceSource = (VssEvidenceSource)(3u - (unsigned)i);
        e->evidenceBoot = i == 0u ? 7u : 10u + (uint32_t)i;
        e->evidenceSequence = sequence; e->ageComplete = true; e->provenanceProven = true;
        e->evaluationPresent = true; e->activationBasisProven = true; e->lossAt = at(n);
    }
    return f;
}
static void flow_bundle_and_warning_semantics(void)
{
    VssWarningFrame frame; VssWarningBundleResult result; VssWarningObservation warning;
    VssPlanDescriptor p; VssTimeEvidence now; VssAudioKey key; uint64_t session;
    size_t i;
    reset(); now = at(0u); CHECK(Input_OpenAfterSynchronization(VSS_TEST, 7u, &now));
    CHECK(Flow_Process(VSS_OPPORTUNITY_INITIAL, &now) == VSS_FLOW_SETTLED);
    CHECK(!view().present); CHECK(Store_Count() == 0u);
    p = plan(VSS_SIGNAL_ANTI_PINCH, 1u); p.value = VSS_VALUE_ACTIVE;
    CHECK(Flow_TestCatalog(&p, 1u, true)); frame = warning_frame(1u, 1u, 100u);
    result = VssFlow_OnWarning(&frame, &frame.receivedAt);
    CHECK(result.complete);
    for (i = 0u; i < VSS_WARNING_LANES; ++i) { CHECK(result.lanes[i].storeAdmission == VSS_STORE_UPDATED); }
    CHECK(Flow_TestSelectionCount() == 2u); /* INITIAL once, then entire bundle once */
    CHECK(view().candidate.signal == VSS_SIGNAL_ANTI_PINCH); CHECK(view().phase == VSS_PB_START_IN_FLIGHT);
    key = view().attempt; session = view().session; now = at(100u);
    CHECK(VssAudio_TestActivate(&key, &now)); emit(key, VSS_AUDIO_OUTPUT_START, 1u, 101u, 101u);
    now = at(102u); (void)Flow_Process(VSS_OPPORTUNITY_TIME_CHECK, &now);
    CHECK(view().phase == VSS_PB_ACTIVE); CHECK(Store_Count() == 0u); /* Stateful emits no occurrence */
    frame = warning_frame(2u, 2u, 110u); result = VssFlow_OnWarning(&frame, &frame.receivedAt);
    CHECK(result.complete); CHECK(view().session == session); CHECK(view().attempts == 1u);
    frame = warning_frame(3u, 3u, 120u); frame.lanes[3].quality = VSS_INPUT_INVALID;
    frame.lanes[3].reason = VSS_REASON_DATA_INVALID;
    result = VssFlow_OnWarning(&frame, &frame.receivedAt); CHECK(result.complete);
    CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_ANTI_PINCH, &warning));
    CHECK(warning.quality == VSS_INPUT_INVALID); CHECK(warning.lastValidValue == VSS_VALUE_ACTIVE);
    CHECK(warning.newSessionAllowed); CHECK(view().phase == VSS_PB_ACTIVE);
    frame = warning_frame(4u, 4u, 130u); frame.lanes[3].value = VSS_VALUE_CLEAR;
    result = VssFlow_OnWarning(&frame, &frame.receivedAt); CHECK(result.complete);
    CHECK(Store_ReadWarning(VSS_TEST, VSS_SIGNAL_ANTI_PINCH, &warning));
    CHECK(warning.lastValidValue == VSS_VALUE_CLEAR); CHECK(view().phase == VSS_PB_STOPPING);
    CHECK(view().session == session); CHECK(!view().wholeSafe);
}
static void flow_no_rx_original_deadline(void)
{
    VssOccurrenceKey occurrence; VssOccurrenceObservation record; VssTimeEvidence now;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME);
    now = at(499u); (void)Flow_Process(VSS_OPPORTUNITY_TIME_CHECK, &now);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
    now = at(500u); (void)Flow_Process(VSS_OPPORTUNITY_TIME_CHECK, &now);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_EXPIRED);
    CHECK(!view().present); CHECK(record.event.meta.original.earliest == 0u);
    now.continuity = VSS_CONTINUITY_UNKNOWN;
    CHECK(Flow_Process(VSS_OPPORTUNITY_TIME_CHECK, &now) == VSS_FLOW_RECOLLECT_OBSERVATIONS);
}
static void replace_requires_retirement_and_fresh_selection(void)
{
    VssAudioKey old; VssPlanDescriptor high; VssSelectionDecision stale, fresh;
    VssReadBasis staleBasis; VssTimeEvidence now; VssOccurrenceKey lowKey;
    VssOccurrenceObservation low;
    reset(); lowKey = add_event(1u, VSS_SIGNAL_WELCOME); old = start(VSS_SIGNAL_WELCOME, 1u);
    emit(old, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 2u, 2u);
    (void)add_event(2u, VSS_SIGNAL_LOCK_ERROR); high = plan(VSS_SIGNAL_LOCK_ERROR, 1u);
    now = at(100u); stale = select_plan(&high, &now); staleBasis = currentBasis;
    CHECK(stale.action == VSS_SELECTION_REPLACE);
    CHECK(Playback_RequestTransition(&stale, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_WAIT);
    CHECK(view().session == old.session); CHECK(view().phase == VSS_PB_STOPPING);
    now = at(101u); fresh = select_plan(&high, &now); CHECK(fresh.action == VSS_SELECTION_WAIT);
    emit(old, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 102u, 102u);
    CHECK(VssAudio_TestCleanup(&old, true, true, true)); cycle(103u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED);
    now = at(104u);
    CHECK(Playback_RequestTransition(&stale, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &staleBasis, &now) == VSS_REQUEST_WAIT);
    CHECK(view().session == old.session);
    fresh = select_plan(&high, &now); CHECK(fresh.action == VSS_SELECTION_CHOOSE);
    CHECK(Playback_RequestTransition(&fresh, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_ADOPTED);
    CHECK(view().session != old.session); CHECK(view().candidate.signal == VSS_SIGNAL_LOCK_ERROR);
    CHECK(Store_ReadOccurrence(&lowKey, &low)); CHECK(low.state == VSS_OCCURRENCE_INTERRUPTED);
    CHECK(!low.replayAllowed);
}
static void not_applied_is_protected_and_retried(void)
{
    VssOccurrenceKey primary, foreign; VssPlaybackFact collision;
    VssAudioKey key; VssOccurrenceObservation record;
    reset(); primary = add_event(1u, VSS_SIGNAL_WELCOME); foreign = add_event(2u, VSS_SIGNAL_GOODBYE);
    CHECK(Store_BindPlayback(&foreign, 99u, 99u, 1u));
    memset(&collision, 0, sizeof collision); collision.present = true; collision.fact = 1u;
    collision.occurrence = foreign; collision.session = 99u; collision.attempt = 99u;
    collision.kind = (uint32_t)VSS_FACT_START_OUTCOME_UNCERTAIN;
    collision.occurredAt = at(1u); collision.basisRevision = 1u;
    CHECK(Store_ApplyPlaybackFact(&collision) == VSS_FACT_APPLIED);
    key = start(VSS_SIGNAL_WELCOME, 1u);
    emit(key, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 0u, 0u);
    CHECK(view().notificationPending); CHECK(view().publicationPending);
    CHECK(Store_ReadOccurrence(&primary, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
    cycle(12u, 1u, 1u); CHECK(factsWritten == 1u); CHECK(applications[0] == VSS_FACT_NOT_APPLIED);
    CHECK(published[0].fact == 1u); CHECK(published[0].occurredAt.earliest == 10u);
    CHECK(view().notificationPending);
    cycle(13u, 1u, 1u); CHECK(factsWritten == 1u); CHECK(applications[0] == VSS_FACT_NOT_APPLIED);
    CHECK(published[0].fact == 1u); CHECK(published[0].occurredAt.earliest == 10u);
    CHECK(view().notificationPending); CHECK(!view().wholeSafe);
}
static void keep_rechecks_current_restriction(void)
{
    VssOccurrenceKey occurrence; VssAudioKey key; VssPlanDescriptor p;
    VssSelectionDecision keep; VssPolicyCurrent current; VssTimeEvidence now;
    VssOccurrenceObservation record; VssAudioObservation lower;
    VssAudioOutput output[1]; size_t written = 99u;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
    now = at(3u); collect(&now); currentBasis.startsAllowed = false;
    Playback_ReadPolicyCurrent(&current); p = plan(VSS_SIGNAL_WELCOME, 1u);
    keep = Select_Choose(candidates, candidateCount, &current, &p, 1u, &currentBasis);
    CHECK(keep.action == VSS_SELECTION_KEEP);
    CHECK(Playback_RequestTransition(&keep, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_WAIT);
    CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().startAllowed);
    CHECK(view().session == key.session); CHECK(!view().wholeSafe);
    CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_PENDING);
    CHECK(!record.firstStartFact.present); CHECK(!record.finalFact.present);
    CHECK(VssAudio_Read(&key, &lower)); CHECK(lower.futurePcmBlocked); CHECK(!lower.outputClosed);
    VssAudio_Poll(output, 1u, &written); CHECK(written == 0u);
    /* A later restriction lift cannot restore the already blocked owner. */
    now = at(4u); collect(&now); keep.basis = currentBasis;
    CHECK(Playback_RequestTransition(&keep, NULL, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_WAIT);
    CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().startAllowed);
    CHECK(Playback_RequestTransition(NULL, &key.session, VSS_STOP_NONE, candidates, candidateCount,
        &currentBasis, &now) == VSS_REQUEST_ADOPTED);
    CHECK(view().phase == VSS_PB_STOPPING); CHECK(!view().wholeSafe);
}
/* C1 permanent regressions; the independent review harness remains unchanged. */
static void c1_policy_order(void)
{
    VssCandidateObservation input[5], permutation[5];
    VssPolicyCurrent current;
    VssPlanDescriptor plans[2];
    VssSelectionDecision decision;
    VssTimeEvidence now = at(100u), initial = at(0u);
    VssWarningFrame frame;
    size_t count, i, j, k, n = 0u, indices[5] = {0u, 1u, 2u, 3u, 4u};
    uint32_t id;
    reset(); CHECK(Input_OpenAfterSynchronization(VSS_TEST, 7u, &initial));
    for (id = 1u; id <= 2u; ++id) {
        VssOccurrenceKey key = {VSS_TEST, VSS_SOURCE_DOMAIN, 7u, 0u};
        VssInputEvidence event;
        memset(&event, 0, sizeof(event)); key.occurrenceId = id;
        event.signal = VSS_SIGNAL_WELCOME; event.value = VSS_VALUE_EVENT;
        event.quality = VSS_INPUT_VALID; event.occurrence = &key;
        event.deliveryOrder = id; event.receivedAt = now;
        event.ageLowMs = id == 1u ? 80u : 75u;
        event.ageHighMs = id == 1u ? 90u : 85u;
        event.ageComplete = true; event.useLimitMs = 500u; event.availability = VSS_AVAILABLE;
        CHECK(Input_Process(&event, &now, NULL, 0u, NULL).storeAdmission == VSS_STORE_ACCEPTED);
    }
    frame = warning_frame(3u, 1u, 100u);
    frame.lanes[2].value = VSS_VALUE_CLEAR; frame.lanes[3].value = VSS_VALUE_CLEAR;
    CHECK(Input_ProcessWarningFrame(&frame, &now).complete);
    CHECK(Store_ReadCandidates(&now, VSS_TEST, input, 5u, &count)); CHECK(count == 5u);
    plans[0] = plan(VSS_SIGNAL_WELCOME, 1u);
    plans[1] = plan(VSS_SIGNAL_REAR_RISK, 1u); plans[1].value = VSS_VALUE_EMERGENCY;
    memset(&current, 0, sizeof(current)); collect(&now);
    /* All 5! enumerations of the same real INPUT/STORE snapshot. */
    for (;;) {
        for (i = 0u; i < 5u; ++i) { permutation[i] = input[indices[i]]; }
        decision = Select_Choose(permutation, 5u, &current, plans, 2u, &currentBasis);
        CHECK(decision.action == VSS_SELECTION_CHOOSE);
        CHECK(decision.candidate.signal == VSS_SIGNAL_REAR_RISK);
        CHECK(decision.candidate.value == VSS_VALUE_EMERGENCY);
        CHECK(decision.candidate.warningMeta.evidenceSequence == 1u); ++n;
        i = 4u;
        while (i != 0u && indices[i - 1u] > indices[i]) { --i; }
        if (i == 0u) { break; }
        j = 4u;
        while (indices[j] < indices[i - 1u]) { --j; }
        k = indices[j]; indices[j] = indices[i - 1u]; indices[i - 1u] = k;
        for (j = 4u; i < j; ++i, --j) { k = indices[i]; indices[i] = indices[j]; indices[j] = k; }
    }
    CHECK(n == 120u);
    for (i = 0u; i < 2u; ++i) {
        permutation[0] = input[i]; permutation[1] = input[1u - i];
        CHECK(Select_Choose(permutation, 2u, &current, plans, 2u, &currentBasis).action == VSS_SELECTION_RECOLLECT);
    }
    plans[1].available = false;
    CHECK(Select_Choose(input, 5u, &current, plans, 2u, &currentBasis).action == VSS_SELECTION_WAIT);
    plans[1].available = true;
    input[0].revision = 0u; /* Even unrelated lower-rank invalidity remains global. */
    CHECK(Select_Choose(input, 5u, &current, plans, 2u, &currentBasis).action == VSS_SELECTION_RECOLLECT);
    input[0].revision = 1u;
    current.present = true; current.valid = true; current.session = 1u;
    current.candidate = input[0]; current.plan = plans[0];
    CHECK(Select_Choose(input, 2u, &current, plans, 2u, &currentBasis).action == VSS_SELECTION_KEEP);
    current.stopping = true;
    CHECK(Select_Choose(input, 5u, &current, plans, 2u, &currentBasis).action == VSS_SELECTION_WAIT);
    /* Same top rank: two later overlapping records do not obscure a proved
     * earlier original. This guards against merely deferring the same bug. */
    memset(&current, 0, sizeof(current));
    input[2] = input[0]; input[2].key.occurrenceId = 3u;
    input[2].eventMeta.key = input[2].key; input[2].eventMeta.original = at(5u);
    for (i = 0u; i < 3u; ++i) {
        for (j = 0u; j < 3u; ++j) {
            if (i == j) { continue; }
            k = 3u - i - j;
            permutation[0] = input[i]; permutation[1] = input[j]; permutation[2] = input[k];
            decision = Select_Choose(permutation, 3u, &current, plans, 2u, &currentBasis);
            CHECK(decision.action == VSS_SELECTION_CHOOSE); CHECK(decision.candidate.key.occurrenceId == 3u);
        }
    }
}
static VssAudioKey c1_start_plan(size_t cues, uint32_t repeats)
{
    VssPlanDescriptor p = plan(VSS_SIGNAL_LOCK_COMPLETE, cues);
    VssSelectionDecision d;
    VssTimeEvidence now = at(1u);
    VssAudioKey key;
    p.repeatCount = repeats; p.cues[2].asset = 3u; p.cues[3].asset = 4u;
    d = select_plan(&p, &now); CHECK(d.action == VSS_SELECTION_CHOOSE);
    CHECK(Playback_RequestTransition(&d, NULL, VSS_STOP_NONE, candidates, candidateCount,
                                    &currentBasis, &now) == VSS_REQUEST_ADOPTED);
    key = view().attempt; cycle(2u, 2u, 2u); now = at(2u);
    CHECK(VssAudio_TestActivate(&key, &now)); return key;
}
static VssAudioKey c1_next_position(VssAudioKey old, uint64_t ended)
{
    VssAudioKey next;
    VssTimeEvidence now;
    CHECK(VssAudio_TestEndSegment(&old));
    emit(old, VSS_AUDIO_OUTPUT_TERMINATED, ended, ended, ended);
    CHECK(VssAudio_TestCleanup(&old, true, true, true));
    cycle(ended + 1u, 2u, 2u); cycle(ended + 2u, 2u, 2u);
    next = view().attempt; CHECK(next.attempt != old.attempt); CHECK(next.session == old.session);
    cycle(ended + 3u, 2u, 2u); now = at(ended + 3u);
    CHECK(VssAudio_TestActivate(&next, &now)); return next;
}
static void c1_missing_positions(void)
{
    size_t variant;
    for (variant = 0u; variant < 3u; ++variant) {
        VssOccurrenceKey occurrence;
        VssOccurrenceObservation record;
        VssAudioKey first, missed;
        size_t cues = variant == 2u ? 1u : (variant == 1u ? 3u : 2u);
        uint32_t repeats = variant == 2u ? 2u : 1u;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_LOCK_COMPLETE);
        first = c1_start_plan(cues, repeats);
        emit(first, VSS_AUDIO_OUTPUT_START, 1u, 10u, 10u); cycle(11u, 2u, 2u);
        missed = c1_next_position(first, 600u); CHECK(view().attempts == 2u);
        CHECK(VssAudio_TestEndSegment(&missed));
        emit(missed, VSS_AUDIO_OUTPUT_NO_START, 2u, 800u, 800u);
        CHECK(VssAudio_TestCleanup(&missed, true, false, true)); cycle(801u, 2u, 2u);
        CHECK(!view().wholeSafe); /* missing PCM reference prevents retirement */
        CHECK(VssAudio_TestCleanup(&missed, true, true, true)); cycle(802u, 2u, 2u);
        CHECK(view().wholeSafe); CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().attempts == 2u);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_INTERRUPTED);
        CHECK(record.finalFact.kind == (uint32_t)VSS_FACT_WHOLE_INTERRUPTED);
        CHECK(record.firstStartFact.occurredAt.earliest == 10u); CHECK(record.firstStartFact.attempt == first.attempt);
        CHECK(!record.replayAllowed); CHECK(record.event.meta.original.earliest == 0u);
        cycle(810u, 2u, 2u); CHECK(view().attempts == 2u); /* no skipped middle cue/retry */
    }
}
static void c1_normal_repeated_plan(void)
{
    VssOccurrenceKey occurrence; VssOccurrenceObservation record;
    VssAudioKey key; size_t position;
    reset(); occurrence = add_event(1u, VSS_SIGNAL_LOCK_COMPLETE); key = c1_start_plan(2u, 2u);
    for (position = 0u; position < 4u; ++position) {
        uint64_t actual = position == 0u ? 10u : 700u + (uint64_t)(position - 1u) * 200u;
        emit(key, VSS_AUDIO_OUTPUT_START, actual, actual, actual); cycle(actual + 1u, 2u, 2u);
        CHECK(view().phase == VSS_PB_ACTIVE); CHECK(view().cue == position % 2u);
        CHECK(view().repeat == (uint32_t)(position / 2u));
        if (position < 3u) { key = c1_next_position(key, 600u + (uint64_t)position * 200u); }
    }
    CHECK(VssAudio_TestEndSegment(&key));
    emit(key, VSS_AUDIO_OUTPUT_TERMINATED, 1200u, 1200u, 1200u);
    CHECK(VssAudio_TestCleanup(&key, true, true, true)); cycle(1201u, 2u, 2u);
    CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().attempts == 4u);
    CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.state == VSS_OCCURRENCE_COMPLETED);
    CHECK(record.firstStartFact.occurredAt.earliest == 10u); CHECK(record.finalFact.attempt == key.attempt);
}
static size_t c1_capture(uint64_t n, VssPlaybackDiagnostic *diagnostics)
{
    VssTimeEvidence now = at(n); size_t written = 0u;
    collect(&now); Playback_Service(candidates, candidateCount, &currentBasis, &now);
    collect(&now);
    Playback_Advance(NULL, 0u, NULL, 0u, NULL, 0u, candidates, candidateCount, &currentBasis, &now,
        NULL, 0u, NULL, published, 2u, &factsWritten, applications, 2u, &applicationsWritten,
        diagnostics, VSS_PB_DIAGNOSTICS, &written);
    CHECK(factsWritten == applicationsWritten); return written;
}
static void c1_unknown_stop_time(void)
{
    size_t variant;
    for (variant = 0u; variant < 6u; ++variant) {
        VssOccurrenceKey occurrence; VssOccurrenceObservation record;
        VssAudioKey key; VssTimeEvidence unknown = at(90u), later = at(200u);
        VssPlaybackDiagnostic diagnostics[VSS_PB_DIAGNOSTICS]; size_t n, i; bool physical = false;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
        if (variant == 1u) { unknown.continuity = VSS_CONTINUITY_UNKNOWN; }
        if (variant == 2u) { unknown.continuity = VSS_CONTINUITY_LOST; }
        if (variant == 3u) { unknown.latest = 89u; }
        if (variant == 4u) { unknown.domain = VSS_PRODUCT; }
        if (variant == 5u) { unknown.epoch = 2u; }
        CHECK(Playback_RequestTransition(NULL, &key.session, VSS_STOP_POLICY, candidates, candidateCount,
            &currentBasis, variant == 0u ? NULL : &unknown) == VSS_REQUEST_ADOPTED);
        CHECK(!view().startAllowed); CHECK(view().phase == VSS_PB_STOPPING);
        CHECK(Playback_RequestTransition(NULL, &key.session, VSS_STOP_POLICY, candidates, candidateCount,
            &currentBasis, &later) == VSS_REQUEST_ADOPTED);
        emit(key, VSS_AUDIO_OUTPUT_START, 1u, 100u, 100u); n = c1_capture(210u, diagnostics);
        for (i = 0u; i < n; ++i) {
            CHECK(diagnostics[i].code != VSS_PB_DIAG_VIOLATION);
            if (diagnostics[i].code == VSS_PB_DIAG_TIME_UNCERTAIN && diagnostics[i].evidencePresent) {
                CHECK(diagnostics[i].evidence.occurredAt.earliest == 100u);
                CHECK(diagnostics[i].evidence.key.attempt == key.attempt); physical = true;
            }
        }
        CHECK(physical); CHECK(view().phase == VSS_PB_ABORTING); CHECK(!view().startAllowed);
        CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(!record.firstStartFact.present);
        CHECK(!record.finalFact.present); CHECK(record.state == VSS_OCCURRENCE_PENDING);
        emit(key, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 220u, 220u);
        CHECK(VssAudio_TestCleanup(&key, true, true, true)); cycle(221u, 2u, 2u);
        CHECK(view().phase == VSS_PB_RETIRED); CHECK(view().wholeSafe);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(!record.firstStartFact.present);
        CHECK(!record.finalFact.present); CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN);
        later = at(222u); collect(&later); CHECK(!candidates[0].newSessionAllowed);
    }
}
static void c1_known_stop_and_foreign_key(void)
{
    const uint64_t low[4] = {50u, 150u, 99u, 100u};
    const uint64_t high[4] = {50u, 150u, 101u, 100u};
    size_t variant;
    for (variant = 0u; variant < 4u; ++variant) {
        VssOccurrenceKey occurrence; VssOccurrenceObservation record; VssAudioKey key;
        VssPlaybackDiagnostic diagnostics[VSS_PB_DIAGNOSTICS]; size_t n, i; bool found = false;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
        request_stop(key, 100u); request_stop(key, 200u); /* first known boundary cannot move */
        emit(key, VSS_AUDIO_OUTPUT_START, 1u, low[variant], high[variant]); n = c1_capture(210u, diagnostics);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(!view().startAllowed);
        if (variant == 0u) {
            CHECK(record.firstStartFact.present); CHECK(record.firstStartFact.occurredAt.earliest == 50u);
            CHECK(view().phase == VSS_PB_STOPPING);
        } else {
            CHECK(!record.firstStartFact.present); CHECK(!record.finalFact.present);
            for (i = 0u; i < n; ++i) {
                if (diagnostics[i].code == (variant == 2u ? VSS_PB_DIAG_TIME_UNCERTAIN : VSS_PB_DIAG_VIOLATION)) {
                    CHECK(diagnostics[i].evidencePresent); found = true;
                }
            }
            CHECK(found); CHECK(view().startKnowledge == VSS_START_PENDING_OR_UNKNOWN);
        }
    }
    {
        VssOccurrenceKey occurrence; VssOccurrenceObservation record; VssAudioKey key; uint64_t wrong;
        reset(); occurrence = add_event(1u, VSS_SIGNAL_WELCOME); key = start(VSS_SIGNAL_WELCOME, 1u);
        wrong = key.session + 1u;
        CHECK(Playback_RequestTransition(NULL, &wrong, VSS_STOP_POLICY, candidates, candidateCount,
            &currentBasis, NULL) == VSS_REQUEST_REJECTED);
        CHECK(view().startAllowed); emit(key, VSS_AUDIO_OUTPUT_START, 1u, 100u, 100u); cycle(110u, 2u, 2u);
        CHECK(Store_ReadOccurrence(&occurrence, &record)); CHECK(record.firstStartFact.present);
        CHECK(view().phase == VSS_PB_ACTIVE);
    }
}
int main(void)
{
    unsigned baselineChecks;
    request_lanes_and_prestart_loss(); first_actual_boundaries(); late_actual_phase_and_final();
    publication_pairs_and_no_start(); unknown_and_qualified_uncertain(); normal_second_cue_after_first_limit();
    invalid_out_does_not_lose_notice(); flow_bundle_and_warning_semantics(); flow_no_rx_original_deadline();
    replace_requires_retirement_and_fresh_selection(); not_applied_is_protected_and_retried();
    keep_rechecks_current_restriction();
    printf("12 groups / %u checks PASS; SYNTHETIC_TEST_EVIDENCE; hardware NOT_RUN\n", checks);
    baselineChecks = checks;
    c1_policy_order(); c1_missing_positions(); c1_normal_repeated_plan();
    c1_unknown_stop_time(); c1_known_stop_and_foreign_key();
    printf("C1 additional 5 groups / %u checks PASS; SYNTHETIC_TEST_EVIDENCE\n", checks - baselineChecks);
    printf("TOTAL playback 17 groups / %u checks PASS; hardware NOT_RUN\n", checks);
    return 0;
}
