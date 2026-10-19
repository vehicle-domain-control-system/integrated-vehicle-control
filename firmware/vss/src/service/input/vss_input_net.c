#include <stddef.h>
#include <string.h>

#include "vss_input_net.h"
#include "../../infra/vss_time.h"

/* The VssTime clock never restarts while VSS runs: one epoch. */
#define INPUT_NET_EPOCH 1u

/* Receive times further back than this are taken as received in the future
 * (the 32-bit difference wrapped). */
#define INPUT_NET_MAX_RX_BACK_MS 0x7FFFFFFFu

/* Status codes are the HEALTH codes: fault bit n = fault kind n, fault code =
 * kind + 1, state and availability in the same order. */
#define INPUT_NET_SAME(name, a, b) \
    typedef char input_net_check_##name[((long)(a) == (long)(b)) ? 1 : -1]
INPUT_NET_SAME(state_startup, NET_VSS_STATE_STARTUP, VSS_SERVICE_STARTUP);
INPUT_NET_SAME(state_ready, NET_VSS_STATE_READY, VSS_SERVICE_READY);
INPUT_NET_SAME(state_playing, NET_VSS_STATE_PLAYING, VSS_SERVICE_PLAYING);
INPUT_NET_SAME(state_fault, NET_VSS_STATE_FAULT, VSS_SERVICE_FAULT);
INPUT_NET_SAME(avail_full, NET_VSS_AVAILABILITY_FULL, VSS_SERVICE_FULL);
INPUT_NET_SAME(avail_degraded, NET_VSS_AVAILABILITY_DEGRADED, VSS_SERVICE_DEGRADED);
INPUT_NET_SAME(avail_unavailable, NET_VSS_AVAILABILITY_UNAVAILABLE, VSS_SERVICE_UNAVAILABLE);
INPUT_NET_SAME(bit_asset, NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE, (1u << VSS_FAULT_ASSET));
INPUT_NET_SAME(bit_start, NET_VSS_FAULT_PLAYBACK_START_FAILURE, (1u << VSS_FAULT_START));
INPUT_NET_SAME(bit_output, NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE, (1u << VSS_FAULT_OUTPUT));
INPUT_NET_SAME(bit_state, NET_VSS_FAULT_PLAYBACK_STATE_FAILURE, (1u << VSS_FAULT_STATE));
INPUT_NET_SAME(bit_init, NET_VSS_FAULT_INITIALIZATION_FAILURE, (1u << VSS_FAULT_INIT));
INPUT_NET_SAME(code_asset, NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE, VSS_FAULT_ASSET + 1);
INPUT_NET_SAME(code_start, NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE, VSS_FAULT_START + 1);
INPUT_NET_SAME(code_output, NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE, VSS_FAULT_OUTPUT + 1);
INPUT_NET_SAME(code_state, NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE, VSS_FAULT_STATE + 1);
INPUT_NET_SAME(code_init, NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE, VSS_FAULT_INIT + 1);
INPUT_NET_SAME(kinds_fit_mask, VSS_FAULT_KINDS, 5);

#define INPUT_NET_FAULT_MASK_ALL ((1u << VSS_FAULT_KINDS) - 1u)

static VssInputNetObservation s_obs[2];
/* Highest TX seen per message within the accepted boot. */
typedef struct
{
    uint32_t eventTx;
    uint32_t warningTx;
} InputNetTx;
static InputNetTx s_tx[2];
/* Scratch for the converted frame (too large for the stack). */
static VssWarningFrame s_frame;

static bool valid_origin(VssOrigin origin)
{
    return (origin == VSS_PRODUCT) || (origin == VSS_TEST);
}

static void count(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        ++*counter;
    }
}

static VssTimeEvidence now_evidence(VssOrigin origin)
{
    VssTimeEvidence t;
    uint64_t now = VssTime_NowMs();

    t.domain = origin;
    t.epoch = INPUT_NET_EPOCH;
    t.earliest = now;
    t.latest = now;
    t.continuity = VSS_CONTINUITY_COMPARABLE;
    return t;
}

