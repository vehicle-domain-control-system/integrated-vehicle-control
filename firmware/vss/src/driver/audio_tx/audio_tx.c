#include "audio_tx.h"
#include "../../hal/include/hal_audio_tx.h"
#include "../../hal/include/hal_timer.h"

#include <stddef.h>

#define AUDIO_TX_HALF_A     0U
#define AUDIO_TX_HALF_B     1U
#define AUDIO_TX_HALF_NONE  0xFFU
#define AUDIO_TX_RING_WORDS (HAL_AUDIO_PRIME_WORDS + (2U * HAL_AUDIO_HALF_WORDS))

/* The DMA plays the prime area, then A, then B, then A again (the source
 * address rewinds at the end of the major loop). It interrupts when A ends
 * (half of the major loop) and when B ends (end of the major loop).
 *
 * The boundary and error interrupts share one priority, so they never
 * preempt each other. Each variable has one writer per context: s_isrFault
 * only in interrupt context, s_mainFault only in the main loop, so neither
 * needs a read-modify-write that the other side could interleave with. */

/* Written in interrupt context (and by Start before the transfer runs). */
static volatile uint8_t  s_phase;        /* 0: next boundary ends A, 1: ends B */
static volatile uint8_t  s_activeHalf;   /* half the DMA is reading */
static volatile uint32_t s_generation;   /* boundaries since Start */
static volatile uint32_t s_boundaryTick;
static volatile uint32_t s_isrFault;

/* Set when the DMA leaves a half; cleared when the main loop has written it
 * (or by the interrupt when the DMA comes back to it unwritten). */
static volatile uint8_t  s_free[2];

/* Written in the main loop. */
static volatile uint8_t  s_writingHalf;
static volatile uint32_t s_mainFault;
static volatile bool     s_started;
static uint32_t s_reportedGeneration;
static uint16_t *s_ring;
static bool s_initOk;
static bool s_running;
static bool s_abortIssued;

/* Longest write of one half, in timer ticks (diagnostic). */
static volatile uint32_t s_writeMaxTicks;

static void AudioTx_OnBoundaryIsr(void)
{
    uint8_t ended;
    uint8_t next;
    uint32_t fault = 0U;

    if (!s_started)
    {
        return;
    }
    ended = (s_phase == 0U) ? AUDIO_TX_HALF_A : AUDIO_TX_HALF_B;
    next = (ended == AUDIO_TX_HALF_A) ? AUDIO_TX_HALF_B : AUDIO_TX_HALF_A;

    /* The DMA now reads 'next'. It was released at the previous boundary;
     * still being free means it was never refilled and stale samples play. */
    if (s_free[next] != 0U)
    {
        s_free[next] = 0U;
        fault |= AUDIO_TX_FAULT_LATE;
    }
    /* The main loop is still copying into the half the DMA started reading. */
    if (s_writingHalf == next)
    {
        fault |= AUDIO_TX_FAULT_OVERLAP;
    }
    if (fault != 0U)
    {
        s_isrFault |= fault;
    }

    s_activeHalf = next;
    s_free[ended] = 1U;
    s_phase = (uint8_t)(s_phase ^ 1U);
    s_boundaryTick = HalTimer_GetTicks();
    s_generation++;
}

static void AudioTx_OnErrorIsr(void)
{
    s_isrFault |= AUDIO_TX_FAULT_SAI;
}

/* Main context only. Stops the transfer once; never restarted until reset. */
static void AudioTx_FailClosed(void)
{
    if (!s_abortIssued)
    {
        s_abortIssued = true;
        HalAudioTx_Abort();
    }
    s_running = false;
}

static AudioTxResult AudioTx_FailWrite(void)
{
    s_mainFault |= AUDIO_TX_FAULT_OVERLAP;
    s_writingHalf = AUDIO_TX_HALF_NONE;
    AudioTx_FailClosed();
    return AUDIO_TX_LATE;
}

