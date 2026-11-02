/*
 * CIS node on the Network stack (S32K144EVB, NET_NODE_BUILD + CIS_USE_NETWORK_API).
 * firmware/CIS/NETWORK_INTEGRATION.md section 3: the bench application of the Network
 * team (CisBench_*) is replaced by the CIS application (CisNode_*).
 *
 * CAN FD: FlexCAN0 PTE4 (RX) / PTE5 (TX), 500 kbit/s / 2 Mbit/s, BRS.
 * UART-C: LPUART1 PTC6 / PTC7 (OpenSDA virtual COM), 115200 8N1.
 * Sensors: HC-SR04 TRIG PTE7 / ECHO PTE8, DHT22 PTA14, VEML7700 SDA PTA2 / SCL PTA3.
 *
 * Debugger: g_net_diag (Network), cis_node_time_ok, and the CIS state behind cis_app_*().
 */
#include "Clock_Ip.h"
#include "Port_Ci_Port_Ip.h"
#include "IntCtrl_Ip.h"
#include "Port_Ci_Port_Ip_Cfg.h"
#include "IntCtrl_Ip_Cfg.h"
#include "net_board_time144.h"
#include "cis_node.h"

volatile bool cis_node_time_ok;     /* false: FTM0 1 MHz did not start (see NETWORK_INTEGRATION.md 4) */

int main(void)
{
    (void)Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);
    (void)Port_Ci_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                               g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);
    (void)IntCtrl_Ip_Init(&IntCtrlConfig_0);

    Board_TimeInit();
    cis_node_time_ok = CisNode_Init();      /* sensors, then Net_Init() */
    for (;;) {
        CisNode_Service();                  /* Net_MainFunction(), CIS functions, CisNet_Publish*() */
    }
    return 0;
}
