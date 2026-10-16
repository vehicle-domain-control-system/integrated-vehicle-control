#include "vss_audio_backend.h"

#include <string.h>
#ifdef VSS_S32K344_REFERENCE_BINDING
/* Exact existing public headers. No RTD type redeclaration or SDK emulation. */
#include "AudioClock.h"
#include "AudioCodec.h"
#include "AudioDmaTransport.h"
#endif

typedef struct {
    bool used;
    bool sessionStopped;
    uint8_t asset;
    VssFactTime preparedAt;
    VssAudioObservation observation;
#ifdef VSS_HOST_TEST
    bool factPresent[3];
    VssAudioOutput facts[3];
#endif
} AudioRecord;
static AudioRecord records[VSS_AUDIO_CAPACITY];
static size_t recordCount;
static VssAudioOutput pending[VSS_AUDIO_OUTPUT_CAPACITY];
static size_t pendingCount;
static VssAudioPcmObservation pcm[2]; /* descriptors only, no PCM allocation */
#ifdef VSS_HOST_TEST
static bool syntheticEnabled;
static int16_t *pcmSamples[2];
static VssAudioTestWriteWindow pcmWindows[2];
#endif

static bool valid_key(const VssAudioKey *key)
{
    return key != NULL && key->session != 0u && key->attempt != 0u &&
        key->operation != 0u && key->segment != 0u &&
        (key->origin == VSS_PRODUCT || key->origin == VSS_TEST);
}
static bool same_key(const VssAudioKey *a, const VssAudioKey *b)
{
    return a->session == b->session && a->attempt == b->attempt &&
        a->operation == b->operation && a->segment == b->segment &&
        a->origin == b->origin;
}
static bool valid_time(const VssFactTime *time, VssOrigin origin)
{
    return time != NULL && time->domain == origin && time->epoch != 0u &&
        time->continuity == VSS_CONTINUITY_COMPARABLE &&
        time->earliest <= time->latest;
}
static AudioRecord *find_record(const VssAudioKey *key)
{
    size_t i;
    if (!valid_key(key)) return NULL;
    for (i = 0u; i < recordCount; ++i) {
        if (same_key(key, &records[i].observation.key)) return &records[i];
    }
    return NULL;
}
static bool record_time(const AudioRecord *record, const VssTimeEvidence *now)
{
    return valid_time(now, record->observation.key.origin) &&
        now->epoch == record->preparedAt.epoch &&
        now->earliest >= record->preparedAt.latest;
}

VssAudioState VssAudio_Prepare(const VssAudioKey *key, uint8_t assetId,
                              const VssTimeEvidence *now)
{
    AudioRecord *record;
    size_t i;
    if (!valid_key(key) || assetId >= 8u || !valid_time(now, key->origin))
        return VSS_AUDIO_REJECTED;
    record = find_record(key);
    if (record != NULL)
        return record->asset == assetId ? record->observation.state : VSS_AUDIO_REJECTED;
    /* Identity is immutable; no alias, wrap/reuse or last-request overwrite. */
    for (i = 0u; i < recordCount; ++i) {
        const VssAudioKey *old = &records[i].observation.key;
        if (old->operation == key->operation || old->attempt == key->attempt)
            return VSS_AUDIO_REJECTED;
        if (old->session == key->session && old->origin == key->origin) {
            const VssAudioObservation *prior = &records[i].observation;
            if (records[i].sessionStopped)
                return VSS_AUDIO_HOLD; /* cannot reopen a stopped Session */
            if (prior->futurePcmBlocked &&
                (!prior->outputClosed || !prior->providerReleased ||
                 !prior->pcmReleased || !prior->lateProtected))
                return VSS_AUDIO_HOLD; /* segment EOS is not physical closure */
        }
    }
    if (recordCount == VSS_AUDIO_CAPACITY) return VSS_AUDIO_HOLD;
    record = &records[recordCount++];
    record->used = true;
    record->asset = assetId;
    record->preparedAt = *now;
    record->observation.key = *key;
    record->observation.state = VSS_AUDIO_HOLD;
    record->observation.physicalHold = true;
    record->observation.coverage = VSS_AUDIO_COVERAGE_INSUFFICIENT;
    record->observation.revision = 1u;
#ifdef VSS_S32K344_REFERENCE_BINDING
    record->observation.clockReady = AudioClock_IsLocked() != FALSE;
    record->observation.codecReady = AudioCodec_IsConfigured() != FALSE;
    record->observation.transportReady = AudioDmaTransport_IsDmaReady() != FALSE &&
        AudioDmaTransport_IsSaiReady() != FALSE;
    record->observation.transportRunning = AudioDmaTransport_IsStarted() != FALSE;
    /* Readiness is configuration evidence only. Existing vendor Start/Service
     * cannot establish operation attribution, a safe cyclic write window or
     * STOP fence, and therefore are never called by this guarded adapter. */
#endif
#ifdef VSS_HOST_TEST
    if (syntheticEnabled) {
        record->observation.state = VSS_AUDIO_PREPARED;
        record->observation.prepared = true;
        record->observation.productionAllowed = true;
        record->observation.physicalHold = false;
    }
#endif
    /* Product: no proven existing A/B write window, asset-plan association,
     * per-operation activation or physical STOP. No lower effect is issued. */
    return record->observation.state;
}

