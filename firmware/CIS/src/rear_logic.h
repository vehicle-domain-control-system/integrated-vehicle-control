/*
 * Rear distance classification. No hardware access: it turns "echo pulse
 * width" or "no echo" into a CIS value, a PROXIMITY_STATUS and a QUALITY_REASON.
 *
 * Rules (CIS-SYS-FUN-026..028, SAF-002):
 *  - a failed measurement is never reported as NO_OBJECT (= safe)
 *  - NO_OBJECT only when the sensor answered and nothing is in range
 *  - after repeated failures the function goes to FAULT; it comes back only
 *    through RECOVERING and a new valid measurement
 *
 * Thresholds are candidates and are to be fixed after bench measurement.
 */
#ifndef REAR_LOGIC_H
#define REAR_LOGIC_H

#include "cis_value.h"

#define REAR_MIN_VALID_MM       100u     /* 10 cm (candidate) */
#define REAR_MAX_VALID_MM       1000u    /* 100 cm (candidate) */
#define REAR_NO_OBJECT_US       36000u   /* HC-SR04 echo with nothing in front is ~38 ms */
#define REAR_FAIL_LIMIT         3u       /* consecutive failures -> FAULT (candidate) */
#define REAR_NO_VALUE           65535u   /* value field when there is no distance */

typedef struct {
    cis_value_t     value;      /* distance in mm (0.1 cm), REAR_NO_VALUE if none */
    cis_proximity_t status;
    uint8_t         fail_count;
} rear_state_t;

void rear_init(rear_state_t *s);

/* An echo pulse was measured. width_us is the HIGH time of the ECHO pin. */
void rear_on_echo(rear_state_t *s, uint32_t width_us, uint32_t now_ms);

/* No (complete) echo was received. */
void rear_on_no_echo(rear_state_t *s, uint32_t now_ms);

/* Echo pulse width -> millimetres (sound: 58 us per cm for the round trip). */
uint32_t rear_width_to_mm(uint32_t width_us);

#endif /* REAR_LOGIC_H */
