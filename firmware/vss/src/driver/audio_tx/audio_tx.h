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

typedef enum {
    AUDIO_TX_OK = 0,
    AUDIO_TX_LATE,
    AUDIO_TX_REJECTED
} AudioTxResult;

/* Prepares the ring with silence in both halves. */
bool AudioTx_Init(void);

bool AudioTx_Start(void);

/* Returns true and fills ev when a boundary event or a fault is pending. */
bool AudioTx_Advance(AudioTxEvent *ev);

/* Copies one half's worth of PCM (HAL_AUDIO_HALF_WORDS words) into the ring.
 * pcm == NULL writes silence. The request is rejected unless generation is
 * the current one and the half is not the one being transmitted. */
AudioTxResult AudioTx_Request(uint8_t half, uint32_t generation, const int16_t *pcm);

bool     AudioTx_IsRunning(void);

/* 0 while healthy. A set bit is latched until reset. */
uint32_t AudioTx_FaultMask(void);

#endif /* AUDIO_TX_H */
