/* Network side of the VSS board project. See net_vss_board.h. */
#include "net_vss_board.h"
#include "network/net_boot_id.h"

/* BOOT_ID: the product source (NVM or noinit RAM counter) is not decided. Until
 * then the value comes from the core cycle counter (board_boot_seed), so it
 * changes on every warm reset such as the reset pin and receivers take the
 * restarted TX_SEQUENCE of VSS_STATUS as a new run (VSS contract C4, VSS_NODE_INTEGRATION.md: BOOT_ID
 * differs on every start). VSS_BOARD_BOOT_ID (different from the S32K344
 * bench's 0x344) is mixed into the seed and is the value with
 * NET_BOARD_BOOT_ID_FIXED. */
#define VSS_BOARD_BOOT_ID 0x56535331u

#ifndef NET_BOARD_BOOT_ID_FIXED
/* BOOT_ID seed (temporary, until a product source such as an NVM reset counter
 * or a hardware RNG): the core cycle counter DWT_CYCCNT. It belongs to the debug
 * domain of the core, which a warm (functional) reset such as the reset pin
 * does not clear, so the value read here carries the length of the earlier
 * runs and changes on every such reset. A power-on reset, and a destructive
 * reset (on the S32K344 some software and debugger resets), starts it again:
 * the value is then the one of every power-on start, and a peer that saw it
 * earlier drops the frames as an old run until the board is reset once more.
 * Fixed ARMv7-M addresses, no SDK header. Other builds (host): 0.
 * NET_BOARD_BOOT_ID_FIXED keeps the bench constant. */
static uint32_t board_boot_seed(void)
{
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
    volatile uint32_t *const demcr = (volatile uint32_t *)0xE000EDFCu;
    volatile uint32_t *const dwt_ctrl = (volatile uint32_t *)0xE0001000u;
    volatile uint32_t *const dwt_cyccnt = (volatile uint32_t *)0xE0001004u;
    *demcr |= 1u << 24;                                  /* DEMCR.TRCENA: DWT on */
    *(volatile uint32_t *)0xE0001FB0u = 0xC5ACCE55u;     /* DWT_LAR: unlock (Cortex-M7 software lock) */
    if ((*dwt_ctrl & (1u << 25)) != 0u) return 0u;       /* DWT_CTRL.NOCYCCNT: no cycle counter */
    *dwt_ctrl |= 1u;                                     /* CYCCNTENA (kept when already on) */
    return *dwt_cyccnt;
#else
    return 0u;
#endif
}
#endif

bool VssBoard_NetNodeConfig(NetNodeConfig *out)
{
    if (out == NULL) return false;
#ifdef NET_BOARD_BOOT_ID_FIXED
    out->boot_id = VSS_BOARD_BOOT_ID; /* test-bench constant */
#else
    {
        /* No record kept across resets: the seed alone gives the value (never 0). */
        NetBootIdRecord none = { 0u, 0u, 0u };
        if (net_boot_id_next(&none, NET_BOOT_RESET_POWER_ON, board_boot_seed() ^ VSS_BOARD_BOOT_ID,
                             &out->boot_id) != NET_OK)
            return false;
    }
#endif
    /* epochs: Net_Init defaults (1) */
    return true;
}
