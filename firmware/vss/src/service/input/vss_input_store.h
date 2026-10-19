#ifndef VSS_INPUT_STORE_H
#define VSS_INPUT_STORE_H

/* INPUT/STORE boundary inside VSS, not a Network or wire ABI. All calls are
 * serialized by the same owner, never from an ISR or a reentrant callback.
 * Non-NULL pointers must name live, correctly aligned objects. No function
 * retains a caller pointer.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../infra/vss_config.h"

#define VSS_LEDGER_CAPACITY 128u
#define VSS_DIAGNOSTIC_CAPACITY 16u
#define VSS_EVENT_CONTEXTS 10u /* five event signals x PRODUCT/TEST */
#define VSS_WARNING_CONTEXTS 8u
#define VSS_INPUT_CONTEXTS (VSS_EVENT_CONTEXTS + VSS_WARNING_CONTEXTS)
#define VSS_WARNING_LANES 4u

typedef enum { VSS_PRODUCT, VSS_TEST } VssOrigin;
typedef enum { VSS_SOURCE_DOMAIN, VSS_SOURCE_OTHER } VssSource;
typedef enum { VSS_CONTINUITY_COMPARABLE, VSS_CONTINUITY_UNKNOWN,
               VSS_CONTINUITY_LOST } VssContinuity;
typedef enum { VSS_INPUT_VALID, VSS_INPUT_INVALID, VSS_INPUT_STALE,
               VSS_INPUT_UNASSESSED } VssInputQuality;
typedef enum { VSS_SIGNAL_WELCOME, VSS_SIGNAL_GOODBYE,
               VSS_SIGNAL_LOCK_COMPLETE, VSS_SIGNAL_UNLOCK_COMPLETE,
               VSS_SIGNAL_LOCK_ERROR, VSS_SIGNAL_ANTI_PINCH,
               VSS_SIGNAL_OCCUPANT_HAZARD, VSS_SIGNAL_REAR_RISK,
               VSS_SIGNAL_REAR_ACTIVATION } VssInputSignal;
typedef enum { VSS_VALUE_EVENT, VSS_VALUE_UNSUPPORTED, VSS_VALUE_ACTIVE,
               VSS_VALUE_CLEAR, VSS_VALUE_EMERGENCY, VSS_VALUE_CAUTION,
               VSS_VALUE_ENABLED, VSS_VALUE_DISABLED } VssInputValue;
typedef enum { VSS_AVAILABLE, VSS_LIMITED, VSS_UNAVAILABLE,
               VSS_AVAILABILITY_NOT_PROVIDED } VssAvailability;
typedef enum { VSS_STORE_NOT_SUBMITTED, VSS_STORE_ACCEPTED,
               VSS_STORE_DUPLICATE, VSS_STORE_UPDATED,
               VSS_STORE_REJECTED } VssStoreAdmission;
typedef enum { VSS_VALIDATION_APPLICABLE, VSS_VALIDATION_QUALITY_CHANGE,
               VSS_VALIDATION_REJECTED, VSS_VALIDATION_NO_CHANGE } VssInputValidation;
typedef enum { VSS_OCCURRENCE_PENDING, VSS_OCCURRENCE_STARTED,
               VSS_OCCURRENCE_COMPLETED, VSS_OCCURRENCE_INTERRUPTED,
               VSS_OCCURRENCE_EXPIRED, VSS_OCCURRENCE_UNCERTAIN_FINAL } VssOccurrenceState;

typedef struct {
    VssOrigin domain;
    uint32_t epoch;
    uint64_t earliest;
    uint64_t latest;
    VssContinuity continuity;
} VssFactTime;
/* Same representation; separate names retain the current/fact distinction. */
typedef VssFactTime VssTimeEvidence;
typedef struct {
    VssOrigin origin;
    VssSource source;
    uint32_t generation;
    uint32_t occurrenceId;
} VssOccurrenceKey;

