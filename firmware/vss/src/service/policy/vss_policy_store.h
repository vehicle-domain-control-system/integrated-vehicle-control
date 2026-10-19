#ifndef VSS_POLICY_STORE_H
#define VSS_POLICY_STORE_H
#include "../input/vss_input_store.h"

/* Internal serialized owner boundary. These capacities are implementation
 * bounds, not approved product pattern/retry/timeout values. */
#define VSS_CANDIDATE_CAPACITY (VSS_LEDGER_CAPACITY + 3u)
#define VSS_PLAN_CUES 4u
#define VSS_PLAN_REPEATS 8u
#define VSS_PLAYBACK_BINDINGS_CAPACITY 32u
#define VSS_PLAYBACK_FACT_CAPACITY 64u
typedef enum { VSS_MODE_EVENT, VSS_MODE_STATEFUL } VssCandidateMode;
typedef struct {
    VssOrigin origin;
    VssCandidateMode mode;
    VssOccurrenceKey key;
    VssInputSignal signal;
    VssInputValue value;
    VssCandidateValidity validity;
    bool newSessionAllowed;
    bool started;
    VssInputMeta eventMeta;
    VssWarningMeta warningMeta;
    uint64_t revision;
} VssCandidateObservation;
typedef struct { uint32_t asset; uint32_t gapMs; } VssCueDescriptor;
typedef struct {
    uint64_t key;
    VssInputSignal signal;
    VssInputValue value;
    uint32_t image;
    bool defined;
    bool available;
    size_t cueCount;
    VssCueDescriptor cues[VSS_PLAN_CUES];
    uint32_t repeatCount;
} VssPlanDescriptor;
typedef enum { VSS_READ_VALID, VSS_READ_INCOMPLETE, VSS_READ_CHANGED } VssReadValidity;
typedef struct {
    VssOrigin origin;
    VssTimeEvidence time;
    VssReadValidity validity;
    uint64_t storeRevision;
    uint64_t pbRevision;
    bool startsAllowed; /* current restriction/resource observation, not HW proof */
} VssReadBasis;
typedef struct {
    bool present;
    bool stopping;
    bool valid;
    uint64_t session;
    VssCandidateObservation candidate;
    VssPlanDescriptor plan;
} VssPolicyCurrent;
typedef enum { VSS_SELECTION_RECOLLECT, VSS_SELECTION_WAIT,
               VSS_SELECTION_KEEP, VSS_SELECTION_REPLACE,
               VSS_SELECTION_CHOOSE } VssSelectionAction;
typedef struct {
    VssSelectionAction action;
    bool candidatePresent;
    VssCandidateObservation candidate;
    VssPlanDescriptor plan;
    VssReadBasis basis;
} VssSelectionDecision;
typedef enum { VSS_FACT_FIRST_ACTUAL_START = 1, VSS_FACT_WHOLE_COMPLETED,
               VSS_FACT_WHOLE_INTERRUPTED, VSS_FACT_NO_START_REEVALUATE,
               VSS_FACT_START_OUTCOME_UNCERTAIN } VssPlaybackFactKind;
/* Producer is PB after normalization, never INPUT, HAL, raw IRQ or public wire.
 * kind asserts PB has separately checked physical coverage, historical
 * permission and whole-scope criteria. STORE cannot infer those from an enum. */
typedef VssReservedPlaybackFact VssPlaybackFact;
typedef enum { VSS_FACT_APPLIED, VSS_FACT_ALREADY_APPLIED,
               VSS_FACT_NOT_APPLIED } VssFactApplication;
typedef enum { VSS_STORE_STABLE, VSS_STORE_CHANGED,
               VSS_STORE_RECONFIRM_REQUIRED } VssStoreProgress;
typedef struct {
    VssOrigin origin;
    bool complete;
    bool notificationPending;
    uint64_t revision;
    uint64_t storeRevision;
} VssStorePlaybackSnapshot;

bool VssPolicy_SameCandidate(const VssCandidateObservation *a,
                             const VssCandidateObservation *b);
bool VssPolicy_ValidPlan(const VssPlanDescriptor *plan);
unsigned VssPolicy_Priority(VssInputSignal signal, VssInputValue value);
bool Store_ReadCandidates(const VssTimeEvidence *time, VssOrigin origin,
                          VssCandidateObservation *out, size_t capacity,
                          size_t *written);
uint64_t Store_ReadRevision(VssOrigin origin);
VssSelectionDecision Select_Choose(const VssCandidateObservation *candidates,
    size_t count, const VssPolicyCurrent *current,
    const VssPlanDescriptor *plans, size_t planCount, const VssReadBasis *basis);
VssFactApplication Store_ApplyPlaybackFact(const VssPlaybackFact *fact);
VssStoreProgress Store_AdvanceDeadlines(const VssTimeEvidence *time,
                                      const VssStorePlaybackSnapshot *playback);

/* Private PB-to-STORE lifetime hooks, not additional public logical Core APIs.
 * Must bind before effects. No history eviction/reuse; exhaustion blocks new
 * effects. MarkRetired consumes PB's independently sufficient scope proof. */
bool Store_BindPlayback(const VssOccurrenceKey *key, uint64_t session,
                       uint64_t attempt, uint64_t expectedRevision);
bool Store_MarkPlaybackAttempted(const VssOccurrenceKey *key, uint64_t session,
                                uint64_t attempt);
bool Store_MarkPlaybackRetired(const VssOccurrenceKey *key, uint64_t session,
                              uint64_t attempt, bool noStart);

/* STORE companion implementation only; expected revision rejects stale writes. */
bool StorePriv_ReadAt(size_t index, VssOccurrenceObservation *out);
bool StorePriv_ApplyRecord(const VssOccurrenceKey *key, uint64_t expectedRevision,
    VssOccurrenceState state, bool replayAllowed,
    const VssReservedPlaybackFact *first, const VssReservedPlaybackFact *final);
#if VSS_CFG_HOST_TEST
void StorePlayback_TestReset(void);
#endif
#endif
