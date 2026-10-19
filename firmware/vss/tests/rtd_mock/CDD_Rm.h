#ifndef MOCK_CDD_RM_H
#define MOCK_CDD_RM_H

#include "Std_Types.h"

typedef struct Rm_ConfigType Rm_ConfigType;

extern const Rm_ConfigType Rm_Config_VS_0;

void Rm_Init(Rm_ConfigType const *ConfigPtr);

#endif /* MOCK_CDD_RM_H */
