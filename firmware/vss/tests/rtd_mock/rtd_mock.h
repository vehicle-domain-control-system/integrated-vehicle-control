#ifndef RTD_MOCK_H
#define RTD_MOCK_H

/* Recording implementation of the stand-in vendor driver API, for host tests
 * that link a board adapter source from src/hal/s32k344. Every call lands in
 * one ordered log with its arguments; return values can be scripted. */

#include <stdbool.h>
#include <stdint.h>

#include "Clock_Ip.h"
#include "Siul2_Port_Ip.h"
#include "IntCtrl_Ip.h"
#include "Lpi2c_Ip.h"
#include "CDD_Rm.h"
#include "Dma_Ip.h"
#include "Sai_Ip.h"

#define RTD_MOCK_LOG_MAX  64u
#define RTD_MOCK_LIST_MAX 8u

typedef enum {
    RTD_MOCK_CLOCK_INIT = 1,
    RTD_MOCK_CLOCK_GET_PLL_STATUS,
    RTD_MOCK_CLOCK_DISTRIBUTE_PLL,
    RTD_MOCK_PORT_INIT,
    RTD_MOCK_INTCTRL_INIT,
    RTD_MOCK_LPI2C_MASTER_INIT,
    RTD_MOCK_RM_INIT,
    RTD_MOCK_DMA_INIT,
    RTD_MOCK_DMA_SET_TRANSFER_LIST,
    RTD_MOCK_SAI_TX_INIT,
    RTD_MOCK_SAI_SEND,
    RTD_MOCK_SAI_ABORT_TRANSFER,
    RTD_MOCK_SAI_GET_SENDING_STATUS,
    RTD_MOCK_SAI_IRQ_HANDLER
} RtdMockCallKind;

typedef struct {
    RtdMockCallKind kind;
    uintptr_t ptr;     /* configuration, data or result pointer argument */
    uint32    a;       /* instance, channel or pin count */
    uint32    b;       /* word count or list length */
    uint32    listLen; /* entries copied into list (DMA transfer list) */
    Dma_Ip_LogicChannelTransferListType list[RTD_MOCK_LIST_MAX];
} RtdMockCall;

/* Clears the log and restores the default (successful) results. */
void RtdMock_Reset(void);

uint32 RtdMock_CallCount(void);
bool   RtdMock_Overflow(void);

/* NULL past the end of the log. */
const RtdMockCall *RtdMock_Call(uint32 index);

uint32 RtdMock_CountOf(RtdMockCallKind kind);

void RtdMock_SetClockInitResult(Clock_Ip_StatusType status);

/* Clock_Ip_GetPllStatus returns UNLOCKED for the first unlockedReads calls,
 * then finalStatus. */
void RtdMock_SetPllStatus(uint32 unlockedReads, Clock_Ip_PllStatusType finalStatus);

void RtdMock_SetPortInitResult(Siul2_Port_Ip_PortStatusType status);
void RtdMock_SetIntCtrlInitResult(IntCtrl_Ip_StatusType status);
void RtdMock_SetDmaInitResult(Dma_Ip_ReturnType status);
void RtdMock_SetTransferListResult(Dma_Ip_ReturnType status);

/* Value Sai_Ip_GetSendingStatus returns. */
void RtdMock_SetSendingStatus(Sai_Ip_StatusType status);

#endif /* RTD_MOCK_H */
