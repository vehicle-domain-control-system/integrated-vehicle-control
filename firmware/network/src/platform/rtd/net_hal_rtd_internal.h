#ifndef NET_HAL_RTD_INTERNAL_H
#define NET_HAL_RTD_INTERNAL_H

/* Private glue between the parts of the L05/M08 RTD backend
 * (src/platform/rtd only; NOT a public header):
 *   net_hal_rtd.c      common part: time base, critical section, start order,
 *                      NetHalBackendOps registration, per-link submit dispatch
 *   net_hal_rtd_can.c  FlexCAN part: RX ISR capture, TX MBs, abort, error callback
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
NetUpperCapture net_hal_rtd_capture_now(uint64_t port_epoch); /* takes the critical section */

/* CAN part, called by the common part. */
void net_hal_rtd_can_reset(void);                   /* from net_hal_rtd_config() */
NetStatus net_hal_rtd_can_prepare(void);            /* controller init, masks, RX MB config, error IRQs */
NetStatus net_hal_rtd_can_arm(void);                /* after started=true: arm RX MBs, start mode */
NetStatus net_hal_rtd_can_submit(const NetHalTransfer *t);
void net_hal_rtd_can_service(uint64_t now_ms);      /* stuck-TX abort, error counters */

#endif
