#ifndef NET_BOARD_NET_DOMAIN_BOARD_H
#define NET_BOARD_NET_DOMAIN_BOARD_H

/* Network side of the DOMAIN board project (S32K344): the hooks of Net_Init()
 * and of the DOMAIN node root (src/node/net_node_domain.c). Project defines:
 *   NET_BOARD_NODE_CONFIG_HOOK=NetDomainBoard_NodeConfig
 *   NET_BOARD_DOMAIN_C2_HOOK=NetDomainBoard_C2Config
 *   NET_BOARD_ISR_TICK_HOOK=net_hal_rtd_last_now_ms32
 *   NET_BOARD_TICK_TO_MS_HOOK=NetDomainBoard_TickToMs
 * Time: DOMAIN gives one main-loop millisecond function (uint32_t). main reads
 * it once per pass and hands the value to Net_MainFunction() and Net_TxFlush();
 * Network and this file never call it. The CAN / LPUART ISR stamps a capture
 * with the nowMs of the previous pass (net_hal_rtd_last_now_ms32), converted by
 * the identity below.
 * Only Network headers and the public DOMAIN C2 header
 * (Domain_Interface_v0.5_C2_target.h): never a DOMAIN application header. */
#include <stdbool.h>
#include <stdint.h>
#include "network/net_node.h"
#include "network/net_domain_c2.h"

bool NetDomainBoard_NodeConfig(NetNodeConfig *out);
bool NetDomainBoard_C2Config(NetDomainC2Api *api, NetDomainC2Config *config);
uint32_t NetDomainBoard_TickToMs(uint32_t tick);

/* CIS environment / Occupant transition binding (net_domain_c2.h
 * NetDomainC2TransitionBinding). Its functions call the DOMAIN private BOOT
 * transitions and need DOMAIN application headers, so the DOMAIN integration
 * implements them and hands them over here before Net_Init(); this file keeps a
 * copy and gives it to the adapter. NULL clears it. Without a binding
 * CIS_ENVIRONMENT (0x300) and CIS_OCCUPANT (0x310) stay held. A call after
 * Net_Init() has no effect on the running node. */
void NetDomainBoard_SetTransitionBinding(const NetDomainC2TransitionBinding *binding);

#ifdef NET_DOMAIN_BENCH_PERMISSION
/* TEST BENCH ONLY, never in a product build: calls DomainIf_TxCisPowerPermission
 * every 200 ms (ALLOW x10, then one UNKNOWN withdrawal) so that a bench sees
 * DOMAIN_PERMISSION before the DOMAIN application sends it. Main loop only,
 * with the same nowMs as Net_MainFunction(). */
void NetDomainBench_PermissionPoll(uint32_t nowMs);
#endif

#endif
