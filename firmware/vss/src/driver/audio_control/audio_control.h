#ifndef AUDIO_CONTROL_H
#define AUDIO_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     clockLocked;
    bool     codecConfigured;
    bool     unmuted;
    bool     muteLatched;
    uint32_t i2cErrors;
} AudioControlStatus;

/* Configures the clock synthesizer and waits for lock, then probes and
 * configures the codec. Returns true when both are ready. */
bool AudioControl_Init(void);

bool AudioControl_Unmute(uint8_t volumePercent);

/* Mutes the output and refuses any later Unmute until reset. */
void AudioControl_MuteLatched(void);

void AudioControl_Read(AudioControlStatus *out);

#endif /* AUDIO_CONTROL_H */
