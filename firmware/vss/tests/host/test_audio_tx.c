/* Audio transmit driver against the HAL stand-in: half-buffer boundary
 * bookkeeping, request guards, missing-boundary detection and the latched,
 * fail-closed fault path. */

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

/* Power-on state of the HAL stand-in and of the driver. */
static void Fresh(void)
{
    HalMock_Reset();
    AudioTx_ResetForTest();
}

/* Fresh driver, started; no boundary yet. */
static void StartFresh(void)
{
    Fresh();
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
    Fresh();
    CHECK(AudioTx_Init());
    CHECK_EQ_U(HalMock_AudioInitCalls(), 1u);
    CHECK(WordsAre(HalMock_RingWords(), HAL_MOCK_RING_WORDS, 0u));
    CHECK(!AudioTx_IsRunning());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 0u);
}

static void test_request_before_start_is_rejected(void)
{
    Fresh();
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
    Fresh();
    HalMock_SetAudioInitResult(false);
    CHECK(!AudioTx_Init());
    CHECK(!AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 0u);
    CHECK(!AudioTx_IsRunning());

    /* A failed init may be retried before the transfer starts. */
    HalMock_SetAudioInitResult(true);
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioSubmits(), 1u);
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

/* The boundary lands on the first timer read of the request, before the
 * write is announced: the request must fail without storing a sample. */
static void test_boundary_before_write_stores_nothing(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    FillPcm(50);
    HalMock_BoundaryOnTickRead(1u);
    CHECK(AudioTx_Request(HALF_A, ev.generation, s_pcm) == AUDIO_TX_LATE);
    CHECK(WordsAre(HalfWords(HALF_A), HAL_AUDIO_HALF_WORDS, 0u));
    CHECK(AudioTx_FaultMask() & AUDIO_TX_FAULT_OVERLAP);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_IsRunning());
}

/* A fault latched while the copy runs (here an interface error on the timer
 * read after the copy) is reported by the request itself, not as success. */
static void test_fault_during_write_is_reported(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_Boundary();
    CHECK(AudioTx_Advance(&ev));
    FillPcm(60);
    HalMock_SaiErrorOnTickRead(2u);
    CHECK(AudioTx_Request(HALF_A, ev.generation, s_pcm) == AUDIO_TX_LATE);
    CHECK_EQ_U(AudioTx_FaultMask(), AUDIO_TX_FAULT_SAI);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_IsRunning());
    CHECK(AudioTx_Request(HALF_B, ev.generation, NULL) == AUDIO_TX_REJECTED);
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

/* Without boundaries the event stream would stay silent while the driver
 * reports itself running. More than AUDIO_TX_STALL_TICKS without a boundary
 * is a fault, counted from Start and then from the last boundary. */
static void test_missing_boundary_after_start_is_a_stall(void)
{
    AudioTxEvent ev;

    Fresh();
    HalMock_SetTicks(1000u);
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    HalMock_SetTicks(1000u + AUDIO_TX_STALL_TICKS);
    CHECK(!AudioTx_Advance(&ev));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    CHECK(AudioTx_IsRunning());

    HalMock_SetTicks(1000u + AUDIO_TX_STALL_TICKS + 1u);
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
    CHECK_EQ_U(ev.faultMask, AUDIO_TX_FAULT_STALL);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
    CHECK(!AudioTx_IsRunning());
    CHECK(AudioTx_Request(HALF_A, 0u, NULL) == AUDIO_TX_REJECTED);

    /* Latched like every other fault. */
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(ev.faultMask, AUDIO_TX_FAULT_STALL);
    CHECK_EQ_U(HalMock_AudioAbortCalls(), 1u);
}

static void test_missing_boundary_after_boundaries_is_a_stall(void)
{
    AudioTxEvent ev;

    StartFresh();
    HalMock_SetTicks(17u);
    BoundaryAndFill(HALF_A, 1u);
    HalMock_SetTicks(34u);
    BoundaryAndFill(HALF_B, 2u);
    HalMock_SetTicks(34u + AUDIO_TX_STALL_TICKS);
    CHECK(!AudioTx_Advance(&ev));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    HalMock_SetTicks(34u + AUDIO_TX_STALL_TICKS + 1u);
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
    CHECK_EQ_U(ev.faultMask, AUDIO_TX_FAULT_STALL);
    CHECK_EQ_U(ev.generation, 2u);
    CHECK_EQ_U(ev.boundaryTick, 34u);
}

