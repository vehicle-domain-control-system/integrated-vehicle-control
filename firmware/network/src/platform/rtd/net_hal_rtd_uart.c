/* L05/M08 RTD backend, LPUART part (S32K1 RTD 3.x / S32K3 RTD 7.x).
 * Only the UART links of the node profile are compiled (net_board_cfg.h:
 * UART-M in HOST/DOMAIN, UART-C in HOST/CIS); for other profiles this file is
 * empty. The backend moves bytes only: frame assembly, CRC, resynchronisation
 * and LINK_SEQUENCE belong to M06/M04 (docs/network/sw_detail/functions/
 * UART_FRAME_STREAM.md: the ISR hands over raw bytes and the actual receive
 * time / error evidence, never parses).
 *
 * RX (one-byte continuous reception, as verified on the DS344 bring-up):
 *   start  : Lpuart_Uart_Ip_Init (void) + AsyncReceive(inst, &rx_byte, 1); the
 *            AsyncReceive result tells whether the link is ready.
 *   RX_FULL: byte + raw stamp into the link's ISR byte ring
 *            (NET_BOARD_UART_ISR_RING), then SetRxBuffer(inst, &rx_byte, 1)
 *            immediately, also when the byte was dropped (ring full: counted).
 *   ERROR  : GetReceiveStatus gives the cause. Overrun counts as a lost byte,
 *            framing / noise / parity as an error. The link is never locked
 *            (M06 resyncs). The RTD error handler completes the reception, so
 *            it is re-armed here; a refused re-arm is retried by
 *            net_hal_rtd_service().
 *   IDLE   : only with the generated timeout interrupt ON (bring-up configs:
 *            OFF): the RTD completes the reception before the event as for
 *            ERROR, so it is re-armed the same way; no loss mark.
 *   loss   : a loss or error is reported in stream order, right before the
 *            next received byte, as its own capture flagged with
 *            NetUpperCapture.overflow (lost bytes) / .errors (line errors).
 *            M04 never feeds a flagged capture's byte into the stream; it only
 *            signals the byte loss (partial frame discarded), so the next real
 *            byte, e.g. the SYNC of the next frame, is not consumed by the flag.
 *   main   : net_hal_rtd_drain() converts stamps and calls net_hal_capture_uart()
 *            with what its budget leaves after the CAN frames; the service's
 *            Port poll (net_hal_rtd_port) takes the bytes still waiting that
 *            were captured at or before its clock reading, before the M06
 *            timer check, so a pass longer than one budget of byte times
 *            loses no byte (net_upper_uart.h, receiver poll contract).
 * TX (one transfer per link on the line):
 *   submit : idle link -> AsyncSend of the whole frame, admission reported at
 *            once. Busy link -> the borrowed transfer waits in submission order
 *            and starts when the link is free (admission then). The frame
 *            deadline (net_upper_packet_eligible) is checked right before
 *            AsyncSend; a missed deadline is reported not admitted + released.
 *   END_TRANSFER with transmit status SUCCESS -> fact + stamp; the main context
 *            reports SUCCEEDED + release and starts the next waiting transfer.
 *   timeout: net_hal_rtd_service() aborts a transfer older than the link
 *            timeout (AbortSendingData, its result recorded) and reports
 *            FAILED + release once the driver let go of the buffer: the abort
 *            returned SUCCESS, or the transmit status reads SUCCESS, or no
 *            byte remains. An abort refused otherwise (the driver may still
 *            read the buffer) keeps the transfer on the link (buffer borrowed,
 *            nothing reported) and is tried again every pass; no later
 *            transfer of that link starts meanwhile. A driver that never lets
 *            go keeps the link's transmission stopped until a reset, seen only
 *            in the diagnostics (as a quarantined CAN TX MB).
 *   start  : the transfer flags and AsyncSend under one critical section, so
 *            an END_TRANSFER (also raised when a reception ends) cannot run
 *            between them while the transmit status still reads the previous
 *            transfer's SUCCESS (tx_end would take it for the new one).
 * Callback wiring (net_board_cfg.h NET_BOARD_UART_CALLBACK): the HAL defines
 * the generated-config callback, or board code routes the Network instance
 * to NetHalRtdUart_OnEvent() (net_hal_rtd_uart.h).
 *
 * RTD symbols used (verified against both bring-up projects' headers):
 *   Lpuart_Uart_Ip_Init, Lpuart_Uart_Ip_AsyncReceive, Lpuart_Uart_Ip_SetRxBuffer,
 *   Lpuart_Uart_Ip_GetReceiveStatus, Lpuart_Uart_Ip_AsyncSend,
 *   Lpuart_Uart_Ip_GetTransmitStatus, Lpuart_Uart_Ip_AbortSendingData. */
