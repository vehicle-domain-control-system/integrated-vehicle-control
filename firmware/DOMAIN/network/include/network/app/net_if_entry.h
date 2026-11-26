#ifndef NETWORK_APP_NET_IF_ENTRY_H
#define NETWORK_APP_NET_IF_ENTRY_H

/* Network public entry points called by the node application super-loop.
 * Implemented by the Network (src/node/net_entry.c) for every node profile;
 * names and arguments are the ones the VSS firmware already calls (firmware/vss src/net_if/
 * vss_net_api.h, src/app/vss_main.c). Declarations must stay identical.
 *
 * Context rules:
 *   - Every function here is main (super-loop) context only, never an ISR.
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
 *     the application is on the same clock axis.
 *   - Net_TxFlush(nowMs): optional, main context only, after the application
 *     ran in the same super-loop pass (Net_MainFunction -> application ->
 *     Net_TxFlush, with the same nowMs or a later one). It sends what the
 *     application submitted since Net_MainFunction in this pass instead of the
 *     next one: M04 publish service and transmit dispatch only. It delivers no
 *     received frame, takes nothing from the ISR ring and calls no application
 *     callback; nowMs follows the Net_MainFunction rule. It does nothing
 *     before a successful Net_Init.
 *   - Net_GetBootId(): the CAN BOOT_ID of this power cycle (read only); 0
 *     before Net_Init or after a failed one. */
#include <stdint.h>

void Net_Init(void);
void Net_MainFunction(uint32_t nowMs);
void Net_TxFlush(uint32_t nowMs);
uint32_t Net_GetBootId(void);

/* Hardware preparation is deliberately separate from BOOT_ID and from
 * physical link health.  PREPARED means that the first Net_Init() completed
 * the configured CAN/UART RTD prepare and arm sequence without attempting a
 * rollback.  It does not prove that a transceiver, bus or peer is present.
 * The state is one-shot for a power cycle; FAILED states are not retried. */
typedef enum {
    NET_HARDWARE_INIT_NOT_INITIALIZED = 0,
    NET_HARDWARE_INIT_INITIALIZING,
    NET_HARDWARE_INIT_PREPARED,
    NET_HARDWARE_INIT_FAILED,
    NET_HARDWARE_INIT_ROLLBACK_FAILED
} NetHardwareInitStatus;

/* Read-only snapshot. Safe to call while an RTD callback is pending; only the
 * foreground Net_Init() owner changes the entry state and the backend part is
 * published under the board IRQ critical section. */
NetHardwareInitStatus Net_GetHardwareInitStatus(void);

#endif
