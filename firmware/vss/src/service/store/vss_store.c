#include "../policy/vss_policy_store.h"
#include <string.h>

static VssOccurrenceObservation records[VSS_LEDGER_CAPACITY];
static size_t count;
static VssWarningObservation warnings[VSS_WARNING_CONTEXTS];
static VssRearObservation rear[2];
static VssStoreAdmission apply_state(const VssStateUpdate *state);
static VssStoreAdmission apply_warning_quality(const VssQualityChange *quality);
#if VSS_CFG_HOST_TEST
static bool rejectWarning;
static VssInputSignal rejectedSignal;
#endif

static bool same_key(const VssOccurrenceKey *a, const VssOccurrenceKey *b)
{
    return a->origin == b->origin && a->source == b->source &&
           a->generation == b->generation && a->occurrenceId == b->occurrenceId;
}

static bool valid_key(const VssOccurrenceKey *k)
{
    return (k->origin == VSS_PRODUCT || k->origin == VSS_TEST) &&
           k->source == VSS_SOURCE_DOMAIN && k->generation != 0u &&
           k->occurrenceId != 0u;
}

static size_t find(const VssOccurrenceKey *key)
{
    size_t i;
    for (i = 0u; i < count; ++i) {
        if (same_key(key, &records[i].event.meta.key)) { break; }
    }
    return i;
}

static bool valid_event(const VssOneShotEvent *e)
{
    const VssInputMeta *m = &e->meta;
    return valid_key(&m->key) && e->signal >= VSS_SIGNAL_WELCOME &&
           e->signal <= VSS_SIGNAL_LOCK_ERROR && m->deliveryOrder != 0u &&
           m->original.domain == m->key.origin && m->original.epoch != 0u &&
           m->original.continuity == VSS_CONTINUITY_COMPARABLE &&
           m->original.earliest <= m->original.latest &&
           m->receivedUseLimit > 0u && m->useLimit > 0u && m->useLimit <= 500u &&
           m->useLimit == (m->receivedUseLimit < 500u ? m->receivedUseLimit : 500u) &&
           m->original.earliest <= UINT64_MAX - m->useLimit &&
           m->validUntil == m->original.earliest + m->useLimit &&
           (m->availability == VSS_AVAILABLE || m->availability == VSS_LIMITED);
}

static bool same_meaning(const VssOneShotEvent *a, const VssOneShotEvent *b)
{
    return a->signal == b->signal &&
           a->meta.original.domain == b->meta.original.domain &&
           a->meta.original.epoch == b->meta.original.epoch &&
           a->meta.original.earliest == b->meta.original.earliest &&
           a->meta.original.latest == b->meta.original.latest &&
           a->meta.receivedUseLimit == b->meta.receivedUseLimit &&
           a->meta.relatedCommand == b->meta.relatedCommand;
    /* deliveryOrder/RX/availability are not original occurrence identity. */
}

