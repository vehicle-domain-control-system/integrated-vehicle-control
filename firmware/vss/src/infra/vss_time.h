#ifndef VSS_TIME_H
#define VSS_TIME_H

#include <stdbool.h>
#include <stdint.h>

#include "vss_config.h"

/* Rate of the board timer tick that stamps output boundaries and drives the
 * system clock (1 tick = 1.25 ms). */
#define VSS_TIME_TICK_HZ 800u

/* Milliseconds covered by a tick count, rounded down. */
uint64_t VssTime_TicksToMs(uint64_t ticks);

/* Reads the free-running board tick counter (VSS_TIME_TICK_HZ, wraps at
 * 2^32). The application registers it once at boot; infra cannot reach the
 * board layer itself. */
typedef uint32_t (*VssTimeTickSource)(void);

/* The clock below keeps its 64-bit extension in plain variables updated by
 * the readers: every function here runs in the super-loop owner context
 * only, never from an ISR. An ISR that needs a receive time records the raw
 * tick (or a flag) and the owner context converts it.
 * NetCanRxMeta.received_at_ms is therefore stamped with VssTime_NowMs32 in
 * the owner context, never computed from the tick by another module. */

/* Registers the tick source, once at boot. Returns false and changes nothing
 * for NULL or when a source is already registered (a restart of the 64-bit
 * extension would move the clock backwards). */
bool VssTime_Init(VssTimeTickSource readTicks);

/* True once a tick source is registered. */
bool VssTime_IsRegistered(void);

/* Milliseconds since tick 0, extended to 64 bits (rounded down). Reads the
 * tick source; it must be called at least once per counter wrap (62 days),
 * which the super-loop does many times per second. 0 before VssTime_Init. */
uint64_t VssTime_NowMs(void);

/* Low 32 bits of VssTime_NowMs. */
uint32_t VssTime_NowMs32(void);

#if VSS_CFG_HOST_TEST
/* Forgets the registered source so a test can register another. */
void VssTime_ResetForTest(void);
#endif

#endif /* VSS_TIME_H */
