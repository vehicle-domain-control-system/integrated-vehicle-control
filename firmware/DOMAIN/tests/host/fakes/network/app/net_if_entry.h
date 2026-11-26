#ifndef NETWORK_APP_NET_IF_ENTRY_H
#define NETWORK_APP_NET_IF_ENTRY_H

#include <stdint.h>

typedef enum
{
    NET_HARDWARE_INIT_NOT_INITIALIZED = 0,
    NET_HARDWARE_INIT_INITIALIZING,
    NET_HARDWARE_INIT_PREPARED,
    NET_HARDWARE_INIT_FAILED,
    NET_HARDWARE_INIT_ROLLBACK_FAILED
} NetHardwareInitStatus;

void Net_Init(void);
void Net_MainFunction(uint32_t nowMs);
void Net_TxFlush(uint32_t nowMs);
NetHardwareInitStatus Net_GetHardwareInitStatus(void);
uint32_t Net_GetBootId(void);

#endif /* NETWORK_APP_NET_IF_ENTRY_H */
