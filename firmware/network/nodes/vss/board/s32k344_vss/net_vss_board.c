/* Network side of the VSS board project. See net_vss_board.h. */
#include "net_vss_board.h"

/* HOLD: the BOOT_ID source (NVM or noinit RAM counter) is not decided. This
 * test-bench constant (different from the S32K344 bench's 0x344) stays the
 * same across resets, so after a reset VSS_STATUS restarts at TX_SEQUENCE 1
 * under the same BOOT_ID and receivers reject it as old (VSS contract C4,
 * VSS_NODE_INTEGRATION.md: BOOT_ID differs on every start). */
#define VSS_BOARD_BOOT_ID 0x56535331u

bool VssBoard_NetNodeConfig(NetNodeConfig *out)
{
    if (out == NULL) return false;
    out->boot_id = VSS_BOARD_BOOT_ID; /* epochs: Net_Init defaults (1) */
    return true;
}
