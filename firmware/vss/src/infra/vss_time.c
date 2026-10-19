#include <stddef.h>

#include "vss_time.h"

static VssTimeTickSource s_readTicks;
static uint32_t s_lastTicks;
static uint32_t s_tickWraps;
static uint32_t s_unplacedTicks;

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

/* Ticks since tick 0, extended to 64 bits. Only with a registered source. */
static uint64_t now_ticks(void)
{
    uint32_t ticks = s_readTicks();

    if (ticks < s_lastTicks)
    {
        ++s_tickWraps;
    }
    s_lastTicks = ticks;
    return ((uint64_t)s_tickWraps << 32) | (uint64_t)ticks;
}

uint64_t VssTime_NowMs(void)
{
    if (s_readTicks == NULL)
    {
        return 0u;
    }
    return VssTime_TicksToMs(now_ticks());
}

uint32_t VssTime_NowMs32(void)
{
    return (uint32_t)VssTime_NowMs();
}

uint32_t VssTime_TickToMs32(uint32_t tick)
{
    uint64_t now;
    uint32_t back;

    if (s_readTicks == NULL)
    {
        if (s_unplacedTicks != UINT32_MAX)
        {
            ++s_unplacedTicks;
        }
        return 0u;
    }
    now = now_ticks();
    back = (uint32_t)now - tick;
    if ((back > 0x7FFFFFFFu) || ((uint64_t)back > now))
    {
        if (s_unplacedTicks != UINT32_MAX)
        {
            ++s_unplacedTicks;
        }
        return (uint32_t)VssTime_TicksToMs(now);
    }
    return (uint32_t)VssTime_TicksToMs(now - back);
}

uint32_t VssTime_UnplacedTicks(void)
{
    return s_unplacedTicks;
}

#if VSS_CFG_HOST_TEST
void VssTime_ResetForTest(void)
{
    s_readTicks = NULL;
    s_lastTicks = 0u;
    s_tickWraps = 0u;
    s_unplacedTicks = 0u;
}
#endif
