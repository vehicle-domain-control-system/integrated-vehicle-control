#include "vision_logic.h"

#define REASON_MAX  ((uint8_t)CIS_REASON_DATA_INVALID)

void vision_init(vision_state_t *s)
{
    cis_value_init(&s->presence);
    cis_value_init(&s->count);
    s->pi_boot_id = 0;
    s->vision_sequence = 0;
    s->have_context = false;
    s->have_sequence = false;
    s->link_up = false;
    s->last_rx_ms = 0;
    s->pi_vision_status = PI_STATE_NOT_READY;
    s->pi_active_fault = 0;
    s->pi_last_fault = 0;
    s->status_sequence = 0;
    s->have_status = false;
    s->status_link_up = false;
    s->last_status_rx_ms = 0;
    s->rejected_frames = 0;
}

/* Modular comparison of 32-bit sequence numbers: new if the difference is 1..0x7FFFFFFF. */
static bool seq_is_new(uint32_t old_seq, uint32_t new_seq)
{
    uint32_t diff = new_seq - old_seq;
    return diff >= 1u && diff <= 0x7FFFFFFFu;
}

static void both_invalidate(vision_state_t *s, cis_reason_t reason)
{
    cis_value_invalidate(&s->presence, reason);
    cis_value_invalidate(&s->count, reason);
}

/* Accepts the Pi boot number. A new boot number discards the old judgement. */
static bool admit_boot(vision_state_t *s, uint32_t boot_id)
{
    if (boot_id == 0u) {                       /* 0 is reserved */
        s->rejected_frames++;
        return false;
    }
    if (!s->have_context || boot_id != s->pi_boot_id) {
        /* A new Pi process: the old judgement and the old fault belong to the process that
         * is gone. Start from "no data"; the first valid judgement of the new process is used. */
        bool had_context = s->have_context;
        cis_value_init(&s->presence);
        cis_value_init(&s->count);
        s->presence.value = (int32_t)PI_TRI_UNKNOWN;
        s->count.value = (int32_t)PI_TRI_UNKNOWN;
        if (had_context) {
            both_invalidate(s, CIS_REASON_NO_DATA);
        }
        s->pi_boot_id = boot_id;
        s->have_context = true;
        s->have_sequence = false;
        s->have_status = false;
    }
    return true;
}

/* Keeps the Pi's own sequence number and ages the value from the time of the judgement. */
static void record(cis_value_t *v, uint32_t pi_sequence, uint32_t now_ms, uint32_t age_ms)
{
    v->sequence = pi_sequence;
    v->sample_ms = now_ms - age_ms;
    v->has_sample = true;
}

static cis_reason_t reason_from_pi(uint8_t reason)
{
    if (reason == 0u || reason > REASON_MAX) {
        return CIS_REASON_DATA_INVALID;        /* invalid without a usable reason */
    }
    return (cis_reason_t)reason;
}

/* Is this field usable? If not, *why tells the reason to report. */
static bool field_usable(uint8_t value, uint8_t validity, uint8_t reason,
                         uint8_t max_value, cis_reason_t out_of_range, cis_reason_t *why)
{
    if (validity != PI_VALID) {
        *why = reason_from_pi(reason);         /* keep the Pi's original reason */
        return false;
    }
    if (value == PI_TRI_UNKNOWN) {
        *why = CIS_REASON_DATA_INVALID;        /* "valid" but unknown does not make sense */
        return false;
    }
    if (value > max_value) {
        *why = out_of_range;
        return false;
    }
    return true;
}

/* A usable value: new observation, unless the function was in FAULT. In that case this
 * judgement only starts the recovery and the next good one completes it. */
static void apply_good(cis_value_t *v, uint8_t value, uint32_t pi_seq, uint32_t now_ms, uint32_t age_ms)
{
    if (v->status == CIS_FUNC_FAULT) {
        cis_value_begin_recovery(v);
        return;
    }
    cis_value_new_sample(v, (int32_t)value, now_ms);
    record(v, pi_seq, now_ms, age_ms);
}

static void apply_bad(cis_value_t *v, uint8_t value, cis_reason_t why, uint32_t pi_seq,
                      uint32_t now_ms, uint32_t age_ms)
{
    v->value = (int32_t)value;
    cis_value_invalidate(v, why);
    record(v, pi_seq, now_ms, age_ms);
}

