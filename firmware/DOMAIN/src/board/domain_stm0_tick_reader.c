#include "domain_stm0_tick_reader.h"

#include "Stm_Ip.h"
#include "Stm_Ip_SA_PBcfg.h"

#include <stddef.h>

#define DOMAIN_STM0_INSTANCE (0U)
#define DOMAIN_MILLISECONDS_PER_SECOND (1000U)

DomainStm0TickReader_Status_t DomainStm0TickReader_Init(
    DomainStm0TickReader_t *reader,
    uint32_t stm0InputClockHz)
{
    DomainStm0TickReader_t initializedReader;
    uint32_t prescalerDivisor;
    uint32_t counterFrequencyHz;

    if (reader == NULL)
    {
        return DOMAIN_STM0_TICK_READER_INVALID_ARGUMENT;
    }
    if (DomainStm0TickReader_IsInitialized(reader))
    {
        return DOMAIN_STM0_TICK_READER_ALREADY_INITIALIZED;
    }
    if (stm0InputClockHz != DOMAIN_STM0_INPUT_CLOCK_HZ)
    {
        return DOMAIN_STM0_TICK_READER_CLOCK_MISMATCH;
    }

    prescalerDivisor = (uint32_t)STM_0_InitConfig_PB.clockPrescaler + 1U;
    if ((prescalerDivisor == 0U) ||
        ((stm0InputClockHz % prescalerDivisor) != 0U))
    {
        return DOMAIN_STM0_TICK_READER_UNSUPPORTED_FREQUENCY;
    }

    counterFrequencyHz = stm0InputClockHz / prescalerDivisor;
    if ((counterFrequencyHz == 0U) ||
        ((counterFrequencyHz % DOMAIN_MILLISECONDS_PER_SECOND) != 0U))
    {
        return DOMAIN_STM0_TICK_READER_UNSUPPORTED_FREQUENCY;
    }

    initializedReader.counterFrequencyHz = counterFrequencyHz;
    initializedReader.ticksPerMillisecond =
        counterFrequencyHz / DOMAIN_MILLISECONDS_PER_SECOND;
    initializedReader.initialized = true;

    Stm_Ip_Init(DOMAIN_STM0_INSTANCE, &STM_0_InitConfig_PB);
    Stm_Ip_InitChannel(DOMAIN_STM0_INSTANCE, &STM_0_ChannelConfig_PB[0]);
    Stm_Ip_StartTimer(DOMAIN_STM0_INSTANCE, 0U);

    *reader = initializedReader;
    return DOMAIN_STM0_TICK_READER_OK;
}

bool DomainStm0TickReader_IsInitialized(
    const DomainStm0TickReader_t *reader)
{
    return (reader != NULL) && reader->initialized &&
           (reader->counterFrequencyHz != 0U) &&
           (reader->ticksPerMillisecond != 0U);
}

bool DomainStm0TickReader_Read(
    void *context,
    uint32_t *outTicks)
{
    DomainStm0TickReader_t *reader = (DomainStm0TickReader_t *)context;
    uint32_t counterValue;

    if ((outTicks == NULL) || !DomainStm0TickReader_IsInitialized(reader))
    {
        return false;
    }

    counterValue = (uint32_t)Stm_Ip_GetCounterValue(DOMAIN_STM0_INSTANCE);
    *outTicks = counterValue;
    return true;
}

bool DomainStm0TickReader_GetTicksPerMillisecond(
    const DomainStm0TickReader_t *reader,
    uint32_t *outTicksPerMillisecond)
{
    if ((outTicksPerMillisecond == NULL) ||
        !DomainStm0TickReader_IsInitialized(reader))
    {
        return false;
    }

    *outTicksPerMillisecond = reader->ticksPerMillisecond;
    return true;
}
