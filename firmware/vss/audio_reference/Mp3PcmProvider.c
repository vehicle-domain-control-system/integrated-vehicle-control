#include "Mp3PcmProvider.h"

#include <stdint.h>

#include "AudioAsset.h"
#include "minimp3.h"


/* =========================================================
 * Stage 13 configuration
 * ========================================================= */
#define MP3_EXPECTED_SAMPLE_RATE_HZ   48000U

/*
 * 4 KB compressed input window.
 *
 * The whole MP3 file is NOT copied to SRAM.
 * We re-read a small window from the current compressed
 * offset for each minimp3 frame decode.
 */
#define MP3_INPUT_WINDOW_BYTES        4096U

/*
 * Protect against a malformed file causing an endless
 * zero-progress decode loop inside one PCM render.
 */
#define MP3_MAX_DECODE_ATTEMPTS       64U


/* =========================================================
 * Decoder state
 * ========================================================= */
static mp3dec_t s_mp3Decoder;

static uint8_t s_mp3InputWindow[
    MP3_INPUT_WINDOW_BYTES
];

static mp3d_sample_t s_mp3DecodedFrame[
    MINIMP3_MAX_SAMPLES_PER_FRAME
];

static AudioAsset_IdType s_mp3AssetId =
    AUDIO_ASSET_ID_MP3_TEST;

static uint32 s_mp3AssetSize = 0U;
static uint32 s_mp3Offset = 0U;

static boolean s_mp3TerminalReported = FALSE;

static uint32 s_decodedSamplesPerChannel = 0U;
static uint32 s_decodedChannels = 0U;
static uint32 s_decodedReadFrame = 0U;
static uint32 s_lastRenderValidFrames = 0U;


/* =========================================================
 * Debug / Observability
 * ========================================================= */
volatile boolean g_mp3ProviderValid = FALSE;
volatile boolean g_mp3ProviderEof = FALSE;
volatile boolean g_mp3LoopEnabled = TRUE;
volatile boolean g_mp3RenderedTerminalBuffer = FALSE;

volatile uint8 g_mp3ActiveAssetId =
    AUDIO_ASSET_ID_MP3_TEST;

volatile uint32 g_mp3AssetSizeBytes = 0U;
volatile uint32 g_mp3AssetOffset = 0U;

volatile uint32 g_mp3InputReadCount = 0U;
volatile uint32 g_mp3InputBytesRead = 0U;

volatile uint32 g_mp3DecodeCallCount = 0U;
volatile uint32 g_mp3DecodedFrameCount = 0U;
volatile uint32 g_mp3ZeroSampleFrameCount = 0U;
volatile uint32 g_mp3DecodeErrorCount = 0U;
volatile uint32 g_mp3LoopCount = 0U;

volatile uint32 g_mp3OutputRenderCount = 0U;
volatile uint32 g_mp3OutputFrameCount = 0U;

volatile uint32 g_mp3LastFrameBytes = 0U;
volatile uint32 g_mp3LastFrameOffset = 0U;
volatile uint32 g_mp3LastSamplesPerChannel = 0U;

volatile uint32 g_mp3SampleRateHz = 0U;
volatile uint32 g_mp3Channels = 0U;
volatile uint32 g_mp3BitrateKbps = 0U;
volatile uint32 g_mp3Layer = 0U;


/* =========================================================
 * Internal helpers
 * ========================================================= */
static void Mp3PcmProvider_ClearDecodedFrame(void)
{
    s_decodedSamplesPerChannel =
        0U;

    s_decodedChannels =
        0U;

    s_decodedReadFrame =
        0U;
}


static boolean Mp3PcmProvider_RestartStream(void)
{
    if (g_mp3LoopEnabled == FALSE)
    {
        g_mp3ProviderEof =
            TRUE;

        return FALSE;
    }

    s_mp3Offset =
        0U;

    g_mp3AssetOffset =
        0U;

    Mp3PcmProvider_ClearDecodedFrame();

    mp3dec_init(
        &s_mp3Decoder
    );

    g_mp3LoopCount++;

    return TRUE;
}


static boolean Mp3PcmProvider_ReadCurrentWindow(
    uint32 *outBytesRead
)
{
    AudioAsset_StatusType status;
    uint32 remaining;
    uint32 requestBytes;
    uint32 bytesRead;

    if (outBytesRead == NULL_PTR)
    {
        g_mp3DecodeErrorCount++;
        return FALSE;
    }

    *outBytesRead =
        0U;

    if (s_mp3Offset >= s_mp3AssetSize)
    {
        return FALSE;
    }

    remaining =
        s_mp3AssetSize
        -
        s_mp3Offset;

    requestBytes =
        remaining;

    if (
        requestBytes
        >
        MP3_INPUT_WINDOW_BYTES
    )
    {
        requestBytes =
            MP3_INPUT_WINDOW_BYTES;
    }

    bytesRead =
        0U;

    status =
        AudioAsset_Read(
            s_mp3AssetId,
            s_mp3Offset,
            s_mp3InputWindow,
            requestBytes,
            &bytesRead
        );

    if (
        (status != AUDIO_ASSET_STATUS_OK)
        ||
        (bytesRead != requestBytes)
    )
    {
        g_mp3DecodeErrorCount++;
        return FALSE;
    }

    g_mp3InputReadCount++;
    g_mp3InputBytesRead += bytesRead;

    *outBytesRead =
        bytesRead;

    return TRUE;
}


