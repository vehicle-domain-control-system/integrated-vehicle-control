#ifndef NET_BOARD_CFG_H
#define NET_BOARD_CFG_H

/* ==========================================================================
 * Board binding for the RTD HAL (src/platform/rtd: net_hal_rtd.c common part,
 * net_hal_rtd_can.c FlexCAN part, net_hal_rtd_uart.c LPUART part) and for the
 * public entry (src/node/net_entry.c).
 * This is the ONLY file to edit when the HAL meets a real S32DS project.
 * Defaults equal network_344_can_bringup (S32K344, RTD 7.0.1, CAN0
 * PTA6/PTA7, FD+BRS 500k/2M, 64 B payload, max_num_mb 16).
 * Every value can also be overridden with -D in the project settings.
 * ========================================================================== */

/* ---- MCU: NETWORK_NODE_PLATFORM (144 = S32K144, 344 = S32K344) ----
 * The node role (NETWORK_UPPER_NODE_PROFILE) does not choose the MCU. An S32DS
 * project chooses it with its CPU define (CPU_S32K144HFT0VLLT or CPU_S32K344);
 * -DNETWORK_NODE_PLATFORM=144 or 344 chooses it explicitly (host builds, other
 * CPU part defines). Both CPU defines, another value, or a value that differs
 * from the CPU define are compile errors. Without any choice the macro stays
 * undefined and the node composition root (src/node/net_node.c) stops the build. */
#if defined(CPU_S32K344) && defined(CPU_S32K144HFT0VLLT)
#error "net_board_cfg.h: CPU_S32K344 and CPU_S32K144HFT0VLLT are both defined"
#endif
#ifndef NETWORK_NODE_PLATFORM
#if defined(CPU_S32K344)
#define NETWORK_NODE_PLATFORM 344
#elif defined(CPU_S32K144HFT0VLLT)
#define NETWORK_NODE_PLATFORM 144
#endif
#endif
#ifdef NETWORK_NODE_PLATFORM
#if NETWORK_NODE_PLATFORM != 144 && NETWORK_NODE_PLATFORM != 344
#error "net_board_cfg.h: NETWORK_NODE_PLATFORM must be 144 or 344"
#elif (defined(CPU_S32K344) && NETWORK_NODE_PLATFORM != 344) || \
      (defined(CPU_S32K144HFT0VLLT) && NETWORK_NODE_PLATFORM != 144)
#error "net_board_cfg.h: NETWORK_NODE_PLATFORM differs from the S32DS CPU define"
#endif
#endif

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
/* Header that declares the two symbols above. Default FlexCAN_Ip.h: RTD
 * includes FlexCAN_Ip_DeviceReg.h -> FlexCAN_Ip_Cfg.h -> the generated
 * PBcfg header (any Functional Group suffix, e.g. FlexCAN_Ip_Sa_VS_0_PBcfg.h)
 * and expands its extern declarations, so no quoted -D is needed. */
#ifndef NET_BOARD_CAN_CFG_HEADER
#define NET_BOARD_CAN_CFG_HEADER        "FlexCAN_Ip.h"
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

/* ---- LPUART links (net_hal_rtd_uart.c) ----
 * The node profile decides which UART links exist (net_upper_profile.h):
 *   UART-M (ESP32 <-> DOMAIN): HOST and DOMAIN profiles
 *   UART-C (Pi <-> CIS)      : HOST and CIS profiles
 * A link absent from the profile is not compiled at all (no LPUART symbol is
 * referenced). Instance numbers and generated-config symbols are the
 * ConfigTools names. Defaults equal the DS344 CIS bench: UART-C on LPUART11
 * (Lpuart_Uart_Ip_xHwConfigPB_11, wired to the S32K144 peer). UART-M has no
 * board yet; its default is a placeholder to be set by the DOMAIN project. */
