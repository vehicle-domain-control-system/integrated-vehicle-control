#ifndef SGTL5000_H
#define SGTL5000_H

#include <stdbool.h>
#include <stdint.h>

#include "../../infra/vss_config.h"

/* SGTL5000 codec, used only by audio_control.c. */

/* Reads CHIP_ID and checks the part number. */
bool AudioControl_Sgtl5000Probe(void);

/* Clocking, I2S format, routing, power-up and output levels. The DAC is left
 * muted. */
bool AudioControl_Sgtl5000Configure(void);

/* Unmutes the DAC and sets the headphone level (percent, 0..100). Refused
 * after the safety mute. */
bool AudioControl_Sgtl5000StartAudio(uint8_t volumePercent);

/* Latches the safety mute first, then mutes the headphone output. The latch
 * holds even if the I2C transfer fails. Returns true when the mute was
 * accepted on the bus. */
bool AudioControl_Sgtl5000LatchMute(void);

uint32_t AudioControl_Sgtl5000I2cErrors(void);

#if VSS_CFG_HOST_TEST
/* Host tests only: power-on state. */
void AudioControl_Sgtl5000ResetForTest(void);
#endif

#endif /* SGTL5000_H */
