/*==================================================================================================
* Project : RTD AUTOSAR 4.7
* Platform : CORTEXM
* Peripheral : S32K14X
* Dependencies : none
*
* Autosar Version : 4.7.0
* Autosar Revision : ASR_REL_4_7_REV_0000
* Autosar Conf.Variant :
* SW Version : 3.0.0
* Build Version : S32K1_RTD_3_0_0_QLP06_D2603_ASR_REL_4_7_REV_0000_20260320
*
* Copyright 2020-2026 NXP
*
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be 
*   used strictly in accordance with the applicable license terms.  By expressly 
*   accepting such terms or by downloading, installing, activating and/or otherwise 
*   using the software, you are agreeing that you have read, and that you agree to 
*   comply with and are bound by, such license terms.  If you do not agree to be 
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

/* Including necessary configuration files. */
#include "Mcal.h"
#include "Clock_Ip.h"
#include "Clock_Ip_Cfg.h"
#include "Port_Ci_Port_Ip.h"
#include "Port_Ci_Port_Ip_Cfg.h"
#include "IntCtrl_Ip.h"
#include "IntCtrl_Ip_Cfg.h"
#ifdef NET_NODE_BUILD
/* Network node build (S32K144 BCM bench, NET_git firmware/network/bench/s32k144_bcm/README.md): Network
 * owns FlexCAN0, the SysTick clock and the superloop; the debug node files
 * are not built. */
#include "network/app/net_if_entry.h"
#include "net_bench/net_board_time144.h"
#include "bcm_node.h"
#else
#include "debug_console.h"
#include "debug_can.h"
#include "debug_uart0.h"
#include "debug_time.h"
#include "debug_tx.h"
#include "debug_rx.h"
#endif

volatile int exit_code = 0;
/* User includes */

#ifdef NET_NODE_BUILD
/* 1 = Clock, 2 = Port, 4 = IntCtrl failed (same codes as the debug node);
 * the loop then stays idle. Net_Init() failures are in g_net_diag. */
int main(void)
{
    if (Clock_Ip_Init(&Clock_Ip_aClockConfig[0]) != CLOCK_IP_SUCCESS)
    {
        exit_code = 1;
    }
    else if (Port_Ci_Port_Ip_Init(
                 NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                 g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals) != PORT_CI_PORT_SUCCESS)
    {
        exit_code = 2;
    }
    else if (IntCtrl_Ip_Init(&IntCtrlConfig_0) != INTCTRL_IP_STATUS_SUCCESS)
    {
        exit_code = 4;
    }
    else
    {
        Board_TimeInit();
        BcmNode_Init();
    }

    for(;;)
    {
        if (exit_code != 0)
        {
            continue;
        }
        BcmNode_Service();
    }
    return exit_code;
}
#else

/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - startup asm routine
 * - main()
*/
int main(void)
{
    if (Clock_Ip_Init(&Clock_Ip_aClockConfig[0]) != CLOCK_IP_SUCCESS)
    {
        exit_code = 1;
    }
    else if (Port_Ci_Port_Ip_Init(
                 NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                 g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals) != PORT_CI_PORT_SUCCESS)
    {
        exit_code = 2;
    }
    else if (!DebugConsole_Init())
    {
        exit_code = 3;
    }
    else if (IntCtrl_Ip_Init(&IntCtrlConfig_0) != INTCTRL_IP_STATUS_SUCCESS)
    {
        exit_code = 4;
    }
    else
    {
        if (!DebugTime_Init())
        {
            (void)DebugConsole_WriteRecord(
                "TIME_UNAVAILABLE; RX/TX remain active, ms timestamps/partial timeout unavailable\r\n", 0U);
        }
        if (!DebugCan_Init()) { exit_code = 5; }
        if (!DebugUart0_Init()) { exit_code = (exit_code == 5) ? 7 : 6; }
        /* 5=CAN, 6=UART0, 7=both: independent services remain available. */
        DebugTx_Init();
        DebugRx_Init();
    }

    for(;;)
    {
        if(exit_code != 0 && exit_code != 5 && exit_code != 6 && exit_code != 7)
        {
            break;
        }
        DebugConsole_Poll();
        DebugCan_Poll();
        DebugUart0_Poll();
        DebugTx_Poll();
        DebugRx_Poll();
    }
    return exit_code;
}
#endif /* NET_NODE_BUILD */

/** @} */
