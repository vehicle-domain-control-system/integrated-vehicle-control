#ifndef MOCK_CLOCK_IP_H
#define MOCK_CLOCK_IP_H

#include "Std_Types.h"

typedef enum {
    CLOCK_IP_SUCCESS = 0x00U,
    CLOCK_IP_ERROR   = 0x01U
} Clock_Ip_StatusType;

typedef enum {
    CLOCK_IP_PLL_LOCKED           = 0x00U,
    CLOCK_IP_PLL_UNLOCKED         = 0x01U,
    CLOCK_IP_PLL_STATUS_UNDEFINED = 0x02U
} Clock_Ip_PllStatusType;

typedef struct { uint8 mock; } Clock_Ip_ClockConfigType;

extern const Clock_Ip_ClockConfigType Clock_Ip_aClockConfig[];

Clock_Ip_StatusType    Clock_Ip_Init(Clock_Ip_ClockConfigType const *Config);
Clock_Ip_PllStatusType Clock_Ip_GetPllStatus(void);
void                   Clock_Ip_DistributePll(void);

#endif /* MOCK_CLOCK_IP_H */
