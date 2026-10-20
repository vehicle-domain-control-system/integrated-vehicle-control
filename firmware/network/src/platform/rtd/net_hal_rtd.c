/* L05/M08 RTD backend, common part (S32K1 RTD 3.x / S32K3 RTD 7.x).
 * Owns what every link shares: the millisecond time base (OsIf or
 * NET_BOARD_NOW_MS_HOOK), the ISR <-> foreground critical section, the start
 * order, the NetHalBackendOps registration and the per-link submit dispatch.
 * Link parts: net_hal_rtd_can.c (FlexCAN). Generated-config names come from
 * net_board_cfg.h. Same file builds against the host mock SDK (sdk_mock/rtd)
 * and the real RTD.
 *
 * RTD symbols used here (verified against both bring-up projects' headers):
 *   OsIf_GetCounter, OsIf_GetElapsed, OsIf_MicrosToTicks. */
#include <stdint.h>
#include <string.h>
#include "network/net_hal_rtd.h"
#include "net_hal_rtd_internal.h"
#include "net_board_cfg.h"
#include "OsIf.h"

NetHalRtdCore net_hal_rtd_core;
#define rtd net_hal_rtd_core

/* ------------------------------------------------------------- time ----- */
uint64_t net_hal_rtd_now_locked(void)
{
#ifdef NET_BOARD_NOW_MS_HOOK
    rtd.now_ms = NET_BOARD_NOW_MS_HOOK();
#else
    uint32_t ticks = OsIf_GetElapsed(&rtd.time_ref, NET_BOARD_TIME_COUNTER);
    uint64_t total = (uint64_t)rtd.tick_rest + ticks;
    rtd.now_ms += total / rtd.ticks_per_ms;
    rtd.tick_rest = (uint32_t)(total % rtd.ticks_per_ms);
#endif
    return rtd.now_ms;
}

NetUpperCapture net_hal_rtd_capture_now(uint64_t port_epoch)
{
    NetUpperCapture c;
    uint32_t key = net_board_irq_save();
    memset(&c, 0, sizeof(c));
    c.port_epoch = port_epoch;
    c.captured_at_ms = net_hal_rtd_now_locked();
    c.timestamp_known = true;
    net_board_irq_restore(key);
    return c;
}

/* ------------------------------------------------ NetHalBackendOps ------ */
static NetStatus ops_now(void *context, uint64_t *out)
{
    uint32_t key;
    (void)context;
    if (out == NULL) return NET_INVALID_ARGUMENT;
    if (!rtd.started) return NET_NOT_CONFIGURED;
    key = net_board_irq_save();
    *out = net_hal_rtd_now_locked();
    net_board_irq_restore(key);
    return NET_OK;
}

static uintptr_t ops_enter(void *context) { (void)context; return (uintptr_t)net_board_irq_save(); }
static void ops_leave(void *context, uintptr_t prior) { (void)context; net_board_irq_restore((uint32_t)prior); }

static NetStatus ops_submit(void *context, const NetHalTransfer *t)
{
    (void)context;
    if (t == NULL) return NET_INVALID_ARGUMENT;
    switch (t->packet.link) {
    case NET_WIRE_CAN:
        if (!rtd.started) return NET_NOT_CONFIGURED;
        return net_hal_rtd_can_submit(t);
    default:
        return NET_NOT_CONFIGURED; /* UART links: no backend part in this build */
    }
}

static const NetHalBackendOps rtd_ops = { ops_now, ops_enter, ops_leave, ops_submit };

/* -------------------------------------------------------- foreground ---- */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out)
{
    if (adapter == NULL || in == NULL || out == NULL || in->can_port_epoch == 0u) return NET_INVALID_ARGUMENT;
    memset(&rtd, 0, sizeof(rtd));
    net_hal_rtd_can_reset();
    rtd.adapter = adapter;
    rtd.can_epoch = in->can_port_epoch;
    rtd.configured = true;
    memset(out, 0, sizeof(*out));
    out->binding_selected = true;
    out->enabled_links = NET_HAL_LINK_BIT(NET_WIRE_CAN);
    out->port_epoch[NET_WIRE_CAN] = in->can_port_epoch;
    out->ops = &rtd_ops;
    out->context = &rtd;
    return NET_OK;
}

NetStatus net_hal_rtd_start(void)
{
    NetStatus st;
    if (!rtd.configured || rtd.started) return NET_INVALID_ARGUMENT;

#ifndef NET_BOARD_NOW_MS_HOOK
    rtd.ticks_per_ms = OsIf_MicrosToTicks(1000U, NET_BOARD_TIME_COUNTER);
    rtd.diag.ticks_per_ms = rtd.ticks_per_ms;
    if (rtd.ticks_per_ms == 0u) { rtd.diag.init_failed_step = NET_HAL_RTD_STEP_TIME; return NET_NOT_CONFIGURED; }
    rtd.time_ref = OsIf_GetCounter(NET_BOARD_TIME_COUNTER);
#endif

    st = net_hal_rtd_can_prepare();
    if (st != NET_OK) return st;
    rtd.started = true; /* callbacks may fire from here on */
    st = net_hal_rtd_can_arm();
    if (st != NET_OK) { rtd.started = false; return st; }
    rtd.diag.started = true;
    return NET_OK;
}

void net_hal_rtd_service(void)
{
    uint32_t key;
    uint64_t now;
    if (!rtd.started) return;
    key = net_board_irq_save();
    now = net_hal_rtd_now_locked();
    net_board_irq_restore(key);
    net_hal_rtd_can_service(now);
}

NetStatus net_hal_rtd_diag(NetHalRtdDiag *out)
{
    uint32_t key;
    if (out == NULL) return NET_INVALID_ARGUMENT;
    key = net_board_irq_save();
    *out = rtd.diag;
    net_board_irq_restore(key);
    return NET_OK;
}
