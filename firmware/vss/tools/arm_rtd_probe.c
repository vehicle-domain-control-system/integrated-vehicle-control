/* Compile-only probe: exercises every RTD entry point and generated symbol the
 * HAL uses, in exactly the form the HAL calls it. Built against the real RTD
 * and generated headers, and against tests/rtd_mock, so a signature change or
 * a drifting mock is a compile error. Never linked or run. */

#include "../src/hal/s32k344/hal_s32k344_cfg.h"

static const uint8 *const s_probeTxData[1U] = { 0 };

Clock_Ip_StatusType        g_probeClockStatus;
Clock_Ip_PllStatusType     g_probePllStatus;
Siul2_Port_Ip_PortStatusType g_probePortStatus;
IntCtrl_Ip_StatusType      g_probeIntStatus;
Lpi2c_Ip_StatusType        g_probeI2cStatus;
Pit_Ip_StatusType          g_probePitStatus;
Dma_Ip_ReturnType          g_probeDmaStatus;
Sai_Ip_StatusType          g_probeSaiStatus;
Siul2_Dio_Ip_PinsLevelType g_probeLevel;
uint32                     g_probeValue;

void AudioPlayer_DmaTxIrqCallback(void)
{
    (void)Dma_Ip_GetLogicChannelParam(HAL_CFG_DMA_LOGIC_CH,
                                      DMA_IP_CH_GET_CURRENT_ITER_COUNT,
                                      &g_probeValue);
}

void AudioPlayer_SaiTxCallback(Sai_Ip_ReportType event)
{
    if ((event == SAI_IP_ERROR) || (event == SAI_IP_RUN_ERROR) ||
        (event == SAI_IP_SYNC_ERROR) || (event == SAI_IP_TX_COMPLETE)) {
        g_probeSaiStatus = SAI_IP_STATUS_ERROR;
    }
}

void AudioPlayer_Sai0IrqHandler(void)
{
    Sai_Ip_IRQHandler(0U);
}

void Cs2100Ref_Callback(uint8 channel)
{
    (void)channel;
    Siul2_Dio_Ip_TogglePins(HAL_CFG_REF_PORT,
                            (Siul2_Dio_Ip_PinsChannelType)(1U << HAL_CFG_REF_PIN));
}

void Probe_Board(void)
{
    uint8 byte = 0U;

    g_probeClockStatus = Clock_Ip_Init(HAL_CFG_CLOCK_CONFIG);
    g_probePllStatus = Clock_Ip_GetPllStatus();
    Clock_Ip_DistributePll();
    g_probePortStatus = Siul2_Port_Ip_Init(HAL_CFG_PIN_COUNT, HAL_CFG_PIN_CONFIG);
    g_probeIntStatus = IntCtrl_Ip_Init(HAL_CFG_INTCTRL_CONFIG);

    Lpi2c_Ip_MasterInit(HAL_CFG_LPI2C_INSTANCE, HAL_CFG_LPI2C_MASTER_CONFIG);
    Lpi2c_Ip_MasterSetSlaveAddr(HAL_CFG_LPI2C_INSTANCE, 0x4FU, FALSE);
    g_probeI2cStatus = Lpi2c_Ip_MasterSendDataBlocking(HAL_CFG_LPI2C_INSTANCE, &byte, 1U, FALSE, 1000U);
    g_probeI2cStatus = Lpi2c_Ip_MasterReceiveDataBlocking(HAL_CFG_LPI2C_INSTANCE, &byte, 1U, TRUE, 1000U);
    if (g_probeI2cStatus != LPI2C_IP_SUCCESS_STATUS) {
        g_probeValue = 1U;
    }
}

void Probe_Timer(void)
{
    Pit_Ip_Init(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_INIT_CONFIG);
    Pit_Ip_InitChannel(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL_CONFIG);
    Pit_Ip_EnableChannelInterrupt(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL);
    g_probePitStatus = Pit_Ip_StartChannel(HAL_CFG_PIT_INSTANCE, HAL_CFG_PIT_CHANNEL,
                                           HAL_CFG_PIT_PERIOD_COUNT);
    if (g_probePitStatus != PIT_IP_SUCCESS) {
        g_probeValue = 2U;
    }
}

void Probe_Gpio(void)
{
    g_probeLevel = Siul2_Dio_Ip_ReadPin(HAL_CFG_SW2_PORT, HAL_CFG_SW2_PIN);
}

#define MCL_START_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#include "Mcl_MemMap.h"
static uint16 s_probeRing[8U + 2U * 2048U] __attribute__((aligned(32)));
#define MCL_STOP_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#include "Mcl_MemMap.h"

void Probe_AudioTx(void)
{
    Dma_Ip_LogicChannelTransferListType list[4U];
    Dma_Ip_LogicChannelStatusType chStatus;
    uint32 remaining = 0U;

    s_probeRing[0] = 0U;
    Rm_Init(HAL_CFG_RM_CONFIG);
    g_probeDmaStatus = Dma_Ip_Init(HAL_CFG_DMA_INIT_CONFIG);
    Sai_Ip_TxInit(HAL_CFG_SAI_INSTANCE, HAL_CFG_SAI_CONFIG);

    list[0U].Param = DMA_IP_CH_SET_SOURCE_SIGNED_LAST_ADDR_ADJ;
    list[0U].Value = (uint32)(0U - (2U * 2048U * 2U));
    list[1U].Param = DMA_IP_CH_SET_CONTROL_EN_HALF_MAJOR_INTERRUPT;
    list[1U].Value = 1U;
    list[2U].Param = DMA_IP_CH_SET_CONTROL_EN_MAJOR_INTERRUPT;
    list[2U].Value = 1U;
    list[3U].Param = DMA_IP_CH_SET_CONTROL_DIS_AUTO_REQUEST;
    list[3U].Value = 0U;
    g_probeDmaStatus = Dma_Ip_SetLogicChannelTransferList(HAL_CFG_DMA_LOGIC_CH, list, 4U);
    g_probeDmaStatus = Dma_Ip_GetLogicChannelStatus(HAL_CFG_DMA_LOGIC_CH, &chStatus);
    g_probeDmaStatus = Dma_Ip_GetLogicChannelParam(HAL_CFG_DMA_LOGIC_CH,
                                                   DMA_IP_CH_GET_SOURCE_ADDRESS, &g_probeValue);
    if (g_probeDmaStatus != DMA_IP_STATUS_SUCCESS) {
        g_probeValue = 3U;
    }

    Sai_Ip_Send(HAL_CFG_SAI_INSTANCE, s_probeTxData, 8U + 2U * 2048U);
    g_probeSaiStatus = Sai_Ip_GetSendingStatus(HAL_CFG_SAI_INSTANCE, &remaining);
    Sai_Ip_AbortTransfer(HAL_CFG_SAI_INSTANCE);
    if (g_probeSaiStatus == SAI_IP_STATUS_ABORTED) {
        g_probeValue = remaining;
    }
}
