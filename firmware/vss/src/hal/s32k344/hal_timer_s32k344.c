#include "../include/hal_timer.h"
#include "hal_s32k344_cfg.h"

/* Incremented only by the PIT channel callback. */
static volatile uint32_t s_ticks = 0U;
static volatile Pit_Ip_StatusType s_startStatus = PIT_IP_ERROR;

void HalTimer_Init(void)
{
    Pit_Ip_Init(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_INIT_CONFIG);
    Pit_Ip_InitChannel(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL_CONFIG);
    Pit_Ip_EnableChannelInterrupt(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL);
    s_startStatus = Pit_Ip_StartChannel(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL,
                                        HAL_CFG_PIT_PERIOD_COUNT);
}

uint32_t HalTimer_GetTicks(void)
{
    return s_ticks;
}

void HalTimer_WaitTicks(uint32_t ticks)
{
    uint32_t start = s_ticks;

    while ((s_ticks - start) < ticks)
    {
    }
}

/* PIT channel callback, bound by name in the generated channel configuration.
 * Toggling the pin each tick drives the clock synthesizer's reference input. */
void Cs2100Ref_Callback(uint8 channel)
{
    (void)channel;
    Siul2_Dio_Ip_TogglePins(HAL_CFG_REF_PORT,
                            (Siul2_Dio_Ip_PinsChannelType)(1U << HAL_CFG_REF_PIN));
    s_ticks++;
}
