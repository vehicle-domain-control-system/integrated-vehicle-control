#ifndef VSS_TIME_H
#define VSS_TIME_H

#include <stdint.h>

/* Rate of the board timer tick that stamps output boundaries and drives the
 * system clock (1 tick = 1.25 ms). */
#define VSS_TIME_TICK_HZ 800u

/* Milliseconds covered by a tick count, rounded down. */
uint64_t VssTime_TicksToMs(uint64_t ticks);

#endif /* VSS_TIME_H */