#include "net_board_cfg.h"

#if NET_BOARD_UART_M_LINK || NET_BOARD_UART_C_LINK
#include <stdint.h>
#include <string.h>
#include "network/net_hal_rtd.h"
#include "net_hal_rtd_internal.h"
#include "net_hal_rtd_uart.h"
#include NET_BOARD_UART_CFG_HEADER

#if NET_BOARD_UART_M_LINK
extern const Lpuart_Uart_Ip_UserConfigType NET_BOARD_UART_M_CONFIG;
#endif
#if NET_BOARD_UART_C_LINK
extern const Lpuart_Uart_Ip_UserConfigType NET_BOARD_UART_C_CONFIG;
#endif

#define LINKS (NET_BOARD_UART_M_LINK + NET_BOARD_UART_C_LINK)
#define RING  NET_BOARD_UART_ISR_RING
#define TXQ   NET_HAL_TX_CAPACITY   /* the adapter never borrows more transfers */

typedef char net_board_uart_ring_nonzero[(RING >= 1u) ? 1 : -1];

/* A received byte (overflow == errors == 0) or a loss mark (either nonzero,
 * byte unused) that precedes the byte stored after it. */
typedef struct {
    uint32_t stamp;     /* net_hal_rtd_isr_stamp_locked() at RX_FULL */
    uint16_t overflow;  /* loss mark: bytes lost (saturated) */
    uint8_t byte;
    uint8_t errors;     /* loss mark: receive errors (saturated) */
} RxEntry;

typedef struct {
    NetWireLink link;
    uint8 instance;
    const Lpuart_Uart_Ip_UserConfigType *cfg;
    uint32_t tx_timeout_ms;
    uint64_t epoch;
    NetHalRtdUartDiag *diag;
    /* RX: written in the ISR, taken in the main context */
    uint8 rx_byte;
    volatile bool rx_armed;
    RxEntry ring[RING];
    uint32_t head, count, lost, errors;
    /* TX: q[q_head] is the transfer on the line while tx_active (main context
     * only); the flags below are shared with the ISR. */
    const NetHalTransfer *q[TXQ];
    uint32_t q_head, q_count;
    volatile bool tx_active, tx_done, aborting;
    bool abort_wait;    /* timed out, abort refused, driver still sending: retried every pass */
    uint32_t done_stamp;
    uint64_t started_at_ms;
} UartLink;

static UartLink uart[LINKS];
#define rtd net_hal_rtd_core

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

static UartLink *by_instance(uint8 instance)
{
    uint32_t i;
    for (i = 0u; i < LINKS; ++i) if (uart[i].instance == instance && uart[i].cfg != NULL) return &uart[i];
    return NULL;
}

static UartLink *by_link(NetWireLink link)
{
    uint32_t i;
    for (i = 0u; i < LINKS; ++i) if (uart[i].link == link && uart[i].cfg != NULL) return &uart[i];
    return NULL;
}

static void set_armed(UartLink *u, bool armed)
{
    u->rx_armed = armed;
    u->diag->uart_rx_armed = armed;
}

/* ISR or main context. BUSY means a reception is still active. */
static void rx_rearm(UartLink *u)
{
    Lpuart_Uart_Ip_StatusType st = Lpuart_Uart_Ip_AsyncReceive(u->instance, &u->rx_byte, 1U);
    uint32_t key = net_board_irq_save();
    if (st == LPUART_UART_IP_STATUS_SUCCESS || st == LPUART_UART_IP_STATUS_BUSY) {
        set_armed(u, true);
    } else {
        set_armed(u, false);
        inc(&u->diag->uart_rx_rearm_errors);
    }
    net_board_irq_restore(key);
}

