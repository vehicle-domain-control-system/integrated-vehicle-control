#ifndef NET_BOARD_CFG_H
#define NET_BOARD_CFG_H

/* ==========================================================================
 * Board binding for the RTD HAL (src/platform/rtd: net_hal_rtd.c common part,
 * net_hal_rtd_can.c FlexCAN part).
 * This is the ONLY file to edit when the HAL meets a real S32DS project.
 * Defaults equal network_344_can_bringup (S32K344, RTD 7.0.1, CAN0
 * PTA6/PTA7, FD+BRS 500k/2M, 64 B payload, max_num_mb 16).
 * Every value can also be overridden with -D in the project settings.
 * ========================================================================== */

/* ---- FlexCAN instance and generated-config symbols (ConfigTools names) ---- */
#ifndef NET_BOARD_CAN_INSTANCE
#define NET_BOARD_CAN_INSTANCE          0U
#endif
#ifndef NET_BOARD_CAN_CONFIG
#define NET_BOARD_CAN_CONFIG            FlexCAN_Config0
#endif
#ifndef NET_BOARD_CAN_STATE
#define NET_BOARD_CAN_STATE             FlexCAN_State0
#endif
/* Generated config header that declares the two symbols above. */
#ifndef NET_BOARD_CAN_CFG_HEADER
#define NET_BOARD_CAN_CFG_HEADER        "FlexCAN_Ip_Sa_PBcfg.h"
#endif
/* Callback names typed into ConfigTools (Controller Callback / Error Callback).
 * The HAL DEFINES functions with these names. Remove the bring-up file that
 * defined them before (bringup_can0.c), or rename them in ConfigTools. */
#ifndef NET_BOARD_CAN_CALLBACK
#define NET_BOARD_CAN_CALLBACK          Can0_BringupCallback
#endif
#ifndef NET_BOARD_CAN_ERROR_CALLBACK
#define NET_BOARD_CAN_ERROR_CALLBACK    Can0_BringupErrorCallback
#endif

/* ---- Message buffer layout (must fit ConfigTools max_num_mb) ----
 * 64 B payload: S32K344 CAN0 config uses 16 MBs; S32K144 CAN0 holds 7.
 * RX MBs accept every standard ID (individual mask 0); the M04 registry
 * filters by node profile in software, so the layout is node independent. */
#ifndef NET_BOARD_CAN_MAX_MB
#define NET_BOARD_CAN_MAX_MB            16U
#endif
#ifndef NET_BOARD_CAN_RX_MB_FIRST
#define NET_BOARD_CAN_RX_MB_FIRST       0U
#endif
#ifndef NET_BOARD_CAN_RX_MB_COUNT
#define NET_BOARD_CAN_RX_MB_COUNT       4U   /* S32K144: 2 */
#endif
#ifndef NET_BOARD_CAN_TX_MB_FIRST
#define NET_BOARD_CAN_TX_MB_FIRST       8U   /* S32K144: 2; uses NET_HAL_TX_CAPACITY (4) MBs */
#endif
/* A TX MB not completed within this time (no ACK, bus off) is aborted and
 * reported FAILED + released, freeing the slot. */
#ifndef NET_BOARD_CAN_TX_TIMEOUT_MS
#define NET_BOARD_CAN_TX_TIMEOUT_MS     100U
#endif

/* ---- Millisecond time base ----
 * Default: OsIf system counter (ConfigTools: OsIf > "Use System Timer" ON;
 * the bring-up projects currently have it OFF -> net_hal_rtd_start() returns
 * NOT_CONFIGURED). The counter is read at least every NetNode_Step() and in
 * the CAN ISR; keep the superloop period well below the counter wrap time.
 * Alternative: define NET_BOARD_NOW_MS_HOOK to a function returning uint64_t
 * monotonic ms (e.g. a PIT/STM based counter you already own). */
#ifndef NET_BOARD_TIME_COUNTER
#define NET_BOARD_TIME_COUNTER          OSIF_COUNTER_SYSTEM
#endif
/* #define NET_BOARD_NOW_MS_HOOK  Board_GetMonotonicMs */
#ifdef NET_BOARD_NOW_MS_HOOK
#include <stdint.h>
/* Board-owned monotonic ms clock (e.g. SysTick 1 kHz counter). Called from the
 * CAN ISR and from NetNode_Step(); must be safe in both contexts. */
extern uint64_t NET_BOARD_NOW_MS_HOOK(void);
#endif

/* ---- Critical section (ISR <-> foreground) ---- */
#if defined(__arm__) || defined(__ARM_ARCH)
static inline uint32_t net_board_irq_save(void)
{
    uint32_t key;
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r" (key) : : "memory");
    return key;
}
static inline void net_board_irq_restore(uint32_t key)
{
    __asm volatile ("msr primask, %0" : : "r" (key) : "memory");
}
#else
/* Host simulation: the mock SDK runs "ISRs" synchronously on one thread. */
static inline uint32_t net_board_irq_save(void) { return 0u; }
static inline void net_board_irq_restore(uint32_t key) { (void)key; }
#endif

#endif