typedef struct {
    VssOccurrenceKey key;
    uint32_t deliveryOrder; /* transport ordering only */
    VssFactTime original;
    uint32_t receivedUseLimit; /* preserve even when clamped */
    uint32_t useLimit;
    uint64_t validUntil; /* owned value; always present for this event slice */
    uint32_t relatedCommand; /* zero = absent */
    VssAvailability availability;
    /* Event reason/semantic ordering are absent, not numeric wire defaults. */
} VssInputMeta;

typedef struct {
    VssInputSignal signal;
    VssInputValue value;
    VssInputQuality quality;
    const VssOccurrenceKey *occurrence; /* required; synchronously borrowed */
    uint32_t deliveryOrder;
    VssFactTime receivedAt;
    uint32_t ageLowMs;
    uint32_t ageHighMs;
    bool ageComplete; /* all upstream delay and timestamp error bounded */
    uint32_t useLimitMs;
    uint32_t relatedCommand;
    VssAvailability availability;
    VssFactTime lossAt; /* only explicit INVALID/STALE input uses this */
} VssInputEvidence;

typedef struct { VssInputSignal signal; VssInputMeta meta; } VssOneShotEvent;
/* Named semantic tags only: none of these are wire enum encodings. */
typedef enum { VSS_EVIDENCE_WINDOW, VSS_EVIDENCE_PI_VIA_CIS,
               VSS_EVIDENCE_CIS_REAR, VSS_EVIDENCE_DOMAIN_ACTIVATION } VssEvidenceSource;
typedef enum { VSS_REASON_NONE, VSS_REASON_NO_DATA, VSS_REASON_DATA_INVALID,
               VSS_REASON_STALE, VSS_REASON_UNSUPPORTED } VssWarningReason;
typedef enum { VSS_MEANING_CONFIRMED, VSS_MEANING_UNCONFIRMED,
               VSS_MEANING_UNSUPPORTED } VssWarningMeaning;
typedef struct {
    VssOrigin origin;
    uint32_t domainBoot;
    uint32_t deliveryOrder;
    VssEvidenceSource evidenceSource;
    uint32_t evidenceBoot;
    uint32_t evidenceSequence;
    VssFactTime original;
    uint32_t useLimit;
    uint64_t validUntil;
    VssWarningReason reason;
    VssAvailability availability;
} VssWarningMeta;
typedef struct {
    VssInputSignal signal;
    VssInputValue value;
    VssWarningMeta meta;
    VssTimeEvidence evaluatedAt;
} VssStateUpdate;
typedef struct {
    VssInputSignal signal;
    VssInputQuality quality;
    VssInputMeta basis;
    VssFactTime lossAt;
    /* Used only for Warning signals; basis above remains the Event variant. */
    VssWarningMeta warningBasis;
    bool provenanceLost; /* source/generation/clock fence: Hold cannot authorize */
} VssQualityChange;
typedef struct {
    VssInputValidation validation;
    bool contextApplied;
    VssStoreAdmission storeAdmission;
} VssInputApplication;
typedef struct {
    VssInputMeta appliedMeta;
    VssInputQuality quality;
    uint64_t revision;
} VssInputContextObservation;

typedef enum { VSS_DIAG_ARGUMENT, VSS_DIAG_GENERATION, VSS_DIAG_ORDER,
               VSS_DIAG_TIME, VSS_DIAG_VALUE, VSS_DIAG_STORE } VssDiagnosticCode;
typedef struct {
    VssDiagnosticCode code;
    bool keyPresent;
    VssOccurrenceKey key;
    VssInputSignal signal;
    uint32_t deliveryOrder;
    uint32_t receivedUseLimit;
    uint32_t useLimit;
    VssFactTime observedAt;
    bool warningPresent;
    VssOrigin warningOrigin;
    uint32_t domainBoot;
    VssEvidenceSource evidenceSource;
    uint32_t evidenceBoot;
    uint32_t evidenceSequence;
    VssInputQuality rawQuality;
    VssInputValue rawValue;
    VssWarningReason reason;
    VssAvailability availability;
    VssWarningMeaning meaning;
} VssDiagnosticEvidence;
typedef struct {
    size_t pending;
    bool lossLatched;
    uint32_t lossCount;
    bool summaryPresent[VSS_INPUT_CONTEXTS + 1u]; /* last slot = unattributed */
    VssDiagnosticEvidence firstLost[VSS_INPUT_CONTEXTS + 1u];
    VssDiagnosticEvidence latestLost[VSS_INPUT_CONTEXTS + 1u];
} VssDiagnosticObservation;

