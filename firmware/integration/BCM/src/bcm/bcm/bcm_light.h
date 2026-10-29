#ifndef BCM_LIGHT_H
#define BCM_LIGHT_H

#include <stdbool.h>
#include <stdint.h>
#include "bcm_types.h"

typedef struct
{
    uint8_t redPct;
    uint8_t greenPct;
    uint8_t bluePct;
} BcmRgbPct;

void BcmLight_Init(void);
void BcmLight_UpdateSustain(bool sustainAllowed);

NetBcmResult BcmLight_Command(const NetBcmLightCommand *command, BcmCommandRef ref,
                              NetBcmResult *superseded, bool *hasSuperseded);

bool BcmLight_IsStopCommand(const NetBcmLightCommand *command);

bool BcmLight_PendingOutput(BcmRgbPct *out);

void BcmLight_OnApplied(bool applied);

bool BcmLight_Tick(NetBcmResult *result);

NetBcmLightState BcmLight_State(void);
bool             BcmLight_ApplyFault(void);

#endif
