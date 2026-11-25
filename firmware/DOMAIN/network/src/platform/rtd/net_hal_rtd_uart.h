#ifndef NET_HAL_RTD_UART_H
#define NET_HAL_RTD_UART_H

/* Board-facing entry of the LPUART part of the RTD backend
 * (src/platform/rtd/net_hal_rtd_uart.c). Include it from the board file whose
 * generated-config callback is shared with other instances (routing mode,
 * NET_BOARD_UART_CALLBACK not defined in net_board_cfg.h) and call
 * NetHalRtdUart_OnEvent() from that callback for the Network UART instance:
 *
 *   void Uart344_BringupCallback(const uint8 HwInstance,
 *                                const Lpuart_Uart_Ip_EventType Event, const void *UserData)
 *   {
 *       if (HwInstance == NET_BOARD_UART_C_INSTANCE) { NetHalRtdUart_OnEvent(HwInstance, Event); return; }
 *       ... console instance ...
 *   }
 *
 * ISR context. An instance that is not one of the profile's UART links is
 * ignored and counted (NetHalRtdDiag.uart_unexpected_instance). Before
 * net_hal_rtd_start() every event is ignored. */
#include "Lpuart_Uart_Ip.h"

void NetHalRtdUart_OnEvent(uint8 instance, Lpuart_Uart_Ip_EventType event);

#endif
