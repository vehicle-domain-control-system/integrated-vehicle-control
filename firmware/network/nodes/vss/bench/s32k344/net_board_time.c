/* S32K344 bench clock and node configuration (not Network product code).
 * See net_board_time.h. */
#include "net_board_time.h"
#include "S32K344_SCB.h"
#include "S32K344_SYSTICK.h"

#define BOARD_SYSTICK_RELOAD ((BOARD_CORE_CLK_HZ / 1000u) - 1u)
#define BOARD_CYCLES_PER_US (BOARD_CORE_CLK_HZ / 1000000u)

#if (BOARD_SYSTICK_RELOAD > S32_SysTick_RVR_RELOAD_MASK)
#error "net_board_time.c: 1 ms does not fit the 24-bit SysTick reload"
#endif

static volatile uint32_t s_board_ms;

void Board_TimeInit(void)
{
    S32_SysTick->CSRr = 0u;
    S32_SysTick->RVR = BOARD_SYSTICK_RELOAD;
    S32_SysTick->CVR = 0u; /* any write clears the counter and COUNTFLAG */
    S32_SysTick->CSRr = S32_SysTick_CSR_CLKSOURCE_MASK | S32_SysTick_CSR_TICKINT_MASK |
                        S32_SysTick_CSR_ENABLE_MASK;
}

/* Overrides the weak startup handler (Project_Settings/Startup_Code/exceptions.c). */
void SysTick_Handler(void);
void SysTick_Handler(void)
{
    s_board_ms++;
}

uint32_t Board_TickMs32(void)
{
    return s_board_ms;
}

uint32_t Board_TickToMs32(uint32_t tick)
{
    return tick;
}

uint32_t Board_NowMs32(void)
{
    return s_board_ms;
}

/* us = ms * 1000 + elapsed cycles of the current ms / cycles per us, in
 * uint32_t arithmetic. A wrapped ms counter differs from the true ms by a
 * multiple of 2^32, and that multiple times 1000 is 0 mod 2^32, so the result
 * is the true us count modulo 2^32: it wraps at 2^32 without a jump (unlike
 * a cycle counter divided by 160, which jumps every 2^32 cycles = 26.8 s).
 *
 * Race: the counter can reload (end of a ms) before the handler has advanced
 * s_board_ms, e.g. while this runs with interrupts masked or the handler is
 * still pending. COUNTFLAG would show that, but reading CSR clears it, so a
 * second reader would miss it. The SysTick pending bit (ICSR.PENDSTSET) is
 * read without side effects instead: pending and a counter value in the upper
 * half of the reload means the value was read after the reload, so one ms is
 * added. A value in the lower half was read before the reload (the ms is
 * still the current one). The handler running between the reads changes
 * s_board_ms and the reads are retried. More than one missed reload
 * (interrupts masked for over 1 ms) cannot be seen. */
uint32_t Board_Us32(void)
{
    uint32_t ms, current, pending;

    do {
        ms = s_board_ms;
        current = S32_SysTick->CVR & S32_SysTick_CVR_CURRENT_MASK;
        pending = S32_SCB->ICSR & S32_SCB_ICSR_PENDSTSET_MASK;
    } while (ms != s_board_ms);
    if ((pending != 0u) && (current > (BOARD_SYSTICK_RELOAD / 2u))) {
        ms++;
    }
    return (ms * 1000u) + ((BOARD_SYSTICK_RELOAD - current) / BOARD_CYCLES_PER_US);
}

bool Board_NetNodeConfig(NetNodeConfig *out)
{
    if (out == NULL) return false;
    /* HOLD: bench constant. A product BOOT_ID must differ on every start;
     * with this constant a reset restarts TX_SEQUENCE at 1 under the same
     * BOOT_ID, which receivers reject (see README.md). */
    out->boot_id = 0x00000344u;
    out->local_epoch = 1u;
    out->can_port_epoch = 1u;
    return true;
}
