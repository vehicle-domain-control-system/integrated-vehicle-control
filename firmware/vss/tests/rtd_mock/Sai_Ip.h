#ifndef MOCK_SAI_IP_H
#define MOCK_SAI_IP_H

#include "Std_Types.h"

typedef enum {
    SAI_IP_STATUS_UNINITIALIZED = 0x00U,
    SAI_IP_STATUS_COMPLETED     = 0x01U,
    SAI_IP_STATUS_ERROR         = 0x02U,
    SAI_IP_STATUS_TIMEOUT       = 0x03U,
    SAI_IP_STATUS_BUSY          = 0x04U,
    SAI_IP_STATUS_ABORTED       = 0x05U
} Sai_Ip_StatusType;

typedef enum {
    SAI_IP_RX_COMPLETE = 0x00U,
    SAI_IP_TX_COMPLETE = 0x01U,
    SAI_IP_ERROR       = 0x03U,
    SAI_IP_FRAME_START = 0x05U,
    SAI_IP_RUN_ERROR   = 0x06U,
    SAI_IP_SYNC_ERROR  = 0x07U
} Sai_Ip_ReportType;

typedef struct Sai_Ip_ConfigType Sai_Ip_ConfigType;

/* Transmit FIFO write-pointer field width (device register header); the
 * FIFO holds 2^(width - 1) words. */
#define SAI_TFR_WFP_WIDTH (4U)

#define SAICONFIGURATION_1_VS_0_INSTANCE (0U)
extern const Sai_Ip_ConfigType Sai_Ip_Sa_xInitConfig_0_VS_0;

void              Sai_Ip_TxInit(const uint8 u8Instance, const Sai_Ip_ConfigType *pConfig);
void              Sai_Ip_Send(const uint8 u8Instance, const uint8 *const aData[], const uint32 u32Count);
Sai_Ip_StatusType Sai_Ip_GetSendingStatus(const uint8 u8Instance, uint32 *pCountRemain);
void              Sai_Ip_AbortTransfer(const uint8 u8Instance);
void              Sai_Ip_IRQHandler(const uint8 u8Instance);

#endif /* MOCK_SAI_IP_H */
