#include "vss_playback.h"
#include <limits.h>
#include <string.h>

typedef struct {
    VssAudioKey key;
    size_t planPosition; /* immutable repeat * cueCount + cue, not Attempt ID */
    bool submitted;
    bool actual;
    bool closed;
    bool noStart;
    bool clean;
    bool bound;
    VssFactTime permissionAt;
    VssFactTime endedAt;
    VssAudioOutput actualEvidence;
} Attempt;
typedef struct {
    bool present;
    bool pending;
    bool resultPresent;
    VssPlaybackFact fact;
    VssFactApplication result;
} Notice;
typedef struct {
    VssPlaybackObservation view;
    Attempt attempts[VSS_PB_ATTEMPTS];
    Notice notices[VSS_PB_NOTICES];
    bool performed[VSS_PB_ATTEMPTS]; /* bounded adopted plan positions */
    bool blocked; /* first revocation occurred, independently of its time */
    bool blockedAtPresent;
    VssFactTime blockedAt;
    bool normalEnd;
    bool finalMade;
    bool physicalActual;
    VssPlaybackDiagnostic diagnostics[VSS_PB_DIAGNOSTICS];
    size_t diagnosticCount;
    VssPlaybackDiagnostic firstLost;
    VssPlaybackDiagnostic latestLost;
} Playback;
static Playback pb;
static uint64_t nextSession = 1u;
static uint64_t nextAttempt = 1u;
static uint64_t nextFact = 1u;
static uint64_t revision = 1u;

