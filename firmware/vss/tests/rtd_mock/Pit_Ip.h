#ifndef MOCK_PIT_IP_H
#define MOCK_PIT_IP_H

#include "Std_Types.h"

typedef enum {
    PIT_IP_SUCCESS = E_OK,
    PIT_IP_ERROR   = E_NOT_OK
} Pit_Ip_StatusType;

typedef struct { uint8 mock; } Pit_Ip_InstanceConfigType;
typedef struct { uint8 mock; } Pit_Ip_ChannelConfigType;

extern const Pit_Ip_InstanceConfigType PIT_0_InitConfig_PB_VS_0;
extern const Pit_Ip_ChannelConfigType  PIT_0_ChannelConfig_PB_VS_0[1U];
#define PIT_0_CH_0 &PIT_0_ChannelConfig_PB_VS_0[0U]

void              Pit_Ip_Init(uint8 instance, const Pit_Ip_InstanceConfigType *config);
void              Pit_Ip_InitChannel(uint8 instance, const Pit_Ip_ChannelConfigType *chnlConfig);
void              Pit_Ip_EnableChannelInterrupt(uint8 instance, uint8 channel);
Pit_Ip_StatusType Pit_Ip_StartChannel(uint8 instance, uint8 channel, uint32 countValue);

#endif /* MOCK_PIT_IP_H */