bool vision_on_result(vision_state_t *s, const pi_vision_result_t *f, uint32_t now_ms)
{
    uint32_t age = (uint32_t)f->vision_age_ms + VISION_UART_MARGIN_MS;
    cis_reason_t why_p = CIS_REASON_NONE;
    cis_reason_t why_c = CIS_REASON_NONE;
    bool ok_p;
    bool ok_c;

    if (!admit_boot(s, f->pi_boot_id)) {
        return false;
    }
    s->last_rx_ms = now_ms;
    s->link_up = true;

    if (s->have_sequence && !seq_is_new(s->vision_sequence, f->vision_sequence)) {
        return false;                                   /* repeat or older: no new observation */
    }
    s->have_sequence = true;
    s->vision_sequence = f->vision_sequence;
    s->pi_vision_status = f->vision_status;
    s->pi_active_fault = f->fault_code;

    if (f->fault_code != PI_FAULT_NONE || f->vision_status == PI_STATE_FAULT) {
        /* The vision function itself is faulted: both values carry the Pi's reasons. */
        cis_reason_t rp = (f->presence_validity == PI_VALID || f->presence_reason == 0u)
                              ? CIS_REASON_VISION_FAULT : reason_from_pi(f->presence_reason);
        cis_reason_t rc = (f->count_validity == PI_VALID || f->count_reason == 0u)
                              ? CIS_REASON_VISION_FAULT : reason_from_pi(f->count_reason);
        cis_value_fault(&s->presence, rp);
        cis_value_fault(&s->count, rc);
        record(&s->presence, f->vision_sequence, now_ms, age);
        record(&s->count, f->vision_sequence, now_ms, age);
        return true;
    }
    if (f->vision_status != PI_STATE_ACTIVE) {
        /* NOT_READY / READY / RECOVERING: no result that may be used yet. */
        cis_reason_t r = (f->vision_status == PI_STATE_RECOVERING) ? CIS_REASON_RECOVERING
                                                                   : CIS_REASON_NOT_READY;
        if (f->vision_status == PI_STATE_RECOVERING) {
            cis_value_begin_recovery(&s->presence);
            cis_value_begin_recovery(&s->count);
        }
        apply_bad(&s->presence, f->presence, r, f->vision_sequence, now_ms, age);
        apply_bad(&s->count, f->count, r, f->vision_sequence, now_ms, age);
        return true;
    }

    ok_p = field_usable(f->presence, f->presence_validity, f->presence_reason,
                        1u, CIS_REASON_DATA_INVALID, &why_p);
    ok_c = field_usable(f->count, f->count_validity, f->count_reason,
                        (uint8_t)VISION_MAX_COUNT, CIS_REASON_OUT_OF_RANGE, &why_c);

    /* Presence and count of one judgement must not contradict each other. */
    if (ok_p && ok_c &&
        ((f->presence == PI_TRI_ABSENT && f->count >= 1u) ||
         (f->presence == PI_TRI_PRESENT && f->count == 0u))) {
        ok_p = false;
        ok_c = false;
        why_p = CIS_REASON_DATA_INVALID;
        why_c = CIS_REASON_DATA_INVALID;
    }

    if (ok_p) {
        apply_good(&s->presence, f->presence, f->vision_sequence, now_ms, age);
    } else {
        apply_bad(&s->presence, f->presence, why_p, f->vision_sequence, now_ms, age);
    }
    if (ok_c) {
        apply_good(&s->count, f->count, f->vision_sequence, now_ms, age);
    } else {
        apply_bad(&s->count, f->count, why_c, f->vision_sequence, now_ms, age);
    }
    return true;
}

void vision_on_status(vision_state_t *s, const pi_vision_status_t *f, uint32_t now_ms)
{
    if (!admit_boot(s, f->pi_boot_id)) {
        return;
    }
    s->last_status_rx_ms = now_ms;
    s->status_link_up = true;

    if (s->have_status && !seq_is_new(s->status_sequence, f->status_sequence)) {
        return;                                         /* repeat or older state evaluation */
    }
    s->have_status = true;
    s->status_sequence = f->status_sequence;
    s->pi_vision_status = f->vision_status;
    s->pi_active_fault = f->active_fault;
    s->pi_last_fault = f->last_fault;

    /* READY / RECOVERING only change the function state: they never renew a judgement.
     * A fault takes the affected value(s) out of use. */
    if (f->vision_status == PI_STATE_FAULT || f->active_fault != PI_FAULT_NONE) {
        uint8_t aff = f->affected_function;
        if (aff == 0u) {
            aff = (uint8_t)(PI_AFFECTS_PRESENCE | PI_AFFECTS_COUNT);
        }
        if (aff & PI_AFFECTS_PRESENCE) {
            cis_value_fault(&s->presence, CIS_REASON_VISION_FAULT);
        }
        if (aff & PI_AFFECTS_COUNT) {
            cis_value_fault(&s->count, CIS_REASON_VISION_FAULT);
        }
    }
}

void vision_check(vision_state_t *s, uint32_t now_ms)
{
    if (s->link_up && (uint32_t)(now_ms - s->last_rx_ms) > VISION_RX_TIMEOUT_MS) {
        s->link_up = false;
        both_invalidate(s, CIS_REASON_COMMUNICATION_FAULT);   /* the last judgement is not current any more */
    }
    if (s->status_link_up && (uint32_t)(now_ms - s->last_status_rx_ms) > VISION_STATUS_TIMEOUT_MS) {
        s->status_link_up = false;
    }
    cis_value_check_stale(&s->presence, now_ms, VISION_STALE_MS);
    cis_value_check_stale(&s->count, now_ms, VISION_STALE_MS);
}

cis_func_status_t vision_function_status(const vision_state_t *s)
{
    if (s->presence.status == CIS_FUNC_FAULT || s->count.status == CIS_FUNC_FAULT) {
        return CIS_FUNC_FAULT;
    }
    if (s->presence.status == CIS_FUNC_RECOVERING || s->count.status == CIS_FUNC_RECOVERING) {
        return CIS_FUNC_RECOVERING;
    }
    if (s->presence.valid || s->count.valid) {
        return CIS_FUNC_ACTIVE;
    }
    return s->have_context ? CIS_FUNC_READY : CIS_FUNC_NOT_READY;
}
