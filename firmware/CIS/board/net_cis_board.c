/* Network side of the CIS board project. See net_cis_board.h. */
#include "net_cis_board.h"

/* ---- 1 kHz clock (Cortex-M4 SysTick, core registers) -------------------------------------- */
#define SYST_CSR    (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR    (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR    (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_TICKINT    (1u << 1)
#define SYST_CSR_CLKSOURCE  (1u << 2)       /* 1 = core clock */

static volatile uint32_t s_ms;              /* low 32 bits */
static volatile uint32_t s_ms_wraps;        /* number of times s_ms wrapped */

/* Replaces the weak handler of the startup code. */
void SysTick_Handler(void)
{
    uint32_t next = s_ms + 1u;

    if (next == 0u) {
        s_ms_wraps++;
    }
    s_ms = next;
}

void CisBoard_TimeInit(void)
{
    s_ms = 0u;
    s_ms_wraps = 0u;
    SYST_CSR = 0u;
    SYST_RVR = (CIS_BOARD_CORE_HZ / 1000u) - 1u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

uint32_t CisBoard_NowMs32(void)
{
    return s_ms;
}

uint64_t CisBoard_NowMs(void)
{
    uint32_t wraps;
    uint32_t ms;

    do {                                    /* retry if the handler wrapped between the reads */
        wraps = s_ms_wraps;
        ms = s_ms;
    } while (wraps != s_ms_wraps);
    return ((uint64_t)wraps << 32) | ms;
}

/* ---- BOOT_ID --------------------------------------------------------------------------------
 * HOLD (same as the Network bench): the source of a BOOT_ID that differs on every start is not
 * decided. This keeps a counter in RAM that the startup code does not clear (section .noinit,
 * the linker script of the project must provide it as NOLOAD). It survives a reset, so every
 * reset gives a new number. After a power cycle the RAM is undefined: the magic value is then
 * wrong and the number restarts from a seed that mixes the SysTick value, which is NOT unique.
 * A receiver keeps a few finished BOOT_IDs and rejects a repeat, so replace this with
 * persistent storage before the product. */
#define BOOT_MAGIC  0xC15B007Du

#if defined(__arm__)
#define CIS_NOINIT  __attribute__((section(".noinit")))
#else
#define CIS_NOINIT                          /* host syntax check only */
#endif
static volatile uint32_t s_boot_magic CIS_NOINIT;
static volatile uint32_t s_boot_counter CIS_NOINIT;

bool CisBoard_NetNodeConfig(NetNodeConfig *out)
{
    uint32_t id;

    if (out == NULL) {
        return false;
    }
    if (s_boot_magic == BOOT_MAGIC) {
        s_boot_counter++;
    } else {
        s_boot_magic = BOOT_MAGIC;
        s_boot_counter = 0x00C15000u ^ (SYST_CVR & 0xFFFFFFu);      /* seed */
    }
    id = s_boot_counter;
    if (id == 0u) {                         /* 0 is reserved */
        id = 1u;
        s_boot_counter = id;
    }
    out->boot_id = id;                      /* epochs: Net_Init defaults (1) */
    return true;
}
