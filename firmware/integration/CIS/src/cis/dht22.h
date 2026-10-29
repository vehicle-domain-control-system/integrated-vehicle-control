/*
 * DHT22 (AM2302) on the S32K144EVB, single-wire protocol.
 *   DATA : PTA14 (J2 pin 16). A 3-pin module has its own pull-up; a bare
 *          4-pin sensor needs 4.7..10 k between DATA and VCC. The pin's
 *          internal pull-up is enabled as a backup.
 *   VCC  : 5 V, GND : GND
 * Needs timebase_init() first (the 1 MHz counter times the pulses).
 *
 * A reading blocks for about 5 ms and must not be interrupted. The sensor
 * can only be read every 2 s or more.
 */
#ifndef DHT22_H
#define DHT22_H

#include "env_logic.h"

#define DHT22_PERIOD_MS     2000u    /* time between readings (sensor minimum is 2 s) */

typedef enum {
    DHT22_READ_OK = 0,
    DHT22_READ_NO_RESPONSE,     /* the sensor did not pull the line low */
    DHT22_READ_TIMEOUT          /* a pulse was missing or too long */
} dht22_read_t;

void dht22_init(void);

/* Blocking read of the 5 raw bytes. */
dht22_read_t dht22_read_raw(uint8_t raw[5]);

/* Reads when due and `allowed` is true, then updates `s`.
 * Returns true when a reading was attempted. */
bool dht22_step(env_state_t *s, uint32_t now_ms, bool allowed);

#endif /* DHT22_H */
