#ifndef MP3_PCM_PROVIDER_H
#define MP3_PCM_PROVIDER_H

#include "Std_Types.h"
#include "AudioAsset.h"

/* VSS integration: one existing DMA half at most; no additional PCM buffer. */
#define MP3_PCM_PROVIDER_MAX_RENDER_FRAMES 1024U

/*
 * Stage 13B — MP3 -> PCM provider.
 *
 * The provider no longer owns one hard-coded Sound ID.
 * A Sound Asset Descriptor selects:
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
    boolean loopEnabled
);

void Mp3PcmProvider_Reset(void);

void Mp3PcmProvider_RenderStereo16(
    uint16 *buffer,
    uint32 frameCount
);

/* Decoded frames actually copied by the last call, excluding zero-fill.
 * This is decoder evidence only, never an audible/physical completion fact. */
uint32 Mp3PcmProvider_GetLastRenderValidFrames(void);

boolean Mp3PcmProvider_WasLastRenderTerminal(
    void
);


/* =========================================================
 * Debug / Observability
 * ========================================================= */
extern volatile boolean g_mp3ProviderValid;
extern volatile boolean g_mp3ProviderEof;
extern volatile boolean g_mp3LoopEnabled;
extern volatile boolean g_mp3RenderedTerminalBuffer;

extern volatile uint8 g_mp3ActiveAssetId;

extern volatile uint32 g_mp3AssetSizeBytes;
extern volatile uint32 g_mp3AssetOffset;

extern volatile uint32 g_mp3InputReadCount;
extern volatile uint32 g_mp3InputBytesRead;

extern volatile uint32 g_mp3DecodeCallCount;
extern volatile uint32 g_mp3DecodedFrameCount;
extern volatile uint32 g_mp3ZeroSampleFrameCount;
extern volatile uint32 g_mp3DecodeErrorCount;
extern volatile uint32 g_mp3LoopCount;

extern volatile uint32 g_mp3OutputRenderCount;
extern volatile uint32 g_mp3OutputFrameCount;

extern volatile uint32 g_mp3LastFrameBytes;
extern volatile uint32 g_mp3LastFrameOffset;
extern volatile uint32 g_mp3LastSamplesPerChannel;

extern volatile uint32 g_mp3SampleRateHz;
extern volatile uint32 g_mp3Channels;
extern volatile uint32 g_mp3BitrateKbps;
extern volatile uint32 g_mp3Layer;

#endif /* MP3_PCM_PROVIDER_H */
