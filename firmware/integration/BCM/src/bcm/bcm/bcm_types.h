#ifndef BCM_TYPES_H
#define BCM_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "network/app/net_if_bcm.h"

#define NET_BCM_FUNCTION_DOOR     0x01u
#define NET_BCM_FUNCTION_CLIMATE  0x02u
#define NET_BCM_FUNCTION_LIGHT    0x04u
#define NET_BCM_FUNCTION_ALL      0x07u

typedef struct
{
    uint32_t domainBoot;
    uint32_t commandId;
} BcmCommandRef;

typedef enum
{
    BCM_FAULT_DOOR_SENSOR        = 0,
    BCM_FAULT_FAN_SENSOR         = 1,
    BCM_FAULT_TEMPERATURE_SENSOR = 2,
    BCM_FAULT_OVERHEAT           = 3,
    BCM_FAULT_FAN                = 4,
    BCM_FAULT_LOCK_ACTUATOR      = 5,
    BCM_FAULT_LIGHT_APPLY        = 6,
    BCM_FAULT_COMM_TIMEOUT       = 7
} BcmFault;

#define BCM_FAULT_COUNT  8u

#define BCM_LIGHT_TYPE_COUNT  4u

#endif