/* -------------------------------------------------------- foreground ---- */
NetStatus net_hal_rtd_uart_reset(const NetHalRtdConfig *in, NetHalConfig *out)
{
    uint32_t i = 0u;
    memset(uart, 0, sizeof(uart));
#if NET_BOARD_UART_M_LINK
    uart[i].link = NET_WIRE_UART_M;
    uart[i].instance = (uint8)NET_BOARD_UART_M_INSTANCE;
    uart[i].cfg = &NET_BOARD_UART_M_CONFIG;
    uart[i].tx_timeout_ms = NET_BOARD_UART_M_TX_TIMEOUT_MS;
    uart[i].epoch = in->uart_m_port_epoch;
    uart[i].diag = &rtd.diag.uart[0];
    ++i;
#endif
#if NET_BOARD_UART_C_LINK
    uart[i].link = NET_WIRE_UART_C;
    uart[i].instance = (uint8)NET_BOARD_UART_C_INSTANCE;
    uart[i].cfg = &NET_BOARD_UART_C_CONFIG;
    uart[i].tx_timeout_ms = NET_BOARD_UART_C_TX_TIMEOUT_MS;
    uart[i].epoch = in->uart_c_port_epoch;
    uart[i].diag = &rtd.diag.uart[1];
    ++i;
#endif
    for (i = 0u; i < LINKS; ++i) {
        if (uart[i].epoch == 0u) return NET_INVALID_ARGUMENT;
        uart[i].diag->uart_present = true;
        out->enabled_links |= NET_HAL_LINK_BIT(uart[i].link);
        out->port_epoch[uart[i].link] = uart[i].epoch;
    }
    rtd.diag.uart_isr_ring_capacity = RING;
    return NET_OK;
}

void net_hal_rtd_uart_prepare(void)
{
    uint32_t i;
    for (i = 0u; i < LINKS; ++i) Lpuart_Uart_Ip_Init(uart[i].instance, uart[i].cfg);
}

NetStatus net_hal_rtd_uart_arm(void)
{
    uint32_t i;
    for (i = 0u; i < LINKS; ++i) {
        Lpuart_Uart_Ip_StatusType st = Lpuart_Uart_Ip_AsyncReceive(uart[i].instance, &uart[i].rx_byte, 1U);
        if (st != LPUART_UART_IP_STATUS_SUCCESS) {
            /* Init returns nothing: the first reception is the readiness proof. */
            rtd.diag.init_failed_step = NET_HAL_RTD_STEP_UART_RX_ARM;
            rtd.diag.init_rtd_status = (uint32_t)st;
            return NET_HOLD;
        }
        set_armed(&uart[i], true);
    }
    return NET_OK;
}

/* ------------------------------------------------------- ISR context ---- */
/* Caller holds the critical section and checked the room. */
static void push(UartLink *u, uint32_t stamp, uint8_t byte, uint32_t lost, uint32_t errors)
{
    RxEntry *e = &u->ring[(u->head + u->count) % RING];
    e->stamp = stamp;
    e->byte = byte;
    e->overflow = (uint16_t)(lost > UINT16_MAX ? UINT16_MAX : lost);
    e->errors = (uint8_t)(errors > UINT8_MAX ? UINT8_MAX : errors);
    ++u->count;
    if (u->count > u->diag->uart_isr_ring_max_depth) u->diag->uart_isr_ring_max_depth = u->count;
}

static void rx_full(UartLink *u)
{
    uint32_t key = net_board_irq_save();
    uint32_t room = RING - u->count, stamp;
    bool pending = u->lost != 0u || u->errors != 0u;
    inc(&u->diag->uart_rx_bytes);
    stamp = net_hal_rtd_isr_stamp_locked();
    if (pending && room != 0u) {
        /* Report the earlier loss first, in stream order. */
        push(u, stamp, 0u, u->lost, u->errors);
        u->lost = 0u;
        u->errors = 0u;
        inc(&u->diag->uart_rx_loss_marks);
        --room;
    }
    if (room != 0u) {
        push(u, stamp, u->rx_byte, 0u, 0u);
    } else {
        /* Ring full: this byte is lost and reported before the next stored one. */
        inc(&u->diag->uart_isr_ring_overflow);
        inc(&u->lost);
    }
    net_board_irq_restore(key);
    /* Re-arm at once, also after a drop, so every later byte raises RX_FULL. */
    Lpuart_Uart_Ip_SetRxBuffer(u->instance, &u->rx_byte, 1U);
}

