/* Host mock of the ConfigTools-generated Lpuart_Uart_Ip_Sa_PBcfg.c of
 * network_344_can_bringup: same symbol names, both instances share the
 * callback name Uart344_BringupCallback, 8N1, interrupt transfers, same
 * baud values. Uart344_BringupCallback is defined either by the Network HAL
 * (NET_BOARD_UART_CALLBACK) or by board code that routes the Network
 * instance to NetHalRtdUart_OnEvent(); in the host tests by the test itself.
 * Kept apart from the FlexCAN config so that builds without a UART link do
 * not pull in the callback reference.
 * Independently written; NOT the NXP file. */
#include "Lpuart_Uart_Ip_Sa_PBcfg.h"

extern void Uart344_BringupCallback(const uint8 HwInstance, const Lpuart_Uart_Ip_EventType Event,
                                    const void *UserData);

static Lpuart_Uart_Ip_StateStructureType Lpuart_Uart_Ip_xStateStructure_0;
static Lpuart_Uart_Ip_StateStructureType Lpuart_Uart_Ip_xStateStructure_11;

const Lpuart_Uart_Ip_UserConfigType Lpuart_Uart_Ip_xHwConfigPB_0 = {
    115440U,                          /* BaudRate (console) */
    33U,                              /* BaudRateDivisor */
    21U,                              /* BaudOverSamplingRatio */
    LPUART_UART_IP_PARITY_DISABLED,
    LPUART_UART_IP_ONE_STOP_BIT,
    LPUART_UART_IP_8_BITS_PER_CHAR,
    LPUART_UART_IP_USING_INTERRUPTS,
    Uart344_BringupCallback,
    NULL_PTR,
    &Lpuart_Uart_Ip_xStateStructure_0
};

const Lpuart_Uart_Ip_UserConfigType Lpuart_Uart_Ip_xHwConfigPB_11 = {
    115942U,                          /* BaudRate (S32K144 peer) */
    15U,                              /* BaudRateDivisor */
    23U,                              /* BaudOverSamplingRatio */
    LPUART_UART_IP_PARITY_DISABLED,
    LPUART_UART_IP_ONE_STOP_BIT,
    LPUART_UART_IP_8_BITS_PER_CHAR,
    LPUART_UART_IP_USING_INTERRUPTS,
    Uart344_BringupCallback,
    NULL_PTR,
    &Lpuart_Uart_Ip_xStateStructure_11
};
