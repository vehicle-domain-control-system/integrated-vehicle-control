#ifndef TEST_STUB_STM_IP_H
#define TEST_STUB_STM_IP_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t uint8;
typedef uint32_t uint32;

typedef struct
{
    bool stopInDebugMode;
    uint8 clockPrescaler;
} Stm_Ip_InstanceConfigType;

typedef struct
{
    uint8 hwChannel;
    void (*callback)(uint8 callbackParam);
    uint8 callbackParam;
    uint8 channelMode;
} Stm_Ip_ChannelConfigType;

void Stm_Ip_Init(uint8 instance, const Stm_Ip_InstanceConfigType *configPtr);
void Stm_Ip_InitChannel(uint8 instance, const Stm_Ip_ChannelConfigType *configPtr);
void Stm_Ip_StartTimer(uint8 instance, uint32 startValue);
uint32 Stm_Ip_GetCounterValue(uint8 instance);

#endif /* TEST_STUB_STM_IP_H */
