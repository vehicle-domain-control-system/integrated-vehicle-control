#include "vss_input_store.h"
#include <string.h>

/* Event and Warning are separate CAN messages with their own TX_SEQUENCE,
 * so each stream keeps its own order and duplicate identity. */
typedef struct {
    bool present;
    uint32_t last;
    VssOccurrenceKey lastKey;
    VssInputEvidence lastDelivery; /* occurrence points only to lastKey */
} InputEventOrder;
typedef struct {
    bool present;
    uint32_t last;
    VssWarningFrame lastFrame; /* pointer-free typed duplicate identity */
} InputWarningOrder;
/* Boot ids carry no order, so a late frame of any recently replaced boot
 * would look like a new DOMAIN restart. The last replaced boots are kept. */
#define INPUT_RETIRED_BOOTS 4u
typedef struct {
    bool configured;
    bool open;
    uint32_t boot;
    uint32_t retiredBoots[INPUT_RETIRED_BOOTS]; /* ring of replaced boots; 0 = empty */
    size_t retiredNext;
    VssTimeEvidence lastTime;
    InputEventOrder eventOrder;
    InputWarningOrder warningOrder;
    bool monitorArmed;
    bool monitorLost;
    VssFactTime lastAdvancingRx;
} InputSource;
typedef struct {
    bool present;
    VssInputMeta appliedMeta;
    VssInputQuality quality;
    bool lossPresent;
    VssFactTime lossAt;
    uint64_t revision; /* bounded by non-wrapping uint32 delivery + one closure */
} InputContext;

static InputSource sources[2];
static InputContext contexts[VSS_EVENT_CONTEXTS];
static VssDiagnosticEvidence pending[VSS_DIAGNOSTIC_CAPACITY];
static VssDiagnosticObservation diagnosticState;
typedef struct {
    bool present;
    bool evidenceSeen;
    bool fenced;
    uint32_t evidenceBoot;
    uint32_t evidenceSequence;
    VssWarningContextObservation observation;
    VssInputValue lastValue;
    bool lossPresent;
    VssFactTime lossAt;
} WarningContext;
static WarningContext warningContexts[VSS_WARNING_CONTEXTS];
static VssInputApplication warning_loss(VssOrigin origin, VssInputSignal signal,
    VssInputQuality quality, const VssFactTime *loss, bool provenanceLost);
static VssInputApplication check_warning_deadlines(const VssTimeEvidence *now);
static void choose_result(VssInputApplication *representative, VssInputApplication r);

static bool valid_origin(VssOrigin origin)
{ return origin == VSS_PRODUCT || origin == VSS_TEST; }

static bool valid_time(const VssFactTime *t)
{
    return t != NULL && valid_origin(t->domain) && t->epoch != 0u &&
           t->continuity == VSS_CONTINUITY_COMPARABLE && t->earliest <= t->latest;
}

static bool same_time(const VssFactTime *a, const VssFactTime *b)
{
    return a->domain == b->domain && a->epoch == b->epoch &&
           a->earliest == b->earliest && a->latest == b->latest &&
           a->continuity == b->continuity;
}

static bool same_delivery(const VssInputEvidence *a, const VssInputEvidence *b)
{
    const VssOccurrenceKey *x = a->occurrence, *y = b->occurrence;
    return x->origin == y->origin && x->source == y->source &&
           x->generation == y->generation && x->occurrenceId == y->occurrenceId &&
           a->signal == b->signal && a->value == b->value && a->quality == b->quality &&
           a->deliveryOrder == b->deliveryOrder && same_time(&a->receivedAt, &b->receivedAt) &&
           a->ageLowMs == b->ageLowMs && a->ageHighMs == b->ageHighMs &&
           a->ageComplete == b->ageComplete && a->useLimitMs == b->useLimitMs &&
           a->relatedCommand == b->relatedCommand && a->availability == b->availability &&
           same_time(&a->lossAt, &b->lossAt);
}

static void remember_delivery(InputSource *source, const VssInputEvidence *in)
{
    InputEventOrder *o = &source->eventOrder;
    o->lastKey = *in->occurrence;
    o->lastDelivery = *in;
    o->lastDelivery.occurrence = &o->lastKey;
    o->present = true;
    o->last = in->deliveryOrder;
}

static VssInputApplication unchanged(void)
{
    VssInputApplication r = {VSS_VALIDATION_NO_CHANGE, false, VSS_STORE_NOT_SUBMITTED};
    return r;
}

static void protect_diagnostic(VssDiagnosticEvidence d, size_t lane)
{
    if (diagnosticState.pending < VSS_DIAGNOSTIC_CAPACITY) {
        pending[diagnosticState.pending++] = d;
    } else {
        diagnosticState.lossLatched = true;
        if (diagnosticState.lossCount != UINT32_MAX) { ++diagnosticState.lossCount; }
        if (!diagnosticState.summaryPresent[lane]) {
            diagnosticState.firstLost[lane] = d;
            diagnosticState.summaryPresent[lane] = true;
        }
        diagnosticState.latestLost[lane] = d;
    }
}

