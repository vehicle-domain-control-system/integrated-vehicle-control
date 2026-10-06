/*
 * CIS common module smoke test for the S32K144EVB.
 *
 * Runs a set of checks on the common module (value / fault / state) and shows
 * the result on the on-board RGB LED:
 *   green LED on = all checks passed
 *   red   LED on = at least one check failed (see g_fail_line in the debugger)
 * g_pass / g_fail / g_fail_line are also readable in the debugger.
 *
 * Registers are accessed by address so this file does not depend on the
 * register names of a particular device header.
 */

#include <stdint.h>

#include "cis_value.h"
#include "cis_fault.h"
#include "cis_state.h"

/*
 * Register access by address, so this file does not depend on the register
 * names of a particular device header (the RTD header names them IP_WDOG,
 * IP_PORTD, ... while the Cookbook header uses WDOG, PORTD, ...).
 * Offsets are from the S32K144 Reference Manual; base addresses are the
 * S32K144 memory map. If in doubt, compare with IP_WDOG_BASE, IP_PCC_BASE,
 * IP_PORTD_BASE and IP_PTD_BASE in the project's S32K144.h.
 */
#define REG32(addr)         (*(volatile uint32_t *)(addr))

#define WDOG_BASE           0x40052000u
#define WDOG_CS             REG32(WDOG_BASE + 0x0u)
#define WDOG_CNT            REG32(WDOG_BASE + 0x4u)
#define WDOG_TOVAL          REG32(WDOG_BASE + 0x8u)

#define PCC_BASE            0x40065000u
#define PCC_PORTD           REG32(PCC_BASE + 0x130u)     /* PCC_PORTD offset 0x130 */
#define PCC_CGC             (1u << 30)                   /* clock gate control */

#define PORTD_BASE          0x4004C000u
#define PORTD_PCR(n)        REG32(PORTD_BASE + 4u * (n)) /* PCR n at offset 4n */
#define PCR_MUX_GPIO        0x00000100u

#define PTD_BASE            0x400FF0C0u
#define PTD_PSOR            REG32(PTD_BASE + 0x04u)
#define PTD_PCOR            REG32(PTD_BASE + 0x08u)
#define PTD_PDDR            REG32(PTD_BASE + 0x14u)

#define LED_RED     15u      /* PTD15, active low */
#define LED_GREEN   16u      /* PTD16, active low */
#define EXPECTED_CHECKS 14u

volatile uint32_t g_pass = 0;
volatile uint32_t g_fail = 0;
volatile uint32_t g_fail_line = 0;

#define CHECK(c) do { if (c) { g_pass++; } else { g_fail++; g_fail_line = __LINE__; } } while (0)

static void WDOG_disable(void)
{
    WDOG_CNT = 0xD928C520u;     /* unlock */
    WDOG_TOVAL = 0x0000FFFFu;   /* max timeout */
    WDOG_CS = 0x00002100u;      /* disable */
}

static void led_init(void)
{
    PCC_PORTD |= PCC_CGC;                                /* clock to PORTD */
    PORTD_PCR(LED_RED)   = PCR_MUX_GPIO;                 /* MUX = GPIO */
    PORTD_PCR(LED_GREEN) = PCR_MUX_GPIO;
    PTD_PDDR |= (1u << LED_RED) | (1u << LED_GREEN);     /* outputs */
    PTD_PSOR  = (1u << LED_RED) | (1u << LED_GREEN);     /* both off (active low) */
}

static void run_checks(void)
{
    cis_value_t v;
    cis_fault_t f;
    cis_func_status_t fn[CIS_FN_COUNT];
    int i;

    /* Resend: AGE grows, SEQUENCE stays */
    cis_value_init(&v);
    CHECK(cis_value_new_sample(&v, 235, 1000));
    CHECK(v.sequence == 1);
    CHECK(cis_value_age_ms(&v, 1600) == 600);
    CHECK(v.sequence == 1);

    /* Fault -> rejected -> recovering -> new sample -> valid */
    cis_value_fault(&v, CIS_REASON_SENSOR_FAULT);
    CHECK(!cis_value_new_sample(&v, 2, 1700));
    cis_value_begin_recovery(&v);
    CHECK(v.status == CIS_FUNC_RECOVERING);
    CHECK(!v.valid);
    CHECK(cis_value_new_sample(&v, 3, 1800));
    CHECK(v.valid);
    CHECK(v.sequence == 2);

    /* Fault history is kept after the fault clears */
    cis_fault_init(&f);
    cis_fault_set(&f, CIS_FAULT_TEMPERATURE_SENSOR);
    CHECK(f.active_fault == 3);
    cis_fault_clear(&f, CIS_FAULT_TEMPERATURE_SENSOR);
    CHECK(f.active_fault == 0 && f.last_fault == 3);

    /* CIS_STATE */
    for (i = 0; i < CIS_FN_COUNT; i++) {
        fn[i] = CIS_FUNC_NOT_READY;
    }
    CHECK(cis_state_eval(CIS_INIT_DONE, false, fn) == CIS_STATE_READY);
    fn[CIS_FN_REAR] = CIS_FUNC_ACTIVE;
    CHECK(cis_state_eval(CIS_INIT_DONE, false, fn) == CIS_STATE_ACTIVE);
}

int main(void)
{
    WDOG_disable();
    led_init();

    run_checks();

    if (g_fail == 0u && g_pass == EXPECTED_CHECKS) {
        PTD_PCOR = (1u << LED_GREEN);       /* green on: PASS */
    } else {
        PTD_PCOR = (1u << LED_RED);         /* red on: FAIL */
    }

    for (;;) {
        /* put a breakpoint here and inspect g_pass / g_fail / g_fail_line */
    }
}
