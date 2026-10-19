#ifndef HAL_AUDIO_TX_H
#define HAL_AUDIO_TX_H

#include <stdbool.h>
#include <stdint.h>

/* Ring layout: [prime 8 words][half A][half B], 16-bit words, stereo
 * interleaved, 1024 frames per half. The ring lives in non-cacheable memory
 * and starts on a 32-byte boundary. The prime area is one transmit FIFO
 * fill. */
#define HAL_AUDIO_PRIME_WORDS 8u
#define HAL_AUDIO_HALF_WORDS  2048u

typedef void (*HalAudioTxIsrFn)(void);

/* Result of the transfer stop, for diagnostics. */
typedef enum {
    HAL_AUDIO_TX_ABORT_NONE = 0,      /* not stopped */
    HAL_AUDIO_TX_ABORT_STOPPED,       /* the vendor driver reports the send as aborted */
    HAL_AUDIO_TX_ABORT_UNCONFIRMED    /* it reports another state (after a DMA error the send
                                       * has already ended in error); the transmitter may
                                       * still be enabled */
} HalAudioTxAbortState;

uint16_t *HalAudioTx_Ring(void);

/* Configures the DMA channel and the serial audio interface. onBoundary runs
 * in interrupt context each time the DMA has moved the last word of one half
 * into the transmit FIFO: that half may be refilled from then on. Its last
 * HAL_AUDIO_PRIME_WORDS words (one FIFO fill, about 83 us) are still to be
 * shifted out at that moment. onError runs in interrupt context when the
 * interface reports an error. Either may be NULL. */
bool HalAudioTx_Init(HalAudioTxIsrFn onBoundary, HalAudioTxIsrFn onError);

/* Submits the whole ring once; the DMA then cycles over both halves. Returns
 * false without submitting when the transfer was already started or stopped,
 * or when the last Init failed. */
bool HalAudioTx_Start(void);

/* Stops the transfer. Only the first call acts; the transmitter is not
 * restarted afterwards. */
void HalAudioTx_Abort(void);

HalAudioTxAbortState HalAudioTx_AbortState(void);

#endif /* HAL_AUDIO_TX_H */
