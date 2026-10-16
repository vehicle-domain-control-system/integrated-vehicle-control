#include "vss_input_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned checks;
static unsigned groups;
#define CHECK(c) do { ++checks; if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
#define RUN(f) do { f(); ++groups; printf("PASS %s\n", #f); } while (0)

static VssFactTime at(VssOrigin origin, uint64_t t)
{
    VssFactTime r = {VSS_PRODUCT, 1u, 0u, 0u, VSS_CONTINUITY_COMPARABLE};
    r.domain = origin; r.earliest = t; r.latest = t; return r;
}

static void reset(void)
{
    VssTimeEvidence t = at(VSS_PRODUCT, 0u);
    Input_TestReset(); Store_TestReset();
    CHECK(Input_OpenAfterSynchronization(VSS_PRODUCT, 7u, &t));
    t.domain = VSS_TEST; CHECK(Input_OpenAfterSynchronization(VSS_TEST, 7u, &t));
}

static VssWarningFrame frame(VssOrigin origin, uint32_t tx, uint32_t sequence, uint64_t rx)
{
    VssWarningFrame f = {0};
    size_t i;
    f.origin = origin; f.source = VSS_SOURCE_DOMAIN; f.domainBoot = 7u;
    f.deliveryOrder = tx; f.receivedAt = at(origin, rx); f.structureVerified = true;
    for (i = 0u; i < 4u; ++i) {
        VssWarningEvidence *e = &f.lanes[i];
        e->value = i == 0u ? VSS_VALUE_ENABLED : (i == 1u ? VSS_VALUE_EMERGENCY : VSS_VALUE_ACTIVE);
        e->quality = VSS_INPUT_VALID; e->reason = VSS_REASON_NONE;
        e->availability = i == 0u ? VSS_AVAILABILITY_NOT_PROVIDED : VSS_AVAILABLE;
        e->meaning = VSS_MEANING_CONFIRMED;
        e->evidenceSource = (VssEvidenceSource)(3u - (unsigned)i);
        e->evidenceBoot = i == 0u ? 7u : 10u + (uint32_t)i;
        e->evidenceSequence = sequence; e->ageComplete = true; e->provenanceProven = true;
        e->evaluationPresent = true; e->activationBasisProven = true;
        e->lossAt = at(origin, rx);
    }
    return f;
}

static VssWarningBundleResult send_frame(const VssWarningFrame *f)
{ return Input_ProcessWarningFrame(f, &f->receivedAt); }

static VssInputApplication tick(VssOrigin origin, uint64_t t)
{
    VssTimeEvidence now = at(origin, t);
    return Input_Process(NULL, &now, NULL, 0u, NULL);
}

static VssWarningObservation state(VssOrigin origin, VssInputSignal signal)
{
    VssWarningObservation w;
    CHECK(Store_ReadWarning(origin, signal, &w)); return w;
}

static VssWarningContextObservation context(VssOrigin origin, VssInputSignal signal)
{
    VssWarningContextObservation c;
    CHECK(Input_ReadWarningContext(origin, signal, &c)); return c;
}

static void bundle_order_partial_and_borrow(void)
{
    VssWarningFrame f = frame(VSS_TEST, 20u, 1u, 1000u);
    VssWarningBundleResult r;
    VssRearObservation rear;
    VssWarningObservation w;
    size_t i;
    reset(); r = send_frame(&f);
    CHECK(r.complete && r.frameValidation == VSS_VALIDATION_APPLICABLE);
    for (i = 0u; i < 4u; ++i) {
        CHECK(r.lanes[i].validation == VSS_VALIDATION_APPLICABLE && r.lanes[i].contextApplied);
        CHECK(r.lanes[i].storeAdmission == VSS_STORE_UPDATED);
    }
    CHECK(Store_ReadRear(VSS_TEST, &rear) && rear.riskApplicable); /* proves activation precedes risk */
    CHECK(Store_Count() == 0u);
    memset(&f, 0xa5, sizeof f);
    w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK);
    CHECK(w.lastValidValue == VSS_VALUE_EMERGENCY && w.lastValidMeta.original.earliest == 1000u);
    CHECK(w.newSessionAllowed && w.lastValidMeta.evidenceBoot == 11u);
    f = frame(VSS_TEST, 21u, 2u, 1050u);
    Store_TestRejectWarningOnce(VSS_SIGNAL_OCCUPANT_HAZARD);
    f.lanes[3].quality = VSS_INPUT_INVALID;
    r = send_frame(&f);
    CHECK(r.complete && r.lanes[0].storeAdmission == VSS_STORE_UPDATED && r.lanes[1].storeAdmission == VSS_STORE_UPDATED);
    CHECK(r.lanes[2].contextApplied && r.lanes[2].storeAdmission == VSS_STORE_REJECTED);
    CHECK(r.lanes[3].validation == VSS_VALIDATION_QUALITY_CHANGE);
    CHECK(state(VSS_TEST, VSS_SIGNAL_REAR_RISK).lastValidMeta.evidenceSequence == 2u);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).lastValidMeta.evidenceSequence == 1u);
    CHECK(context(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).appliedMeta.evidenceSequence == 2u);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).inputIncomplete);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
}

