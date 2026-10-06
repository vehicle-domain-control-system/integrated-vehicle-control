/*
 * HC-SR04 ultrasonic sensor on the S32K144EVB.
 *   TRIG : PTE7 (GPIO output)
 *   ECHO : PTE8 (FTM0_CH6 input capture, 5 V signal, EVB VDD is 5 V)
 * Needs timebase_init() first (FTM0 runs at 1 MHz).
 */
#ifndef HCSR04_H
#define HCSR04_H

#include "rear_logic.h"

#define HCSR04_PERIOD_MS        80u      /* time between triggers (>= 60 ms) */
#define HCSR04_RISE_TIMEOUT_US  10000u   /* ECHO must start within this time */
#define HCSR04_FALL_TIMEOUT_US  45000u   /* and end within this time (max ~38 ms) */

void hcsr04_init(void);

/* True while no measurement is in progress (safe moment for a long blocking job). */
bool hcsr04_idle(void);

/* Call often from the main loop. Never blocks (except the 10 us trigger pulse).
 * Returns true when a measurement finished and `s` was updated. */
bool hcsr04_step(rear_state_t *s, uint32_t now_ms);

#endif /* HCSR04_H */
