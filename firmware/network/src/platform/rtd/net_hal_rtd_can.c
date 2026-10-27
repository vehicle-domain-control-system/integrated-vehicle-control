/* L05/M08 RTD backend, FlexCAN part (S32K1 RTD 3.x / S32K3 RTD 7.x).
 * RX/TX/error ISR -> ISR ring, the main-context ring drain into the adapter,
 * TX message buffers and the stuck-TX abort.
 * Time base, critical section, start order and backend ops registration are
 * in net_hal_rtd.c; the two parts share net_hal_rtd_internal.h only.
 *
 * ISR ring (docs/network/network_sw_architecture.md section 8: the ISR hands
 * over only the minimum received data, the actual time and device errors):
 *   RX frame  : copied frame + raw stamp. At most NET_BOARD_CAN_ISR_RING
 *               frames; a frame that finds them all taken is dropped and
 *               counted (isr_ring_overflow). An MB overrun (an earlier frame
 *               of that MB overwritten) is counted too (rx_overruns).
 *               CAN_PORT_RECOVERY.md (RX overflow): an RX overflow is a real
 *               loss observation, recorded with its unit and boundary only,
 *               nothing is synthesized for the missing frame, and the result
 *               is OK. So the next stored frame, itself complete, is delivered
 *               as received (no loss flag on its capture); the boundary is
 *               counted once (rx_loss_events). A single CAN frame has no
 *               stream state to resynchronise.
 *   TX_DONE   : TX MB completed. The slot stays busy until the main context
 *               has reported SUCCEEDED + release, so each MB has at most one
 *               pending fact; their room is reserved and never dropped.
 *   BUSOFF    : one pending bus-off fact at most (a bus-off while the link
 *               is already closed changes nothing); its room is reserved as
 *               well.
 * The main context takes entries in arrival order (net_hal_rtd_can_drain).
 *
 * Bus-off recovery (main context): the generated FlexCAN configs set
 * FLEXCAN_IP_BUSOFF_RECOVERY_U32, so the controller rejoins the bus by
 * itself. When the drain takes the bus-off entry (busoff_close) the backend
 * closes the link in the same call: new frames are refused (no borrow), every
 * frame still in a TX MB gets its actual result (aborted, or completed), and
 * only then the link failure goes to the adapter, so M04 sees the link down
 * in the same pass and keeps no transfer of unknown fate. While the link is
 * closed, received frames are dropped here (busoff_rx_dropped), also those
 * still in the ring when it opens again. After the hold time (net_board_cfg.h
 * NET_BOARD_CAN_BUSOFF_*) the link opens again under port epoch + 1, which the
 * backend then uses for every capture (busoff_reopen). Nothing sent before the
 * bus-off is sent again; delivery resumes from the current state
 * (docs/network/network_sw_architecture.md: bus-off is recovered inside the
 * CAN transport, an old command is not run again; 07_CROSS_NODE_INTERFACE_
 * RULES.md SC17).
 *
 * RTD symbols used (verified against both bring-up projects' headers):
 *   FlexCAN_Ip_Init, FlexCAN_Ip_SetRxMaskType, FlexCAN_Ip_SetRxIndividualMask,
 *   FlexCAN_Ip_ConfigRxMb, FlexCAN_Ip_Receive, FlexCAN_Ip_SetErrorInt,
 *   FlexCAN_Ip_SetStartMode, FlexCAN_Ip_Send, FlexCAN_Ip_AbortTransfer,
 *   FlexCAN_Ip_GetBuffStatusFlag, FlexCAN_Ip_ClearBuffStatusFlag,
 *   FlexCAN_Ip_GetControllerTxErrorCounter, FlexCAN_Ip_GetControllerRxErrorCounter.
 *   GetStartMode/GetStopMode are deliberately unused (absent on S32K1 RTD). */
#include <stdint.h>
#include <string.h>
#include "network/net_hal_rtd.h"
#include "net_hal_rtd_internal.h"
#include "net_board_cfg.h"
#include "FlexCAN_Ip.h"
#include NET_BOARD_CAN_CFG_HEADER