static void frame_identity_order_and_nonwrap(void)
{
    VssWarningFrame f = frame(VSS_TEST, 20u, 1u, 1000u), duplicate;
    VssWarningBundleResult r;
    VssWarningObservation before, after;
    reset(); (void)send_frame(&f); before = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    duplicate = f; duplicate.receivedAt = at(VSS_TEST, 1100u);
    r = send_frame(&duplicate);
    CHECK(!r.complete && r.frameValidation == VSS_VALIDATION_NO_CHANGE);
    after = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(after.revision == before.revision && after.lastValidMeta.validUntil == before.lastValidMeta.validUntil);
    duplicate.lanes[2].value = VSS_VALUE_CLEAR;
    CHECK(send_frame(&duplicate).frameValidation == VSS_VALIDATION_REJECTED);
    duplicate.deliveryOrder = 19u;
    CHECK(send_frame(&duplicate).frameValidation == VSS_VALIDATION_REJECTED);
    (void)tick(VSS_TEST, 1150u);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).firstLossAt.earliest == 1150u);
    reset(); f = frame(VSS_TEST, UINT32_MAX, 1u, 1000u);
    CHECK(send_frame(&f).complete);
    f.deliveryOrder = 1u; f.lanes[2].evidenceSequence = 2u;
    CHECK(send_frame(&f).frameValidation == VSS_VALIDATION_REJECTED);
    f.origin = VSS_PRODUCT; f.receivedAt.domain = VSS_PRODUCT;
    CHECK(send_frame(&f).complete); /* independent origin TX */
}

static void repeated_original_and_e01_fence(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssWarningObservation w;
    VssWarningBundleResult r;
    reset(); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 1u, 1050u); /* apparent age0 must NOT rebase original */
    r = send_frame(&f); CHECK(r.lanes[2].validation == VSS_VALIDATION_NO_CHANGE);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.lastValidMeta.original.earliest == 1000u && w.lastValidMeta.validUntil == 2000u);
    CHECK(w.lastValidMeta.deliveryOrder == 1u && w.newSessionAllowed);
    f.deliveryOrder = 3u; f.lanes[2].value = VSS_VALUE_CLEAR;
    r = send_frame(&f); CHECK(r.lanes[2].validation == VSS_VALIDATION_QUALITY_CHANGE);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.lastValidValue == VSS_VALUE_ACTIVE && w.quality == VSS_INPUT_INVALID);
    CHECK(w.firstLossAt.earliest == 1050u && w.holdAnchor == 1050u);
    f.deliveryOrder = 4u; f.lanes[2].value = VSS_VALUE_ACTIVE;
    (void)send_frame(&f);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).quality == VSS_INPUT_INVALID);
    f.deliveryOrder = 5u; f.lanes[2].evidenceSequence = 2u;
    CHECK(send_frame(&f).lanes[2].validation == VSS_VALIDATION_APPLICABLE);
    f.deliveryOrder = 6u; f.lanes[2].evidenceSequence = 1u; f.lanes[2].quality = VSS_INPUT_INVALID;
    CHECK(send_frame(&f).lanes[2].validation == VSS_VALIDATION_REJECTED);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).quality == VSS_INPUT_VALID);
    f.deliveryOrder = 7u; f.lanes[2].quality = VSS_INPUT_VALID; f.lanes[2].evidenceSequence = UINT32_MAX;
    CHECK(send_frame(&f).lanes[2].validation == VSS_VALIDATION_APPLICABLE);
    f.deliveryOrder = 8u; f.lanes[2].evidenceSequence = 1u;
    CHECK(send_frame(&f).lanes[2].validation == VSS_VALIDATION_REJECTED);
}

