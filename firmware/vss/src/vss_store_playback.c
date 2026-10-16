#include "vss_policy_store.h"
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
    VssOccurrenceObservation record;
    VssWarningObservation warning;
    VssRearObservation rear;
    if (!origin_valid(origin)) { return 0u; }
    revision = bindingRevision[(size_t)origin];
    for (i = 0u; i < Store_Count(); ++i) {
        if (!StorePriv_ReadAt(i, &record)) { return 0u; }
        if (record.event.meta.key.origin != origin) { continue; }
        if (revision >= UINT64_MAX - record.revision) { return 0u; }
        revision += record.revision + 1u;
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
    VssOccurrenceObservation record;
    VssWarningObservation warning;
    if (written == NULL || capacity > VSS_CANDIDATE_CAPACITY ||
        (capacity != 0u && out == NULL)) { return false; }
    *written = 0u;
    if (time == NULL || !origin_valid(origin) || time->domain != origin) { return false; }
    for (i = 0u; i < Store_Count() + 3u; ++i) {
        VssCandidateObservation c;
        memset(&c, 0, sizeof c);
        c.origin = origin;
        if (i < Store_Count()) {
            if (!StorePriv_ReadAt(i, &record)) { return false; }
            if (record.rejectedTombstone || record.event.meta.key.origin != origin) { continue; }
            c.mode = VSS_MODE_EVENT; c.key = record.event.meta.key;
            c.eventMeta = record.event.meta; c.signal = record.event.signal;
            c.value = VSS_VALUE_EVENT; c.revision = record.revision;
            c.started = record.state == VSS_OCCURRENCE_STARTED;
            c.newSessionAllowed = Store_FirstStartAllowed(&c.key, time) && new_allowed(&c.key);
            if (record.state == VSS_OCCURRENCE_PENDING) {
                c.validity = Store_FirstStartAllowed(&c.key, time) ? VSS_CANDIDATE_APPLICABLE : VSS_CANDIDATE_UNCERTAIN;
            } else if (c.started) {
                c.validity = record.replayAllowed && comparable(time, &c.eventMeta.original)
                    ? VSS_CANDIDATE_APPLICABLE : VSS_CANDIDATE_UNCERTAIN;
            } else { c.validity = VSS_CANDIDATE_NOT_APPLICABLE; }
        } else {
            VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + (unsigned)(i - Store_Count()));
            if (origin == VSS_PRODUCT && signal == VSS_SIGNAL_ANTI_PINCH) { continue; }
            if (!Store_ReadWarning(origin, signal, &warning) || !warning.lastValidPresent) { continue; }
            c.mode = VSS_MODE_STATEFUL; c.signal = signal; c.value = warning.lastValidValue;
            c.warningMeta = warning.lastValidMeta; c.revision = warning.revision;
            c.validity = warning.validity; c.newSessionAllowed = warning.newSessionAllowed;
            if (!same_time(time, &warning.evaluatedAt)) {
                c.validity = VSS_CANDIDATE_UNCERTAIN; c.newSessionAllowed = false;
            }
        }
        if (n < capacity) { out[n++] = c; } else { complete = false; }
    }
    *written = n;
    return complete;
}
bool Store_BindPlayback(const VssOccurrenceKey *key, uint64_t session,
                       uint64_t attempt, uint64_t expectedRevision)
{
    VssOccurrenceObservation record;
    size_t i;
    bool continuing;
    if (key == NULL || !origin_valid(key->origin) || session == 0u || attempt == 0u ||
        !Store_ReadOccurrence(key, &record) || record.rejectedTombstone ||
        record.revision != expectedRevision || !record.replayAllowed) { return false; }
    continuing = record.state == VSS_OCCURRENCE_STARTED && record.firstStartFact.present &&
        record.firstStartFact.session == session;
    if (!continuing && (record.state != VSS_OCCURRENCE_PENDING || !new_allowed(key))) { return false; }
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
    VssOccurrenceObservation record;
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
    if (!Store_ReadOccurrence(&fact->occurrence, &record) || record.rejectedTombstone ||
        binding_find(&fact->occurrence, fact->session, fact->attempt) == bindingCount ||
        !comparable(&fact->occurredAt, &record.event.meta.original) ||
        fact->occurredAt.earliest < record.event.meta.original.latest ||
        record.state == VSS_OCCURRENCE_EXPIRED || record.finalFact.present ||
        record.state == VSS_OCCURRENCE_COMPLETED || record.state == VSS_OCCURRENCE_INTERRUPTED ||
        record.state == VSS_OCCURRENCE_UNCERTAIN_FINAL || factCount == VSS_PLAYBACK_FACT_CAPACITY) { return VSS_FACT_NOT_APPLIED; }
    state = record.state; replay = record.replayAllowed;
    switch ((VssPlaybackFactKind)fact->kind) {
    case VSS_FACT_FIRST_ACTUAL_START:
        if (record.state != VSS_OCCURRENCE_PENDING || record.firstStartFact.present ||
            bindings[binding_find(&fact->occurrence, fact->session, fact->attempt)].retired ||
            !bindings[binding_find(&fact->occurrence, fact->session, fact->attempt)].attempted ||
            fact->occurredAt.latest - record.event.meta.original.earliest >= record.event.meta.useLimit ||
            (record.inputLossPresent && (!comparable(&fact->occurredAt, &record.inputLossAt) ||
             fact->occurredAt.latest >= record.inputLossAt.earliest))) { return VSS_FACT_NOT_APPLIED; }
        state = VSS_OCCURRENCE_STARTED; first = fact; break;
    case VSS_FACT_WHOLE_COMPLETED:
    case VSS_FACT_WHOLE_INTERRUPTED:
        if (!record.firstStartFact.present || record.firstStartFact.session != fact->session ||
            record.state != VSS_OCCURRENCE_STARTED ||
            fact->occurredAt.earliest < record.firstStartFact.occurredAt.latest) { return VSS_FACT_NOT_APPLIED; }
        state = fact->kind == (uint32_t)VSS_FACT_WHOLE_COMPLETED ? VSS_OCCURRENCE_COMPLETED : VSS_OCCURRENCE_INTERRUPTED;
        replay = false; final = fact; break;
    case VSS_FACT_START_OUTCOME_UNCERTAIN:
        if (record.firstStartFact.present && record.firstStartFact.session != fact->session) { return VSS_FACT_NOT_APPLIED; }
        state = VSS_OCCURRENCE_UNCERTAIN_FINAL; replay = false; final = fact; break;
    case VSS_FACT_NO_START_REEVALUATE:
        if (record.firstStartFact.present || record.state != VSS_OCCURRENCE_PENDING) { return VSS_FACT_NOT_APPLIED; }
        break; /* no age/replay reset and no new Pending */
    default: return VSS_FACT_NOT_APPLIED;
    }
    if (!StorePriv_ApplyRecord(&fact->occurrence, record.revision, state, replay, first, final)) { return VSS_FACT_NOT_APPLIED; }
    facts[factCount++] = *fact;
    return VSS_FACT_APPLIED;
}
VssStoreProgress Store_AdvanceDeadlines(const VssTimeEvidence *time,
                                      const VssStorePlaybackSnapshot *playback)
{
    bool changed = false, reconfirm = false;
    size_t i;
    uint64_t before;
    VssOccurrenceObservation record;
    if (playback == NULL || !origin_valid(playback->origin) || !playback->complete ||
        playback->revision == 0u || playback->notificationPending ||
        playback->storeRevision != Store_ReadRevision(playback->origin)) { return VSS_STORE_RECONFIRM_REQUIRED; }
    before = Store_ReadRevision(playback->origin);
    Store_AdvanceWarningDeadlines(time);
    if (Store_ReadRevision(playback->origin) != before) { changed = true; }
    for (i = 0u; i < Store_Count(); ++i) {
        if (!StorePriv_ReadAt(i, &record)) { reconfirm = true; continue; }
        if (record.rejectedTombstone || record.event.meta.key.origin != playback->origin ||
            record.state != VSS_OCCURRENCE_PENDING) { continue; }
        if (time == NULL || !comparable(time, &record.event.meta.original) ||
            time->earliest < record.event.meta.original.latest || !new_allowed(&record.event.meta.key)) {
            reconfirm = true; continue;
        }
        if (!record.replayAllowed || time->earliest - record.event.meta.original.latest >= record.event.meta.useLimit) {
            if (StorePriv_ApplyRecord(&record.event.meta.key, record.revision,
                VSS_OCCURRENCE_EXPIRED, false, NULL, NULL)) { changed = true; }
            else { reconfirm = true; }
        } else if (time->latest - record.event.meta.original.earliest >= record.event.meta.useLimit) { reconfirm = true; }
    }
    if (time == NULL || time->domain != playback->origin || time->continuity != VSS_CONTINUITY_COMPARABLE ||
        time->earliest > time->latest || time->epoch == 0u) { reconfirm = true; }
    return changed ? VSS_STORE_CHANGED : (reconfirm ? VSS_STORE_RECONFIRM_REQUIRED : VSS_STORE_STABLE);
}
#ifdef VSS_HOST_TEST
void StorePlayback_TestReset(void)
{
    memset(bindings, 0, sizeof bindings); memset(facts, 0, sizeof facts);
    bindingCount = 0u; factCount = 0u; bindingRevision[0] = 1u; bindingRevision[1] = 1u;
}
#endif
