#include "../include/hal_gpio.h"
#include "hal_s32k344_cfg.h"

bool HalGpio_ReadSw2(void)
{
    return Siul2_Dio_Ip_ReadPin(HAL_CFG_SW2_PORT, HAL_CFG_SW2_PIN) != 0U;
}
