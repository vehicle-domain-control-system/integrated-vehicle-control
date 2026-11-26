#ifndef DOMAIN_CAN0_TRANSCEIVER_H
#define DOMAIN_CAN0_TRANSCEIVER_H

#include <stdbool.h>

typedef enum
{
    DOMAIN_CAN0_TRANSCEIVER_MODE_UNKNOWN = 0,
    DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY,
    DOMAIN_CAN0_TRANSCEIVER_MODE_NORMAL
} DomainCan0Transceiver_Mode_t;

typedef enum
{
    DOMAIN_CAN0_TRANSCEIVER_NOT_INITIALIZED = 0,
    DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED
} DomainCan0Transceiver_Status_t;

/*
 * Precondition: Siul2_Port_Ip_Init() completed successfully and configured
 * PTG5/GPIO197 as an output. This function requests STB HIGH (Standby).
 * REQUEST_ACCEPTED reports only that the RTD write was issued; it does not
 * prove the physical pin voltage or the transceiver operating mode.
 */
DomainCan0Transceiver_Status_t DomainCan0Transceiver_InitAfterPort(void);

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestStandby(void);

DomainCan0Transceiver_Status_t DomainCan0Transceiver_RequestNormal(void);

bool DomainCan0Transceiver_IsControlAvailable(void);

/* Returns the last software-requested mode, not a physical pin readback. */
DomainCan0Transceiver_Mode_t DomainCan0Transceiver_GetLastRequestedMode(void);

#endif /* DOMAIN_CAN0_TRANSCEIVER_H */