/* received_at_ms is the low 32 bits of the same clock as now. */
static VssFactTime receive_time(VssOrigin origin, uint32_t rxMs32, uint64_t now)
{
    VssFactTime t;
    uint32_t back = (uint32_t)((uint32_t)now - rxMs32);

    t.domain = origin;
    t.epoch = INPUT_NET_EPOCH;
    if ((back > INPUT_NET_MAX_RX_BACK_MS) || ((uint64_t)back > now))
    {
        t.earliest = now;
        t.latest = now;
        t.continuity = VSS_CONTINUITY_UNKNOWN;
    }
    else
    {
        t.earliest = now - back;
        t.latest = t.earliest;
        t.continuity = VSS_CONTINUITY_COMPARABLE;
    }
    return t;
}

static VssInputQuality quality_of(NetValueQuality q)
{
    switch (q)
    {
    case NET_QUALITY_OK:      return VSS_INPUT_VALID;
    case NET_QUALITY_STALE:   return VSS_INPUT_STALE;
    case NET_QUALITY_NO_DATA: return VSS_INPUT_UNASSESSED;
    default:                  return VSS_INPUT_INVALID;
    }
}

static VssAvailability availability_of(NetAvailability a)
{
    switch (a)
    {
    case NET_AVAILABILITY_AVAILABLE: return VSS_AVAILABLE;
    case NET_AVAILABILITY_LIMITED:   return VSS_LIMITED;
    default:                         return VSS_UNAVAILABLE;
    }
}

static VssWarningReason reason_of(NetQualityReason r)
{
    switch (r)
    {
    case NET_QUALITY_REASON_NONE:    return VSS_REASON_NONE;
    case NET_QUALITY_REASON_NO_DATA: return VSS_REASON_NO_DATA;
    case NET_QUALITY_REASON_STALE:   return VSS_REASON_STALE;
    default:                         return VSS_REASON_DATA_INVALID;
    }
}

/* Reported age plus the receive-path allowance; 65535 is a saturated age. */
static void age_interval(uint16_t ageMs, uint32_t *low, uint32_t *high, bool *complete)
{
    *low = ageMs;
    *high = (uint32_t)ageMs + (uint32_t)VSS_CFG_RX_GAP_BUDGET_MS;
    *complete = (ageMs != NET_AGE_SATURATED_MS);
}

bool VssInputNet_ConvertEvent(VssOrigin origin, const NetVssEvent *m, const NetCanRxMeta *rx,
                              const VssFactTime *receivedAt, VssOccurrenceKey *key,
                              VssInputEvidence *out)
{
    VssInputSignal signal;

    if (!valid_origin(origin) || (m == NULL) || (rx == NULL) || (receivedAt == NULL) ||
        (key == NULL) || (out == NULL))
    {
        return false;
    }
    switch (m->event_kind)
    {
    case NET_VSS_EVENT_WELCOME:         signal = VSS_SIGNAL_WELCOME; break;
    case NET_VSS_EVENT_GOODBYE:         signal = VSS_SIGNAL_GOODBYE; break;
    case NET_VSS_EVENT_LOCK_COMPLETE:   signal = VSS_SIGNAL_LOCK_COMPLETE; break;
    case NET_VSS_EVENT_UNLOCK_COMPLETE: signal = VSS_SIGNAL_UNLOCK_COMPLETE; break;
    case NET_VSS_EVENT_LOCK_ERROR:      signal = VSS_SIGNAL_LOCK_ERROR; break;
    default:                            return false;
    }
    if ((m->value_quality > NET_QUALITY_NO_DATA) ||
        (m->input_availability > NET_AVAILABILITY_UNAVAILABLE))
    {
        return false;
    }
    key->origin = origin;
    key->source = VSS_SOURCE_DOMAIN;
    key->generation = rx->source.boot_id;
    key->occurrenceId = m->occurrence_id;

    memset(out, 0, sizeof *out);
    out->signal = signal;
    out->value = VSS_VALUE_EVENT;
    out->quality = quality_of(m->value_quality);
    out->occurrence = key;
    out->deliveryOrder = rx->source.tx_sequence;
    out->receivedAt = *receivedAt;
    age_interval(m->original_age_ms, &out->ageLowMs, &out->ageHighMs, &out->ageComplete);
    out->useLimitMs = m->use_limit_ms;
    out->relatedCommand = m->related_command_id;
    out->availability = availability_of(m->input_availability);
    /* INPUT reads the loss time for a quality other than OK and for an
     * unavailable input. */
    if ((out->quality != VSS_INPUT_VALID) || (out->availability == VSS_UNAVAILABLE))
    {
        out->lossAt = *receivedAt;
    }
    return true;
}