VssStoreAdmission Store_ApplyInput(const VssOneShotEvent *event,
                                 const VssStateUpdate *state,
                                 const VssQualityChange *quality)
{
    size_t i;
    if ((unsigned)(event != NULL) + (unsigned)(state != NULL) +
        (unsigned)(quality != NULL) != 1u) {
        return VSS_STORE_REJECTED;
    }
    if (state != NULL) { return apply_state(state); }
    if (quality != NULL && quality->signal >= VSS_SIGNAL_ANTI_PINCH &&
        quality->signal <= VSS_SIGNAL_REAR_ACTIVATION) {
        return apply_warning_quality(quality);
    }
    if (event != NULL) {
        if (!valid_event(event)) { return VSS_STORE_REJECTED; }
        i = find(&event->meta.key);
        if (i < count) {
            return !records[i].rejectedTombstone && same_meaning(event, &records[i].event)
                   ? VSS_STORE_DUPLICATE : VSS_STORE_REJECTED;
        }
        if (count == VSS_LEDGER_CAPACITY) { return VSS_STORE_REJECTED; }
        /* Whole record, including both future fact slots, is reserved first. */
        records[count].event = *event;
        records[count].state = VSS_OCCURRENCE_PENDING;
        records[count].replayAllowed = true;
        records[count].revision = 1u;
        ++count;
        return VSS_STORE_ACCEPTED;
    }
    if (!valid_key(&quality->basis.key) ||
        quality->signal < VSS_SIGNAL_WELCOME || quality->signal > VSS_SIGNAL_LOCK_ERROR ||
        (quality->quality != VSS_INPUT_INVALID && quality->quality != VSS_INPUT_STALE)) {
        return VSS_STORE_REJECTED;
    }
    for (i = 0u; i < count; ++i) {
        VssOccurrenceObservation *r = &records[i];
        const VssOccurrenceKey *k = &r->event.meta.key;
        if (k->origin == quality->basis.key.origin &&
            k->source == quality->basis.key.source &&
            k->generation == quality->basis.key.generation &&
            r->event.signal == quality->signal && !r->rejectedTombstone) {
            r->replayAllowed = false;
            if (!r->inputLossPresent) {
                r->inputLossAt = quality->lossAt;
                r->inputLossPresent = true;
                ++r->revision; /* at most once per accepted input record */
            }
            /* INPUT alone never synthesizes Expired/Started/final. */
        }
    }
    return VSS_STORE_UPDATED;
}

void Store_ProtectRejectedEvent(const VssOccurrenceKey *key, VssInputSignal signal,
                                const VssTimeEvidence *at)
{
    if (key == NULL || !valid_key(key) || signal < VSS_SIGNAL_WELCOME ||
        signal > VSS_SIGNAL_LOCK_ERROR || find(key) < count ||
        count == VSS_LEDGER_CAPACITY) { return; }
    memset(&records[count], 0, sizeof records[count]);
    records[count].event.meta.key = *key;
    records[count].event.signal = signal;
    /* A tombstone has no original occurrence time of its own; its time is the
     * rejection time, which is what the reclaim age is measured from. */
    if (at != NULL) { records[count].event.meta.original = *at; }
    records[count].rejectedTombstone = true;
    records[count].revision = 1u;
    ++count; /* Never reused while it exists, including after generation loss. */
}

size_t Store_Count(void) { return count; }

bool StorePriv_ReadAt(size_t index, VssOccurrenceObservation *out)
{
    if (index >= count || out == NULL) { return false; }
    *out = records[index];
    return true;
}

bool StorePriv_RemoveAt(size_t index)
{
    if (index >= count) { return false; }
    --count;
    if (index < count) {
        memmove(&records[index], &records[index + 1u], (count - index) * sizeof records[0]);
    }
    memset(&records[count], 0, sizeof records[count]);
    return true;
}

bool StorePriv_ApplyRecord(const VssOccurrenceKey *key, uint64_t expectedRevision,
    VssOccurrenceState state, bool replayAllowed,
    const VssReservedPlaybackFact *first, const VssReservedPlaybackFact *final)
{
    size_t i;
    if (key == NULL) { return false; }
    i = find(key);
    if (i == count || records[i].rejectedTombstone ||
        records[i].revision != expectedRevision || expectedRevision == UINT64_MAX ||
        (!records[i].replayAllowed && replayAllowed)) { return false; }
    records[i].state = state;
    records[i].replayAllowed = replayAllowed;
    if (first != NULL) { records[i].firstStartFact = *first; }
    if (final != NULL) { records[i].finalFact = *final; }
    ++records[i].revision;
    return true;
}

bool Store_ReadOccurrence(const VssOccurrenceKey *key, VssOccurrenceObservation *out)
{
    size_t i;
    if (key == NULL || out == NULL) { return false; }
    i = find(key);
    if (i == count) { return false; }
    *out = records[i];
    return true;
}

