#ifndef VSS_PLAYBACK_H
#define VSS_PLAYBACK_H
#include "vss_policy_store.h"
#include "vss_audio_backend.h"

/* Private C99 binding of the logical Core contracts, not a wire/target ABI.
 * One serialized owner. No retained caller pointers, heap, tasks or queues.
 * Backend capacity is a lifetime protection bound, not a product repeat rule. */
#define VSS_PB_ATTEMPTS VSS_AUDIO_CAPACITY
#define VSS_PB_NOTICES 2u
#define VSS_PB_DIAGNOSTICS 16u

typedef enum { VSS_PB_IDLE, VSS_PB_PREPARING, VSS_PB_START_IN_FLIGHT,
               VSS_PB_ACTIVE, VSS_PB_STOPPING, VSS_PB_ABORTING,
               VSS_PB_QUARANTINED, VSS_PB_RETIRED } VssPlaybackPhase;
typedef enum { VSS_START_PENDING_OR_UNKNOWN, VSS_START_ACTUAL_CONFIRMED,
               VSS_START_NO_START_CONFIRMED,
               VSS_START_UNCERTAIN_FINAL } VssStartKnowledge;
typedef enum { VSS_REQUEST_REJECTED, VSS_REQUEST_WAIT,
               VSS_REQUEST_KEPT, VSS_REQUEST_ADOPTED } VssRequestDisposition;
typedef enum { VSS_STOP_NONE, VSS_STOP_POLICY, VSS_STOP_CONDITION,
               VSS_STOP_FAILURE } VssTerminationReason;
typedef enum { VSS_PB_DIAG_ATTRIBUTION, VSS_PB_DIAG_COVERAGE,
               VSS_PB_DIAG_TIME_UNCERTAIN, VSS_PB_DIAG_VIOLATION,
               VSS_PB_DIAG_CAPACITY, VSS_PB_DIAG_FINAL_TEST_ONLY } VssPlaybackDiagCode;
typedef struct {
    VssPlaybackDiagCode code;
    VssAudioKey key;
    VssFactTime occurredAt;
    VssAudioOutput evidence; /* preserves a rejected physical observation */
    bool evidencePresent;
} VssPlaybackDiagnostic;
typedef struct {
    bool present;
    uint64_t session;
    VssAudioKey attempt;
    VssCandidateObservation candidate;
    VssPlanDescriptor plan;
    VssPlaybackPhase phase;
    VssStartKnowledge startKnowledge;
    bool notificationPending;
    bool publicationPending;
    bool startAllowed;
    bool wholeSafe;
    bool diagnosticsLost;
    uint32_t diagnosticsLostCount;
    size_t attempts;
    size_t cue;
    uint32_t repeat;
    uint64_t revision;
} VssPlaybackObservation;

VssRequestDisposition Playback_RequestTransition(
    const VssSelectionDecision *decision, const uint64_t *terminateSession,
    VssTerminationReason reason, const VssCandidateObservation *candidates,
    size_t candidateCount, const VssReadBasis *basis,
    const VssTimeEvidence *time);

/* Independent typed request/output/cleanup IN and four independent OUT lanes.
 * Current restrictions/resources are protected in basis/plan and backend
 * observations in this private slice; complete product R6 binding is OPEN. */
void Playback_Advance(
    const VssAudioObservation *requests, size_t requestCount,
    const VssAudioOutput *outputs, size_t outputCount,
    const VssAudioObservation *cleanup, size_t cleanupCount,
    const VssCandidateObservation *candidates, size_t candidateCount,
    const VssReadBasis *basis, const VssTimeEvidence *time,
    VssPlaybackObservation *observation, size_t observationCapacity,
    size_t *observationWritten, VssPlaybackFact *facts, size_t factCapacity,
    size_t *factWritten, VssFactApplication *applications,
    size_t applicationCapacity, size_t *applicationWritten,
    VssPlaybackDiagnostic *diagnostics, size_t diagnosticCapacity,
    size_t *diagnosticWritten);
bool Playback_Read(VssPlaybackObservation *out);
void Playback_ReadPolicyCurrent(VssPolicyCurrent *out);
void Playback_ReadStoreSnapshot(VssOrigin origin, VssStorePlaybackSnapshot *out);
/* STREAM owner poll -> PB. No evidence constructed from return/status flags. */
void Playback_Service(const VssCandidateObservation *candidates, size_t count,
                      const VssReadBasis *basis, const VssTimeEvidence *time);

typedef enum { VSS_OPPORTUNITY_INITIAL, VSS_OPPORTUNITY_INPUT_OR_FOLLOW_UP,
               VSS_OPPORTUNITY_TIME_CHECK } VssProcessingOpportunity;
typedef enum { VSS_FLOW_RECOLLECT_OBSERVATIONS, VSS_FLOW_FOLLOW_UP_REQUIRED,
               VSS_FLOW_SETTLED } VssFlowProgress;
VssFlowProgress Flow_Process(VssProcessingOpportunity opportunity,
                             const VssTimeEvidence *time);
/* Private synchronous typed owner entry, not new public Core or Network ABI.
 * Completes the entire bundle before exactly one Flow selection opportunity. */
VssInputApplication VssFlow_OnEvent(const VssInputEvidence *event,
                                   const VssTimeEvidence *time);
VssWarningBundleResult VssFlow_OnWarning(const VssWarningFrame *frame,
                                       const VssTimeEvidence *time);

#ifdef VSS_HOST_TEST
void Playback_TestReset(void);
/* Explicit synthetic TBD-FINAL criterion, never timeout/coverage inferred. */
bool Playback_TestQualifiedUncertainFinal(uint64_t session,
                                         const VssTimeEvidence *time);
void Flow_TestReset(void);
bool Flow_TestCatalog(const VssPlanDescriptor *plans, size_t count,
                       bool startsAllowed);
uint64_t Flow_TestSelectionCount(void);
#endif
#endif