static bool time_valid(const VssFactTime *t)
{
    return t != NULL && (t->domain == VSS_PRODUCT || t->domain == VSS_TEST) &&
        t->continuity == VSS_CONTINUITY_COMPARABLE && t->earliest <= t->latest;
}
static bool comparable(const VssFactTime *a, const VssFactTime *b)
{
    return time_valid(a) && time_valid(b) && a->domain == b->domain && a->epoch == b->epoch;
}
static bool same_time(const VssFactTime *a, const VssFactTime *b)
{
    return comparable(a, b) && a->earliest == b->earliest && a->latest == b->latest;
}
static bool key_equal(const VssAudioKey *a, const VssAudioKey *b)
{
    return a->session == b->session && a->attempt == b->attempt &&
        a->operation == b->operation && a->segment == b->segment && a->origin == b->origin;
}
static bool ending(void)
{
    return pb.view.phase == VSS_PB_STOPPING || pb.view.phase == VSS_PB_ABORTING ||
        pb.view.phase == VSS_PB_QUARANTINED || pb.view.phase == VSS_PB_RETIRED;
}
static void changed(void)
{
    if (revision != UINT64_MAX) { ++revision; }
    pb.view.revision = revision;
}
static void diagnose(VssPlaybackDiagCode code, const VssAudioKey *key,
                     const VssFactTime *time, const VssAudioOutput *evidence)
{
    VssPlaybackDiagnostic d;
    memset(&d, 0, sizeof(d)); d.code = code;
    if (key != NULL) { d.key = *key; }
    if (time != NULL) { d.occurredAt = *time; }
    if (evidence != NULL) { d.evidencePresent = true; d.evidence = *evidence; }
    if (pb.diagnosticCount < VSS_PB_DIAGNOSTICS) {
        pb.diagnostics[pb.diagnosticCount++] = d;
    } else {
        if (!pb.view.diagnosticsLost) { pb.firstLost = d; }
        pb.latestLost = d; pb.view.diagnosticsLost = true;
        if (pb.view.diagnosticsLostCount != UINT32_MAX) { ++pb.view.diagnosticsLostCount; }
    }
}
static void stop(VssTerminationReason reason, const VssTimeEvidence *time)
{
    size_t i;
    if (!pb.view.present || pb.view.phase == VSS_PB_RETIRED) { return; }
    if (!pb.blocked) {
        pb.blocked = true;
        pb.blockedAtPresent = time_valid(time);
        if (pb.blockedAtPresent) { pb.blockedAt = *time; }
        /* A later STOP cannot replace an unknown first revocation with now. */
    }
    pb.view.startAllowed = false;
    if (pb.view.phase != VSS_PB_QUARANTINED && pb.view.phase != VSS_PB_ABORTING) {
        pb.view.phase = reason == VSS_STOP_FAILURE ? VSS_PB_ABORTING : VSS_PB_STOPPING;
    }
    for (i = 0u; i < pb.view.attempts; ++i) {
        (void)VssAudio_Stop(&pb.attempts[i].key, time);
    }
    changed();
}
static bool basis_current(const VssReadBasis *basis, const VssTimeEvidence *time)
{
    size_t i;
    for (i = 0u; i < VSS_PB_NOTICES; ++i) {
        if (pb.notices[i].pending) { return false; }
    }
    return basis != NULL && time_valid(time) && basis->validity == VSS_READ_VALID &&
        basis->origin == time->domain && same_time(&basis->time, time) &&
        basis->storeRevision != 0u && basis->storeRevision == Store_ReadRevision(basis->origin) &&
        basis->pbRevision == revision && revision != UINT64_MAX;
}
static const VssCandidateObservation *find_candidate(const VssCandidateObservation *items,
    size_t count, const VssCandidateObservation *which)
{
    size_t i;
    if (count > VSS_CANDIDATE_CAPACITY || (count != 0u && items == NULL)) { return NULL; }
    for (i = 0u; i < count; ++i) {
        if (VssPolicy_SameCandidate(&items[i], which)) { return &items[i]; }
    }
    return NULL;
}
static bool candidate_permitted(const VssCandidateObservation *candidate,
                               const VssTimeEvidence *time)
{
    if (candidate == NULL) { return false; }
    if (candidate->mode == VSS_MODE_EVENT && pb.view.present &&
        pb.view.startKnowledge == VSS_START_ACTUAL_CONFIRMED &&
        VssPolicy_SameCandidate(candidate, &pb.view.candidate)) {
        VssOccurrenceObservation record;
        return Store_ReadOccurrence(&candidate->key, &record) && record.replayAllowed &&
            !record.inputLossPresent && record.state == VSS_OCCURRENCE_STARTED &&
            comparable(time, &record.event.meta.original);
    }
    if (candidate->validity != VSS_CANDIDATE_APPLICABLE) { return false; }
    if (candidate->mode == VSS_MODE_STATEFUL) {
        return candidate->newSessionAllowed && (!pb.view.present ||
            pb.view.phase == VSS_PB_RETIRED || candidate->value == pb.view.candidate.value);
    }
    /* Admission of a new Session and permission of its already protected
     * first START are distinct; binding itself removes newSessionAllowed. */
    return Store_FirstStartAllowed(&candidate->key, time);
}
static bool notices_busy(void)
{
    size_t i;
    for (i = 0u; i < VSS_PB_NOTICES; ++i) {
        if (pb.notices[i].pending || pb.notices[i].resultPresent) { return true; }
    }
    return false;
}
static bool make_fact(VssPlaybackFactKind kind, const Attempt *attempt,
                      const VssFactTime *time)
{
    size_t i;
    if (pb.view.candidate.mode != VSS_MODE_EVENT) { return true; }
    for (i = 0u; i < VSS_PB_NOTICES; ++i) {
        Notice *n = &pb.notices[i];
        if (!n->present && nextFact != UINT64_MAX) {
            memset(n, 0, sizeof(*n)); n->present = true; n->pending = true;
            n->fact.present = true; n->fact.kind = (uint32_t)kind;
            n->fact.fact = nextFact++; n->fact.occurrence = pb.view.candidate.key;
            n->fact.session = pb.view.session; n->fact.attempt = attempt->key.attempt;
            n->fact.occurredAt = *time; n->fact.basisRevision = revision;
            return true;
        }
    }
    diagnose(VSS_PB_DIAG_CAPACITY, &attempt->key, time, NULL);
    return false;
}
static bool new_attempt(const VssTimeEvidence *time)
{
    Attempt *a;
    VssAudioState result;
    VssOccurrenceObservation record;
    uint32_t asset = pb.view.plan.cues[pb.view.cue].asset;
    if (pb.view.attempts >= VSS_PB_ATTEMPTS || nextAttempt == UINT64_MAX || asset > 7u) {
        diagnose(VSS_PB_DIAG_CAPACITY, &pb.view.attempt, time, NULL); return false;
    }
    a = &pb.attempts[pb.view.attempts]; memset(a, 0, sizeof(*a));
    a->key.session = pb.view.session; a->key.attempt = nextAttempt;
    a->key.operation = nextAttempt++; a->key.origin = pb.view.candidate.origin;
    a->key.segment = (uint32_t)pb.view.attempts + 1u;
    a->planPosition = (size_t)pb.view.repeat * pb.view.plan.cueCount + pb.view.cue;
    if (pb.view.candidate.mode == VSS_MODE_EVENT) {
        if (!Store_ReadOccurrence(&pb.view.candidate.key, &record) ||
            !Store_BindPlayback(&pb.view.candidate.key, pb.view.session, a->key.attempt,
                                record.revision)) { return false; }
        a->bound = true;
    }
    ++pb.view.attempts; pb.view.attempt = a->key;
    pb.view.phase = VSS_PB_PREPARING; changed();
    /* Identity, original Meta, whole plan and both notification slots protected
     * before calling a backend which could partially take effect. */
    result = VssAudio_Prepare(&a->key, (uint8_t)asset, time);
    if (result != VSS_AUDIO_PREPARED && result != VSS_AUDIO_SUBMITTED) {
        stop(VSS_STOP_FAILURE, time);
    }
    return true;
}

