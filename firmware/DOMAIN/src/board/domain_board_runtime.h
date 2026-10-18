#ifndef DOMAIN_BOARD_RUNTIME_H
#define DOMAIN_BOARD_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "domain_board_timebase.h"
#include "domain_runtime.h"
#include "domain_stm0_tick_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DOMAIN_BOARD_RUNTIME_OK = 0,
    DOMAIN_BOARD_RUNTIME_INVALID_ARGUMENT,
    DOMAIN_BOARD_RUNTIME_NOT_INITIALIZED,
    DOMAIN_BOARD_RUNTIME_ALREADY_INITIALIZED,
    DOMAIN_BOARD_RUNTIME_STM0_INIT_FAILED,
    DOMAIN_BOARD_RUNTIME_TIMEBASE_INIT_FAILED,
    DOMAIN_BOARD_RUNTIME_DOMAIN_INIT_FAILED,
    DOMAIN_BOARD_RUNTIME_RUNTIME_POLL_FAILED,
    DOMAIN_BOARD_RUNTIME_FAULT_BLOCKED
} DomainBoardRuntime_Status_t;

typedef enum
{
    DOMAIN_BOARD_RUNTIME_STAGE_NONE = 0,
    DOMAIN_BOARD_RUNTIME_STAGE_CONFIG_VALIDATED,
    DOMAIN_BOARD_RUNTIME_STAGE_STM0_READY,
    DOMAIN_BOARD_RUNTIME_STAGE_TIMEBASE_READY,
    DOMAIN_BOARD_RUNTIME_STAGE_DOMAIN_READY,
    DOMAIN_BOARD_RUNTIME_STAGE_FAULT_BLOCKED
} DomainBoardRuntime_Stage_t;

typedef struct
{
    uint32_t stm0InputClockHz;
    DomainRuntime_Config_t runtimeConfig;
} DomainBoardRuntime_Config_t;

typedef struct
{
    DomainBoardRuntime_Stage_t stage;
    DomainBoardRuntime_Status_t lastStatus;
    DomainRuntime_Status_t lastRuntimeStatus;
    bool initAttempted;
    bool ready;
    bool faultBlocked;
} DomainBoardRuntime_Snapshot_t;

/*
 * Fixed board-level owner for STM0, the millisecond adapter, and DomainRuntime.
 * The object must have static storage duration or be zero-initialized before
 * first use, and must remain at a stable address for the complete runtime.
 */
typedef struct
{
    uint32_t integrityMarker;
    DomainStm0TickReader_t stm0Reader;
    DomainBoardTimebase_t timebase;
    DomainRuntime_t runtime;
    DomainBoardRuntime_Stage_t stage;
    DomainBoardRuntime_Status_t lastStatus;
    DomainRuntime_Status_t lastRuntimeStatus;
    bool initAttempted;
    bool ready;
    bool faultBlocked;
} DomainBoardRuntime_t;

/*
 * Caller preconditions:
 * - Clock_Ip_Init(&Clock_Ip_aClockConfig[0]) completed successfully exactly
 *   once under the agreed Board/Network clock owner.
 * - STM0 is admitted for Domain use and its input clock equals
 *   config->stm0InputClockHz.
 * - Network ingress and TxPort/RxObserver registration remain closed.
 * - runtimeConfig.timeProvider and timeProviderContext are NULL; this adapter
 *   installs DomainBoardTimebase_GetNowMs and its owned timebase context.
 *
 * This function performs STM0 reader -> BoardTimebase -> DomainRuntime setup.
 * It does not initialize Clock_Ip, a watchdog, Network, an ISR, or an RTOS.
 * Once a hardware-affecting initialization attempt fails, the context becomes
 * fault-blocked and cannot be retried implicitly.
 */
DomainBoardRuntime_Status_t DomainBoardRuntime_Init(
    DomainBoardRuntime_t *boardRuntime,
    const DomainBoardRuntime_Config_t *config);

/* Non-blocking single-owner Superloop step. Never call from an ISR. */
DomainBoardRuntime_Status_t DomainBoardRuntime_Poll(
    DomainBoardRuntime_t *boardRuntime);

bool DomainBoardRuntime_IsReady(
    const DomainBoardRuntime_t *boardRuntime);

/* Read-only value copy; failure leaves outSnapshot unchanged. */
DomainBoardRuntime_Status_t DomainBoardRuntime_GetSnapshot(
    const DomainBoardRuntime_t *boardRuntime,
    DomainBoardRuntime_Snapshot_t *outSnapshot);

/* Ready-only access for the same serialized owner context. */
DomainRuntime_t *DomainBoardRuntime_GetDomainRuntime(
    DomainBoardRuntime_t *boardRuntime);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_BOARD_RUNTIME_H */
