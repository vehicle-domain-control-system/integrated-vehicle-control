#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdbool.h>
#include <stdint.h>

/* Free-running tick that also drives the codec reference-clock pin.
 * One tick is 1.25 ms. */
#define HAL_TIMER_TICK_HZ 800u

void     HalTimer_Init(void);
uint32_t HalTimer_GetTicks(void);

/* Busy-wait for the given number of ticks. Boot sequence only. */
void     HalTimer_WaitTicks(uint32_t ticks);

#endif /* HAL_TIMER_H */
