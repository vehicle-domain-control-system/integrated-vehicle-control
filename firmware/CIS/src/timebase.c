#include "timebase.h"
#include "regs_s32k144.h"

#ifdef NET_NODE_BUILD
/* S32DS project of the Network team: the millisecond clock is theirs (SysTick, net_board_time144.c);
 * this file only keeps FTM0 as the free-running 1 MHz counter of the sensor drivers. */
#include "net_board_time144.h"
#endif

#define WAIT_LIMIT 2000000u

static uint32_t s_overflows;

bool timebase_init(void)
{
    uint32_t n;

    /* SOSC: 8 MHz crystal, dividers 1 (same as Cookbook SOSC_init_8MHz).
     * In the Network project the clock configuration may already run SOSC (and lock its
     * registers): then only a divider that is off is turned on (divide by 1), a running one is
     * left as it is (other peripherals may use it). SOSCDIV1 feeds FTM0, SOSCDIV2 feeds LPI2C0. */
    if ((SCG_SOSCCSR & SCG_SOSCCSR_SOSCEN) && (SCG_SOSCCSR & SCG_SOSCCSR_SOSCVLD)) {
        if ((SCG_SOSCDIV & 0x7u) == 0u) {
            SCG_SOSCDIV = (SCG_SOSCDIV & ~0x7u) | 0x1u;          /* SOSCDIV1 = divide by 1 */
        }
        if ((SCG_SOSCDIV & 0x700u) == 0u) {
            SCG_SOSCDIV = (SCG_SOSCDIV & ~0x700u) | 0x100u;      /* SOSCDIV2 = divide by 1 */
        }
    } else {
        SCG_SOSCDIV = 0x00000101u;       /* SOSCDIV1 = SOSCDIV2 = divide by 1 */
        SCG_SOSCCFG = 0x00000024u;       /* range 2 (medium), EREFS = external crystal */
        for (n = 0; (SCG_SOSCCSR & SCG_SOSCCSR_LK) && n < WAIT_LIMIT; n++) { }
        SCG_SOSCCSR = SCG_SOSCCSR_SOSCEN;
        for (n = 0; !(SCG_SOSCCSR & SCG_SOSCCSR_SOSCVLD) && n < WAIT_LIMIT; n++) { }
    }
    /* FTM0 is 1 MHz only with SOSCDIV1 = divide by 1 (value 1); any other value is a wrong time base */
    if (!(SCG_SOSCCSR & SCG_SOSCCSR_SOSCVLD) || (SCG_SOSCDIV & 0x7u) != 0x1u) {
        return false;
    }

    /* FTM0: clock source SOSCDIV1 (PCS = 1), prescaler 8 -> 1 MHz */
    PCC_FTM0 &= ~PCC_CGC;
    PCC_FTM0 = PCC_PCS(1) | PCC_CGC;
    FTM0_MODE |= FTM_MODE_WPDIS;         /* allow register writes */
    FTM0_SC = 0x00000003u;               /* PS = 3 (divide by 8), clock stopped */
    FTM0_MOD = 0x0000FFFFu;              /* free running, 65536 ticks */
    FTM0_CNT = 0u;
    FTM0_SC = (3u << 3) | 0x3u;          /* CLKS = 3 (external clock = SOSCDIV1), PS = 3 */

    s_overflows = 0;
    return true;
}

uint16_t timebase_cnt(void)
{
    return (uint16_t)FTM0_CNT;
}

static uint64_t now_us64(void)
{
    uint16_t c = (uint16_t)FTM0_CNT;
    if (FTM0_SC & FTM_SC_TOF) {
        FTM0_SC &= ~FTM_SC_TOF;          /* flag is cleared by writing 0 after reading 1 */
        s_overflows++;
        c = (uint16_t)FTM0_CNT;
    }
    return ((uint64_t)s_overflows << 16) | c;
}

uint32_t timebase_now_us(void)
{
    return (uint32_t)now_us64();
}

uint32_t timebase_now_ms(void)
{
#ifdef NET_NODE_BUILD
    return Board_NowMs32();
#else
    return (uint32_t)(now_us64() / 1000u);   /* monotonic for ~49 days */
#endif
}
