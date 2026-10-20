/* L05/M08 RTD backend, common part (S32K1 RTD 3.x / S32K3 RTD 7.x).
 * Owns what every link shares: the millisecond time base (application tick
 * hooks, NET_BOARD_NOW_MS_HOOK or OsIf; net_board_cfg.h), the conversion of
 * ISR stamps to Network ms, the ISR <-> foreground critical section, the start
 * order, the NetHalBackendOps registration, the per-link submit dispatch and
 * the bounded ISR ring drain. Link parts: net_hal_rtd_can.c (FlexCAN) and
 * net_hal_rtd_uart.c (LPUART, only the UART links of the node profile).
 * Generated-config names come from net_board_cfg.h. Same file builds against
 * the host mock SDK (sdk_mock/rtd) and the real RTD.
 *
 * Application clock (VSS contract: received_at_ms is on the VSS clock axis,
 * the tick is read in the ISR and converted in the main context only):
 * Network now is the nowMs handed to Net_MainFunction(), set here through
 * net_hal_rtd_set_now(); there is no millisecond counter of its own.
 *
 * RTD symbols used here (verified against both bring-up projects' headers):
 *   OsIf_GetCounter, OsIf_GetElapsed, OsIf_MicrosToTicks (time base C only). */
#include <stdint.h>
#include <string.h>
#include "network/net_hal_rtd.h"
#include "net_hal_rtd_internal.h"
#include "net_board_cfg.h"
#include "OsIf.h"

NetHalRtdCore net_hal_rtd_core;
#define rtd net_hal_rtd_core

/* Drain budget: one main pass never offers the adapter more frames than its
 * RX queue holds (NetNode_Step() empties that queue on every pass). */
typedef char net_board_drain_budget_in_range[(NET_BOARD_ISR_DRAIN_BUDGET >= 1u &&
                                              NET_BOARD_ISR_DRAIN_BUDGET <= NET_HAL_RX_CAPACITY) ? 1 : -1];

#define UART_LINKS (NET_BOARD_UART_M_LINK || NET_BOARD_UART_C_LINK)

#if defined(NET_BOARD_TIME_APP_TICK)
#define TIME_SOURCE NET_HAL_RTD_TIME_APP_TICK
#elif defined(NET_BOARD_NOW_MS_HOOK)
#define TIME_SOURCE NET_HAL_RTD_TIME_NOW_MS_HOOK
#else
#define TIME_SOURCE NET_HAL_RTD_TIME_OSIF
#endif

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

/* ------------------------------------------------------------- time ----- */
uint64_t net_hal_rtd_now_locked(void)
{
#if defined(NET_BOARD_TIME_APP_TICK)
    /* Last Net_MainFunction() nowMs, extended to 64 bits (net_hal_rtd_set_now). */
#elif defined(NET_BOARD_NOW_MS_HOOK)
    rtd.now_ms = NET_BOARD_NOW_MS_HOOK();
#else
    uint32_t ticks = OsIf_GetElapsed(&rtd.time_ref, NET_BOARD_TIME_COUNTER);
    uint64_t total = (uint64_t)rtd.tick_rest + ticks;
    rtd.now_ms += total / rtd.ticks_per_ms;
    rtd.tick_rest = (uint32_t)(total % rtd.ticks_per_ms);
#endif
    return rtd.now_ms;
}

uint32_t net_hal_rtd_isr_stamp_locked(void)
{
#if defined(NET_BOARD_TIME_APP_TICK)
    return NET_BOARD_ISR_TICK_HOOK();   /* raw tick only: no conversion in the ISR */
#else
    return (uint32_t)net_hal_rtd_now_locked();
#endif
}

uint64_t net_hal_rtd_place_stamp(uint32_t stamp)
{
    uint32_t key, ms, back;
    uint64_t now;
    key = net_board_irq_save();
    now = net_hal_rtd_now_locked();
    net_board_irq_restore(key);
#if defined(NET_BOARD_TIME_APP_TICK)
    ms = NET_BOARD_TICK_TO_MS_HOOK(stamp);
#else
    ms = stamp;                         /* the ISR stored milliseconds */
#endif
    /* Place the 32-bit time on the 64-bit axis relative to now. A time ahead
     * of now (the tick arrived after the application read nowMs) or further
     * back than the axis start cannot be a capture time: use now and count. */
    back = (uint32_t)now - ms;
    if (back > 0x7FFFFFFFu || (uint64_t)back > now) {
        inc(&rtd.diag.tick_future_clamped);
        return now;
    }
    return now - back;
}

bool net_hal_rtd_set_now(uint64_t now_ms)
{
#if defined(NET_BOARD_TIME_APP_TICK)
    uint32_t key = net_board_irq_save();
    if (now_ms > rtd.now_ms) rtd.now_ms = now_ms;
    net_board_irq_restore(key);
    return true;
#else
    (void)now_ms;
    return false;
#endif
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
#if UART_LINKS
    case NET_WIRE_UART_M:
    case NET_WIRE_UART_C:
        if (!rtd.started) return NET_NOT_CONFIGURED;
        return net_hal_rtd_uart_submit(t); /* NOT_CONFIGURED for a link this profile lacks */
#endif
    default:
        return NET_NOT_CONFIGURED; /* link not compiled in for this profile */
    }
}

static const NetHalBackendOps rtd_ops = { ops_now, ops_enter, ops_leave, ops_submit };