bool Store_FirstStartAllowed(const VssOccurrenceKey *key, const VssTimeEvidence *now)
{
    size_t i;
    const VssOccurrenceObservation *r;
    const VssFactTime *original;
    if (key == NULL || now == NULL) { return false; }
    i = find(key);
    if (i == count) { return false; }
    r = &records[i];
    original = &r->event.meta.original;
    return !r->rejectedTombstone && r->replayAllowed &&
           r->state == VSS_OCCURRENCE_PENDING &&
           now->continuity == VSS_CONTINUITY_COMPARABLE &&
           now->domain == original->domain && now->epoch == original->epoch &&
           now->earliest <= now->latest && now->earliest >= original->latest &&
           now->latest - original->earliest < r->event.meta.useLimit;
}

static bool warning_address(VssOrigin origin, VssInputSignal signal)
{
    return (origin == VSS_PRODUCT || origin == VSS_TEST) &&
           signal >= VSS_SIGNAL_ANTI_PINCH && signal <= VSS_SIGNAL_REAR_ACTIVATION;
}

static size_t warning_index(VssOrigin origin, VssInputSignal signal)
{ return (size_t)origin * 4u + (size_t)signal - (size_t)VSS_SIGNAL_ANTI_PINCH; }

static uint32_t warning_ttl(VssInputSignal signal)
{
    static const uint32_t ttl[4] = {250u, 1000u, 150u, 500u};
    return ttl[(size_t)signal - (size_t)VSS_SIGNAL_ANTI_PINCH];
}

static uint32_t warning_hold(VssInputSignal signal)
{
    static const uint32_t hold[4] = {250u, 500u, 150u, 0u};
    return hold[(size_t)signal - (size_t)VSS_SIGNAL_ANTI_PINCH];
}

static bool comparable(const VssFactTime *a, const VssFactTime *b)
{
    return a->continuity == VSS_CONTINUITY_COMPARABLE &&
           b->continuity == VSS_CONTINUITY_COMPARABLE && a->epoch != 0u &&
           a->domain == b->domain && a->epoch == b->epoch &&
           a->earliest <= a->latest && b->earliest <= b->latest;
}

static bool newer_warning(const VssWarningMeta *a, const VssWarningMeta *b)
{
    return a->origin == b->origin && a->domainBoot == b->domainBoot &&
           a->evidenceSource == b->evidenceSource && a->evidenceBoot == b->evidenceBoot &&
           a->evidenceSequence > b->evidenceSequence;
}

/* A new DOMAIN generation replaces a warning whose provenance the old
 * generation lost; within one generation only a newer evaluation does. */
static bool replaces_warning(const VssWarningMeta *m, const VssWarningObservation *w)
{
    return !w->lastValidPresent || newer_warning(m, &w->lastValidMeta) ||
           (w->provenanceLost && m->domainBoot != w->lastValidMeta.domainBoot);
}

/* The rear fence demands a newer rear evaluation than the fenced one. Across
 * DOMAIN generations only the rear evidence identity is compared. */
static bool passes_rear_fence(const VssWarningMeta *m, const VssRearObservation *g)
{
    if (!g->fencePresent || newer_warning(m, &g->riskFence)) { return true; }
    return m->origin == g->riskFence.origin && m->domainBoot != g->riskFence.domainBoot &&
           m->evidenceSource == g->riskFence.evidenceSource &&
           (m->evidenceBoot != g->riskFence.evidenceBoot ||
            m->evidenceSequence > g->riskFence.evidenceSequence);
}

static bool fresh_warning(const VssWarningObservation *w, const VssTimeEvidence *now)
{
    return w->lastValidPresent && w->quality == VSS_INPUT_VALID &&
           !w->provenanceLost && !w->inputIncomplete && comparable(now, &w->lastValidMeta.original) &&
           now->earliest >= w->lastValidMeta.original.latest &&
           now->latest < w->lastValidMeta.validUntil;
}

