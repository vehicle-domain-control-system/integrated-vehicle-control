#ifndef FLEXCAN_IP_H
#define FLEXCAN_IP_H
/* Host mock of the RTD FlexCAN_Ip API subset used by src/platform/rtd.
 * Names, parameter types, enum members and struct members follow the real
 * S32K3 RTD 7.0.1 / S32K1 RTD 3.0.0 headers (checked against the bring-up
 * projects). Independently written; contains only what the HAL uses.
 * Real RTD exposes several of these as macros over Call_* functions; the
 * call syntax is identical.
 * Independently written; NOT the NXP file. */
#include "Std_Types.h"

#define FLEXCAN_IP_FEATURE_HAS_FD STD_ON

typedef enum {
    FLEXCAN_STATUS_SUCCESS = E_OK,
    FLEXCAN_STATUS_ERROR = E_NOT_OK,
    FLEXCAN_STATUS_BUSY,
    FLEXCAN_STATUS_TIMEOUT,
    FLEXCAN_STATUS_BUFF_OUT_OF_RANGE,
    FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS
} Flexcan_Ip_StatusType;

typedef enum {
    FLEXCAN_EVENT_RX_COMPLETE = 0U,
    FLEXCAN_EVENT_RXFIFO_COMPLETE,
    FLEXCAN_EVENT_RXFIFO_WARNING,
    FLEXCAN_EVENT_RXFIFO_OVERFLOW,
    FLEXCAN_EVENT_TX_COMPLETE,
    FLEXCAN_EVENT_ERROR,
    FLEXCAN_EVENT_ERROR_FAST,
    FLEXCAN_EVENT_BUSOFF,
    FLEXCAN_EVENT_RX_WARNING,
    FLEXCAN_EVENT_TX_WARNING
} Flexcan_Ip_EventType;

typedef enum { FLEXCAN_MSG_ID_STD = 0, FLEXCAN_MSG_ID_EXT } Flexcan_Ip_MsgBuffIdType;
typedef enum { FLEXCAN_RX_MASK_GLOBAL, FLEXCAN_RX_MASK_INDIVIDUAL } Flexcan_Ip_RxMaskType;
typedef enum {
    FLEXCAN_IP_INT_RX_WARNING, FLEXCAN_IP_INT_TX_WARNING, FLEXCAN_IP_INT_ERR,
    FLEXCAN_IP_INT_ERR_FAST, FLEXCAN_IP_INT_BUSOFF
} Flexcan_Ip_ErrorIntType;

typedef struct {
    Flexcan_Ip_MsgBuffIdType msg_id_type;
    uint8 data_length;
    boolean fd_enable;
    uint8 fd_padding;
    boolean enable_brs;
    boolean is_remote;
    boolean is_polling;
} Flexcan_Ip_DataInfoType;

typedef struct {
    uint32 cs;
    uint32 msgId;
    uint8 data[64];
    uint8 dataLen;
    uint8 id_hit;
    uint32 time_stamp;
} Flexcan_Ip_MsgBuffType;

/* Opaque in the HAL: only addresses are passed. */
typedef struct { uint32 mock_instance_used; } Flexcan_Ip_StateType;

typedef void (* FlexCAN_Ip_CallbackType)(uint8 instance, Flexcan_Ip_EventType eventType,
                                         uint32 buffIdx, const Flexcan_Ip_StateType * flexcanState);
typedef void (* FlexCAN_Ip_ErrorCallbackType)(uint8 instance, Flexcan_Ip_EventType eventType,
                                              uint32 u32ErrStatus, const Flexcan_Ip_StateType * flexcanState);

/* Generated-config shape reduced to what the mock bus needs. */
typedef struct {
    uint8 max_num_mb;
    boolean fd_enable;
    boolean bitRateSwitch;
    FlexCAN_Ip_CallbackType Callback;
    FlexCAN_Ip_ErrorCallbackType ErrorCallback;
} Flexcan_Ip_ConfigType;

Flexcan_Ip_StatusType FlexCAN_Ip_Init(uint8 Flexcan_Ip_u8Instance, Flexcan_Ip_StateType * Flexcan_Ip_pState,
                                      const Flexcan_Ip_ConfigType * Flexcan_Ip_pData);
Flexcan_Ip_StatusType FlexCAN_Ip_SetStartMode(uint8 instance);
Flexcan_Ip_StatusType FlexCAN_Ip_SetStopMode(uint8 instance);
Flexcan_Ip_StatusType FlexCAN_Ip_SetRxMaskType(uint8 instance, Flexcan_Ip_RxMaskType type);
Flexcan_Ip_StatusType FlexCAN_Ip_SetRxIndividualMask(uint8 instance, uint8 mb_idx, uint32 mask);
Flexcan_Ip_StatusType FlexCAN_Ip_SetErrorInt(uint8 u8Instance, Flexcan_Ip_ErrorIntType type, boolean enable);
Flexcan_Ip_StatusType FlexCAN_Ip_ConfigRxMb(uint8 instance, uint8 mb_idx, const Flexcan_Ip_DataInfoType * rx_info, uint32 msg_id);
Flexcan_Ip_StatusType FlexCAN_Ip_Receive(uint8 instance, uint8 mb_idx, Flexcan_Ip_MsgBuffType * data, boolean isPolling);
Flexcan_Ip_StatusType FlexCAN_Ip_Send(uint8 instance, uint8 mb_idx, const Flexcan_Ip_DataInfoType * tx_info,
                                      uint32 msg_id, const uint8 * mb_data);
Flexcan_Ip_StatusType FlexCAN_Ip_GetTransferStatus(uint8 instance, uint8 mb_idx);
Flexcan_Ip_StatusType FlexCAN_Ip_AbortTransfer(uint8 u8Instance, uint8 mb_idx);
uint8 FlexCAN_Ip_GetControllerTxErrorCounter(uint8 instance);
uint8 FlexCAN_Ip_GetControllerRxErrorCounter(uint8 instance);

#endif