/*
 * Decode one MP3 frame into s_mp3DecodedFrame.
 *
 * Returns TRUE when decoded PCM is ready.
 * Returns FALSE on EOF or fatal format/read failure.
 */
static boolean Mp3PcmProvider_DecodeNextFrame(void)
{
    uint32 attempt;

    for (
        attempt = 0U;
        attempt < MP3_MAX_DECODE_ATTEMPTS;
        attempt++
    )
    {
        uint32 inputBytes;
        mp3dec_frame_info_t info;
        int samplesPerChannel;

        if (s_mp3Offset >= s_mp3AssetSize)
        {
            if (
                Mp3PcmProvider_RestartStream()
                ==
                FALSE
            )
            {
                return FALSE;
            }
        }

        inputBytes =
            0U;

        if (
            Mp3PcmProvider_ReadCurrentWindow(
                &inputBytes
            )
            ==
            FALSE
        )
        {
            g_mp3ProviderValid =
                FALSE;

            return FALSE;
        }

        info.frame_bytes =
            0;

        info.frame_offset =
            0;

        info.channels =
            0;

        info.hz =
            0;

        info.layer =
            0;

        info.bitrate_kbps =
            0;

        g_mp3DecodeCallCount++;

        samplesPerChannel =
            mp3dec_decode_frame(
                &s_mp3Decoder,
                s_mp3InputWindow,
                (int)inputBytes,
                s_mp3DecodedFrame,
                &info
            );

        if (info.frame_bytes > 0)
        {
            s_mp3Offset +=
                (uint32)info.frame_bytes;

            if (s_mp3Offset > s_mp3AssetSize)
            {
                s_mp3Offset =
                    s_mp3AssetSize;
            }

            g_mp3AssetOffset =
                s_mp3Offset;

            g_mp3LastFrameBytes =
                (uint32)info.frame_bytes;

            g_mp3LastFrameOffset =
                (uint32)info.frame_offset;
        }

        if (samplesPerChannel > 0)
        {
            if (
                (info.hz != (int)MP3_EXPECTED_SAMPLE_RATE_HZ)
                ||
                (
                    (info.channels != 1)
                    &&
                    (info.channels != 2)
                )
                ||
                (info.layer != 3)
            )
            {
                g_mp3DecodeErrorCount++;
                g_mp3ProviderValid =
                    FALSE;

                return FALSE;
            }

            s_decodedSamplesPerChannel =
                (uint32)samplesPerChannel;

            s_decodedChannels =
                (uint32)info.channels;

            s_decodedReadFrame =
                0U;

            g_mp3LastSamplesPerChannel =
                (uint32)samplesPerChannel;

            g_mp3SampleRateHz =
                (uint32)info.hz;

            g_mp3Channels =
                (uint32)info.channels;

            g_mp3BitrateKbps =
                (uint32)info.bitrate_kbps;

            g_mp3Layer =
                (uint32)info.layer;

            g_mp3DecodedFrameCount++;

            return TRUE;
        }

        /*
         * A frame may legitimately yield no PCM while the
         * Layer III bit reservoir is being established.
         *
         * If minimp3 consumed bytes, continue from the new
         * compressed offset.
         */
        g_mp3ZeroSampleFrameCount++;

        if (info.frame_bytes == 0)
        {
            /*
             * No progress. If only a tiny tail remains, treat
             * it as EOF. Otherwise skip one byte so malformed
             * data cannot lock the super loop forever.
             */
            if (inputBytes <= 4U)
            {
                s_mp3Offset =
                    s_mp3AssetSize;

                g_mp3AssetOffset =
                    s_mp3Offset;
            }
            else
            {
                s_mp3Offset++;

                g_mp3AssetOffset =
                    s_mp3Offset;
            }
        }
    }

    g_mp3DecodeErrorCount++;
    g_mp3ProviderValid =
        FALSE;

    return FALSE;
}


/* =========================================================
 * Public
 * ========================================================= */
void Mp3PcmProvider_Init(void)
{
    s_mp3AssetId =
        AUDIO_ASSET_ID_MP3_TEST;

    g_mp3ActiveAssetId =
        s_mp3AssetId;

    g_mp3LoopEnabled =
        TRUE;

    Mp3PcmProvider_Reset();
}


void Mp3PcmProvider_SelectAsset(
    AudioAsset_IdType assetId,
    boolean loopEnabled
)
{
    s_mp3AssetId =
        assetId;

    g_mp3ActiveAssetId =
        assetId;

    g_mp3LoopEnabled =
        loopEnabled;
}


