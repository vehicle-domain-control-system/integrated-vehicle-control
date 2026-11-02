/*
 * Temperature / humidity values from the DHT22. No hardware access.
 *
 * Units follow the network draft: temperature in 0.01 degC, humidity in 0.01 %RH.
 *
 * Rules:
 *  - the DHT22 produces a new value only every >= 2 s. Between two readings
 *    the last value is kept and its AGE keeps growing (SEQUENCE does not change);
 *    it becomes INVALID/STALE only after ENV_STALE_MS (candidate)
 *  - a failed reading does not invalidate the last value at once; after
 *    ENV_FAIL_LIMIT consecutive failures the function goes to FAULT
 *  - a range error invalidates only the value that is out of range
 *  - FAULT is left only through RECOVERING and a new valid reading
 */
#ifndef ENV_LOGIC_H
#define ENV_LOGIC_H

#include "cis_value.h"

#define ENV_TEMP_MIN_C01    (-4000)    /* -40.00 degC (DHT22 spec) */
#define ENV_TEMP_MAX_C01    8000       /*  80.00 degC (DHT22 spec) */
#define ENV_HUM_MIN_C01     0          /*   0.00 %RH */
#define ENV_HUM_MAX_C01     10000      /* 100.00 %RH */
#define ENV_STALE_MS        5000u      /* candidate: 2 x sample period + margin */
#define ENV_FAIL_LIMIT      3u         /* consecutive failed readings -> FAULT (candidate) */

typedef struct {
    cis_value_t temperature;     /* 0.01 degC */
    cis_value_t humidity;        /* 0.01 %RH */
    uint8_t     fail_count;
} env_state_t;

typedef enum { DHT22_DECODE_OK = 0, DHT22_DECODE_CHECKSUM } dht22_decode_t;

void env_init(env_state_t *s);

/* 5 raw bytes -> temperature (0.1 degC) and humidity (0.1 %RH). */
dht22_decode_t dht22_decode(const uint8_t raw[5], int16_t *temp_x10, uint16_t *hum_x10);

/* A complete, checksum-correct reading. */
void env_on_reading(env_state_t *s, int16_t temp_x10, uint16_t hum_x10, uint32_t now_ms);

/* No answer, timeout, or checksum error. */
void env_on_failure(env_state_t *s, uint32_t now_ms);

/* Call regularly: marks values older than ENV_STALE_MS as INVALID/STALE. */
void env_check_stale(env_state_t *s, uint32_t now_ms);

#endif /* ENV_LOGIC_H */
