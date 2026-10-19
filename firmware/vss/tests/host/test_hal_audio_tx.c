/* Audio transmit board adapter against the recording vendor-driver stand-in:
 * init order, the four ring controls written after the SAI init, the init
 * failure paths, the interrupt callbacks and the stop latch. Linked with
 * hal_audio_tx_s32k344.c and tests/rtd_mock/rtd_mock.c, not with the HAL
 * stand-in. The adapter's latches last for the whole program, so the
 * submission is checked in test_hal_audio_tx_send.c. */

#include "test_util.h"

#include "../rtd_mock/rtd_mock.h"
#include "../../src/hal/include/hal_audio_tx.h"
#include "../../src/hal/s32k344/hal_s32k344_cfg.h"

#define RING_TOTAL_WORDS (HAL_AUDIO_PRIME_WORDS + 2u * HAL_AUDIO_HALF_WORDS)
#define RING_BYTES       (2u * HAL_AUDIO_HALF_WORDS * 2u)

static uint32_t s_boundaries;
static uint32_t s_errors;

static void OnBoundary(void)
{
    ++s_boundaries;
}

static void OnError(void)
{
    ++s_errors;
}

static bool CallIs(uint32 index, RtdMockCallKind kind, const void *ptr, uint32 a, uint32 b)
{
    const RtdMockCall *c = RtdMock_Call(index);

    if ((c == NULL) || (c->kind != kind) || (c->ptr != (uintptr_t)ptr) || (c->a != a) || (c->b != b))
    {
        printf("  call %u: kind=%d ptr=%p a=%u b=%u\n", (unsigned)index,
               (c != NULL) ? (int)c->kind : -1, (c != NULL) ? (void *)c->ptr : NULL,
               (c != NULL) ? (unsigned)c->a : 0u, (c != NULL) ? (unsigned)c->b : 0u);
        return false;
    }
    return true;
}

static bool ListEntryIs(const RtdMockCall *c, uint32 i, Dma_Ip_LogicChannelTransferParamType param,
                        uint32 value)
{
    return (c != NULL) && (i < c->listLen) && (c->list[i].Param == param) && (c->list[i].Value == value);
}

static void test_dma_init_failure_skips_sai_init(void)
{
    RtdMock_Reset();
    RtdMock_SetDmaInitResult(DMA_IP_STATUS_ERROR);
    CHECK(!HalAudioTx_Init(OnBoundary, OnError));
    CHECK_EQ_U(RtdMock_CallCount(), 2u);
    CHECK(CallIs(0u, RTD_MOCK_RM_INIT, &Rm_Config_VS_0, 0u, 0u));
    CHECK(CallIs(1u, RTD_MOCK_DMA_INIT, &Dma_Ip_Sa_xDmaInitPB_VS_0, 0u, 0u));

    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);
}

static void test_ring_setup_failure_refuses_start(void)
{
    RtdMock_Reset();
    RtdMock_SetTransferListResult(DMA_IP_STATUS_ERROR);
    CHECK(!HalAudioTx_Init(OnBoundary, OnError));
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_TX_INIT), 1u);
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_DMA_SET_TRANSFER_LIST), 1u);
    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);
}

/* The SAI init writes the vendor's block-transfer controls; the ring
 * controls must follow it, before the submission. */
static void test_init_order_and_ring_controls(void)
{
    const RtdMockCall *list;

    RtdMock_Reset();
    CHECK(HalAudioTx_Init(OnBoundary, OnError));
    CHECK_EQ_U(RtdMock_CallCount(), 4u);
    CHECK(CallIs(0u, RTD_MOCK_RM_INIT, &Rm_Config_VS_0, 0u, 0u));
    CHECK(CallIs(1u, RTD_MOCK_DMA_INIT, &Dma_Ip_Sa_xDmaInitPB_VS_0, 0u, 0u));
    CHECK(CallIs(2u, RTD_MOCK_SAI_TX_INIT, &Sai_Ip_Sa_xInitConfig_0_VS_0, 0u, 0u));

    list = RtdMock_Call(3u);
    CHECK((list != NULL) && (list->kind == RTD_MOCK_DMA_SET_TRANSFER_LIST));
    CHECK((list != NULL) && (list->a == 0u) && (list->b == 4u) && (list->listLen == 4u));
    CHECK(ListEntryIs(list, 0u, DMA_IP_CH_SET_SOURCE_SIGNED_LAST_ADDR_ADJ, 0xFFFFE000u));
    CHECK(ListEntryIs(list, 1u, DMA_IP_CH_SET_CONTROL_EN_HALF_MAJOR_INTERRUPT, 1u));
    CHECK(ListEntryIs(list, 2u, DMA_IP_CH_SET_CONTROL_EN_MAJOR_INTERRUPT, 1u));
    CHECK(ListEntryIs(list, 3u, DMA_IP_CH_SET_CONTROL_DIS_AUTO_REQUEST, 0u));
    CHECK_EQ_U(0u - RING_BYTES, 0xFFFFE000u);

    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);
    CHECK(HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_NONE);
}

static void test_ring_is_aligned(void)
{
    CHECK(HalAudioTx_Ring() != NULL);
    CHECK_EQ_U((uintptr_t)HalAudioTx_Ring() % 32u, 0u);
}

static void test_interrupt_callbacks(void)
{
    s_boundaries = 0u;
    s_errors = 0u;
    AudioPlayer_DmaTxIrqCallback();
    AudioPlayer_DmaTxIrqCallback();
    CHECK_EQ_U(s_boundaries, 2u);

    AudioPlayer_SaiTxCallback(SAI_IP_ERROR);
    AudioPlayer_SaiTxCallback(SAI_IP_RUN_ERROR);
    AudioPlayer_SaiTxCallback(SAI_IP_SYNC_ERROR);
    CHECK_EQ_U(s_errors, 3u);
    AudioPlayer_SaiTxCallback(SAI_IP_TX_COMPLETE);
    AudioPlayer_SaiTxCallback(SAI_IP_FRAME_START);
    CHECK_EQ_U(s_errors, 3u);
    CHECK_EQ_U(s_boundaries, 2u);

    RtdMock_Reset();
    AudioPlayer_Sai0IrqHandler();
    CHECK(CallIs(0u, RTD_MOCK_SAI_IRQ_HANDLER, NULL, 0u, 0u));
}

/* Stop before any start: abort, then read the send status; only the first
 * call acts, and no start follows, even after a new init. */
static void test_stop_latch_blocks_start(void)
{
    RtdMock_Reset();
    RtdMock_SetSendingStatus(SAI_IP_STATUS_ABORTED);
    HalAudioTx_Abort();
    CHECK_EQ_U(RtdMock_CallCount(), 2u);
    CHECK(CallIs(0u, RTD_MOCK_SAI_ABORT_TRANSFER, NULL, 0u, 0u));
    CHECK(CallIs(1u, RTD_MOCK_SAI_GET_SENDING_STATUS, NULL, 0u, 0u));
    CHECK(HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_STOPPED);

    HalAudioTx_Abort();
    CHECK_EQ_U(RtdMock_CallCount(), 2u);
    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);

    CHECK(HalAudioTx_Init(OnBoundary, OnError));
    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);
    CHECK(HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_STOPPED);
    CHECK(!RtdMock_Overflow());
}

int main(void)
{
    test_dma_init_failure_skips_sai_init();
    test_ring_setup_failure_refuses_start();
    test_init_order_and_ring_controls();
    test_ring_is_aligned();
    test_interrupt_callbacks();
    test_stop_latch_blocks_start();
    TEST_FINISH();
}