static void test_stall_age_survives_tick_wrap(void)
{
    AudioTxEvent ev;

    Fresh();
    HalMock_SetTicks(0xFFFFFFF0u);
    CHECK(AudioTx_Init());
    CHECK(AudioTx_Start());
    HalMock_SetTicks(0x20u);           /* 48 ticks later */
    CHECK(!AudioTx_Advance(&ev));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    HalMock_SetTicks(0x30u);           /* 64 ticks later */
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(ev.faultMask, AUDIO_TX_FAULT_STALL);
}

static void test_no_stall_before_start(void)
{
    AudioTxEvent ev;

    Fresh();
    CHECK(AudioTx_Init());
    HalMock_SetTicks(10u * AUDIO_TX_STALL_TICKS);
    CHECK(!AudioTx_Advance(&ev));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
    CHECK(AudioTx_Start());
    CHECK(!AudioTx_Advance(&ev));
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
}

/* After a fault the HAL keeps its stop latch, so the transmitter cannot be
 * started again. Init must not clear the driver's latch either, or Start
 * would report a transfer that never runs. */
static void test_init_after_fault_is_refused(void)
{
    AudioTxEvent ev;

    StartFresh();
    BoundaryAndFill(HALF_A, 1u);
    HalMock_SaiError();
    CHECK(AudioTx_Advance(&ev));
    CHECK_EQ_U(HalMock_AudioStops(), 1u);

    CHECK(!AudioTx_Init());
    CHECK_EQ_U(HalMock_AudioInitCalls(), 1u);
    CHECK_EQ_U(AudioTx_FaultMask(), AUDIO_TX_FAULT_SAI);
    CHECK(!AudioTx_Start());
    CHECK(!AudioTx_IsRunning());
    CHECK_EQ_U(HalMock_AudioSubmits(), 1u);
    CHECK(AudioTx_Advance(&ev));
    CHECK(ev.kind == AUDIO_TX_EVENT_FAULT);
}

/* Init while the transfer runs would re-run the vendor init under the DMA
 * and disarm the boundary bookkeeping. */
static void test_init_while_running_is_refused(void)
{
    StartFresh();
    BoundaryAndFill(HALF_A, 1u);
    CHECK(!AudioTx_Init());
    CHECK_EQ_U(HalMock_AudioInitCalls(), 1u);
    CHECK(AudioTx_IsRunning());
    BoundaryAndFill(HALF_B, 2u);
    BoundaryAndFill(HALF_A, 3u);
    CHECK_EQ_U(AudioTx_FaultMask(), 0u);
}

/* The HAL refuses to submit (its stop latch is already set): Start must not
 * report a running transfer. */
static void test_start_follows_hal_refusal(void)
{
    AudioTxEvent ev;

    Fresh();
    CHECK(AudioTx_Init());
    HalAudioTx_Abort();
    CHECK(!AudioTx_Start());
    CHECK_EQ_U(HalMock_AudioStartCalls(), 1u);
    CHECK_EQ_U(HalMock_AudioSubmits(), 0u);
    CHECK(!AudioTx_IsRunning());
    CHECK(AudioTx_Request(HALF_A, 0u, NULL) == AUDIO_TX_REJECTED);
    HalMock_SetTicks(10u * AUDIO_TX_STALL_TICKS);
    CHECK(!AudioTx_Advance(&ev));
}

static void test_abort_state_is_readable(void)
{
    AudioTxEvent ev;

    StartFresh();
    CHECK(AudioTx_AbortState() == AUDIO_TX_ABORT_NONE);
    HalMock_SaiError();
    CHECK(AudioTx_AbortState() == AUDIO_TX_ABORT_NONE);   /* the main loop issues the stop */
    CHECK(AudioTx_Advance(&ev));
    CHECK(AudioTx_AbortState() == AUDIO_TX_ABORT_STOPPED);

    /* The vendor driver had already ended the send in error. */
    StartFresh();
    HalMock_SetAbortResult(HAL_AUDIO_TX_ABORT_UNCONFIRMED);
    HalMock_SaiError();
    CHECK(AudioTx_Advance(&ev));
    CHECK(AudioTx_AbortState() == AUDIO_TX_ABORT_UNCONFIRMED);
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
    test_boundary_before_write_stores_nothing();
    test_fault_during_write_is_reported();
    test_fault_latch_rejects_and_aborts_once();
    test_sai_error_is_a_fault();
    test_missing_boundary_after_start_is_a_stall();
    test_missing_boundary_after_boundaries_is_a_stall();
    test_stall_age_survives_tick_wrap();
    test_no_stall_before_start();
    test_init_after_fault_is_refused();
    test_init_while_running_is_refused();
    test_start_follows_hal_refusal();
    test_abort_state_is_readable();
    test_advance_null_is_safe();
    TEST_FINISH();
}