extern Flexcan_Ip_StateType NET_BOARD_CAN_STATE;
extern const Flexcan_Ip_ConfigType NET_BOARD_CAN_CONFIG;

#define RX_FIRST  NET_BOARD_CAN_RX_MB_FIRST
#define RX_COUNT  NET_BOARD_CAN_RX_MB_COUNT
#define TX_FIRST  NET_BOARD_CAN_TX_MB_FIRST
#define TX_COUNT  NET_HAL_TX_CAPACITY

/* Compile-time MB layout checks: no overlap, inside max_num_mb. */
typedef char net_board_rx_tx_disjoint[(RX_FIRST + RX_COUNT <= TX_FIRST ||
                                       TX_FIRST + TX_COUNT <= RX_FIRST) ? 1 : -1];
typedef char net_board_mb_in_range[(RX_FIRST + RX_COUNT <= NET_BOARD_CAN_MAX_MB &&
                                    TX_FIRST + TX_COUNT <= NET_BOARD_CAN_MAX_MB &&
                                    RX_COUNT >= 1u) ? 1 : -1];

/* FlexCAN MB control/status word (hardware layout, also used by RTD's copy). */
#define CS_EDL   0x80000000UL
#define CS_BRS   0x40000000UL
#define CS_ESI   0x20000000UL
#define CS_CODE_SHIFT 24U
#define CS_CODE_MASK  0x0F000000UL
#define CS_IDE   0x00200000UL
#define CS_RTR   0x00100000UL
#define CS_DLC_SHIFT 16U
#define CS_DLC_MASK   0x000F0000UL
#define CS_CODE_RX_OVERRUN 0x6UL

typedef char net_board_isr_ring_nonzero[(NET_BOARD_CAN_ISR_RING >= 1u) ? 1 : -1];

typedef struct {
    /* busy: MB owned by a transfer until its outcome is reported.
     * done: TX_COMPLETE seen in the ISR, SUCCEEDED + release not yet reported.
     * quarantined: the abort timed out; the transfer is reported, but the MB
     * stays busy until its flag shows the controller finished with it.
     * done_stamp: ISR stamp of TX_COMPLETE (a bus-off reports a done MB
     * before its ring entry is drained).
     * submitted_ms: low 32 bits of the submit time; the TX timeout compares
     * the modular difference (far shorter than the 49-day wrap).
     * id: CAN ID of the frame in the MB (one frame per ID at a time). */
    volatile bool busy, aborting, done, quarantined;
    volatile uint32_t done_stamp;
    NetTxKey key;
    uint32_t submitted_ms;
    uint16_t id;
    union { uint32_t align; uint8_t bytes[64]; } data; /* RTD reads payload words */
} TxSlot;

enum { RING_RX = 1, RING_TX_DONE, RING_BUSOFF };
/* Received frames + one fact per TX MB + one bus-off. */
#define RING_SIZE (NET_BOARD_CAN_ISR_RING + TX_COUNT + 1u)

typedef struct {
    uint8_t kind;       /* RING_* */
    uint8_t tx_slot;    /* RING_TX_DONE */
    uint32_t stamp;     /* net_hal_rtd_isr_stamp_locked() at the event */
    NetWireCanEnvelope frame;
} RingEntry;

typedef struct {
    RingEntry e[RING_SIZE];
    uint32_t head, count, rx_count;
    uint32_t lost;      /* frames dropped since the last stored one (loss boundary pending) */
    bool busoff_queued;
} IsrRing;

/* Link state of the bus-off recovery (main context only), also in
 * NetHalRtdDiag.can_link_state. */
enum { LINK_OPEN = 0, LINK_CLOSED = 1 };

typedef struct {
    Flexcan_Ip_MsgBuffType rx[RX_COUNT];
    TxSlot tx[TX_COUNT];
    IsrRing ring;
    uint8_t link;           /* LINK_*: CLOSED refuses new frames, drops received ones */
    uint32_t ring_stale;    /* ring entries stored while the link was closed, still to drain */
    uint64_t hold_until_ms; /* CLOSED: the link opens again at this time */
} RtdCan;

