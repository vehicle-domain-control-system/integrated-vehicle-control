#include "../include/hal_board.h"
#include "hal_s32k344_cfg.h"

/* Init results kept for the debugger. */
static volatile Clock_Ip_StatusType          s_clockStatus = CLOCK_IP_ERROR;
static volatile Clock_Ip_PllStatusType       s_pllStatus = CLOCK_IP_PLL_STATUS_UNDEFINED;
static volatile Siul2_Port_Ip_PortStatusType s_pinStatus = SIUL2_PORT_ERROR;
static volatile IntCtrl_Ip_StatusType        s_intCtrlStatus = INTCTRL_IP_STATUS_ERROR;
static volatile uint32                       s_pllStatusReads = 0U;

bool HalBoard_Init(void)
{
    uint32 reads = 0U;

    /* Same calls and order as the verified bring-up. Every step runs even if
     * an earlier one reported an error; the result only summarizes them. */
    s_clockStatus = Clock_Ip_Init(HAL_CFG_CLOCK_CONFIG);

    /* The vendor clock driver asks for the lock status to be polled until
     * locked before the PLL output is distributed. Each read already waits
     * up to that driver's own timeout, so a few reads are enough. */
    do
    {
        s_pllStatus = Clock_Ip_GetPllStatus();
        reads++;
    } while ((s_pllStatus != CLOCK_IP_PLL_LOCKED) && (reads < HAL_BOARD_PLL_STATUS_READS));
    s_pllStatusReads = reads;
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
