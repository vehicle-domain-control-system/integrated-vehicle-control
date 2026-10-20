#ifndef NETWORK_NET_HAL_RTD_H
#define NETWORK_NET_HAL_RTD_H

/* L05/M08 RTD backend for the SDK-neutral NetHalAdapter (S32K1/S32K3 RTD,
 * FlexCAN_Ip + OsIf). This header is SDK-free; only the files in
 * src/platform/rtd include RTD headers (net_hal_rtd.c: time base, start,
 * backend ops; net_hal_rtd_can.c: FlexCAN).
 *
 * Execution model (matches docs/network/sw_detail/layers/L05):
 *   ISR  : RTD FlexCAN callback -> copy frame -> net_hal_capture_can() -> re-arm MB
 *          TX_COMPLETE -> net_hal_capture_tx(admission+final+release)
 *   Main : NetNode_Step() -> net_upper_step() polls the adapter queue;
 *          net_hal_rtd_service() supervises stuck TX MBs. */
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

typedef struct {
    bool started;
    uint8_t init_failed_step;   /* NetHalRtdStep */
    uint32_t init_rtd_status;   /* raw RTD status of the failed step */
    uint32_t ticks_per_ms;
    uint32_t rx_frames, rx_queue_rejected, rx_overruns, rx_rearm_errors, rx_length_mismatch;
    uint32_t tx_sent, tx_busy, tx_send_errors, tx_complete, tx_timeout_aborts;
    uint32_t err_events, busoff_events, last_error_status, unexpected_events;
    uint8_t tx_error_counter, rx_error_counter;
} NetHalRtdDiag;

/* Fill the NetHalConfig (ops/context/links/epoch) for net_s32k344_upper_port_init()
 * or net_s32k144_upper_port_init(). The adapter pointer is kept for ISR capture. */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out);

/* Initialize FlexCAN from the generated config, arm RX MBs, start the controller.
 * Call after the adapter and the upper service are initialized and after
 * Clock/Port/IntCtrl/OsIf init. Returns NOT_CONFIGURED when the OsIf system
 * timer is disabled (no millisecond time base). */
NetStatus net_hal_rtd_start(void);

/* Foreground supervision: abort TX MBs older than NET_BOARD_CAN_TX_TIMEOUT_MS
 * (no ACK on the bench) and report FAILED + released. Read error counters. */
void net_hal_rtd_service(void);

NetStatus net_hal_rtd_diag(NetHalRtdDiag *out);

#endif
