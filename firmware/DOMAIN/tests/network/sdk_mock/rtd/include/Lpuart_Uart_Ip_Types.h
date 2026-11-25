#ifndef LPUART_UART_IP_TYPES_H
#define LPUART_UART_IP_TYPES_H
/* Host mock of the RTD Lpuart_Uart_Ip types used by src/platform/rtd.
 * Type names, enum members and values, callback signature and the member
 * order of the user configuration follow the S32K3 RTD 7.0.1 / S32K1 RTD
 * 3.0.0 headers as used by the bring-up projects (interrupt transfers, DMA
 * and the receive idle timeout disabled, as in their generated config).
 * The receive idle timeout can be switched on for a test build by defining
 * LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT as STD_ON (the generated
 * Lpuart_Uart_Ip_Defines.h setting in a real project).
 * Only what the Network HAL and the host tests use.
 * Independently written; NOT the NXP file. */
#include "Std_Types.h"

#define LPUART_UART_IP_HAS_DMA_ENABLED           STD_OFF
#ifndef LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT
#define LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT  STD_OFF
#endif

typedef enum {
    LPUART_UART_IP_USING_DMA        = 0U,
    LPUART_UART_IP_USING_INTERRUPTS = 1U
} Lpuart_Uart_Ip_TransferType;

typedef enum {
    LPUART_UART_IP_STATUS_SUCCESS       = 0x00U,
    LPUART_UART_IP_STATUS_ERROR         = 0x01U,
    LPUART_UART_IP_STATUS_BUSY          = 0x02U,
    LPUART_UART_IP_STATUS_TIMEOUT       = 0x03U,
    LPUART_UART_IP_STATUS_TX_UNDERRUN   = 0x04U,
    LPUART_UART_IP_STATUS_RX_OVERRUN    = 0x05U,
    LPUART_UART_IP_STATUS_ABORTED       = 0x06U,
    LPUART_UART_IP_STATUS_FRAMING_ERROR = 0x07U,
    LPUART_UART_IP_STATUS_PARITY_ERROR  = 0x08U,
    LPUART_UART_IP_STATUS_NOISE_ERROR   = 0x09U,
    LPUART_UART_IP_STATUS_DMA_ERROR     = 0x10U
#if (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
    , LPUART_UART_IP_STATUS_RX_IDLE_STATE = 0x11U
#endif
} Lpuart_Uart_Ip_StatusType;

typedef enum {
    LPUART_UART_IP_EVENT_RX_FULL      = 0x00U,
    LPUART_UART_IP_EVENT_TX_EMPTY     = 0x01U,
    LPUART_UART_IP_EVENT_END_TRANSFER = 0x02U,
    LPUART_UART_IP_EVENT_ERROR        = 0x03U
#if (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
    , LPUART_UART_IP_EVENT_IDLE_STATE = 0x04U
#endif
} Lpuart_Uart_Ip_EventType;

typedef enum {
    LPUART_UART_IP_7_BITS_PER_CHAR  = 0x0U,
    LPUART_UART_IP_8_BITS_PER_CHAR  = 0x1U,
    LPUART_UART_IP_9_BITS_PER_CHAR  = 0x2U,
    LPUART_UART_IP_10_BITS_PER_CHAR = 0x3U
} Lpuart_Uart_Ip_BitCountPerCharType;

typedef enum {
    LPUART_UART_IP_PARITY_DISABLED = 0x0U,
    LPUART_UART_IP_PARITY_EVEN     = 0x2U,
    LPUART_UART_IP_PARITY_ODD      = 0x3U
} Lpuart_Uart_Ip_ParityModeType;

typedef enum {
    LPUART_UART_IP_ONE_STOP_BIT = 0x0U,
    LPUART_UART_IP_TWO_STOP_BIT = 0x1U
} Lpuart_Uart_Ip_StopBitCountType;

typedef void (*Lpuart_Uart_Ip_CallbackType)(const uint8 HwInstance,
                                            const Lpuart_Uart_Ip_EventType Event,
                                            const void *UserData);

/* Driver runtime state (one per instance, owned by the generated config). */
typedef struct {
    uint32 BaudRate;
    const uint8 *TxBuff;
    uint8 *RxBuff;
    volatile uint32 TxSize;
    volatile uint32 RxSize;
    volatile boolean IsTxBusy;
    volatile boolean IsRxBusy;
    volatile Lpuart_Uart_Ip_StatusType TransmitStatus;
    volatile Lpuart_Uart_Ip_StatusType ReceiveStatus;
} Lpuart_Uart_Ip_StateStructureType;

/* Generated user configuration (Lpuart_Uart_Ip_Sa_PBcfg.c). */
typedef struct {
    uint32 BaudRate;
    uint32 BaudRateDivisor;
    uint8 BaudOverSamplingRatio;
    Lpuart_Uart_Ip_ParityModeType ParityMode;
    Lpuart_Uart_Ip_StopBitCountType StopBitsCount;
    Lpuart_Uart_Ip_BitCountPerCharType BitCountPerChar;
    Lpuart_Uart_Ip_TransferType TransferType;
    Lpuart_Uart_Ip_CallbackType Callback;
    void *CallbackParam;
    Lpuart_Uart_Ip_StateStructureType *StateStruct;
} Lpuart_Uart_Ip_UserConfigType;

#endif
