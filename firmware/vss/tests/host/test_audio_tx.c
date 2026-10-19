/* Audio transmit driver against the HAL stand-in: half-buffer boundary
 * bookkeeping, request guards and the latched, fail-closed fault path. */

#include "test_util.h"
#include "hal_mock.h"

#include "../../src/driver/audio_tx/audio_tx.h"

#define HALF_A 0u
#define HALF_B 1u

static int16_t s_pcm[HAL_AUDIO_HALF_WORDS];

static uint16_t *HalfWords(uint8_t half)
{
    return HalMock_RingWords() + HAL_AUDIO_PRIME_WORDS + (uint32_t)half * HAL_AUDIO_HALF_WORDS;
}

static void FillPcm(int16_t base)
{
    uint32_t i;

    for (i = 0u; i < HAL_AUDIO_HALF_WORDS; ++i)
    {
        s_pcm[i] = (int16_t)(base + (int16_t)(i & 0x7Fu));
    }
}

static bool HalfMatchesPcm(uint8_t half)
{
    const uint16_t *w = HalfWords(half);
    uint32_t i;

    for (i = 0u; i < HAL_AUDIO_HALF_WORDS; ++i)
    {
        if (w[i] != (uint16_t)s_pcm[i])
        {
            return false;
        }
    }
    return true;
}

static bool WordsAre(const uint16_t *w, uint32_t count, uint16_t value)
{
    uint32_t i;

    for (i = 0u; i < count; ++i)
    {
        if (w[i] != value)
        {
            return false;
        }
    }
    return true;
}

/* Fresh driver, started; no boundary yet. */
static void StartFresh(void)
{
    HalMock_Reset();
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
}

/* One boundary, reported and answered with silence. */
static void BoundaryAndFill(uint8_t expectedFree, uint32_t expectedGeneration)
{
    AudioTxEvent ev;

    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_HALF_FREE);
    CHECK_EQ_U(ev.freeHalf, expectedFree);
    CHECK_EQ_U(ev.generation, expectedGeneration);
    CHECK(AudioTx_Request(ev.freeHalf, ev.generation, NULL) == AUDIO_TX_OK);
}

static void test_init_clears_ring_and_initializes_hal_once(void)
{
    HalMock_Reset();
    CHECK(AudioTx_Init());
    CHECK_EQ_U(HalMock_AudioInitCalls(), 1u);
    CHECK(WordsAre(HalMock_RingWords(), HAL_MOCK_RING_WORDS, 0u));
    CHECK(!AudioTx_IsRunning());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 0u);
}