static void diagnose(VssDiagnosticCode code, const VssInputEvidence *in,
                     const VssTimeEvidence *time)
{
    VssDiagnosticEvidence d = {0};
    size_t lane = VSS_INPUT_CONTEXTS;
    d.code = code;
    d.observedAt.continuity = VSS_CONTINUITY_UNKNOWN;
    if (time != NULL) { d.observedAt = *time; }
    if (in != NULL) {
        d.signal = in->signal;
        d.deliveryOrder = in->deliveryOrder;
        d.receivedUseLimit = in->useLimitMs;
        d.useLimit = in->useLimitMs < 500u ? in->useLimitMs : 500u;
        if (in->occurrence != NULL) {
            d.keyPresent = true;
            d.key = *in->occurrence;
            if (valid_origin(d.key.origin) && d.signal >= VSS_SIGNAL_WELCOME &&
                d.signal <= VSS_SIGNAL_LOCK_ERROR) {
                lane = (size_t)d.key.origin * 5u + (size_t)d.signal;
            }
        }
    }
    protect_diagnostic(d, lane);
}

static void drain(VssDiagnosticEvidence *out, size_t capacity, size_t *written)
{
    size_t n, i;
    /* Whole OUT group validated before even initializing written. */
    if (written == NULL || capacity > VSS_DIAGNOSTIC_CAPACITY ||
        (capacity != 0u && out == NULL)) { return; }
    *written = 0u;
    n = capacity < diagnosticState.pending ? capacity : diagnosticState.pending;
    for (i = 0u; i < n; ++i) {
        out[i] = pending[i]; /* pointer-free full copy is the synchronous handoff */
        ++*written;
    }
    for (i = n; i < diagnosticState.pending; ++i) { pending[i - n] = pending[i]; }
    diagnosticState.pending -= n;
}

bool Input_ReadDiagnostics(VssDiagnosticObservation *out)
{
    if (out == NULL) { return false; }
    *out = diagnosticState;
    return true;
}

bool Input_ReadContext(VssOrigin origin, VssInputSignal signal,
                       VssInputContextObservation *out)
{
    const InputContext *c;
    if (!valid_origin(origin) || signal < VSS_SIGNAL_WELCOME ||
        signal > VSS_SIGNAL_LOCK_ERROR || out == NULL) { return false; }
    c = &contexts[(size_t)origin * 5u + (size_t)signal];
    if (!c->present) { return false; }
    out->appliedMeta = c->appliedMeta;
    out->quality = c->quality;
    out->revision = c->revision;
    return true;
}

bool Input_OpenAfterSynchronization(VssOrigin origin, uint32_t domainBoot,
                                   const VssTimeEvidence *synchronizedAt)
{
    InputSource *s;
    if (!valid_origin(origin) || domainBoot == 0u || !valid_time(synchronizedAt) ||
        synchronizedAt->domain != origin) { return false; }
    s = &sources[(size_t)origin];
    if (s->configured) { return false; }
    s->configured = true;
    s->open = true;
    s->boot = domainBoot;
    s->lastTime = *synchronizedAt;
    return true;
}

static bool retired(const InputSource *s, uint32_t boot)
{
    size_t i;
    for (i = 0u; i < INPUT_RETIRED_BOOTS; ++i) {
        if (boot != 0u && s->retiredBoots[i] == boot) { return true; }
    }
    return false;
}

static VssInputApplication lose_context(size_t lane, VssInputQuality quality,
                                        const VssFactTime *loss)
{
    InputContext *c = &contexts[lane];
    VssInputApplication r = unchanged();
    VssQualityChange q = {0};
    if (!c->present || (c->lossPresent && c->quality == quality)) { return r; }
    if (!c->lossPresent) { c->lossAt = *loss; c->lossPresent = true; }
    c->quality = quality;
    ++c->revision;
    q.signal = (VssInputSignal)(lane % 5u);
    q.quality = quality;
    q.basis = c->appliedMeta;
    q.lossAt = c->lossAt;
    r.validation = VSS_VALIDATION_QUALITY_CHANGE;
    r.contextApplied = true;
    r.storeAdmission = Store_ApplyInput(NULL, NULL, &q);
    return r;
}

static VssInputApplication close_source(VssOrigin origin, const VssFactTime *loss)
{
    size_t i;
    VssInputApplication first = unchanged();
    sources[(size_t)origin].open = false;
    for (i = (size_t)origin * 5u; i < ((size_t)origin + 1u) * 5u; ++i) {
        VssInputApplication r = lose_context(i, VSS_INPUT_INVALID, loss);
        if (first.validation == VSS_VALIDATION_NO_CHANGE && r.contextApplied) { first = r; }
    }
    for (i = 0u; i < 4u; ++i) {
        VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + (unsigned)i);
        choose_result(&first, warning_loss(origin, signal, VSS_INPUT_INVALID, loss, true));
    }
    return first;
}