typedef struct {
    VssInputValue value;
    VssInputQuality quality; /* UNASSESSED is the typed NO_DATA observation */
    VssWarningReason reason;
    VssAvailability availability;
    VssWarningMeaning meaning;
    VssEvidenceSource evidenceSource;
    uint32_t evidenceBoot;
    uint32_t evidenceSequence;
    uint32_t ageLowMs;
    uint32_t ageHighMs;
    bool ageComplete;
    bool provenanceProven; /* source boot and evaluation number both present */
    bool evaluationPresent; /* activation format0 => false; no raw decoder here */
    bool activationBasisProven; /* activation metadata present (same DOMAIN evaluation), not TX */
    VssFactTime lossAt;
} VssWarningEvidence;
typedef struct {
    VssOrigin origin;
    VssSource source;
    uint32_t domainBoot;
    uint32_t deliveryOrder;
    VssFactTime receivedAt;
    bool structureVerified; /* adapter obligation, not a CRC/DLC check */
    /* Fixed processing order: activation, rear risk, occupant, anti. */
    VssWarningEvidence lanes[VSS_WARNING_LANES];
} VssWarningFrame;
typedef struct {
    VssInputValidation frameValidation;
    bool complete; /* all four lane outcomes available; never an all-success ACK */
    VssInputApplication lanes[VSS_WARNING_LANES];
} VssWarningBundleResult;
typedef struct {
    bool lastValidPresent;
    VssWarningMeta appliedMeta;
    VssInputQuality quality;
    VssWarningEvidence latestEvidence;
    VssInputApplication lastApplication;
    uint64_t revision;
} VssWarningContextObservation;
typedef enum { VSS_CANDIDATE_NOT_APPLICABLE, VSS_CANDIDATE_APPLICABLE,
               VSS_CANDIDATE_EXPIRED, VSS_CANDIDATE_UNCERTAIN } VssCandidateValidity;
typedef struct {
    bool lastValidPresent;
    VssInputValue lastValidValue;
    VssWarningMeta lastValidMeta;
    VssInputQuality quality;
    bool firstLossPresent;
    VssFactTime firstLossAt;
    bool holdAnchorPresent;
    uint64_t holdAnchor;
    bool holdExpired;
    bool removed;
    bool provenanceLost;
    bool inputIncomplete; /* a verified update failed; old value cannot authorize */
    VssCandidateValidity validity;
    bool newSessionAllowed; /* candidate only; no physical start/retirement proof */
    VssTimeEvidence evaluatedAt;
    uint64_t revision;
} VssWarningObservation;
typedef struct {
    bool riskApplicable;
    bool fencePresent;
    VssWarningMeta riskFence;
    uint64_t revision;
} VssRearObservation;

/* Normalized playback fact slot. Only PLAYBACK produces these; INPUT/STORE
 * acceptance never gives them physical meaning. */
typedef struct {
    bool present;
    uint32_t kind;
    uint64_t fact;
    VssOccurrenceKey occurrence;
    uint64_t session;
    uint64_t attempt;
    VssFactTime occurredAt;
    uint64_t basisRevision;
} VssReservedPlaybackFact;
typedef struct {
    VssOneShotEvent event;
    bool rejectedTombstone;
    bool replayAllowed;
    bool inputLossPresent;
    VssFactTime inputLossAt;
    VssOccurrenceState state;
    VssReservedPlaybackFact firstStartFact;
    VssReservedPlaybackFact finalFact;
    uint64_t revision; /* published after owner mutation, no hidden read writes */
} VssOccurrenceObservation;