static void close_rear(VssOrigin origin)
{
    VssRearObservation *g = &rear[(size_t)origin];
    VssWarningObservation *r = &warnings[warning_index(origin, VSS_SIGNAL_REAR_RISK)];
    bool changed = g->riskApplicable;
    /* The fence moves to any rear evidence that would pass it, so each
     * generation fences its own last evaluation. */
    if (r->lastValidPresent && passes_rear_fence(&r->lastValidMeta, g)) {
        g->riskFence = r->lastValidMeta;
        g->fencePresent = true;
        changed = true;
    }
    g->riskApplicable = false;
    if (r->lastValidPresent && !r->removed) {
        r->removed = true;
        r->newSessionAllowed = false;
        r->validity = VSS_CANDIDATE_NOT_APPLICABLE;
        ++r->revision;
    }
    if (changed) { ++g->revision; }
}

static bool state_value_valid(VssInputSignal signal, VssInputValue value)
{
    if (signal == VSS_SIGNAL_REAR_ACTIVATION) {
        return value == VSS_VALUE_ENABLED || value == VSS_VALUE_DISABLED;
    }
    if (signal == VSS_SIGNAL_REAR_RISK) {
        return value == VSS_VALUE_CLEAR || value == VSS_VALUE_CAUTION || value == VSS_VALUE_EMERGENCY;
    }
    return value == VSS_VALUE_ACTIVE || value == VSS_VALUE_CLEAR;
}

static VssStoreAdmission apply_state(const VssStateUpdate *state)
{
    const VssWarningMeta *m = &state->meta;
    VssWarningObservation *w;
    VssRearObservation *g;
    if (!warning_address(m->origin, state->signal) ||
        (m->origin == VSS_PRODUCT && state->signal == VSS_SIGNAL_ANTI_PINCH) ||
        !state_value_valid(state->signal, state->value) || m->domainBoot == 0u ||
        m->deliveryOrder == 0u || m->evidenceBoot == 0u || m->evidenceSequence == 0u ||
        m->evidenceSource != (VssEvidenceSource)((unsigned)state->signal - (unsigned)VSS_SIGNAL_ANTI_PINCH) ||
        m->original.domain != m->origin || !comparable(&m->original, &state->evaluatedAt) ||
        state->evaluatedAt.earliest < m->original.latest ||
        m->useLimit != warning_ttl(state->signal) ||
        m->original.earliest > UINT64_MAX - m->useLimit ||
        m->validUntil != m->original.earliest + m->useLimit ||
        state->evaluatedAt.latest >= m->validUntil || m->reason != VSS_REASON_NONE ||
        (state->signal == VSS_SIGNAL_REAR_ACTIVATION
         ? (m->availability != VSS_AVAILABILITY_NOT_PROVIDED || m->evidenceBoot != m->domainBoot)
         : (m->availability != VSS_AVAILABLE && m->availability != VSS_LIMITED))) {
        return VSS_STORE_REJECTED;
    }
#if VSS_CFG_HOST_TEST
    if (rejectWarning && rejectedSignal == state->signal) {
        w = &warnings[warning_index(m->origin, state->signal)];
        w->inputIncomplete = true; w->newSessionAllowed = false; ++w->revision;
        rejectWarning = false; return VSS_STORE_REJECTED;
    }
#endif
    w = &warnings[warning_index(m->origin, state->signal)];
    if (!replaces_warning(m, w)) {
        return VSS_STORE_REJECTED;
    }
    w->lastValidPresent = true;
    w->lastValidValue = state->value;
    w->lastValidMeta = *m;
    w->quality = VSS_INPUT_VALID;
    w->firstLossPresent = false;
    w->holdAnchorPresent = false;
    w->holdExpired = false;
    w->provenanceLost = false;
    w->inputIncomplete = false;
    w->removed = state->value == VSS_VALUE_CLEAR || state->value == VSS_VALUE_DISABLED;
    w->evaluatedAt = state->evaluatedAt;
    ++w->revision;
    g = &rear[(size_t)m->origin];
    if (state->signal == VSS_SIGNAL_REAR_ACTIVATION && state->value == VSS_VALUE_DISABLED) {
        close_rear(m->origin);
    }
    if (state->signal == VSS_SIGNAL_REAR_RISK) {
        const VssWarningObservation *activation = &warnings[warning_index(m->origin, VSS_SIGNAL_REAR_ACTIVATION)];
        bool allowed = fresh_warning(activation, &state->evaluatedAt) &&
                       activation->lastValidValue == VSS_VALUE_ENABLED &&
                       passes_rear_fence(m, g);
        if (g->riskApplicable != allowed) { ++g->revision; }
        g->riskApplicable = allowed;
        if (!allowed) { close_rear(m->origin); }
    }
    return VSS_STORE_UPDATED;
}