static void quality_reason_provenance_matrix(void)
{
    unsigned variant;
    for (variant = 0u; variant < 21u; ++variant) {
        VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
        VssWarningObservation w;
        VssWarningEvidence *e;
        reset(); (void)send_frame(&f);
        f = frame(VSS_TEST, 2u, 2u, 1050u); e = &f.lanes[2];
        switch (variant) {
        case 0u: e->quality = VSS_INPUT_INVALID; break;
        case 1u: e->quality = VSS_INPUT_STALE; break;
        case 2u: e->quality = VSS_INPUT_UNASSESSED; e->reason = VSS_REASON_NO_DATA; break;
        case 3u: e->availability = VSS_UNAVAILABLE; break;
        case 4u: e->reason = VSS_REASON_DATA_INVALID; break;
        case 5u: e->meaning = VSS_MEANING_UNCONFIRMED; break;
        case 6u: e->meaning = VSS_MEANING_UNSUPPORTED; e->value = VSS_VALUE_CLEAR; break;
        case 7u: e->evidenceBoot = 0u; break;
        case 8u: e->evidenceSequence = 0u; break;
        case 9u: e->provenanceProven = false; break;
        case 10u: e->evidenceSource = VSS_EVIDENCE_CIS_REAR; break;
        case 11u: e->ageComplete = false; break;
        case 12u: e->ageHighMs = 65535u; break;
        case 13u: e->ageLowMs = 2u; e->ageHighMs = 1u; break;
        case 14u: e->ageLowMs = 1100u; e->ageHighMs = 1100u; break;
        case 15u: e->quality = (VssInputQuality)99; break;
        case 16u: e->availability = (VssAvailability)99; break;
        case 17u: e->reason = (VssWarningReason)99; break;
        case 18u: e->meaning = (VssWarningMeaning)99; break;
        case 19u: e->value = (VssInputValue)99; break;
        default: e->ageLowMs = 1000u; e->ageHighMs = 1000u; break;
        }
        CHECK(send_frame(&f).complete);
        w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
        CHECK(w.quality != VSS_INPUT_VALID && w.lastValidValue == VSS_VALUE_ACTIVE);
        CHECK(w.lastValidMeta.original.earliest == 1000u && w.lastValidMeta.evidenceSequence == 1u);
        CHECK(w.firstLossPresent && w.firstLossAt.earliest == 1050u && w.holdAnchor == 1050u);
        CHECK(state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
        f.deliveryOrder = 3u; e->lossAt = at(VSS_TEST, 1060u); f.receivedAt = e->lossAt;
        (void)send_frame(&f);
        CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).firstLossAt.earliest == 1050u);
    }
}

