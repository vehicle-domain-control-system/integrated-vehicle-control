#include "vss_playback.h"

#include <string.h>

#include "../audio_stream/audio_stream.h"
#include "../asset/audio_asset.h"

static VssPlaybackObservation s_view;
static uint64_t s_revision = 1u;
static uint64_t s_nextSession = 1u;
static uint64_t s_nextFact = 1u;
static uint64_t s_firstStartLatest;    /* end of the FIRST_ACTUAL_START interval */
static uint64_t s_adoptedAtMs;         /* when the current session was adopted */

/* Scratch: these are too large for the stack. */
static StreamFact s_facts[AUDIO_STREAM_FACT_CAPACITY];
static VssPlaybackFact s_fact;
static VssOccurrenceObservation s_record;

/* Stream fault bits that mean the output itself failed. */
#define PLAYBACK_OUTPUT_FAULT_BITS (0xFFFFUL | AUDIO_STREAM_FAULT_REQUEST)

static void count_up(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static void changed(void)
{
    if (s_revision != UINT64_MAX)
    {
        ++s_revision;
    }
    s_view.revision = s_revision;
}

static bool is_event(void)
{
    return s_view.candidate.mode == VSS_MODE_EVENT;
}

static bool live(void)
{
    return s_view.present && (s_view.phase != PLAYBACK_IDLE);
}

static void store_refused(void)
{
    count_up(&s_view.storeRefusals);
}

/* [max(atMs, notBefore), +span] in the time domain of the session's record. */
static VssFactTime fact_time(uint64_t atMs, uint64_t notBefore)
{
    VssFactTime t;
    uint64_t start = (atMs > notBefore) ? atMs : notBefore;

    t.domain = s_view.candidate.eventMeta.original.domain;
    t.epoch = s_view.candidate.eventMeta.original.epoch;
    t.earliest = start;
    t.latest = (start <= UINT64_MAX - PLAYBACK_FACT_SPAN_MS) ? start + PLAYBACK_FACT_SPAN_MS : UINT64_MAX;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

/* Records a playback fact for the current event session. True when STORE
 * applied it. */
static bool apply_fact(VssPlaybackFactKind kind, uint64_t atMs, uint64_t notBefore)
{
    VssFactApplication result;

    if (s_nextFact == UINT64_MAX)
    {
        return false;
    }
    memset(&s_fact, 0, sizeof s_fact);
    s_fact.present = true;
    s_fact.kind = (uint32_t)kind;
    s_fact.fact = s_nextFact++;
    s_fact.occurrence = s_view.candidate.key;
    s_fact.session = s_view.session;
    s_fact.attempt = s_view.session;
    s_fact.occurredAt = fact_time(atMs, notBefore);
    s_fact.basisRevision = Store_ReadRevision(s_view.candidate.origin);
    result = Store_ApplyPlaybackFact(&s_fact);
    return (result == VSS_FACT_APPLIED) || (result == VSS_FACT_ALREADY_APPLIED);
}

/* Retires the event session's binding. A no-start retirement needs the
 * NO_START_REEVALUATE fact in STORE; without it the binding is retired as
 * played, which keeps the record from being selected again. */
static void retire_binding(bool noStart)
{
    const VssOccurrenceKey *key = &s_view.candidate.key;

    if (Store_MarkPlaybackRetired(key, s_view.session, s_view.session, noStart))
    {
        return;
    }
    if (noStart && Store_MarkPlaybackRetired(key, s_view.session, s_view.session, false))
    {
        return;
    }
    store_refused();
}

static void end_session(PlaybackEnd end, uint64_t atMs)
{
    s_view.lastSession = s_view.session;
    s_view.lastEnd = end;
    s_view.lastEndAtMs = atMs;
    s_view.present = false;
    s_view.phase = PLAYBACK_IDLE;
    s_view.started = false;
    s_view.stopRequested = false;
    s_view.startUncertain = false;
    changed();
}

static void request_stop(void)
{
    if (!live() || s_view.stopRequested)
    {
        return;
    }
    (void)AudioStream_RequestControl(s_view.token, false);
    s_view.stopRequested = true;
    s_view.phase = PLAYBACK_STOPPING;
    changed();
}

/* The event session ended without a first start accepted by STORE. With
 * retry allowed the record stays selectable within its use limit; without
 * it the record is closed for good (the only final fact STORE accepts
 * before a first start is START_OUTCOME_UNCERTAIN).
 *
 * The stream dates a stop or fault before any output at the last boundary,
 * which can lie before the event was received. Nothing reached the output,
 * so the fact is dated no earlier than the adoption of the session, which
 * followed the event. */
static void close_without_start(uint64_t atMs, bool retryAllowed)
{
    if (retryAllowed && apply_fact(VSS_FACT_NO_START_REEVALUATE, atMs, s_adoptedAtMs))
    {
        retire_binding(true);
        return;
    }
    if (!retryAllowed && apply_fact(VSS_FACT_START_OUTCOME_UNCERTAIN, atMs, s_adoptedAtMs))
    {
        retire_binding(false);
        return;
    }
    store_refused();
    retire_binding(false);
}

static void on_started(const StreamFact *f)
{
    s_view.lastStartedSession = s_view.session;
    if (!is_event())
    {
        s_view.started = true;
    }
    else if (apply_fact(VSS_FACT_FIRST_ACTUAL_START, f->atMs, 0u))
    {
        s_view.started = true;
        s_firstStartLatest = s_fact.occurredAt.latest;
    }
    else
    {
        /* Output began, but STORE cannot count it as the first start (use
         * limit passed, or the input was lost before it): stop, and close
         * the record as uncertain. */
        s_view.startUncertain = true;
        request_stop();
        if (!apply_fact(VSS_FACT_START_OUTCOME_UNCERTAIN, f->atMs, 0u))
        {
            store_refused();
        }
    }
    if (!s_view.stopRequested)
    {
        s_view.phase = PLAYBACK_ACTIVE;
    }
    changed();
}

/* COMPLETED, STOPPED and FAULT: the session's final fact. retryAllowed is
 * false when the occurrence must not be selected again. */
static void on_ended(const StreamFact *f, PlaybackEnd end, bool retryAllowed)
{
    if (is_event())
    {
        if (s_view.startUncertain)
        {
            retire_binding(false);
        }
        else if (s_view.started)
        {
            VssPlaybackFactKind kind = (f->kind == STREAM_FACT_COMPLETED)
                ? VSS_FACT_WHOLE_COMPLETED : VSS_FACT_WHOLE_INTERRUPTED;
            if (!apply_fact(kind, f->atMs, s_firstStartLatest))
            {
                store_refused();
            }
            retire_binding(false);
        }
        else
        {
            close_without_start(f->atMs, retryAllowed);
        }
    }
    end_session(end, f->atMs);
}

static void on_fault(const StreamFact *f)
{
    bool decodeOnly = (f->faultMask & ~AUDIO_STREAM_FAULT_DECODE) == 0u;

    s_view.lastFaultMask = f->faultMask;
    s_view.lastFaultAtMs = f->atMs;
    if ((f->faultMask & PLAYBACK_OUTPUT_FAULT_BITS) != 0u)
    {
        count_up(&s_view.outputFaults);
        s_view.outputFailed = true;
    }
    else if (decodeOnly)
    {
        count_up(&s_view.assetFaults);
        if (s_view.plan.cues[0].asset < 32u)
        {
            s_view.failedAssets |= 1UL << s_view.plan.cues[0].asset;
        }
    }
    else
    {
        count_up(&s_view.outputFaults);
    }
    /* The asset is fixed data: a decode failure would repeat, so its
     * occurrence is not offered for another attempt. */
    on_ended(f, PLAYBACK_END_FAULT, !decodeOnly);
}

static void apply_stream_fact(const StreamFact *f)
{
    if (!live() || (f->token != s_view.token))
    {
        count_up(&s_view.strayFacts);
        return;
    }
    switch (f->kind)
    {
    case STREAM_FACT_STARTED:
        on_started(f);
        break;
    case STREAM_FACT_COMPLETED:
        on_ended(f, is_event() ? PLAYBACK_END_COMPLETED : PLAYBACK_END_STOPPED, true);
        break;
    case STREAM_FACT_STOPPED:
        if (!is_event())
        {
            on_ended(f, PLAYBACK_END_STOPPED, true);
        }
        else if (s_view.startUncertain)
        {
            on_ended(f, PLAYBACK_END_START_UNCERTAIN, true);
        }
        else
        {
            on_ended(f, s_view.started ? PLAYBACK_END_INTERRUPTED : PLAYBACK_END_NO_START, true);
        }
        break;
    case STREAM_FACT_FAULT:
        on_fault(f);
        break;
    default:
        count_up(&s_view.strayFacts);
        break;
    }
}

static const VssCandidateObservation *find_candidate(const VssCandidateObservation *candidates,
                                                     size_t count,
                                                     const VssCandidateObservation *which)
{
    size_t i;

    if ((count > VSS_CANDIDATE_CAPACITY) || ((count != 0u) && (candidates == NULL)))
    {
        return NULL;
    }
    for (i = 0u; i < count; ++i)
    {
        if (VssPolicy_SameCandidate(&candidates[i], which))
        {
            return &candidates[i];
        }
    }
    return NULL;
}

static bool valid_now(const VssTimeEvidence *now)
{
    return (now != NULL) && ((now->domain == VSS_PRODUCT) || (now->domain == VSS_TEST)) &&
           (now->continuity == VSS_CONTINUITY_COMPARABLE) && (now->earliest <= now->latest);
}

void Playback_Init(void)
{
    memset(&s_view, 0, sizeof s_view);
    s_revision = 1u;
    s_view.revision = s_revision;
    s_nextSession = 1u;
    s_nextFact = 1u;
    s_firstStartLatest = 0u;
    s_adoptedAtMs = 0u;
}

/* Stops a stateful session whose warning is no longer an applicable
 * candidate of the same value. */
static void check_stateful(const VssCandidateObservation *candidates, size_t count,
                           const VssTimeEvidence *now)
{
    if (live() && !s_view.stopRequested && !is_event() && valid_now(now) &&
        (now->domain == s_view.candidate.origin))
    {
        const VssCandidateObservation *c = find_candidate(candidates, count, &s_view.candidate);

        if ((c == NULL) || (c->validity != VSS_CANDIDATE_APPLICABLE) ||
            (c->value != s_view.candidate.value))
        {
            request_stop();
        }
    }
}

void Playback_Advance(const VssCandidateObservation *candidates, size_t count,
                      const VssTimeEvidence *now)
{
    size_t n;
    size_t i;

    do
    {
        n = AudioStream_Advance(s_facts, AUDIO_STREAM_FACT_CAPACITY);
        for (i = 0u; i < n; ++i)
        {
            apply_stream_fact(&s_facts[i]);
        }
    } while (n == AUDIO_STREAM_FACT_CAPACITY);

    if (candidates != NULL)
    {
        check_stateful(candidates, count, now);
    }
}

/* Binds the record, prepares and starts the stream. False when the session
 * could not start; the observation then holds the reason. */
static bool start_session(const VssCandidateObservation *c, const VssPlanDescriptor *plan,
                          const VssTimeEvidence *now)
{
    const VssOccurrenceKey *key = &c->key;
    AudioAsset_IdType asset = (AudioAsset_IdType)plan->cues[0].asset;
    StreamResult r;

    s_view.present = true;
    s_view.phase = PLAYBACK_STARTING;
    s_view.session = s_nextSession++;
    s_view.token = (uint32_t)s_view.session;
    s_view.candidate = *c;
    s_view.plan = *plan;
    s_view.started = false;
    s_view.stopRequested = false;
    s_view.startUncertain = false;
    s_firstStartLatest = 0u;
    s_adoptedAtMs = now->earliest;

    if (is_event())
    {
        /* Identity and record are protected before the stream can act. */
        if (!Store_ReadOccurrence(key, &s_record) ||
            !Store_BindPlayback(key, s_view.session, s_view.session, s_record.revision))
        {
            store_refused();
            end_session(PLAYBACK_END_NOT_STARTED, now->earliest);
            return false;
        }
        if (!Store_MarkPlaybackAttempted(key, s_view.session, s_view.session))
        {
            store_refused();
            retire_binding(false);
            end_session(PLAYBACK_END_NOT_STARTED, now->earliest);
            return false;
        }
    }
    r = AudioStream_Prepare(s_view.token, asset, !is_event());
    if (r == STREAM_OK)
    {
        r = AudioStream_RequestControl(s_view.token, true);
        if (r != STREAM_OK)
        {
            /* Prepared but not started: release the stream session. */
            (void)AudioStream_RequestControl(s_view.token, false);
        }
    }
    if (r != STREAM_OK)
    {
        count_up(&s_view.startRefusals);
        if (is_event())
        {
            close_without_start(now->earliest, true);
        }
        end_session(PLAYBACK_END_NOT_STARTED, now->earliest);
        return false;
    }
    changed();
    return true;
}

VssRequestDisposition Playback_RequestTransition(const VssSelectionDecision *decision,
                                                 const VssCandidateObservation *candidates,
                                                 size_t count, const VssReadBasis *basis,
                                                 const VssTimeEvidence *now)
{
    const VssCandidateObservation *c;
    const VssPlanDescriptor *plan;

    if ((decision == NULL) || (basis == NULL) || !valid_now(now) ||
        (count > VSS_CANDIDATE_CAPACITY) || ((count != 0u) && (candidates == NULL)))
    {
        return VSS_REQUEST_REJECTED;
    }
    if (basis->validity == VSS_READ_VALID)
    {
        /* A complete candidate list of this pass. */
        check_stateful(candidates, count, now);
    }
    if ((basis->validity != VSS_READ_VALID) || (basis->origin != now->domain) ||
        (basis->storeRevision == 0u) ||
        (basis->storeRevision != Store_ReadRevision(basis->origin)) ||
        (basis->pbRevision != s_revision) ||
        (decision->basis.storeRevision != basis->storeRevision) ||
        (decision->basis.pbRevision != basis->pbRevision))
    {
        return VSS_REQUEST_WAIT;
    }
    if (live() && !basis->startsAllowed)
    {
        request_stop();
    }
    switch (decision->action)
    {
    case VSS_SELECTION_KEEP:
        return (live() && !s_view.stopRequested) ? VSS_REQUEST_KEPT : VSS_REQUEST_WAIT;
    case VSS_SELECTION_REPLACE:
        request_stop();
        return VSS_REQUEST_WAIT;
    case VSS_SELECTION_CHOOSE:
        break;
    default:
        return VSS_REQUEST_WAIT;
    }
    plan = &decision->plan;
    if (live() || !decision->candidatePresent || !basis->startsAllowed ||
        !VssPolicy_ValidPlan(plan) || !plan->available ||
        (plan->cueCount != 1u) || (plan->repeatCount != 1u) ||
        (plan->signal != decision->candidate.signal) ||
        (plan->value != decision->candidate.value) ||
        (decision->candidate.origin != basis->origin))
    {
        return VSS_REQUEST_WAIT;
    }
    /* The output is checked here, not only through the basis: a transmit
     * fault may have been latched since the health evaluation the basis
     * came from, and starting into it would count as a start refusal. */
    if (s_view.outputFailed || AudioStream_OutputFailed() ||
        !AudioStream_IsIdle() || (s_nextSession >= UINT32_MAX))
    {
        return VSS_REQUEST_WAIT;
    }
    c = find_candidate(candidates, count, &decision->candidate);
    if ((c == NULL) || (c->revision != decision->candidate.revision) ||
        (c->validity != VSS_CANDIDATE_APPLICABLE) || !c->newSessionAllowed)
    {
        return VSS_REQUEST_WAIT;
    }
    return start_session(c, plan, now) ? VSS_REQUEST_ADOPTED : VSS_REQUEST_REJECTED;
}

bool Playback_Read(VssPlaybackObservation *out)
{
    if (out == NULL)
    {
        return false;
    }
    *out = s_view;
    out->revision = s_revision;
    return true;
}

void Playback_ReadPolicyCurrent(VssPolicyCurrent *out)
{
    if (out == NULL)
    {
        return;
    }
    memset(out, 0, sizeof *out);
    out->present = live();
    out->stopping = out->present && s_view.stopRequested;
    out->session = s_view.session;
    if (out->present)
    {
        out->candidate = s_view.candidate;
        out->plan = s_view.plan;
        out->valid = !s_view.stopRequested && VssPolicy_ValidPlan(&s_view.plan);
    }
}

void Playback_ReadStoreSnapshot(VssOrigin origin, VssStorePlaybackSnapshot *out)
{
    if (out == NULL)
    {
        return;
    }
    memset(out, 0, sizeof *out);
    out->origin = origin;
    out->complete = s_revision != UINT64_MAX;
    out->notificationPending = false;   /* facts are applied as they arrive */
    out->revision = s_revision;
    out->storeRevision = Store_ReadRevision(origin);
}

#if VSS_CFG_HOST_TEST
void Playback_ResetForTest(void)
{
    Playback_Init();
}
#endif
