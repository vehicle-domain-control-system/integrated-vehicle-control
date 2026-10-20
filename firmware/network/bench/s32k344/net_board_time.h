#ifndef NET_BENCH_NET_BOARD_TIME_H
#define NET_BENCH_NET_BOARD_TIME_H

/* S32K344 bench clock (not Network product code).
 *
 * SysTick runs at 1 kHz from the core clock; its handler (defined here,
 * overriding the weak startup one) advances a volatile ms counter. That
 * counter is the Network tick of time base A (VSS_NODE_INTEGRATION.md
 * "time base"): the CAN ISR records Board_TickMs32(), main converts it with
 * Board_TickToMs32() (identity, the tick already is a ms), and main passes
 * Board_NowMs32() to Net_MainFunction().
 *
 * Board project defines (Debug_FLASH):
 *   NET_BOARD_ISR_TICK_HOOK=Board_TickMs32
 *   NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32
 *   NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig
 *   NET_BOARD_US_HOOK=Board_Us32 */
#include <stdbool.h>
#include <stdint.h>
#include "network/net_node.h"

/* Core clock the SysTick counts. The RTD frequency query
 * (Clock_Ip_GetClockFrequency) is compiled out in this project
 * (CLOCK_IP_GET_FREQUENCY_API is STD_OFF in the generated clock defines), so
 * the configured CORE_CLK value of the generated clock configuration is used:
 * 160 MHz (generate/src/Clock_Ip_Cfg.c). */
#define BOARD_CORE_CLK_HZ 160000000u

/* Main context, once, after Clock_Ip_Init and before Net_Init. */
void Board_TimeInit(void);

/* SysTick ms counter: read only, safe in an ISR (time base A tick). */
uint32_t Board_TickMs32(void);
/* Tick -> ms: identity (main context). */
uint32_t Board_TickToMs32(uint32_t tick);
/* Current ms for Net_MainFunction(): the same counter, wraps at 2^32. */
uint32_t Board_NowMs32(void);
/* Free-running us counter that wraps at 2^32 (main context). */
uint32_t Board_Us32(void);

/* Net_Init() node configuration hook. */
bool Board_NetNodeConfig(NetNodeConfig *out);

#endif
