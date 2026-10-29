#include "env_logic.h"

void env_init(env_state_t *s)
{
    cis_value_init(&s->temperature);
    cis_value_init(&s->humidity);
    s->fail_count = 0;
}

dht22_decode_t dht22_decode(const uint8_t raw[5], int16_t *temp_x10, uint16_t *hum_x10)
{
    uint8_t sum = (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]);
    uint16_t t_raw;

    if (sum != raw[4]) {
        return DHT22_DECODE_CHECKSUM;
    }
    *hum_x10 = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    t_raw = (uint16_t)(((uint16_t)raw[2] << 8) | raw[3]);
    if (t_raw & 0x8000u) {                      /* sign-magnitude, bit 15 = negative */
        *temp_x10 = (int16_t)(-(int16_t)(t_raw & 0x7FFFu));
    } else {
        *temp_x10 = (int16_t)t_raw;
    }
    return DHT22_DECODE_OK;
}

/* The sensor answered. While the function is in FAULT this only starts the
 * recovery; the next good reading completes it. */
static bool accept_reading(env_state_t *s)
{
    s->fail_count = 0;
    if (s->temperature.status == CIS_FUNC_FAULT || s->humidity.status == CIS_FUNC_FAULT) {
        cis_value_begin_recovery(&s->temperature);
        cis_value_begin_recovery(&s->humidity);
        return false;
    }
    return true;
}

void env_on_reading(env_state_t *s, int16_t temp_x10, uint16_t hum_x10, uint32_t now_ms)
{
    int32_t t = (int32_t)temp_x10 * 10;
    int32_t h = (int32_t)hum_x10 * 10;

    if (!accept_reading(s)) {
        return;
    }

    /* Each value is judged on its own: a bad humidity does not hide the temperature. */
    cis_value_new_sample(&s->temperature, t, now_ms);
    if (t < ENV_TEMP_MIN_C01 || t > ENV_TEMP_MAX_C01) {
        cis_value_invalidate(&s->temperature, CIS_REASON_OUT_OF_RANGE);
    }
    cis_value_new_sample(&s->humidity, h, now_ms);
    if (h < ENV_HUM_MIN_C01 || h > ENV_HUM_MAX_C01) {
        cis_value_invalidate(&s->humidity, CIS_REASON_OUT_OF_RANGE);
    }
}

void env_on_failure(env_state_t *s, uint32_t now_ms)
{
    (void)now_ms;
    if (s->fail_count < 255u) {
        s->fail_count++;
    }
    /* A failure while recovering sends the function straight back to FAULT. */
    if (s->fail_count >= ENV_FAIL_LIMIT ||
        s->temperature.status == CIS_FUNC_RECOVERING ||
        s->humidity.status == CIS_FUNC_RECOVERING) {
        cis_value_fault(&s->temperature, CIS_REASON_SENSOR_FAULT);
        cis_value_fault(&s->humidity, CIS_REASON_SENSOR_FAULT);
    }
    /* Otherwise the last values stay as they are; env_check_stale() ages them. */
}

void env_check_stale(env_state_t *s, uint32_t now_ms)
{
    cis_value_check_stale(&s->temperature, now_ms, ENV_STALE_MS);
    cis_value_check_stale(&s->humidity, now_ms, ENV_STALE_MS);
}
