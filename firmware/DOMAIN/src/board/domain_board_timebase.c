#include "domain_board_timebase.h"

#include <stddef.h>

DomainBoardTimebase_Status_t DomainBoardTimebase_Init(
    DomainBoardTimebase_t *timebase,
    const DomainBoardTimebase_Config_t *config)
{
    DomainBoardTimebase_t initializedTimebase;
    uint32_t initialTick;

    if ((timebase == NULL) || (config == NULL))
    {
        return DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT;
    }
    if (DomainBoardTimebase_IsInitialized(timebase))
    {
        return DOMAIN_BOARD_TIMEBASE_ALREADY_INITIALIZED;
    }
    if ((config->tickReader == NULL) ||
        (config->ticksPerMillisecond == 0U))
    {
        return DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT;
    }
    if (!config->tickReader(config->tickReaderContext, &initialTick))
    {
        return DOMAIN_BOARD_TIMEBASE_TICK_UNAVAILABLE;
    }

    initializedTimebase.tickReader = config->tickReader;
    initializedTimebase.tickReaderContext = config->tickReaderContext;
    initializedTimebase.ticksPerMillisecond = config->ticksPerMillisecond;
    initializedTimebase.lastTick = initialTick;
    initializedTimebase.fractionalTicks = 0U;
    initializedTimebase.nowMs = 0U;
    initializedTimebase.initialized = true;
    *timebase = initializedTimebase;
    return DOMAIN_BOARD_TIMEBASE_OK;
}

bool DomainBoardTimebase_IsInitialized(
    const DomainBoardTimebase_t *timebase)
{
    return (timebase != NULL) && timebase->initialized &&
           (timebase->tickReader != NULL) &&
           (timebase->ticksPerMillisecond != 0U);
}

bool DomainBoardTimebase_GetNowMs(
    void *context,
    uint32_t *outNowMs)
{
    DomainBoardTimebase_t *timebase = (DomainBoardTimebase_t *)context;
    uint32_t currentTick;
    uint32_t elapsedTicks;
    uint32_t elapsedMs;
    uint32_t remainderTicks;
    uint32_t ticksUntilNextMs;

    if ((outNowMs == NULL) || !DomainBoardTimebase_IsInitialized(timebase))
    {
        return false;
    }
    if (!timebase->tickReader(
            timebase->tickReaderContext,
            &currentTick))
    {
        return false;
    }

    elapsedTicks = (uint32_t)(currentTick - timebase->lastTick);
    elapsedMs = elapsedTicks / timebase->ticksPerMillisecond;
    remainderTicks = elapsedTicks % timebase->ticksPerMillisecond;
    ticksUntilNextMs = timebase->ticksPerMillisecond -
                       timebase->fractionalTicks;

    if (remainderTicks >= ticksUntilNextMs)
    {
        elapsedMs += 1U;
        remainderTicks -= ticksUntilNextMs;
    }
    else
    {
        remainderTicks += timebase->fractionalTicks;
    }

    timebase->lastTick = currentTick;
    timebase->fractionalTicks = remainderTicks;
    timebase->nowMs += elapsedMs;
    *outNowMs = timebase->nowMs;
    return true;
}
