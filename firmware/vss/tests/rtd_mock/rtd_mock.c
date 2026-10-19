#include <stdbool.h>
#include <string.h>

#include "rtd_mock.h"

/* The stand-in headers leave the generated configuration types incomplete;
 * here they only need an address. */
struct Rm_ConfigType { uint8 mock; };
struct Dma_Ip_InitType { uint8 mock; };
struct Sai_Ip_ConfigType { uint8 mock; };
struct IntCtrl_Ip_CtrlConfigType { uint8 mock; };
struct Lpi2c_Ip_MasterConfigType { uint8 mock; };

const Clock_Ip_ClockConfigType Clock_Ip_aClockConfig[1] = { { 0U } };
const Siul2_Port_Ip_PinSettingsConfig
    g_pin_mux_InitConfigArr_PortContainer_0_VS_0[NUM_OF_CONFIGURED_PINS_PortContainer_0_VS_0];
const IntCtrl_Ip_CtrlConfigType intCtrlConfig = { 0U };
const Lpi2c_Ip_MasterConfigType I2c_Lpi2cMaster_HwChannel0_Channel0_VS_0 = { 0U };
const Rm_ConfigType Rm_Config_VS_0 = { 0U };
const Dma_Ip_InitType Dma_Ip_Sa_xDmaInitPB_VS_0 = { 0U };
const Sai_Ip_ConfigType Sai_Ip_Sa_xInitConfig_0_VS_0 = { 0U };

static RtdMockCall s_log[RTD_MOCK_LOG_MAX];
static uint32 s_count;
static bool s_overflow;

static Clock_Ip_StatusType s_clockInitResult;
static uint32 s_pllUnlockedReads;
static Clock_Ip_PllStatusType s_pllFinal;
static Siul2_Port_Ip_PortStatusType s_portInitResult;
static IntCtrl_Ip_StatusType s_intCtrlInitResult;
static Dma_Ip_ReturnType s_dmaInitResult;
static Dma_Ip_ReturnType s_transferListResult;
static Sai_Ip_StatusType s_sendingStatus;

void RtdMock_Reset(void)
{
    memset(s_log, 0, sizeof(s_log));
    s_count = 0U;
    s_overflow = false;
    s_clockInitResult = CLOCK_IP_SUCCESS;
    s_pllUnlockedReads = 0U;
    s_pllFinal = CLOCK_IP_PLL_LOCKED;
    s_portInitResult = SIUL2_PORT_SUCCESS;
    s_intCtrlInitResult = INTCTRL_IP_STATUS_SUCCESS;
    s_dmaInitResult = DMA_IP_STATUS_SUCCESS;
    s_transferListResult = DMA_IP_STATUS_SUCCESS;
    s_sendingStatus = SAI_IP_STATUS_ABORTED;
}

uint32 RtdMock_CallCount(void)
{
    return s_count;
}

bool RtdMock_Overflow(void)
{
    return s_overflow;
}

const RtdMockCall *RtdMock_Call(uint32 index)
{
    return (index < s_count) ? &s_log[index] : NULL;
}

uint32 RtdMock_CountOf(RtdMockCallKind kind)
{
    uint32 i;
    uint32 n = 0U;

    for (i = 0U; i < s_count; ++i)
    {
        if (s_log[i].kind == kind)
        {
            ++n;
        }
    }
    return n;
}

void RtdMock_SetClockInitResult(Clock_Ip_StatusType status)
{
    s_clockInitResult = status;
}

void RtdMock_SetPllStatus(uint32 unlockedReads, Clock_Ip_PllStatusType finalStatus)
{
    s_pllUnlockedReads = unlockedReads;
    s_pllFinal = finalStatus;
}

void RtdMock_SetPortInitResult(Siul2_Port_Ip_PortStatusType status)
{
    s_portInitResult = status;
}

void RtdMock_SetIntCtrlInitResult(IntCtrl_Ip_StatusType status)
{
    s_intCtrlInitResult = status;
}

void RtdMock_SetDmaInitResult(Dma_Ip_ReturnType status)
{
    s_dmaInitResult = status;
}

void RtdMock_SetTransferListResult(Dma_Ip_ReturnType status)
{
    s_transferListResult = status;
}

void RtdMock_SetSendingStatus(Sai_Ip_StatusType status)
{
    s_sendingStatus = status;
}

static RtdMockCall *RtdMock_Append(RtdMockCallKind kind, const void *ptr, uint32 a, uint32 b)
{
    RtdMockCall *c;

    if (s_count >= RTD_MOCK_LOG_MAX)
    {
        s_overflow = true;
        return NULL;
    }
    c = &s_log[s_count++];
    memset(c, 0, sizeof(*c));
    c->kind = kind;
    c->ptr = (uintptr_t)ptr;
    c->a = a;
    c->b = b;
    return c;
}

