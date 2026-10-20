#ifndef NET_BOARD_CFG_H
#define NET_BOARD_CFG_H

/* ==========================================================================
 * Board binding for the RTD HAL (src/platform/rtd: net_hal_rtd.c common part,
 * net_hal_rtd_can.c FlexCAN part) and for the public entry (src/node/net_entry.c).
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

/* ---- Time base: pick ONE of three ----
 * The CAN ISR never hands a frame to the Network core. It stores the frame,
 * a TX completion or a bus-off fact together with a raw time stamp in the
 * backend ISR ring (below); the main context (NetNode_Step(), i.e. inside
 * Net_MainFunction()) takes the ring entries out, turns the stamp into Network
 * milliseconds and only then calls net_hal_capture_*().
 *
 * A) Application clock (board builds; VSS contract: received_at_ms is on the
 *    VSS clock axis). Define BOTH
 *      NET_BOARD_ISR_TICK_HOOK    uint32_t f(void)          read in the ISR
 *      NET_BOARD_TICK_TO_MS_HOOK  uint32_t f(uint32_t tick) main context only
 *    The ISR records NET_BOARD_ISR_TICK_HOOK() and nothing else; the tick is
 *    converted with NET_BOARD_TICK_TO_MS_HOOK() in the main context. Network
 *    "now" is the nowMs of the last Net_MainFunction() (32-bit wrap extended
 *    to 64 bits); no millisecond counter of its own. A converted time ahead of
 *    that now is clamped to now and counted (NetHalRtdDiag.tick_future_clamped).
 *      VSS:            HalTimer_GetTicks / VssTime_TickToMs32 (1 tick = 1.25 ms)
 *      Bench w/o VSS:  a SysTick 1 kHz ms counter as tick, identity conversion
 * B) NET_BOARD_NOW_MS_HOOK: a board ms clock returning uint64_t, read in the ISR
 *    and in the main context (bench / host). The ISR stores its milliseconds as
 *    the tick; the conversion is the identity.
 * C) Default: OsIf system counter (ConfigTools: OsIf > "Use System Timer" ON;
 *    the bring-up projects currently have it OFF -> net_hal_rtd_start() returns
 *    NOT_CONFIGURED). Read in the ISR and in the main context; keep the
 *    superloop period well below the counter wrap time. Milliseconds as tick. */
#ifndef NET_BOARD_TIME_COUNTER
#define NET_BOARD_TIME_COUNTER          OSIF_COUNTER_SYSTEM
#endif
/* #define NET_BOARD_ISR_TICK_HOOK    HalTimer_GetTicks   */
/* #define NET_BOARD_TICK_TO_MS_HOOK  VssTime_TickToMs32  */
/* #define NET_BOARD_NOW_MS_HOOK      Board_GetMonotonicMs */
#include <stdint.h>
#if defined(NET_BOARD_ISR_TICK_HOOK) && defined(NET_BOARD_TICK_TO_MS_HOOK)
#ifdef NET_BOARD_NOW_MS_HOOK
#error "net_board_cfg.h: NET_BOARD_NOW_MS_HOOK cannot be combined with the tick hooks"
#endif
#define NET_BOARD_TIME_APP_TICK 1
/* Free-running board tick; reads a value only, safe in an ISR. */
extern uint32_t NET_BOARD_ISR_TICK_HOOK(void);
/* Tick -> application milliseconds (32-bit axis). Main context only. */
extern uint32_t NET_BOARD_TICK_TO_MS_HOOK(uint32_t tick);
#elif defined(NET_BOARD_ISR_TICK_HOOK) || defined(NET_BOARD_TICK_TO_MS_HOOK)
#error "net_board_cfg.h: define both NET_BOARD_ISR_TICK_HOOK and NET_BOARD_TICK_TO_MS_HOOK, or neither"
#endif
#ifdef NET_BOARD_NOW_MS_HOOK
/* Board-owned monotonic ms clock (e.g. SysTick 1 kHz counter). Called from the
 * CAN ISR and from NetNode_Step(); must be safe in both contexts. */
extern uint64_t NET_BOARD_NOW_MS_HOOK(void);
#endif

/* ---- CAN ISR ring (ISR -> main hand-off) ----
 * NET_BOARD_CAN_ISR_RING: received frames the ring holds between two main
 * passes. TX completion and bus-off facts have their own reserved room on top
 * (one per TX MB, one bus-off), so they are never dropped. A frame that finds
 * the ring full is dropped, counted (NetHalRtdDiag.isr_ring_overflow) and
 * flagged on the next captured frame (NetUpperCapture.overflow).
 * NET_BOARD_ISR_DRAIN_BUDGET: ring entries taken per main pass, keeps one
 * Net_MainFunction() short (VSS contract: a few ms, never above 15 ms). It
 * must not exceed the adapter RX queue (NET_HAL_RX_CAPACITY, 16), which every
 * NetNode_Step() empties. */
#ifndef NET_BOARD_CAN_ISR_RING
#define NET_BOARD_CAN_ISR_RING          32U
#endif
#ifndef NET_BOARD_ISR_DRAIN_BUDGET
#define NET_BOARD_ISR_DRAIN_BUDGET      16U
#endif

/* ---- Public entry hooks (src/node/net_entry.c) ----
 * NET_BOARD_NODE_CONFIG_HOOK  bool f(NetNodeConfig *out)  (required for Net_Init)
 *   Net_Init(void) has no argument: BOOT_ID and the epochs come from here.
 *   out arrives with boot_id 0 and both epochs 1; the hook must set a nonzero
 *   BOOT_ID that changes on every reboot. false or BOOT_ID 0 -> Net_Init fails
 *   (NetNodeDiag.entry_init_result) and Net_MainFunction() stays inert.
 * NET_BOARD_US_HOOK  uint32_t f(void)  (optional)
 *   Free-running microsecond counter. When defined, the longest
 *   Net_MainFunction() pass is kept in NetNodeDiag.main_max_us; otherwise 0. */
/* #define NET_BOARD_NODE_CONFIG_HOOK  Board_GetNetNodeConfig */
/* #define NET_BOARD_US_HOOK           Board_GetMicros        */
#ifdef NET_BOARD_US_HOOK
extern uint32_t NET_BOARD_US_HOOK(void);
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
