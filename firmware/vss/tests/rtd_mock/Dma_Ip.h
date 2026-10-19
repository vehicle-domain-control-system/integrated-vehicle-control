#ifndef MOCK_DMA_IP_H
#define MOCK_DMA_IP_H

#include "Std_Types.h"

typedef enum {
    DMA_IP_STATUS_SUCCESS     = E_OK,
    DMA_IP_STATUS_ERROR       = E_NOT_OK,
    DMA_IP_STATUS_WRONG_STATE = 2U
} Dma_Ip_ReturnType;

typedef enum {
    DMA_IP_CH_SET_SOURCE_SIGNED_LAST_ADDR_ADJ     = 2U,
    DMA_IP_CH_SET_CONTROL_EN_MAJOR_INTERRUPT      = 21U,
    DMA_IP_CH_SET_CONTROL_EN_HALF_MAJOR_INTERRUPT = 22U,
    DMA_IP_CH_SET_CONTROL_DIS_AUTO_REQUEST        = 23U
} Dma_Ip_LogicChannelTransferParamType;

typedef enum {
    DMA_IP_CH_GET_SOURCE_ADDRESS     = 0U,
    DMA_IP_CH_GET_CURRENT_ITER_COUNT = 3U
} Dma_Ip_LogicChannelInfoParamType;

typedef enum {
    DMA_IP_CH_RESET_STATE         = 0U,
    DMA_IP_CH_READY_STATE         = 1U,
    DMA_IP_CH_TRANSFER_STATE      = 2U,
    DMA_IP_CH_SCATTERGATHER_STATE = 3U,
    DMA_IP_CH_ERROR_STATE         = 4U
} Dma_Ip_HwChannelStateValueType;

typedef struct {
    Dma_Ip_LogicChannelTransferParamType Param;
    uint32 Value;
} Dma_Ip_LogicChannelTransferListType;

typedef struct {
    Dma_Ip_HwChannelStateValueType ChStateValue;
    uint32  Errors;
    boolean Active;
    boolean Done;
    boolean HRS_present;
} Dma_Ip_LogicChannelStatusType;

typedef struct Dma_Ip_InitType Dma_Ip_InitType;

/* Generated configuration: one logic instance, one logic channel. */
#define SA_DMA_LOGIC_CH_0 ((uint8)0U)
extern const Dma_Ip_InitType Dma_Ip_Sa_xDmaInitPB_VS_0;

Dma_Ip_ReturnType Dma_Ip_Init(const Dma_Ip_InitType *const DmaInit);
Dma_Ip_ReturnType Dma_Ip_GetLogicChannelStatus(const uint32 LogicCh,
                                               Dma_Ip_LogicChannelStatusType *const ChStatus);
Dma_Ip_ReturnType Dma_Ip_SetLogicChannelTransferList(const uint32 LogicCh,
                                                     const Dma_Ip_LogicChannelTransferListType List[],
                                                     const uint32 ListDimension);
Dma_Ip_ReturnType Dma_Ip_GetLogicChannelParam(const uint32 LogicCh,
                                              const Dma_Ip_LogicChannelInfoParamType Param,
                                              uint32 *const Value);

#endif /* MOCK_DMA_IP_H */
