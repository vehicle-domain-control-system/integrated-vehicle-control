#include "dht22.h"
#include "regs_s32k144.h"
#include "timebase.h"

#define DATA_PIN            14u      /* PTA14 */
#define START_LOW_US        2000u    /* host start signal: low for >= 1 ms */
#define RELEASE_TIMEOUT_US  200u
#define RESPONSE_TIMEOUT_US 200u
#define BIT_TIMEOUT_US      200u
#define BIT_ONE_MIN_US      40u      /* '0' high ~26-28 us, '1' high ~70 us */

/* Diagnostics for the debugger */
volatile uint32_t g_dht_ok;
volatile uint32_t g_dht_no_response;
volatile uint32_t g_dht_timeout;
volatile uint32_t g_dht_checksum;
volatile uint8_t  g_dht_raw[5];

static uint32_t s_last_read_ms;

static inline uint32_t data_level(void)
{
    return (PTA_PDIR >> DATA_PIN) & 1u;
}

static void interrupts_off(void)
{
#if defined(__GNUC__) && defined(__arm__)
    __asm volatile ("cpsid i" ::: "memory");
#endif
}

static void interrupts_on(void)
{
#if defined(__GNUC__) && defined(__arm__)
    __asm volatile ("cpsie i" ::: "memory");
#endif
}

/* Waits until the line has `level`. Returns false on timeout.
 * *elapsed gets the waiting time in us. */
static bool wait_for(uint32_t level, uint16_t timeout_us, uint16_t *elapsed)
{
    uint16_t t0 = timebase_cnt();
    uint16_t dt;

    while (data_level() != level) {
        dt = (uint16_t)(timebase_cnt() - t0);
        if (dt > timeout_us) {
            return false;
        }
    }
    if (elapsed != 0) {
        *elapsed = (uint16_t)(timebase_cnt() - t0);
    }
    return true;
}

void dht22_init(void)
{
    PCC_PORTA |= PCC_CGC;
    PORTA_PCR(DATA_PIN) = PCR_MUX(1) | PCR_PE | PCR_PS_PULLUP;   /* GPIO + pull-up */
    PTA_PDDR &= ~(1u << DATA_PIN);                                /* input: line idles high */
    s_last_read_ms = 0;
}

dht22_read_t dht22_read_raw(uint8_t raw[5])
{
    uint16_t dt;
    uint16_t t0;
    int bit;
    dht22_read_t result = DHT22_READ_OK;

    for (bit = 0; bit < 5; bit++) {
        raw[bit] = 0;
    }

    interrupts_off();

    /* Start signal: host pulls the line low, then releases it. */
    PTA_PCOR = (1u << DATA_PIN);
    PTA_PDDR |= (1u << DATA_PIN);
    t0 = timebase_cnt();
    while ((uint16_t)(timebase_cnt() - t0) < START_LOW_US) { }
    PTA_PDDR &= ~(1u << DATA_PIN);                                /* release */

    /* Let the pull-up raise the line first; otherwise the slowly rising line
     * could be mistaken for the sensor's answer. */
    if (!wait_for(1u, RELEASE_TIMEOUT_US, &dt)) {
        result = DHT22_READ_NO_RESPONSE;                          /* line stuck low */
    /* Sensor answer: low ~80 us, high ~80 us, then the first bit starts with a low. */
    } else if (!wait_for(0u, RESPONSE_TIMEOUT_US, &dt)) {
        result = DHT22_READ_NO_RESPONSE;
    } else if (!wait_for(1u, RESPONSE_TIMEOUT_US, &dt) ||
               !wait_for(0u, RESPONSE_TIMEOUT_US, &dt)) {
        result = DHT22_READ_TIMEOUT;
    } else {
        /* 40 bits: each is a low (~50 us) followed by a high whose length is the value. */
        for (bit = 0; bit < 40; bit++) {
            if (!wait_for(1u, BIT_TIMEOUT_US, &dt) || !wait_for(0u, BIT_TIMEOUT_US, &dt)) {
                result = DHT22_READ_TIMEOUT;
                break;
            }
            raw[bit / 8] = (uint8_t)(raw[bit / 8] << 1);
            if (dt > BIT_ONE_MIN_US) {
                raw[bit / 8] |= 1u;
            }
        }
    }

    interrupts_on();
    return result;
}

bool dht22_step(env_state_t *s, uint32_t now_ms, bool allowed)
{
    uint8_t raw[5];
    dht22_read_t r;
    int16_t t;
    uint16_t h;
    int i;

    if (!allowed || (uint32_t)(now_ms - s_last_read_ms) < DHT22_PERIOD_MS) {
        return false;
    }
    s_last_read_ms = now_ms;

    r = dht22_read_raw(raw);
    for (i = 0; i < 5; i++) {
        g_dht_raw[i] = raw[i];
    }

    if (r == DHT22_READ_NO_RESPONSE) {
        g_dht_no_response++;
        env_on_failure(s, now_ms);
    } else if (r == DHT22_READ_TIMEOUT) {
        g_dht_timeout++;
        env_on_failure(s, now_ms);
    } else if (dht22_decode(raw, &t, &h) != DHT22_DECODE_OK) {
        g_dht_checksum++;
        env_on_failure(s, now_ms);
    } else {
        g_dht_ok++;
        env_on_reading(s, t, h, now_ms);
    }
    return true;
}
