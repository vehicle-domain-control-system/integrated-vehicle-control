#ifndef AUDIO_TX_H
#define AUDIO_TX_H

#include <stdbool.h>
#include <stdint.h>

#include "../../infra/vss_config.h"

/* One half-buffer: AUDIO_TX_HALF_FRAMES stereo frames of interleaved int16
 * samples (left, right), 21.3 ms at 48 kHz. */
#define AUDIO_TX_HALF_FRAMES 1024u
#define AUDIO_TX_HALF_WORDS  (2u * AUDIO_TX_HALF_FRAMES)

typedef enum {
    AUDIO_TX_EVENT_NONE = 0,
    AUDIO_TX_EVENT_HALF_FREE,
    AUDIO_TX_EVENT_FAULT
} AudioTxEventKind;

typedef struct {
    AudioTxEventKind kind;
    uint8_t  freeHalf;       /* 0 = A, 1 = B; valid for HALF_FREE */
    uint32_t generation;     /* count of half-buffer boundaries so far */
    uint32_t boundaryTick;   /* timer tick captured at the boundary (VSS_TIME_TICK_HZ) */
    uint32_t faultMask;      /* valid for FAULT */
} AudioTxEvent;

/* Fault bits reported by AudioTx_FaultMask() and in FAULT events. */
#define AUDIO_TX_FAULT_LATE    (1UL << 0)  /* a free half was not refilled before the DMA reached it */
#define AUDIO_TX_FAULT_OVERLAP (1UL << 1)  /* a boundary occurred while a half was being written */
#define AUDIO_TX_FAULT_SAI     (1UL << 2)  /* the serial audio interface reported an error */
#define AUDIO_TX_FAULT_STALL   (1UL << 3)  /* no boundary for more than AUDIO_TX_STALL_TICKS while running */

/* Longest time without a boundary while running, in timer ticks: three
 * half-buffers (64 ms) at the 800 Hz tick, rounded up. A half lasts about
 * 17 ticks; the first boundary after Start comes after the prime words and
 * half A, about 17 ticks as well. */
#define AUDIO_TX_STALL_TICKS   52u

typedef enum {
    AUDIO_TX_OK = 0,
    AUDIO_TX_LATE,
    AUDIO_TX_REJECTED
} AudioTxResult;

/* Result of the one transfer stop issued after a fault, for diagnostics. */
typedef enum {
    AUDIO_TX_ABORT_NONE = 0,      /* no stop issued */
    AUDIO_TX_ABORT_STOPPED,       /* the transmit HAL reports the transfer as aborted */
    AUDIO_TX_ABORT_UNCONFIRMED    /* it does not; the transmitter may still be enabled and only
                                   * the codec mute keeps the output silent */
} AudioTxAbortState;

/* Prepares the ring with silence in both halves and initializes the
 * transmit path. Called once at boot. Once Start has run, or the transfer
 * has been stopped after a fault, it returns false and changes nothing: the
 * transfer and the fault latch stay as they are until reset. */
bool AudioTx_Init(void);

/* Starts the transfer once. False when Init did not succeed, the transfer
 * was already started, a fault is latched, or the transmit HAL refused to
 * submit the ring. */
bool AudioTx_Start(void);

/* Returns true and fills ev when a boundary event or a fault is pending.
 * Each boundary is reported once. A missing boundary (AUDIO_TX_FAULT_STALL)
 * is detected here. After a fault every call returns FAULT. */
bool AudioTx_Advance(AudioTxEvent *ev);

/* Copies one half's worth of PCM (AUDIO_TX_HALF_WORDS words) into the ring.
 * pcm == NULL writes silence. The request is rejected unless generation is
 * the current one and the half is the free half reported for it, not yet
 * written. AUDIO_TX_LATE: a boundary occurred during the write, or a fault
 * was latched while it ran. A fault, once latched, stops the transfer and
 * rejects every later request. */
AudioTxResult AudioTx_Request(uint8_t half, uint32_t generation, const int16_t *pcm);

bool     AudioTx_IsRunning(void);

/* 0 while healthy. A set bit is latched until reset. */
uint32_t AudioTx_FaultMask(void);

AudioTxAbortState AudioTx_AbortState(void);

#if VSS_CFG_HOST_TEST
/* Host tests only: returns the driver to its power-on state so that each
 * case starts from Init. Not built for the target. */
void AudioTx_ResetForTest(void);
#endif

#endif /* AUDIO_TX_H */