static void unsupported_no_data_activation_e03(void)
{
    VssWarningFrame p = frame(VSS_PRODUCT, 1u, 1u, 1000u), t;
    VssWarningObservation w;
    reset(); p.lanes[0].evaluationPresent = false;
    p.lanes[2].quality = VSS_INPUT_UNASSESSED; p.lanes[2].reason = VSS_REASON_NO_DATA;
    (void)send_frame(&p);
    w = state(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(!w.lastValidPresent && w.quality == VSS_INPUT_UNASSESSED && !w.newSessionAllowed);
    w = state(VSS_PRODUCT, VSS_SIGNAL_ANTI_PINCH);
    CHECK(!w.lastValidPresent && w.quality == VSS_INPUT_UNASSESSED);
    CHECK(!state(VSS_PRODUCT, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    CHECK(!state(VSS_PRODUCT, VSS_SIGNAL_REAR_ACTIVATION).lastValidPresent);
    p = frame(VSS_PRODUCT, 2u, 2u, 1050u); p.lanes[0].activationBasisProven = false;
    (void)send_frame(&p);
    CHECK(!state(VSS_PRODUCT, VSS_SIGNAL_REAR_ACTIVATION).lastValidPresent);
    CHECK(state(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    t = frame(VSS_TEST, 1u, 1u, 1050u); (void)send_frame(&t);
    CHECK(state(VSS_TEST, VSS_SIGNAL_ANTI_PINCH).newSessionAllowed);
    p = frame(VSS_PRODUCT, 3u, 3u, 1100u); (void)send_frame(&p);
    CHECK(state(VSS_PRODUCT, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    p = frame(VSS_PRODUCT, 4u, 4u, 1110u); p.lanes[0].evaluationPresent = false;
    (void)send_frame(&p);
    CHECK(state(VSS_PRODUCT, VSS_SIGNAL_REAR_ACTIVATION).quality == VSS_INPUT_INVALID);
    CHECK(!state(VSS_PRODUCT, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    CHECK(state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
}

static void hold_three_way_all_durations(void)
{
    const VssInputSignal signals[4] = {VSS_SIGNAL_ANTI_PINCH, VSS_SIGNAL_OCCUPANT_HAZARD,
                                      VSS_SIGNAL_REAR_RISK, VSS_SIGNAL_REAR_ACTIVATION};
    const uint32_t holds[4] = {250u, 500u, 150u, 0u};
    size_t i;
    for (i = 0u; i < 4u; ++i) {
        VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
        VssWarningObservation w;
        VssTimeEvidence now;
        uint64_t deadline = 1100u + holds[i];
        size_t lane = 3u - i;
        reset(); (void)send_frame(&f);
        f = frame(VSS_TEST, 2u, 2u, 1100u);
        f.lanes[lane].quality = VSS_INPUT_INVALID;
        (void)send_frame(&f);
        w = state(VSS_TEST, signals[i]);
        CHECK(w.firstLossAt.earliest == 1100u && w.holdAnchor == 1100u);
        if (holds[i] != 0u) {
            now = at(VSS_TEST, deadline - 1u); Store_AdvanceWarningDeadlines(&now);
            w = state(VSS_TEST, signals[i]); CHECK(w.validity == VSS_CANDIDATE_APPLICABLE && w.newSessionAllowed);
            now.latest = deadline; Store_AdvanceWarningDeadlines(&now);
            w = state(VSS_TEST, signals[i]); CHECK(w.validity == VSS_CANDIDATE_UNCERTAIN && !w.newSessionAllowed && !w.holdExpired);
            now = at(VSS_TEST, deadline - 1u); now.continuity = VSS_CONTINUITY_UNKNOWN;
            Store_AdvanceWarningDeadlines(&now);
            CHECK(state(VSS_TEST, signals[i]).validity == VSS_CANDIDATE_UNCERTAIN);
        }
        now = at(VSS_TEST, deadline); Store_AdvanceWarningDeadlines(&now);
        w = state(VSS_TEST, signals[i]); CHECK(w.validity == VSS_CANDIDATE_EXPIRED && !w.newSessionAllowed && w.holdExpired);
        CHECK(w.quality == VSS_INPUT_INVALID && w.lastValidValue != VSS_VALUE_CLEAR);
        now = at(VSS_TEST, deadline - 1u); Store_AdvanceWarningDeadlines(&now);
        CHECK(state(VSS_TEST, signals[i]).validity == VSS_CANDIDATE_EXPIRED); /* expiry latch */
    }
}

static void unknown_loss_and_anchor_minimum(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssWarningObservation w;
    VssTimeEvidence now;
    VssQualityChange q = {0};
    reset(); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 2u, 1050u); f.lanes[2].quality = VSS_INPUT_INVALID;
    f.lanes[2].lossAt.continuity = VSS_CONTINUITY_UNKNOWN; (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.firstLossPresent && !w.holdAnchorPresent && w.validity == VSS_CANDIDATE_UNCERTAIN);
    f.deliveryOrder = 3u; f.receivedAt = at(VSS_TEST, 1060u); f.lanes[2].lossAt = f.receivedAt;
    (void)send_frame(&f); w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(!w.holdAnchorPresent && w.firstLossAt.continuity == VSS_CONTINUITY_UNKNOWN);
    /* STORE unit boundary: a real loss can be later than the original TTL.
     * firstLoss remains 2200 while anchor is 2000; now never becomes either. */
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    q.signal = VSS_SIGNAL_OCCUPANT_HAZARD; q.quality = VSS_INPUT_INVALID;
    q.warningBasis = w.lastValidMeta; q.lossAt = at(VSS_TEST, 2200u);
    CHECK(Store_ApplyInput(NULL, NULL, &q) == VSS_STORE_UPDATED);
    now = at(VSS_TEST, 2400u); Store_AdvanceWarningDeadlines(&now);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.firstLossAt.earliest == 2200u && w.holdAnchor == 2000u && w.newSessionAllowed);
    q.lossAt = at(VSS_TEST, 2400u); (void)Store_ApplyInput(NULL, NULL, &q);
    now = at(VSS_TEST, 2500u); Store_AdvanceWarningDeadlines(&now);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.firstLossAt.earliest == 2200u && w.holdAnchor == 2000u && w.holdExpired);
}

static void rear_fence_new_risk_and_clear(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssRearObservation g;
    VssWarningObservation w;
    reset(); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 2u, 1050u); f.lanes[0].value = VSS_VALUE_DISABLED;
    f.lanes[1].evidenceSequence = 1u; (void)send_frame(&f);
    CHECK(Store_ReadRear(VSS_TEST, &g) && !g.riskApplicable && g.fencePresent);
    CHECK(g.riskFence.evidenceSequence == 1u && state(VSS_TEST, VSS_SIGNAL_REAR_RISK).removed);
    f = frame(VSS_TEST, 3u, 3u, 1100u); f.lanes[1].evidenceSequence = 1u;
    (void)send_frame(&f); CHECK(!state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    f = frame(VSS_TEST, 4u, 4u, 1110u); f.lanes[1].evidenceSequence = 2u;
    f.lanes[1].value = VSS_VALUE_CAUTION; (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK); CHECK(w.lastValidValue == VSS_VALUE_CAUTION && w.newSessionAllowed);
    f = frame(VSS_TEST, 5u, 5u, 1120u); f.lanes[1].evidenceSequence = 3u;
    f.lanes[1].value = VSS_VALUE_CLEAR; (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK); CHECK(w.lastValidValue == VSS_VALUE_CLEAR && !w.newSessionAllowed);
    f = frame(VSS_TEST, 6u, 6u, 1130u); f.lanes[1].quality = VSS_INPUT_INVALID;
    (void)send_frame(&f); w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK);
    CHECK(w.lastValidValue == VSS_VALUE_CLEAR && !w.newSessionAllowed && w.removed);
    /* Changing value on the same evidence cannot escalate either. */
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); f.lanes[1].value = VSS_VALUE_CAUTION; (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 1u, 1050u); (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK);
    CHECK(w.lastValidValue == VSS_VALUE_CAUTION && w.quality == VSS_INPUT_INVALID);
}

static void ttl_liveness_and_delayed_null(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssWarningObservation w;
    uint32_t step;
    reset(); (void)send_frame(&f);
    CHECK(tick(VSS_TEST, 1149u).validation == VSS_VALIDATION_NO_CHANGE);
    (void)tick(VSS_TEST, 2000u);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.firstLossAt.earliest == 1150u && w.holdAnchor == 1150u && w.holdExpired);
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&f);
    for (step = 1u; step <= 20u; ++step) {
        f = frame(VSS_TEST, step + 1u, step + 1u, 1000u + 50u * step);
        f.lanes[2].evidenceSequence = 1u;
        (void)send_frame(&f);
        w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
        CHECK(w.lastValidMeta.original.earliest == 1000u && w.lastValidMeta.validUntil == 2000u);
        if (step < 20u) { CHECK(w.quality == VSS_INPUT_VALID && !w.firstLossPresent); }
        else { CHECK(w.quality == VSS_INPUT_STALE && w.firstLossAt.earliest == 2000u && w.holdAnchor == 2000u); }
    }
    /* RX at 1100 processed at 1200 has no gap at old 1150; first loss at 1250. */
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 2u, 1100u);
    { VssTimeEvidence t = at(VSS_TEST, 1200u); CHECK(Input_ProcessWarningFrame(&f, &t).complete); }
    CHECK(!state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).firstLossPresent);
    (void)tick(VSS_TEST, 1250u); CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).firstLossAt.earliest == 1250u);
}