#include "network/net_upper_profile.h"
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST || NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
#define NET_BOARD_UART_M_LINK 1
#else
#define NET_BOARD_UART_M_LINK 0
#endif
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST || NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#define NET_BOARD_UART_C_LINK 1
#else
#define NET_BOARD_UART_C_LINK 0
#endif
#ifndef NET_BOARD_UART_M_INSTANCE
#define NET_BOARD_UART_M_INSTANCE       0U
#endif
#ifndef NET_BOARD_UART_M_CONFIG
#define NET_BOARD_UART_M_CONFIG         Lpuart_Uart_Ip_xHwConfigPB_0
#endif
#ifndef NET_BOARD_UART_C_INSTANCE
#define NET_BOARD_UART_C_INSTANCE       11U
#endif
#ifndef NET_BOARD_UART_C_CONFIG
#define NET_BOARD_UART_C_CONFIG         Lpuart_Uart_Ip_xHwConfigPB_11
#endif
/* Generated config header that declares the configuration symbols above. */
#ifndef NET_BOARD_UART_CFG_HEADER
#define NET_BOARD_UART_CFG_HEADER       "Lpuart_Uart_Ip_Sa_PBcfg.h"
#endif
/* A frame transfer (AsyncSend) not ended within this time is aborted
 * (AbortSendingData) and reported FAILED + released. One transfer per link at
 * a time; 138 B at 115200 bit/s take about 12 ms. */
#ifndef NET_BOARD_UART_TX_TIMEOUT_MS
#define NET_BOARD_UART_TX_TIMEOUT_MS    100U
#endif
#ifndef NET_BOARD_UART_M_TX_TIMEOUT_MS
#define NET_BOARD_UART_M_TX_TIMEOUT_MS  NET_BOARD_UART_TX_TIMEOUT_MS
#endif
#ifndef NET_BOARD_UART_C_TX_TIMEOUT_MS
#define NET_BOARD_UART_C_TX_TIMEOUT_MS  NET_BOARD_UART_TX_TIMEOUT_MS
#endif
/* Received bytes the per-link ISR ring holds until the main context takes
 * them (net_hal_rtd_drain, then the service's Port poll on demand). A byte
 * that finds the ring full is dropped, counted and reported as a loss
 * capture (NetUpperCapture.overflow) right before the next stored byte;
 * 256 B is about 22 ms of a fully loaded 115200 bit/s line. */
#ifndef NET_BOARD_UART_ISR_RING
#define NET_BOARD_UART_ISR_RING         256U
#endif
/* Callback wiring of the generated LPUART config (ConfigTools callback name):
 *  - NET_BOARD_UART_CALLBACK defined: the HAL DEFINES a function with that
 *    name and handles every instance that uses it (other instances are
 *    ignored and counted, NetHalRtdDiag.uart_unexpected_instance).
 *  - not defined (routing): the HAL only provides
 *      void NetHalRtdUart_OnEvent(uint8 instance, Lpuart_Uart_Ip_EventType event);
 *    (net_hal_rtd_uart.h) and board code calls it from its own callback for
 *    the Network instance. DS344 uses this mode: LPUART0 (console) and
 *    LPUART11 share Uart344_BringupCallback, which keeps the console and
 *    hands instance 11 to NetHalRtdUart_OnEvent(). */
/* #define NET_BOARD_UART_CALLBACK  Uart344_BringupCallback */
#if NET_BOARD_UART_M_LINK && NET_BOARD_UART_C_LINK
typedef char net_board_uart_instances_distinct[(NET_BOARD_UART_M_INSTANCE != NET_BOARD_UART_C_INSTANCE) ? 1 : -1];
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
 * the ring full is dropped and counted (NetHalRtdDiag.isr_ring_overflow); an
 * MB overrun is counted too (rx_overruns). The next stored frame is complete
 * and is delivered as received; the loss boundary is counted once
 * (rx_loss_events). CAN_PORT_RECOVERY.md: a real loss is recorded, nothing is
 * synthesized, the link stays up.
 * NET_BOARD_ISR_DRAIN_BUDGET: ring entries taken per main pass, keeps one
 * Net_MainFunction() short (VSS contract: a few ms, never above 15 ms). It
 * must not exceed the adapter RX queue (NET_HAL_RX_CAPACITY, 16), which every
 * NetNode_Step() empties. The budget is shared: CAN ring entries first, then
 * received UART bytes (one adapter event per byte), up to the same total.
 * UART: M06 accepts no byte captured before its last drained timer check. The
 * service's Port (net_hal_rtd_port) therefore takes the bytes still waiting
 * in the ring, captured at or before the service's clock reading, before that
 * check (at most this budget per poll; the service's poll budget,
 * NET_UPPER_POLL_BUDGET 32 events per step, two steps per NetNode_Step(),
 * bounds a pass). A byte is lost only when the byte ring overflows: keep the
 * main pass period below the ring time (256 B: about 22 ms of a fully loaded
 * 115200 bit/s line) and the average line load below 64 B per pass. */
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