/* Power-on state of every driver variable. */
static void AudioTx_ClearState(void)
{
    s_started = false;
    s_running = false;
    s_abortIssued = false;
    s_isrFault = 0U;
    s_mainFault = 0U;
    s_phase = 0U;
    s_activeHalf = AUDIO_TX_HALF_A;
    s_free[AUDIO_TX_HALF_A] = 0U;
    s_free[AUDIO_TX_HALF_B] = 0U;
    s_writingHalf = AUDIO_TX_HALF_NONE;
    s_generation = 0U;
    s_reportedGeneration = 0U;
    s_boundaryTick = 0U;
    s_writeMaxTicks = 0U;
    s_ring = NULL;
    s_initOk = false;
}

/* Main context only. More than AUDIO_TX_STALL_TICKS since the last boundary
 * (or since Start) means the DMA no longer moves: a stopped bit clock or a
 * submission that never happened raises no error of its own. */
static void AudioTx_CheckStall(void)
{
    uint32_t last;

    if (!s_running || (AudioTx_FaultMask() != 0U))
    {
        return;
    }
    /* The boundary tick is read before the clock. A boundary in between
     * leaves the previous tick here, which is at most one half older, so
     * the age never goes negative. */
    last = s_boundaryTick;
    if ((HalTimer_GetTicks() - last) > AUDIO_TX_STALL_TICKS)
    {
        s_mainFault |= AUDIO_TX_FAULT_STALL;
    }
}

bool AudioTx_Init(void)
{
    uint16_t *ring;
    uint32_t i;

    /* Once started, the vendor init would run again under the moving DMA and
     * reset the ring controls. Once stopped, the transmit HAL refuses any new
     * submission; clearing the fault latch here would let Start report a
     * transfer that never runs. Both last until reset. */
    if (s_started || s_abortIssued)
    {
        return false;
    }
    AudioTx_ClearState();
    ring = HalAudioTx_Ring();
    s_ring = ring;

    if (ring == NULL)
    {
        return false;
    }
    /* Prime area and both halves start as silence. */
    for (i = 0U; i < AUDIO_TX_RING_WORDS; ++i)
    {
        ring[i] = 0U;
    }
    s_initOk = HalAudioTx_Init(AudioTx_OnBoundaryIsr, AudioTx_OnErrorIsr);
    return s_initOk;
}

bool AudioTx_Start(void)
{
    if (!s_initOk || s_started || (AudioTx_FaultMask() != 0U))
    {
        return false;
    }
    /* Both halves already hold silence, so neither is free at the start. */
    s_phase = 0U;
    s_activeHalf = AUDIO_TX_HALF_A;
    s_free[AUDIO_TX_HALF_A] = 0U;
    s_free[AUDIO_TX_HALF_B] = 0U;
    s_writingHalf = AUDIO_TX_HALF_NONE;
    s_generation = 0U;
    s_reportedGeneration = 0U;
    s_boundaryTick = HalTimer_GetTicks();
    s_running = true;
    s_started = true;
    if (!HalAudioTx_Start())
    {
        /* Nothing was submitted, so no boundary will follow. */
        s_started = false;
        s_running = false;
        return false;
    }
    return true;
}

