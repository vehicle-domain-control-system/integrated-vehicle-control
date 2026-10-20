#ifndef LPUART_UART_IP_SA_PBCFG_H
#define LPUART_UART_IP_SA_PBCFG_H
/* Host mock of the ConfigTools-generated LPUART config header of
 * network_344_can_bringup: same instance macros and configuration symbols
 * (LPUART0 console, LPUART11 wired to the S32K144 debug node).
 * Independently written; NOT the NXP file. */
#include "Lpuart_Uart_Ip_Types.h"

#define LPUART_UART_IP_INSTANCE_USING_0   0U
#define LPUART_UART_IP_INSTANCE_USING_11  11U

extern const Lpuart_Uart_Ip_UserConfigType Lpuart_Uart_Ip_xHwConfigPB_0;
extern const Lpuart_Uart_Ip_UserConfigType Lpuart_Uart_Ip_xHwConfigPB_11;

#endif