static void malformed_does_not_advance_or_refresh(void)
{
    unsigned variant;
    for (variant = 0u; variant < 12u; ++variant) {
        VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u), bad;
        VssTimeEvidence now = at(VSS_TEST, 1100u);
        reset(); (void)send_frame(&f); bad = frame(VSS_TEST, 100u, 2u, 1100u);
        switch (variant) {
        case 0u: bad.structureVerified = false; break;
        case 1u: bad.source = VSS_SOURCE_OTHER; break;
        case 2u: bad.domainBoot = 0u; break;
        case 3u: bad.origin = VSS_PRODUCT; break;
        case 4u: bad.deliveryOrder = 0u; break;
        case 5u: bad.receivedAt.domain = VSS_PRODUCT; break;
        case 6u: bad.receivedAt.epoch = 0u; break;
        case 7u: bad.receivedAt.epoch = 2u; break;
        case 8u: bad.receivedAt.continuity = VSS_CONTINUITY_UNKNOWN; break;
        case 9u: bad.receivedAt.latest = 1101u; break;
        case 10u: bad.receivedAt.earliest = 1101u; break;
        default: bad.receivedAt = at(VSS_TEST, 999u); break;
        }
        CHECK(Input_ProcessWarningFrame(&bad, &now).frameValidation == VSS_VALIDATION_REJECTED);
        (void)tick(VSS_TEST, 1150u);
        CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).firstLossAt.earliest == 1150u);
        f = frame(VSS_TEST, 2u, 2u, 1150u); CHECK(send_frame(&f).complete);
        CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).quality == VSS_INPUT_VALID);
    }
}

static void source_boot_clock_and_origin_fences(void)
{
    unsigned variant;
    for (variant = 0u; variant < 5u; ++variant) {
        VssWarningFrame f = frame(VSS_PRODUCT, 1u, 1u, 1000u), t;
        VssTimeEvidence now = at(VSS_PRODUCT, 1050u);
        VssWarningObservation w;
        reset(); (void)send_frame(&f); t = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&t);
        f = frame(VSS_PRODUCT, 2u, 2u, 1050u);
        if (variant == 0u) { f.lanes[2].evidenceBoot++; }
        if (variant == 1u) { f.domainBoot++; }
        if (variant == 2u) { now.epoch++; }
        if (variant == 3u) { now.continuity = VSS_CONTINUITY_LOST; }
        if (variant == 4u) { now = at(VSS_PRODUCT, 900u); }
        (void)Input_ProcessWarningFrame(&f, &now);
        w = state(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD);
        CHECK(w.provenanceLost && !w.newSessionAllowed && w.lastValidMeta.evidenceBoot == 12u);
        CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
        if (variant == 0u) {
            CHECK(state(VSS_PRODUCT, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
            f = frame(VSS_PRODUCT, 3u, 3u, 1060u); (void)send_frame(&f);
            CHECK(state(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD).provenanceLost);
        }
    }
}

static void uncertain_time_age_projection_and_overflow(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1300u);
    VssWarningObservation w;
    VssTimeEvidence now = at(VSS_TEST, 1400u);
    size_t i;
    reset(); f.receivedAt.latest = 1302u;
    for (i = 0u; i < 4u; ++i) { f.lanes[i].ageLowMs = 299u; f.lanes[i].ageHighMs = 301u; }
    CHECK(Input_ProcessWarningFrame(&f, &now).complete);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.lastValidMeta.original.earliest == 999u && w.lastValidMeta.original.latest == 1003u);
    CHECK(w.lastValidMeta.validUntil == 1999u && w.newSessionAllowed);
    now.continuity = VSS_CONTINUITY_UNKNOWN; (void)Input_Process(NULL, &now, NULL, 0u, NULL);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(!w.newSessionAllowed && w.validity == VSS_CANDIDATE_UNCERTAIN && !w.holdExpired);
    reset(); f = frame(VSS_TEST, 1u, 1u, UINT64_MAX - 10u); (void)send_frame(&f);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).lastValidPresent);
    CHECK(context(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).quality == VSS_INPUT_INVALID);
    f.deliveryOrder = 0u; CHECK(send_frame(&f).frameValidation == VSS_VALIDATION_REJECTED);
}

