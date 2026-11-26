#ifndef NET_HAL_RTD_INTERNAL_H
#define NET_HAL_RTD_INTERNAL_H

/* Private glue between the parts of the L05/M08 RTD backend
 * (src/platform/rtd only; NOT a public header):
 *   net_hal_rtd.c      common part: time base, stamp conversion, critical
 *                      section, start order, NetHalBackendOps registration,
 *                      per-link submit dispatch, ISR ring drain entry
 *   net_hal_rtd_can.c  FlexCAN part: RX/TX/error ISR -> ISR ring, ring drain
 *                      into the adapter, TX MBs, abort
 *   net_hal_rtd_uart.c LPUART part (only the profile's UART links): byte ISR
 *                      ring, drain into the adapter, one transfer per link
 * The link parts never include each other; they share only what is below. */
#include <stdint.h>
#include <stdbool.h>
#include "network/net_hal_rtd.h"

/* State shared by every link part. Owned by net_hal_rtd.c. */
typedef struct {
    bool configured, started;   /* started: link callbacks may report from here on */
    /* Start is one-shot after a hardware failure.  rollback_* is deliberately
     * private: it records cleanup without changing the public Network ABI. */
    bool init_in_progress, start_failed;
    bool rollback_in_progress, rollback_attempted, rollback_succeeded;
    uint8_t rollback_failed_step;
    uint32_t rollback_rtd_status;
    NetHalAdapter *adapter;
    uint64_t can_epoch;
    /* time base */
    uint32_t time_ref, ticks_per_ms, tick_rest;
    uint64_t now_ms;
    NetHalRtdDiag diag;
} NetHalRtdCore;

/* Private backend state used by the public entry to distinguish an ordinary
 * initialization failure from a cleanup failure without exposing RTD details
 * or changing an existing public diagnostic structure. */
typedef enum {
    NET_HAL_RTD_HW_NOT_INITIALIZED = 0,
    NET_HAL_RTD_HW_INITIALIZING,
    NET_HAL_RTD_HW_PREPARED,
    NET_HAL_RTD_HW_FAILED,
    NET_HAL_RTD_HW_ROLLBACK_FAILED
} NetHalRtdHardwareInitStatus;

typedef enum {
    NET_HAL_RTD_ROLLBACK_NONE = 0,
    NET_HAL_RTD_ROLLBACK_UART_ABORT_TX,
    NET_HAL_RTD_ROLLBACK_UART_ABORT_RX,
    NET_HAL_RTD_ROLLBACK_UART_DEINIT,
    NET_HAL_RTD_ROLLBACK_CAN_ABORT_RX,
    NET_HAL_RTD_ROLLBACK_CAN_ABORT_TX,
    NET_HAL_RTD_ROLLBACK_CAN_STOP,
    NET_HAL_RTD_ROLLBACK_CAN_DEINIT
} NetHalRtdRollbackStep;

typedef enum {
    NET_HAL_RTD_OWNER_NONE = 0,
    NET_HAL_RTD_OWNER_PRODUCTION,
    NET_HAL_RTD_OWNER_UART_DIAGNOSTIC,
    NET_HAL_RTD_OWNER_UNCERTAIN
} NetHalRtdOwner;

extern NetHalRtdCore net_hal_rtd_core;

/* Common part, called by the link parts. */
uint64_t net_hal_rtd_now_locked(void);              /* caller holds the critical section */
NetUpperCapture net_hal_rtd_capture_now(uint64_t port_epoch); /* main context; takes the critical section */
/* ISR context, caller holds the critical section: the raw stamp stored with a
 * ring entry. Time base A: NET_BOARD_ISR_TICK_HOOK(). B/C: Network ms (low 32 bits). */
uint32_t net_hal_rtd_isr_stamp_locked(void);
/* Main context: a stored stamp as Network ms on the 64-bit axis. Time base A
 * converts with NET_BOARD_TICK_TO_MS_HOOK(), B/C use the stamp as ms. A time
 * ahead of now gives now and counts tick_future_clamped (largest lead
 * tick_ahead_max_ms); one before the axis start gives now and counts
 * tick_unplaceable. */
uint64_t net_hal_rtd_place_stamp(uint32_t stamp);
void net_hal_rtd_record_rollback_failure(NetHalRtdRollbackStep step, uint32_t status);
NetHalRtdHardwareInitStatus net_hal_rtd_hardware_init_status(void);
bool net_hal_rtd_owner_claim(NetHalRtdOwner owner);
bool net_hal_rtd_owner_release(NetHalRtdOwner owner, bool cleanup_succeeded);
NetHalRtdOwner net_hal_rtd_owner_get(void);

/* CAN part, called by the common part. */
void net_hal_rtd_can_reset(void);                   /* from net_hal_rtd_config() */
NetStatus net_hal_rtd_can_prepare(void);            /* controller init, masks, RX MB config, error IRQs */
NetStatus net_hal_rtd_can_arm(void);                /* after started=true: arm RX MBs, start mode */
bool net_hal_rtd_can_rollback(void);                 /* main context, callbacks already gated */
NetStatus net_hal_rtd_can_submit(const NetHalTransfer *t);
void net_hal_rtd_can_service(uint64_t now_ms);      /* stuck-TX abort, error counters */
uint32_t net_hal_rtd_can_drain(uint32_t budget);    /* main: ISR ring -> adapter, at most budget entries */

/* UART part, called by the common part only when the profile has a UART link
 * (net_board_cfg.h NET_BOARD_UART_M_LINK / NET_BOARD_UART_C_LINK). */
/* From net_hal_rtd_config(): checks the link port epochs, resets the part and
 * adds the links to the adapter configuration. */
NetStatus net_hal_rtd_uart_reset(const NetHalRtdConfig *in, NetHalConfig *out);
void net_hal_rtd_uart_prepare(void);                /* Lpuart_Uart_Ip_Init per link */
NetStatus net_hal_rtd_uart_arm(void);               /* after started=true: first one-byte reception */
bool net_hal_rtd_uart_rollback(void);                /* main context, callbacks already gated */
NetStatus net_hal_rtd_uart_submit(const NetHalTransfer *t);
void net_hal_rtd_uart_service(uint64_t now_ms);     /* transfer timeout, waiting transfer, RX re-arm */
uint32_t net_hal_rtd_uart_drain(uint32_t budget);   /* main: TX facts + at most budget received bytes */
/* Main, from the Port poll (net_hal_rtd_port): at most budget received bytes
 * (and loss marks) whose stamp lies at or before limit_ms, in ring order; a
 * later entry and everything behind it stay for the next poll or pass. */
uint32_t net_hal_rtd_uart_drain_until(uint32_t budget, uint64_t limit_ms);

NetStatus net_hal_rtd_uart_diagnostic_config(NetHalAdapter *adapter, uint64_t port_epoch,
                                              NetHalConfig *out);
NetStatus net_hal_rtd_uart_diagnostic_start(void);
void net_hal_rtd_uart_diagnostic_service(void);
uint32_t net_hal_rtd_uart_diagnostic_drain(void);
NetStatus net_hal_rtd_uart_diagnostic_stop(void);

#endif