/* -------------------------------------------------------- foreground ---- */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out)
{
    if (adapter == NULL || in == NULL || out == NULL || in->can_port_epoch == 0u) return NET_INVALID_ARGUMENT;
#if NET_BOARD_UART_M_LINK
    if (in->uart_m_port_epoch == 0u) return NET_INVALID_ARGUMENT;
#endif
#if NET_BOARD_UART_C_LINK
    if (in->uart_c_port_epoch == 0u) return NET_INVALID_ARGUMENT;
#endif
    memset(&rtd, 0, sizeof(rtd));
    net_hal_rtd_can_reset();
    rtd.adapter = adapter;
    rtd.can_epoch = in->can_port_epoch;
    rtd.configured = true;
    rtd.diag.time_source = (uint8_t)TIME_SOURCE;
    rtd.diag.isr_ring_capacity = NET_BOARD_CAN_ISR_RING;
    rtd.diag.isr_drain_budget = NET_BOARD_ISR_DRAIN_BUDGET;
    memset(out, 0, sizeof(*out));
    out->binding_selected = true;
    out->enabled_links = NET_HAL_LINK_BIT(NET_WIRE_CAN);
    out->port_epoch[NET_WIRE_CAN] = in->can_port_epoch;
    out->ops = &rtd_ops;
    out->context = &rtd;
#if UART_LINKS
    return net_hal_rtd_uart_reset(in, out);
#else
    return NET_OK;
#endif
}

NetStatus net_hal_rtd_start(void)
{
    NetStatus st;
    if (!rtd.configured || rtd.started) return NET_INVALID_ARGUMENT;

#if !defined(NET_BOARD_TIME_APP_TICK) && !defined(NET_BOARD_NOW_MS_HOOK)
    rtd.ticks_per_ms = OsIf_MicrosToTicks(1000U, NET_BOARD_TIME_COUNTER);
    rtd.diag.ticks_per_ms = rtd.ticks_per_ms;
    if (rtd.ticks_per_ms == 0u) { rtd.diag.init_failed_step = NET_HAL_RTD_STEP_TIME; return NET_NOT_CONFIGURED; }
    rtd.time_ref = OsIf_GetCounter(NET_BOARD_TIME_COUNTER);
#endif

    st = net_hal_rtd_can_prepare();
    if (st != NET_OK) return st;
#if UART_LINKS
    net_hal_rtd_uart_prepare();
#endif
    rtd.started = true; /* callbacks may fire from here on */
    st = net_hal_rtd_can_arm();
    if (st != NET_OK) { rtd.started = false; return st; }
#if UART_LINKS
    st = net_hal_rtd_uart_arm();
    if (st != NET_OK) { rtd.started = false; return st; }
#endif
    rtd.diag.started = true;
    return NET_OK;
}

uint32_t net_hal_rtd_drain(void)
{
    uint32_t n;
    if (!rtd.started) return 0u;
#if UART_LINKS
    /* Shared budget (the adapter RX queue holds one event per UART byte),
     * CAN first: a UART byte that does not fit here is taken on demand by the
     * Port poll before the M06 timer check (net_hal_rtd_port), so it never
     * waits behind CAN frames; CAN frames, which keep their own capture time,
     * are not starved by a busy UART line. */
    n = net_hal_rtd_can_drain(NET_BOARD_ISR_DRAIN_BUDGET);
    n += net_hal_rtd_uart_drain(NET_BOARD_ISR_DRAIN_BUDGET - n);
#else
    n = net_hal_rtd_can_drain(NET_BOARD_ISR_DRAIN_BUDGET);
#endif
    if (n > rtd.diag.main_drained_max) rtd.diag.main_drained_max = n;
    return n;
}

/* ------------------------------------------------ service Port hand-off -- */
/* M06 receiver contract (net_upper_uart.h, receiver poll): its timer check
 * runs only after the capture queue is drained AND the Port poll reported no
 * work, with the real clock now; a byte captured before that check is refused
 * afterwards (TIME_HOLD, the rest of the frame lost). A received UART byte
 * still in an ISR byte ring is part of the capture queue. So while one taken
 * at or before the service's clock reading waits there, the poll hands it
 * over instead of reporting no work. A byte captured after that reading may
 * wait: it cannot be older than the check. CAN frames are never taken here
 * (one budget per pass, docs C8; a CAN frame keeps its capture time). */
static const NetUpperPortOps *adapter_port_ops;
static uint64_t service_now_ms;
static bool service_now_known;

static NetStatus port_now(void *context, uint64_t *out)
{
    NetStatus st = adapter_port_ops->now_ms(context, out);
    if (st == NET_OK && out != NULL) { service_now_ms = *out; service_now_known = true; }
    return st;
}

static NetStatus port_submit(void *context, const NetUpperPacket *packet, NetTxKey key)
{
    return adapter_port_ops->submit(context, packet, key);
}

static NetStatus port_poll(void *context, NetUpperEvent *out)
{
    NetStatus st = adapter_port_ops->poll(context, out);
#if UART_LINKS
    if (st == NET_NO_WORK && rtd.started && service_now_known) {
        uint32_t n = net_hal_rtd_uart_drain_until(NET_BOARD_ISR_DRAIN_BUDGET, service_now_ms);
        if (n != 0u) {
            rtd.diag.uart_poll_drained = (rtd.diag.uart_poll_drained > UINT32_MAX - n) ?
                UINT32_MAX : rtd.diag.uart_poll_drained + n;
            st = adapter_port_ops->poll(context, out);
        }
    }
#endif
    return st;
}

NetUpperPort net_hal_rtd_port(NetHalAdapter *adapter)
{
    static const NetUpperPortOps ops = { port_now, port_submit, port_poll };
    NetUpperPort p = net_hal_adapter_port(adapter);
    adapter_port_ops = p.ops;
    service_now_known = false;
    p.ops = &ops;               /* same context: the adapter */
    return p;
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
#if UART_LINKS
    net_hal_rtd_uart_service(now);
#endif
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
