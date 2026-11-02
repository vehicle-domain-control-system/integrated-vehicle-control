/*
 * Time base on FTM0's free-running counter.
 * Clock: 8 MHz SOSC / 8 = 1 MHz, so one counter tick is exactly 1 us.
 * timebase_now_us() must be called at least once per 65 ms (the 16-bit
 * counter period); the main loop does this.
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>
#include <stdbool.h>

/* Starts SOSC and FTM0. Returns false if the oscillator did not start. */
bool timebase_init(void);

uint16_t timebase_cnt(void);      /* raw 16-bit counter, 1 us per tick */
uint32_t timebase_now_us(void);   /* 32-bit microseconds (wraps after ~71 min) */
uint32_t timebase_now_ms(void);   /* milliseconds, monotonic */

#endif /* TIMEBASE_H */
