/* L05/M08 RTD backend, FlexCAN part (S32K1 RTD 3.x / S32K3 RTD 7.x).
 * RX ISR capture, TX message buffers, stuck-TX abort and the error callback.
 * Time base, critical section, start order and backend ops registration are
 * in net_hal_rtd.c; the two parts share net_hal_rtd_internal.h only.
 *
 * RTD symbols used (verified against both bring-up projects' headers):
 *   FlexCAN_Ip_Init, FlexCAN_Ip_SetRxMaskType, FlexCAN_Ip_SetRxIndividualMask,
 *   FlexCAN_Ip_ConfigRxMb, FlexCAN_Ip_Receive, FlexCAN_Ip_SetErrorInt,
 *   FlexCAN_Ip_SetStartMode, FlexCAN_Ip_Send, FlexCAN_Ip_AbortTransfer,
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

typedef struct {
    volatile bool busy, aborting;
    NetTxKey key;
    uint64_t submitted_at_ms;
    union { uint32_t align; uint8_t bytes[64]; } data; /* RTD reads payload words */
} TxSlot;

typedef struct {
    Flexcan_Ip_MsgBuffType rx[RX_COUNT];
    TxSlot tx[TX_COUNT];
} RtdCan;

static RtdCan can;
#define rtd net_hal_rtd_core
static const uint8_t dlc_len[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

void net_hal_rtd_can_reset(void)
{
    memset(&can, 0, sizeof(can));
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

    key = net_board_irq_save();
    for (i = 0u; i < TX_COUNT; ++i) {
        if (!can.tx[i].busy) {
            slot = &can.tx[i];
            mb = (uint8_t)(TX_FIRST + i);
            slot->busy = true;
            slot->aborting = false;
            slot->key = t->key;
            slot->submitted_at_ms = net_hal_rtd_now_locked();
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
        /* Driver accepted the MB. Final outcome + release follow in the ISR. */
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
    NetUpperCapture c = net_hal_rtd_capture_now(rtd.can_epoch);
    uint32_t cs = m->cs, n;

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
        /* An earlier frame in this MB was lost: report the loss, do not hide it. */
        c.overflow = 1u;
        inc(&rtd.diag.rx_overruns);
    }
    inc(&rtd.diag.rx_frames);
    if (net_hal_capture_can(rtd.adapter, &f, &c) != NET_OK) inc(&rtd.diag.rx_queue_rejected);
    /* Copy is complete: re-arm immediately so back-to-back frames are kept. */
    if (FlexCAN_Ip_Receive(NET_BOARD_CAN_INSTANCE, (uint8)mb, m, FALSE) != FLEXCAN_STATUS_SUCCESS)
        inc(&rtd.diag.rx_rearm_errors);
}

static void tx_complete(uint32_t mb)
{
    TxSlot *slot = &can.tx[mb - TX_FIRST];
    NetTxEvent ev;
    NetUpperCapture c;
    if (!slot->busy) { inc(&rtd.diag.unexpected_events); return; }
    c = net_hal_rtd_capture_now(rtd.can_epoch);
    memset(&ev, 0, sizeof(ev));
    ev.key = slot->key;
    ev.admission_observed = true; ev.accepted = true;
    ev.outcome = NET_TX_SUCCEEDED;
    ev.release_observed = true;
    slot->busy = false;
    inc(&rtd.diag.tx_complete);
    (void)net_hal_capture_tx(rtd.adapter, NET_WIRE_CAN, &ev, &c);
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
        /* Reported to M04 (link failed, TX held). Link recovery (quiesce ->
         * new epoch -> resume) is a deliberate integration step, not automatic. */
        NetUpperCapture c = net_hal_rtd_capture_now(rtd.can_epoch);
        inc(&rtd.diag.busoff_events);
        (void)net_hal_capture_link(rtd.adapter, NET_UPPER_PORT_ERROR, NET_WIRE_CAN, &c);
    }
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

void net_hal_rtd_can_service(uint64_t now)
{
    uint32_t i, key;
    for (i = 0u; i < TX_COUNT; ++i) {
        TxSlot *slot = &can.tx[i];
        bool expired;
        key = net_board_irq_save();
        expired = slot->busy && !slot->aborting &&
                  now - slot->submitted_at_ms >= NET_BOARD_CAN_TX_TIMEOUT_MS;
        if (expired) slot->aborting = true;
        net_board_irq_restore(key);
        if (!expired) continue;
        (void)FlexCAN_Ip_AbortTransfer(NET_BOARD_CAN_INSTANCE, (uint8)(TX_FIRST + i));
        key = net_board_irq_save();
        if (slot->busy) {
            /* Not completed before the abort: never claim success. */
            NetTxEvent ev;
            NetUpperCapture c;
            memset(&ev, 0, sizeof(ev));
            ev.key = slot->key; ev.admission_observed = true; ev.accepted = true;
            ev.outcome = NET_TX_FAILED; ev.release_observed = true;
            slot->busy = false;
            memset(&c, 0, sizeof(c));
            c.port_epoch = rtd.can_epoch; c.captured_at_ms = net_hal_rtd_now_locked(); c.timestamp_known = true;
            inc(&rtd.diag.tx_timeout_aborts);
            net_board_irq_restore(key);
            (void)net_hal_capture_tx(rtd.adapter, NET_WIRE_CAN, &ev, &c);
        } else {
            net_board_irq_restore(key); /* completed in the meantime; ISR reported it */
        }
    }
    rtd.diag.tx_error_counter = FlexCAN_Ip_GetControllerTxErrorCounter(NET_BOARD_CAN_INSTANCE);
    rtd.diag.rx_error_counter = FlexCAN_Ip_GetControllerRxErrorCounter(NET_BOARD_CAN_INSTANCE);
}