static void rx_error(UartLink *u)
{
    uint32 remaining = 0u;
    Lpuart_Uart_Ip_StatusType st = Lpuart_Uart_Ip_GetReceiveStatus(u->instance, &remaining);
    uint32_t key = net_board_irq_save();
    u->diag->uart_last_error_status = (uint32_t)st;
    if (st == LPUART_UART_IP_STATUS_RX_OVERRUN) {
        inc(&u->diag->uart_rx_overrun);
        inc(&u->lost);          /* at least one byte was overwritten */
    } else {
        inc(&u->diag->uart_rx_errors);
        inc(&u->errors);        /* framing, noise, parity or other */
    }
    set_armed(u, false);        /* the RTD error handler completed the reception */
    net_board_irq_restore(key);
    rx_rearm(u);
}

#if defined(LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT) && (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
/* Receive idle line (generated config with the timeout interrupt): the RTD
 * error handler completes the reception before this event, as for ERROR, so
 * it is re-armed the same way. No byte was lost and no line error happened:
 * no loss mark, the stream goes on. */
static void rx_idle(UartLink *u)
{
    uint32_t key = net_board_irq_save();
    inc(&u->diag->uart_rx_idle_events);
    set_armed(u, false);
    net_board_irq_restore(key);
    rx_rearm(u);
}
#endif

static void tx_end(UartLink *u)
{
    uint32_t key = net_board_irq_save();
    if (u->tx_active && !u->tx_done && !u->aborting) {
        uint32 remaining = 0u;
        /* END_TRANSFER also ends a reception; only a finished transmit counts. */
        if (Lpuart_Uart_Ip_GetTransmitStatus(u->instance, &remaining) == LPUART_UART_IP_STATUS_SUCCESS) {
            u->tx_done = true;  /* reported SUCCEEDED + release by the main context */
            u->done_stamp = net_hal_rtd_isr_stamp_locked();
            inc(&u->diag->uart_tx_complete);
        }
    }
    net_board_irq_restore(key);
}

void NetHalRtdUart_OnEvent(uint8 instance, Lpuart_Uart_Ip_EventType event)
{
    UartLink *u = by_instance(instance);
    uint32_t key;
    if (u == NULL) {
        key = net_board_irq_save();
        inc(&rtd.diag.uart_unexpected_instance);
        net_board_irq_restore(key);
        return;
    }
    if (!rtd.started) {
        key = net_board_irq_save();
        inc(&rtd.diag.unexpected_events);
        net_board_irq_restore(key);
        return;
    }
    if (event == LPUART_UART_IP_EVENT_RX_FULL) rx_full(u);
    else if (event == LPUART_UART_IP_EVENT_END_TRANSFER) tx_end(u);
    else if (event == LPUART_UART_IP_EVENT_ERROR) rx_error(u);
#if defined(LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT) && (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
    else if (event == LPUART_UART_IP_EVENT_IDLE_STATE) rx_idle(u);
#endif
    /* TX_EMPTY: nothing to do. The idle event exists only with the timeout
     * interrupt configured (both bring-up configs: OFF). */
}

#ifdef NET_BOARD_UART_CALLBACK
/* The generated config names this callback; every instance using it lands here. */
void NET_BOARD_UART_CALLBACK(const uint8 HwInstance, const Lpuart_Uart_Ip_EventType Event, const void *UserData)
{
    (void)UserData;
    NetHalRtdUart_OnEvent(HwInstance, Event);
}
#endif

/* ------------------------------------------------------- main context ---- */
static void report(const UartLink *u, NetTxKey key, bool accepted, NetTxOutcome outcome,
                   bool release, const NetUpperCapture *c)
{
    NetTxEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.key = key;
    ev.admission_observed = true;
    ev.accepted = accepted;
    ev.outcome = outcome;
    ev.release_observed = release;
    (void)net_hal_capture_tx(rtd.adapter, u->link, &ev, c);
}

static NetTxKey pop(UartLink *u)
{
    NetTxKey key = u->q[u->q_head]->key; /* last use of the borrowed transfer */
    u->q[u->q_head] = NULL;
    u->q_head = (u->q_head + 1u) % TXQ;
    --u->q_count;
    return key;
}