static void event_warning_shared_order_monitor_and_capacity(void)
{
    VssOccurrenceKey key = {VSS_PRODUCT, VSS_SOURCE_DOMAIN, 7u, 1u};
    VssInputEvidence e = {0};
    VssWarningFrame f;
    VssOccurrenceObservation before, after;
    VssTimeEvidence now = at(VSS_PRODUCT, 1000u);
    uint32_t i;
    reset(); e.signal = VSS_SIGNAL_WELCOME; e.value = VSS_VALUE_EVENT; e.quality = VSS_INPUT_VALID;
    e.occurrence = &key; e.receivedAt = now; e.ageComplete = true; e.useLimitMs = 500u; e.availability = VSS_AVAILABLE;
    for (i = 1u; i <= VSS_LEDGER_CAPACITY; ++i) {
        key.occurrenceId = i; e.deliveryOrder = i;
        CHECK(Input_Process(&e, &now, NULL, 0u, NULL).storeAdmission == VSS_STORE_ACCEPTED);
    }
    key.occurrenceId = 1u; CHECK(Store_ReadOccurrence(&key, &before));
    f = frame(VSS_PRODUCT, 128u, 1u, 1000u);
    CHECK(send_frame(&f).frameValidation == VSS_VALIDATION_REJECTED); /* same TX different frame kind */
    f.deliveryOrder = 129u; CHECK(send_frame(&f).complete);
    CHECK(Store_Count() == 128u && state(VSS_PRODUCT, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    e.deliveryOrder = 128u; CHECK(Input_Process(&e, &now, NULL, 0u, NULL).validation == VSS_VALIDATION_REJECTED);
    CHECK(Store_ReadOccurrence(&key, &after) && memcmp(&before, &after, sizeof before) == 0);
    (void)tick(VSS_PRODUCT, 1150u); CHECK(Store_ReadOccurrence(&key, &after));
    CHECK(!after.replayAllowed && after.inputLossAt.earliest == 1150u);
    CHECK(after.state == VSS_OCCURRENCE_PENDING && !after.firstStartFact.present && !after.finalFact.present);
    now = at(VSS_PRODUCT, 1160u); e.receivedAt = now; e.deliveryOrder = 130u;
    CHECK(Input_Process(&e, &now, NULL, 0u, NULL).validation == VSS_VALIDATION_REJECTED);
    f = frame(VSS_PRODUCT, 131u, 2u, 1170u); CHECK(send_frame(&f).complete);
    CHECK(Store_ReadOccurrence(&key, &after) && !after.replayAllowed);
    CHECK(Store_Count() == 128u);
}

static void null_representative_and_diagnostics(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssInputApplication r;
    VssDiagnosticObservation d;
    VssDiagnosticEvidence out[16];
    VssTimeEvidence now = at(VSS_TEST, 1150u);
    size_t written = 55u, i;
    reset(); (void)send_frame(&f);
    Store_TestRejectWarningOnce(VSS_SIGNAL_OCCUPANT_HAZARD);
    r = Input_Process(NULL, &now, NULL, 0u, NULL);
    CHECK(r.validation == VSS_VALIDATION_QUALITY_CHANGE && r.contextApplied && r.storeAdmission == VSS_STORE_REJECTED);
    CHECK(context(VSS_TEST, VSS_SIGNAL_ANTI_PINCH).lastApplication.storeAdmission == VSS_STORE_UPDATED);
    CHECK(context(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).lastApplication.storeAdmission == VSS_STORE_REJECTED);
    CHECK(context(VSS_TEST, VSS_SIGNAL_REAR_ACTIVATION).quality == VSS_INPUT_INVALID);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u);
    for (i = 0u; i < 24u; ++i) {
        f.deliveryOrder = (uint32_t)i + 1u; f.lanes[2].reason = VSS_REASON_DATA_INVALID;
        (void)send_frame(&f);
    }
    CHECK(Input_ReadDiagnostics(&d) && d.pending == 16u && d.lossLatched && d.lossCount == 8u);
    i = VSS_EVENT_CONTEXTS + 4u + 1u;
    CHECK(d.summaryPresent[i] && d.firstLost[i].deliveryOrder == 17u && d.latestLost[i].deliveryOrder == 24u);
    CHECK(d.firstLost[i].warningPresent && !d.firstLost[i].keyPresent && d.firstLost[i].evidenceBoot == 12u);
    now = at(VSS_TEST, 1000u);
    (void)Input_Process(NULL, &now, NULL, 1u, &written); CHECK(written == 55u);
    (void)Input_Process(NULL, &now, out, 16u, &written); CHECK(written == 16u);
    CHECK(out[0].reason == VSS_REASON_DATA_INVALID && out[0].signal == VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(Input_ReadDiagnostics(&d) && d.lossCount == 8u && d.pending == 0u);
}

static void all_contexts_resources_and_read_contract(void)
{
    VssWarningFrame p = frame(VSS_PRODUCT, 1u, 1u, 1000u), t = frame(VSS_TEST, 1u, 1u, 1000u);
    VssWarningObservation w, before;
    VssRearObservation rear;
    VssWarningContextObservation c;
    VssTimeEvidence now = at(VSS_TEST, 1000u);
    clock_t start, end;
    unsigned i;
    reset(); (void)send_frame(&p); (void)send_frame(&t);
    CHECK(VSS_INPUT_CONTEXTS == 18u && VSS_WARNING_CONTEXTS == 8u && VSS_LEDGER_CAPACITY == 128u && VSS_DIAGNOSTIC_CAPACITY == 16u);
    CHECK(!Store_ReadWarning((VssOrigin)99, VSS_SIGNAL_REAR_RISK, &w));
    CHECK(!Store_ReadWarning(VSS_TEST, VSS_SIGNAL_WELCOME, &w));
    CHECK(!Store_ReadWarning(VSS_TEST, VSS_SIGNAL_REAR_RISK, NULL));
    CHECK(!Store_ReadRear((VssOrigin)99, &rear) && !Store_ReadRear(VSS_TEST, NULL));
    CHECK(!Input_ReadWarningContext(VSS_TEST, VSS_SIGNAL_REAR_RISK, NULL));
    CHECK(!Input_ReadWarningContext((VssOrigin)99, VSS_SIGNAL_REAR_RISK, &c));
    before = state(VSS_TEST, VSS_SIGNAL_REAR_RISK); w = before;
    memset(&w, 0xa5, sizeof w); w = state(VSS_TEST, VSS_SIGNAL_REAR_RISK);
    CHECK(memcmp(&before, &w, sizeof w) == 0);
    start = clock();
    for (i = 0u; i < 200000u; ++i) { (void)Input_Process(NULL, &now, NULL, 0u, NULL); }
    end = clock();
    printf("HOST_REFERENCE null+warning-deadlines 200000 calls %.6f seconds; not MCU WCET\n", (double)(end-start) / (double)CLOCKS_PER_SEC);
    printf("HOST_SIZE frame=%u warning=%u rear=%u event=%u diagnostic=%u summary=%u\n",
           (unsigned)sizeof p, (unsigned)sizeof w, (unsigned)sizeof rear,
           (unsigned)sizeof(VssOccurrenceObservation), (unsigned)sizeof(VssDiagnosticEvidence),
           (unsigned)sizeof(VssDiagnosticObservation));
    CHECK(Store_Count() == 0u);
}

static void processing_boundary_restrictions(void)
{
    VssWarningFrame f = frame(VSS_TEST, 1u, 1u, 1000u);
    VssTimeEvidence now;
    VssWarningObservation w;
    VssRearObservation g;
    VssInputApplication r;
    reset(); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 2u, 1160u); f.structureVerified = false;
    CHECK(!send_frame(&f).complete);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.quality == VSS_INPUT_INVALID && w.firstLossAt.earliest == 1150u);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed);
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&f);
    (void)Input_Process(NULL, NULL, NULL, 0u, NULL);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.validity == VSS_CANDIDATE_UNCERTAIN && !w.newSessionAllowed && !w.firstLossPresent);
    now = at(VSS_TEST, 1050u); (void)Input_Process(NULL, &now, NULL, 0u, NULL);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    f = frame(VSS_TEST, 2u, 2u, 1060u); f.lanes[1].evidenceBoot++;
    (void)send_frame(&f);
    CHECK(Store_ReadRear(VSS_TEST, &g) && !g.riskApplicable && g.fencePresent);
    CHECK(state(VSS_TEST, VSS_SIGNAL_REAR_RISK).removed);
    /* A normal state rejected by STORE cannot be retried by a same-source
     * repeat or leave the old ACTIVE authorized while a newer CLEAR was lost. */
    reset(); f = frame(VSS_TEST, 1u, 1u, 1000u); (void)send_frame(&f);
    f = frame(VSS_TEST, 2u, 2u, 1050u); f.lanes[2].value = VSS_VALUE_CLEAR;
    Store_TestRejectWarningOnce(VSS_SIGNAL_OCCUPANT_HAZARD); (void)send_frame(&f);
    w = state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD);
    CHECK(w.lastValidValue == VSS_VALUE_ACTIVE && w.inputIncomplete && !w.newSessionAllowed);
    f.deliveryOrder = 3u; CHECK(send_frame(&f).lanes[2].validation == VSS_VALIDATION_NO_CHANGE);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    f.deliveryOrder = 4u; f.lanes[2].quality = VSS_INPUT_INVALID;
    CHECK(send_frame(&f).lanes[2].storeAdmission == VSS_STORE_UPDATED);
    CHECK(!state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    f = frame(VSS_TEST, 5u, 3u, 1100u); (void)send_frame(&f);
    CHECK(state(VSS_TEST, VSS_SIGNAL_OCCUPANT_HAZARD).newSessionAllowed);
    /* First rejected lane then a later STORE rejection: preserve all three
     * fields from the first rejection; independent earlier changes survive. */
    Store_TestRejectWarningOnce(VSS_SIGNAL_REAR_RISK);
    now = at(VSS_TEST, 1250u); r = Input_Process(NULL, &now, NULL, 0u, NULL);
    CHECK(r.contextApplied && r.validation == VSS_VALIDATION_QUALITY_CHANGE && r.storeAdmission == VSS_STORE_REJECTED);
    CHECK(context(VSS_TEST, VSS_SIGNAL_ANTI_PINCH).quality == VSS_INPUT_INVALID);
}

