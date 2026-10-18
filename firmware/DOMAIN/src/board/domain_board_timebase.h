#ifndef DOMAIN_BOARD_TIMEBASE_H
#define DOMAIN_BOARD_TIMEBASE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform binding supplied by the board integration layer.
 *
 * The reader returns a validated, free-running uint32_t hardware tick count.
 * It must be non-blocking, callable from the single non-ISR Domain owner, and
 * leave outTicks unchanged when it returns false. Normal uint32_t rollover is
 * supported. The time between successful reads must be shorter than one full
 * hardware-counter period so that more than one wrap cannot be ambiguous.
 */
typedef bool (*DomainBoardTimebase_TickReader_t)(
    void *context,
    uint32_t *outTicks);

typedef enum
{
    DOMAIN_BOARD_TIMEBASE_OK = 0,
    DOMAIN_BOARD_TIMEBASE_INVALID_ARGUMENT,
    DOMAIN_BOARD_TIMEBASE_TICK_UNAVAILABLE,
    DOMAIN_BOARD_TIMEBASE_ALREADY_INITIALIZED
} DomainBoardTimebase_Status_t;

typedef struct
{
    DomainBoardTimebase_TickReader_t tickReader;
    void *tickReaderContext;
    uint32_t ticksPerMillisecond;
} DomainBoardTimebase_Config_t;

typedef struct
{
    DomainBoardTimebase_TickReader_t tickReader;
    void *tickReaderContext;
    uint32_t ticksPerMillisecond;
    uint32_t lastTick;
    uint32_t fractionalTicks;
    uint32_t nowMs;
    bool initialized;
} DomainBoardTimebase_t;

/*
 * The caller must zero-initialize this context before its first use. Init takes
 * one successful counter sample and sets the local epoch to zero milliseconds.
 * Reinitializing a live context is rejected to preserve monotonic time. Failed
 * or duplicate initialization leaves the supplied context unchanged.
 *
 * This pure adapter does not configure SysTick, PIT, STM, GPT, an interrupt,
 * Clock_Ip, OsIf, or a watchdog. Selection and initialization of the actual
 * S32K344 counter remain a board-integration prerequisite.
 */
DomainBoardTimebase_Status_t DomainBoardTimebase_Init(
    DomainBoardTimebase_t *timebase,
    const DomainBoardTimebase_Config_t *config);

bool DomainBoardTimebase_IsInitialized(
    const DomainBoardTimebase_t *timebase);

/*
 * DomainRuntime_TimeProvider_t-compatible entry point.
 *
 * A successful read converts elapsed ticks to milliseconds without refreshing
 * the epoch when no full millisecond elapsed. Counter and millisecond rollover
 * use standard uint32_t modulo arithmetic. Failure changes neither the context
 * nor outNowMs.
 */
bool DomainBoardTimebase_GetNowMs(
    void *context,
    uint32_t *outNowMs);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_BOARD_TIMEBASE_H */
