/*
 * Illuminance from the VEML7700. No hardware access.
 *
 * Sensor setting: gain 1/8, integration time 100 ms -> 0.4608 lx per count,
 * 65535 counts = 30199 lx. A saturated reading (65535) is reported as
 * INVALID/OUT_OF_RANGE, never as a number.
 *
 * Rules: the last reading is kept and aged between readings (SEQUENCE does not
 * change), STALE after LUX_STALE_MS, FAULT after LUX_FAIL_LIMIT consecutive
 * failed readings, back to normal only through RECOVERING and a new valid reading.
 */
#ifndef LUX_LOGIC_H
#define LUX_LOGIC_H

#include "cis_value.h"

#define LUX_RES_X10000      4608u      /* lx per count x 10000 (gain 1/8, IT 100 ms) */
#define LUX_RAW_SATURATED   0xFFFFu
#define LUX_STALE_MS        1000u      /* candidate */
#define LUX_FAIL_LIMIT      3u         /* consecutive failures -> FAULT (candidate) */

typedef struct {
    cis_value_t illuminance;           /* lx */
    uint8_t     fail_count;
} lux_state_t;

void lux_init(lux_state_t *s);
uint32_t lux_from_raw(uint16_t raw);
void lux_on_raw(lux_state_t *s, uint16_t raw, uint32_t now_ms);
void lux_on_failure(lux_state_t *s, uint32_t now_ms);
void lux_check_stale(lux_state_t *s, uint32_t now_ms);

#endif /* LUX_LOGIC_H */
