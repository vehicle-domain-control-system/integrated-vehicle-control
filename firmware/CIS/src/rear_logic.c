#include "rear_logic.h"

void rear_init(rear_state_t *s)
{
    cis_value_init(&s->value);
    s->status = CIS_PROX_UNAVAILABLE;
    s->fail_count = 0;
}

uint32_t rear_width_to_mm(uint32_t width_us)
{
    return (width_us * 10u + 29u) / 58u;      /* rounded */
}

/* The sensor answered. Records a new observation, unless the function was
 * in FAULT: then only recovery starts and the next answer completes it. */
static bool accept_answer(rear_state_t *s, int32_t value, uint32_t now_ms)
{
    s->fail_count = 0;
    if (s->value.status == CIS_FUNC_FAULT) {
        cis_value_begin_recovery(&s->value);
        s->status = CIS_PROX_RECOVERING;
        return false;
    }
    cis_value_new_sample(&s->value, value, now_ms);
    return true;
}

uint32_t rear_report_distance(const rear_state_t *s)
{
    return (s->status == CIS_PROX_VALID_DISTANCE) ? (uint32_t)s->value.value : REAR_NO_VALUE;
}

/* The reported distance or measurement state changed without a new echo (no answer, fault, recovery
 * step): this is a new observation for the receivers, which refuse a changed distance or state under
 * an old observation number. It gets a new sequence number and the age counts from now. A quality
 * change alone (same distance and state) keeps the sequence. */
static void new_observation_if_changed(rear_state_t *s, uint32_t now_ms, uint32_t seq_before,
                                       cis_proximity_t status_before, uint32_t distance_before)
{
    if (s->value.sequence == seq_before &&
        (s->status != status_before || rear_report_distance(s) != distance_before)) {
        s->value.sequence++;
        s->value.sample_ms = now_ms;
        s->value.has_sample = true;
    }
}

static void rear_on_echo_inner(rear_state_t *s, uint32_t width_us, uint32_t now_ms)
{
    uint32_t mm = rear_width_to_mm(width_us);

    if (width_us >= REAR_NO_OBJECT_US || mm > REAR_MAX_VALID_MM) {
        /* The sensor answered and nothing is inside the valid range. */
        if (accept_answer(s, (int32_t)REAR_NO_VALUE, now_ms)) {
            s->status = CIS_PROX_NO_OBJECT;
        }
    } else if (mm < REAR_MIN_VALID_MM) {
        /* Too close to measure: not usable, and not "no object". */
        if (accept_answer(s, (int32_t)REAR_NO_VALUE, now_ms)) {
            cis_value_invalidate(&s->value, CIS_REASON_OUT_OF_RANGE);
            s->status = CIS_PROX_UNAVAILABLE;
        }
    } else {
        if (accept_answer(s, (int32_t)mm, now_ms)) {
            s->status = CIS_PROX_VALID_DISTANCE;
        }
    }
}

void rear_on_echo(rear_state_t *s, uint32_t width_us, uint32_t now_ms)
{
    uint32_t seq = s->value.sequence;
    cis_proximity_t status = s->status;
    uint32_t distance = rear_report_distance(s);

    rear_on_echo_inner(s, width_us, now_ms);
    new_observation_if_changed(s, now_ms, seq, status, distance);
}

static void rear_on_no_echo_inner(rear_state_t *s)
{
    if (s->fail_count < 255u) {
        s->fail_count++;
    }
    if (s->fail_count >= REAR_FAIL_LIMIT || s->value.status == CIS_FUNC_RECOVERING) {
        cis_value_fault(&s->value, CIS_REASON_SENSOR_FAULT);
        s->status = CIS_PROX_FAULT;
    } else {
        cis_value_invalidate(&s->value, CIS_REASON_NO_DATA);
        s->status = CIS_PROX_UNAVAILABLE;
    }
}

void rear_on_no_echo(rear_state_t *s, uint32_t now_ms)
{
    uint32_t seq = s->value.sequence;
    cis_proximity_t status = s->status;
    uint32_t distance = rear_report_distance(s);

    rear_on_no_echo_inner(s);
    new_observation_if_changed(s, now_ms, seq, status, distance);
}