static VssStoreAdmission apply_warning_quality(const VssQualityChange *q)
{
    const VssWarningMeta *m = &q->warningBasis;
    VssWarningObservation *w;
    if (!warning_address(m->origin, q->signal) || m->domainBoot == 0u ||
        q->quality < VSS_INPUT_INVALID || q->quality > VSS_INPUT_UNASSESSED) {
        return VSS_STORE_REJECTED;
    }
#if VSS_CFG_HOST_TEST
    if (rejectWarning && rejectedSignal == q->signal) {
        w = &warnings[warning_index(m->origin, q->signal)];
        w->inputIncomplete = true; w->newSessionAllowed = false; ++w->revision;
        rejectWarning = false; return VSS_STORE_REJECTED;
    }
#endif
    w = &warnings[warning_index(m->origin, q->signal)];
    if (w->lastValidPresent && (m->domainBoot != w->lastValidMeta.domainBoot ||
        m->evidenceSource != w->lastValidMeta.evidenceSource ||
        m->evidenceBoot != w->lastValidMeta.evidenceBoot ||
        m->evidenceSequence < w->lastValidMeta.evidenceSequence)) { return VSS_STORE_REJECTED; }
    w->quality = q->quality;
    if (w->lastValidPresent && !w->firstLossPresent) {
        w->firstLossPresent = true;
        w->firstLossAt = q->lossAt;
        if (comparable(&q->lossAt, &w->lastValidMeta.original)) {
            w->holdAnchorPresent = true;
            w->holdAnchor = q->lossAt.earliest < w->lastValidMeta.validUntil
                          ? q->lossAt.earliest : w->lastValidMeta.validUntil;
        }
    }
    w->provenanceLost = w->provenanceLost || q->provenanceLost;
    w->newSessionAllowed = false; /* until the owner completes deadline evaluation */
    w->validity = VSS_CANDIDATE_UNCERTAIN;
    ++w->revision;
    if (q->signal == VSS_SIGNAL_REAR_ACTIVATION ||
        (q->signal == VSS_SIGNAL_REAR_RISK && q->provenanceLost)) { close_rear(m->origin); }
    return VSS_STORE_UPDATED;
}

static VssCandidateValidity warning_candidate(VssWarningObservation *w,
                                              VssInputSignal signal, const VssTimeEvidence *now)
{
    uint64_t deadline;
    if (!w->lastValidPresent || w->removed) { return VSS_CANDIDATE_NOT_APPLICABLE; }
    if (w->holdExpired) { return VSS_CANDIDATE_EXPIRED; }
    if (w->provenanceLost || w->inputIncomplete || !comparable(now, &w->lastValidMeta.original) ||
        now->earliest < w->lastValidMeta.original.latest) { return VSS_CANDIDATE_UNCERTAIN; }
    if (w->quality == VSS_INPUT_VALID) {
        return fresh_warning(w, now) ? VSS_CANDIDATE_APPLICABLE : VSS_CANDIDATE_UNCERTAIN;
    }
    if (!w->firstLossPresent || !w->holdAnchorPresent ||
        w->holdAnchor > UINT64_MAX - warning_hold(signal)) { return VSS_CANDIDATE_UNCERTAIN; }
    deadline = w->holdAnchor + warning_hold(signal);
    if (now->latest < deadline) { return VSS_CANDIDATE_APPLICABLE; }
    if (now->earliest >= deadline) { w->holdExpired = true; return VSS_CANDIDATE_EXPIRED; }
    return VSS_CANDIDATE_UNCERTAIN;
}

