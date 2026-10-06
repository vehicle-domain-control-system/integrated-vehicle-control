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

void rear_on_echo(rear_state_t *s, uint32_t width_us, uint32_t now_ms)
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

void rear_on_no_echo(rear_state_t *s, uint32_t now_ms)
{
    (void)now_ms;
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
