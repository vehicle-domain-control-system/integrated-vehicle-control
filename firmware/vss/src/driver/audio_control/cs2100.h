#ifndef CS2100_H
#define CS2100_H

#include <stdbool.h>
#include <stdint.h>

/* CS2100 clock synthesizer, used only by audio_control.c. */

/* Reads the device ID, writes the configuration, waits for the PLL and
 * checks its lock. Returns true when the PLL reports lock. */
bool AudioControl_Cs2100Init(void);

uint32_t AudioControl_Cs2100I2cErrors(void);

#endif /* CS2100_H */
