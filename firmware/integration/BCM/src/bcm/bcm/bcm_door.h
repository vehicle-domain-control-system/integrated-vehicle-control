#ifndef BCM_DOOR_H
#define BCM_DOOR_H

#include <stdbool.h>
#include <stdint.h>
#include "bcm_types.h"

typedef struct
{
    bool             contactValid;
    bool             contactOpen;
    NetDoorLockState lockMeasured;
    bool             sustainAllowed;
} BcmDoorInputs;

typedef struct
{
    bool          driveActive;
    NetDoorTarget driveTarget;
} BcmDoorOutputs;

void BcmDoor_Init(void);

void BcmDoor_UpdateInputs(const BcmDoorInputs *inputs, uint32_t nowMs);

bool BcmDoor_IsSettled(void);

NetBcmResult BcmDoor_Command(const NetBcmDoorCommand *command, BcmCommandRef ref, uint32_t nowMs);

NetBcmResult BcmDoor_Rejected(BcmCommandRef ref, NetDoorTarget target, NetReason reason);

bool BcmDoor_Tick(uint32_t nowMs, NetBcmResult *result);

BcmDoorOutputs  BcmDoor_Outputs(void);
NetBcmDoorState BcmDoor_State(void);
bool            BcmDoor_ActuatorFault(void);
bool            BcmDoor_SensorFault(void);

#endif
