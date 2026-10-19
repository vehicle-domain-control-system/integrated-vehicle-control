#include "../policy/vss_policy_store.h"
#include <string.h>

typedef struct {
    VssOccurrenceKey key;
    uint64_t session;
    uint64_t attempt;
    bool attempted;
    bool retired;
    bool noStart;
} PlaybackBinding;
static PlaybackBinding bindings[VSS_PLAYBACK_BINDINGS_CAPACITY];
static size_t bindingCount;
static VssPlaybackFact facts[VSS_PLAYBACK_FACT_CAPACITY];
static size_t factCount;
static uint64_t bindingRevision[2] = {1u, 1u};
/* Revision contributions of reclaimed records, per origin, so that
 * Store_ReadRevision never decreases when records are removed. */
static uint64_t reclaimedRevision[2];
static VssOccurrenceObservation reclaimScratch;
/* Per-function scratch for records and candidates; these objects are too
 * large for the stack and every call here is serialized by one owner. */
static VssOccurrenceObservation revisionRecord;
static VssOccurrenceObservation candidateRecord;
static VssCandidateObservation candidateScratch;
static VssOccurrenceObservation bindRecord;
static VssOccurrenceObservation factRecord;
static VssOccurrenceObservation deadlineRecord;

static bool origin_valid(VssOrigin o) { return o == VSS_PRODUCT || o == VSS_TEST; }
static bool same_key(const VssOccurrenceKey *a, const VssOccurrenceKey *b)
{
    return a->origin == b->origin && a->source == b->source &&
        a->generation == b->generation && a->occurrenceId == b->occurrenceId;
}
static bool same_time(const VssFactTime *a, const VssFactTime *b)
{
    return a->domain == b->domain && a->epoch == b->epoch && a->earliest == b->earliest &&
        a->latest == b->latest && a->continuity == b->continuity;
}
static bool comparable(const VssFactTime *a, const VssFactTime *b)
{
    return origin_valid(a->domain) && a->domain == b->domain && a->epoch != 0u &&
        a->epoch == b->epoch && a->continuity == VSS_CONTINUITY_COMPARABLE &&
        b->continuity == VSS_CONTINUITY_COMPARABLE && a->earliest <= a->latest && b->earliest <= b->latest;
}
static bool new_allowed(const VssOccurrenceKey *key)
{
    size_t i;
    for (i = 0u; i < bindingCount; ++i) {
        if (same_key(key, &bindings[i].key) && (!bindings[i].retired ||
            !bindings[i].noStart)) { return false; }
    }
    return true;
}
static size_t binding_find(const VssOccurrenceKey *key, uint64_t session, uint64_t attempt)
{
    size_t i;
    for (i = 0u; i < bindingCount; ++i) {
        if (bindings[i].session == session && bindings[i].attempt == attempt &&
            same_key(key, &bindings[i].key)) { break; }
    }
    return i;
}
uint64_t Store_ReadRevision(VssOrigin origin)
{
    uint64_t revision;
    size_t i;
    VssWarningObservation warning;
    VssRearObservation rear;
    if (!origin_valid(origin)) { return 0u; }
    revision = bindingRevision[(size_t)origin];
    if (revision > UINT64_MAX - reclaimedRevision[(size_t)origin]) { return 0u; }
    revision += reclaimedRevision[(size_t)origin];
    for (i = 0u; i < Store_Count(); ++i) {
        if (!StorePriv_ReadAt(i, &revisionRecord)) { return 0u; }
        if (revisionRecord.event.meta.key.origin != origin) { continue; }
        if (revision >= UINT64_MAX - revisionRecord.revision) { return 0u; }
        revision += revisionRecord.revision + 1u;
    }
    for (i = 0u; i < 4u; ++i) {
        VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + (unsigned)i);
        if (!Store_ReadWarning(origin, signal, &warning) || revision > UINT64_MAX - warning.revision) { return 0u; }
        revision += warning.revision;
    }
    if (!Store_ReadRear(origin, &rear) || revision > UINT64_MAX - rear.revision) { return 0u; }
    return revision + rear.revision;
    /* All summands only increase in production; no hash/collision assumption. */
}
bool Store_ReadCandidates(const VssTimeEvidence *time, VssOrigin origin,
                          VssCandidateObservation *out, size_t capacity, size_t *written)
{
    size_t i, n = 0u;
    bool complete = true;

    VssWarningObservation warning;
    if (written == NULL || capacity > VSS_CANDIDATE_CAPACITY ||
        (capacity != 0u && out == NULL)) { return false; }
    *written = 0u;
    if (time == NULL || !origin_valid(origin) || time->domain != origin) { return false; }
    for (i = 0u; i < Store_Count() + 3u; ++i) {

        memset(&candidateScratch, 0, sizeof candidateScratch);
        candidateScratch.origin = origin;
        if (i < Store_Count()) {
            if (!StorePriv_ReadAt(i, &candidateRecord)) { return false; }
            if (candidateRecord.rejectedTombstone || candidateRecord.event.meta.key.origin != origin) { continue; }
            candidateScratch.mode = VSS_MODE_EVENT; candidateScratch.key = candidateRecord.event.meta.key;
            candidateScratch.eventMeta = candidateRecord.event.meta; candidateScratch.signal = candidateRecord.event.signal;
            candidateScratch.value = VSS_VALUE_EVENT; candidateScratch.revision = candidateRecord.revision;
            candidateScratch.started = candidateRecord.state == VSS_OCCURRENCE_STARTED;
            candidateScratch.newSessionAllowed = Store_FirstStartAllowed(&candidateScratch.key, time) && new_allowed(&candidateScratch.key);
            if (candidateRecord.state == VSS_OCCURRENCE_PENDING) {
                candidateScratch.validity = Store_FirstStartAllowed(&candidateScratch.key, time) ? VSS_CANDIDATE_APPLICABLE : VSS_CANDIDATE_UNCERTAIN;
            } else if (candidateScratch.started) {
                candidateScratch.validity = candidateRecord.replayAllowed && comparable(time, &candidateScratch.eventMeta.original)
                    ? VSS_CANDIDATE_APPLICABLE : VSS_CANDIDATE_UNCERTAIN;
            } else { candidateScratch.validity = VSS_CANDIDATE_NOT_APPLICABLE; }
        } else {
            VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + (unsigned)(i - Store_Count()));
            if (origin == VSS_PRODUCT && signal == VSS_SIGNAL_ANTI_PINCH) { continue; }
            if (!Store_ReadWarning(origin, signal, &warning) || !warning.lastValidPresent) { continue; }
            candidateScratch.mode = VSS_MODE_STATEFUL; candidateScratch.signal = signal; candidateScratch.value = warning.lastValidValue;
            candidateScratch.warningMeta = warning.lastValidMeta; candidateScratch.revision = warning.revision;
            candidateScratch.validity = warning.validity; candidateScratch.newSessionAllowed = warning.newSessionAllowed;
            if (!same_time(time, &warning.evaluatedAt)) {
                candidateScratch.validity = VSS_CANDIDATE_UNCERTAIN; candidateScratch.newSessionAllowed = false;
            }
        }
        if (n < capacity) { out[n++] = candidateScratch; } else { complete = false; }
    }
    *written = n;
    return complete;
}
bool Store_BindPlayback(const VssOccurrenceKey *key, uint64_t session,
                       uint64_t attempt, uint64_t expectedRevision)
{

    size_t i;
    bool continuing;
    if (key == NULL || !origin_valid(key->origin) || session == 0u || attempt == 0u ||
        !Store_ReadOccurrence(key, &bindRecord) || bindRecord.rejectedTombstone ||
        bindRecord.revision != expectedRevision || !bindRecord.replayAllowed) { return false; }
    continuing = bindRecord.state == VSS_OCCURRENCE_STARTED && bindRecord.firstStartFact.present &&
        bindRecord.firstStartFact.session == session;
    if (!continuing && (bindRecord.state != VSS_OCCURRENCE_PENDING || !new_allowed(key))) { return false; }
    for (i = 0u; i < bindingCount; ++i) {
        if (bindings[i].attempt == attempt || (bindings[i].session == session &&
            (!continuing || !same_key(key, &bindings[i].key)))) { return false; }
        if (same_key(key, &bindings[i].key) && !bindings[i].retired) { return false; }
    }
    if (bindingCount == VSS_PLAYBACK_BINDINGS_CAPACITY ||
        bindingRevision[(size_t)key->origin] == UINT64_MAX) { return false; }
    bindings[bindingCount].key = *key;
    bindings[bindingCount].session = session;
    bindings[bindingCount].attempt = attempt;
    ++bindingCount; ++bindingRevision[(size_t)key->origin];
    return true;
}
bool Store_MarkPlaybackAttempted(const VssOccurrenceKey *key, uint64_t session, uint64_t attempt)
{
    size_t i;
    if (key == NULL || !origin_valid(key->origin)) { return false; }
    i = binding_find(key, session, attempt);
    if (i == bindingCount || bindings[i].retired) { return false; }
    if (!bindings[i].attempted) {
        if (bindingRevision[(size_t)key->origin] == UINT64_MAX) { return false; }
        bindings[i].attempted = true; ++bindingRevision[(size_t)key->origin];
    }
    return true;
}
bool Store_MarkPlaybackRetired(const VssOccurrenceKey *key, uint64_t session,
                              uint64_t attempt, bool noStart)
{
    size_t i, j;
    bool proof = false;
    if (key == NULL || !origin_valid(key->origin)) { return false; }
    i = binding_find(key, session, attempt);
    if (i == bindingCount) { return false; }
    if (noStart) {
        for (j = 0u; j < factCount; ++j) {
            if (facts[j].session == session && facts[j].kind == (uint32_t)VSS_FACT_NO_START_REEVALUATE &&
                same_key(&facts[j].occurrence, key)) { proof = true; }
        }
        if (!proof) { return false; }
    }
    if (bindings[i].retired) { return bindings[i].noStart == noStart; }
    if (bindingRevision[(size_t)key->origin] == UINT64_MAX) { return false; }
    bindings[i].retired = true; bindings[i].noStart = noStart;
    ++bindingRevision[(size_t)key->origin];
    return true;
}
static bool same_fact(const VssPlaybackFact *a, const VssPlaybackFact *b)
{
    return a->present == b->present && a->kind == b->kind && a->fact == b->fact &&
        same_key(&a->occurrence, &b->occurrence) && a->session == b->session &&
        a->attempt == b->attempt && same_time(&a->occurredAt, &b->occurredAt) &&
        a->basisRevision == b->basisRevision;
}
VssFactApplication Store_ApplyPlaybackFact(const VssPlaybackFact *fact)
{

    VssOccurrenceState state;
    bool replay;
    const VssPlaybackFact *first = NULL, *final = NULL;
    size_t i;
    if (fact == NULL || !fact->present || fact->fact == 0u || fact->basisRevision == 0u ||
        fact->session == 0u || fact->attempt == 0u || !origin_valid(fact->occurrence.origin)) { return VSS_FACT_NOT_APPLIED; }
    for (i = 0u; i < factCount; ++i) {
        if (facts[i].fact == fact->fact) { return same_fact(fact, &facts[i]) ? VSS_FACT_ALREADY_APPLIED : VSS_FACT_NOT_APPLIED; }
        if (facts[i].session == fact->session && facts[i].kind == fact->kind) { return VSS_FACT_NOT_APPLIED; }
    }
    if (!Store_ReadOccurrence(&fact->occurrence, &factRecord) || factRecord.rejectedTombstone ||
        binding_find(&fact->occurrence, fact->session, fact->attempt) == bindingCount ||
        !comparable(&fact->occurredAt, &factRecord.event.meta.original) ||
        fact->occurredAt.earliest < factRecord.event.meta.original.latest ||
        factRecord.state == VSS_OCCURRENCE_EXPIRED || factRecord.finalFact.present ||
        factRecord.state == VSS_OCCURRENCE_COMPLETED || factRecord.state == VSS_OCCURRENCE_INTERRUPTED ||
        factRecord.state == VSS_OCCURRENCE_UNCERTAIN_FINAL || factCount == VSS_PLAYBACK_FACT_CAPACITY) { return VSS_FACT_NOT_APPLIED; }
    state = factRecord.state; replay = factRecord.replayAllowed;
    switch ((VssPlaybackFactKind)fact->kind) {
    case VSS_FACT_FIRST_ACTUAL_START:
        if (factRecord.state != VSS_OCCURRENCE_PENDING || factRecord.firstStartFact.present ||
            bindings[binding_find(&fact->occurrence, fact->session, fact->attempt)].retired ||
            !bindings[binding_find(&fact->occurrence, fact->session, fact->attempt)].attempted ||
            fact->occurredAt.latest - factRecord.event.meta.original.earliest >= factRecord.event.meta.useLimit ||
            (factRecord.inputLossPresent && (!comparable(&fact->occurredAt, &factRecord.inputLossAt) ||
             fact->occurredAt.latest >= factRecord.inputLossAt.earliest))) { return VSS_FACT_NOT_APPLIED; }
        state = VSS_OCCURRENCE_STARTED; first = fact; break;
    case VSS_FACT_WHOLE_COMPLETED:
    case VSS_FACT_WHOLE_INTERRUPTED:
        if (!factRecord.firstStartFact.present || factRecord.firstStartFact.session != fact->session ||
            factRecord.state != VSS_OCCURRENCE_STARTED ||
            fact->occurredAt.earliest < factRecord.firstStartFact.occurredAt.latest) { return VSS_FACT_NOT_APPLIED; }
        state = fact->kind == (uint32_t)VSS_FACT_WHOLE_COMPLETED ? VSS_OCCURRENCE_COMPLETED : VSS_OCCURRENCE_INTERRUPTED;
        replay = false; final = fact; break;
    case VSS_FACT_START_OUTCOME_UNCERTAIN:
        if (factRecord.firstStartFact.present && factRecord.firstStartFact.session != fact->session) { return VSS_FACT_NOT_APPLIED; }
        state = VSS_OCCURRENCE_UNCERTAIN_FINAL; replay = false; final = fact; break;
    case VSS_FACT_NO_START_REEVALUATE:
        if (factRecord.firstStartFact.present || factRecord.state != VSS_OCCURRENCE_PENDING) { return VSS_FACT_NOT_APPLIED; }
        break; /* no age/replay reset and no new Pending */
    default: return VSS_FACT_NOT_APPLIED;
    }
    if (!StorePriv_ApplyRecord(&fact->occurrence, factRecord.revision, state, replay, first, final)) { return VSS_FACT_NOT_APPLIED; }
    facts[factCount++] = *fact;
    return VSS_FACT_APPLIED;
}
VssStoreProgress Store_AdvanceDeadlines(const VssTimeEvidence *time,
                                      const VssStorePlaybackSnapshot *playback)
{
    bool changed = false, reconfirm = false;
    size_t i;
    uint64_t before;
    if (playback == NULL || !origin_valid(playback->origin) || !playback->complete ||
        playback->revision == 0u || playback->notificationPending ||
        playback->storeRevision != Store_ReadRevision(playback->origin)) { return VSS_STORE_RECONFIRM_REQUIRED; }
    before = Store_ReadRevision(playback->origin);
    Store_AdvanceWarningDeadlines(time);
    if (Store_ReadRevision(playback->origin) != before) { changed = true; }
    for (i = 0u; i < Store_Count(); ++i) {
        if (!StorePriv_ReadAt(i, &deadlineRecord)) { reconfirm = true; continue; }
        if (deadlineRecord.rejectedTombstone || deadlineRecord.event.meta.key.origin != playback->origin ||
            deadlineRecord.state != VSS_OCCURRENCE_PENDING) { continue; }
        if (time == NULL || !comparable(time, &deadlineRecord.event.meta.original) ||
            time->earliest < deadlineRecord.event.meta.original.latest || !new_allowed(&deadlineRecord.event.meta.key)) {
            reconfirm = true; continue;
        }
        if (!deadlineRecord.replayAllowed || time->earliest - deadlineRecord.event.meta.original.latest >= deadlineRecord.event.meta.useLimit) {
            if (StorePriv_ApplyRecord(&deadlineRecord.event.meta.key, deadlineRecord.revision,
                VSS_OCCURRENCE_EXPIRED, false, NULL, NULL)) { changed = true; }
            else { reconfirm = true; }
        } else if (time->latest - deadlineRecord.event.meta.original.earliest >= deadlineRecord.event.meta.useLimit) { reconfirm = true; }
    }
    if (time == NULL || time->domain != playback->origin || time->continuity != VSS_CONTINUITY_COMPARABLE ||
        time->earliest > time->latest || time->epoch == 0u) { reconfirm = true; }
    return changed ? VSS_STORE_CHANGED : (reconfirm ? VSS_STORE_RECONFIRM_REQUIRED : VSS_STORE_STABLE);
}
static bool live_binding(const VssOccurrenceKey *key)
{
    size_t i;
    for (i = 0u; i < bindingCount; ++i) {
        if (same_key(key, &bindings[i].key) && !bindings[i].retired) { return true; }
    }
    return false;
}
static void drop_bindings_and_facts(const VssOccurrenceKey *key)
{
    size_t i = 0u;
    while (i < bindingCount) {
        if (same_key(key, &bindings[i].key)) {
            --bindingCount;
            if (i < bindingCount) {
                memmove(&bindings[i], &bindings[i + 1u], (bindingCount - i) * sizeof bindings[0]);
            }
            memset(&bindings[bindingCount], 0, sizeof bindings[0]);
        } else { ++i; }
    }
    i = 0u;
    while (i < factCount) {
        if (same_key(key, &facts[i].occurrence)) {
            --factCount;
            if (i < factCount) {
                memmove(&facts[i], &facts[i + 1u], (factCount - i) * sizeof facts[0]);
            }
            memset(&facts[factCount], 0, sizeof facts[0]);
        } else { ++i; }
    }
}
size_t Store_Reclaim(const VssTimeEvidence *now)
{
    size_t i = 0u, reclaimed = 0u;
    if (now == NULL || !origin_valid(now->domain) || now->epoch == 0u ||
        now->continuity != VSS_CONTINUITY_COMPARABLE || now->earliest > now->latest) { return 0u; }
    while (i < Store_Count()) {
        const VssOccurrenceObservation *r = &reclaimScratch;
        const VssFactTime *o = &r->event.meta.original;
        uint64_t *base;
        if (!StorePriv_ReadAt(i, &reclaimScratch)) { break; }
        base = &reclaimedRevision[(size_t)r->event.meta.key.origin];
        /* One system clock serves both origins: only the epoch, continuity
         * and the time values are compared, not the domain. A tombstone
         * without a time (epoch 0) is never reclaimed. */
        if (!origin_valid(r->event.meta.key.origin) || o->epoch != now->epoch ||
            o->continuity != VSS_CONTINUITY_COMPARABLE || o->earliest > o->latest ||
            now->earliest < o->latest ||
            now->earliest - o->latest < (uint64_t)VSS_CFG_STORE_RECLAIM_MS ||
            live_binding(&r->event.meta.key) ||
            *base > UINT64_MAX - 1u - r->revision) { ++i; continue; }
        drop_bindings_and_facts(&r->event.meta.key);
        if (!StorePriv_RemoveAt(i)) { break; }
        *base += r->revision + 1u;
        ++reclaimed;
    }
    return reclaimed;
}
#if VSS_CFG_HOST_TEST
void StorePlayback_TestReset(void)
{
    memset(bindings, 0, sizeof bindings); memset(facts, 0, sizeof facts);
    bindingCount = 0u; factCount = 0u; bindingRevision[0] = 1u; bindingRevision[1] = 1u;
    reclaimedRevision[0] = 0u; reclaimedRevision[1] = 0u;
}
#endif
