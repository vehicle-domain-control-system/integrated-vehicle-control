#ifndef VSS_CUE_TABLE_H
#define VSS_CUE_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_asset.h"

/* Which cue sounds for which input. One row per playable (signal, value)
 * pair; the flow builds its plan catalog from these rows (one cue, played
 * once per plan repeat).
 *
 * signal and value carry the codes of the input path's signal and value
 * enumerations, in their declaration order:
 *   signal 0 WELCOME, 1 GOODBYE, 2 LOCK_COMPLETE, 3 UNLOCK_COMPLETE,
 *          4 LOCK_ERROR, 5 ANTI_PINCH, 6 OCCUPANT_HAZARD, 7 REAR_RISK
 *   value  0 EVENT, 2 ACTIVE, 4 EMERGENCY, 5 CAUTION */

#define AUDIO_ASSET_CUE_SIGNAL_WELCOME          0U
#define AUDIO_ASSET_CUE_SIGNAL_GOODBYE          1U
#define AUDIO_ASSET_CUE_SIGNAL_LOCK_COMPLETE    2U
#define AUDIO_ASSET_CUE_SIGNAL_UNLOCK_COMPLETE  3U
#define AUDIO_ASSET_CUE_SIGNAL_LOCK_ERROR       4U
#define AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH       5U
#define AUDIO_ASSET_CUE_SIGNAL_OCCUPANT_HAZARD  6U
#define AUDIO_ASSET_CUE_SIGNAL_REAR_RISK        7U

#define AUDIO_ASSET_CUE_VALUE_EVENT             0U
#define AUDIO_ASSET_CUE_VALUE_ACTIVE            2U
#define AUDIO_ASSET_CUE_VALUE_EMERGENCY         4U
#define AUDIO_ASSET_CUE_VALUE_CAUTION           5U

#define AUDIO_ASSET_CUE_ROWS 9U

typedef struct
{
    uint8_t signal;
    uint8_t value;
    AudioAsset_IdType asset;
} AudioAsset_CueRow;

/* Row i (0 .. AUDIO_ASSET_CUE_ROWS-1); NULL outside the table. */
const AudioAsset_CueRow *AudioAsset_CueAt(size_t index);

/* True and the asset when the pair has a cue. */
bool AudioAsset_FindCue(uint8_t signal, uint8_t value, AudioAsset_IdType *asset);

#endif /* VSS_CUE_TABLE_H */