static bool clock_discontinuous(const InputSource *s, const VssTimeEvidence *now)
{
    return now->continuity == VSS_CONTINUITY_LOST ||
           (now->epoch != 0u && now->epoch != s->lastTime.epoch) ||
           (valid_time(now) && now->latest < s->lastTime.earliest);
}

bool Input_AcceptNewGeneration(VssOrigin origin, uint32_t newBoot,
                               const VssTimeEvidence *at)
{
    InputSource *s;
    size_t i;
    if (!valid_origin(origin) || newBoot == 0u || !valid_time(at) || at->domain != origin) {
        return false;
    }
    s = &sources[(size_t)origin];
    if (!s->configured || newBoot == s->boot || retired(s, newBoot) ||
        clock_discontinuous(s, at)) { return false; }
    if (s->open) {
        /* No source-generation time proof: preserve UNKNOWN loss basis. */
        VssFactTime loss = *at;
        loss.continuity = VSS_CONTINUITY_UNKNOWN;
        (void)close_source(origin, &loss);
    }
    /* The new generation starts its own transport order, receive monitor and
     * per-lane provenance; the old generation's warnings stay fenced in STORE
     * until a new-generation evaluation replaces them. */
    s->retiredBoots[s->retiredNext] = s->boot;
    s->retiredNext = (s->retiredNext + 1u) % INPUT_RETIRED_BOOTS;
    s->boot = newBoot;
    s->open = true;
    s->lastTime = *at;
    memset(&s->eventOrder, 0, sizeof s->eventOrder);
    memset(&s->warningOrder, 0, sizeof s->warningOrder);
    s->monitorArmed = false;
    s->monitorLost = false;
    memset(&s->lastAdvancingRx, 0, sizeof s->lastAdvancingRx);
    for (i = 0u; i < VSS_WARNING_LANES; ++i) {
        WarningContext *c = &warningContexts[(size_t)origin * VSS_WARNING_LANES + i];
        c->fenced = false;
        c->evidenceSeen = false;
        c->evidenceBoot = 0u;
        c->evidenceSequence = 0u;
    }
    return true;
}

static bool project(const VssInputEvidence *in, const VssTimeEvidence *now, VssInputMeta *m)
{
    const VssFactTime *rx = &in->receivedAt;
    /* Clock 0 is the start of this VSS run; the ledger was cleared before
     * it. An original time that may lie before clock 0 (age beyond the run)
     * may have played in an earlier run whose ledger is gone: not replayed. */
    if (!valid_time(rx) || rx->domain != now->domain || rx->epoch != now->epoch ||
        rx->latest > now->earliest || !in->ageComplete || in->ageLowMs > in->ageHighMs ||
        in->ageHighMs >= 65535u || rx->earliest < in->ageHighMs ||
        rx->latest < in->ageLowMs || in->useLimitMs == 0u) { return false; }
    memset(m, 0, sizeof *m);
    m->key = *in->occurrence;
    m->deliveryOrder = in->deliveryOrder;
    m->original = *rx;
    m->original.earliest = rx->earliest - in->ageHighMs;
    m->original.latest = rx->latest - in->ageLowMs;
    m->receivedUseLimit = in->useLimitMs;
    m->useLimit = in->useLimitMs < 500u ? in->useLimitMs : 500u;
    if (m->original.earliest > UINT64_MAX - m->useLimit) { return false; }
    m->validUntil = m->original.earliest + m->useLimit;
    m->relatedCommand = in->relatedCommand;
    m->availability = in->availability;
    return true;
}

