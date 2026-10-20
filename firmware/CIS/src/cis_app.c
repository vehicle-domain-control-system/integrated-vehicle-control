#include "cis_app.h"
#include "hcsr04.h"
#include "dht22.h"
#include "veml7700.h"
#include "pi_uart.h"

#define PERMISSION_TX_MS    500u

static rear_state_t    s_rear;
static rear_output_t   s_rear_out;
static env_state_t     s_env;
static lux_state_t     s_lux;
static vision_state_t  s_vision;
static power_gate_t    s_gate;

static uint32_t s_last_perm_ms;
static uint16_t s_link_seq;          /* UART-C LINK_SEQUENCE of the frames sent to the Pi */

void cis_app_init(void)
{
    rear_init(&s_rear);
    hcsr04_init();
    env_init(&s_env);
    dht22_init();
    lux_init(&s_lux);
    (void)veml7700_init();           /* g_veml_present shows whether it answered */
    vision_init(&s_vision);
#ifndef CIS_USE_NETWORK_API
    pi_uart_init();
#endif
    power_gate_init(&s_gate);
    s_last_perm_ms = 0u;
    s_link_seq = 0u;
}

void cis_app_on_permission(uint8_t permission, bool quality_ok, uint32_t source_age_ms,
                           uint32_t now_ms)
{
    power_gate_on_permission(&s_gate, permission, quality_ok, source_age_ms, now_ms);
}

void cis_app_on_vision_result(const pi_vision_result_t *f, uint32_t now_ms)
{
    (void)vision_on_result(&s_vision, f, now_ms);
}

void cis_app_on_vision_status(const pi_vision_status_t *f, uint32_t now_ms)
{
    vision_on_status(&s_vision, f, now_ms);
}

/* Raspberry Pi: read all received frames first so the FIFO never fills up.
 * With CIS_USE_NETWORK_API the Network stack owns the UART and delivers the frames
 * through cis_app_on_vision_*(), so only the link supervision runs here. */
static void vision_step(uint32_t now_ms)
{
#ifndef CIS_USE_NETWORK_API
    pi_frame_t frame;

    while (pi_uart_poll(&frame, now_ms)) {
        if (frame.type == PI_TYPE_VISION_RESULT) {
            cis_app_on_vision_result(&frame.result, now_ms);
        } else if (frame.type == PI_TYPE_VISION_STATUS) {
            cis_app_on_vision_status(&frame.status, now_ms);
        }
    }
#endif
    vision_check(&s_vision, now_ms);
}

/* Permission for the Pi (P_PERMISSION). The vehicle power permission comes from the
 * DOMAIN and is not relayed yet, so it is reported as unknown. With CIS_USE_NETWORK_API
 * the Network stack relays the DOMAIN permission itself. */
static void permission_tx_step(uint32_t now_ms)
{
#ifndef CIS_USE_NETWORK_API
    static const pi_permission_t perm = {
        CIS_BOOT_ID,                        /* cis_boot_id */
        1u,                                 /* update_sequence: no evaluation change yet */
        PI_PERMISSION_UNKNOWN,
        PI_QUALITY_NO_DATA,
        (uint8_t)CIS_REASON_NO_DATA,
        1500u,                              /* use_limit_ms (proposal) */
        0xFFFFu                             /* source_age_ms: unknown */
    };

    if (hcsr04_idle() && (uint32_t)(now_ms - s_last_perm_ms) >= PERMISSION_TX_MS) {
        uint8_t out[PI_FRAME_MAX];
        size_t n = pi_build_permission(out, s_link_seq++, &perm);
        s_last_perm_ms = now_ms;
        pi_uart_send(out, n);
    }
#else
    (void)now_ms;
    (void)s_last_perm_ms;
    (void)s_link_seq;
#endif
}

static void rear_step(uint32_t now_ms)
{
    (void)hcsr04_step(&s_rear, now_ms);
    power_gate_apply(&s_gate, &s_rear, now_ms, &s_rear_out);
}

/* The DHT22 read blocks for ~7 ms: only start it while no echo is pending. */
static void env_step(uint32_t now_ms)
{
    dht22_step(&s_env, now_ms, hcsr04_idle());
    env_check_stale(&s_env, now_ms);
}

/* I2C transactions also block for up to ~1 ms: same rule as the DHT22. */
static void lux_step(uint32_t now_ms)
{
    veml7700_step(&s_lux, now_ms, hcsr04_idle());
    lux_check_stale(&s_lux, now_ms);
}

void cis_app_run(uint32_t now_ms)
{
    vision_step(now_ms);
    permission_tx_step(now_ms);
    rear_step(now_ms);
    env_step(now_ms);
    lux_step(now_ms);
}

const rear_state_t   *cis_app_rear(void)        { return &s_rear; }
const rear_output_t  *cis_app_rear_output(void) { return &s_rear_out; }
const env_state_t    *cis_app_env(void)         { return &s_env; }
const lux_state_t    *cis_app_lux(void)         { return &s_lux; }
const vision_state_t *cis_app_vision(void)      { return &s_vision; }
const power_gate_t   *cis_app_gate(void)        { return &s_gate; }