/* ---- clock, pins, interrupts, I2C ------------------------------------ */

Clock_Ip_StatusType Clock_Ip_Init(Clock_Ip_ClockConfigType const *Config)
{
    (void)RtdMock_Append(RTD_MOCK_CLOCK_INIT, Config, 0U, 0U);
    return s_clockInitResult;
}

Clock_Ip_PllStatusType Clock_Ip_GetPllStatus(void)
{
    (void)RtdMock_Append(RTD_MOCK_CLOCK_GET_PLL_STATUS, NULL, 0U, 0U);
    if (s_pllUnlockedReads != 0U)
    {
        s_pllUnlockedReads--;
        return CLOCK_IP_PLL_UNLOCKED;
    }
    return s_pllFinal;
}

void Clock_Ip_DistributePll(void)
{
    (void)RtdMock_Append(RTD_MOCK_CLOCK_DISTRIBUTE_PLL, NULL, 0U, 0U);
}

Siul2_Port_Ip_PortStatusType Siul2_Port_Ip_Init(uint32 pinCount,
                                                const Siul2_Port_Ip_PinSettingsConfig configArray[])
{
    (void)RtdMock_Append(RTD_MOCK_PORT_INIT, configArray, pinCount, 0U);
    return s_portInitResult;
}

IntCtrl_Ip_StatusType IntCtrl_Ip_Init(const IntCtrl_Ip_CtrlConfigType *pIntCtrlCtrlConfig)
{
    (void)RtdMock_Append(RTD_MOCK_INTCTRL_INIT, pIntCtrlCtrlConfig, 0U, 0U);
    return s_intCtrlInitResult;
}

void Lpi2c_Ip_MasterInit(uint8 Instance, const Lpi2c_Ip_MasterConfigType *ConfigPtr)
{
    (void)RtdMock_Append(RTD_MOCK_LPI2C_MASTER_INIT, ConfigPtr, Instance, 0U);
}

/* ---- resource manager, DMA, SAI --------------------------------------- */

void Rm_Init(Rm_ConfigType const *ConfigPtr)
{
    (void)RtdMock_Append(RTD_MOCK_RM_INIT, ConfigPtr, 0U, 0U);
}

Dma_Ip_ReturnType Dma_Ip_Init(const Dma_Ip_InitType *const DmaInit)
{
    (void)RtdMock_Append(RTD_MOCK_DMA_INIT, DmaInit, 0U, 0U);
    return s_dmaInitResult;
}

Dma_Ip_ReturnType Dma_Ip_SetLogicChannelTransferList(const uint32 LogicCh,
                                                     const Dma_Ip_LogicChannelTransferListType List[],
                                                     const uint32 ListDimension)
{
    RtdMockCall *c = RtdMock_Append(RTD_MOCK_DMA_SET_TRANSFER_LIST, List, LogicCh, ListDimension);
    uint32 i;

    if (c != NULL)
    {
        for (i = 0U; (i < ListDimension) && (i < RTD_MOCK_LIST_MAX); ++i)
        {
            c->list[i] = List[i];
        }
        c->listLen = i;
    }
    return s_transferListResult;
}

void Sai_Ip_TxInit(const uint8 u8Instance, const Sai_Ip_ConfigType *pConfig)
{
    (void)RtdMock_Append(RTD_MOCK_SAI_TX_INIT, pConfig, u8Instance, 0U);
}

/* Records the first channel's data pointer (one channel is used). */
void Sai_Ip_Send(const uint8 u8Instance, const uint8 *const aData[], const uint32 u32Count)
{
    (void)RtdMock_Append(RTD_MOCK_SAI_SEND, aData[0], u8Instance, u32Count);
}

Sai_Ip_StatusType Sai_Ip_GetSendingStatus(const uint8 u8Instance, uint32 *pCountRemain)
{
    (void)RtdMock_Append(RTD_MOCK_SAI_GET_SENDING_STATUS, pCountRemain, u8Instance, 0U);
    return s_sendingStatus;
}

void Sai_Ip_AbortTransfer(const uint8 u8Instance)
{
    (void)RtdMock_Append(RTD_MOCK_SAI_ABORT_TRANSFER, NULL, u8Instance, 0U);
}

void Sai_Ip_IRQHandler(const uint8 u8Instance)
{
    (void)RtdMock_Append(RTD_MOCK_SAI_IRQ_HANDLER, NULL, u8Instance, 0U);
}