/* One-time startup opening, per origin. The caller must have established
 * the DOMAIN generation after VSS synchronization; default closed. A clock
 * discontinuity closes admission for the rest of the VSS run; a generation
 * change is resumed only through Input_AcceptNewGeneration. Neither clears
 * the ledger. The ledger starts with the VSS run at clock 0 of the time
 * domain, whenever the opening happens: an event whose original time may lie
 * before clock 0 is not replayed (a run before this one may already have
 * played it); one that originated after it is replayable even when it
 * arrives as the message that opens INPUT. */
bool Input_OpenAfterSynchronization(VssOrigin origin, uint32_t domainBoot,
                                   const VssTimeEvidence *synchronizedAt);
/* A DOMAIN restart that the Network accepted: closes the current generation
 * of an opened origin (its pending events lose replay, its warnings lose
 * provenance), then opens newBoot with fresh Event/Warning order and receive
 * monitor. Refused for boot 0, for the current boot, for any of the last four
 * boots it replaced, before the first opening, and on a clock discontinuity.
 * Frames of those replaced boots are rejected afterwards without closing the
 * current one. A boot id is never reused by the sender. */
bool Input_AcceptNewGeneration(VssOrigin origin, uint32_t newBoot,
                               const VssTimeEvidence *at);
/* True while the origin's admission is open: opened for a DOMAIN generation
 * and not closed by a clock discontinuity. */
bool Input_IsOpen(VssOrigin origin);
VssInputApplication Input_Process(const VssInputEvidence *input,
                                 const VssTimeEvidence *time,
                                 VssDiagnosticEvidence *diagnostics,
                                 size_t capacity, size_t *written);
VssStoreAdmission Store_ApplyInput(const VssOneShotEvent *event,
                                 const VssStateUpdate *state,
                                 const VssQualityChange *quality);
bool Input_ReadDiagnostics(VssDiagnosticObservation *out);
bool Input_ReadContext(VssOrigin origin, VssInputSignal signal,
                       VssInputContextObservation *out);
bool Store_ReadOccurrence(const VssOccurrenceKey *key, VssOccurrenceObservation *out);
size_t Store_Count(void);
/* Read-only FIRST-start opportunity. Not actual/NO_START/retirement evidence. */
bool Store_FirstStartAllowed(const VssOccurrenceKey *key, const VssTimeEvidence *now);

/* INPUT-only rejection protection. Not an additional Core or Network API.
 * at is the processing time of the rejection; it is kept as the tombstone's
 * time so that the slot can be reclaimed once the occurrence is too old to be
 * accepted again (see Store_Reclaim). */
void Store_ProtectRejectedEvent(const VssOccurrenceKey *key, VssInputSignal signal,
                                const VssTimeEvidence *at);

/* Private typed adapter/test observations, NOT additional public Core APIs.
 * Callers may arbitrate only after this synchronous whole-bundle return. */
VssWarningBundleResult Input_ProcessWarningFrame(const VssWarningFrame *frame,
                                                const VssTimeEvidence *time);
bool Input_ReadWarningContext(VssOrigin origin, VssInputSignal signal,
                              VssWarningContextObservation *out);
/* Warning-only portion of the deadline work; playback facts are separate. */
void Store_AdvanceWarningDeadlines(const VssTimeEvidence *time);
bool Store_ReadWarning(VssOrigin origin, VssInputSignal signal, VssWarningObservation *out);
bool Store_ReadRear(VssOrigin origin, VssRearObservation *out);

#if VSS_CFG_HOST_TEST
void Input_TestReset(void);
void Store_TestReset(void);
void Store_TestSeedFacts(const VssOccurrenceKey *key, VssOccurrenceState state,
                        const VssReservedPlaybackFact *first,
                        const VssReservedPlaybackFact *final);
void Store_TestRejectWarningOnce(VssInputSignal signal);
#endif
#endif
