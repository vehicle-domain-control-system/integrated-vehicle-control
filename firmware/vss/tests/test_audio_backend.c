#include "vss_audio_backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "FAIL audio line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static VssFactTime at(uint64_t value)
{
    VssFactTime t = { VSS_PRODUCT, 1u, 0u, 0u, VSS_CONTINUITY_COMPARABLE };
    t.earliest = value; t.latest = value; return t;
}
static VssAudioKey key(uint64_t id)
{
    VssAudioKey k = { 1u, 1u, 1u, 1u, VSS_PRODUCT };
    k.attempt = id; k.operation = id; return k;
}
static VssAudioTestEvidence evidence(VssAudioKey k, VssAudioOutputKind kind,
                                     uint64_t fact, uint64_t time)
{
    VssAudioTestEvidence e;
    memset(&e, 0, sizeof(e)); e.key = k; e.kind = kind; e.fact = fact;
    e.occurredAt = at(time); e.activationAt = at(20u);
    e.coverage = VSS_AUDIO_COVERAGE_SUFFICIENT;
    e.originalScopeMatched = true; e.observedAfterActivation = true;
    e.pastNoOutputProven = true; e.residualOutputEnded = true;
    e.futureOutputBlocked = true; return e;
}
static void prepare(VssAudioKey k)
{
    VssFactTime t = at(10u);
    CHECK(VssAudio_Prepare(&k, 3u, &t) == VSS_AUDIO_PREPARED);
    t = at(15u); CHECK(VssAudio_Start(&k, &t) == VSS_AUDIO_SUBMITTED);
    t = at(20u); CHECK(VssAudio_TestActivate(&k, &t));
}
static void product_default_hold(void)
{
    VssAudioKey k = key(1u);
    VssFactTime t = at(10u);
    VssAudioObservation o;
    VssAudioOutput output[3]; size_t written = 99u;
    VssAudio_TestReset();
    CHECK(VssAudio_Prepare(&k, 3u, &t) == VSS_AUDIO_HOLD);
    CHECK(VssAudio_Start(&k, &t) == VSS_AUDIO_HOLD);
    CHECK(VssAudio_Stop(&k, &t) == VSS_AUDIO_HOLD);
    CHECK(VssAudio_Read(&k, &o));
    CHECK(o.physicalHold && !o.prepared && !o.startSubmitted && !o.activationPresent);
    CHECK(o.futurePcmBlocked && !o.productionAllowed && !o.outputClosed);
    CHECK(!o.providerReleased && !o.pcmReleased && !o.lateProtected);
    VssAudio_Poll(output, 3u, &written); CHECK(written == 0u);
    CHECK(VssAudio_Prepare(&k, 0u, &t) == VSS_AUDIO_REJECTED);
    k = key(2u); CHECK(VssAudio_Prepare(&k, 8u, &t) == VSS_AUDIO_REJECTED);
}
static void independent_evidence_and_late(void)
{
    VssAudioKey k = key(1u);
    VssAudioTestEvidence e;
    VssAudioObservation o;
    VssAudioOutput out[3]; size_t written = 77u;
    VssFactTime t = at(30u);
    VssAudio_TestReset(); VssAudio_TestEnable(true); prepare(k);
    e = evidence(k, VSS_AUDIO_OUTPUT_START, 1u, 21u);
    e.coverage = VSS_AUDIO_COVERAGE_INSUFFICIENT; CHECK(!VssAudio_TestObserve(&e));
    e.coverage = VSS_AUDIO_COVERAGE_SUFFICIENT;
    e.originalScopeMatched = false; CHECK(!VssAudio_TestObserve(&e));
    e.originalScopeMatched = true; e.observedAfterActivation = false;
    CHECK(!VssAudio_TestObserve(&e)); e.observedAfterActivation = true;
    e.occurredAt = at(20u); CHECK(!VssAudio_TestObserve(&e));
    e.occurredAt = at(21u); e.activationAt = at(19u); CHECK(!VssAudio_TestObserve(&e));
    e.activationAt = at(20u); e.key.operation = 50u; CHECK(!VssAudio_TestObserve(&e));
    e.key = k;
    CHECK(VssAudio_Stop(&k, &t) == VSS_AUDIO_CLEANUP_PENDING);
    CHECK(VssAudio_TestObserve(&e)); /* legal late original fact, no restart */
    CHECK(VssAudio_TestObserve(&e)); /* duplicate fact emits once */
    e.fact = 2u; CHECK(!VssAudio_TestObserve(&e));
    CHECK(VssAudio_Read(&k, &o));
    CHECK(o.futurePcmBlocked && !o.productionAllowed && !o.outputClosed);
    CHECK(o.state == VSS_AUDIO_CLEANUP_PENDING);
    e = evidence(k, VSS_AUDIO_OUTPUT_NO_START, 2u, 31u);
    CHECK(!VssAudio_TestObserve(&e)); /* cannot deny existing actual */
    e = evidence(k, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 31u);
    e.residualOutputEnded = false; CHECK(!VssAudio_TestObserve(&e));
    e.residualOutputEnded = true; e.futureOutputBlocked = false;
    CHECK(!VssAudio_TestObserve(&e)); e.futureOutputBlocked = true;
    CHECK(VssAudio_TestObserve(&e));
    CHECK(VssAudio_Read(&k, &o)); CHECK(o.outputClosed && !o.pcmReleased);
    CHECK(VssAudio_Start(&k, &t) == VSS_AUDIO_HOLD);
    VssAudio_Poll(NULL, 2u, &written); CHECK(written == 77u);
    VssAudio_Poll(NULL, 0u, &written); CHECK(written == 0u);
    VssAudio_Poll(out, 1u, &written); CHECK(written == 1u);
    CHECK(out[0].kind == VSS_AUDIO_OUTPUT_START && out[0].fact == 1u && out[0].synthetic);
    VssAudio_Poll(out, 3u, &written); CHECK(written == 1u);
    CHECK(out[0].kind == VSS_AUDIO_OUTPUT_TERMINATED && out[0].fact == 2u);
    CHECK(VssAudio_TestCleanup(&k, true, false, true));
    CHECK(VssAudio_Read(&k, &o)); CHECK(o.providerReleased && !o.pcmReleased && o.lateProtected);
}
static void no_start_and_capacity(void)
{
    VssAudioKey k = key(1u);
    VssAudioTestEvidence e;
    VssAudioObservation o;
    VssFactTime t = at(30u);
    unsigned i;
    VssAudio_TestReset(); VssAudio_TestEnable(true); prepare(k);
    e = evidence(k, VSS_AUDIO_OUTPUT_NO_START, 1u, 31u);
    CHECK(!VssAudio_TestObserve(&e)); /* needs producer fence too */
    CHECK(VssAudio_Stop(&k, &t) == VSS_AUDIO_CLEANUP_PENDING);
    e.pastNoOutputProven = false; CHECK(!VssAudio_TestObserve(&e));
    e.pastNoOutputProven = true; CHECK(VssAudio_TestObserve(&e));
    e = evidence(k, VSS_AUDIO_OUTPUT_START, 2u, 21u); CHECK(!VssAudio_TestObserve(&e));
    k = key(2u); CHECK(VssAudio_Prepare(&k, 3u, &t) == VSS_AUDIO_HOLD);
    for (i = 2u; i <= VSS_AUDIO_CAPACITY; ++i) {
        k = key((uint64_t)i); k.session = (uint64_t)i; prepare(k);
    }
    k = key(9u); k.session = 9u; CHECK(VssAudio_Prepare(&k, 3u, &t) == VSS_AUDIO_HOLD);
    k = key(1u); CHECK(VssAudio_Read(&k, &o)); CHECK(o.outputClosed && o.futurePcmBlocked);
    k.segment = 2u; CHECK(VssAudio_Prepare(&k, 3u, &t) == VSS_AUDIO_REJECTED);
}
static VssAudioTestWriteWindow window(VssAudioKey k, uint8_t buffer, uint64_t cycle)
{
    VssAudioTestWriteWindow w;
    memset(&w, 0, sizeof(w)); w.key = k; w.buffer = buffer; w.cycle = cycle;
    w.opensAt = at(10u); w.closesBefore = at(40u);
    w.coverage = VSS_AUDIO_COVERAGE_SUFFICIENT;
    w.oldReferencesEnded = true; w.oldContentsCannotRecur = true;
    w.memoryCoherent = true; w.writeBudget = 2u; return w;
}
static void pcm_cycles_and_window(void)
{
    /* Exactly two transfer buffers in this fixture; backend owns descriptors. */
    int16_t a[VSS_AUDIO_PCM_FRAMES * 2u];
    int16_t b[VSS_AUDIO_PCM_FRAMES * 2u];
    VssAudioKey k = key(1u);
    VssAudioPcmObservation p;
    VssAudioTestWriteWindow wa = window(k, 0u, 1u), wb = window(k, 1u, 1u);
    VssFactTime t = at(21u);
    VssAudio_TestReset(); VssAudio_TestEnable(true); prepare(k);
    CHECK(VssAudio_TestBindPcm(&k, 0u, a, VSS_AUDIO_PCM_FRAMES, &wa, &t));
    CHECK(VssAudio_TestBindPcm(&k, 1u, b, VSS_AUDIO_PCM_FRAMES, &wb, &t));
    CHECK(!VssAudio_TestHandoff(&k, 0u, 1u, 0u, false, true, &t));
    CHECK(!VssAudio_TestHandoff(&k, 0u, 1u, 1025u, false, true, &t));
    CHECK(VssAudio_TestHandoff(&k, 0u, 1u, 31u, true, true, &t));
    CHECK(VssAudio_TestConsumed(&k, 0u, 1u));
    CHECK(VssAudio_ReadPcm(0u, &p));
    CHECK(p.usage == VSS_PCM_HW_PROTECTED && p.validFrames == 31u && p.consumed);
    CHECK(!VssAudio_TestHandoff(&k, 0u, 1u, 20u, false, true, &t));
    wa.oldContentsCannotRecur = false; CHECK(!VssAudio_TestReturn(&wa, &t));
    wa.oldContentsCannotRecur = true; wa.oldReferencesEnded = false;
    CHECK(!VssAudio_TestReturn(&wa, &t)); wa.oldReferencesEnded = true;
    wa.memoryCoherent = false; CHECK(!VssAudio_TestReturn(&wa, &t)); wa.memoryCoherent = true;
    t = at(40u); CHECK(!VssAudio_TestReturn(&wa, &t)); /* exact window end fails */
    t = at(39u); t.latest = 40u; CHECK(!VssAudio_TestReturn(&wa, &t));
    t = at(38u); CHECK(!VssAudio_TestReturn(&wa, &t)); /* no WCET margin at boundary */
    t = at(21u); wa.cycle = 2u; CHECK(!VssAudio_TestReturn(&wa, &t)); wa.cycle = 1u;
    CHECK(VssAudio_TestReturn(&wa, &t)); CHECK(!VssAudio_TestReturn(&wa, &t));
    CHECK(VssAudio_ReadPcm(0u, &p)); CHECK(p.usage == VSS_PCM_CPU_WRITABLE && p.cycle == 2u);
    CHECK(!VssAudio_TestConsumed(&k, 0u, 1u));
    CHECK(VssAudio_TestHandoff(&k, 1u, 1u, 1024u, false, false, &t));
    CHECK(VssAudio_ReadPcm(1u, &p)); CHECK(p.usage == VSS_PCM_RETURN_PENDING);
    CHECK(VssAudio_Stop(&k, &t) == VSS_AUDIO_CLEANUP_PENDING);
    CHECK(!VssAudio_TestCleanup(&k, true, true, true)); /* B retained */
    CHECK(!VssAudio_TestHandoff(&k, 0u, 2u, 20u, false, true, &t));
    CHECK(VssAudio_TestReturn(&wb, &t)); CHECK(VssAudio_TestCleanup(&k, true, true, true));
}
static void normal_next_cue_and_session_stop(void)
{
    VssAudioKey first = key(1u), next = key(2u), forbidden = key(3u);
    VssAudioTestEvidence e;
    VssAudioObservation o;
    VssFactTime t = at(40u);
    VssAudio_TestReset(); VssAudio_TestEnable(true); prepare(first);
    e = evidence(first, VSS_AUDIO_OUTPUT_START, 1u, 21u);
    CHECK(VssAudio_TestObserve(&e));
    CHECK(VssAudio_TestEndSegment(&first));
    CHECK(VssAudio_Read(&first, &o));
    CHECK(o.futurePcmBlocked && !o.productionAllowed && !o.outputClosed);
    CHECK(VssAudio_Prepare(&next, 3u, &t) == VSS_AUDIO_HOLD);
    e = evidence(first, VSS_AUDIO_OUTPUT_TERMINATED, 2u, 30u);
    CHECK(VssAudio_TestObserve(&e));
    CHECK(VssAudio_Prepare(&next, 3u, &t) == VSS_AUDIO_HOLD); /* refs remain */
    CHECK(VssAudio_TestCleanup(&first, true, true, true));
    CHECK(VssAudio_Prepare(&next, 3u, &t) == VSS_AUDIO_PREPARED);
    CHECK(VssAudio_Start(&next, &t) == VSS_AUDIO_SUBMITTED);
    t = at(41u); CHECK(VssAudio_TestActivate(&next, &t));
    CHECK(VssAudio_Stop(&next, &t) == VSS_AUDIO_CLEANUP_PENDING);
    CHECK(!VssAudio_TestEndSegment(&next));
    CHECK(VssAudio_Prepare(&forbidden, 3u, &t) == VSS_AUDIO_HOLD);
}
int main(void)
{
    product_default_hold(); independent_evidence_and_late();
    no_start_and_capacity(); pcm_cycles_and_window();
    normal_next_cue_and_session_stop();
    printf("PASS audio backend 5 groups, %u checks; SYNTHETIC_TEST_EVIDENCE; hardware NOT_RUN\n", checks);
    return 0;
}
