#ifndef NETWORK_NET_HAL_RTD_H
#define NETWORK_NET_HAL_RTD_H

/* L05/M08 RTD backend for the SDK-neutral NetHalAdapter (S32K1/S32K3 RTD,
 * FlexCAN_Ip + OsIf). This header is SDK-free; only the files in
 * src/platform/rtd include RTD headers (net_hal_rtd.c: time base, start,
 * backend ops; net_hal_rtd_can.c: FlexCAN).
 *
 * Execution model (docs/network/network_sw_architecture.md section 8: short
 * CAN RX ISR -> static CAN RX queue -> Network superloop):
 *   ISR  : RTD FlexCAN callback -> copy frame + raw time stamp into the backend
 *          ISR ring -> re-arm MB. TX_COMPLETE and bus-off: fact + stamp into
 *          the same ring. The ISR never calls net_hal_capture_*().
 *   Main : NetNode_Step() -> net_hal_rtd_drain() takes at most
 *          NET_BOARD_ISR_DRAIN_BUDGET ring entries, converts each stamp to
 *          Network ms and calls net_hal_capture_*(); net_upper_step() then polls
 *          the adapter queue; net_hal_rtd_service() supervises stuck TX MBs.
 * Time base (net_board_cfg.h): A) application tick hooks, Network now set by
 * net_hal_rtd_set_now() from Net_MainFunction(); B) NET_BOARD_NOW_MS_HOOK;
 * C) OsIf system counter. */
#include "network/net_hal_adapter.h"

typedef struct {
    uint64_t can_port_epoch; /* nonzero; changes only on a deliberate link reset */
} NetHalRtdConfig;

/* Start step reported in NetHalRtdDiag.init_failed_step (0 = none). */
typedef enum {
    NET_HAL_RTD_STEP_NONE = 0, NET_HAL_RTD_STEP_TIME, NET_HAL_RTD_STEP_INIT, NET_HAL_RTD_STEP_MASK_TYPE,
    NET_HAL_RTD_STEP_MASK, NET_HAL_RTD_STEP_RX_CONFIG, NET_HAL_RTD_STEP_ERROR_INT,
    NET_HAL_RTD_STEP_RX_ARM, NET_HAL_RTD_STEP_START
} NetHalRtdStep;

/* Time base selected at compile time (NetHalRtdDiag.time_source). */
typedef enum {
    NET_HAL_RTD_TIME_OSIF = 0,      /* OsIf system counter */
    NET_HAL_RTD_TIME_NOW_MS_HOOK,   /* NET_BOARD_NOW_MS_HOOK */
    NET_HAL_RTD_TIME_APP_TICK       /* NET_BOARD_ISR_TICK_HOOK + NET_BOARD_TICK_TO_MS_HOOK */
} NetHalRtdTimeSource;

typedef struct {
    bool started;
    uint8_t time_source;        /* NetHalRtdTimeSource */
    uint8_t init_failed_step;   /* NetHalRtdStep */
    uint32_t init_rtd_status;   /* raw RTD status of the failed step */
    uint32_t ticks_per_ms;
    uint32_t rx_frames, rx_queue_rejected, rx_overruns, rx_rearm_errors, rx_length_mismatch;
    uint32_t tx_sent, tx_busy, tx_send_errors, tx_complete, tx_timeout_aborts;
    uint32_t err_events, busoff_events, last_error_status, unexpected_events;
    uint8_t tx_error_counter, rx_error_counter;
    /* ISR ring (ISR -> main hand-off) */
    uint32_t isr_ring_capacity;   /* received frames held between two main passes */
    uint32_t isr_drain_budget;    /* ring entries taken per main pass */
    uint32_t isr_ring_overflow;   /* received frames dropped: ring full */
    uint32_t isr_ring_max_depth;  /* most entries ever waiting in the ring */
    uint32_t main_drained_max;    /* most entries taken in one main pass */
    uint32_t tick_future_clamped; /* stamps ahead of Network now (or not placeable on
                                     its 64-bit axis): delivered at now */
} NetHalRtdDiag;

/* Fill the NetHalConfig (ops/context/links/epoch) for net_s32k344_upper_port_init()
 * or net_s32k144_upper_port_init(). The adapter pointer is kept for the
 * main-context hand-off (net_hal_rtd_drain, TX admission, TX abort). */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out);

/* Initialize FlexCAN from the generated config, arm RX MBs, start the controller.
 * Call after the adapter and the upper service are initialized and after
 * Clock/Port/IntCtrl/OsIf init. Time base C (OsIf) only: returns
 * NOT_CONFIGURED when the OsIf system timer is disabled (no millisecond time
 * base). Time bases A and B do not use OsIf. */
NetStatus net_hal_rtd_start(void);

/* Foreground supervision: abort TX MBs older than NET_BOARD_CAN_TX_TIMEOUT_MS
 * (no ACK on the bench) and report FAILED + released. Read error counters. */
void net_hal_rtd_service(void);

/* Main context only. Application clock (time base A): now_ms becomes the
 * backend "now" (never moves backwards) and returns true. Time bases B and C
 * keep their own clock: the call is ignored and returns false. */
bool net_hal_rtd_set_now(uint64_t now_ms);

/* Main context only (first thing in NetNode_Step()). Takes at most
 * NET_BOARD_ISR_DRAIN_BUDGET entries from the ISR ring in arrival order,
 * converts their stamps and hands them to the adapter (net_hal_capture_*).
 * Returns the number of entries taken. */
uint32_t net_hal_rtd_drain(void);

NetStatus net_hal_rtd_diag(NetHalRtdDiag *out);

#endif