void Mp3PcmProvider_Reset(void)
{
    AudioAsset_StatusType status;
    uint32 sizeBytes;

    s_lastRenderValidFrames = 0U;

    sizeBytes =
        0U;

    status =
        AudioAsset_GetSize(
            s_mp3AssetId,
            &sizeBytes
        );

    s_mp3AssetSize =
        sizeBytes;

    s_mp3Offset =
        0U;

    Mp3PcmProvider_ClearDecodedFrame();

    mp3dec_init(
        &s_mp3Decoder
    );

    g_mp3AssetSizeBytes =
        sizeBytes;

    g_mp3AssetOffset =
        0U;

    g_mp3InputReadCount =
        0U;

    g_mp3InputBytesRead =
        0U;

    g_mp3DecodeCallCount =
        0U;

    g_mp3DecodedFrameCount =
        0U;

    g_mp3ZeroSampleFrameCount =
        0U;

    g_mp3DecodeErrorCount =
        0U;

    g_mp3LoopCount =
        0U;

    g_mp3OutputRenderCount =
        0U;

    g_mp3OutputFrameCount =
        0U;

    g_mp3LastFrameBytes =
        0U;

    g_mp3LastFrameOffset =
        0U;

    g_mp3LastSamplesPerChannel =
        0U;

    g_mp3SampleRateHz =
        0U;

    g_mp3Channels =
        0U;

    g_mp3BitrateKbps =
        0U;

    g_mp3Layer =
        0U;

    g_mp3ProviderEof =
        FALSE;

    g_mp3RenderedTerminalBuffer =
        FALSE;

    s_mp3TerminalReported =
        FALSE;

    g_mp3ProviderValid =
        (
            (status == AUDIO_ASSET_STATUS_OK)
            &&
            (sizeBytes > 0U)
            &&
            (s_mp3AssetId >= AUDIO_ASSET_ID_MP3_TEST)
            &&
            (s_mp3AssetId < AUDIO_ASSET_COUNT)
        )
        ? TRUE
        : FALSE;

    if (g_mp3ProviderValid == FALSE)
    {
        g_mp3DecodeErrorCount++;
    }
}


void Mp3PcmProvider_RenderStereo16(
    uint16 *buffer,
    uint32 frameCount
)
{
    uint32 outputFrame;

    s_lastRenderValidFrames = 0U;
    g_mp3RenderedTerminalBuffer = FALSE;

    if ((buffer == NULL_PTR) || (frameCount > MP3_PCM_PROVIDER_MAX_RENDER_FRAMES))
    {
        g_mp3DecodeErrorCount++;
        return;
    }

    outputFrame =
        0U;

    g_mp3RenderedTerminalBuffer =
        FALSE;

    while (outputFrame < frameCount)
    {
        sint16 left;
        sint16 right;
        uint32 pcmIndex;

        if (g_mp3ProviderValid == FALSE)
        {
            break;
        }

        if (
            s_decodedReadFrame
            >=
            s_decodedSamplesPerChannel
        )
        {
            if (
                Mp3PcmProvider_DecodeNextFrame()
                ==
                FALSE
            )
            {
                break;
            }
        }

        pcmIndex =
            s_decodedReadFrame
            *
            s_decodedChannels;

        left =
            (sint16)s_mp3DecodedFrame[
                pcmIndex
            ];

        if (s_decodedChannels == 1U)
        {
            right =
                left;
        }
        else
        {
            right =
                (sint16)s_mp3DecodedFrame[
                    pcmIndex + 1U
                ];
        }

        buffer[(outputFrame * 2U)] =
            (uint16)left;

        buffer[(outputFrame * 2U) + 1U] =
            (uint16)right;

        s_decodedReadFrame++;
        outputFrame++;

        if (
            (g_mp3LoopEnabled == FALSE)
            &&
            (s_mp3Offset >= s_mp3AssetSize)
            &&
            (
                s_decodedReadFrame
                >=
                s_decodedSamplesPerChannel
            )
            &&
            (s_mp3TerminalReported == FALSE)
        )
        {
            /*
             * This output buffer contains the final decoded
             * PCM sample of the one-shot MP3.
             *
             * AudioPlayer later converts this provider-level
             * marker into the actual SAI buffer-complete event.
             */
            g_mp3ProviderEof =
                TRUE;

            g_mp3RenderedTerminalBuffer =
                TRUE;

            s_mp3TerminalReported =
                TRUE;
        }
    }

    s_lastRenderValidFrames = outputFrame;

    /*
     * Decoder EOF / error:
     * never replay stale PCM.
     * Fill the remainder with explicit silence.
     */
    while (outputFrame < frameCount)
    {
        buffer[(outputFrame * 2U)] =
            0U;

        buffer[(outputFrame * 2U) + 1U] =
            0U;

        outputFrame++;
    }

    g_mp3OutputRenderCount++;
    g_mp3OutputFrameCount += frameCount;
}


uint32 Mp3PcmProvider_GetLastRenderValidFrames(void)
{
    return s_lastRenderValidFrames;
}


boolean Mp3PcmProvider_WasLastRenderTerminal(
    void
)
{
    return
        g_mp3RenderedTerminalBuffer;
}