static bool warning_codes_valid(const NetVssWarningState *m)
{
    if ((m->antipinch_state > NET_FLAG_YES) || (m->occupant_hazard_state > NET_FLAG_YES) ||
        (m->rear_obstacle_state > NET_REAR_WARNING_EMERGENCY) ||
        (m->rear_detection_activation > NET_FLAG_YES))
    {
        return false;
    }
    if ((m->antipinch_quality > NET_QUALITY_NO_DATA) || (m->occupant_hazard_quality > NET_QUALITY_NO_DATA) ||
        (m->rear_obstacle_quality > NET_QUALITY_NO_DATA) || (m->rear_activation_quality > NET_QUALITY_NO_DATA))
    {
        return false;
    }
    if ((m->antipinch_reason > NET_QUALITY_REASON_DATA_INVALID) ||
        (m->occupant_hazard_reason > NET_QUALITY_REASON_DATA_INVALID) ||
        (m->rear_obstacle_reason > NET_QUALITY_REASON_DATA_INVALID) ||
        (m->rear_activation_reason > NET_QUALITY_REASON_DATA_INVALID))
    {
        return false;
    }
    if ((m->antipinch_availability > NET_AVAILABILITY_UNAVAILABLE) ||
        (m->occupant_hazard_availability > NET_AVAILABILITY_UNAVAILABLE) ||
        (m->rear_obstacle_availability > NET_AVAILABILITY_UNAVAILABLE))
    {
        return false;
    }
    if (m->activation_meta_format == 1u)
    {
        return true;
    }
    /* Format 0: the activation metadata is absent and must be all zero. */
    return (m->activation_meta_format == 0u) && (m->rear_detection_activation == 0u) &&
           (m->rear_activation_quality == 0u) && (m->rear_activation_reason == 0u) &&
           (m->rear_activation_sequence == 0u) && (m->rear_activation_age_ms == 0u);
}

static void fill_lane(VssWarningEvidence *e, VssInputValue value, NetValueQuality quality,
                      NetQualityReason reason, VssAvailability availability,
                      VssEvidenceSource source, uint32_t boot, uint32_t sequence,
                      uint16_t ageMs, const VssFactTime *receivedAt)
{
    memset(e, 0, sizeof *e);
    e->value = value;
    e->quality = quality_of(quality);
    e->reason = reason_of(reason);
    e->availability = availability;
    e->meaning = ((quality == NET_QUALITY_OK) && (reason == NET_QUALITY_REASON_NONE))
                 ? VSS_MEANING_CONFIRMED : VSS_MEANING_UNCONFIRMED;
    e->evidenceSource = source;
    e->evidenceBoot = boot;
    e->evidenceSequence = sequence;
    age_interval(ageMs, &e->ageLowMs, &e->ageHighMs, &e->ageComplete);
    e->provenanceProven = (boot != 0u) && (sequence != 0u);
    e->evaluationPresent = true;
    e->activationBasisProven = false;
    e->lossAt = *receivedAt;
}

static VssInputValue flag_value(NetFlag f)
{
    if (f == NET_FLAG_NO)  { return VSS_VALUE_CLEAR; }
    if (f == NET_FLAG_YES) { return VSS_VALUE_ACTIVE; }
    return VSS_VALUE_UNSUPPORTED;
}

static VssInputValue rear_value(NetRearWarningState s)
{
    switch (s)
    {
    case NET_REAR_WARNING_CLEAR:     return VSS_VALUE_CLEAR;
    case NET_REAR_WARNING_CAUTION:   return VSS_VALUE_CAUTION;
    case NET_REAR_WARNING_EMERGENCY: return VSS_VALUE_EMERGENCY;
    default:                         return VSS_VALUE_UNSUPPORTED;
    }
}

