#include "../include/hal_audio_tx.h"
#include "hal_s32k344_cfg.h"

#define HAL_AUDIO_RING_WORDS  (HAL_AUDIO_HALF_WORDS * 2U)
#define HAL_AUDIO_TOTAL_WORDS (HAL_AUDIO_PRIME_WORDS + HAL_AUDIO_RING_WORDS)
#define HAL_AUDIO_RING_BYTES  (HAL_AUDIO_RING_WORDS * 2U)

/* The vendor driver writes words into the transmit FIFO by CPU until it is
 * full and hands only the rest to the DMA. The FIFO holds 2^(WFP width - 1)
 * words (the top write-pointer bit is the wrap flag), so the prime area must
 * be exactly that long for the DMA to start at half A. */
typedef char HalAudioTx_PrimeIsOneFifoFill[
    ((1UL << (SAI_TFR_WFP_WIDTH - 1UL)) == HAL_AUDIO_PRIME_WORDS) ? 1 : -1];

/* Transmit ring: [prime 8 words][A 2048 words][B 2048 words].
 * Non-cacheable, so the CPU and the DMA see the same samples without cache
 * maintenance. */
#define MCL_START_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#include "Mcl_MemMap.h"

static uint16 s_ring[HAL_AUDIO_TOTAL_WORDS] __attribute__((aligned(32)));

#define MCL_STOP_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#include "Mcl_MemMap.h"

/* The single SAI submission starts at the prime area. The vendor driver writes the
 * first 8 words into the FIFO by CPU, then the DMA runs A and B as one
 * 4096-word major loop. */
static const uint8 *const s_txData[1U] = { (const uint8 *)&s_ring[0U] };

static HalAudioTxIsrFn s_onBoundary = NULL_PTR;
static HalAudioTxIsrFn s_onError = NULL_PTR;

static volatile Dma_Ip_ReturnType s_dmaInitStatus = DMA_IP_STATUS_ERROR;
static volatile Dma_Ip_ReturnType s_dmaRingStatus = DMA_IP_STATUS_ERROR;
static volatile bool s_started = false;
static volatile bool s_aborted = false;
static volatile uint32 s_abortSendingStatus = 0xFFFFFFFFU;
static volatile HalAudioTxAbortState s_abortState = HAL_AUDIO_TX_ABORT_NONE;

/* Interrupt observations for the debugger. */
static volatile uint32 s_boundaryIrqCount = 0U;
static volatile uint32 s_saiCallbackCount = 0U;
static volatile uint32 s_saiErrorCount = 0U;
static volatile uint32 s_saiUnexpectedCompleteCount = 0U;
static volatile uint8  s_saiLastEvent = 0xFFU;

uint16_t *HalAudioTx_Ring(void)
{
    return (uint16_t *)&s_ring[0U];
}

/* The SAI init leaves the vendor's block-transfer TCD. Only these controls
 * are overridden so the DMA cycles over A and B without being re-armed:
 * SLAST rewinds the source by the ring size, interrupts at half and at the
 * end of the major loop, and the request stays enabled at the end. */
static Dma_Ip_ReturnType HalAudioTx_ConfigureRing(void)
{
    Dma_Ip_LogicChannelTransferListType list[4U];

    list[0U].Param = DMA_IP_CH_SET_SOURCE_SIGNED_LAST_ADDR_ADJ;
    list[0U].Value = (uint32)(0U - HAL_AUDIO_RING_BYTES);
    list[1U].Param = DMA_IP_CH_SET_CONTROL_EN_HALF_MAJOR_INTERRUPT;
    list[1U].Value = 1U;
    list[2U].Param = DMA_IP_CH_SET_CONTROL_EN_MAJOR_INTERRUPT;
    list[2U].Value = 1U;
    list[3U].Param = DMA_IP_CH_SET_CONTROL_DIS_AUTO_REQUEST;
    list[3U].Value = 0U;

    return Dma_Ip_SetLogicChannelTransferList(HAL_CFG_DMA_LOGIC_CH, list, 4U);
}

