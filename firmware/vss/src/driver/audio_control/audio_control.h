#ifndef AUDIO_CONTROL_H
#define AUDIO_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "../../infra/vss_config.h"

typedef struct {
    bool     clockLocked;
    bool     codecConfigured;
    bool     unmuted;
    bool     muteLatched;
    bool     muteAccepted;   /* the codec took the latched mute on the bus */
    uint32_t i2cErrors;
} AudioControlStatus;

/* Configures the clock synthesizer and waits for lock, then probes and
 * configures the codec. Returns true when both are ready. */
bool AudioControl_Init(void);

bool AudioControl_Unmute(uint8_t volumePercent);

/* Mutes the output and refuses any later Unmute until reset. Returns true
 * when the codec accepted the mute; false when the I2C transfer failed or
 * the codec was never configured. The latch holds either way. Only the
 * first call talks to the codec; later calls return its result. */
bool AudioControl_MuteLatched(void);

void AudioControl_Read(AudioControlStatus *out);

#if VSS_CFG_HOST_TEST
/* Host tests only: returns the clock synthesizer and codec control to their
 * power-on state, mute latch included. Not built for the target. */
void AudioControl_ResetForTest(void);
#endif

#endif /* AUDIO_CONTROL_H */
