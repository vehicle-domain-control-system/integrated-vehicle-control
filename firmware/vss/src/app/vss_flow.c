#include "vss_flow.h"

#include <string.h>

#include "../service/asset/audio_asset.h"
#include "../service/asset/vss_cue_table.h"
#include "../service/audio_stream/audio_stream.h"
#include "../service/health/vss_health.h"
#include "../service/input/vss_input_net.h"
#include "../service/policy/vss_policy_store.h"
#include "../infra/vss_time.h"

#define FLOW_NO_START_BITS ((uint8_t)((1u << (unsigned)VSS_FAULT_INIT) | (1u << (unsigned)VSS_FAULT_OUTPUT)))

static VssOrigin s_origin;
static VssFlowObservation s_obs;
static bool s_reclaimed;
static uint64_t s_lastReclaimMs;
static VssTimeEvidence s_now;
static bool s_statusWaiting;     /* s_status was refused and waits */

/* Scratch: too large for the stack. */
static VssPlanDescriptor s_catalog[AUDIO_ASSET_CUE_ROWS];
static VssCandidateObservation s_candidates[VSS_CANDIDATE_CAPACITY];
static VssStorePlaybackSnapshot s_snapshot;
static VssPlaybackObservation s_playback;
static VssPolicyCurrent s_current;
static VssReadBasis s_basis;
static VssSelectionDecision s_decision;
static VssStatusSnapshot s_status;
static NetVssStatus s_netStatus;

