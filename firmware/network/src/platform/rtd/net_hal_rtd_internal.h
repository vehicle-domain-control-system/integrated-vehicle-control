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
    NetHalAdapter *adapter;
    uint64_t can_epoch;
    /* time base */
    uint32_t time_ref, ticks_per_ms, tick_rest;
    uint64_t now_ms;
    NetHalRtdDiag diag;
} NetHalRtdCore;

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

/* CAN part, called by the common part. */
void net_hal_rtd_can_reset(void);                   /* from net_hal_rtd_config() */
NetStatus net_hal_rtd_can_prepare(void);            /* controller init, masks, RX MB config, error IRQs */
NetStatus net_hal_rtd_can_arm(void);                /* after started=true: arm RX MBs, start mode */
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
NetStatus net_hal_rtd_uart_submit(const NetHalTransfer *t);
void net_hal_rtd_uart_service(uint64_t now_ms);     /* transfer timeout, waiting transfer, RX re-arm */
uint32_t net_hal_rtd_uart_drain(uint32_t budget);   /* main: TX facts + at most budget received bytes */
/* Main, from the Port poll (net_hal_rtd_port): at most budget received bytes
 * (and loss marks) whose stamp lies at or before limit_ms, in ring order; a
 * later entry and everything behind it stay for the next poll or pass. */
uint32_t net_hal_rtd_uart_drain_until(uint32_t budget, uint64_t limit_ms);

#endif
