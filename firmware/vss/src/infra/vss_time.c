#include "vss_time.h"

uint64_t VssTime_TicksToMs(uint64_t ticks)
{
    return (ticks * 1000u) / VSS_TIME_TICK_HZ;
}
