#ifndef DOMAIN_APPLICATION_ENTRY_H
#define DOMAIN_APPLICATION_ENTRY_H

#include <stdbool.h>

#include "domain_board_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DOMAIN_APPLICATION_ENTRY_OK = 0,
    DOMAIN_APPLICATION_ENTRY_INVALID_ARGUMENT,
    DOMAIN_APPLICATION_ENTRY_NOT_INITIALIZED,
    DOMAIN_APPLICATION_ENTRY_ALREADY_INITIALIZED,
    DOMAIN_APPLICATION_ENTRY_BOARD_INIT_FAILED,
    DOMAIN_APPLICATION_ENTRY_POLL_FAILED,
    DOMAIN_APPLICATION_ENTRY_FAULT_BLOCKED
} DomainApplicationEntry_Status_t;

typedef struct
{
    bool initAttempted;
    bool ready;
    bool faultBlocked;
    DomainApplicationEntry_Status_t lastStatus;
    DomainBoardRuntime_Status_t lastBoardStatus;
} DomainApplicationEntry_Snapshot_t;

/*
 * Single, static Domain Application owner for the board runtime.
 *
 * Preconditions:
 * - the sole board/bootstrap owner completed Clock_Ip_Init successfully;
 * - Network ingress remains closed and RxObserver/TxPort are unregistered;
 * - this API is called only by the serialized, non-ISR superloop owner.
 *
 * This module does not initialize Clock_Ip, a watchdog, Network, an RTOS, or
 * any CAN/UART driver. A failed initialization or Poll is sticky and keeps the
 * Application fail-closed for the rest of this boot.
 */
DomainApplicationEntry_Status_t DomainApplicationEntry_Init(
    const DomainBoardRuntime_Config_t *config);

/* Bounded superloop step. It never opens Network ingress or transmits data. */
DomainApplicationEntry_Status_t DomainApplicationEntry_Poll(void);

bool DomainApplicationEntry_IsReady(void);

/* Read-only value copy; failure leaves outSnapshot unchanged. */
DomainApplicationEntry_Status_t DomainApplicationEntry_GetSnapshot(
    DomainApplicationEntry_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_APPLICATION_ENTRY_H */
