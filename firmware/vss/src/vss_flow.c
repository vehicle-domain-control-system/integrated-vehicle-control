#include "vss_playback.h"
#include <string.h>

/* Owner scratch, not a queued input or an additional ledger. Evaluation
 * pointers never escape this synchronous processing opportunity. */
static VssCandidateObservation candidates[VSS_CANDIDATE_CAPACITY];
static VssPlanDescriptor catalog[9];
static size_t catalogCount;
static bool startsAllowed; /* HEALTH/product plan binding deliberately absent */
static uint64_t selections;

static bool collect(const VssTimeEvidence *time, size_t *count, VssReadBasis *basis)
{
    VssPlaybackObservation playback;
    uint64_t before = Store_ReadRevision(time->domain);
    bool complete = Store_ReadCandidates(time, time->domain, candidates,
                                         VSS_CANDIDATE_CAPACITY, count);
    (void)Playback_Read(&playback);
    memset(basis, 0, sizeof(*basis)); basis->origin = time->domain; basis->time = *time;
    basis->storeRevision = Store_ReadRevision(time->domain); basis->pbRevision = playback.revision;
    basis->startsAllowed = startsAllowed;
    basis->validity = complete && !playback.notificationPending &&
        before == basis->storeRevision && before != 0u &&
        time->continuity == VSS_CONTINUITY_COMPARABLE && time->earliest <= time->latest ?
        VSS_READ_VALID : VSS_READ_INCOMPLETE;
    return basis->validity == VSS_READ_VALID;
}
VssFlowProgress Flow_Process(VssProcessingOpportunity opportunity, const VssTimeEvidence *time)
{
    size_t count = 0u;
    VssReadBasis basis;
    VssStorePlaybackSnapshot snapshot;
    VssPlaybackObservation playback;
    VssPolicyCurrent current;
    VssSelectionDecision decision;
    bool coherent;
    if (opportunity < VSS_OPPORTUNITY_INITIAL || opportunity > VSS_OPPORTUNITY_TIME_CHECK) {
        return VSS_FLOW_FOLLOW_UP_REQUIRED;
    }
    if (time == NULL || (time->domain != VSS_PRODUCT && time->domain != VSS_TEST)) {
        return VSS_FLOW_RECOLLECT_OBSERVATIONS;
    }
    /* No-RX quality supervision still runs after an already consumed bundle.
     * P1-B helper only advances Warning validity; Event deadline follows facts. */
    (void)Input_Process(NULL, time, NULL, 0u, NULL);
    (void)collect(time, &count, &basis);
    Playback_Service(candidates, count, &basis, time);
    Playback_ReadStoreSnapshot(time->domain, &snapshot);
    (void)Store_AdvanceDeadlines(time, &snapshot);
    coherent = collect(time, &count, &basis);
    if (!coherent) { return VSS_FLOW_RECOLLECT_OBSERVATIONS; }
    Playback_ReadPolicyCurrent(&current);
    decision = Select_Choose(candidates, count, &current, catalog, catalogCount, &basis);
    if (selections != UINT64_MAX) { ++selections; }
    if (decision.action == VSS_SELECTION_RECOLLECT) { return VSS_FLOW_RECOLLECT_OBSERVATIONS; }
    (void)Playback_RequestTransition(&decision, NULL, VSS_STOP_NONE,
                                    candidates, count, &basis, time);
    /* Adoption changes stamps. Recollect before immediate lower stages/START. */
    (void)collect(time, &count, &basis);
    Playback_Service(candidates, count, &basis, time);
    (void)Playback_Read(&playback);
    /* HEALTH/Status P4 consumer is not synthesized. Unpublished facts and
     * diagnostics remain protected at their owner for a real consumer. */
    if (playback.notificationPending || playback.publicationPending ||
        (playback.present && playback.phase != VSS_PB_RETIRED)) {
        return VSS_FLOW_FOLLOW_UP_REQUIRED;
    }
    return VSS_FLOW_SETTLED;
}
VssInputApplication VssFlow_OnEvent(const VssInputEvidence *event, const VssTimeEvidence *time)
{
    VssInputApplication result = Input_Process(event, time, NULL, 0u, NULL);
    (void)Flow_Process(VSS_OPPORTUNITY_INPUT_OR_FOLLOW_UP, time);
    return result;
}
VssWarningBundleResult VssFlow_OnWarning(const VssWarningFrame *frame, const VssTimeEvidence *time)
{
    VssWarningBundleResult result = Input_ProcessWarningFrame(frame, time);
    (void)Flow_Process(VSS_OPPORTUNITY_INPUT_OR_FOLLOW_UP, time);
    return result;
}
#ifdef VSS_HOST_TEST
void Flow_TestReset(void)
{
    memset(candidates, 0, sizeof(candidates)); memset(catalog, 0, sizeof(catalog));
    catalogCount = 0u; startsAllowed = false; selections = 0u;
}
bool Flow_TestCatalog(const VssPlanDescriptor *plans, size_t count, bool allowed)
{
    size_t i;
    if (count > 9u || (count != 0u && plans == NULL)) { return false; }
    for (i = 0u; i < count; ++i) {
        if (!VssPolicy_ValidPlan(&plans[i])) { return false; }
    }
    for (i = 0u; i < count; ++i) { catalog[i] = plans[i]; }
    catalogCount = count; startsAllowed = allowed; return true;
}
uint64_t Flow_TestSelectionCount(void) { return selections; }
#endif
