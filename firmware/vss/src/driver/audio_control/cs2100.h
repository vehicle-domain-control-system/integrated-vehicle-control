#ifndef CS2100_H
#define CS2100_H

#include <stdbool.h>
#include <stdint.h>

#include "../../infra/vss_config.h"

/* CS2100 clock synthesizer, used only by audio_control.c. */

/* Reads the device ID, writes the configuration, waits for the PLL and
 * checks its lock. Returns true when the PLL reports lock. */
bool AudioControl_Cs2100Init(void);

uint32_t AudioControl_Cs2100I2cErrors(void);

#if VSS_CFG_HOST_TEST
/* Host tests only: power-on state. */
void AudioControl_Cs2100ResetForTest(void);
#endif

#endif /* CS2100_H */
