#ifndef BCM_CLIMATE_H
#define BCM_CLIMATE_H

#include <stdbool.h>
#include <stdint.h>
#include "bcm_types.h"

typedef struct
{
    NetFanLevel fanMeasured;
    bool        heatsinkValid;
    int16_t     heatsinkDeciC;
    bool        sustainAllowed;
} BcmClimateInputs;

typedef struct
{
    NetFanLevel         fanCommand;
    NetThermalDirection direction;
    uint8_t             outputPct;
} BcmClimateOutputs;

typedef struct
{
    NetBcmEventKind kind;
    BcmCommandRef   related;
} BcmClimateEvent;

void BcmClimate_Init(void);
void BcmClimate_UpdateInputs(const BcmClimateInputs *inputs, uint32_t nowMs);

NetBcmResult BcmClimate_Command(const NetBcmClimateCommand *command, BcmCommandRef ref,
                                NetBcmResult *superseded, bool *hasSuperseded);

bool BcmClimate_IsStopCommand(const NetBcmClimateCommand *command);

bool BcmClimate_Tick(uint32_t nowMs, NetBcmResult *result);

bool BcmClimate_TakeEvent(BcmClimateEvent *event);

uint32_t BcmClimate_EventsDropped(void);

BcmClimateOutputs  BcmClimate_Outputs(void);
NetBcmClimateState BcmClimate_State(void);

bool BcmClimate_FanFault(void);
bool BcmClimate_FanSensorFault(void);
bool BcmClimate_TemperatureSensorFault(void);
bool BcmClimate_Overheat(void);
bool BcmClimate_Recovering(void);

#endif