static RtdCan can;
#define rtd net_hal_rtd_core
static const uint8_t dlc_len[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

void net_hal_rtd_can_reset(void)
{
    memset(&can, 0, sizeof(can));
}

/* ------------------------------------------------------------ ISR ring -- */
/* Caller holds the critical section. RX frames may use NET_BOARD_CAN_ISR_RING
 * entries; facts (TX_DONE, BUSOFF) use the reserved remainder. */
static RingEntry *ring_reserve(uint8_t kind)
{
    IsrRing *r = &can.ring;
    RingEntry *e;
    if (r->count >= RING_SIZE || (kind == RING_RX && r->rx_count >= NET_BOARD_CAN_ISR_RING)) return NULL;
    e = &r->e[(r->head + r->count) % RING_SIZE];
    e->kind = kind;
    e->tx_slot = 0u;
    e->stamp = net_hal_rtd_isr_stamp_locked();
    ++r->count;
    if (kind == RING_RX) ++r->rx_count;
    if (r->count > rtd.diag.isr_ring_max_depth) rtd.diag.isr_ring_max_depth = r->count;
    return e;
}

/* -------------------------------------------------------------- TX ------ */
NetStatus net_hal_rtd_can_submit(const NetHalTransfer *t)
{
    const NetWireCanEnvelope *f;
    Flexcan_Ip_DataInfoType info;
    Flexcan_Ip_StatusType st;
    TxSlot *slot = NULL;
    uint8_t mb = 0u;
    uint32_t key, i;
    f = &t->packet.can;
    if (f->extended || f->rtr || f->length > 64u) return NET_REJECTED;
    if (can.link != LINK_OPEN) return NET_HOLD;   /* closed after a bus-off: no new frame, no borrow */

    key = net_board_irq_save();
    /* One frame per CAN ID in the controller: FlexCAN sends equal IDs in MB
     * order, not submission order, so a second frame of an ID waits until
     * the first is reported (its TX_SEQUENCE never goes out behind a later
     * one). A quarantined MB does not count: its frame is already reported
     * FAILED and the MB may stay stuck; if that frame still leaves later, it
     * carries the older number and the receiver drops it as stale. */
    for (i = 0u; i < TX_COUNT; ++i) {
        if (can.tx[i].busy && !can.tx[i].quarantined && can.tx[i].id == f->standard_id) {
            net_board_irq_restore(key);
            inc(&rtd.diag.tx_busy);
            return NET_NO_CAPACITY;
        }
    }
    for (i = 0u; i < TX_COUNT; ++i) {
        if (!can.tx[i].busy) {
            slot = &can.tx[i];
            mb = (uint8_t)(TX_FIRST + i);
            slot->busy = true;
            slot->aborting = false;
            slot->done = false;
            slot->quarantined = false;
            slot->id = (uint16_t)f->standard_id;
            slot->key = t->key;
            slot->submitted_ms = (uint32_t)net_hal_rtd_now_locked();
            break;
        }
    }
    net_board_irq_restore(key);
    if (slot == NULL) { inc(&rtd.diag.tx_busy); return NET_NO_CAPACITY; }

    memcpy(slot->data.bytes, f->bytes, f->length);
    memset(&info, 0, sizeof(info));
    info.msg_id_type = FLEXCAN_MSG_ID_STD;
    info.data_length = (uint8)f->length;
    info.fd_enable = f->fdf ? TRUE : FALSE;
    info.fd_padding = 0U;
    info.enable_brs = f->brs ? TRUE : FALSE;
    info.is_remote = FALSE;
    info.is_polling = FALSE;
    st = FlexCAN_Ip_Send(NET_BOARD_CAN_INSTANCE, mb, &info, f->standard_id, slot->data.bytes);
    if (st == FLEXCAN_STATUS_SUCCESS) {
        /* Driver accepted the MB (main context: reported synchronously). Final
         * outcome + release follow from the ISR through the ring. */
        NetTxEvent ev;
        NetUpperCapture c = net_hal_rtd_capture_now(rtd.can_epoch);
        memset(&ev, 0, sizeof(ev));
        ev.key = t->key; ev.admission_observed = true; ev.accepted = true;
        (void)net_hal_capture_tx(rtd.adapter, NET_WIRE_CAN, &ev, &c);
        inc(&rtd.diag.tx_sent);
        return NET_OK;
    }
    slot->busy = false; /* no borrow, no later event for this call */
    if (st == FLEXCAN_STATUS_BUSY) { inc(&rtd.diag.tx_busy); return NET_NO_CAPACITY; }
    inc(&rtd.diag.tx_send_errors);
    return NET_HOLD;
}

/* ------------------------------------------------------- ISR context ---- */
static void rx_complete(uint32_t mb)
{
    Flexcan_Ip_MsgBuffType *m = &can.rx[mb - RX_FIRST];
    NetWireCanEnvelope f;
    RingEntry *e;
    uint32_t cs = m->cs, n, key, overrun = 0u;

    memset(&f, 0, sizeof(f));
    f.standard_id = m->msgId & 0x7FFu;
    f.extended = (cs & CS_IDE) != 0u;
    f.rtr = (cs & CS_RTR) != 0u;
    f.fdf = (cs & CS_EDL) != 0u;
    f.brs = (cs & CS_BRS) != 0u;
    f.esi = (cs & CS_ESI) != 0u;
    f.dlc = (uint8_t)((cs & CS_DLC_MASK) >> CS_DLC_SHIFT);
    f.length = dlc_len[f.dlc];
    n = m->dataLen <= 64u ? m->dataLen : 64u;
    memcpy(f.bytes, m->data, n);
    if (n != f.length) { f.driver_error = true; inc(&rtd.diag.rx_length_mismatch); }
    if (((cs & CS_CODE_MASK) >> CS_CODE_SHIFT) == CS_CODE_RX_OVERRUN) {
        /* An earlier frame in this MB was overwritten: a real loss right
         * before this frame, which itself is complete. Recorded, not hidden;
         * the frame is still delivered. */
        overrun = 1u;
        inc(&rtd.diag.rx_overruns);
    }
    inc(&rtd.diag.rx_frames);
    key = net_board_irq_save();
    e = ring_reserve(RING_RX);
    if (e != NULL) {
        net_wire_copy(&e->frame, &f, sizeof(f));   /* interrupts masked */
        /* Loss boundary: frames were lost right before this stored one. */
        if (can.ring.lost != 0u || overrun != 0u) inc(&rtd.diag.rx_loss_events);
        can.ring.lost = 0u;
    } else {
        /* Ring full: this frame is lost (counted); the boundary is counted
         * when the next frame is stored. */
        inc(&rtd.diag.isr_ring_overflow);
        inc(&can.ring.lost);
    }
    net_board_irq_restore(key);
    /* Copy is complete: re-arm immediately so back-to-back frames are kept. */
    if (FlexCAN_Ip_Receive(NET_BOARD_CAN_INSTANCE, (uint8)mb, m, FALSE) != FLEXCAN_STATUS_SUCCESS)
        inc(&rtd.diag.rx_rearm_errors);
}

static void tx_complete(uint32_t mb)
{
    uint32_t idx = mb - TX_FIRST;
    TxSlot *slot = &can.tx[idx];
    RingEntry *e;
    uint32_t key = net_board_irq_save();
    /* A quarantined MB's transfer is already reported (its abort timed out). */
    if (!slot->busy || slot->done || slot->quarantined) {
        inc(&rtd.diag.unexpected_events); net_board_irq_restore(key); return;
    }
    e = ring_reserve(RING_TX_DONE);
    if (e != NULL) {
        e->tx_slot = (uint8_t)idx;
        slot->done = true;      /* the MB stays busy until the main context reports it */
        slot->done_stamp = e->stamp;
        inc(&rtd.diag.tx_complete);
    } else {
        /* Not reachable (one reserved entry per MB). The MB stays busy and the
         * TX timeout reports it FAILED: never claim success without a record. */
        inc(&rtd.diag.unexpected_events);
    }
    net_board_irq_restore(key);
}

void NET_BOARD_CAN_CALLBACK(uint8 instance, Flexcan_Ip_EventType eventType,
                            uint32 buffIdx, const Flexcan_Ip_StateType *flexcanState)
{
    (void)flexcanState;
    if (!rtd.started || instance != NET_BOARD_CAN_INSTANCE) { inc(&rtd.diag.unexpected_events); return; }
    /* Unsigned offset compare: one test covers both bounds. */
    if (eventType == FLEXCAN_EVENT_RX_COMPLETE && (uint32_t)(buffIdx - RX_FIRST) < RX_COUNT) {
        rx_complete(buffIdx);
    } else if (eventType == FLEXCAN_EVENT_TX_COMPLETE && (uint32_t)(buffIdx - TX_FIRST) < TX_COUNT) {
        tx_complete(buffIdx);
    } else {
        inc(&rtd.diag.unexpected_events);
    }
}

void NET_BOARD_CAN_ERROR_CALLBACK(uint8 instance, Flexcan_Ip_EventType eventType,
                                  uint32 u32ErrStatus, const Flexcan_Ip_StateType *flexcanState)
{
    (void)flexcanState;
    if (instance != NET_BOARD_CAN_INSTANCE) { inc(&rtd.diag.unexpected_events); return; }
    inc(&rtd.diag.err_events);
    rtd.diag.last_error_status = u32ErrStatus;
    if (eventType == FLEXCAN_EVENT_BUSOFF && rtd.started) {
        /* Handled in the main context (bus-off recovery, top of this file).
         * A bus-off already waiting covers this one. */
        uint32_t key = net_board_irq_save();
        inc(&rtd.diag.busoff_events);
        if (!can.ring.busoff_queued && ring_reserve(RING_BUSOFF) != NULL) can.ring.busoff_queued = true;
        net_board_irq_restore(key);
    }
}

/* ------------------------------------------------------- main context ---- */
static NetUpperCapture capture_at(uint32_t stamp)
{
    NetUpperCapture c;
    memset(&c, 0, sizeof(c));
    c.port_epoch = rtd.can_epoch;
    c.captured_at_ms = net_hal_rtd_place_stamp(stamp);
    c.timestamp_known = true;
    return c;
}

/* A completed MB: SUCCEEDED + release at its TX_COMPLETE stamp. An MB that
 * is no longer done was reported already (busoff_close), its ring entry is
 * skipped. */
static void report_tx_done(uint32_t idx)
{
    TxSlot *slot = &can.tx[idx];
    NetTxEvent ev;
    NetUpperCapture c;
    uint32_t key, stamp;
    memset(&ev, 0, sizeof(ev));
    key = net_board_irq_save();
    if (!slot->done) { net_board_irq_restore(key); return; }
    ev.key = slot->key;
    stamp = slot->done_stamp;
    slot->busy = false;
    slot->done = false;
    net_board_irq_restore(key);
    ev.admission_observed = true; ev.accepted = true;
    ev.outcome = NET_TX_SUCCEEDED;
    ev.release_observed = true;
    c = capture_at(stamp);
    (void)net_hal_capture_tx(rtd.adapter, NET_WIRE_CAN, &ev, &c);
    rtd.diag.busoff_streak = 0u;   /* the bus carried a frame */
}

static void abort_expired(uint32_t i, bool busoff);

/* Bus-off taken from the ring (link open): close the link and settle every
 * TX MB now, in this drain call, then report the link failure. Order of the
 * adapter facts: the MB results first, the port error after them. */
static void busoff_close(uint32_t stamp)
{
    uint32_t i, key;
    bool abort;
    uint64_t now;
    NetUpperCapture c;
    can.link = LINK_CLOSED;          /* submit refuses from here on */
    rtd.diag.can_link_state = LINK_CLOSED;
    for (i = 0u; i < TX_COUNT; ++i) {
        TxSlot *slot = &can.tx[i];
        key = net_board_irq_save();
        abort = slot->busy && !slot->aborting && !slot->done && !slot->quarantined;
        if (abort) slot->aborting = true;
        net_board_irq_restore(key);
        if (abort) abort_expired(i, true);
    }
    /* Completed before or during the aborts: reported now, not by the ring. */
    for (i = 0u; i < TX_COUNT; ++i) report_tx_done(i);
    inc(&rtd.diag.busoff_streak);
    c = capture_at(stamp);
    (void)net_hal_capture_link(rtd.adapter, NET_UPPER_PORT_ERROR, NET_WIRE_CAN, &c);
    key = net_board_irq_save();
    now = net_hal_rtd_now_locked();
    net_board_irq_restore(key);
    can.hold_until_ms = now + (rtd.diag.busoff_streak <= NET_BOARD_CAN_BUSOFF_FAST_TRIES ?
                               NET_BOARD_CAN_BUSOFF_HOLD_MS : NET_BOARD_CAN_BUSOFF_SLOW_HOLD_MS);
}

uint32_t net_hal_rtd_can_drain(uint32_t budget)
{
    IsrRing *r = &can.ring;
    RingEntry e;
    NetUpperCapture c;
    uint32_t n = 0u, key;
    bool stale;
    while (n < budget) {
        key = net_board_irq_save();
        if (r->count == 0u) { net_board_irq_restore(key); break; }
        net_wire_copy(&e, &r->e[r->head], sizeof(e));   /* interrupts masked */
        r->head = (r->head + 1u) % RING_SIZE;
        --r->count;
        if (e.kind == RING_RX) --r->rx_count;
        if (e.kind == RING_BUSOFF) r->busoff_queued = false;
        net_board_irq_restore(key);
        ++n;
        /* Stored while the link was closed (before busoff_reopen). */
        stale = can.ring_stale != 0u;
        if (stale) --can.ring_stale;
        switch (e.kind) {
        case RING_RX:
            if (stale || can.link != LINK_OPEN) { inc(&rtd.diag.busoff_rx_dropped); break; }
            c = capture_at(e.stamp);   /* a complete frame: no loss flag */
            if (net_hal_capture_can(rtd.adapter, &e.frame, &c) != NET_OK) inc(&rtd.diag.rx_queue_rejected);
            break;
        case RING_TX_DONE:
            report_tx_done(e.tx_slot);
            break;
        case RING_BUSOFF:
            /* One while the link is closed changes nothing. */
            if (!stale && can.link == LINK_OPEN) busoff_close(e.stamp);
            break;
        default:
            inc(&rtd.diag.unexpected_events);
            break;
        }
    }
    return n;
}

/* -------------------------------------------------------- foreground ---- */
static bool step(uint8_t id, Flexcan_Ip_StatusType st)
{
    if (st == FLEXCAN_STATUS_SUCCESS) return true;
    rtd.diag.init_failed_step = id;
    rtd.diag.init_rtd_status = (uint32_t)st;
    return false;
}

NetStatus net_hal_rtd_can_prepare(void)
{
    static const Flexcan_Ip_DataInfoType rx_info = {
        .msg_id_type = FLEXCAN_MSG_ID_STD, .data_length = 64U,
        .fd_enable = TRUE, .fd_padding = 0U, .enable_brs = TRUE,
        .is_remote = FALSE, .is_polling = FALSE
    };
    uint32_t i;
    if (!step(NET_HAL_RTD_STEP_INIT, FlexCAN_Ip_Init(NET_BOARD_CAN_INSTANCE, &NET_BOARD_CAN_STATE, &NET_BOARD_CAN_CONFIG)) ||
        !step(NET_HAL_RTD_STEP_MASK_TYPE, FlexCAN_Ip_SetRxMaskType(NET_BOARD_CAN_INSTANCE, FLEXCAN_RX_MASK_INDIVIDUAL)))
        return NET_HOLD;
    for (i = 0u; i < RX_COUNT; ++i) {
        uint8 mb = (uint8)(RX_FIRST + i);
        if (!step(NET_HAL_RTD_STEP_MASK, FlexCAN_Ip_SetRxIndividualMask(NET_BOARD_CAN_INSTANCE, mb, 0U)) ||
            !step(NET_HAL_RTD_STEP_RX_CONFIG, FlexCAN_Ip_ConfigRxMb(NET_BOARD_CAN_INSTANCE, mb, &rx_info, 0U)))
            return NET_HOLD;
    }
    /* Low-rate error sources only: per-frame ERR interrupts would storm when
     * no peer ACKs. Bus errors stay visible through the error counters. */
    if (!step(NET_HAL_RTD_STEP_ERROR_INT, FlexCAN_Ip_SetErrorInt(NET_BOARD_CAN_INSTANCE, FLEXCAN_IP_INT_BUSOFF, TRUE)) ||
        !step(NET_HAL_RTD_STEP_ERROR_INT, FlexCAN_Ip_SetErrorInt(NET_BOARD_CAN_INSTANCE, FLEXCAN_IP_INT_TX_WARNING, TRUE)) ||
        !step(NET_HAL_RTD_STEP_ERROR_INT, FlexCAN_Ip_SetErrorInt(NET_BOARD_CAN_INSTANCE, FLEXCAN_IP_INT_RX_WARNING, TRUE)))
        return NET_HOLD;
    return NET_OK;
}

NetStatus net_hal_rtd_can_arm(void)
{
    uint32_t i;
    for (i = 0u; i < RX_COUNT; ++i) {
        uint8 mb = (uint8)(RX_FIRST + i);
        if (!step(NET_HAL_RTD_STEP_RX_ARM, FlexCAN_Ip_Receive(NET_BOARD_CAN_INSTANCE, mb, &can.rx[i], FALSE)))
            return NET_HOLD;
    }
    if (!step(NET_HAL_RTD_STEP_START, FlexCAN_Ip_SetStartMode(NET_BOARD_CAN_INSTANCE)))
        return NET_HOLD;
    return NET_OK;
}

/* A quarantined MB (abort timed out) is reused only once its message buffer
 * flag shows the controller finished with it: the abort completed or the
 * frame went out. The RTD disabled that MB's interrupt in the abort, so no
 * ISR clears the flag; it is cleared here before the MB is handed out again.
 * The RTD clears the flag itself right after a timed-out abort, so an MB
 * that finished just before that clear never shows it again and stays
 * quarantined until reset (seen as tx_abort_timeouts above
 * tx_quarantine_released). */
static void release_quarantined(uint32_t i)
{
    TxSlot *slot = &can.tx[i];
    uint8 mb = (uint8)(TX_FIRST + i);
    uint32_t key;
    if (!slot->quarantined) return;
    if (FlexCAN_Ip_GetBuffStatusFlag(NET_BOARD_CAN_INSTANCE, mb) == FALSE) return;
    FlexCAN_Ip_ClearBuffStatusFlag(NET_BOARD_CAN_INSTANCE, mb);
    key = net_board_irq_save();
    slot->quarantined = false;
    slot->aborting = false;
    slot->busy = false;
    inc(&rtd.diag.tx_quarantine_released);
    net_board_irq_restore(key);
}

/* A TX MB older than NET_BOARD_CAN_TX_TIMEOUT_MS is aborted. The abort result
 * decides the report (RTD FlexCAN_Ip_AbortTransfer):
 *   NO_TRANSFER_IN_PROGRESS: the frame was transmitted before the abort took
 *     the MB (the RTD then raises no TX_COMPLETE): SUCCEEDED + release,
 *     counted apart (tx_abort_already_sent);
 *   TIMEOUT: the abort did not finish; the outcome is unknown, so FAILED +
 *     release is reported (never a success claim) and the MB is quarantined
 *     until its flag shows it idle (tx_abort_timeouts, release_quarantined);
 *   SUCCESS / other: aborted, FAILED + release.
 * A TX_COMPLETE that raced the abort is reported by the ring instead. */
static void abort_expired(uint32_t i, bool busoff)
{
    TxSlot *slot = &can.tx[i];
    Flexcan_Ip_StatusType st;
    NetTxEvent ev;
    NetUpperCapture c;
    uint32_t key;
    st = FlexCAN_Ip_AbortTransfer(NET_BOARD_CAN_INSTANCE, (uint8)(TX_FIRST + i));
    key = net_board_irq_save();
    rtd.diag.tx_abort_last_status = (uint32_t)st;
    if (!slot->busy || slot->done) {
        slot->aborting = false;
        net_board_irq_restore(key); /* completed in the meantime; the ring reports it */
        return;
    }
    memset(&ev, 0, sizeof(ev));
    ev.key = slot->key; ev.admission_observed = true; ev.accepted = true; ev.release_observed = true;
    inc(busoff ? &rtd.diag.busoff_tx_aborts : &rtd.diag.tx_timeout_aborts);
    if (st == FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS) {
        ev.outcome = NET_TX_SUCCEEDED;
        slot->busy = false;
        slot->aborting = false;
        rtd.diag.busoff_streak = 0u;    /* the frame went out */
        inc(&rtd.diag.tx_abort_already_sent);
    } else if (st == FLEXCAN_STATUS_TIMEOUT) {
        ev.outcome = NET_TX_FAILED;
        slot->quarantined = true;   /* busy stays set: the MB is not handed out */
        inc(&rtd.diag.tx_abort_timeouts);
    } else {
        ev.outcome = NET_TX_FAILED;
        slot->busy = false;
        slot->aborting = false;
    }
    memset(&c, 0, sizeof(c));
    c.port_epoch = rtd.can_epoch; c.captured_at_ms = net_hal_rtd_now_locked(); c.timestamp_known = true;
    net_board_irq_restore(key);
    (void)net_hal_capture_tx(rtd.adapter, NET_WIRE_CAN, &ev, &c);
}

/* The hold after a bus-off is over: LINK_RESET under port epoch + 1, then
 * the backend uses that epoch. Entries still in the ring were stored while
 * the link was closed: the drain drops their frames and ignores a bus-off
 * among them. A refused adapter call is tried again in the next pass. */
static void busoff_reopen(uint64_t now)
{
    uint32_t key;
    NetUpperCapture c;
    if (can.link != LINK_CLOSED || now < can.hold_until_ms) return;
    c = net_hal_rtd_capture_now(rtd.can_epoch + 1u);
    if (net_hal_capture_link(rtd.adapter, NET_UPPER_LINK_RESET, NET_WIRE_CAN, &c) != NET_OK) return;
    key = net_board_irq_save();
    can.ring_stale = can.ring.count;
    net_board_irq_restore(key);
    rtd.can_epoch += 1u;
    can.link = LINK_OPEN;
    inc(&rtd.diag.busoff_recoveries);
    rtd.diag.can_link_state = LINK_OPEN;
}

void net_hal_rtd_can_service(uint64_t now)
{
    uint32_t i, key;
    for (i = 0u; i < TX_COUNT; ++i) {
        TxSlot *slot = &can.tx[i];
        bool expired;
        release_quarantined(i);
        key = net_board_irq_save();
        expired = slot->busy && !slot->aborting && !slot->done &&
                  (uint32_t)now - slot->submitted_ms >= NET_BOARD_CAN_TX_TIMEOUT_MS;
        if (expired) slot->aborting = true;
        net_board_irq_restore(key);
        if (expired) abort_expired(i, false);
    }
    busoff_reopen(now);
    rtd.diag.tx_error_counter = FlexCAN_Ip_GetControllerTxErrorCounter(NET_BOARD_CAN_INSTANCE);
    rtd.diag.rx_error_counter = FlexCAN_Ip_GetControllerRxErrorCounter(NET_BOARD_CAN_INSTANCE);
}