/* Starts the transfer at q[q_head] if it is still deliverable. */
static Lpuart_Uart_Ip_StatusType start_head(UartLink *u, const NetUpperCapture *c, bool *expired)
{
    const NetHalTransfer *t = u->q[u->q_head];
    Lpuart_Uart_Ip_StatusType st;
    uint32_t key;
    *expired = net_upper_packet_eligible(&t->packet, c->captured_at_ms) != NET_OK;
    if (*expired) return LPUART_UART_IP_STATUS_ERROR;
    /* AsyncSend inside the critical section (file header, start): it only
     * sets the transfer up, and its interrupts run after the restore (the RTD
     * exclusive areas keep the interrupts masked when entered masked). */
    key = net_board_irq_save();
    u->tx_active = true;
    u->tx_done = false;
    u->aborting = false;
    u->abort_wait = false;
    u->started_at_ms = c->captured_at_ms;
    st = Lpuart_Uart_Ip_AsyncSend(u->instance, t->packet.uart.bytes, (uint32)t->packet.uart.length);
    if (st != LPUART_UART_IP_STATUS_SUCCESS) u->tx_active = false;
    net_board_irq_restore(key);
    return st;
}

/* A waiting transfer starts once the link is free; one that cannot start is
 * reported not admitted + released, never as sent. */
static void start_next(UartLink *u)
{
    while (!u->tx_active && u->q_count != 0u) {
        NetUpperCapture c = net_hal_rtd_capture_now(u->epoch);
        bool expired;
        Lpuart_Uart_Ip_StatusType st = start_head(u, &c, &expired);
        if (st == LPUART_UART_IP_STATUS_SUCCESS) {
            inc(&u->diag->uart_tx_sent);
            report(u, u->q[u->q_head]->key, true, NET_TX_UNKNOWN, false, &c);
        } else {
            NetTxKey key = pop(u);
            inc(expired ? &u->diag->uart_tx_expired : &u->diag->uart_tx_send_errors);
            report(u, key, false, NET_TX_UNKNOWN, true, &c);
        }
    }
}

NetStatus net_hal_rtd_uart_submit(const NetHalTransfer *t)
{
    UartLink *u = by_link(t->packet.link);
    NetUpperCapture c;
    Lpuart_Uart_Ip_StatusType st;
    bool expired;
    uint32_t waiting;
    if (u == NULL) return NET_NOT_CONFIGURED;
    if (t->packet.uart.length == 0u || t->packet.uart.length > NET_WIRE_UART_FRAME_MAX) return NET_REJECTED;
    if (u->q_count >= TXQ) return NET_NO_CAPACITY;
    u->q[(u->q_head + u->q_count) % TXQ] = t;
    ++u->q_count;
    if (u->tx_active || u->q_count > 1u) {
        /* Borrow starts now; admission is reported when the link is free. */
        inc(&u->diag->uart_tx_waiting);
        waiting = u->q_count - (u->tx_active ? 1u : 0u);
        if (waiting > u->diag->uart_tx_waiting_max) u->diag->uart_tx_waiting_max = waiting;
        return NET_OK;
    }
    /* Idle link: the result is synchronous; non-OK leaves no borrow behind. */
    c = net_hal_rtd_capture_now(u->epoch);
    st = start_head(u, &c, &expired);
    if (st == LPUART_UART_IP_STATUS_SUCCESS) {
        inc(&u->diag->uart_tx_sent);
        report(u, t->key, true, NET_TX_UNKNOWN, false, &c);
        return NET_OK;
    }
    (void)pop(u);
    if (expired) { inc(&u->diag->uart_tx_expired); return NET_EXPIRED; }
    if (st == LPUART_UART_IP_STATUS_BUSY) return NET_NO_CAPACITY;
    inc(&u->diag->uart_tx_send_errors);
    return NET_HOLD;
}

/* A taken ring entry to the adapter, at its placed capture time. */
static void hand_over(const UartLink *u, const RxEntry *e, uint64_t at)
{
    NetUpperCapture c;
    memset(&c, 0, sizeof(c));
    c.port_epoch = u->epoch;
    c.captured_at_ms = at;
    c.timestamp_known = true;
    c.errors = e->errors;     /* nonzero only on a loss mark */
    c.overflow = e->overflow;
    if (net_hal_capture_uart(rtd.adapter, u->link, e->byte, &c) != NET_OK)
        inc(&u->diag->uart_rx_queue_rejected);
}

