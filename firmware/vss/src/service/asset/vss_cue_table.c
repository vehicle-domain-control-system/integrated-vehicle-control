#include "vss_cue_table.h"

#include "../input/vss_input_store.h"

/* The row codes are the input path's enumeration values; a change of either
 * side stops the build here. */
#define CUE_CODE_CHECK(name, code, value)     typedef char cue_code_check_##name[((unsigned)(code) == (unsigned)(value)) ? 1 : -1]

CUE_CODE_CHECK(signal_welcome,         AUDIO_ASSET_CUE_SIGNAL_WELCOME,         VSS_SIGNAL_WELCOME);
CUE_CODE_CHECK(signal_goodbye,         AUDIO_ASSET_CUE_SIGNAL_GOODBYE,         VSS_SIGNAL_GOODBYE);
CUE_CODE_CHECK(signal_lock_complete,   AUDIO_ASSET_CUE_SIGNAL_LOCK_COMPLETE,   VSS_SIGNAL_LOCK_COMPLETE);
CUE_CODE_CHECK(signal_unlock_complete, AUDIO_ASSET_CUE_SIGNAL_UNLOCK_COMPLETE, VSS_SIGNAL_UNLOCK_COMPLETE);
CUE_CODE_CHECK(signal_lock_error,      AUDIO_ASSET_CUE_SIGNAL_LOCK_ERROR,      VSS_SIGNAL_LOCK_ERROR);
CUE_CODE_CHECK(signal_anti_pinch,      AUDIO_ASSET_CUE_SIGNAL_ANTI_PINCH,      VSS_SIGNAL_ANTI_PINCH);
CUE_CODE_CHECK(signal_occupant,        AUDIO_ASSET_CUE_SIGNAL_OCCUPANT_HAZARD, VSS_SIGNAL_OCCUPANT_HAZARD);
CUE_CODE_CHECK(signal_rear_risk,       AUDIO_ASSET_CUE_SIGNAL_REAR_RISK,       VSS_SIGNAL_REAR_RISK);
CUE_CODE_CHECK(value_event,            AUDIO_ASSET_CUE_VALUE_EVENT,            VSS_VALUE_EVENT);
CUE_CODE_CHECK(value_active,           AUDIO_ASSET_CUE_VALUE_ACTIVE,           VSS_VALUE_ACTIVE);
CUE_CODE_CHECK(value_emergency,        AUDIO_ASSET_CUE_VALUE_EMERGENCY,        VSS_VALUE_EMERGENCY);
CUE_CODE_CHECK(value_caution,          AUDIO_ASSET_CUE_VALUE_CAUTION,          VSS_VALUE_CAUTION);
/* The uint8_t row fields hold every signal and value code. */
CUE_CODE_CHECK(signal_fits_u8, VSS_SIGNAL_REAR_ACTIVATION <= 255, 1);
CUE_CODE_CHECK(value_fits_u8,  VSS_VALUE_DISABLED <= 255, 1);

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
