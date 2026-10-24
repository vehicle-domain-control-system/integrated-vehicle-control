#ifndef NETWORK_NET_HAL_RTD_H
#define NETWORK_NET_HAL_RTD_H

/* L05/M08 RTD backend for the SDK-neutral NetHalAdapter (S32K1/S32K3 RTD,
 * FlexCAN_Ip + Lpuart_Uart_Ip + OsIf). This header is SDK-free; only the
 * files in src/platform/rtd include RTD headers (net_hal_rtd.c: time base,
 * start, backend ops; net_hal_rtd_can.c: FlexCAN; net_hal_rtd_uart.c: LPUART
 * links of the node profile, board callback entry in net_hal_rtd_uart.h).
 *
 * Execution model (docs/network/network_sw_architecture.md section 8: short
 * CAN RX ISR -> static CAN RX queue -> Network superloop):
 *   ISR  : RTD FlexCAN callback -> copy frame + raw time stamp into the backend
 *          ISR ring -> re-arm MB. TX_COMPLETE and bus-off: fact + stamp into
 *          the same ring; a CAN frame lost (ring full, MB overrun) is counted
 *          and the next stored frame is delivered as received. LPUART RX_FULL
 *          -> byte + raw stamp into the link's byte ring -> re-arm the
 *          one-byte reception; END_TRANSFER: TX fact + stamp; ERROR: counted,
 *          reported as a loss mark before the next byte, reception re-armed.
 *          The ISR never calls net_hal_capture_*().
 *   Main : NetNode_Step() -> net_hal_rtd_drain() takes at most
 *          NET_BOARD_ISR_DRAIN_BUDGET ring entries (CAN frames first, then
 *          UART bytes), converts each stamp to Network ms and calls
 *          net_hal_capture_*(); net_upper_step() then polls the adapter queue
 *          through net_hal_rtd_port(), which takes waiting UART bytes on
 *          demand; net_hal_rtd_service() supervises stuck TX MBs and UART
 *          transfers.
 * Frame assembly, CRC and resynchronisation of UART bytes are M06 (L04), not
 * the backend.
 * Time base (net_board_cfg.h): A) application tick hooks, Network now set by
 * net_hal_rtd_set_now() from Net_MainFunction(); B) NET_BOARD_NOW_MS_HOOK;
 * C) OsIf system counter. */
#include "network/net_hal_adapter.h"

typedef struct {
    uint64_t can_port_epoch; /* nonzero; changes only on a deliberate link reset */
    /* Nonzero for each UART link the node profile compiles in (net_board_cfg.h:
     * UART-M in HOST/DOMAIN, UART-C in HOST/CIS); ignored otherwise. */
    uint64_t uart_m_port_epoch, uart_c_port_epoch;
} NetHalRtdConfig;

/* Start step reported in NetHalRtdDiag.init_failed_step (0 = none). */
typedef enum {
    NET_HAL_RTD_STEP_NONE = 0, NET_HAL_RTD_STEP_TIME, NET_HAL_RTD_STEP_INIT, NET_HAL_RTD_STEP_MASK_TYPE,
    NET_HAL_RTD_STEP_MASK, NET_HAL_RTD_STEP_RX_CONFIG, NET_HAL_RTD_STEP_ERROR_INT,
    NET_HAL_RTD_STEP_RX_ARM, NET_HAL_RTD_STEP_START,
    NET_HAL_RTD_STEP_UART_RX_ARM    /* first Lpuart_Uart_Ip_AsyncReceive (Init returns void) */
} NetHalRtdStep;

