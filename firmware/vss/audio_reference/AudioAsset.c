#include "AudioAsset.h"
#include "EmbeddedWavAssets.h"
#include "EmbeddedMp3Sample.h"
#include "EmbeddedMp3MusicDemo.h"
#include "EmbeddedMp3PlaylistTracks.h"

typedef struct
{
    const uint8_t *data;
    uint32_t sizeBytes;
} AudioAsset_InternalFlashEntryType;

static const AudioAsset_InternalFlashEntryType g_audioAssetTable[AUDIO_ASSET_COUNT] =
{
    {
        g_embeddedWavWelcome,
        EMBEDDED_WAV_WELCOME_SIZE_BYTES
    },
    {
        g_embeddedWavLock,
        EMBEDDED_WAV_LOCK_SIZE_BYTES
    },
    {
        g_embeddedWavWarning,
        EMBEDDED_WAV_WARNING_SIZE_BYTES
    },
    {
        g_embeddedMp3Test,
        EMBEDDED_MP3_TEST_SIZE_BYTES
    },
    {
        g_embeddedMp3MusicDemo,
        EMBEDDED_MP3_MUSIC_DEMO_SIZE_BYTES
    },
    {
        g_embeddedMp3PlaylistA,
        EMBEDDED_MP3_PLAYLIST_A_SIZE_BYTES
    },
    {
        g_embeddedMp3PlaylistB,
        EMBEDDED_MP3_PLAYLIST_B_SIZE_BYTES
    },
    {
        g_embeddedMp3PlaylistC,
        EMBEDDED_MP3_PLAYLIST_C_SIZE_BYTES
    }
};

volatile uint32_t g_assetReadCallCount = 0U;
volatile uint32_t g_assetBytesReadTotal = 0U;
volatile uint32_t g_assetReadErrorCount = 0U;
volatile uint32_t g_assetLastOffset = 0U;
volatile uint32_t g_assetLastLength = 0U;
volatile uint8_t  g_assetLastId = 0xFFU;

void AudioAsset_Init(void)
{
    g_assetReadCallCount = 0U;
    g_assetBytesReadTotal = 0U;
    g_assetReadErrorCount = 0U;
    g_assetLastOffset = 0U;
    g_assetLastLength = 0U;
    g_assetLastId = 0xFFU;
}

AudioAsset_StatusType AudioAsset_GetSize(
    AudioAsset_IdType assetId,
    uint32_t *sizeBytes
)
{
    if (sizeBytes == (uint32_t *)0)
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_INVALID_ARG;
    }

    if (assetId >= AUDIO_ASSET_COUNT)
    {
        *sizeBytes = 0U;
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_INVALID_ID;
    }

    *sizeBytes =
        g_audioAssetTable[assetId].sizeBytes;

    return AUDIO_ASSET_STATUS_OK;
}

AudioAsset_StatusType AudioAsset_Read(
    AudioAsset_IdType assetId,
    uint32_t offset,
    uint8_t *dst,
    uint32_t length,
    uint32_t *bytesRead
)
{
    const AudioAsset_InternalFlashEntryType *asset;
    uint32_t available;
    uint32_t copyLength;
    uint32_t i;

    if (bytesRead == (uint32_t *)0)
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_INVALID_ARG;
    }

    *bytesRead = 0U;

    if (
        (dst == (uint8_t *)0)
        &&
        (length != 0U)
    )
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_INVALID_ARG;
    }

    if (assetId >= AUDIO_ASSET_COUNT)
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_INVALID_ID;
    }

    asset =
        &g_audioAssetTable[assetId];

    if (offset > asset->sizeBytes)
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_OUT_OF_RANGE;
    }

    available =
        asset->sizeBytes - offset;

    copyLength =
        (length < available)
        ? length
        : available;

    for (i = 0U; i < copyLength; i++)
    {
        dst[i] =
            asset->data[offset + i];
    }

    *bytesRead =
        copyLength;

    g_assetReadCallCount++;
    g_assetBytesReadTotal += copyLength;
    g_assetLastOffset = offset;
    g_assetLastLength = copyLength;
    g_assetLastId = assetId;

    if (copyLength != length)
    {
        g_assetReadErrorCount++;
        return AUDIO_ASSET_STATUS_OUT_OF_RANGE;
    }

    return AUDIO_ASSET_STATUS_OK;
}