uint32_t net_hal_rtd_uart_drain(uint32_t budget)
{
    uint32_t i, n = 0u, key;
    for (i = 0u; i < LINKS; ++i) {
        UartLink *u = &uart[i];
        bool done;
        uint32_t stamp;
        key = net_board_irq_save();
        done = u->tx_done;
        stamp = u->done_stamp;
        if (done) { u->tx_done = false; u->tx_active = false; }
        net_board_irq_restore(key);
        if (done) {
            NetUpperCapture c;
            NetTxKey k = pop(u);
            memset(&c, 0, sizeof(c));
            c.port_epoch = u->epoch;
            c.captured_at_ms = net_hal_rtd_place_stamp(stamp);
            c.timestamp_known = true;
            report(u, k, true, NET_TX_SUCCEEDED, true, &c);
            start_next(u);
        }
    }
    for (i = 0u; i < LINKS; ++i) {
        UartLink *u = &uart[i];
        while (n < budget) {
            RxEntry e;
            key = net_board_irq_save();
            if (u->count == 0u) { net_board_irq_restore(key); break; }
            e = u->ring[u->head];
            u->head = (u->head + 1u) % RING;
            --u->count;
            net_board_irq_restore(key);
            ++n;
            hand_over(u, &e, net_hal_rtd_place_stamp(e.stamp));
        }
    }
    return n;
}

uint32_t net_hal_rtd_uart_drain_until(uint32_t budget, uint64_t limit_ms)
{
    uint32_t i, n = 0u, key;
    for (i = 0u; i < LINKS; ++i) {
        UartLink *u = &uart[i];
        while (n < budget) {
            RxEntry e;
            uint64_t at;
            key = net_board_irq_save();
            if (u->count == 0u) { net_board_irq_restore(key); break; }
            e = u->ring[u->head];   /* the ISR writes behind the tail only */
            net_board_irq_restore(key);
            at = net_hal_rtd_place_stamp(e.stamp);
            if (at > limit_ms) break; /* after that reading: it waits, in ring order */
            key = net_board_irq_save();
            u->head = (u->head + 1u) % RING;
            --u->count;
            net_board_irq_restore(key);
            ++n;
            hand_over(u, &e, at);
        }
    }
    return n;
}

void net_hal_rtd_uart_service(uint64_t now)
{
    uint32_t i, key;
    for (i = 0u; i < LINKS; ++i) {
        UartLink *u = &uart[i];
        bool expired, held = false;
        key = net_board_irq_save();
        expired = u->tx_active && !u->tx_done &&
                  (u->abort_wait || (!u->aborting && now - u->started_at_ms >= u->tx_timeout_ms));
        if (expired) u->aborting = true;
        net_board_irq_restore(key);
        if (expired) {
            /* The transfer is reported FAILED + release (no success claim) once
             * the driver let go of the buffer. A refused abort keeps it unless
             * the transmit status reads SUCCESS or no byte remains (the driver
             * may still read the buffer): tried again in the next pass. */
            Lpuart_Uart_Ip_StatusType ast = Lpuart_Uart_Ip_AbortSendingData(u->instance);
            if (ast != LPUART_UART_IP_STATUS_SUCCESS) {
                uint32 remaining = 0u;
                Lpuart_Uart_Ip_StatusType ts = Lpuart_Uart_Ip_GetTransmitStatus(u->instance, &remaining);
                held = ts != LPUART_UART_IP_STATUS_SUCCESS && remaining != 0u;
            }
            key = net_board_irq_save();
            u->diag->uart_tx_abort_last_status = (uint32_t)ast;
            if (ast != LPUART_UART_IP_STATUS_SUCCESS) inc(&u->diag->uart_tx_abort_errors);
            if (!u->tx_done && held) {
                if (!u->abort_wait) inc(&u->diag->uart_tx_abort_waits);
                u->abort_wait = true;
                u->diag->uart_tx_abort_waiting = 1u;
                net_board_irq_restore(key);
            } else if (!u->tx_done) {
                /* Not ended before the abort: never claim success. */
                NetUpperCapture c;
                NetTxKey k;
                u->tx_active = false;
                u->aborting = false;
                u->abort_wait = false;
                u->diag->uart_tx_abort_waiting = 0u;
                net_board_irq_restore(key);
                k = pop(u);
                inc(&u->diag->uart_tx_timeout_aborts);
                c = net_hal_rtd_capture_now(u->epoch);
                report(u, k, true, NET_TX_FAILED, true, &c);
            } else {
                u->aborting = false;    /* ended in the meantime; the drain reports it */
                u->abort_wait = false;
                u->diag->uart_tx_abort_waiting = 0u;
                net_board_irq_restore(key);
            }
        }
        if (!u->tx_active) start_next(u);
        if (!u->rx_armed) rx_rearm(u);
    }
}

#else
/* No UART link in this node profile: nothing is compiled (ISO C needs one
 * declaration per translation unit). */
typedef int net_hal_rtd_uart_absent;
#endif
