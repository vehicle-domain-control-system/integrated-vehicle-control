/*
 * CIS rear distance demo for the S32K144EVB (HC-SR04 on FTM0_CH6).
 *
 * RGB LED:
 *   red    = object close   (valid distance, <= NEAR_MM)
 *   green  = object far     (valid distance, > NEAR_MM)
 *   blue   = outside the valid range of 10..100 cm (nothing within range, or too close)
 *   blue blinking = no usable measurement for more than LED_HOLD_MS
 *                   (no echo / sensor fault / recovering)
 *   red + blue (magenta) = oscillator did not start (clock problem)
 *
 * Wiring: HC-SR04 VCC -> 5 V, GND -> GND, TRIG -> PTE7 (J2-14), ECHO -> PTE8 (J2-8).
 *         DHT22  VCC -> 5 V, GND -> GND, DATA -> PTA14 (J2-16).
 * Debugger variables:
 *   rear : g_distance_mm, g_status, g_reason, g_sequence, g_age_ms
 *   DHT22: g_temp_c01 (0.01 degC), g_hum_c01 (0.01 %RH), g_env_*_valid, g_env_*_seq,
 *          g_env_*_age_ms, g_env_*_reason, g_dht_ok / g_dht_no_response /
 *          g_dht_timeout / g_dht_checksum (read results), g_dht_raw[5]
 */

#include <stdint.h>

#include "regs_s32k144.h"
#include "timebase.h"
#include "hcsr04.h"
#include "dht22.h"
#include "rear_logic.h"

/* Wiring tests (set back to 0 for the distance demo):
 *   1 = PTE7 (TRIG) is held HIGH (about 5 V), green LED on. Measure with a
 *       multimeter between the TRIG wire at the sensor and GND.
 *   2 = ECHO test without the FTM capture: a trigger pulse is sent every 80 ms
 *       and PTE8 (ECHO) is polled as a plain input.
 *         red   = no ECHO pulse seen yet (ECHO does not arrive)
 *         green = at least one ECHO pulse was seen (ECHO wiring and sensor OK)
 *       Debugger: g_echo_pulses (count), g_echo_max_us (longest pulse, us),
 *       g_echo_last_us (last pulse). Distance in cm = us / 58. */
#ifndef HCSR04_PIN_TEST
#define HCSR04_PIN_TEST 0
#endif

#define NEAR_MM         500u     /* red at or below 50 cm, green above (adjustable) */

#define LED_BLUE_PIN    0u       /* PTD0,  active low */
#define LED_RED_PIN     15u      /* PTD15, active low */
#define LED_GREEN_PIN   16u      /* PTD16, active low */
#define LED_MASK        ((1u << LED_BLUE_PIN) | (1u << LED_RED_PIN) | (1u << LED_GREEN_PIN))

#define BLINK_HALF_MS   250u
#define LED_HOLD_MS     400u     /* keep the last good colour this long after a missed measurement */

static rear_state_t g_rear;
static env_state_t  g_env;

volatile uint32_t g_distance_mm;
volatile uint32_t g_status;
volatile uint32_t g_reason;
volatile uint32_t g_sequence;
volatile uint32_t g_age_ms;

volatile int32_t  g_temp_c01;
volatile int32_t  g_hum_c01;
volatile uint32_t g_env_temp_valid;
volatile uint32_t g_env_hum_valid;
volatile uint32_t g_env_temp_seq;
volatile uint32_t g_env_hum_seq;
volatile uint32_t g_env_temp_age_ms;
volatile uint32_t g_env_hum_age_ms;
volatile uint32_t g_env_temp_reason;
volatile uint32_t g_env_hum_reason;

/* HCSR04_PIN_TEST == 2 */
volatile uint32_t g_echo_pulses;
volatile uint32_t g_echo_max_us;
volatile uint32_t g_echo_last_us;

static void WDOG_disable(void)
{
    WDOG_CNT = 0xD928C520u;      /* unlock */
    WDOG_TOVAL = 0x0000FFFFu;    /* max timeout */
    WDOG_CS = 0x00002100u;       /* disable */
}

static void led_init(void)
{
    PCC_PORTD |= PCC_CGC;
    PORTD_PCR(LED_BLUE_PIN)  = PCR_MUX(1);
    PORTD_PCR(LED_RED_PIN)   = PCR_MUX(1);
    PORTD_PCR(LED_GREEN_PIN) = PCR_MUX(1);
    PTD_PDDR |= LED_MASK;
    PTD_PSOR  = LED_MASK;        /* all off (active low) */
}

/* mask = LEDs to turn on, all others off */
static void led_show(uint32_t on_mask)
{
    PTD_PSOR = LED_MASK;
    PTD_PCOR = on_mask;
}

