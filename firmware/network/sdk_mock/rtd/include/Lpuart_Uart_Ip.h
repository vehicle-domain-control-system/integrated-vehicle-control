#ifndef LPUART_UART_IP_H
#define LPUART_UART_IP_H
/* Host mock of the RTD Lpuart_Uart_Ip API subset used by src/platform/rtd.
 * Names and parameter types follow the S32K3 RTD 7.0.1 / S32K1 RTD 3.0.0
 * headers (checked against the bring-up projects). Behaviour is described in
 * sdk_mock/rtd/src/mock_rtd.c.
 * Independently written; NOT the NXP file. */
#include "Lpuart_Uart_Ip_Types.h"

void Lpuart_Uart_Ip_Init(const uint8 Instance, const Lpuart_Uart_Ip_UserConfigType *UserConfig);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_Deinit(const uint8 Instance);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncSend(const uint8 Instance, const uint8 *TxBuff, const uint32 TxSize);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetTransmitStatus(const uint8 Instance, uint32 *BytesRemaining);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AbortSendingData(const uint8 Instance);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncReceive(const uint8 Instance, uint8 *RxBuff, const uint32 RxSize);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetReceiveStatus(const uint8 Instance, uint32 *BytesRemaining);
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AbortReceivingData(const uint8 Instance);
void Lpuart_Uart_Ip_SetRxBuffer(const uint8 Instance, uint8 *RxBuff, const uint32 RxSize);
void Lpuart_Uart_Ip_SetTxBuffer(const uint8 Instance, const uint8 *TxBuff, const uint32 TxSize);

#endif
