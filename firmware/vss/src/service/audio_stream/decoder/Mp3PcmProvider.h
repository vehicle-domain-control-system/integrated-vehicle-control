#ifndef MP3_PCM_PROVIDER_H
#define MP3_PCM_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "../../asset/audio_asset.h"

/* Largest render: one output half-buffer. */
#define MP3_PCM_PROVIDER_MAX_RENDER_FRAMES 1024U

/*
 * MP3 -> PCM provider.
 *
 * The caller selects:
 *
 *   Asset ID
 *   one-shot / loop
 *
 * The provider only performs:
 *
 *   compressed MP3 bytes
 *   -> decoded 48 kHz / 16-bit PCM
 */

void Mp3PcmProvider_Init(void);

void Mp3PcmProvider_SelectAsset(
    AudioAsset_IdType assetId,
    bool loopEnabled
);

void Mp3PcmProvider_Reset(void);

/* Fills frameCount stereo frames (frameCount <= MP3_PCM_PROVIDER_MAX_RENDER_FRAMES);
 * mono is duplicated to both channels. After EOF or a decode failure the
 * rest of the buffer is silence. */
void Mp3PcmProvider_RenderStereo16(
    uint16_t *buffer,
    uint32_t frameCount
);

/* The last render held the final decoded PCM sample of a one-shot asset. */
bool Mp3PcmProvider_WasLastRenderTerminal(
    void
);

/* Actual decoded PCM frames from the most recent render. This excludes
 * zero-fill after EOF or decode failure; zero-valued decoded samples still
 * count as valid frames. Caller must consume it before the next render. */
uint32_t Mp3PcmProvider_LastRenderValidFrames(void);

/* The selected asset exists and no decode failure has occurred since Reset. */
bool Mp3PcmProvider_IsValid(void);

/* A one-shot asset has no more frames to decode. */
bool Mp3PcmProvider_IsEof(void);

/* Read and decode failures since Reset (a missing asset counts as one). */
uint32_t Mp3PcmProvider_DecodeErrorCount(void);

/* Format of the most recently decoded frame (0 before the first one). */
uint32_t Mp3PcmProvider_LastSampleRateHz(void);
uint32_t Mp3PcmProvider_LastChannels(void);

#endif /* MP3_PCM_PROVIDER_H */
