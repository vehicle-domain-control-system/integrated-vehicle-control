#include "vss_policy_store.h"
#include <string.h>

static bool same_key(const VssOccurrenceKey *a, const VssOccurrenceKey *b)
{
    return a->origin == b->origin && a->source == b->source &&
        a->generation == b->generation && a->occurrenceId == b->occurrenceId;
}
static bool valid_time(const VssFactTime *t)
{
    return t->continuity == VSS_CONTINUITY_COMPARABLE && t->epoch != 0u &&
        t->earliest <= t->latest && (t->domain == VSS_PRODUCT || t->domain == VSS_TEST);
}
bool VssPolicy_SameCandidate(const VssCandidateObservation *a,
                             const VssCandidateObservation *b)
{
    if (a == NULL || b == NULL || a->mode != b->mode || a->origin != b->origin ||
        a->signal != b->signal) { return false; }
    if (a->mode == VSS_MODE_EVENT) { return same_key(&a->key, &b->key); }
    return a->mode == VSS_MODE_STATEFUL && a->warningMeta.domainBoot == b->warningMeta.domainBoot &&
        a->warningMeta.evidenceSource == b->warningMeta.evidenceSource &&
        a->warningMeta.evidenceBoot == b->warningMeta.evidenceBoot;
    /* Stateful repeated ACTIVE is the same state, never a new occurrence. */
}
unsigned VssPolicy_Priority(VssInputSignal signal, VssInputValue value)
{
    switch (signal) {
    case VSS_SIGNAL_ANTI_PINCH: return value == VSS_VALUE_ACTIVE ? 9u : 0u;
    case VSS_SIGNAL_REAR_RISK: return value == VSS_VALUE_EMERGENCY ? 8u :
        (value == VSS_VALUE_CAUTION ? 6u : 0u);
    case VSS_SIGNAL_OCCUPANT_HAZARD: return value == VSS_VALUE_ACTIVE ? 7u : 0u;
    case VSS_SIGNAL_LOCK_ERROR: return value == VSS_VALUE_EVENT ? 5u : 0u;
    case VSS_SIGNAL_UNLOCK_COMPLETE: return value == VSS_VALUE_EVENT ? 4u : 0u;
    case VSS_SIGNAL_LOCK_COMPLETE: return value == VSS_VALUE_EVENT ? 3u : 0u;
    case VSS_SIGNAL_GOODBYE: return value == VSS_VALUE_EVENT ? 2u : 0u;
    case VSS_SIGNAL_WELCOME: return value == VSS_VALUE_EVENT ? 1u : 0u;
    default: return 0u;
    }
}
bool VssPolicy_ValidPlan(const VssPlanDescriptor *p)
{
    size_t i;
    if (p == NULL || !p->defined || p->key == 0u || p->image == 0u ||
        p->cueCount == 0u || p->cueCount > VSS_PLAN_CUES || p->repeatCount == 0u ||
        p->repeatCount > VSS_PLAN_REPEATS || VssPolicy_Priority(p->signal, p->value) == 0u) {
        return false;
    }
    for (i = 0u; i < p->cueCount; ++i) { if (p->cues[i].asset > 7u) { return false; } }
    return true;
}
static int key_order(const VssOccurrenceKey *a, const VssOccurrenceKey *b)
{
    if (a->origin != b->origin) { return a->origin < b->origin ? -1 : 1; }
    if (a->source != b->source) { return a->source < b->source ? -1 : 1; }
    if (a->generation != b->generation) { return a->generation < b->generation ? -1 : 1; }
    return a->occurrenceId < b->occurrenceId ? -1 : (a->occurrenceId > b->occurrenceId ? 1 : 0);
}
/* 2 = no proved order. Fixed identity only resolves equal original evidence. */
static int original_order(const VssCandidateObservation *a, const VssCandidateObservation *b)
{
    const VssFactTime *x = &a->eventMeta.original, *y = &b->eventMeta.original;
    if (!valid_time(x) || !valid_time(y) || x->domain != y->domain || x->epoch != y->epoch) { return 2; }
    if (x->latest < y->earliest) { return -1; }
    if (y->latest < x->earliest) { return 1; }
    if (x->earliest == y->earliest && x->latest == y->latest) { return key_order(&a->key, &b->key); }
    return 2;
}
VssSelectionDecision Select_Choose(const VssCandidateObservation *candidates,
    size_t count, const VssPolicyCurrent *current,
    const VssPlanDescriptor *plans, size_t planCount, const VssReadBasis *basis)
{
    VssSelectionDecision result;
    const VssCandidateObservation *winner = NULL;
    const VssPlanDescriptor *plan = NULL;
    size_t i;
    unsigned rank = 0u;
    memset(&result, 0, sizeof result);
    result.action = VSS_SELECTION_RECOLLECT;
    if (basis == NULL || current == NULL || count > VSS_CANDIDATE_CAPACITY || planCount > 9u ||
        (count != 0u && candidates == NULL) || (planCount != 0u && plans == NULL)) { return result; }
    result.basis = *basis;
    if (basis->validity != VSS_READ_VALID || !valid_time(&basis->time) ||
        basis->origin != basis->time.domain || basis->pbRevision == 0u ||
        basis->storeRevision == 0u || basis->storeRevision != Store_ReadRevision(basis->origin)) { return result; }
    if (current->present && (current->session == 0u || current->candidate.origin != basis->origin)) { return result; }
    for (i = 0u; i < count; ++i) {
        const VssCandidateObservation *c = &candidates[i];
        unsigned p;
        bool sameCurrent = current->present && VssPolicy_SameCandidate(c, &current->candidate);
        if (c->origin != basis->origin) { return result; }
        if (c->validity != VSS_CANDIDATE_APPLICABLE || (!c->newSessionAllowed && !sameCurrent)) { continue; }
        if (c->revision == 0u || (c->mode != VSS_MODE_EVENT && c->mode != VSS_MODE_STATEFUL)) { return result; }
        if (c->origin == VSS_PRODUCT && c->signal == VSS_SIGNAL_ANTI_PINCH) { continue; }
        p = VssPolicy_Priority(c->signal, c->value);
        if (p == 0u) { return result; }
        if (winner == NULL || p > rank) { winner = c; rank = p; }
        else if (p == rank && c->mode == VSS_MODE_EVENT && winner->mode == VSS_MODE_EVENT) {
            /* Provisional minimum only: no time order is authorized here.
             * Validate it against every relevant peer after the maximum rank
             * is known. Lower-rank ambiguity must not hide an urgent winner. */
            if (c->eventMeta.original.earliest < winner->eventMeta.original.earliest ||
                (c->eventMeta.original.earliest == winner->eventMeta.original.earliest &&
                 key_order(&c->key, &winner->key) < 0)) { winner = c; }
        }
    }
    if (current->present && current->stopping) { result.action = VSS_SELECTION_WAIT; return result; }
    if (current->present && current->valid && current->candidate.mode == VSS_MODE_EVENT &&
        (winner == NULL || rank <= VssPolicy_Priority(current->candidate.signal, current->candidate.value))) {
        if (!VssPolicy_ValidPlan(&current->plan)) { return result; }
        result.action = VSS_SELECTION_KEEP;
        result.candidatePresent = true; result.candidate = current->candidate; result.plan = current->plan;
        return result;
    }
    result.action = VSS_SELECTION_WAIT;
    if (winner == NULL) { return result; }
    for (i = 0u; i < count; ++i) {
        const VssCandidateObservation *c = &candidates[i];
        bool sameCurrent = current->present && VssPolicy_SameCandidate(c, &current->candidate);
        if (c->validity != VSS_CANDIDATE_APPLICABLE || (!c->newSessionAllowed && !sameCurrent) ||
            (c->origin == VSS_PRODUCT && c->signal == VSS_SIGNAL_ANTI_PINCH) ||
            VssPolicy_Priority(c->signal, c->value) != rank || VssPolicy_SameCandidate(c, winner)) { continue; }
        if (c->mode != VSS_MODE_EVENT || winner->mode != VSS_MODE_EVENT ||
            original_order(winner, c) != -1) {
            result.action = VSS_SELECTION_RECOLLECT; return result;
        }
    }
    for (i = 0u; i < planCount; ++i) {
        if (plans[i].signal == winner->signal && plans[i].value == winner->value) {
            if (plan != NULL) { result.action = VSS_SELECTION_RECOLLECT; return result; }
            plan = &plans[i];
        }
    }
    if (!VssPolicy_ValidPlan(plan)) { return result; }
    result.candidatePresent = true; result.candidate = *winner; result.plan = *plan;
    if (!plan->available || !basis->startsAllowed) { return result; }
    if (current->present && current->valid && VssPolicy_SameCandidate(winner, &current->candidate) &&
        winner->value == current->candidate.value && plan->key == current->plan.key) {
        result.action = VSS_SELECTION_KEEP;
    } else { result.action = current->present ? VSS_SELECTION_REPLACE : VSS_SELECTION_CHOOSE; }
    return result;
}
