#ifndef NETWORK_APP_NET_IF_ENTRY_H
#define NETWORK_APP_NET_IF_ENTRY_H

/* Network public entry points called by the node application super-loop.
 * Implemented by the Network (src/node/net_entry.c) for every node profile;
 * names and arguments are the ones the VSS firmware already calls (firmware/vss src/net_if/
 * vss_net_api.h, src/app/vss_main.c). Declarations must stay identical.
 *
 * Context rules:
 *   - Both functions are main (super-loop) context only, never an ISR.
 *   - Net_Init(): once per power cycle, after the board clock is running
 *     (VSS: after VssTime_Init) and before Net_MainFunction(). Takes BOOT_ID
 *     and the epochs from the board hook NET_BOARD_NODE_CONFIG_HOOK
 *     (net_board_cfg.h) and starts the node's links (CAN; UART-C on CIS).
 *     Later calls change nothing; a failed init leaves Net_MainFunction() inert (see NetNodeDiag).
 *   - Net_MainFunction(nowMs): once per super-loop pass. Every Network ->
 *     application callback (VssApp_On*, CisApp_On*) runs synchronously inside it, never
 *     from an interrupt. The CAN ISR only stores frames and raw ticks; this
 *     call converts the ticks, delivers, supervises and transmits, with a
 *     bounded amount of work per pass.
 *   - nowMs is the application's own millisecond clock (VSS: VssTime_NowMs32).
 *     It must not go backwards and may wrap at 32 bits; Network extends it to
 *     64 bits and uses it as its current time. received_at_ms delivered to
 *     the application is on the same clock axis. */
#include <stdint.h>

void Net_Init(void);
void Net_MainFunction(uint32_t nowMs);

#endif
