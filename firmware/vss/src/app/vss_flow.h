#ifndef VSS_FLOW_H
#define VSS_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "../service/input/vss_input_store.h"
#include "../service/playback/vss_playback.h"
#include "../net_if/vss_net_api.h"

/* One pass of the VSS service chain for the active input origin:
 *
 *   VssTime_NowMs            the pass time, read once at the start
 *   AudioStream_Service      refill and output facts
 *   Input_Process(NULL)      input deadlines and receive monitor
 *   Store_AdvanceDeadlines   expired one-shot records, warning deadlines
 *   Store_Reclaim            every VSS_CFG_STORE_RECLAIM_PERIOD_MS
 *   Playback_Advance         output facts into STORE
 *   Store_ReadCandidates     once per pass
 *   Select_Choose            against the cue plan catalog
 *   Playback_RequestTransition
 *   Health_Evaluate
 *   Health_BuildStatus       a new evaluation, or the report still waiting:
 *     VssInputNet_ToNetStatus -> VssNet_PublishStatus
 *
 * Call order in the super-loop: deliver the inputs of this loop first
 * (Net_MainFunction, the built-in test input), then call Flow_Process. The
 * pass reads its time itself, after the inputs, so no input it processes
 * carries a time later than the pass; a pass time taken before a delivery
 * would make a just-received warning look not yet valid and stop it.
 *
 * A status report the Network refuses as NOT_READY or NO_CAPACITY is kept,
 * one report and no queue: every following pass submits it again, with the
 * same number and content and its ages measured to that pass, until the
 * Network takes it. A new evaluation made while one waits takes its place;
 * the waiting one is never sent. A report refused for any other result
 * (INVALID_ARGUMENT) is counted and dropped; the next evaluation goes out
 * with the next number.
 *
 * New sessions are not started while HEALTH reports an initialization or
 * output fault, nor while the output has failed since that evaluation
 * (checked by playback). Runs in the super-loop owner context. */

typedef enum
{
    VSS_OPPORTUNITY_INPUT = 0,    /* a message reached INPUT since the last pass */
    VSS_OPPORTUNITY_TIME_CHECK    /* the periodic pass */
} VssProcessingOpportunity;

typedef enum
{
    VSS_FLOW_SETTLED = 0,         /* nothing left to do before the next period */
    VSS_FLOW_FOLLOW_UP_REQUIRED,  /* a session started or stop was requested: run again soon */
    VSS_FLOW_RECOLLECT_OBSERVATIONS /* the decision needs a fresh read: run again soon */
} VssFlowProgress;

/* Diagnostic view. Counters saturate. */
typedef struct
{
    VssOrigin origin;
    uint32_t passes;
    uint32_t inputPasses;
    uint32_t passesWithoutClock;  /* refused: VssTime has no tick source yet */
    uint32_t incompleteReads;     /* candidate reads that were not complete */
    uint32_t recollects;
    uint32_t adoptions;
    size_t lastCandidateCount;
    VssSelectionAction lastAction;
    VssRequestDisposition lastDisposition;
    uint32_t reclaimSweeps;
    uint32_t recordsReclaimed;
    uint32_t statusBuilt;         /* new evaluations */
    uint32_t statusPublished;     /* accepted by the Network */
    uint32_t statusRefused;       /* VssNet_PublishStatus returned other than OK */
    uint32_t statusResubmitted;   /* submissions of a report refused before */
    uint32_t statusReplaced;      /* waiting reports replaced by a newer evaluation */
    uint32_t statusDropped;       /* refused reports not kept: not NOT_READY or NO_CAPACITY */
    NetSubmitResult lastRefusal;
    uint32_t statusUnconvertible;
    uint32_t lastPublishedSequence;
    bool statusWaiting;           /* a refused report waits for the next pass */
    uint32_t waitingSequence;
} VssFlowObservation;

/* Builds the plan catalog from the cue table (one cue, played once; a cue
 * without data is marked unavailable), initializes playback and selects the
 * origin whose input is processed. Called once at boot, after
 * AudioStream_Init and Health_Init. */
void Flow_Init(VssOrigin activeOrigin);

/* One pass at VssTime_NowMs. RECOLLECT_OBSERVATIONS without any work when
 * no tick source is registered yet. */
VssFlowProgress Flow_Process(VssProcessingOpportunity opp);

bool Flow_Read(VssFlowObservation *out);

#endif /* VSS_FLOW_H */
