#ifndef AUDIO_ASSET_H
#define AUDIO_ASSET_H

#include <stdint.h>

/*
 * Stage 13 — Audio Asset Reader
 *
 * Current backend:
 *   multiple WAV/MP3 const assets in Internal Program Flash
 *
 * Future backend:
 *   QSPI external flash
 *
 * Upper WAV/MP3 code must not depend on the physical storage backend.
 */

typedef uint8_t AudioAsset_IdType;

#define AUDIO_ASSET_ID_WELCOME_WAV      ((AudioAsset_IdType)0U)
#define AUDIO_ASSET_ID_LOCK_WAV         ((AudioAsset_IdType)1U)
#define AUDIO_ASSET_ID_WARNING_WAV      ((AudioAsset_IdType)2U)
#define AUDIO_ASSET_ID_MP3_TEST         ((AudioAsset_IdType)3U)
#define AUDIO_ASSET_ID_MP3_MUSIC_DEMO   ((AudioAsset_IdType)4U)
#define AUDIO_ASSET_ID_MP3_PLAYLIST_A    ((AudioAsset_IdType)5U)
#define AUDIO_ASSET_ID_MP3_PLAYLIST_B    ((AudioAsset_IdType)6U)
#define AUDIO_ASSET_ID_MP3_PLAYLIST_C    ((AudioAsset_IdType)7U)
#define AUDIO_ASSET_COUNT               ((AudioAsset_IdType)8U)

typedef enum
{
    AUDIO_ASSET_STATUS_OK = 0,
    AUDIO_ASSET_STATUS_INVALID_ARG,
    AUDIO_ASSET_STATUS_INVALID_ID,
    AUDIO_ASSET_STATUS_OUT_OF_RANGE
} AudioAsset_StatusType;

void AudioAsset_Init(void);

AudioAsset_StatusType AudioAsset_GetSize(
    AudioAsset_IdType assetId,
    uint32_t *sizeBytes
);

AudioAsset_StatusType AudioAsset_Read(
    AudioAsset_IdType assetId,
    uint32_t offset,
    uint8_t *dst,
    uint32_t length,
    uint32_t *bytesRead
);

/* Debug / observability */
extern volatile uint32_t g_assetReadCallCount;
extern volatile uint32_t g_assetBytesReadTotal;
extern volatile uint32_t g_assetReadErrorCount;
extern volatile uint32_t g_assetLastOffset;
extern volatile uint32_t g_assetLastLength;
extern volatile uint8_t  g_assetLastId;

#endif /* AUDIO_ASSET_H */
