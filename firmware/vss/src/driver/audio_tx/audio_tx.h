#ifndef AUDIO_TX_H
#define AUDIO_TX_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AUDIO_TX_EVENT_NONE = 0,
    AUDIO_TX_EVENT_HALF_FREE,
    AUDIO_TX_EVENT_FAULT
} AudioTxEventKind;

typedef struct {
    AudioTxEventKind kind;
    uint8_t  freeHalf;       /* 0 = A, 1 = B; valid for HALF_FREE */
    uint32_t generation;     /* count of half-buffer boundaries so far */
    uint32_t boundaryTick;   /* HalTimer tick captured at the boundary */
    uint32_t faultMask;      /* valid for FAULT */
} AudioTxEvent;

/* Fault bits reported by AudioTx_FaultMask() and in FAULT events. */
#define AUDIO_TX_FAULT_LATE    (1UL << 0)  /* a free half was not refilled before the DMA reached it */
#define AUDIO_TX_FAULT_OVERLAP (1UL << 1)  /* a boundary occurred while a half was being written */
#define AUDIO_TX_FAULT_SAI     (1UL << 2)  /* the serial audio interface reported an error */

typedef enum {
    AUDIO_TX_OK = 0,
    AUDIO_TX_LATE,
    AUDIO_TX_REJECTED
} AudioTxResult;

/* Prepares the ring with silence in both halves and initializes the
 * transmit path. Called once at boot. */
bool AudioTx_Init(void);

bool AudioTx_Start(void);

/* Returns true and fills ev when a boundary event or a fault is pending.
 * Each boundary is reported once. After a fault every call returns FAULT. */
bool AudioTx_Advance(AudioTxEvent *ev);

/* Copies one half's worth of PCM (HAL_AUDIO_HALF_WORDS words) into the ring.
 * pcm == NULL writes silence. The request is rejected unless generation is
 * the current one and the half is the free half reported for it, not yet
 * written. AUDIO_TX_LATE: a boundary occurred during the write; the fault is
 * latched. A fault, once latched, stops the transfer and rejects every later
 * request. */
AudioTxResult AudioTx_Request(uint8_t half, uint32_t generation, const int16_t *pcm);

bool     AudioTx_IsRunning(void);

/* 0 while healthy. A set bit is latched until reset. */
uint32_t AudioTx_FaultMask(void);

#endif /* AUDIO_TX_H */