static VssInputApplication process(const VssInputEvidence *in, const VssTimeEvidence *now)
{
    VssInputApplication r = unchanged();
    const VssOccurrenceKey *key;
    InputSource *s;
    InputContext *c;
    VssOneShotEvent event;
    size_t lane;
    if (now == NULL || !valid_origin(now->domain)) {
        diagnose(VSS_DIAG_TIME, in, now);
        if (in != NULL) { r.validation = VSS_VALIDATION_REJECTED; }
        return r;
    }
    s = &sources[(size_t)now->domain];
    /* Only a definite continuity loss/reset/backward jump fences this origin.
     * UNKNOWN/bad intervals block new fresh acceptance without inventing lossAt. */
    if (s->configured && clock_discontinuous(s, now)) {
        VssFactTime loss = *now;
        loss.continuity = VSS_CONTINUITY_LOST;
        r = close_source(now->domain, &loss);
        diagnose(VSS_DIAG_TIME, in, now);
        if (in != NULL && !r.contextApplied) { r.validation = VSS_VALIDATION_REJECTED; }
        return r;
    }
    if (!valid_time(now)) {
        diagnose(VSS_DIAG_TIME, in, now);
        if (in != NULL) { r.validation = VSS_VALIDATION_REJECTED; }
        return r;
    }
    if (s->configured) { s->lastTime = *now; }
    if (in == NULL) { return check_warning_deadlines(now); }
    (void)check_warning_deadlines(now);
    key = in->occurrence;
    if (key == NULL || !valid_origin(key->origin) || key->origin != now->domain ||
        key->source != VSS_SOURCE_DOMAIN || key->occurrenceId == 0u ||
        in->signal < VSS_SIGNAL_WELCOME || in->signal > VSS_SIGNAL_LOCK_ERROR ||
        (in->value != VSS_VALUE_EVENT && in->value != VSS_VALUE_UNSUPPORTED) ||
        in->quality < VSS_INPUT_VALID || in->quality > VSS_INPUT_UNASSESSED ||
        in->availability < VSS_AVAILABLE || in->availability > VSS_UNAVAILABLE ||
        in->deliveryOrder == 0u) {
        diagnose(VSS_DIAG_ARGUMENT, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    /* Confirm the typed delivery's RX clock context before its generation
     * and TX: a delivery without usable RX time neither consumes TX nor
     * closes the current generation. Upstream wire/source association remains
     * the adapter's proof obligation; this check does not require the EVENT's
     * age or semantic value to be valid. */
    if (!valid_time(&in->receivedAt) || in->receivedAt.domain != now->domain ||
        in->receivedAt.epoch != now->epoch || in->receivedAt.latest > now->earliest) {
        diagnose(VSS_DIAG_TIME, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    if (!s->open || key->generation == 0u || key->generation != s->boot) {
        /* A late delivery of a replaced boot cannot close the new one. */
        if (s->open && key->generation != 0u && key->generation != s->boot &&
            !retired(s, key->generation)) {
            /* No source-generation time proof: preserve UNKNOWN loss basis. */
            VssFactTime loss = *now;
            loss.continuity = VSS_CONTINUITY_UNKNOWN;
            r = close_source(key->origin, &loss);
        }
        diagnose(VSS_DIAG_GENERATION, in, now);
        if (!r.contextApplied) { r.validation = VSS_VALIDATION_REJECTED; }
        return r;
    }
    if (s->eventOrder.present && in->deliveryOrder <= s->eventOrder.last) {
        if (in->deliveryOrder == s->eventOrder.last &&
            same_delivery(in, &s->eventOrder.lastDelivery)) { return r; }
        diagnose(VSS_DIAG_ORDER, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    /* Delivery ordering is independent of lane context/quality, EVENT age and
     * STORE admission. Preserve even a semantically rejected newest delivery
     * so that an older TX cannot subsequently introduce a fresh occurrence.
     * This does not apply a semantic context or create a STORE record. */
    remember_delivery(s, in);
    if (s->monitorLost) {
        Store_ProtectRejectedEvent(key, in->signal, now);
        diagnose(VSS_DIAG_TIME, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    lane = (size_t)key->origin * 5u + (size_t)in->signal;
    c = &contexts[lane];
    if (in->quality != VSS_INPUT_VALID || in->availability == VSS_UNAVAILABLE) {
        VssFactTime loss = in->lossAt;
        if (!c->present) { return r; }
        /* An explicit, ordered loss is valid even with unknown loss time.
         * Bad/cross-epoch time stays insufficient, never replaced with now. */
        if (!valid_time(&loss) || loss.domain != key->origin ||
            loss.epoch != now->epoch || loss.latest > now->latest) {
            loss.continuity = VSS_CONTINUITY_UNKNOWN;
        }
        r = lose_context(lane, in->quality == VSS_INPUT_STALE ? VSS_INPUT_STALE : VSS_INPUT_INVALID, &loss);
        return r;
    }
    if (in->value != VSS_VALUE_EVENT) {
        Store_ProtectRejectedEvent(key, in->signal, now);
        diagnose(VSS_DIAG_VALUE, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    event.signal = in->signal;
    if (!project(in, now, &event.meta) ||
        now->earliest < event.meta.original.latest ||
        now->latest - event.meta.original.earliest >= event.meta.useLimit) {
        Store_ProtectRejectedEvent(key, in->signal, now);
        diagnose(VSS_DIAG_TIME, in, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    /* INPUT context first; STORE rejection must not roll back source ordering. */
    c->present = true;
    c->appliedMeta = event.meta;
    c->quality = VSS_INPUT_VALID;
    c->lossPresent = false;
    ++c->revision;
    r.validation = VSS_VALIDATION_APPLICABLE;
    r.contextApplied = true;
    r.storeAdmission = Store_ApplyInput(&event, NULL, NULL);
    if (r.storeAdmission == VSS_STORE_REJECTED) { diagnose(VSS_DIAG_STORE, in, now); }
    return r;
}

VssInputApplication Input_Process(const VssInputEvidence *input,
                                 const VssTimeEvidence *time,
                                 VssDiagnosticEvidence *diagnostics,
                                 size_t capacity, size_t *written)
{
    VssInputApplication r = process(input, time);
    Store_AdvanceWarningDeadlines(time);
    drain(diagnostics, capacity, written);
    return r;
}

static size_t warning_index(VssOrigin origin, VssInputSignal signal)
{ return (size_t)origin * 4u + (size_t)signal - (size_t)VSS_SIGNAL_ANTI_PINCH; }

static uint32_t warning_ttl(VssInputSignal signal)
{
    static const uint32_t ttl[4] = {250u, 1000u, 150u, 500u};
    return ttl[(size_t)signal - (size_t)VSS_SIGNAL_ANTI_PINCH];
}

static void choose_result(VssInputApplication *representative, VssInputApplication r)
{
    bool rejected = r.validation == VSS_VALIDATION_REJECTED || r.storeAdmission == VSS_STORE_REJECTED;
    bool alreadyRejected = representative->validation == VSS_VALIDATION_REJECTED ||
                           representative->storeAdmission == VSS_STORE_REJECTED;
    if (!alreadyRejected && (rejected ||
        (representative->validation == VSS_VALIDATION_NO_CHANGE && r.contextApplied))) {
        *representative = r;
    }
}

static void diagnose_warning(VssDiagnosticCode code, VssOrigin origin, VssInputSignal signal,
                             uint32_t boot, uint32_t tx, const VssWarningEvidence *e,
                             const VssTimeEvidence *time)
{
    VssDiagnosticEvidence d = {0};
    size_t index = VSS_INPUT_CONTEXTS;
    d.code = code; d.signal = signal; d.deliveryOrder = tx;
    d.warningPresent = true; d.warningOrigin = origin; d.domainBoot = boot;
    d.observedAt.continuity = VSS_CONTINUITY_UNKNOWN;
    if (time != NULL) { d.observedAt = *time; }
    if (valid_origin(origin) && signal >= VSS_SIGNAL_ANTI_PINCH && signal <= VSS_SIGNAL_REAR_ACTIVATION) {
        index = VSS_EVENT_CONTEXTS + warning_index(origin, signal);
        d.useLimit = warning_ttl(signal);
    }
    if (e != NULL) {
        d.evidenceSource = e->evidenceSource; d.evidenceBoot = e->evidenceBoot;
        d.evidenceSequence = e->evidenceSequence; d.rawQuality = e->quality;
        d.rawValue = e->value; d.reason = e->reason; d.availability = e->availability; d.meaning = e->meaning;
    }
    protect_diagnostic(d, index);
}

static VssInputApplication warning_loss(VssOrigin origin, VssInputSignal signal,
    VssInputQuality quality, const VssFactTime *loss, bool provenanceLost)
{
    WarningContext *c = &warningContexts[warning_index(origin, signal)];
    VssInputApplication r = unchanged();
    VssQualityChange q = {0};
    bool newlyFenced = provenanceLost && !c->fenced;
    if (!c->present) { return r; }
    if (provenanceLost) { c->fenced = true; }
    if (c->lossPresent && c->observation.quality == quality && !newlyFenced) { return r; }
    if (!c->lossPresent) { c->lossAt = *loss; c->lossPresent = true; }
    c->observation.quality = quality;
    ++c->observation.revision;
    q.signal = signal; q.quality = quality; q.lossAt = c->lossAt;
    q.warningBasis = c->observation.appliedMeta;
    if (!c->observation.lastValidPresent) {
        q.warningBasis.origin = origin;
        q.warningBasis.domainBoot = sources[(size_t)origin].boot;
    }
    q.provenanceLost = c->fenced;
    r.validation = VSS_VALIDATION_QUALITY_CHANGE; r.contextApplied = true;
    r.storeAdmission = Store_ApplyInput(NULL, NULL, &q);
    c->observation.lastApplication = r;
    if (r.storeAdmission == VSS_STORE_REJECTED) {
        diagnose_warning(VSS_DIAG_STORE, origin, signal, q.warningBasis.domainBoot,
                         q.warningBasis.deliveryOrder, &c->observation.latestEvidence, loss);
    }
    return r;
}

static VssInputApplication check_warning_deadlines(const VssTimeEvidence *now)
{
    InputSource *s = &sources[(size_t)now->domain];
    VssInputApplication representative = unchanged();
    VssFactTime monitorLoss = s->lastAdvancingRx;
    bool monitorDue = false;
    size_t i;
    if (s->monitorArmed && monitorLoss.earliest <= UINT64_MAX - 150u &&
        monitorLoss.latest <= UINT64_MAX - 150u && monitorLoss.epoch == now->epoch) {
        monitorLoss.earliest += 150u; monitorLoss.latest += 150u;
        monitorDue = now->latest >= monitorLoss.earliest;
    }
    if (monitorDue) { s->monitorLost = true; }
    /* Fixed 9 per supplied origin (18 total); no other domain's clock subtraction. */
    for (i = 0u; i < 9u; ++i) {
        VssInputSignal signal = (VssInputSignal)i;
        if (i < 5u) {
            if (monitorDue) {
                choose_result(&representative, lose_context((size_t)now->domain * 5u + i,
                              VSS_INPUT_INVALID, &monitorLoss));
            }
        } else {
            WarningContext *c = &warningContexts[warning_index(now->domain, signal)];
            VssFactTime loss = monitorLoss;
            VssInputQuality quality = VSS_INPUT_INVALID;
            bool due = monitorDue;
            if (!c->present || !c->observation.lastValidPresent) { continue; }
            if (c->observation.appliedMeta.original.epoch == now->epoch &&
                now->latest >= c->observation.appliedMeta.validUntil) {
                if (!due || c->observation.appliedMeta.validUntil <= loss.earliest) {
                    loss = c->observation.appliedMeta.original;
                    loss.earliest = c->observation.appliedMeta.validUntil; loss.latest = loss.earliest;
                    quality = VSS_INPUT_STALE;
                }
                due = true;
            }
            if (due) { choose_result(&representative, warning_loss(now->domain, signal, quality, &loss, false)); }
        }
    }
    return representative;
}

static bool same_warning_evidence(const VssWarningEvidence *a, const VssWarningEvidence *b)
{
    return a->value == b->value && a->quality == b->quality && a->reason == b->reason &&
        a->availability == b->availability && a->meaning == b->meaning &&
        a->evidenceSource == b->evidenceSource && a->evidenceBoot == b->evidenceBoot &&
        a->evidenceSequence == b->evidenceSequence && a->ageLowMs == b->ageLowMs &&
        a->ageHighMs == b->ageHighMs && a->ageComplete == b->ageComplete &&
        a->provenanceProven == b->provenanceProven && a->evaluationPresent == b->evaluationPresent &&
        a->activationBasisProven == b->activationBasisProven && same_time(&a->lossAt, &b->lossAt);
}

static bool same_warning_frame(const VssWarningFrame *a, const VssWarningFrame *b)
{
    size_t i;
    /* RX changes on a duplicate are not new payload or liveness evidence.
     * This is typed identity ONLY; adapter raw-byte identity remains a gate. */
    if (a->origin != b->origin || a->source != b->source || a->domainBoot != b->domainBoot ||
        a->deliveryOrder != b->deliveryOrder || a->structureVerified != b->structureVerified) { return false; }
    for (i = 0u; i < 4u; ++i) {
        if (!same_warning_evidence(&a->lanes[i], &b->lanes[i])) { return false; }
    }
    return true;
}

static bool project_warning(const VssWarningFrame *f, VssInputSignal signal,
                            const VssWarningEvidence *e, VssWarningMeta *m)
{
    const VssFactTime *rx = &f->receivedAt;
    if (!e->ageComplete || e->ageLowMs > e->ageHighMs || e->ageHighMs >= 65535u ||
        rx->earliest < e->ageHighMs || rx->latest < e->ageLowMs) { return false; }
    memset(m, 0, sizeof *m);
    m->origin = f->origin; m->domainBoot = f->domainBoot; m->deliveryOrder = f->deliveryOrder;
    m->evidenceSource = e->evidenceSource; m->evidenceBoot = e->evidenceBoot;
    m->evidenceSequence = e->evidenceSequence; m->original = *rx;
    m->original.earliest -= e->ageHighMs; m->original.latest -= e->ageLowMs;
    m->useLimit = warning_ttl(signal);
    if (m->original.earliest > UINT64_MAX - m->useLimit) { return false; }
    m->validUntil = m->original.earliest + m->useLimit;
    m->reason = e->reason; m->availability = e->availability;
    return true;
}

static bool warning_value_valid(VssInputSignal signal, VssInputValue value)
{
    if (signal == VSS_SIGNAL_REAR_ACTIVATION) { return value == VSS_VALUE_ENABLED || value == VSS_VALUE_DISABLED; }
    if (signal == VSS_SIGNAL_REAR_RISK) {
        return value == VSS_VALUE_CLEAR || value == VSS_VALUE_CAUTION || value == VSS_VALUE_EMERGENCY;
    }
    return value == VSS_VALUE_ACTIVE || value == VSS_VALUE_CLEAR;
}

static VssInputApplication warning_lane(const VssWarningFrame *f, VssInputSignal signal,
                                        const VssWarningEvidence *e, const VssTimeEvidence *now)
{
    WarningContext *c = &warningContexts[warning_index(f->origin, signal)];
    VssInputApplication r = unchanged();
    VssFactTime loss = e->lossAt;
    VssStateUpdate state = {0};
    VssInputQuality quality = VSS_INPUT_VALID;
    VssDiagnosticCode reason = VSS_DIAG_VALUE;
    bool same = false, provenanceLost = false;
    if (!valid_time(&loss) || loss.domain != f->origin || loss.epoch != now->epoch || loss.latest > now->latest) {
        loss.continuity = VSS_CONTINUITY_UNKNOWN;
    }
    /* An older source observation cannot change even current quality. */
    if (c->evidenceSeen && e->provenanceProven && e->evidenceBoot == c->evidenceBoot &&
        e->evidenceSource == (VssEvidenceSource)((unsigned)signal - (unsigned)VSS_SIGNAL_ANTI_PINCH) &&
        e->evidenceSequence != 0u && e->evidenceSequence < c->evidenceSequence) {
        diagnose_warning(VSS_DIAG_ORDER, f->origin, signal, f->domainBoot, f->deliveryOrder, e, now);
        r.validation = VSS_VALIDATION_REJECTED;
        return r;
    }
    c->present = true;
    c->observation.latestEvidence = *e;
    if (c->evidenceSeen && e->evidenceBoot != 0u && e->evidenceBoot != c->evidenceBoot) {
        provenanceLost = true; reason = VSS_DIAG_GENERATION;
    }
    if (e->evidenceSource != (VssEvidenceSource)((unsigned)signal - (unsigned)VSS_SIGNAL_ANTI_PINCH) ||
        !e->provenanceProven || e->evidenceBoot == 0u || e->evidenceSequence == 0u ||
        (signal == VSS_SIGNAL_REAR_ACTIVATION && e->evidenceBoot != f->domainBoot)) {
        quality = VSS_INPUT_INVALID; reason = VSS_DIAG_GENERATION;
    } else if (!provenanceLost && !c->fenced) {
        same = c->evidenceSeen && e->evidenceSequence == c->evidenceSequence;
        c->evidenceSeen = true; c->evidenceBoot = e->evidenceBoot; c->evidenceSequence = e->evidenceSequence;
    }
    if (provenanceLost || c->fenced) { quality = VSS_INPUT_INVALID; }
    if (e->quality < VSS_INPUT_VALID || e->quality > VSS_INPUT_UNASSESSED ||
        e->reason < VSS_REASON_NONE || e->reason > VSS_REASON_UNSUPPORTED ||
        e->availability < VSS_AVAILABLE || e->availability > VSS_AVAILABILITY_NOT_PROVIDED ||
        e->meaning < VSS_MEANING_CONFIRMED || e->meaning > VSS_MEANING_UNSUPPORTED) {
        quality = VSS_INPUT_INVALID;
    } else if (e->quality == VSS_INPUT_UNASSESSED ||
               (signal == VSS_SIGNAL_REAR_ACTIVATION && !e->evaluationPresent)) {
        quality = c->observation.lastValidPresent ? VSS_INPUT_INVALID : VSS_INPUT_UNASSESSED;
    } else if (e->quality != VSS_INPUT_VALID) {
        quality = e->quality == VSS_INPUT_STALE ? VSS_INPUT_STALE : VSS_INPUT_INVALID;
    } else if (e->reason != VSS_REASON_NONE || e->meaning != VSS_MEANING_CONFIRMED ||
        !warning_value_valid(signal, e->value) ||
        (signal == VSS_SIGNAL_REAR_ACTIVATION
         ? (e->availability != VSS_AVAILABILITY_NOT_PROVIDED || !e->activationBasisProven)
         : (e->availability != VSS_AVAILABLE && e->availability != VSS_LIMITED))) {
        quality = VSS_INPUT_INVALID;
    }
    if (signal == VSS_SIGNAL_ANTI_PINCH && f->origin == VSS_PRODUCT) {
        quality = VSS_INPUT_UNASSESSED; reason = VSS_DIAG_VALUE;
    }
    if (quality == VSS_INPUT_VALID && (!e->ageComplete || e->ageHighMs >= 65535u ||
        e->ageLowMs > e->ageHighMs)) { quality = VSS_INPUT_INVALID; reason = VSS_DIAG_TIME; }
    if (quality == VSS_INPUT_VALID && same) {
        if (!c->observation.lastValidPresent || c->observation.quality != VSS_INPUT_VALID ||
            e->value != c->lastValue || c->observation.appliedMeta.evidenceSequence != e->evidenceSequence) {
            quality = VSS_INPUT_INVALID; reason = VSS_DIAG_ORDER; /* a value change needs a new source evaluation, never a new TX */
        } else {
            return r; /* same original: liveness only, preserve originalAt/TTL */
        }
    }
    if (quality == VSS_INPUT_VALID) {
        state.signal = signal; state.value = e->value; state.evaluatedAt = *now;
        if (!project_warning(f, signal, e, &state.meta) || now->earliest < state.meta.original.latest) {
            quality = VSS_INPUT_INVALID; reason = VSS_DIAG_TIME;
        } else if (now->latest >= state.meta.validUntil) {
            quality = VSS_INPUT_STALE; reason = VSS_DIAG_TIME;
            /* A rejected new original does not replace the previous lastValid
             * basis; explicit loss evidence (possibly UNKNOWN) stays separate. */
        }
    }
    if (quality != VSS_INPUT_VALID) {
        diagnose_warning(reason, f->origin, signal, f->domainBoot, f->deliveryOrder, e, now);
        return warning_loss(f->origin, signal, quality, &loss, provenanceLost);
    }
    c->observation.appliedMeta = state.meta;
    c->observation.lastValidPresent = true; c->observation.quality = VSS_INPUT_VALID;
    c->lastValue = state.value; c->lossPresent = false;
    ++c->observation.revision;
    r.validation = VSS_VALIDATION_APPLICABLE; r.contextApplied = true;
    r.storeAdmission = Store_ApplyInput(NULL, &state, NULL);
    if (r.storeAdmission == VSS_STORE_REJECTED) {
        diagnose_warning(VSS_DIAG_STORE, f->origin, signal, f->domainBoot, f->deliveryOrder, e, now);
    }
    return r;
}

static VssWarningBundleResult process_warning_frame(const VssWarningFrame *f, const VssTimeEvidence *now)
{
    VssWarningBundleResult result = {VSS_VALIDATION_REJECTED, false, {{0}}};
    InputSource *s;
    size_t i;
    for (i = 0u; i < 4u; ++i) { result.lanes[i] = unchanged(); }
    if (now == NULL || !valid_origin(now->domain)) { diagnose(VSS_DIAG_TIME, NULL, now); return result; }
    s = &sources[(size_t)now->domain];
    if (s->configured && clock_discontinuous(s, now)) {
        VssFactTime loss = *now; loss.continuity = VSS_CONTINUITY_LOST;
        (void)close_source(now->domain, &loss);
        diagnose(VSS_DIAG_TIME, NULL, now); return result;
    }
    if (!valid_time(now)) { diagnose(VSS_DIAG_TIME, NULL, now); return result; }
    if (s->configured) { s->lastTime = *now; }
    if (f == NULL || !f->structureVerified || !valid_origin(f->origin) || f->origin != now->domain ||
        f->source != VSS_SOURCE_DOMAIN || f->domainBoot == 0u || f->deliveryOrder == 0u ||
        !valid_time(&f->receivedAt) || f->receivedAt.domain != now->domain ||
        f->receivedAt.epoch != now->epoch || f->receivedAt.latest > now->earliest) {
        diagnose(VSS_DIAG_ARGUMENT, NULL, now); return result;
    }
    if (!s->open || f->domainBoot != s->boot) {
        if (s->open && f->domainBoot != s->boot && !retired(s, f->domainBoot)) {
            VssFactTime loss = *now; loss.continuity = VSS_CONTINUITY_UNKNOWN;
            (void)close_source(f->origin, &loss);
        }
        diagnose(VSS_DIAG_GENERATION, NULL, now); return result;
    }
    if (s->warningOrder.present && f->deliveryOrder <= s->warningOrder.last) {
        if (f->deliveryOrder == s->warningOrder.last && same_warning_frame(f, &s->warningOrder.lastFrame)) {
            result.frameValidation = VSS_VALIDATION_NO_CHANGE;
        } else { diagnose(VSS_DIAG_ORDER, NULL, now); }
        return result;
    }
    if (s->monitorArmed && f->receivedAt.latest < s->lastAdvancingRx.earliest) {
        diagnose(VSS_DIAG_TIME, NULL, now); return result;
    }
    /* A missed timeout before this actual RX must be applied before recovery.
     * Processing delay after RX does not invent an earlier RX silence. */
    (void)check_warning_deadlines(&f->receivedAt);
    s->warningOrder.present = true; s->warningOrder.last = f->deliveryOrder;
    s->warningOrder.lastFrame = *f; s->lastAdvancingRx = f->receivedAt;
    s->monitorArmed = true; s->monitorLost = false;
    result.frameValidation = VSS_VALIDATION_APPLICABLE;
    for (i = 0u; i < 4u; ++i) {
        VssInputSignal signal = (VssInputSignal)((unsigned)VSS_SIGNAL_REAR_ACTIVATION - (unsigned)i);
        result.lanes[i] = warning_lane(f, signal, &f->lanes[i], now);
        warningContexts[warning_index(f->origin, signal)].observation.lastApplication = result.lanes[i];
    }
    result.complete = true;
    return result;
}

VssWarningBundleResult Input_ProcessWarningFrame(const VssWarningFrame *frame, const VssTimeEvidence *time)
{
    VssWarningBundleResult result = process_warning_frame(frame, time);
    if (valid_time(time)) { (void)check_warning_deadlines(time); }
    Store_AdvanceWarningDeadlines(time);
    return result;
}

bool Input_ReadWarningContext(VssOrigin origin, VssInputSignal signal, VssWarningContextObservation *out)
{
    const WarningContext *c;
    if (!valid_origin(origin) || signal < VSS_SIGNAL_ANTI_PINCH || signal > VSS_SIGNAL_REAR_ACTIVATION ||
        out == NULL) { return false; }
    c = &warningContexts[warning_index(origin, signal)];
    if (!c->present) { return false; }
    *out = c->observation;
    return true;
}

#if VSS_CFG_HOST_TEST
void Input_TestReset(void)
{
    memset(sources, 0, sizeof sources);
    memset(contexts, 0, sizeof contexts);
    memset(warningContexts, 0, sizeof warningContexts);
    memset(pending, 0, sizeof pending);
    memset(&diagnosticState, 0, sizeof diagnosticState);
}
#endif