bool HalAudioTx_Init(HalAudioTxIsrFn onBoundary, HalAudioTxIsrFn onError)
{
    s_onBoundary = onBoundary;
    s_onError = onError;
    s_dmaRingStatus = DMA_IP_STATUS_ERROR;

    /* The resource manager routes SAI0 TX through DMAMUX0 to DMA channel 0. */
    Rm_Init(HAL_CFG_RM_CONFIG);
    s_dmaInitStatus = Dma_Ip_Init(HAL_CFG_DMA_INIT_CONFIG);
    if (s_dmaInitStatus != DMA_IP_STATUS_SUCCESS)
    {
        return false;
    }

    Sai_Ip_TxInit(HAL_CFG_SAI_INSTANCE, HAL_CFG_SAI_CONFIG);
    s_dmaRingStatus = HalAudioTx_ConfigureRing();
    return s_dmaRingStatus == DMA_IP_STATUS_SUCCESS;
}

bool HalAudioTx_Start(void)
{
    if (s_started || s_aborted ||
        (s_dmaInitStatus != DMA_IP_STATUS_SUCCESS) ||
        (s_dmaRingStatus != DMA_IP_STATUS_SUCCESS))
    {
        return false;
    }
    s_started = true;
    Sai_Ip_Send(HAL_CFG_SAI_INSTANCE, s_txData, HAL_AUDIO_TOTAL_WORDS);
    return true;
}

void HalAudioTx_Abort(void)
{
    Sai_Ip_StatusType status;

    if (s_aborted)
    {
        return;
    }
    /* Latched before the call: the transmitter is never started again. */
    s_aborted = true;
    Sai_Ip_AbortTransfer(HAL_CFG_SAI_INSTANCE);
    /* The vendor abort acts only on a send it still sees as busy. After a DMA
     * error it has already ended the send (status ERROR) with only the DMA
     * request disabled, so the transmitter stays enabled: unconfirmed.
     * ABORTED is the vendor driver's view, not proof the output is silent. */
    status = Sai_Ip_GetSendingStatus(HAL_CFG_SAI_INSTANCE, NULL_PTR);
    s_abortSendingStatus = (uint32)status;
    s_abortState = (status == SAI_IP_STATUS_ABORTED) ? HAL_AUDIO_TX_ABORT_STOPPED
                                                     : HAL_AUDIO_TX_ABORT_UNCONFIRMED;
}

HalAudioTxAbortState HalAudioTx_AbortState(void)
{
    return s_abortState;
}

/* DMA channel 0 interrupt callback (half and major loop), bound by name in
 * the generated DMA configuration. */
void AudioPlayer_DmaTxIrqCallback(void)
{
    s_boundaryIrqCount++;
    if (s_onBoundary != NULL_PTR)
    {
        s_onBoundary();
    }
}

/* SAI TX callback, bound by name in the generated SAI configuration. Normal
 * progress is reported by the DMA callback; this one only reports errors.
 * A run, sync or general error would let the DMA replay stale samples, so it
 * is passed on as a fault. */
void AudioPlayer_SaiTxCallback(Sai_Ip_ReportType event)
{
    s_saiCallbackCount++;
    s_saiLastEvent = (uint8)event;

    if ((event == SAI_IP_ERROR) || (event == SAI_IP_RUN_ERROR) || (event == SAI_IP_SYNC_ERROR))
    {
        s_saiErrorCount++;
        if (s_onError != NULL_PTR)
        {
            s_onError();
        }
    }
    else if (event == SAI_IP_TX_COMPLETE)
    {
        /* Not expected: the ring never completes. */
        s_saiUnexpectedCompleteCount++;
    }
    else
    {
        /* Visible through s_saiLastEvent. */
    }
}

/* SAI0 interrupt handler, bound by name in the generated interrupt table. */
void AudioPlayer_Sai0IrqHandler(void)
{
    Sai_Ip_IRQHandler(HAL_CFG_SAI_INSTANCE);
}
