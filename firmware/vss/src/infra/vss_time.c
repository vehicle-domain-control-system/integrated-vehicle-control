#include <stddef.h>

#include "vss_time.h"

static VssTimeTickSource s_readTicks;
static uint32_t s_lastTicks;
static uint32_t s_tickWraps;

uint64_t VssTime_TicksToMs(uint64_t ticks)
{
    return (ticks * 1000u) / VSS_TIME_TICK_HZ;
}

bool VssTime_Init(VssTimeTickSource readTicks)
{
    if ((readTicks == NULL) || (s_readTicks != NULL))
    {
        return false;
    }
    s_readTicks = readTicks;
    s_lastTicks = readTicks();
    s_tickWraps = 0u;
    return true;
}

bool VssTime_IsRegistered(void)
{
    return s_readTicks != NULL;
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

#if VSS_CFG_HOST_TEST
void VssTime_ResetForTest(void)
{
    s_readTicks = NULL;
    s_lastTicks = 0u;
    s_tickWraps = 0u;
}
#endif
