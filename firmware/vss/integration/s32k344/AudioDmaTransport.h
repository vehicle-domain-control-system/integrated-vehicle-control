#ifndef AUDIO_DMA_TRANSPORT_H
#define AUDIO_DMA_TRANSPORT_H

#include "Std_Types.h"

/*
 * Hardware-continuous SAI/eDMA transport.
 *
 * Responsibility:
 *   - own the non-cacheable prime + A/B PCM transport memory
 *   - initialize the generated DMAMUX/eDMA path and SAI TX path
 *   - apply the verified continuous TCD controls
 *   - handle HALF/MAJOR ownership events
 *   - refill only the DMA-free half in main context
 *   - retain SAI/eDMA transport diagnostics
 *
 * PCM providers, MP3/WAV decoding, playlist policy, VSS arbitration,
 * codec setup, and audio-clock setup remain outside this module.
 *
 * The public contract intentionally avoids RTD/vendor types. Existing
 * debugger-facing g_dma_* / g_sai_* / g_audioError symbols remain global in
 * the .c file and therefore still appear in the ELF / S32DS Expressions view.
 */

void AudioDmaTransport_PrimePlayerBuffers(void);
void AudioDmaTransport_InitDmaPath(void);
void AudioDmaTransport_InitSaiPath(void);
void AudioDmaTransport_Start(void);
void AudioDmaTransport_Service(void);

boolean AudioDmaTransport_IsDmaReady(void);
boolean AudioDmaTransport_IsSaiReady(void);
boolean AudioDmaTransport_IsStarted(void);
boolean AudioDmaTransport_HasError(void);

#endif /* AUDIO_DMA_TRANSPORT_H */
