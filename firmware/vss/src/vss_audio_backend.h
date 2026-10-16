#ifndef VSS_AUDIO_BACKEND_H
#define VSS_AUDIO_BACKEND_H

/* Private STREAM/TX boundary. Serialized main owner only; no target ABI.
 * Production has no physical-proof injection. The archived continuous SAI
 * backend lacks proven attribution, STOP and cyclic write-window evidence,
 * so the target path remains HOLD. Test evidence is compile-time isolated. */
#include "vss_input_store.h"

#define VSS_AUDIO_CAPACITY 8u
#define VSS_AUDIO_OUTPUT_CAPACITY 24u
#define VSS_AUDIO_PCM_FRAMES 1024u
typedef struct {
    uint64_t session;
    uint64_t attempt;
    uint64_t operation;
    uint32_t segment;
    VssOrigin origin;
} VssAudioKey;
typedef enum { VSS_AUDIO_REJECTED, VSS_AUDIO_HOLD, VSS_AUDIO_PREPARED,
               VSS_AUDIO_SUBMITTED, VSS_AUDIO_CLEANUP_PENDING } VssAudioState;
typedef enum { VSS_AUDIO_COVERAGE_INSUFFICIENT, VSS_AUDIO_COVERAGE_LOST,
               VSS_AUDIO_COVERAGE_SUFFICIENT } VssAudioCoverage;
typedef enum { VSS_AUDIO_OUTPUT_START, VSS_AUDIO_OUTPUT_NO_START,
               VSS_AUDIO_OUTPUT_TERMINATED } VssAudioOutputKind;
typedef struct {
    VssAudioKey key;
    VssAudioOutputKind kind;
    uint64_t fact;
    VssFactTime occurredAt;
    VssFactTime activationAt;
    VssAudioCoverage coverage;
    bool futureBlocked;
    bool synthetic;
} VssAudioOutput;
typedef struct {
    VssAudioKey key;
    VssAudioState state;
    bool prepared;
    bool startSubmitted;
    bool activationPresent;
    VssFactTime activationAt;
    bool productionAllowed;
    bool futurePcmBlocked;
    bool providerReleased;
    bool pcmReleased;
    bool lateProtected;
    bool outputClosed;
    bool physicalHold;
    bool clockReady;
    bool codecReady;
    bool transportReady;
    bool transportRunning;
    VssAudioCoverage coverage;
    uint32_t rejectedObservations;
    uint64_t revision;
} VssAudioObservation;
typedef enum { VSS_PCM_UNBOUND, VSS_PCM_CPU_WRITABLE,
               VSS_PCM_HANDOFF_PENDING, VSS_PCM_HW_PROTECTED,
               VSS_PCM_RETURN_PENDING } VssAudioPcmUsage;
typedef struct {
    VssAudioKey key;
    uint8_t buffer; /* A=0, B=1 only. */
    uint64_t cycle;
    VssAudioPcmUsage usage;
    uint32_t validFrames;
    bool consumed;
    bool lastForSegment;
} VssAudioPcmObservation;

/* assetId is an explicit original AudioAsset ID (0..7), never a semantic
 * default. This private slice does not invent missing product asset maps. */
VssAudioState VssAudio_Prepare(const VssAudioKey *key, uint8_t assetId,
                              const VssTimeEvidence *now);
VssAudioState VssAudio_Start(const VssAudioKey *key, const VssTimeEvidence *now);
VssAudioState VssAudio_Stop(const VssAudioKey *key, const VssTimeEvidence *now);
bool VssAudio_Read(const VssAudioKey *key, VssAudioObservation *out);
/* Invalid/full OUT never discards pending facts; capacity=0 is permitted. */
void VssAudio_Poll(VssAudioOutput *out, size_t capacity, size_t *written);
bool VssAudio_ReadPcm(uint8_t buffer, VssAudioPcmObservation *out);

#ifdef VSS_HOST_TEST
/* SYNTHETIC_TEST_EVIDENCE only. None of these symbols exists in product. */
typedef struct {
    VssAudioKey key;
    VssAudioOutputKind kind;
    uint64_t fact;
    VssFactTime occurredAt;
    VssFactTime activationAt;
    VssAudioCoverage coverage;
    bool originalScopeMatched;
    bool observedAfterActivation;
    bool pastNoOutputProven;
    bool residualOutputEnded;
    bool futureOutputBlocked;
} VssAudioTestEvidence;
typedef struct {
    VssAudioKey key;
    uint8_t buffer;
    uint64_t cycle;
    VssFactTime opensAt;
    VssFactTime closesBefore;
    VssAudioCoverage coverage;
    bool oldReferencesEnded;
    bool oldContentsCannotRecur;
    bool memoryCoherent;
    uint64_t writeBudget; /* synthetic measured WCET+IRQ margin, same time units */
} VssAudioTestWriteWindow;
void VssAudio_TestReset(void);
void VssAudio_TestEnable(bool enable);
bool VssAudio_TestActivate(const VssAudioKey *key, const VssFactTime *activationAt);
/* Current segment EOS/production fence only: not a Session STOP or a physical
 * output/cleanup fact. Later normal cue requires this scope's closure+cleanup. */
bool VssAudio_TestEndSegment(const VssAudioKey *key);
bool VssAudio_TestObserve(const VssAudioTestEvidence *evidence);
bool VssAudio_TestCleanup(const VssAudioKey *key, bool providerReleased,
                          bool pcmReleased, bool lateProtected);
bool VssAudio_TestBindPcm(const VssAudioKey *key, uint8_t buffer,
                         int16_t *samples, size_t frames,
                         const VssAudioTestWriteWindow *window,
                         const VssTimeEvidence *now);
bool VssAudio_TestHandoff(const VssAudioKey *key, uint8_t buffer, uint64_t cycle,
                         uint32_t validFrames, bool lastForSegment,
                         bool submittedWithPossibleAccess,
                         const VssTimeEvidence *now);
bool VssAudio_TestConsumed(const VssAudioKey *key, uint8_t buffer, uint64_t cycle);
bool VssAudio_TestReturn(const VssAudioTestWriteWindow *window,
                        const VssTimeEvidence *now);
#endif
#endif