static void test_request_before_start_is_rejected(void)
{
    HalMock_Reset();
    CHECK(AudioTx_Init());
    FillPcm(100);
    CHECK(AudioTx_Request(HALF_B, 0u, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(WordsAre(HalfWords(HALF_B), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
}

static void test_start_once(void)
{
    StartFresh();
    CHECK_EQ_U(HalMock_AudioStartCalls(), 1u);
    CHECK(AudioTx_IsRunning());
    CHECK(!AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 1u);
}

static void test_start_refused_when_hal_init_fails(void)
{
    HalMock_Reset();
    HalMock_SetAudioInitResult(false);
    CHECK(!AudioTx_Init());
    CHECK(!AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 0u);
    CHECK(!AudioTx_IsRunning());
}

static void test_no_event_without_boundary(void)
{
    AudioTxEvent ev;

    StartFresh();
    CHECK(!AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_NONE);
}

static void test_boundaries_alternate_and_generation_counts(void)
{
    uint32_t g;

    StartFresh();
    /* First boundary ends A (half of the major loop), the next ends B
     * (end of the major loop), and so on. */
    for (g = 1u; g <= 6u; ++g)
    {
        BoundaryAndFill((g % 2u == 1u) ? HALF_A : HALF_B, g);
    }
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    CHECK(AudioTx_IsRunning());
}

static void test_half_free_event_fields(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_SetTicks(1234u);
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_HALF_FREE);
    CHECK_EQ_U(ev.freeHalf, HALF_A);
    CHECK_EQ_U(ev.generation, 1u);
    CHECK_EQ_U(ev.boundaryTick, 1234u);
    CHECK_EQ_U(ev.faultMask, 0u);

    /* Reported once. */
    CHECK(!AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_NONE);
}

static void test_request_copies_into_the_free_half_only(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    FillPcm(-300);
    CHECK(AudioTx_Request(HALF_A, ev.generation, s_pcm) == AUDIO_TX_OK);
    CHECK(HalfMatchesPcm(HALF_A));
    CHECK(WordsAre(HalfWords(HALF_B), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK(WordsAre(HalMock_RingWords(), HAL_AUDIO_PRIME_WORDS, 0u));

    /* NULL writes silence over earlier samples. */
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(ev.freeHalf, HALF_B);
    CHECK(AudioTx_Request(HALF_B, ev.generation, NULL) == AUDIO_TX_OK);
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(ev.freeHalf, HALF_A);
    CHECK(AudioTx_Request(HALF_A, ev.generation, NULL) == AUDIO_TX_OK);
    CHECK(WordsAre(HalfWords(HALF_A), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
}

static void test_request_guards_reject_without_fault(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    FillPcm(7);

    CHECK(AudioTx_Request(2u, ev.generation, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(AudioTx_Request(0xFFu, ev.generation, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(AudioTx_Request(HALF_A, ev.generation + 1u, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(AudioTx_Request(HALF_A, ev.generation - 1u, s_pcm) == AUDIO_TX_REJECTED);
    /* B is the half being transmitted. */
    CHECK(AudioTx_Request(HALF_B, ev.generation, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(WordsAre(HalfWords(HALF_A), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK(WordsAre(HalfWords(HALF_B), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);

    /* The valid request still goes through, once. */
    CHECK(AudioTx_Request(HALF_A, ev.generation, s_pcm) == AUDIO_TX_OK);
    CHECK(AudioTx_Request(HALF_A, ev.generation, NULL) == AUDIO_TX_REJECTED);
    CHECK(HalfMatchesPcm(HALF_A));

    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 0u);
    CHECK(AudioTx_IsRunning());
}

static void test_unrefilled_half_reached_again_is_late(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();          /* A free */
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(ev.freeHalf, HALF_A);
    /* A is not written before the DMA comes back to it. */
    HalMock_Boundary();
    CHECK(AudioTx_FaultMask() & AUDIO_TX_FAULT_LATE);
    CHECK(!(AudioTx_FaultMask() & AUDIO_TX_FAULT_OVERLAP));
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 0u);   /* no abort in interrupt context */

    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
    CHECK(ev.faultMask & AUDIO_TX_FAULT_LATE);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_IsRunning());
}

static void test_late_request_after_boundary_is_rejected(void)
{
    AudioTxEvent ev;
    uint32_t staleGeneration;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    staleGeneration = ev.generation;
    HalMock_Boundary();          /* deadline missed while "decoding" */
    FillPcm(1);
    CHECK(AudioTx_Request(HALF_A, staleGeneration, s_pcm) == AUDIO_TX_REJECTED);
    CHECK(AudioTx_FaultMask() & AUDIO_TX_FAULT_LATE);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(WordsAre(HalfWords(HALF_A), HAL_AUDIO_HALF_WORDS, 0u));
}

static uint32_t s_faultSeenByInterrupt;

static void CaptureFaultAtBoundary(void)
{
    s_faultSeenByInterrupt = AudioTx_FaultMask();
}

static void test_boundary_during_write_is_overlap(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    FillPcm(50);
    /* The write reads the timer before and after the copy; the boundary
     * lands on the second read, after the samples were stored. */
    s_faultSeenByInterrupt = 0u;
    HalMock_SetBoundaryProbe(CaptureFaultAtBoundary);
    HalMock_BoundaryOnTickRead(2u);
    CHECK(AudioTx_Request(HALF_A, ev.generation, s_pcm) == AUDIO_TX_LATE);
    /* The interrupt itself saw the write in progress. */
    CHECK(s_faultSeenByInterrupt & AUDIO_TX_FAULT_OVERLAP);
    CHECK(AudioTx_FaultMask() & AUDIO_TX_FAULT_OVERLAP);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_IsRunning());

    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
    CHECK(ev.faultMask & AUDIO_TX_FAULT_OVERLAP);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
}

static void test_boundary_before_first_store_is_overlap(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    HalMock_BoundaryOnTickRead(1u);
    CHECK(AudioTx_Request(HALF_A, ev.generation, NULL) == AUDIO_TX_LATE);
    CHECK(AudioTx_FaultMask() & AUDIO_TX_FAULT_OVERLAP);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
}

static void test_fault_latch_rejects_and_aborts_once(void)
{
    AudioTxEvent ev;
    uint32_t mask;
    int i;

    StartFresh();
    BoundaryAndFill(HALF_A, 1u);
    HalMock_Boundary();          /* B free */
    CHECK(AudioTx_Advance(&ev));
    HalMock_Boundary();          /* B not written: late */
    mask = AudioTx_FaultMask();
    CHECK(mask & AUDIO_TX_FAULT_LATE);

    /* Even a request that would otherwise be valid is refused. */
    CHECK(AudioTx_Request(HALF_B, 3u, NULL) == AUDIO_TX_REJECTED);
    CHECK(AudioTx_Request(ev.freeHalf, ev.generation, NULL) == AUDIO_TX_REJECTED);
    for (i = 0; i < 3; ++i)
    {
        CHECK(AudioTx_Advance(&ev));
        CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
        CHECK_EQ_U(ev.faultMask, mask);
    }
    /* Further boundaries (a transfer that did not stop) change nothing. */
    HalMock_Boundary();
    CHECK(AudioTx_Request(HALF_A, 4u, NULL) == AUDIO_TX_REJECTED);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 1u);
    CHECK(!AudioTx_IsRunning());
}

static void test_sai_error_is_a_fault(void)
{
    AudioTxEvent ev;

    StartFresh();
    BoundaryAndFill(HALF_A, 1u);
    HalMock_SaiError();
    CHECK_EQ_U(AudioTx_FaultMask(), AUDIO_TX_FAULT_SAI);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 0u);
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
    CHECK_EQ_U(ev.faultMask, AUDIO_TX_FAULT_SAI);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
}

static void test_advance_null_is_safe(void)
{
    StartFresh();
    HalMock_Boundary();
    CHECK(!AudioTx_Advance(NULL));
}

int main(void)
{
    test_init_clears_ring_and_initializes_hal_once();
    test_request_before_start_is_rejected();
    test_start_once();
    test_start_refused_when_hal_init_fails();
    test_no_event_without_boundary();
    test_boundaries_alternate_and_generation_counts();
    test_half_free_event_fields();
    test_request_copies_into_the_free_half_only();
    test_request_guards_reject_without_fault();
    test_unrefilled_half_reached_again_is_late();
    test_late_request_after_boundary_is_rejected();
    test_boundary_during_write_is_overlap();
    test_boundary_before_first_store_is_overlap();
    test_fault_latch_rejects_and_aborts_once();
    test_sai_error_is_a_fault();
    test_advance_null_is_safe();
    TEST_FINISH();
}
