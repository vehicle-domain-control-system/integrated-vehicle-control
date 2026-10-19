#ifndef HAL_AUDIO_TX_H
#define HAL_AUDIO_TX_H

#include <stdbool.h>
#include <stdint.h>

/* Ring layout: [prime 8 words][half A][half B], 16-bit words, stereo
 * interleaved, 1024 frames per half. The ring lives in non-cacheable memory
 * and starts on a 32-byte boundary. */
#define HAL_AUDIO_PRIME_WORDS 8u
#define HAL_AUDIO_HALF_WORDS  2048u

typedef void (*HalAudioTxIsrFn)(void);

uint16_t *HalAudioTx_Ring(void);

/* Configures the DMA channel and the serial audio interface. onBoundary runs
 * in interrupt context each time one half has been fully transmitted;
 * onError runs there when the interface reports an error. Either may be
 * NULL. */
bool HalAudioTx_Init(HalAudioTxIsrFn onBoundary, HalAudioTxIsrFn onError);

/* Submits the whole ring once; the DMA then cycles over both halves. */
void HalAudioTx_Start(void);

/* Stops the transfer. Meant to be called once; the transmitter is not
 * restarted afterwards. */
void HalAudioTx_Abort(void);

#endif /* HAL_AUDIO_TX_H */
