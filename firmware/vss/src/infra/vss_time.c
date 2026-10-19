#include <stddef.h>

#include "vss_time.h"

static VssTimeTickSource s_readTicks;
static uint32_t s_lastTicks;
static uint32_t s_tickWraps;

uint64_t VssTime_TicksToMs(uint64_t ticks)
{
    return (ticks * 1000u) / VSS_TIME_TICK_HZ;
}

void VssTime_Init(VssTimeTickSource readTicks)
{
    s_readTicks = readTicks;
    s_lastTicks = (readTicks != NULL) ? readTicks() : 0u;
    s_tickWraps = 0u;
}

uint64_t VssTime_NowMs(void)
{
    uint32_t ticks;

    if (s_readTicks == NULL)
    {
        return 0u;
    }
    ticks = s_readTicks();
    if (ticks < s_lastTicks)
    {
        ++s_tickWraps;
    }
    s_lastTicks = ticks;
    return VssTime_TicksToMs(((uint64_t)s_tickWraps << 32) | (uint64_t)ticks);
}

uint32_t VssTime_NowMs32(void)
{
    return (uint32_t)VssTime_NowMs();
}