static void per_signal_ttl_and_eighteen_contexts(void)
{
    unsigned origin, signal;
    VssOccurrenceKey key = {VSS_PRODUCT, VSS_SOURCE_DOMAIN, 7u, 1u};
    VssInputEvidence e = {0};
    VssInputContextObservation ec;
    VssWarningFrame f;
    VssTimeEvidence now;
    VssWarningObservation w;
    reset();
    for (origin = 0u; origin < 2u; ++origin) {
        key.origin = (VssOrigin)origin; now = at(key.origin, 1000u);
        e.value = VSS_VALUE_EVENT; e.quality = VSS_INPUT_VALID; e.occurrence = &key;
        e.receivedAt = now; e.ageComplete = true; e.useLimitMs = 500u;
        for (signal = 0u; signal < 5u; ++signal) {
            e.signal = (VssInputSignal)signal; e.deliveryOrder = signal + 1u; key.occurrenceId = signal + 1u;
            CHECK(Input_Process(&e, &now, NULL, 0u, NULL).storeAdmission == VSS_STORE_ACCEPTED);
        }
        f = frame(key.origin, 6u, 1u, 1000u); CHECK(send_frame(&f).complete);
    }
    CHECK(Store_Count() == 10u);
    (void)tick(VSS_TEST, 1150u);
    for (signal = 0u; signal < 5u; ++signal) {
        CHECK(Input_ReadContext(VSS_TEST, (VssInputSignal)signal, &ec) && ec.quality == VSS_INPUT_INVALID);
        CHECK(Input_ReadContext(VSS_PRODUCT, (VssInputSignal)signal, &ec) && ec.quality == VSS_INPUT_VALID);
    }
    /* Original TTL wins over a later RX silence boundary for each signal. */
    for (signal = 0u; signal < 4u; ++signal) {
        const uint32_t ttl[4] = {250u, 1000u, 150u, 500u};
        VssInputSignal s = (VssInputSignal)((unsigned)VSS_SIGNAL_ANTI_PINCH + signal);
        size_t lane = 3u - (size_t)signal;
        reset(); f = frame(VSS_TEST, 1u, 1u, 2000u);
        f.lanes[lane].ageLowMs = ttl[signal] - 1u; f.lanes[lane].ageHighMs = ttl[signal] - 1u;
        CHECK(send_frame(&f).lanes[lane].validation == VSS_VALIDATION_APPLICABLE);
        w = state(VSS_TEST, s); CHECK(w.lastValidMeta.validUntil == 2001u && !w.firstLossPresent);
        (void)tick(VSS_TEST, 2001u); w = state(VSS_TEST, s);
        CHECK(w.quality == VSS_INPUT_STALE && w.firstLossAt.earliest == 2001u && w.holdAnchor == 2001u);
        if (signal == 3u) { CHECK(w.holdExpired && !state(VSS_TEST, VSS_SIGNAL_REAR_RISK).newSessionAllowed); }
        reset(); f = frame(VSS_TEST, 1u, 1u, 2000u);
        f.lanes[lane].ageLowMs = ttl[signal]; f.lanes[lane].ageHighMs = ttl[signal];
        CHECK(send_frame(&f).lanes[lane].validation == VSS_VALIDATION_QUALITY_CHANGE);
        CHECK(!state(VSS_TEST, s).lastValidPresent);
    }
}

int main(void)
{
    RUN(bundle_order_partial_and_borrow);
    RUN(frame_identity_order_and_nonwrap);
    RUN(repeated_original_and_e01_fence);
    RUN(quality_reason_provenance_matrix);
    RUN(unsupported_no_data_activation_e03);
    RUN(hold_three_way_all_durations);
    RUN(unknown_loss_and_anchor_minimum);
    RUN(rear_fence_new_risk_and_clear);
    RUN(ttl_liveness_and_delayed_null);
    RUN(malformed_does_not_advance_or_refresh);
    RUN(source_boot_clock_and_origin_fences);
    RUN(uncertain_time_age_projection_and_overflow);
    RUN(event_warning_shared_order_monitor_and_capacity);
    RUN(null_representative_and_diagnostics);
    RUN(all_contexts_resources_and_read_contract);
    RUN(processing_boundary_restrictions);
    RUN(per_signal_ttl_and_eighteen_contexts);
    printf("PASS P1-B %u groups, %u checks; raw/network/audio/target/board NOT_RUN\n", groups, checks);
    return 0;
}