static void count_up(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static void count_add(uint32_t *counter, size_t n)
{
    *counter = ((uint32_t)n > UINT32_MAX - *counter) ? UINT32_MAX : (*counter + (uint32_t)n);
}

/* The pass time on the clock every input time is taken from. */
static bool read_now(VssTimeEvidence *now)
{
    uint64_t ms;

    if (!VssTime_IsRegistered())
    {
        return false;
    }
    ms = VssTime_NowMs();
    now->domain = s_origin;
    now->epoch = VSS_TIME_EPOCH;
    now->earliest = ms;
    now->latest = ms;
    now->continuity = VSS_CONTINUITY_COMPARABLE;
    return true;
}

static void build_catalog(void)
{
    size_t i;

    memset(s_catalog, 0, sizeof s_catalog);
    for (i = 0u; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        const AudioAsset_CueRow *row = AudioAsset_CueAt(i);
        VssPlanDescriptor *p = &s_catalog[i];
        uint32_t size = 0u;

        if (row == NULL)
        {
            continue;
        }
        p->key = (uint64_t)i + 1u;
        p->signal = (VssInputSignal)row->signal;
        p->value = (VssInputValue)row->value;
        p->image = 1u;
        p->defined = true;
        p->available = (AudioAsset_GetSize(row->asset, &size) == AUDIO_ASSET_STATUS_OK) && (size != 0u);
        p->cueCount = 1u;
        p->cues[0].asset = row->asset;
        p->cues[0].gapMs = 0u;
        p->repeatCount = 1u;
    }
}

void Flow_Init(VssOrigin activeOrigin)
{
    s_origin = activeOrigin;
    memset(&s_obs, 0, sizeof s_obs);
    s_obs.origin = activeOrigin;
    s_reclaimed = false;
    s_lastReclaimMs = 0u;
    s_statusWaiting = false;
    build_catalog();
    Playback_Init();
}

static void reclaim(const VssTimeEvidence *now)
{
    if (s_reclaimed && ((now->earliest - s_lastReclaimMs) < (uint64_t)VSS_CFG_STORE_RECLAIM_PERIOD_MS))
    {
        return;
    }
    count_add(&s_obs.recordsReclaimed, Store_Reclaim(now));
    count_up(&s_obs.reclaimSweeps);
    s_reclaimed = true;
    s_lastReclaimMs = now->earliest;
}

/* Submits a new evaluation, or else the refused report that waits. A
 * refused report stays for the next pass; a new evaluation replaces it. */
static void publish_status(const VssTimeEvidence *now)
{
    NetSubmitResult result;

    if (Health_BuildStatus(now, &s_status))
    {
        count_up(&s_obs.statusBuilt);
        if (s_statusWaiting)
        {
            count_up(&s_obs.statusReplaced);
        }
    }
    else if (s_statusWaiting)
    {
        count_up(&s_obs.statusResubmitted);
    }
    else
    {
        return;
    }
    s_statusWaiting = false;
    if (!VssInputNet_ToNetStatus(&s_status, now, &s_netStatus))
    {
        count_up(&s_obs.statusUnconvertible);
    }
    else
    {
        result = VssNet_PublishStatus(&s_netStatus);
        if (result == NET_SUBMIT_OK)
        {
            count_up(&s_obs.statusPublished);
            s_obs.lastPublishedSequence = s_netStatus.update_sequence;
        }
        else
        {
            count_up(&s_obs.statusRefused);
            s_obs.lastRefusal = result;
            /* Only a refusal for want of link or room can end; the same
             * report offered again would get an invalid-argument answer
             * again. */
            if ((result == NET_SUBMIT_NOT_READY) || (result == NET_SUBMIT_NO_CAPACITY))
            {
                s_statusWaiting = true;
            }
            else
            {
                count_up(&s_obs.statusDropped);
            }
        }
    }
    s_obs.statusWaiting = s_statusWaiting;
    s_obs.waitingSequence = s_statusWaiting ? s_status.reportSequence : 0u;
}

VssFlowProgress Flow_Process(VssProcessingOpportunity opp)
{
    const VssTimeEvidence *now = &s_now;
    VssRequestDisposition disposition;
    uint64_t playbackRevision;
    size_t count = 0u;
    bool complete;

    if (!read_now(&s_now))
    {
        count_up(&s_obs.passesWithoutClock);
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    count_up(&s_obs.passes);
    if (opp == VSS_OPPORTUNITY_INPUT)
    {
        count_up(&s_obs.inputPasses);
    }

    AudioStream_Service();
    (void)Input_Process(NULL, now, NULL, 0u, NULL);
    Playback_ReadStoreSnapshot(s_origin, &s_snapshot);
    (void)Store_AdvanceDeadlines(now, &s_snapshot);
    reclaim(now);
    /* Facts only: the stateful stop check runs on this pass's candidates
     * inside Playback_RequestTransition. */
    Playback_Advance(NULL, 0u, now);

    complete = Store_ReadCandidates(now, s_origin, s_candidates, VSS_CANDIDATE_CAPACITY, &count);
    if (!complete)
    {
        count_up(&s_obs.incompleteReads);
    }
    (void)Playback_Read(&s_playback);
    playbackRevision = s_playback.revision;
    memset(&s_basis, 0, sizeof s_basis);
    s_basis.origin = s_origin;
    s_basis.time = *now;
    s_basis.validity = complete ? VSS_READ_VALID : VSS_READ_INCOMPLETE;
    s_basis.storeRevision = Store_ReadRevision(s_origin);
    s_basis.pbRevision = playbackRevision;
    s_basis.startsAllowed = (Health_CurrentMask() & FLOW_NO_START_BITS) == 0u;
    Playback_ReadPolicyCurrent(&s_current);
    s_decision = Select_Choose(s_candidates, count, &s_current, s_catalog, AUDIO_ASSET_CUE_ROWS, &s_basis);
    disposition = Playback_RequestTransition(&s_decision, s_candidates, count, &s_basis, now);

    s_obs.lastCandidateCount = count;
    s_obs.lastAction = s_decision.action;
    s_obs.lastDisposition = disposition;

    Health_Evaluate(now);
    publish_status(now);

    if (disposition == VSS_REQUEST_ADOPTED)
    {
        count_up(&s_obs.adoptions);
    }
    if (!complete || (s_decision.action == VSS_SELECTION_RECOLLECT))
    {
        count_up(&s_obs.recollects);
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    (void)Playback_Read(&s_playback);
    if ((disposition == VSS_REQUEST_ADOPTED) || (s_playback.revision != playbackRevision))
    {
        return VSS_FLOW_FOLLOW_UP_REQUIRED;
    }
    return VSS_FLOW_SETTLED;
}

bool Flow_Read(VssFlowObservation *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = s_obs;
    return true;
}
