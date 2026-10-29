#ifndef BCM_DIAG_H
#define BCM_DIAG_H

#include <stdbool.h>
#include <stdint.h>
#include "bcm_types.h"

typedef struct
{
    bool initFailed;
    bool inputsSettled;
    bool faults[BCM_FAULT_COUNT];
    bool recovering;
} BcmDiagInputs;

void BcmDiag_Init(void);
void BcmDiag_Update(const BcmDiagInputs *inputs, uint32_t nowMs);

NetControllerState BcmDiag_EcuState(void);
NetBcmStatus       BcmDiag_Status(void);

#endif