void Store_AdvanceWarningDeadlines(const VssTimeEvidence *time)
{
    size_t i;
    if (time == NULL || (time->domain != VSS_PRODUCT && time->domain != VSS_TEST)) {
        for (i = 0u; i < VSS_WARNING_CONTEXTS; ++i) {
            VssWarningObservation *w = &warnings[i];
            if (w->lastValidPresent && !w->removed && !w->holdExpired) {
                if (w->validity != VSS_CANDIDATE_UNCERTAIN || w->newSessionAllowed) { ++w->revision; }
                w->validity = VSS_CANDIDATE_UNCERTAIN; w->newSessionAllowed = false;
                w->evaluatedAt.continuity = VSS_CONTINUITY_UNKNOWN;
            }
        }
        return;
    }
    /* The activation is always evaluated first, even on uncertain time. */
    {
        const VssWarningObservation *a = &warnings[warning_index(time->domain, VSS_SIGNAL_REAR_ACTIVATION)];
        if (a->lastValidPresent && (a->quality != VSS_INPUT_VALID ||
            a->lastValidValue == VSS_VALUE_DISABLED || a->provenanceLost)) { close_rear(time->domain); }
    }
    for (i = 0u; i < 4u; ++i) {
        VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + (unsigned)i);
        VssWarningObservation *w = &warnings[(size_t)time->domain * 4u + i];
        VssCandidateValidity validity = warning_candidate(w, signal, time);
        bool allowed;
        if (signal == VSS_SIGNAL_REAR_RISK && !rear[(size_t)time->domain].riskApplicable &&
            validity == VSS_CANDIDATE_APPLICABLE) { validity = VSS_CANDIDATE_NOT_APPLICABLE; }
        if (signal == VSS_SIGNAL_REAR_RISK && validity == VSS_CANDIDATE_APPLICABLE &&
            !fresh_warning(&warnings[warning_index(time->domain, VSS_SIGNAL_REAR_ACTIVATION)], time)) {
            validity = VSS_CANDIDATE_UNCERTAIN;
        }
        allowed = validity == VSS_CANDIDATE_APPLICABLE && signal != VSS_SIGNAL_REAR_ACTIVATION;
        if (w->validity != validity || w->newSessionAllowed != allowed) { ++w->revision; }
        w->validity = validity;
        w->newSessionAllowed = allowed;
        w->evaluatedAt = *time;
    }
}

bool Store_ReadWarning(VssOrigin origin, VssInputSignal signal, VssWarningObservation *out)
{
    if (!warning_address(origin, signal) || out == NULL) { return false; }
    *out = warnings[warning_index(origin, signal)];
    if (out->revision == 0u && !out->lastValidPresent) { out->quality = VSS_INPUT_UNASSESSED; }
    return true;
}

bool Store_ReadRear(VssOrigin origin, VssRearObservation *out)
{
    if ((origin != VSS_PRODUCT && origin != VSS_TEST) || out == NULL) { return false; }
    *out = rear[(size_t)origin];
    return true;
}

#if VSS_CFG_HOST_TEST
void Store_TestReset(void)
{
    memset(records, 0, sizeof records);
    memset(warnings, 0, sizeof warnings);
    memset(rear, 0, sizeof rear);
    rejectWarning = false;
    count = 0u;
}
void Store_TestRejectWarningOnce(VssInputSignal signal)
{ rejectWarning = true; rejectedSignal = signal; }
void Store_TestSeedFacts(const VssOccurrenceKey *key, VssOccurrenceState state,
                        const VssReservedPlaybackFact *first,
                        const VssReservedPlaybackFact *final)
{
    size_t i = find(key);
    if (i < count) {
        records[i].state = state;
        if (first != NULL) { records[i].firstStartFact = *first; }
        if (final != NULL) { records[i].finalFact = *final; }
        ++records[i].revision;
    }
}
#endif
