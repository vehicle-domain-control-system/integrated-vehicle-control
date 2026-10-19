/* Audio transmit board adapter against the recording vendor-driver stand-in:
 * the single submission of the whole ring and the stop after the vendor
 * driver has already ended the send in error (DMA error path). Linked with
 * hal_audio_tx_s32k344.c and tests/rtd_mock/rtd_mock.c. */

#include "test_util.h"

#include "../rtd_mock/rtd_mock.h"
#include "../../src/hal/include/hal_audio_tx.h"
#include "../../src/hal/s32k344/hal_s32k344_cfg.h"

#define RING_TOTAL_WORDS (HAL_AUDIO_PRIME_WORDS + 2u * HAL_AUDIO_HALF_WORDS)

static bool CallIs(uint32 index, RtdMockCallKind kind, const void *ptr, uint32 a, uint32 b)
{
    const RtdMockCall *c = RtdMock_Call(index);

    return (c != NULL) && (c->kind == kind) && (c->ptr == (uintptr_t)ptr) && (c->a == a) && (c->b == b);
}

static void test_start_before_init_is_refused(void)
{
    RtdMock_Reset();
    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CallCount(), 0u);
}

/* SAI init and ring controls come first; then one submission of the whole
 * ring (prime area, A and B) from the prime area. */
static void test_start_submits_the_ring_once(void)
{
    RtdMock_Reset();
    CHECK(HalAudioTx_Init(NULL, NULL));
    CHECK_EQ_U(RtdMock_CallCount(), 4u);
    CHECK(HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CallCount(), 5u);
    CHECK(CallIs(2u, RTD_MOCK_SAI_TX_INIT, &Sai_Ip_Sa_xInitConfig_0_VS_0, 0u, 0u));
    CHECK(RtdMock_Call(3u)->kind == RTD_MOCK_DMA_SET_TRANSFER_LIST);
    CHECK(CallIs(4u, RTD_MOCK_SAI_SEND, HalAudioTx_Ring(), 0u, RING_TOTAL_WORDS));
    CHECK_EQ_U(RING_TOTAL_WORDS, 4104u);

    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 1u);
}

/* Callbacks registered as NULL are skipped (the program would crash
 * otherwise); the interrupts reach no vendor call. */
static void test_null_callbacks_are_safe(void)
{
    RtdMock_Reset();
    AudioPlayer_DmaTxIrqCallback();
    AudioPlayer_SaiTxCallback(SAI_IP_ERROR);
    CHECK_EQ_U(RtdMock_CallCount(), 0u);
}

/* After a DMA error the vendor driver has ended the send with status ERROR;
 * its abort then does nothing and the stop is reported as unconfirmed. */
static void test_stop_after_dma_error_is_unconfirmed(void)
{
    RtdMock_Reset();
    RtdMock_SetSendingStatus(SAI_IP_STATUS_ERROR);
    CHECK(HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_NONE);
    HalAudioTx_Abort();
    CHECK_EQ_U(RtdMock_CallCount(), 2u);
    CHECK(CallIs(0u, RTD_MOCK_SAI_ABORT_TRANSFER, NULL, 0u, 0u));
    CHECK(CallIs(1u, RTD_MOCK_SAI_GET_SENDING_STATUS, NULL, 0u, 0u));
    CHECK(HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_UNCONFIRMED);

    HalAudioTx_Abort();
    CHECK_EQ_U(RtdMock_CallCount(), 2u);
    CHECK(!HalAudioTx_Start());
    CHECK_EQ_U(RtdMock_CountOf(RTD_MOCK_SAI_SEND), 0u);
}

int main(void)
{
    test_start_before_init_is_refused();
    test_start_submits_the_ring_once();
    test_null_callbacks_are_safe();
    test_stop_after_dma_error_is_unconfirmed();
    TEST_FINISH();
}
