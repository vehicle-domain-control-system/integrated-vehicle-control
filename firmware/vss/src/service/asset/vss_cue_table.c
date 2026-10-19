#include "vss_cue_table.h"

static const AudioAsset_CueRow s_cueRows[AUDIO_ASSET_CUE_ROWS] =
{
    { AUDIO_ASSET_CUE_SIGNAL_WELCOME,         AUDIO_ASSET_CUE_VALUE_EVENT,     AUDIO_ASSET_ID_WELCOME },
    { AUDIO_ASSET_CUE_SIGNAL_GOODBYE,         AUDIO_ASSET_CUE_VALUE_EVENT,     AUDIO_ASSET_ID_GOODBYE },
    { AUDIO_ASSET_CUE_SIGNAL_LOCK_COMPLETE,   AUDIO_ASSET_CUE_VALUE_EVENT,     AUDIO_ASSET_ID_LOCK },
    { AUDIO_ASSET_CUE_SIGNAL_UNLOCK_COMPLETE, AUDIO_ASSET_CUE_VALUE_EVENT,     AUDIO_ASSET_ID_UNLOCK },
    { AUDIO_ASSET_CUE_SIGNAL_LOCK_ERROR,      AUDIO_ASSET_CUE_VALUE_EVENT,     AUDIO_ASSET_ID_LOCK_ERROR },
    { AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH,      AUDIO_ASSET_CUE_VALUE_ACTIVE,    AUDIO_ASSET_ID_ANTI_PINCH },
    { AUDIO_ASSET_CUE_SIGNAL_OCCUPANT_HAZARD, AUDIO_ASSET_CUE_VALUE_ACTIVE,    AUDIO_ASSET_ID_OCCUPANT },
    { AUDIO_ASSET_CUE_SIGNAL_REAR_RISK,       AUDIO_ASSET_CUE_VALUE_CAUTION,   AUDIO_ASSET_ID_REAR_CAUTION },
    { AUDIO_ASSET_CUE_SIGNAL_REAR_RISK,       AUDIO_ASSET_CUE_VALUE_EMERGENCY, AUDIO_ASSET_ID_REAR_EMERGENCY }
};

const AudioAsset_CueRow *AudioAsset_CueAt(size_t index)
{
    if (index >= AUDIO_ASSET_CUE_ROWS)
    {
        return NULL;
    }
    return &s_cueRows[index];
}

bool AudioAsset_FindCue(uint8_t signal, uint8_t value, AudioAsset_IdType *asset)
{
    size_t i;

    for (i = 0U; i < AUDIO_ASSET_CUE_ROWS; ++i)
    {
        if ((s_cueRows[i].signal == signal) && (s_cueRows[i].value == value))
        {
            if (asset != NULL)
            {
                *asset = s_cueRows[i].asset;
            }
            return true;
        }
    }
    return false;
}
