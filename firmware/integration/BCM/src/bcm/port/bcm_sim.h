#ifndef BCM_SIM_H
#define BCM_SIM_H

#include <stdbool.h>
#include <stdint.h>
#include "../bcm/bcm_types.h"

#define BCM_SIM_FAN_RESPONSE_MS  500u
#define BCM_SIM_LOCK_TRAVEL_MS   300u

void BcmSim_Init(void);

void             BcmSim_DoorDrive(bool active, NetDoorTarget target, uint32_t nowMs);
NetDoorLockState BcmSim_LockState(void);

void        BcmSim_FanCommand(NetFanLevel level, uint32_t nowMs);
NetFanLevel BcmSim_FanMeasured(uint32_t nowMs);

void BcmSim_InjectLockStuck(bool stuck);
void BcmSim_InjectFanStuck(bool stuck);
void BcmSim_InjectFanLost(bool lost);

#endif