VssRequestDisposition Playback_RequestTransition(
    const VssSelectionDecision *decision, const uint64_t *terminateSession,
    VssTerminationReason reason, const VssCandidateObservation *candidates,
    size_t candidateCount, const VssReadBasis *basis, const VssTimeEvidence *time)
{
    const VssCandidateObservation *current;
    if ((decision == NULL) == (terminateSession == NULL) ||
        (decision != NULL && reason != VSS_STOP_NONE) ||
        (terminateSession != NULL && (reason < VSS_STOP_NONE || reason > VSS_STOP_FAILURE))) {
        return VSS_REQUEST_REJECTED;
    }
    if (terminateSession != NULL) {
        if (!pb.view.present || *terminateSession != pb.view.session) { return VSS_REQUEST_REJECTED; }
        if (pb.view.phase == VSS_PB_RETIRED) { return VSS_REQUEST_KEPT; }
        stop(reason, time); return VSS_REQUEST_ADOPTED;
    }
    if (!basis_current(basis, time) || decision->basis.storeRevision != basis->storeRevision ||
        decision->basis.pbRevision != basis->pbRevision ||
        decision->basis.origin != basis->origin ||
        !same_time(&decision->basis.time, time)) { return VSS_REQUEST_WAIT; }
    if (decision->action == VSS_SELECTION_WAIT || decision->action == VSS_SELECTION_RECOLLECT) {
        return VSS_REQUEST_WAIT;
    }
    if (decision->action == VSS_SELECTION_KEEP) {
        if (!basis->startsAllowed || !pb.view.startAllowed) {
            stop(VSS_STOP_CONDITION, time);
            return VSS_REQUEST_WAIT;
        }
        current = find_candidate(candidates, candidateCount, &pb.view.candidate);
        return pb.view.present && !ending() && candidate_permitted(current, time) ?
            VSS_REQUEST_KEPT : VSS_REQUEST_WAIT;
    }
    if (decision->action == VSS_SELECTION_REPLACE) {
        if (pb.view.present && pb.view.phase != VSS_PB_RETIRED) { stop(VSS_STOP_POLICY, time); }
        return VSS_REQUEST_WAIT; /* no saved winner; fresh selection after retirement */
    }
    if (decision->action != VSS_SELECTION_CHOOSE || !decision->candidatePresent ||
        !basis->startsAllowed || !VssPolicy_ValidPlan(&decision->plan) || !decision->plan.available ||
        decision->plan.cueCount * decision->plan.repeatCount > VSS_PB_ATTEMPTS ||
        decision->plan.signal != decision->candidate.signal ||
        decision->plan.value != decision->candidate.value) { return VSS_REQUEST_WAIT; }
    if ((pb.view.present && pb.view.phase != VSS_PB_RETIRED) || notices_busy() ||
        pb.view.diagnosticsLost || nextSession == UINT64_MAX || nextFact >= UINT64_MAX - 2u) {
        return VSS_REQUEST_WAIT;
    }
    current = find_candidate(candidates, candidateCount, &decision->candidate);
    if (current == NULL || current->revision != decision->candidate.revision ||
        !current->newSessionAllowed || !candidate_permitted(current, time)) { return VSS_REQUEST_WAIT; }
    /* Retain diagnostics across owner reuse; clear only fully published notices. */
    memset(&pb.view, 0, sizeof(pb.view)); memset(pb.attempts, 0, sizeof(pb.attempts));
    memset(pb.notices, 0, sizeof(pb.notices)); pb.blockedAtPresent = false;
    pb.blocked = false;
    memset(pb.performed, 0, sizeof(pb.performed));
    pb.normalEnd = false; pb.finalMade = false; pb.physicalActual = false;
    pb.view.present = true; pb.view.session = nextSession++;
    pb.view.candidate = *current; pb.view.plan = decision->plan;
    pb.view.startKnowledge = VSS_START_PENDING_OR_UNKNOWN; pb.view.startAllowed = true;
    changed();
    if (!new_attempt(time)) { stop(VSS_STOP_FAILURE, time); }
    return VSS_REQUEST_ADOPTED; /* adoption is not successful prepare or actual */
}