bool AudioTx_Advance(AudioTxEvent *ev)
{
    uint32_t fault;
    uint32_t generation;
    uint32_t tick;
    uint8_t active;

    if (ev == NULL)
    {
        return false;
    }
    ev->kind = AUDIO_TX_EVENT_NONE;
    ev->freeHalf = 0U;
    ev->generation = 0U;
    ev->boundaryTick = 0U;
    ev->faultMask = 0U;

    AudioTx_CheckStall();
    fault = AudioTx_FaultMask();
    if (fault != 0U)
    {
        AudioTx_FailClosed();
        ev->kind = AUDIO_TX_EVENT_FAULT;
        ev->faultMask = fault;
        ev->generation = s_generation;
        ev->boundaryTick = s_boundaryTick;
        return true;
    }
    if (!s_running)
    {
        return false;
    }

    /* Consistent snapshot: retry if a boundary arrived while reading. */
    do
    {
        generation = s_generation;
        active = s_activeHalf;
        tick = s_boundaryTick;
    } while (generation != s_generation);

    if (generation == s_reportedGeneration)
    {
        return false;
    }
    s_reportedGeneration = generation;
    ev->kind = AUDIO_TX_EVENT_HALF_FREE;
    ev->freeHalf = (active == AUDIO_TX_HALF_A) ? AUDIO_TX_HALF_B : AUDIO_TX_HALF_A;
    ev->generation = generation;
    ev->boundaryTick = tick;
    return true;
}

AudioTxResult AudioTx_Request(uint8_t half, uint32_t generation, const int16_t *pcm)
{
    volatile uint16_t *dst;
    uint32_t t0;
    uint32_t elapsed;
    uint32_t i;

    if (AudioTx_FaultMask() != 0U)
    {
        AudioTx_FailClosed();
        return AUDIO_TX_REJECTED;
    }
    if (!s_running || (half > AUDIO_TX_HALF_B) || (generation != s_generation) ||
        (half == s_activeHalf) || (s_free[half] == 0U))
    {
        return AUDIO_TX_REJECTED;
    }

    /* Read before the write is announced, so a boundary up to here is caught
     * by the check below before any sample is stored. */
    t0 = HalTimer_GetTicks();

    /* From here the interrupt treats a boundary into this half as overlap. */
    s_writingHalf = half;
    if (generation != s_generation)
    {
        return AudioTx_FailWrite();
    }

    /* The source is a CPU-owned stage filled beforehand; only this copy
     * touches the ring. The volatile destination keeps every store ahead of
     * the generation check that follows. */
    dst = &s_ring[HAL_AUDIO_PRIME_WORDS + ((uint32_t)half * HAL_AUDIO_HALF_WORDS)];
    if (pcm != NULL)
    {
        for (i = 0U; i < HAL_AUDIO_HALF_WORDS; ++i)
        {
            dst[i] = (uint16_t)pcm[i];
        }
    }
    else
    {
        for (i = 0U; i < HAL_AUDIO_HALF_WORDS; ++i)
        {
            dst[i] = 0U;
        }
    }
    elapsed = HalTimer_GetTicks() - t0;
    if (elapsed > s_writeMaxTicks)
    {
        s_writeMaxTicks = elapsed;
    }

    /* A boundary during the copy means the DMA may have read a torn half. */
    if (generation != s_generation)
    {
        return AudioTx_FailWrite();
    }
    s_free[half] = 0U;
    s_writingHalf = AUDIO_TX_HALF_NONE;

    /* A fault latched while this request ran is reported here rather than
     * as success: an interface error, or a boundary right after the check
     * above, which still saw the half as being written (the samples are
     * complete then, but the latch already stops the transfer). */
    if (AudioTx_FaultMask() != 0U)
    {
        AudioTx_FailClosed();
        return AUDIO_TX_LATE;
    }
    return AUDIO_TX_OK;
}

bool AudioTx_IsRunning(void)
{
    return s_running && (AudioTx_FaultMask() == 0U);
}

uint32_t AudioTx_FaultMask(void)
{
    return s_isrFault | s_mainFault;
}

AudioTxAbortState AudioTx_AbortState(void)
{
    if (!s_abortIssued)
    {
        return AUDIO_TX_ABORT_NONE;
    }
    return (HalAudioTx_AbortState() == HAL_AUDIO_TX_ABORT_STOPPED) ? AUDIO_TX_ABORT_STOPPED
                                                                  : AUDIO_TX_ABORT_UNCONFIRMED;
}

#if VSS_CFG_HOST_TEST
void AudioTx_ResetForTest(void)
{
    AudioTx_ClearState();
}
#endif
