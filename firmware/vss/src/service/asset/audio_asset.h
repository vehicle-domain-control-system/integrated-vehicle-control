#ifndef AUDIO_ASSET_H
#define AUDIO_ASSET_H

#include <stdint.h>

#include "../../infra/vss_config.h"

/* Read access to the embedded cue sounds. The decoder reads through this
 * interface only, so the storage behind it (internal flash today) can change
 * without touching the decoder. */

typedef uint8_t AudioAsset_IdType;

#define AUDIO_ASSET_ID_WELCOME         ((AudioAsset_IdType)0U)
#define AUDIO_ASSET_ID_GOODBYE         ((AudioAsset_IdType)1U)
#define AUDIO_ASSET_ID_LOCK            ((AudioAsset_IdType)2U)
#define AUDIO_ASSET_ID_UNLOCK          ((AudioAsset_IdType)3U)
#define AUDIO_ASSET_ID_LOCK_ERROR      ((AudioAsset_IdType)4U)
#define AUDIO_ASSET_ID_ANTI_PINCH      ((AudioAsset_IdType)5U)
#define AUDIO_ASSET_ID_OCCUPANT        ((AudioAsset_IdType)6U)
#define AUDIO_ASSET_ID_REAR_CAUTION    ((AudioAsset_IdType)7U)
#define AUDIO_ASSET_ID_REAR_EMERGENCY  ((AudioAsset_IdType)8U)
#define AUDIO_ASSET_COUNT              ((AudioAsset_IdType)9U)

typedef enum
{
    AUDIO_ASSET_STATUS_OK = 0,
    AUDIO_ASSET_STATUS_INVALID_ARG,
    AUDIO_ASSET_STATUS_INVALID_ID,
    AUDIO_ASSET_STATUS_OUT_OF_RANGE
} AudioAsset_StatusType;

/* Clears the read counters. */
void AudioAsset_Init(void);

AudioAsset_StatusType AudioAsset_GetSize(
    AudioAsset_IdType assetId,
    uint32_t *sizeBytes
);

/* Copies up to length bytes from offset. A read that runs past the end
 * copies what is there and returns AUDIO_ASSET_STATUS_OUT_OF_RANGE. */
AudioAsset_StatusType AudioAsset_Read(
    AudioAsset_IdType assetId,
    uint32_t offset,
    uint8_t *dst,
    uint32_t length,
    uint32_t *bytesRead
);

/* Failed GetSize/Read calls since Init. */
uint32_t AudioAsset_ReadErrorCount(void);

#if VSS_CFG_HOST_TEST
/* Host tests only: serves data in place of one asset until restored with
 * data == NULL. Not built for the target. */
void AudioAsset_OverrideForTest(AudioAsset_IdType assetId, const uint8_t *data, uint32_t sizeBytes);

/* Host tests only: every asset whose bit (1 << id) is set reads as empty
 * (size 0) until the mask is cleared. Takes precedence over the override. */
void AudioAsset_HideForTest(uint32_t assetMask);
#endif

#endif /* AUDIO_ASSET_H */