/* Colour of an answered measurement; 0 means "no usable measurement". */
static uint32_t answered_color(const rear_state_t *s, bool *answered)
{
    *answered = true;
    switch (s->status) {
    case CIS_PROX_VALID_DISTANCE:
        return ((uint32_t)s->value.value <= NEAR_MM) ? (1u << LED_RED_PIN) : (1u << LED_GREEN_PIN);
    case CIS_PROX_NO_OBJECT:
        return 1u << LED_BLUE_PIN;                          /* beyond range */
    case CIS_PROX_UNAVAILABLE:
        if (s->value.reason == CIS_REASON_OUT_OF_RANGE) {
            return 1u << LED_BLUE_PIN;                      /* too close */
        }
        break;
    default:
        break;
    }
    *answered = false;
    return 0u;
}

int main(void)
{
    WDOG_disable();
    led_init();

    if (!timebase_init()) {
        led_show((1u << LED_RED_PIN) | (1u << LED_BLUE_PIN));
        for (;;) { }
    }

    rear_init(&g_rear);
    hcsr04_init();
    env_init(&g_env);
    dht22_init();

#if HCSR04_PIN_TEST == 1
    PTE_PSOR = (1u << 7);                  /* PTE7 (TRIG) = HIGH */
    led_show(1u << LED_GREEN_PIN);
    for (;;) { }
#elif HCSR04_PIN_TEST == 2
    {
        uint32_t last_trig_ms = 0u;
        uint16_t t_rise = 0u;
        uint32_t level_prev = 0u;

        led_show(1u << LED_RED_PIN);
        for (;;) {
            uint32_t now_ms = timebase_now_ms();
            uint32_t level = (PTE_PDIR >> 8) & 1u;       /* ECHO = PTE8 */

            if ((uint32_t)(now_ms - last_trig_ms) >= 80u) {
                uint16_t t0 = timebase_cnt();
                last_trig_ms = now_ms;
                PTE_PSOR = (1u << 7);                     /* TRIG pulse, 12 us */
                while ((uint16_t)(timebase_cnt() - t0) < 12u) { }
                PTE_PCOR = (1u << 7);
            }
            if (level && !level_prev) {
                t_rise = timebase_cnt();
            } else if (!level && level_prev) {
                g_echo_last_us = (uint16_t)(timebase_cnt() - t_rise);
                g_echo_pulses++;
                if (g_echo_last_us > g_echo_max_us) {
                    g_echo_max_us = g_echo_last_us;
                }
                led_show(1u << LED_GREEN_PIN);            /* latched: an echo was seen */
            }
            level_prev = level;
        }
    }
#endif

    {
        uint32_t last_color = 0u;
        uint32_t last_answer_ms = 0u;
        bool have_answer = false;

        for (;;) {
            uint32_t now_ms = timebase_now_ms();    /* also keeps the 16-bit counter tracked */
            uint32_t color;
            bool answered;

            if (hcsr04_step(&g_rear, now_ms)) {
                g_distance_mm = (uint32_t)g_rear.value.value;
                g_status = (uint32_t)g_rear.status;
                g_reason = (uint32_t)g_rear.value.reason;
                g_sequence = g_rear.value.sequence;
            }
            g_age_ms = cis_value_age_ms(&g_rear.value, now_ms);

            /* The DHT22 read blocks for ~5 ms: only start it while no echo is pending. */
            dht22_step(&g_env, now_ms, hcsr04_idle());
            env_check_stale(&g_env, now_ms);
            g_temp_c01 = g_env.temperature.value;
            g_hum_c01 = g_env.humidity.value;
            g_env_temp_valid = g_env.temperature.valid;
            g_env_hum_valid = g_env.humidity.valid;
            g_env_temp_seq = g_env.temperature.sequence;
            g_env_hum_seq = g_env.humidity.sequence;
            g_env_temp_age_ms = cis_value_age_ms(&g_env.temperature, now_ms);
            g_env_hum_age_ms = cis_value_age_ms(&g_env.humidity, now_ms);
            g_env_temp_reason = (uint32_t)g_env.temperature.reason;
            g_env_hum_reason = (uint32_t)g_env.humidity.reason;

            color = answered_color(&g_rear, &answered);
            if (answered) {
                last_color = color;
                last_answer_ms = now_ms;
                have_answer = true;
            } else if (have_answer && (uint32_t)(now_ms - last_answer_ms) < LED_HOLD_MS) {
                color = last_color;                 /* short gap: keep the last colour */
            } else {
                color = ((now_ms / BLINK_HALF_MS) & 1u) ? (1u << LED_BLUE_PIN) : 0u;
            }
            led_show(color);
        }
    }
}
