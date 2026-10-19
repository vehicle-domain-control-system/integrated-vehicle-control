#ifndef VSS_PLAYBACK_H
#define VSS_PLAYBACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../policy/vss_policy_store.h"

/* One playback session at a time, driven by the selection decisions and by
 * the audio stream's facts. The flow owns the call order:
 *
 *   AudioStream_Service -> Playback_Advance -> (candidates) -> Select_Choose
 *   -> Playback_RequestTransition
 *
 * All calls run in the super-loop owner context. No caller pointer is kept.
 *
 * Session phases: IDLE -> STARTING (prepared and started at the stream, no
 * output yet) -> ACTIVE (the stream reported STARTED) -> STOPPING (STOP
 * requested, waiting for the stream's final fact) -> IDLE. A session ends
 * within the Playback_Advance call that sees its final fact; the last ended
 * session is kept in the observation.
 *
 * Event (one-shot) sessions are bound to their STORE record before the
 * stream is touched, and every output fact becomes a STORE playback fact:
 *   STARTED            FIRST_ACTUAL_START; when STORE refuses it (use limit
 *                      passed, input lost) the session is stopped and the
 *                      record closed with START_OUTCOME_UNCERTAIN
 *   COMPLETED          WHOLE_COMPLETED, binding retired
 *   STOPPED, started   WHOLE_INTERRUPTED, binding retired
 *   STOPPED, no start  NO_START_REEVALUATE, binding retired as no-start, so
 *                      the record can be selected again within its use limit;
 *                      dated no earlier than the session's adoption (the
 *                      stream dates it at the last boundary, which can lie
 *                      before the event was received)
 *   FAULT              as STOPPED, plus the fault observation below; a
 *                      decode failure before the start closes the record
 *                      instead (START_OUTCOME_UNCERTAIN): the asset is
 *                      fixed data, a retry would fail the same way
 * Stateful (warning) sessions loop at the stream and touch STORE only
 * through the candidate list: when the warning is no longer an applicable
 * candidate the session is stopped.
 *
 * Stop requests come from a REPLACE decision, a basis that no longer allows
 * starts, a stateful candidate that disappeared or changed value, and the
 * stream's FAULT.
 *
 * No session is adopted while the output has failed - a transmit fault
 * latched by the driver or the stream's latched output failure, checked at
 * the request itself: the request waits, and no start refusal is counted.
 *
 * Fact times: the stream reports the output boundary in system-clock ms,
 * rounded down to the tick (1.25 ms); the output itself follows the boundary
 * by less than 1 ms (interface FIFO and codec). A fact therefore covers
 * [at, at + PLAYBACK_FACT_SPAN_MS]. A final fact never starts before the
 * first-start fact ended. */

#define PLAYBACK_FACT_SPAN_MS 3u

typedef enum {
    PLAYBACK_IDLE = 0,
    PLAYBACK_STARTING,
    PLAYBACK_ACTIVE,
    PLAYBACK_STOPPING
} PlaybackPhase;

typedef enum {
    VSS_REQUEST_REJECTED = 0,   /* bad arguments, or the session could not start */
    VSS_REQUEST_WAIT,           /* nothing adopted; collect and decide again */
    VSS_REQUEST_KEPT,           /* the running session continues */
    VSS_REQUEST_ADOPTED         /* a new session started at the stream */
} VssRequestDisposition;

/* How the last session ended. */
typedef enum {
    PLAYBACK_END_NONE = 0,
    PLAYBACK_END_COMPLETED,       /* one-shot played to its end */
    PLAYBACK_END_INTERRUPTED,     /* stopped after it had started */
    PLAYBACK_END_NO_START,        /* stopped before any output; selectable again */
    PLAYBACK_END_START_UNCERTAIN, /* output began but STORE could not accept the start */
    PLAYBACK_END_STOPPED,         /* stateful session stopped */
    PLAYBACK_END_FAULT,           /* ended by a stream fault */
    PLAYBACK_END_NOT_STARTED      /* the stream refused to prepare or start it */
} PlaybackEnd;

typedef struct {
    bool present;                 /* a session exists (phase != IDLE) */
    PlaybackPhase phase;
    uint64_t session;             /* 1.. ; also the STORE attempt id */
    uint32_t token;               /* stream token of the session */
    VssCandidateObservation candidate;  /* of the current, else the last, session */
    VssPlanDescriptor plan;
    bool started;                 /* the stream reported STARTED and, for an event,
                                     STORE accepted it as the first start */
    bool stopRequested;
    bool startUncertain;          /* START_OUTCOME_UNCERTAIN was recorded */
    uint64_t revision;            /* +1 on every change */

    uint64_t lastStartedSession;  /* last session the stream reported STARTED for, 0 before any */
    uint64_t lastSession;         /* last session that ended, 0 before any */
    PlaybackEnd lastEnd;
    uint64_t lastEndAtMs;         /* time of its final fact */

    /* Fault observations, polled by the health service. Counters saturate. */
    uint32_t startRefusals;       /* stream refused Prepare or START */
    uint32_t storeRefusals;       /* STORE refused a binding, fact or retirement */
    uint32_t outputFaults;        /* stream FAULT with a transmit or refill fault */
    uint32_t assetFaults;         /* stream FAULT from decoding alone */
    uint32_t failedAssets;        /* bit (1 << asset id) per asset that failed to decode */
    bool outputFailed;            /* a transmit fault was seen: no new session is started */
    uint32_t lastFaultMask;
    uint64_t lastFaultAtMs;
    uint32_t strayFacts;          /* stream facts of another token */
} VssPlaybackObservation;

/* Idle, counters cleared. Called once at boot after AudioStream_Init. */
void Playback_Init(void);

/* Applies a selection decision made on basis at now. candidates are the
 * observations the decision was made from; when the basis is VALID they are
 * also checked for the stateful stop condition first. */
VssRequestDisposition Playback_RequestTransition(const VssSelectionDecision *decision,
                                                 const VssCandidateObservation *candidates,
                                                 size_t count, const VssReadBasis *basis,
                                                 const VssTimeEvidence *now);

/* Consumes the stream's facts and checks the stateful stop condition
 * against candidates (those of now's origin). With candidates NULL only the
 * facts are consumed: a flow that collects candidates once, after the facts,
 * leaves the check to Playback_RequestTransition. */
void Playback_Advance(const VssCandidateObservation *candidates, size_t count,
                      const VssTimeEvidence *now);

bool Playback_Read(VssPlaybackObservation *out);
void Playback_ReadPolicyCurrent(VssPolicyCurrent *out);
void Playback_ReadStoreSnapshot(VssOrigin origin, VssStorePlaybackSnapshot *out);

#if VSS_CFG_HOST_TEST
void Playback_ResetForTest(void);
#endif

#endif /* VSS_PLAYBACK_H */