static VssInputValue activation_value(NetFlag f)
{
    if (f == NET_FLAG_NO)  { return VSS_VALUE_DISABLED; }
    if (f == NET_FLAG_YES) { return VSS_VALUE_ENABLED; }
    return VSS_VALUE_UNSUPPORTED;
}

/* The anti-pinch lane's fixed pattern while its producer is not supported. */
static bool antipinch_unsupported(const NetVssWarningState *m)
{
    return (m->antipinch_quality == NET_QUALITY_NO_DATA) &&
           (m->antipinch_reason == NET_QUALITY_REASON_NO_DATA) &&
           (m->antipinch_availability == NET_AVAILABILITY_UNAVAILABLE) &&
           (m->antipinch_source_boot == 0u) && (m->antipinch_source_sequence == 0u) &&
           (m->antipinch_original_age_ms == NET_AGE_SATURATED_MS);
}

bool VssInputNet_ConvertWarning(VssOrigin origin, const NetVssWarningState *m,
                                const NetCanRxMeta *rx, const VssFactTime *receivedAt,
                                VssWarningFrame *out)
{
    bool activationMeta;
    VssWarningEvidence *lane;

    if ((out == NULL) || !valid_origin(origin) || (m == NULL) || (rx == NULL) || (receivedAt == NULL))
    {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->origin = origin;
    out->source = VSS_SOURCE_DOMAIN;
    out->domainBoot = rx->source.boot_id;
    out->deliveryOrder = rx->source.tx_sequence;
    out->receivedAt = *receivedAt;
    out->structureVerified = warning_codes_valid(m);

    /* Lane 0: rear activation, evaluated by DOMAIN in its own boot. */
    activationMeta = (m->activation_meta_format == 1u);
    lane = &out->lanes[0];
    fill_lane(lane, activation_value(m->rear_detection_activation), m->rear_activation_quality,
              m->rear_activation_reason, VSS_AVAILABILITY_NOT_PROVIDED, VSS_EVIDENCE_DOMAIN_ACTIVATION,
              rx->source.boot_id, m->rear_activation_sequence, m->rear_activation_age_ms, receivedAt);
    lane->evaluationPresent = activationMeta;
    lane->activationBasisProven = activationMeta;

    /* Lane 1: rear risk. */
    fill_lane(&out->lanes[1], rear_value(m->rear_obstacle_state), m->rear_obstacle_quality,
              m->rear_obstacle_reason, availability_of(m->rear_obstacle_availability),
              VSS_EVIDENCE_CIS_REAR, m->rear_obstacle_source_boot, m->rear_obstacle_source_sequence,
              m->rear_obstacle_original_age_ms, receivedAt);

    /* Lane 2: occupant hazard. */
    fill_lane(&out->lanes[2], flag_value(m->occupant_hazard_state), m->occupant_hazard_quality,
              m->occupant_hazard_reason, availability_of(m->occupant_hazard_availability),
              VSS_EVIDENCE_PI_VIA_CIS, m->occupant_hazard_source_boot,
              m->occupant_hazard_source_sequence, m->occupant_hazard_original_age_ms, receivedAt);

    /* Lane 3: anti-pinch. */
    lane = &out->lanes[3];
    fill_lane(lane, flag_value(m->antipinch_state), m->antipinch_quality, m->antipinch_reason,
              availability_of(m->antipinch_availability), VSS_EVIDENCE_WINDOW,
              m->antipinch_source_boot, m->antipinch_source_sequence,
              m->antipinch_original_age_ms, receivedAt);
    if (antipinch_unsupported(m))
    {
        lane->meaning = VSS_MEANING_UNSUPPORTED;
    }
    return out->structureVerified;
}

/* First message of an origin opens INPUT for its boot; another boot is a
 * DOMAIN restart the Network accepted. Only a message INPUT can take opens or
 * changes the generation: TX not 0 and a receive time placed on the clock
 * (the caller adds its own message rule). A refusal (boot 0, a boot INPUT
 * replaced, unusable time) leaves the generation as it is; INPUT then
 * rejects the message itself. */
static void follow_generation(VssOrigin origin, const NetCanRxMeta *rx, const VssFactTime *receivedAt,
                              const VssTimeEvidence *now)
{
    VssInputNetObservation *o = &s_obs[(size_t)origin];
    uint32_t boot = rx->source.boot_id;

    if ((boot == 0u) || (rx->source.tx_sequence == 0u) ||
        (receivedAt->continuity != VSS_CONTINUITY_COMPARABLE))
    {
        return;
    }
    if (!o->generationOpen)
    {
        if (Input_OpenAfterSynchronization(origin, boot, now))
        {
            o->generationOpen = true;
            o->acceptedBoot = boot;
            memset(&s_tx[(size_t)origin], 0, sizeof s_tx[(size_t)origin]);
        }
        return;
    }
    if ((boot != o->acceptedBoot) && Input_AcceptNewGeneration(origin, boot, now))
    {
        o->acceptedBoot = boot;
        count(&o->generationChanges);
        memset(&s_tx[(size_t)origin], 0, sizeof s_tx[(size_t)origin]);
    }
}

/* TX_SEQUENCE starts at 1 in each sender boot. Back at 1 within the accepted
 * boot means the sender restarted without a new boot id: INPUT then rejects
 * the stream as out of order until it passes the old TX. Counted to name the
 * cause. */
static void watch_tx(VssInputNetObservation *o, uint32_t *highest, const NetCanRxMeta *rx)
{
    uint32_t tx = rx->source.tx_sequence;

    if (!o->generationOpen || (rx->source.boot_id != o->acceptedBoot) || (tx == 0u))
    {
        return;
    }
    if ((tx == 1u) && (*highest > 1u))
    {
        count(&o->txRestartsSameBoot);
    }
    if (tx > *highest)
    {
        *highest = tx;
    }
}

void VssInputNet_OnEvent(VssOrigin origin, const NetVssEvent *m, const NetCanRxMeta *rx)
{
    VssInputNetObservation *o;
    VssTimeEvidence now;
    VssFactTime receivedAt;
    VssOccurrenceKey key;
    VssInputEvidence evidence;

    if (!valid_origin(origin))
    {
        return;
    }
    o = &s_obs[(size_t)origin];
    if (rx == NULL)
    {
        count(&o->eventsDiscarded);
        return;
    }
    if (!VssTime_IsRegistered())
    {
        count(&o->inputsWithoutClock);
        return;
    }
    now = now_evidence(origin);
    receivedAt = receive_time(origin, rx->received_at_ms, now.latest);
    if (!VssInputNet_ConvertEvent(origin, m, rx, &receivedAt, &key, &evidence))
    {
        count(&o->eventsDiscarded);
        return;
    }
    if (m->occurrence_id != 0u)
    {
        follow_generation(origin, rx, &receivedAt, &now);
    }
    watch_tx(o, &s_tx[(size_t)origin].eventTx, rx);
    o->lastEvent = Input_Process(&evidence, &now, NULL, 0u, NULL);
    count(&o->eventsForwarded);
}

void VssInputNet_OnWarningState(VssOrigin origin, const NetVssWarningState *m, const NetCanRxMeta *rx)
{
    VssInputNetObservation *o;
    VssTimeEvidence now;
    VssFactTime receivedAt;
    bool verified;

    if (!valid_origin(origin))
    {
        return;
    }
    o = &s_obs[(size_t)origin];
    if ((m == NULL) || (rx == NULL))
    {
        count(&o->warningFramesUnverified);
        return;
    }
    if (!VssTime_IsRegistered())
    {
        count(&o->inputsWithoutClock);
        return;
    }
    now = now_evidence(origin);
    receivedAt = receive_time(origin, rx->received_at_ms, now.latest);
    verified = VssInputNet_ConvertWarning(origin, m, rx, &receivedAt, &s_frame);
    o->warningRawQuality[0] = m->rear_activation_quality;
    o->warningRawReason[0] = m->rear_activation_reason;
    o->warningRawQuality[1] = m->rear_obstacle_quality;
    o->warningRawReason[1] = m->rear_obstacle_reason;
    o->warningRawQuality[2] = m->occupant_hazard_quality;
    o->warningRawReason[2] = m->occupant_hazard_reason;
    o->warningRawQuality[3] = m->antipinch_quality;
    o->warningRawReason[3] = m->antipinch_reason;
    /* A frame that failed the structure rule does not change the generation;
     * INPUT rejects it without consuming its order. */
    if (verified)
    {
        follow_generation(origin, rx, &receivedAt, &now);
        watch_tx(o, &s_tx[(size_t)origin].warningTx, rx);
    }
    else
    {
        count(&o->warningFramesUnverified);
    }
    o->lastWarning = Input_ProcessWarningFrame(&s_frame, &now);
    count(&o->warningFramesForwarded);
}

static uint16_t age_ms16(uint64_t ms)
{
    return (ms >= (uint64_t)NET_AGE_SATURATED_MS) ? (uint16_t)NET_AGE_SATURATED_MS : (uint16_t)ms;
}

bool VssInputNet_ToNetStatus(const VssStatusSnapshot *s, const VssTimeEvidence *now, NetVssStatus *out)
{
    if ((s == NULL) || (now == NULL) || (out == NULL))
    {
        return false;
    }
    if ((s->state > (uint8_t)VSS_SERVICE_FAULT) || (s->availability > (uint8_t)VSS_SERVICE_UNAVAILABLE) ||
        ((s->currentMask & (uint8_t)~INPUT_NET_FAULT_MASK_ALL) != 0u) || (s->reportSequence == 0u))
    {
        return false;
    }
    if ((now->continuity != VSS_CONTINUITY_COMPARABLE) || (now->epoch == 0u) ||
        (now->earliest > now->latest) || (now->earliest < s->evaluatedAtMs))
    {
        return false;
    }
    if (s->lastFaultPresent &&
        (((unsigned)s->lastFaultKind >= (unsigned)VSS_FAULT_KINDS) || (now->earliest < s->lastFaultAtMs)))
    {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->vss_state = s->state;
    out->vss_availability = s->availability;
    out->vss_accepting_events = s->accepting ? NET_FLAG_YES : NET_FLAG_NO;
    /* Only the output path fault is an active output fault; fault_mask
     * carries every current fault. */
    out->vss_fault_active = ((s->currentMask & (1u << VSS_FAULT_OUTPUT)) != 0u) ? NET_FLAG_YES : NET_FLAG_NO;
    out->fault_mask = s->currentMask;
    if (s->lastFaultPresent)
    {
        out->vss_last_fault = (uint16_t)((unsigned)s->lastFaultKind + 1u);
        out->last_fault_occurrence = s->lastFaultOccurrence;
        out->last_fault_age_ms = age_ms16(now->latest - s->lastFaultAtMs);
        out->last_fault_quality = NET_QUALITY_OK;
    }
    else
    {
        out->vss_last_fault = NET_VSS_FAULT_CODE_NONE;
        out->last_fault_occurrence = 0u;
        out->last_fault_age_ms = 0u;
        out->last_fault_quality = NET_QUALITY_NO_DATA;
    }
    out->update_sequence = s->reportSequence;
    out->source_age_ms = age_ms16(now->latest - s->evaluatedAtMs);
    return true;
}

bool VssInputNet_Read(VssOrigin origin, VssInputNetObservation *out)
{
    if (!valid_origin(origin) || (out == NULL))
    {
        return false;
    }
    *out = s_obs[(size_t)origin];
    return true;
}

void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)
{
    VssInputNet_OnEvent(VSS_PRODUCT, message, meta);
}

void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)
{
    VssInputNet_OnWarningState(VSS_PRODUCT, message, meta);
}

#if VSS_CFG_HOST_TEST
void VssInputNet_ResetForTest(void)
{
    memset(s_obs, 0, sizeof s_obs);
    memset(s_tx, 0, sizeof s_tx);
    memset(&s_frame, 0, sizeof s_frame);
}
#endif
