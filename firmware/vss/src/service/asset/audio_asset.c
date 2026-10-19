#include "audio_asset.h"
#include "EmbeddedVssMp3Cues.h"

#include <stddef.h>

typedef struct
{
    const uint8_t *data;
    uint32_t sizeBytes;
} AudioAsset_InternalFlashEntryType;

/* Indexed by AudioAsset_IdType. */
static const AudioAsset_InternalFlashEntryType g_audioAssetTable[AUDIO_ASSET_COUNT] =
{
    { g_vssCueWelcome,        VSS_CUE_WELCOME_SIZE_BYTES },
    { g_vssCueGoodbye,        VSS_CUE_GOODBYE_SIZE_BYTES },
    { g_vssCueLock,           VSS_CUE_LOCK_COMPLETE_SIZE_BYTES },
    { g_vssCueUnlock,         VSS_CUE_UNLOCK_COMPLETE_SIZE_BYTES },
    { g_vssCueLockError,      VSS_CUE_LOCK_ERROR_SIZE_BYTES },
    { g_vssCueAntiPinch,      VSS_CUE_ANTI_PINCH_SIZE_BYTES },
    { g_vssCueOccupantHazard, VSS_CUE_OCCUPANT_HAZARD_SIZE_BYTES },
    { g_vssCueRearCaution,    VSS_CUE_REAR_CAUTION_SIZE_BYTES },
    { g_vssCueRearEmergency,  VSS_CUE_REAR_EMERGENCY_SIZE_BYTES }
};

/* Debug / observability */
volatile uint32_t g_assetReadCallCount = 0U;
volatile uint32_t g_assetBytesReadTotal = 0U;
volatile uint32_t g_assetReadErrorCount = 0U;
volatile uint32_t g_assetLastOffset = 0U;
volatile uint32_t g_assetLastLength = 0U;
volatile uint8_t  g_assetLastId = 0xFFU;

#if VSS_CFG_HOST_TEST
static AudioAsset_IdType s_overrideId = AUDIO_ASSET_COUNT;
static AudioAsset_InternalFlashEntryType s_override;
static uint32_t s_hiddenMask;
static const AudioAsset_InternalFlashEntryType s_hidden = { (const uint8_t *)0, 0U };
#endif

static const AudioAsset_InternalFlashEntryType *AudioAsset_Entry(AudioAsset_IdType assetId)
{
#if VSS_CFG_HOST_TEST
    if ((s_hiddenMask & (1UL << assetId)) != 0U)
    {
        return &s_hidden;
    }
    if (assetId == s_overrideId)
    {
        return &s_override;
    }
#endif
    return &g_audioAssetTable[assetId];
}

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
        AudioAsset_Entry(assetId)->sizeBytes;

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
        AudioAsset_Entry(assetId);

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

uint32_t AudioAsset_ReadErrorCount(void)
{
    return g_assetReadErrorCount;
}

#if VSS_CFG_HOST_TEST
void AudioAsset_OverrideForTest(AudioAsset_IdType assetId, const uint8_t *data, uint32_t sizeBytes)
{
    if ((data == NULL) || (assetId >= AUDIO_ASSET_COUNT))
    {
        s_overrideId = AUDIO_ASSET_COUNT;
        return;
    }
    s_overrideId = assetId;
    s_override.data = data;
    s_override.sizeBytes = sizeBytes;
}

void AudioAsset_HideForTest(uint32_t assetMask)
{
    s_hiddenMask = assetMask;
}
#endif
