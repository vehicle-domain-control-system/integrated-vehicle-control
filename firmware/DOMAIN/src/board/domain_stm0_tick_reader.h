#ifndef DOMAIN_STM0_TICK_READER_H
#define DOMAIN_STM0_TICK_READER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DOMAIN_STM0_INPUT_CLOCK_HZ (48000000UL)

typedef enum
{
    DOMAIN_STM0_TICK_READER_OK = 0,
    DOMAIN_STM0_TICK_READER_INVALID_ARGUMENT,
    DOMAIN_STM0_TICK_READER_CLOCK_MISMATCH,
    DOMAIN_STM0_TICK_READER_UNSUPPORTED_FREQUENCY,
    DOMAIN_STM0_TICK_READER_ALREADY_INITIALIZED
} DomainStm0TickReader_Status_t;

typedef struct
{
    uint32_t counterFrequencyHz;
    uint32_t ticksPerMillisecond;
    bool initialized;
} DomainStm0TickReader_t;

/*
 * Board-private STM0 initialization.
 *
 * The caller must have completed Clock_Ip_Init successfully and must pass the
 * admitted STM0 input clock frequency. The current generated Clock setting is
 * 48 MHz. CLOCK_IP_GET_FREQUENCY_API is disabled, so this module cannot prove
 * that the hardware clock was applied and rejects any other supplied value.
 * The context must be zero-initialized before its first Init call.
 *
 * RTD Stm_Ip_Init/StartTimer return void. OK therefore means that all software
 * preconditions were validated and the RTD calls were issued; it is not proof
 * that the physical counter is advancing. Board bring-up must verify that.
 * At the current 48 MHz counter rate, the uint32_t counter wraps after about
 * 89.48 seconds. DomainBoardTimebase handles one modulo wrap, but successful
 * reads must be less than one complete counter period apart.
 */
DomainStm0TickReader_Status_t DomainStm0TickReader_Init(
    DomainStm0TickReader_t *reader,
    uint32_t stm0InputClockHz);

bool DomainStm0TickReader_IsInitialized(
    const DomainStm0TickReader_t *reader);

/* DomainBoardTimebase_TickReader_t-compatible callback. */
bool DomainStm0TickReader_Read(
    void *context,
    uint32_t *outTicks);

bool DomainStm0TickReader_GetTicksPerMillisecond(
    const DomainStm0TickReader_t *reader,
    uint32_t *outTicksPerMillisecond);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_STM0_TICK_READER_H */
