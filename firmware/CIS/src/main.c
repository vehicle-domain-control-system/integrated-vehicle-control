/*
 * CIS rear distance demo for the S32K144EVB (HC-SR04 on FTM0_CH6).
 *
 * RGB LED:
 *   red    = object close   (valid distance, <= NEAR_MM)
 *   green  = object far     (valid distance, > NEAR_MM)
 *   blue   = outside the sensor range of 2..500 cm (nothing within range, or too close)
 *   blue blinking = no usable measurement for more than LED_HOLD_MS
 *                   (no echo / sensor fault / recovering)
 *   red + blue (magenta) = oscillator did not start (clock problem)
 *
 * Wiring: HC-SR04 VCC -> 5 V, GND -> GND, TRIG -> PTE7, ECHO -> PTE8.
 * Debugger variables: g_distance_mm, g_status, g_reason, g_sequence, g_age_ms.
 */

#include <stdint.h>

#include "regs_s32k144.h"
#include "timebase.h"
#include "hcsr04.h"
#include "rear_logic.h"

/* Set to 1 to find out where PTE7 (TRIG) is: PTE7 is held HIGH (about 5 V) and
 * the green LED is on. Measure with a multimeter between a header pin and GND.
 * Set back to 0 for the distance demo. */
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

volatile uint32_t g_distance_mm;
volatile uint32_t g_status;
volatile uint32_t g_reason;
volatile uint32_t g_sequence;
volatile uint32_t g_age_ms;

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

#if HCSR04_PIN_TEST
    PTE_PSOR = (1u << 7);                  /* PTE7 (TRIG) = HIGH */
    led_show(1u << LED_GREEN_PIN);
    for (;;) { }
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