VssAudioState VssAudio_Start(const VssAudioKey *key, const VssTimeEvidence *now)
{
    AudioRecord *record = find_record(key);
    if (record == NULL || !record_time(record, now)) return VSS_AUDIO_REJECTED;
    if (record->observation.futurePcmBlocked || record->observation.outputClosed)
        return VSS_AUDIO_HOLD;
    if (record->observation.startSubmitted) return record->observation.state;
    if (!record->observation.prepared || record->observation.physicalHold)
        return VSS_AUDIO_HOLD;
#ifdef VSS_HOST_TEST
    if (syntheticEnabled) {
        record->observation.startSubmitted = true;
        record->observation.state = VSS_AUDIO_SUBMITTED;
        ++record->observation.revision;
    }
#endif
    return record->observation.state;
}

VssAudioState VssAudio_Stop(const VssAudioKey *key, const VssTimeEvidence *now)
{
    AudioRecord *record = find_record(key);
    size_t i;
    if (record == NULL) return VSS_AUDIO_REJECTED;
    /* Session-wide logical production fence precedes any lower cleanup.
     * This does not assert the physical future-output fence or output end. */
    for (i = 0u; i < recordCount; ++i) {
        VssAudioObservation *observation = &records[i].observation;
        if (observation->key.session == key->session &&
            observation->key.origin == key->origin) {
            records[i].sessionStopped = true;
            if (!observation->futurePcmBlocked) {
                observation->productionAllowed = false;
                observation->futurePcmBlocked = true;
                ++observation->revision;
            }
            observation->state = observation->physicalHold ?
                VSS_AUDIO_HOLD : VSS_AUDIO_CLEANUP_PENDING;
        }
    }
    return record_time(record, now) ? record->observation.state : VSS_AUDIO_HOLD;
}

bool VssAudio_Read(const VssAudioKey *key, VssAudioObservation *out)
{
    const AudioRecord *record = find_record(key);
    if (record == NULL || out == NULL) return false;
    *out = record->observation;
    return true;
}

void VssAudio_Poll(VssAudioOutput *out, size_t capacity, size_t *written)
{
    size_t count;
    size_t i;
    if (written == NULL || capacity > VSS_AUDIO_OUTPUT_CAPACITY ||
        (capacity > 0u && out == NULL)) return;
    *written = 0u;
    count = capacity < pendingCount ? capacity : pendingCount;
    for (i = 0u; i < count; ++i) out[i] = pending[i];
    for (i = count; i < pendingCount; ++i) pending[i - count] = pending[i];
    pendingCount -= count;
    *written = count;
}

bool VssAudio_ReadPcm(uint8_t buffer, VssAudioPcmObservation *out)
{
    if (buffer > 1u || out == NULL) return false;
    *out = pcm[buffer];
    return true;
}

