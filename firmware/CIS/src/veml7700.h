/*
 * VEML7700 ambient light sensor on LPI2C0.
 *   SDA : PTA2 (J1 pin 1)  - through the board's SH11 solder jumper
 *   SCL : PTA3 (J1 pin 3)  - through the board's SH12 solder jumper
 *   VCC : 5 V (5 V I2C compatible module), GND : GND
 * The module carries its own pull-ups. I2C address 0x10, 100 kHz.
 * Needs timebase_init() first (the 8 MHz SOSC feeds LPI2C0 and times the waits).
 *
 * A transaction blocks for up to about 1 ms, so call it only while no ultrasonic
 * echo is pending (see hcsr04_idle()).
 */
#ifndef VEML7700_H
#define VEML7700_H

#include "lux_logic.h"

#define VEML7700_PERIOD_MS  200u     /* time between readings (integration time is 100 ms) */

/* Frees a stuck bus, starts LPI2C0 and configures the sensor.
 * Returns true if the sensor answered and holds the expected configuration. */
bool veml7700_init(void);

/* Reads when due and `allowed` is true, then updates `s`.
 * Returns true when a reading was attempted. */
bool veml7700_step(lux_state_t *s, uint32_t now_ms, bool allowed);

#endif /* VEML7700_H */
