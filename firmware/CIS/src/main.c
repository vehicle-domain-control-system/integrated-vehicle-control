/*
 * CIS rear distance demo for the S32K144EVB (HC-SR04 on FTM0_CH6).
 *
 * RGB LED:
 *   red    = object close   (valid distance, <= NEAR_MM)
 *   green  = object far     (valid distance, > NEAR_MM)
 *   blue   = outside the recognizable range (nothing within range, or too close)
 *   blue blinking = no usable measurement (no echo / sensor fault / recovering)
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

#define NEAR_MM         500u     /* red at or below 50 cm, green above (adjustable) */

#define LED_BLUE_PIN    0u       /* PTD0,  active low */
#define LED_RED_PIN     15u      /* PTD15, active low */
#define LED_GREEN_PIN   16u      /* PTD16, active low */
#define LED_MASK        ((1u << LED_BLUE_PIN) | (1u << LED_RED_PIN) | (1u << LED_GREEN_PIN))

#define BLINK_HALF_MS   250u

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

static uint32_t led_color_for(const rear_state_t *s, uint32_t now_ms)
{
    switch (s->status) {
    case CIS_PROX_VALID_DISTANCE:
        return ((uint32_t)s->value.value <= NEAR_MM) ? (1u << LED_RED_PIN) : (1u << LED_GREEN_PIN);
    case CIS_PROX_NO_OBJECT:
        return 1u << LED_BLUE_PIN;                          /* beyond range */
    case CIS_PROX_UNAVAILABLE:
        if (s->value.reason == CIS_REASON_OUT_OF_RANGE) {
            return 1u << LED_BLUE_PIN;                      /* too close */
        }
        /* fall through: no usable measurement */
    default:
        return ((now_ms / BLINK_HALF_MS) & 1u) ? (1u << LED_BLUE_PIN) : 0u;
    }
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

    for (;;) {
        uint32_t now_ms = timebase_now_ms();    /* also keeps the 16-bit counter tracked */

        if (hcsr04_step(&g_rear, now_ms)) {
            g_distance_mm = (uint32_t)g_rear.value.value;
            g_status = (uint32_t)g_rear.status;
            g_reason = (uint32_t)g_rear.value.reason;
            g_sequence = g_rear.value.sequence;
        }
        g_age_ms = cis_value_age_ms(&g_rear.value, now_ms);

        led_show(led_color_for(&g_rear, now_ms));
    }
}
