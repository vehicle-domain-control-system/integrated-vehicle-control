#include "audio_control.h"
#include "cs2100.h"
#include "sgtl5000.h"
#include "../../infra/vss_config.h"

#include <stddef.h>

static AudioControlStatus s_status;

bool AudioControl_Init(void)
{
    /* The codec runs from the synthesizer's clock: it is touched only after
     * the PLL reports lock. */
    s_status.codecConfigured = false;
    s_status.unmuted = false;
    s_status.clockLocked = AudioControl_Cs2100Init();
    if (s_status.clockLocked && AudioControl_Sgtl5000Probe())
    {
        s_status.codecConfigured = AudioControl_Sgtl5000Configure();
    }
    return s_status.clockLocked && s_status.codecConfigured;
}

bool AudioControl_Unmute(uint8_t volumePercent)
{
    if (s_status.muteLatched || !s_status.codecConfigured)
    {
        return false;
    }
    if (volumePercent > VSS_CFG_VOLUME_PERCENT_MAX)
    {
        volumePercent = VSS_CFG_VOLUME_PERCENT_MAX;
    }
    s_status.unmuted = AudioControl_Sgtl5000StartAudio(volumePercent);
    return s_status.unmuted;
}

bool AudioControl_MuteLatched(void)
{
    /* The latch is set on the first call; the bus write is repeated on every
     * call until the codec takes it, because a refused mute leaves sound on. */
    s_status.muteLatched = true;
    s_status.unmuted = false;
    if (!s_status.muteAccepted)
    {
        s_status.muteAccepted = AudioControl_Sgtl5000LatchMute();
    }
    return s_status.muteAccepted;
}

void AudioControl_Read(AudioControlStatus *out)
{
    if (out == NULL)
    {
        return;
    }
    *out = s_status;
    out->i2cErrors = AudioControl_Cs2100I2cErrors() + AudioControl_Sgtl5000I2cErrors();
}

#if VSS_CFG_HOST_TEST
void AudioControl_ResetForTest(void)
{
    AudioControl_Cs2100ResetForTest();
    AudioControl_Sgtl5000ResetForTest();
    s_status.clockLocked = false;
    s_status.codecConfigured = false;
    s_status.unmuted = false;
    s_status.muteLatched = false;
    s_status.muteAccepted = false;
    s_status.i2cErrors = 0U;
}
#endif