static Attempt *find_attempt(const VssAudioKey *key)
{
    size_t i;
    for (i = 0u; i < pb.view.attempts; ++i) {
        if (key_equal(&pb.attempts[i].key, key)) { return &pb.attempts[i]; }
    }
    return NULL;
}
/* Return 1 legal, -1 definitely outside, 0 insufficient/overlap. */
static int historical_permission(const Attempt *a, const VssAudioOutput *output)
{
    const VssFactTime *t = &output->occurredAt;
    if (!a->submitted || !comparable(t, &a->permissionAt) ||
        t->earliest < a->permissionAt.latest) { return 0; }
    if (pb.blocked) {
        if (!pb.blockedAtPresent || !comparable(t, &pb.blockedAt)) { return 0; }
        if (t->earliest >= pb.blockedAt.latest) { return -1; }
        if (t->latest >= pb.blockedAt.earliest) { return 0; }
    }
    if (pb.view.candidate.mode == VSS_MODE_EVENT) {
        VssOccurrenceObservation r;
        const VssInputMeta *m = &pb.view.candidate.eventMeta;
        if (!Store_ReadOccurrence(&pb.view.candidate.key, &r)) { return 0; }
        if (r.inputLossPresent) {
            if (!comparable(t, &r.inputLossAt)) { return 0; }
            if (t->earliest >= r.inputLossAt.latest) { return -1; }
            if (t->latest >= r.inputLossAt.earliest) { return 0; }
        }
        if (pb.view.startKnowledge != VSS_START_ACTUAL_CONFIRMED) {
            if (!comparable(t, &m->original)) { return 0; }
            if (t->earliest >= m->original.latest && t->latest >= m->original.earliest &&
                t->latest - m->original.earliest < m->useLimit) { return 1; }
            if ((t->earliest >= m->original.latest &&
                 t->earliest - m->original.latest >= m->useLimit) ||
                t->latest < m->original.earliest) { return -1; }
            return 0;
        }
    } else {
        VssWarningObservation w;
        static const uint32_t hold[4] = {250u, 500u, 150u, 0u};
        unsigned index = (unsigned)pb.view.candidate.signal - (unsigned)VSS_SIGNAL_ANTI_PINCH;
        if (index >= 3u || !Store_ReadWarning(pb.view.candidate.origin,
                                             pb.view.candidate.signal, &w)) { return 0; }
        if (w.provenanceLost || w.inputIncomplete) { return 0; }
        if (w.lastValidValue != pb.view.candidate.value) {
            if (!comparable(t, &w.lastValidMeta.original)) { return 0; }
            if (t->earliest >= w.lastValidMeta.original.latest) { return -1; }
            if (t->latest >= w.lastValidMeta.original.earliest) { return 0; }
        } else if (w.firstLossPresent) {
            if (!w.holdAnchorPresent || !comparable(t, &w.firstLossAt) ||
                w.holdAnchor > UINT64_MAX - hold[index]) { return 0; }
            if (t->earliest >= w.holdAnchor + hold[index]) { return -1; }
            if (t->latest >= w.holdAnchor + hold[index]) { return 0; }
        } else if (t->latest >= w.lastValidMeta.validUntil) { return 0; }
        if (pb.view.candidate.signal == VSS_SIGNAL_REAR_RISK) {
            VssWarningObservation activation;
            if (!Store_ReadWarning(pb.view.candidate.origin, VSS_SIGNAL_REAR_ACTIVATION,
                                   &activation) || activation.lastValidValue != VSS_VALUE_ENABLED ||
                activation.quality != VSS_INPUT_VALID || activation.provenanceLost ||
                !comparable(t, &activation.lastValidMeta.original) ||
                t->latest >= activation.lastValidMeta.validUntil) { return 0; }
        }
    }
    return 1;
}
static void apply_output(const VssAudioOutput *output, const VssTimeEvidence *now)
{
    Attempt *a = find_attempt(&output->key);
    int legal;
    if (a == NULL || pb.view.phase == VSS_PB_RETIRED ||
        (pb.view.startKnowledge == VSS_START_UNCERTAIN_FINAL && output->kind == VSS_AUDIO_OUTPUT_START)) {
        diagnose(VSS_PB_DIAG_ATTRIBUTION, &output->key, &output->occurredAt, output); return;
    }
    if (output->fact == 0u || output->coverage != VSS_AUDIO_COVERAGE_SUFFICIENT ||
        !comparable(&output->occurredAt, &output->activationAt) ||
        output->occurredAt.earliest < output->activationAt.latest) {
        diagnose(VSS_PB_DIAG_COVERAGE, &output->key, &output->occurredAt, output);
        stop(VSS_STOP_FAILURE, now); return;
    }
#ifndef VSS_HOST_TEST
    /* No target producer has proved physical coverage. Defense in depth. */
    diagnose(VSS_PB_DIAG_COVERAGE, &output->key, &output->occurredAt, output);
    stop(VSS_STOP_FAILURE, now); return;
#endif
    if (output->kind == VSS_AUDIO_OUTPUT_START) {
        VssAudioObservation lower;
        if (!VssAudio_Read(&a->key, &lower) || !lower.activationPresent ||
            !same_time(&lower.activationAt, &output->activationAt) ||
            output->occurredAt.earliest <= output->activationAt.latest || a->noStart) {
            diagnose(VSS_PB_DIAG_ATTRIBUTION, &output->key, &output->occurredAt, output);
            stop(VSS_STOP_FAILURE, now); return;
        }
        if (a->actual) { return; }
        a->actual = true; a->actualEvidence = *output; pb.physicalActual = true;
        legal = historical_permission(a, output);
        if (legal != 1) {
            diagnose(legal < 0 ? VSS_PB_DIAG_VIOLATION : VSS_PB_DIAG_TIME_UNCERTAIN,
                     &output->key, &output->occurredAt, output);
            stop(VSS_STOP_FAILURE, now); return;
        }
        if (pb.view.startKnowledge != VSS_START_ACTUAL_CONFIRMED) {
            if (!make_fact(VSS_FACT_FIRST_ACTUAL_START, a, &output->occurredAt)) {
                stop(VSS_STOP_FAILURE, now); return;
            }
            pb.view.startKnowledge = VSS_START_ACTUAL_CONFIRMED;
        }
        /* Evidence fulfils its protected plan position. A Session FIRST or
         * a different position's actual cannot fulfil this cue/repetition.
         * This does not require every possible Attempt to have an actual. */
        pb.performed[a->planPosition] = true;
        if (!ending()) { pb.view.phase = VSS_PB_ACTIVE; }
        changed();
    } else if ((output->kind == VSS_AUDIO_OUTPUT_NO_START ||
                output->kind == VSS_AUDIO_OUTPUT_TERMINATED) && output->futureBlocked) {
        if (output->kind == VSS_AUDIO_OUTPUT_NO_START && a->actual) {
            diagnose(VSS_PB_DIAG_COVERAGE, &output->key, &output->occurredAt, output);
            stop(VSS_STOP_FAILURE, now); return;
        }
        a->closed = true; a->noStart = output->kind == VSS_AUDIO_OUTPUT_NO_START;
        a->endedAt = output->occurredAt; changed();
    } else {
        diagnose(VSS_PB_DIAG_COVERAGE, &output->key, &output->occurredAt, output);
        stop(VSS_STOP_FAILURE, now);
    }
}
static void apply_cleanup(const VssAudioObservation *cleanup)
{
    Attempt *a = find_attempt(&cleanup->key);
    if (a != NULL && cleanup->futurePcmBlocked && cleanup->providerReleased &&
        cleanup->pcmReleased && cleanup->lateProtected && cleanup->outputClosed &&
        cleanup->coverage == VSS_AUDIO_COVERAGE_SUFFICIENT) { a->clean = true; }
}
static void apply_request(const VssAudioObservation *request,
    const VssCandidateObservation *candidates, size_t count,
    const VssReadBasis *basis, const VssTimeEvidence *now)
{
    Attempt *a = find_attempt(&request->key);
    const VssCandidateObservation *candidate;
    if (a == NULL || ending() || a->submitted || !request->prepared ||
        pb.view.phase != VSS_PB_PREPARING || !key_equal(&request->key, &pb.view.attempt)) { return; }
    candidate = find_candidate(candidates, count, &pb.view.candidate);
    if (!basis_current(basis, now) || !basis->startsAllowed || !pb.view.startAllowed ||
        !candidate_permitted(candidate, now)) { stop(VSS_STOP_CONDITION, now); return; }
    if (a->bound && !Store_MarkPlaybackAttempted(&pb.view.candidate.key,
                                               a->key.session, a->key.attempt)) {
        stop(VSS_STOP_FAILURE, now); return;
    }
    a->permissionAt = *now; a->submitted = true;
    pb.view.phase = VSS_PB_START_IN_FLIGHT; changed();
    if (VssAudio_Start(&a->key, now) != VSS_AUDIO_SUBMITTED) { stop(VSS_STOP_FAILURE, now); }
}
static void progress_plan(const VssCandidateObservation *candidates, size_t count,
                          const VssReadBasis *basis, const VssTimeEvidence *now)
{
    Attempt *a;
    size_t i;
    bool whole = true, allNoStart = true;
    bool next;
    if (!pb.view.present || pb.view.phase == VSS_PB_RETIRED || pb.view.attempts == 0u) { return; }
    a = &pb.attempts[pb.view.attempts - 1u];
    next = pb.view.cue + 1u < pb.view.plan.cueCount || pb.view.repeat + 1u < pb.view.plan.repeatCount;
    if (!ending() && a->closed && a->clean) {
        if (!pb.performed[a->planPosition]) {
            /* Safe closure alone is not performance. No implicit skip/retry;
             * retain any earlier FIRST and finish through interrupted/unknown
             * or whole NO_START rules once all scope cleanup is sufficient. */
            stop(VSS_STOP_FAILURE, now);
        } else if (pb.view.startKnowledge == VSS_START_ACTUAL_CONFIRMED && next) {
            uint32_t gap = pb.view.plan.cues[pb.view.cue].gapMs;
            const VssCandidateObservation *candidate = find_candidate(candidates, count, &pb.view.candidate);
            /* Basis may have changed because this call applied physical facts.
             * Recollect on the next bounded owner opportunity, never use stale. */
            if (!basis_current(basis, now)) { return; }
            if (!basis->startsAllowed || !candidate_permitted(candidate, now)) {
                stop(VSS_STOP_CONDITION, now);
            } else if (comparable(now, &a->endedAt) && a->endedAt.latest <= UINT64_MAX - gap &&
                       now->earliest >= a->endedAt.latest + gap) {
                if (a->bound && !Store_MarkPlaybackRetired(&pb.view.candidate.key,
                    a->key.session, a->key.attempt, false)) { stop(VSS_STOP_FAILURE, now); return; }
                if (++pb.view.cue == pb.view.plan.cueCount) { pb.view.cue = 0u; ++pb.view.repeat; }
                if (!new_attempt(now)) { stop(VSS_STOP_FAILURE, now); }
                return;
            } else { return; }
        } else {
            pb.normalEnd = pb.view.startKnowledge == VSS_START_ACTUAL_CONFIRMED && !next;
            for (i = 0u; i < pb.view.plan.cueCount * pb.view.plan.repeatCount; ++i) {
                pb.normalEnd = pb.normalEnd && pb.performed[i];
            }
            stop(VSS_STOP_POLICY, now);
        }
    }
    if (!ending()) { return; }
    for (i = 0u; i < pb.view.attempts; ++i) {
        whole = whole && pb.attempts[i].closed && pb.attempts[i].clean;
        allNoStart = allNoStart && pb.attempts[i].noStart;
    }
    if (!whole || pb.view.startAllowed) { return; }
    if (!pb.finalMade) {
        if (pb.view.startKnowledge == VSS_START_UNCERTAIN_FINAL) {
            pb.finalMade = true;
        } else if (allNoStart && !pb.physicalActual) {
            if (!make_fact(VSS_FACT_NO_START_REEVALUATE, a, &a->endedAt)) { return; }
            pb.view.startKnowledge = VSS_START_NO_START_CONFIRMED; pb.finalMade = true;
        } else if (pb.view.startKnowledge == VSS_START_ACTUAL_CONFIRMED) {
            if (!make_fact(pb.normalEnd ? VSS_FACT_WHOLE_COMPLETED : VSS_FACT_WHOLE_INTERRUPTED,
                           a, &a->endedAt)) { return; }
            pb.finalMade = true;
        } else {
            /* Physically safe retirement does not establish past NO_START.
             * Binding remains non-replayable; no invented uncertain final. */
            pb.finalMade = true;
        }
    }
    pb.view.wholeSafe = true; pb.view.phase = VSS_PB_RETIRED; changed();
}
static void apply_notices(void)
{
    size_t i;
    for (i = 0u; i < VSS_PB_NOTICES; ++i) {
        Notice *n = &pb.notices[i];
        if (n->pending && !n->resultPresent) {
            n->result = Store_ApplyPlaybackFact(&n->fact);
            n->resultPresent = true;
            if (n->result == VSS_FACT_APPLIED || n->result == VSS_FACT_ALREADY_APPLIED) {
                n->pending = false;
            }
        }
    }
    if (pb.view.phase == VSS_PB_RETIRED && pb.view.candidate.mode == VSS_MODE_EVENT) {
        bool notificationPending = false;
        for (i = 0u; i < VSS_PB_NOTICES; ++i) { notificationPending = notificationPending || pb.notices[i].pending; }
        if (!notificationPending) {
            for (i = 0u; i < pb.view.attempts; ++i) {
                if (pb.attempts[i].bound) {
                    (void)Store_MarkPlaybackRetired(&pb.view.candidate.key, pb.view.session,
                        pb.attempts[i].key.attempt, pb.view.startKnowledge == VSS_START_NO_START_CONFIRMED);
                }
            }
        }
    }
}
bool Playback_Read(VssPlaybackObservation *out)
{
    size_t i;
    if (out == NULL) { return false; }
    *out = pb.view; out->revision = revision;
    out->notificationPending = false; out->publicationPending = false;
    for (i = 0u; i < VSS_PB_NOTICES; ++i) {
        out->notificationPending = out->notificationPending || pb.notices[i].pending;
        out->publicationPending = out->publicationPending || pb.notices[i].resultPresent;
    }
    return true;
}
void Playback_ReadPolicyCurrent(VssPolicyCurrent *out)
{
    if (out == NULL) { return; }
    memset(out, 0, sizeof(*out));
    out->present = pb.view.present && pb.view.phase != VSS_PB_RETIRED;
    out->session = pb.view.session; out->candidate = pb.view.candidate;
    out->plan = pb.view.plan; out->stopping = ending();
    out->valid = out->present && VssPolicy_ValidPlan(&pb.view.plan) && pb.view.startAllowed;
}
void Playback_ReadStoreSnapshot(VssOrigin origin, VssStorePlaybackSnapshot *out)
{
    VssPlaybackObservation view;
    if (out == NULL) { return; }
    (void)Playback_Read(&view); memset(out, 0, sizeof(*out));
    out->origin = origin; out->complete = revision != UINT64_MAX;
    out->notificationPending = view.notificationPending; out->revision = revision;
    out->storeRevision = Store_ReadRevision(origin);
}
static bool out_valid(const void *out, size_t capacity, const size_t *written)
{
    return written != NULL && (capacity == 0u || out != NULL);
}
void Playback_Advance(
    const VssAudioObservation *requests, size_t requestCount,
    const VssAudioOutput *outputs, size_t outputCount,
    const VssAudioObservation *cleanup, size_t cleanupCount,
    const VssCandidateObservation *candidates, size_t candidateCount,
    const VssReadBasis *basis, const VssTimeEvidence *time,
    VssPlaybackObservation *observation, size_t observationCapacity, size_t *observationWritten,
    VssPlaybackFact *facts, size_t factCapacity, size_t *factWritten,
    VssFactApplication *applications, size_t applicationCapacity, size_t *applicationWritten,
    VssPlaybackDiagnostic *diagnostics, size_t diagnosticCapacity, size_t *diagnosticWritten)
{
    size_t i, published = 0u;
    bool ov = out_valid(observation, observationCapacity, observationWritten);
    bool fv = out_valid(facts, factCapacity, factWritten);
    bool av = out_valid(applications, applicationCapacity, applicationWritten);
    bool dv = out_valid(diagnostics, diagnosticCapacity, diagnosticWritten);
    if (ov) { *observationWritten = 0u; }
    if (fv) { *factWritten = 0u; }
    if (av) { *applicationWritten = 0u; }
    if (dv) { *diagnosticWritten = 0u; }
    /* Apply outputs before request stages. No count traversal past owner bounds. */
    if (outputCount <= VSS_AUDIO_OUTPUT_CAPACITY && (outputCount == 0u || outputs != NULL)) {
        for (i = 0u; i < outputCount; ++i) { apply_output(&outputs[i], time); }
    } else { stop(VSS_STOP_FAILURE, time); diagnose(VSS_PB_DIAG_COVERAGE, NULL, time, NULL); }
    if (cleanupCount <= VSS_PB_ATTEMPTS && (cleanupCount == 0u || cleanup != NULL)) {
        for (i = 0u; i < cleanupCount; ++i) { apply_cleanup(&cleanup[i]); }
    } else { stop(VSS_STOP_FAILURE, time); }
    apply_notices(); /* FIRST must be visible before following-cue permissions. */
    if (pb.view.present && !ending()) {
        const VssCandidateObservation *candidate = find_candidate(candidates, candidateCount, &pb.view.candidate);
        /* Current read may predate this call's facts, so compare STORE and time
         * for permission; a changed PB stamp alone asks for fresh collection. */
        if (basis == NULL || basis->validity != VSS_READ_VALID || !basis->startsAllowed ||
            !same_time(&basis->time, time) || !candidate_permitted(candidate, time)) {
            stop(VSS_STOP_CONDITION, time);
        }
    }
    if (requestCount <= VSS_PB_ATTEMPTS && (requestCount == 0u || requests != NULL)) {
        for (i = 0u; i < requestCount; ++i) {
            apply_request(&requests[i], candidates, candidateCount, basis, time);
        }
    } else { stop(VSS_STOP_FAILURE, time); }
    progress_plan(candidates, candidateCount, basis, time);
    apply_notices();
    if (fv && av) {
        for (i = 0u; i < VSS_PB_NOTICES && published < factCapacity && published < applicationCapacity; ++i) {
            if (pb.notices[i].resultPresent) {
                facts[published] = pb.notices[i].fact; applications[published] = pb.notices[i].result;
                ++published; pb.notices[i].resultPresent = false;
            }
        }
        *factWritten = published; *applicationWritten = published;
    }
    if (ov && observationCapacity != 0u && pb.view.present) {
        (void)Playback_Read(observation); *observationWritten = 1u;
    }
    if (dv) {
        size_t n = pb.diagnosticCount < diagnosticCapacity ? pb.diagnosticCount : diagnosticCapacity;
        for (i = 0u; i < n; ++i) { diagnostics[i] = pb.diagnostics[i]; }
        if (n != 0u) {
            pb.diagnosticCount -= n;
            memmove(pb.diagnostics, pb.diagnostics + n, pb.diagnosticCount * sizeof(pb.diagnostics[0]));
        }
        *diagnosticWritten = n;
    }
}
void Playback_Service(const VssCandidateObservation *candidates, size_t count,
                      const VssReadBasis *basis, const VssTimeEvidence *time)
{
    VssAudioOutput outputs[VSS_AUDIO_OUTPUT_CAPACITY];
    VssAudioObservation lower[VSS_PB_ATTEMPTS];
    size_t outputCount = 0u, lowerCount = 0u, i;
    VssAudio_Poll(outputs, VSS_AUDIO_OUTPUT_CAPACITY, &outputCount);
    for (i = 0u; i < pb.view.attempts; ++i) {
        if (VssAudio_Read(&pb.attempts[i].key, &lower[lowerCount])) { ++lowerCount; }
    }
    Playback_Advance(lower, lowerCount, outputs, outputCount, lower, lowerCount,
        candidates, count, basis, time, NULL, 0u, NULL, NULL, 0u, NULL,
        NULL, 0u, NULL, NULL, 0u, NULL);
}
#ifdef VSS_HOST_TEST
void Playback_TestReset(void)
{
    memset(&pb, 0, sizeof(pb)); nextSession = 1u; nextAttempt = 1u; nextFact = 1u;
    revision = 1u;
}
bool Playback_TestQualifiedUncertainFinal(uint64_t session, const VssTimeEvidence *time)
{
    Attempt *a;
    if (!pb.view.present || session != pb.view.session || pb.view.phase == VSS_PB_RETIRED ||
        pb.finalMade || pb.view.attempts == 0u || !time_valid(time)) { return false; }
    a = &pb.attempts[pb.view.attempts - 1u];
    if (!make_fact(VSS_FACT_START_OUTCOME_UNCERTAIN, a, time)) { return false; }
    stop(VSS_STOP_FAILURE, time); pb.view.phase = VSS_PB_QUARANTINED;
    pb.view.startKnowledge = VSS_START_UNCERTAIN_FINAL; pb.finalMade = true;
    diagnose(VSS_PB_DIAG_FINAL_TEST_ONLY, &a->key, time, NULL); changed();
    apply_notices(); return true;
}
#endif
