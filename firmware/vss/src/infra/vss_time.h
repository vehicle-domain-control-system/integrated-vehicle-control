#ifndef VSS_TIME_H
#define VSS_TIME_H

#include <stdint.h>

/* Rate of the board timer tick that stamps output boundaries and drives the
 * system clock (1 tick = 1.25 ms). */
#define VSS_TIME_TICK_HZ 800u

/* Milliseconds covered by a tick count, rounded down. */
uint64_t VssTime_TicksToMs(uint64_t ticks);

/* Reads the free-running board tick counter (VSS_TIME_TICK_HZ, wraps at
 * 2^32). The application registers it once at boot; infra cannot reach the
 * board layer itself. */
typedef uint32_t (*VssTimeTickSource)(void);

/* Registers the tick source and restarts the 64-bit extension. NULL leaves
 * the clock at 0. */
void VssTime_Init(VssTimeTickSource readTicks);

/* Milliseconds since tick 0, extended to 64 bits (rounded down). Reads the
 * tick source; it must be called at least once per counter wrap (62 days),
 * which the super-loop does many times per second. 0 before VssTime_Init. */
uint64_t VssTime_NowMs(void);

/* Low 32 bits of VssTime_NowMs. NetCanRxMeta.received_at_ms is stamped with
 * this clock. */
uint32_t VssTime_NowMs32(void);

#endif /* VSS_TIME_H */