#ifdef VSS_HOST_TEST
static bool same_time(const VssFactTime *a, const VssFactTime *b)
{
    return a->domain == b->domain && a->epoch == b->epoch &&
        a->earliest == b->earliest && a->latest == b->latest &&
        a->continuity == b->continuity;
}
static bool reject_observation(AudioRecord *record, VssAudioCoverage coverage)
{
    if (record != NULL) {
        if (record->observation.rejectedObservations != UINT32_MAX)
            ++record->observation.rejectedObservations;
        if (coverage == VSS_AUDIO_COVERAGE_LOST) {
            record->sessionStopped = true;
            record->observation.coverage = coverage;
            record->observation.productionAllowed = false;
            record->observation.futurePcmBlocked = true;
            record->observation.physicalHold = true;
        }
        ++record->observation.revision;
    }
    return false;
}
void VssAudio_TestReset(void)
{
    memset(records, 0, sizeof(records));
    memset(pending, 0, sizeof(pending));
    memset(pcm, 0, sizeof(pcm));
    memset(pcmSamples, 0, sizeof(pcmSamples));
    memset(pcmWindows, 0, sizeof(pcmWindows));
    recordCount = 0u;
    pendingCount = 0u;
    syntheticEnabled = false;
}
void VssAudio_TestEnable(bool enable)
{
    syntheticEnabled = enable;
}
bool VssAudio_TestActivate(const VssAudioKey *key, const VssFactTime *activationAt)
{
    AudioRecord *record = find_record(key);
    if (!syntheticEnabled || record == NULL ||
        !record_time(record, activationAt) || !record->observation.startSubmitted ||
        record->observation.futurePcmBlocked) return false;
    if (record->observation.activationPresent)
        return same_time(activationAt, &record->observation.activationAt);
    record->observation.activationPresent = true;
    record->observation.activationAt = *activationAt;
    ++record->observation.revision;
    return true;
}
bool VssAudio_TestEndSegment(const VssAudioKey *key)
{
    AudioRecord *record = find_record(key);
    if (!syntheticEnabled || record == NULL || record->sessionStopped)
        return false;
    record->observation.productionAllowed = false;
    record->observation.futurePcmBlocked = true;
    record->observation.state = VSS_AUDIO_CLEANUP_PENDING;
    ++record->observation.revision;
    return true;
}
bool VssAudio_TestObserve(const VssAudioTestEvidence *evidence)
{
    AudioRecord *record;
    VssAudioOutput output;
    size_t index;
    if (!syntheticEnabled || evidence == NULL) return false;
    record = find_record(&evidence->key);
    if (record == NULL) return false;
    if (evidence->kind < VSS_AUDIO_OUTPUT_START || evidence->kind > VSS_AUDIO_OUTPUT_TERMINATED ||
        evidence->fact == 0u || !evidence->originalScopeMatched ||
        evidence->coverage != VSS_AUDIO_COVERAGE_SUFFICIENT ||
        record->observation.coverage == VSS_AUDIO_COVERAGE_LOST ||
        !record_time(record, &evidence->occurredAt) ||
        !record->observation.startSubmitted || !record->observation.activationPresent ||
        !same_time(&evidence->activationAt, &record->observation.activationAt))
        return reject_observation(record, evidence->coverage);
    index = (size_t)evidence->kind;
    if (evidence->kind == VSS_AUDIO_OUTPUT_START) {
        if (!evidence->observedAfterActivation ||
            evidence->occurredAt.earliest <= evidence->activationAt.latest ||
            record->factPresent[VSS_AUDIO_OUTPUT_NO_START])
            return reject_observation(record, evidence->coverage);
    } else {
        if (!evidence->futureOutputBlocked || !record->observation.futurePcmBlocked ||
            evidence->occurredAt.earliest < evidence->activationAt.latest)
            return reject_observation(record, evidence->coverage);
        if (evidence->kind == VSS_AUDIO_OUTPUT_NO_START &&
            (!evidence->pastNoOutputProven || record->factPresent[VSS_AUDIO_OUTPUT_START]))
            return reject_observation(record, evidence->coverage);
        if (evidence->kind == VSS_AUDIO_OUTPUT_TERMINATED && !evidence->residualOutputEnded)
            return reject_observation(record, evidence->coverage);
    }
    memset(&output, 0, sizeof(output));
    output.key = evidence->key;
    output.kind = evidence->kind;
    output.fact = evidence->fact;
    output.occurredAt = evidence->occurredAt;
    output.activationAt = evidence->activationAt;
    output.coverage = evidence->coverage;
    output.futureBlocked = evidence->futureOutputBlocked;
    output.synthetic = true;
    if (record->factPresent[index]) {
        const VssAudioOutput *old = &record->facts[index];
        return old->fact == output.fact && same_time(&old->occurredAt, &output.occurredAt) &&
            old->futureBlocked == output.futureBlocked;
    }
    if (pendingCount == VSS_AUDIO_OUTPUT_CAPACITY)
        return reject_observation(record, VSS_AUDIO_COVERAGE_LOST);
    record->facts[index] = output;
    record->factPresent[index] = true;
    pending[pendingCount++] = output;
    record->observation.coverage = VSS_AUDIO_COVERAGE_SUFFICIENT;
    if (evidence->kind != VSS_AUDIO_OUTPUT_START) record->observation.outputClosed = true;
    ++record->observation.revision;
    return true;
}
bool VssAudio_TestCleanup(const VssAudioKey *key, bool providerReleased,
                          bool pcmReleased, bool lateProtected)
{
    AudioRecord *record = find_record(key);
    size_t i;
    if (!syntheticEnabled || record == NULL || !record->observation.futurePcmBlocked)
        return false;
    if (pcmReleased) {
        for (i = 0u; i < 2u; ++i) {
            if (same_key(&pcm[i].key, key) && pcm[i].usage != VSS_PCM_UNBOUND &&
                pcm[i].usage != VSS_PCM_CPU_WRITABLE) return false;
        }
    }
    record->observation.providerReleased |= providerReleased;
    record->observation.pcmReleased |= pcmReleased;
    record->observation.lateProtected |= lateProtected;
    ++record->observation.revision;
    return true;
}
static bool safe_window(const VssAudioTestWriteWindow *window,
                        const VssTimeEvidence *now)
{
    return window != NULL && valid_key(&window->key) && window->buffer <= 1u &&
        window->cycle != 0u && window->coverage == VSS_AUDIO_COVERAGE_SUFFICIENT &&
        window->oldReferencesEnded && window->oldContentsCannotRecur &&
        window->memoryCoherent && valid_time(now, window->key.origin) &&
        valid_time(&window->opensAt, window->key.origin) &&
        valid_time(&window->closesBefore, window->key.origin) &&
        now->epoch == window->opensAt.epoch && now->epoch == window->closesBefore.epoch &&
        now->earliest >= window->opensAt.latest &&
        window->writeBudget != 0u && now->latest <= UINT64_MAX - window->writeBudget &&
        now->latest + window->writeBudget < window->closesBefore.earliest;
}
bool VssAudio_TestBindPcm(const VssAudioKey *key, uint8_t buffer,
                         int16_t *samples, size_t frames,
                         const VssAudioTestWriteWindow *window,
                         const VssTimeEvidence *now)
{
    AudioRecord *record = find_record(key);
    if (!syntheticEnabled || record == NULL || buffer > 1u || samples == NULL ||
        frames != VSS_AUDIO_PCM_FRAMES || !safe_window(window, now) ||
        window->buffer != buffer || !same_key(key, &window->key) ||
        !record->observation.productionAllowed || pcm[buffer].usage != VSS_PCM_UNBOUND)
        return false;
    pcm[buffer].key = *key;
    pcm[buffer].buffer = buffer;
    pcm[buffer].cycle = window->cycle;
    pcm[buffer].usage = VSS_PCM_CPU_WRITABLE;
    pcmSamples[buffer] = samples;
    pcmWindows[buffer] = *window;
    return true;
}
bool VssAudio_TestHandoff(const VssAudioKey *key, uint8_t buffer, uint64_t cycle,
                         uint32_t validFrames, bool lastForSegment,
                         bool submittedWithPossibleAccess,
                         const VssTimeEvidence *now)
{
    AudioRecord *record = find_record(key);
    if (!syntheticEnabled || record == NULL || buffer > 1u || cycle == 0u ||
        !same_key(key, &pcm[buffer].key) || pcm[buffer].cycle != cycle ||
        pcm[buffer].usage != VSS_PCM_CPU_WRITABLE || pcmSamples[buffer] == NULL ||
        validFrames == 0u || validFrames > VSS_AUDIO_PCM_FRAMES ||
        !record->observation.productionAllowed || !safe_window(&pcmWindows[buffer], now))
        return false;
    pcm[buffer].validFrames = validFrames;
    pcm[buffer].lastForSegment = lastForSegment;
    pcm[buffer].consumed = false;
    pcm[buffer].usage = VSS_PCM_HANDOFF_PENDING; /* before any simulated effect */
    pcm[buffer].usage = submittedWithPossibleAccess ?
        VSS_PCM_HW_PROTECTED : VSS_PCM_RETURN_PENDING;
    /* A failed lower return is not proof of no access. */
    return true;
}
bool VssAudio_TestConsumed(const VssAudioKey *key, uint8_t buffer, uint64_t cycle)
{
    if (!syntheticEnabled || !valid_key(key) || buffer > 1u ||
        !same_key(key, &pcm[buffer].key) || pcm[buffer].cycle != cycle ||
        (pcm[buffer].usage != VSS_PCM_HW_PROTECTED &&
         pcm[buffer].usage != VSS_PCM_RETURN_PENDING)) return false;
    pcm[buffer].consumed = true;
    return true; /* consumption does not return CPU ownership */
}
bool VssAudio_TestReturn(const VssAudioTestWriteWindow *window,
                        const VssTimeEvidence *now)
{
    VssAudioPcmObservation *slot;
    if (!syntheticEnabled || !safe_window(window, now)) return false;
    slot = &pcm[window->buffer];
    if (!same_key(&slot->key, &window->key) || slot->cycle != window->cycle ||
        (slot->usage != VSS_PCM_HW_PROTECTED && slot->usage != VSS_PCM_RETURN_PENDING))
        return false;
    if (slot->cycle == UINT64_MAX) return false;
    slot->usage = VSS_PCM_CPU_WRITABLE;
    ++slot->cycle; /* old protected key ended; new cycle must not alias */
    slot->validFrames = 0u;
    slot->lastForSegment = false;
    slot->consumed = false;
    pcmWindows[window->buffer] = *window;
    pcmWindows[window->buffer].cycle = slot->cycle;
    return true;
}
#endif