/* Per UART link counters (NetHalRtdDiag.uart[0] = UART-M, [1] = UART-C). */
typedef struct {
    bool uart_present;              /* link compiled in for this profile */
    bool uart_rx_armed;             /* one-byte reception currently armed */
    uint32_t uart_rx_bytes;         /* RX_FULL bytes seen in the ISR */
    uint32_t uart_rx_errors;        /* framing / noise / parity / other receive errors */
    uint32_t uart_rx_overrun;       /* receive overrun errors */
    uint32_t uart_rx_rearm_errors;  /* AsyncReceive refused after an error (retried by service) */
    uint32_t uart_rx_queue_rejected;/* adapter refused a drained byte */
    uint32_t uart_rx_loss_marks;    /* loss / error captures reported before the next byte */
    uint32_t uart_isr_ring_overflow;/* bytes dropped: ISR ring full */
    uint32_t uart_isr_ring_max_depth;
    uint32_t uart_last_error_status;/* raw Lpuart_Uart_Ip_GetReceiveStatus() of the last error */
    uint32_t uart_tx_sent;          /* AsyncSend accepted (admission reported) */
    uint32_t uart_tx_waiting;       /* transfers that waited for the link to become free */
    uint32_t uart_tx_waiting_max;   /* most transfers waiting at once */
    uint32_t uart_tx_complete;      /* END_TRANSFER with transmit status SUCCESS */
    uint32_t uart_tx_timeout_aborts;/* AbortSendingData after NET_BOARD_UART_x_TX_TIMEOUT_MS */
    uint32_t uart_tx_abort_errors;  /* of those, AbortSendingData not SUCCESS */
    uint32_t uart_tx_abort_last_status; /* raw Lpuart_Uart_Ip_StatusType of the last abort */
    uint32_t uart_rx_idle_events;   /* receive idle-line events (timeout interrupt builds): re-armed */
    uint32_t uart_tx_send_errors;   /* AsyncSend refused */
    uint32_t uart_tx_expired;       /* deadline passed before the transfer could start */
} NetHalRtdUartDiag;

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
    /* CAN frames stored right after a loss (ring full or MB overrun): loss
     * boundaries. The stored frame itself is complete and delivered. */
    uint32_t rx_loss_events;
    uint32_t tx_sent, tx_busy, tx_send_errors, tx_complete;
    /* TX MB timeout (NET_BOARD_CAN_TX_TIMEOUT_MS) and its abort result:
     * tx_timeout_aborts: aborts of a transfer not completed before them;
     * tx_abort_already_sent: of those, NO_TRANSFER_IN_PROGRESS (the frame had
     *   gone out): reported SUCCEEDED, not FAILED;
     * tx_abort_timeouts: of those, TIMEOUT: reported FAILED, the MB kept out
     *   of use until its flag shows it idle (tx_quarantine_released);
     * tx_abort_last_status: raw Flexcan_Ip_StatusType of the last abort. */
    uint32_t tx_timeout_aborts, tx_abort_already_sent, tx_abort_timeouts, tx_quarantine_released;
    uint32_t tx_abort_last_status;
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
    /* LPUART links (net_hal_rtd_uart.c) */
    uint32_t uart_isr_ring_capacity;  /* received bytes held per link between two main passes */
    uint32_t uart_unexpected_instance;/* events of an instance that is no Network UART link */
    uint32_t uart_poll_drained;       /* ring entries the Port poll took on demand (net_hal_rtd_port) */
    NetHalRtdUartDiag uart[2];        /* [0] UART-M, [1] UART-C */
} NetHalRtdDiag;

/* Fill the NetHalConfig (ops/context/links/epoch) for net_hal_adapter_init()
 * (the node root passes NETWORK_NODE_PLATFORM). The adapter pointer is kept for the
 * main-context hand-off (net_hal_rtd_drain, TX admission, TX abort). */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out);

/* Initialize FlexCAN from the generated config, arm RX MBs, start the controller;
 * initialize the profile's LPUART links and arm their one-byte reception
 * (the first AsyncReceive result tells whether a link is ready).
 * Call after the adapter and the upper service are initialized and after
 * Clock/Port/IntCtrl/OsIf init. Time base C (OsIf) only: returns
 * NOT_CONFIGURED when the OsIf system timer is disabled (no millisecond time
 * base). Time bases A and B do not use OsIf. */
NetStatus net_hal_rtd_start(void);

/* Foreground supervision: abort TX MBs older than NET_BOARD_CAN_TX_TIMEOUT_MS
 * (no ACK on the bench) and UART transfers older than the link timeout, report
 * them FAILED + released, start a waiting UART transfer, re-arm a UART
 * reception that stopped. Read error counters. */
void net_hal_rtd_service(void);

/* Main context only. Application clock (time base A): now_ms becomes the
 * backend "now" (never moves backwards) and returns true. Time bases B and C
 * keep their own clock: the call is ignored and returns false. */
bool net_hal_rtd_set_now(uint64_t now_ms);

/* Main context only (first thing in NetNode_Step()). Takes at most
 * NET_BOARD_ISR_DRAIN_BUDGET entries from the ISR rings in arrival order per
 * ring (CAN frames first, then UART bytes), converts their stamps and hands
 * them to the adapter (net_hal_capture_*). Returns the number of entries taken. */
uint32_t net_hal_rtd_drain(void);

/* The NetUpperPort the service uses: the adapter port (net_hal_adapter_port)
 * whose poll does not report "no work" while a received UART byte captured at
 * or before the service's clock reading still waits in an ISR byte ring. The
 * M06 receiver runs its timer check only after the Port reports no work
 * (net_upper_uart.h, receiver poll), and refuses a byte captured before that
 * check; so such a byte is taken on demand (at most NET_BOARD_ISR_DRAIN_BUDGET
 * per poll, the service's poll budget bounds a step) instead of waiting for
 * the next pass. Profiles without a UART link: the adapter port as it is.
 * Call after net_hal_rtd_config(); the adapter must outlive the service. */
NetUpperPort net_hal_rtd_port(NetHalAdapter *adapter);

NetStatus net_hal_rtd_diag(NetHalRtdDiag *out);

#endif
