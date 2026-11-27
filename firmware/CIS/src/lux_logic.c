#include "lux_logic.h"

void lux_init(lux_state_t *s)
{
    cis_value_init(&s->illuminance);
    s->fail_count = 0;
}

uint32_t lux_from_raw(uint16_t raw)
{
    return ((uint32_t)raw * LUX_RES_X10000 + 5000u) / 10000u;      /* rounded */
}

void lux_on_raw(lux_state_t *s, uint16_t raw, uint32_t now_ms)
{
    s->fail_count = 0;
    if (s->illuminance.status == CIS_FUNC_FAULT) {
        cis_value_begin_recovery(&s->illuminance);                 /* next good reading completes it */
        return;
    }
    cis_value_new_sample(&s->illuminance, (int32_t)lux_from_raw(raw), now_ms);
    if (raw == LUX_RAW_SATURATED) {
        cis_value_invalidate(&s->illuminance, CIS_REASON_OUT_OF_RANGE);
    }
}

void lux_on_failure(lux_state_t *s, uint32_t now_ms)
{
    (void)now_ms;
    if (s->fail_count < 255u) {
        s->fail_count++;
    }
    if (s->fail_count >= LUX_FAIL_LIMIT || s->illuminance.status == CIS_FUNC_RECOVERING) {
        cis_value_fault(&s->illuminance, CIS_REASON_SENSOR_FAULT);
    }
    /* Otherwise the last value stays; lux_check_stale() ages it. */
}

void lux_check_stale(lux_state_t *s, uint32_t now_ms)
{
    cis_value_check_stale(&s->illuminance, now_ms, LUX_STALE_MS);
}
