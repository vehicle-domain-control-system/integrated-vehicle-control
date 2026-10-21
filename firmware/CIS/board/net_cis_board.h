#ifndef NET_CIS_BOARD_H
#define NET_CIS_BOARD_H

/*
 * Board part of the Network stack for the CIS (S32K144EVB-Q100, RTD).
 *   - Net_Init() hook that gives the BOOT_ID (NET_BOARD_NODE_CONFIG_HOOK)
 *   - a 1 kHz millisecond clock that is safe in the CAN/UART interrupts
 *     (NET_BOARD_NOW_MS_HOOK) and the 32-bit value the main loop passes to
 *     Net_MainFunction()
 * Only Network headers are used here. Defines for the project: see README.md.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "network/net_node.h"

/* Core clock in Hz that the Clock configuration of the RTD project gives the core (SysTick source). */
#ifndef CIS_BOARD_CORE_HZ
#define CIS_BOARD_CORE_HZ   80000000u
#endif

/* Starts the 1 kHz SysTick. Call once after the RTD clock initialisation, before Net_Init(). */
void CisBoard_TimeInit(void);

/* Milliseconds since CisBoard_TimeInit(), 32 bit (wraps after about 49.7 days). Main loop. */
uint32_t CisBoard_NowMs32(void);

/* Same clock as 64 bit, readable in an interrupt (NET_BOARD_NOW_MS_HOOK). */
uint64_t CisBoard_NowMs(void);

/* Net_Init() hook (NET_BOARD_NODE_CONFIG_HOOK). */
bool CisBoard_NetNodeConfig(NetNodeConfig *out);

#endif /* NET_CIS_BOARD_H */
