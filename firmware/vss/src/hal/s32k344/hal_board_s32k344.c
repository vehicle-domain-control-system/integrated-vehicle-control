#include "../include/hal_board.h"
#include "hal_s32k344_cfg.h"

/* Init results kept for the debugger. */
static volatile Clock_Ip_StatusType          s_clockStatus = CLOCK_IP_ERROR;
static volatile Clock_Ip_PllStatusType       s_pllStatus = CLOCK_IP_PLL_STATUS_UNDEFINED;
static volatile Siul2_Port_Ip_PortStatusType s_pinStatus = SIUL2_PORT_ERROR;
static volatile IntCtrl_Ip_StatusType        s_intCtrlStatus = INTCTRL_IP_STATUS_ERROR;

bool HalBoard_Init(void)
{
    /* Same calls and order as the verified bring-up. Every step runs even if
     * an earlier one reported an error; the result only summarizes them. */
    s_clockStatus = Clock_Ip_Init(HAL_CFG_CLOCK_CONFIG);
    s_pllStatus = Clock_Ip_GetPllStatus();
    Clock_Ip_DistributePll();

    s_pinStatus = Siul2_Port_Ip_Init(HAL_CFG_PIN_COUNT, HAL_CFG_PIN_CONFIG);

    /* PIT0, DMA channel 0 and SAI0 share one priority: no nesting. */
    s_intCtrlStatus = IntCtrl_Ip_Init(HAL_CFG_INTCTRL_CONFIG);

    Lpi2c_Ip_MasterInit(HAL_CFG_LPI2C_INSTANCE, HAL_CFG_LPI2C_MASTER_CONFIG);

    return (s_clockStatus == CLOCK_IP_SUCCESS) &&
           (s_pllStatus == CLOCK_IP_PLL_LOCKED) &&
           (s_pinStatus == SIUL2_PORT_SUCCESS) &&
           (s_intCtrlStatus == INTCTRL_IP_STATUS_SUCCESS);
}
