#ifndef BCM_PORT_H
#define BCM_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include "bcm_types.h"

uint32_t BcmPort_BootId(void);

bool BcmPort_InitOk(void);

bool             BcmPort_DoorContact(bool *isOpen);
NetDoorLockState BcmPort_LockState(void);
void             BcmPort_DoorDrive(bool active, NetDoorTarget target);

void        BcmPort_FanCommand(NetFanLevel level);
NetFanLevel BcmPort_FanMeasured(void);
void        BcmPort_Thermal(NetThermalDirection direction, uint8_t outputPct);
bool        BcmPort_HeatsinkRaw(uint16_t *raw);

bool BcmPort_LightApply(uint8_t redPct, uint8_t greenPct, uint8_t bluePct);

#endif
