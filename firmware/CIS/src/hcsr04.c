#include "hcsr04.h"
#include "regs_s32k144.h"
#include "timebase.h"

#define TRIG_PIN        7u      /* PTE7 */
#define ECHO_PIN        8u      /* PTE8 = FTM0_CH6 */
#define TRIG_PULSE_US   12u     /* >= 10 us */

#define CHSC_CAPTURE_RISING   0x04u   /* MSB:MSA = 00, ELSB:ELSA = 01 */
#define CHSC_CAPTURE_FALLING  0x08u   /* MSB:MSA = 00, ELSB:ELSA = 10 */

typedef enum { ST_IDLE = 0, ST_WAIT_RISE, ST_WAIT_FALL } hcsr04_state_t;

static hcsr04_state_t s_state = ST_IDLE;
static uint32_t s_last_trigger_ms;
static bool     s_first = true;
static uint16_t s_t_trigger;
static uint16_t s_t_rise;

static void channel_disarm(void)
{
    (void)FTM0_C6SC;                 /* read first, then write 0: this clears CHF */
    FTM0_C6SC = 0u;
}

void hcsr04_init(void)
{
    PCC_PORTE |= PCC_CGC;
    PORTE_PCR(TRIG_PIN) = PCR_MUX(1);          /* GPIO */
    PORTE_PCR(ECHO_PIN) = PCR_MUX(2);          /* FTM0_CH6 */
    PTE_PDDR |= (1u << TRIG_PIN);
    PTE_PCOR  = (1u << TRIG_PIN);              /* TRIG low */
    channel_disarm();
    s_state = ST_IDLE;
    s_first = true;
}

static void send_trigger(void)
{
    uint16_t t0 = timebase_cnt();
    PTE_PSOR = (1u << TRIG_PIN);
    while ((uint16_t)(timebase_cnt() - t0) < TRIG_PULSE_US) { }
    PTE_PCOR = (1u << TRIG_PIN);
}

bool hcsr04_step(rear_state_t *s, uint32_t now_ms)
{
    switch (s_state) {
    case ST_IDLE:
        if (s_first || (uint32_t)(now_ms - s_last_trigger_ms) >= HCSR04_PERIOD_MS) {
            s_first = false;
            s_last_trigger_ms = now_ms;
            channel_disarm();
            FTM0_C6SC = CHSC_CAPTURE_RISING;
            s_t_trigger = timebase_cnt();
            send_trigger();
            s_state = ST_WAIT_RISE;
        }
        return false;

    case ST_WAIT_RISE:
        if (FTM0_C6SC & FTM_CHSC_CHF) {
            s_t_rise = (uint16_t)FTM0_C6V;      /* captured at the edge itself */
            channel_disarm();
            FTM0_C6SC = CHSC_CAPTURE_FALLING;
            s_state = ST_WAIT_FALL;
        } else if ((uint16_t)(timebase_cnt() - s_t_trigger) > HCSR04_RISE_TIMEOUT_US) {
            channel_disarm();
            s_state = ST_IDLE;
            rear_on_no_echo(s, now_ms);
            return true;
        }
        return false;

    case ST_WAIT_FALL:
        if (FTM0_C6SC & FTM_CHSC_CHF) {
            uint16_t t_fall = (uint16_t)FTM0_C6V;
            channel_disarm();
            s_state = ST_IDLE;
            rear_on_echo(s, (uint16_t)(t_fall - s_t_rise), now_ms);
            return true;
        } else if ((uint16_t)(timebase_cnt() - s_t_rise) > HCSR04_FALL_TIMEOUT_US) {
            channel_disarm();
            s_state = ST_IDLE;
            rear_on_no_echo(s, now_ms);
            return true;
        }
        return false;
    }
    return false;
}
