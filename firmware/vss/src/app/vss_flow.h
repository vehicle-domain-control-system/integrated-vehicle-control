#ifndef VSS_FLOW_H
#define VSS_FLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "../service/input/vss_input_store.h"
#include "../service/playback/vss_playback.h"
#include "../net_if/vss_net_api.h"

/* One pass of the VSS service chain for the active input origin:
 *
 *   AudioStream_Service      refill and output facts
 *   Input_Process(NULL)      input deadlines and receive monitor
 *   Store_AdvanceDeadlines   expired one-shot records, warning deadlines
 *   Store_Reclaim            every VSS_CFG_STORE_RECLAIM_PERIOD_MS
 *   Playback_Advance         output facts into STORE
 *   Store_ReadCandidates     once per pass
 *   Select_Choose            against the cue plan catalog
 *   Playback_RequestTransition
 *   Health_Evaluate
 *   Health_BuildStatus       on a new evaluation only:
 *     VssInputNet_ToNetStatus -> VssNet_PublishStatus
 *
 * A status report the Network does not accept is not kept for a retry: the
 * next evaluation (at the latest VSS_CFG_STATUS_PERIOD_MS later, sooner when
 * the state changes) is submitted with its own number. Refusals are counted.
 *
 * New sessions are not started while HEALTH reports an initialization or
 * output fault. Runs in the super-loop owner context. */

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
    uint32_t refusedTimes;        /* passes refused for their time argument */
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
    NetSubmitResult lastRefusal;
    uint32_t statusUnconvertible;
    uint32_t lastPublishedSequence;
} VssFlowObservation;

/* Builds the plan catalog from the cue table (one cue, played once; a cue
 * without data is marked unavailable), initializes playback and selects the
 * origin whose input is processed. Called once at boot, after
 * AudioStream_Init and Health_Init. */
void Flow_Init(VssOrigin activeOrigin);

VssFlowProgress Flow_Process(VssProcessingOpportunity opp, const VssTimeEvidence *now);

bool Flow_Read(VssFlowObservation *out);

#endif /* VSS_FLOW_H */
