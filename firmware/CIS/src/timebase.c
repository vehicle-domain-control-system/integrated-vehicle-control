#include "timebase.h"
#include "regs_s32k144.h"

#define WAIT_LIMIT 2000000u

static uint32_t s_overflows;

bool timebase_init(void)
{
    uint32_t n;

    /* SOSC: 8 MHz crystal, dividers 1 (same as Cookbook SOSC_init_8MHz) */
    SCG_SOSCDIV = 0x00000101u;           /* SOSCDIV1 = SOSCDIV2 = divide by 1 */
    SCG_SOSCCFG = 0x00000024u;           /* range 2 (medium), EREFS = external crystal */
    for (n = 0; (SCG_SOSCCSR & SCG_SOSCCSR_LK) && n < WAIT_LIMIT; n++) { }
    SCG_SOSCCSR = SCG_SOSCCSR_SOSCEN;
    for (n = 0; !(SCG_SOSCCSR & SCG_SOSCCSR_SOSCVLD) && n < WAIT_LIMIT; n++) { }
    if (!(SCG_SOSCCSR & SCG_SOSCCSR_SOSCVLD)) {
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
    return (uint32_t)(now_us64() / 1000u);   /* monotonic for ~49 days */
}
