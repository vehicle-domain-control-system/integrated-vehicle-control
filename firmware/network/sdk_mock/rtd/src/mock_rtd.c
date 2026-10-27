/* Host mock of RTD FlexCAN_Ip + OsIf behaviour that the Network HAL relies on:
 *  - Receive() arms an MB with a user buffer; an injected frame matching the
 *    MB (individual mask) is copied into that buffer, the MB goes idle and the
 *    configured Callback runs with FLEXCAN_EVENT_RX_COMPLETE (as in the IRQ).
 *  - Send() copies the payload into the MB; the frame stays pending until the
 *    bus side pulls it, then TX_COMPLETE runs. BUSY if the MB is pending.
 *  - AbortTransfer() on a pending TX MB returns the result set by
 *    MockRtd_CanSetAbortResult (default SUCCESS: aborted). With
 *    NO_TRANSFER_IN_PROGRESS the frame went out during the abort (the bus
 *    side gets it first, no TX_COMPLETE); with TIMEOUT the MB stays held by the
 *    controller, its message buffer flag clear, until MockRtd_CanFinishAbort
 *    sets the flag (GetBuffStatusFlag / ClearBuffStatusFlag).
 *  - OsIf system counter: 1 tick/us, 32-bit wrap; disabled timer -> 0 ticks/ms.
 *  - Lpuart_Uart_Ip (interrupt transfers): AsyncReceive/SetRxBuffer arm a
 *    user buffer; an injected byte is stored there and RX_FULL runs when the
 *    buffer is full, END_TRANSFER only if the callback did not re-arm (the
 *    RTD RX IRQ order). AsyncSend keeps the caller's buffer and reads it when
 *    the line side takes the bytes (the buffer must stay valid until
 *    END_TRANSFER). A receive error completes the reception and runs ERROR;
 *    GetReceiveStatus then returns that error. AbortSendingData runs no
 *    callback.
 * Independently written; NOT the NXP file. */
#include <string.h>
#include "mock_rtd.h"

#define MOCK_INSTANCES 2u
#define MOCK_MB 32u
#define CS_EDL 0x80000000UL
#define CS_BRS 0x40000000UL
#define CS_IDE 0x00200000UL
#define CS_DLC_SHIFT 16U
#define CS_CODE_SHIFT 24U
#define CODE_RX_FULL 0x2UL
#define CODE_RX_OVERRUN 0x6UL

typedef struct {
    bool rx_configured, rx_armed, rx_full_unread;
    uint32 mask, id;
    Flexcan_Ip_MsgBuffType *buf;
    bool tx_pending;
    bool abort_pending;  /* timed-out abort not finished: MB held by the controller */
    bool iflag;          /* message buffer flag (set when a timed-out abort finishes) */
    uint32 tx_id;
    Flexcan_Ip_DataInfoType tx_info;
    uint8 tx_data[64];
    /* Frame that went out while an abort ran (NO_TRANSFER_IN_PROGRESS): on
     * the bus already, handed to the far end without TX_COMPLETE. */
    bool sent_in_abort;
    uint32 sent_id;
    Flexcan_Ip_DataInfoType sent_info;
    uint8 sent_data[64];
} MockMb;

typedef struct {
    bool init, started, ack_missing;
    const Flexcan_Ip_ConfigType *cfg;
    Flexcan_Ip_StateType *state;
    Flexcan_Ip_RxMaskType mask_type;
    MockMb mb[MOCK_MB];
    bool rx_refused;
    uint32 aborts, sends_on_aborting;
    Flexcan_Ip_StatusType abort_result;
    uint8 tec, rec;
} MockCan;

typedef struct {
    const Lpuart_Uart_Ip_UserConfigType *cfg;
    Lpuart_Uart_Ip_StateStructureType *st;
    bool tx_stall, rx_refused;
    uint32 inits, sends, aborts, rx_lost;
} MockUart;

#define MOCK_UART_INSTANCES 16u

static MockCan can[MOCK_INSTANCES];
static MockUart uart[MOCK_UART_INSTANCES];
static uint64_t now_us;
static bool timer_enabled = true;
static const uint8 dlc_len[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

void MockRtd_Reset(void)
{
    memset(can, 0, sizeof(can));
    memset(uart, 0, sizeof(uart));
    now_us = 0u;
    timer_enabled = true;
}
void MockRtd_AdvanceUs(uint64_t us) { now_us += us; }
uint64_t MockRtd_NowUs(void) { return now_us; }
void MockRtd_SetSystemTimerEnabled(bool enabled) { timer_enabled = enabled; }

uint8 MockRtd_DlcForLength(uint8 length)
{
    uint8 d;
    for (d = 0u; d < 16u; ++d) if (dlc_len[d] >= length) return d;
    return 15u;
}

/* ---------------------------------------------------------------- OsIf -- */
void OsIf_Init(const void* Config) { (void)Config; }
uint32 OsIf_GetCounter(OsIf_CounterType c)
{
    return (c == OSIF_COUNTER_SYSTEM && timer_enabled) ? (uint32)now_us : 0u;
}
uint32 OsIf_GetElapsed(uint32 * const CurrentRef, OsIf_CounterType c)
{
    uint32 cur = OsIf_GetCounter(c);
    uint32 elapsed = cur - *CurrentRef; /* modulo 2^32 */
    *CurrentRef = cur;
    return elapsed;
}
void OsIf_SetTimerFrequency(uint32 Freq, OsIf_CounterType c) { (void)Freq; (void)c; }
uint32 OsIf_MicrosToTicks(uint32 Micros, OsIf_CounterType c)
{
    return (c == OSIF_COUNTER_SYSTEM && timer_enabled) ? Micros : 0u;
}

/* ------------------------------------------------------------- FlexCAN -- */
static MockCan *get(uint8 i) { return i < MOCK_INSTANCES ? &can[i] : NULL; }
static bool mb_ok(const MockCan *c, uint8 mb) { return c != NULL && c->init && c->cfg != NULL && mb < c->cfg->max_num_mb; }

Flexcan_Ip_StatusType FlexCAN_Ip_Init(uint8 i, Flexcan_Ip_StateType *state, const Flexcan_Ip_ConfigType *cfg)
{
    MockCan *c = get(i);
    if (c == NULL || state == NULL || cfg == NULL || cfg->max_num_mb > MOCK_MB) return FLEXCAN_STATUS_ERROR;
    memset(c, 0, sizeof(*c));
    c->init = true; c->cfg = cfg; c->state = state;
    state->mock_instance_used = 1u;
    return FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_SetStartMode(uint8 i)
{ MockCan *c = get(i); if (c == NULL || !c->init) return FLEXCAN_STATUS_ERROR; c->started = true; return FLEXCAN_STATUS_SUCCESS; }
Flexcan_Ip_StatusType FlexCAN_Ip_SetStopMode(uint8 i)
{ MockCan *c = get(i); if (c == NULL || !c->init) return FLEXCAN_STATUS_ERROR; c->started = false; return FLEXCAN_STATUS_SUCCESS; }
Flexcan_Ip_StatusType FlexCAN_Ip_SetRxMaskType(uint8 i, Flexcan_Ip_RxMaskType t)
{ MockCan *c = get(i); if (c == NULL || !c->init || c->started) return FLEXCAN_STATUS_ERROR; c->mask_type = t; return FLEXCAN_STATUS_SUCCESS; }
Flexcan_Ip_StatusType FlexCAN_Ip_SetRxIndividualMask(uint8 i, uint8 mb, uint32 mask)
{ MockCan *c = get(i); if (!mb_ok(c, mb) || c->started) return FLEXCAN_STATUS_ERROR; c->mb[mb].mask = mask; return FLEXCAN_STATUS_SUCCESS; }
Flexcan_Ip_StatusType FlexCAN_Ip_SetErrorInt(uint8 i, Flexcan_Ip_ErrorIntType t, boolean en)
{ MockCan *c = get(i); (void)t; (void)en; return (c != NULL && c->init) ? FLEXCAN_STATUS_SUCCESS : FLEXCAN_STATUS_ERROR; }
Flexcan_Ip_StatusType FlexCAN_Ip_ConfigRxMb(uint8 i, uint8 mb, const Flexcan_Ip_DataInfoType *info, uint32 id)
{
    MockCan *c = get(i);
    if (!mb_ok(c, mb) || info == NULL) return FLEXCAN_STATUS_ERROR;
    c->mb[mb].rx_configured = true; c->mb[mb].id = id;
    return FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_Receive(uint8 i, uint8 mb, Flexcan_Ip_MsgBuffType *data, boolean isPolling)
{
    MockCan *c = get(i);
    (void)isPolling;
    if (!mb_ok(c, mb) || data == NULL || !c->mb[mb].rx_configured) return FLEXCAN_STATUS_ERROR;
    if (c->mb[mb].rx_armed) return FLEXCAN_STATUS_BUSY;
    if (c->rx_refused) return FLEXCAN_STATUS_ERROR;     /* MockRtd_CanSetReceiveRefused */
    c->mb[mb].buf = data; c->mb[mb].rx_armed = true;
    return FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_Send(uint8 i, uint8 mb, const Flexcan_Ip_DataInfoType *info, uint32 id, const uint8 *data)
{
    MockCan *c = get(i);
    if (!mb_ok(c, mb) || info == NULL || data == NULL || info->data_length > 64u) return FLEXCAN_STATUS_ERROR;
    if (!c->started) return FLEXCAN_STATUS_ERROR;
    if (c->mb[mb].tx_pending) return FLEXCAN_STATUS_BUSY;
    /* After a timed-out abort the RTD state is idle again, so the real driver
     * would write the MB the controller still holds: counted, not refused. */
    if (c->mb[mb].abort_pending) ++c->sends_on_aborting;
    c->mb[mb].tx_pending = true; c->mb[mb].tx_id = id; c->mb[mb].tx_info = *info;
    memcpy(c->mb[mb].tx_data, data, info->data_length); /* copied at Send, like the MB write */
    return FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_GetTransferStatus(uint8 i, uint8 mb)
{
    MockCan *c = get(i);
    if (!mb_ok(c, mb)) return FLEXCAN_STATUS_ERROR;
    return (c->mb[mb].tx_pending || c->mb[mb].rx_armed) ? FLEXCAN_STATUS_BUSY : FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_AbortTransfer(uint8 i, uint8 mb)
{
    MockCan *c = get(i);
    Flexcan_Ip_StatusType result;
    MockMb *m;
    if (!mb_ok(c, mb)) return FLEXCAN_STATUS_ERROR;
    m = &c->mb[mb];
    if (!m->tx_pending && !m->rx_armed) return FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS;
    ++c->aborts;
    m->rx_armed = false;
    result = m->tx_pending ? c->abort_result : FLEXCAN_STATUS_SUCCESS;
    c->abort_result = FLEXCAN_STATUS_SUCCESS;
    if (result == FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS && m->tx_pending) {
        /* Transmitted before the abort took the MB: on the bus, no TX_COMPLETE. */
        m->sent_in_abort = true;
        m->sent_id = m->tx_id; m->sent_info = m->tx_info;
        memcpy(m->sent_data, m->tx_data, sizeof(m->sent_data));
    } else if (result == FLEXCAN_STATUS_TIMEOUT) {
        m->abort_pending = true;        /* held by the controller, flag clear */
        m->iflag = false;
    }
    m->tx_pending = false;
    return result;
}
boolean FlexCAN_Ip_GetBuffStatusFlag(uint8 i, uint8 mb)
{
    MockCan *c = get(i);
    return (mb_ok(c, mb) && c->mb[mb].iflag) ? TRUE : FALSE;
}
void FlexCAN_Ip_ClearBuffStatusFlag(uint8 i, uint8 mb)
{
    MockCan *c = get(i);
    if (mb_ok(c, mb)) c->mb[mb].iflag = false;
}
uint8 FlexCAN_Ip_GetControllerTxErrorCounter(uint8 i) { MockCan *c = get(i); return c ? c->tec : 0u; }
uint8 FlexCAN_Ip_GetControllerRxErrorCounter(uint8 i) { MockCan *c = get(i); return c ? c->rec : 0u; }

/* ------------------------------------------------------------ bus hooks -- */
int MockRtd_CanInject(uint8 i, const MockCanFrame *f)
{
    MockCan *c = get(i);
    uint8 mb, len;
    if (c == NULL || !c->started || f == NULL || f->dlc > 15u) return -1;
    len = dlc_len[f->dlc];
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb) {
        MockMb *m = &c->mb[mb];
        uint32 cs;
        if (!m->rx_configured || !m->rx_armed) continue;
        if (((f->id ^ m->id) & m->mask & 0x7FFu) != 0u) continue;
        cs = ((f->overrun ? CODE_RX_OVERRUN : CODE_RX_FULL) << CS_CODE_SHIFT) | ((uint32)f->dlc << CS_DLC_SHIFT);
        if (f->fd) cs |= CS_EDL;
        if (f->brs) cs |= CS_BRS;
        if (f->extended) cs |= CS_IDE;
        memset(m->buf, 0, sizeof(*m->buf));
        m->buf->cs = cs;
        m->buf->msgId = f->id;
        m->buf->dataLen = len;
        memcpy(m->buf->data, f->data, len);
        m->buf->time_stamp = (uint32)(now_us & 0xFFFFu);
        m->rx_armed = false; /* idle until the next Receive() */
        if (c->cfg->Callback != NULL) c->cfg->Callback(i, FLEXCAN_EVENT_RX_COMPLETE, mb, c->state);
        return (int)mb;
    }
    if (c->rec < 255u) ++c->rec; /* nobody armed: frame lost on this node */
    return -1;
}

int MockRtd_CanTransmitNext(uint8 i, MockCanFrame *out)
{
    MockCan *c = get(i);
    int best = -1;
    uint8 mb;
    if (c == NULL || !c->started) return -1;
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb) {
        MockMb *m = &c->mb[mb];
        if (!m->sent_in_abort) continue;
        /* Already on the bus during an abort: handed over first, no callback. */
        if (out != NULL) {
            memset(out, 0, sizeof(*out));
            out->id = m->sent_id; out->fd = m->sent_info.fd_enable != FALSE; out->brs = m->sent_info.enable_brs != FALSE;
            out->length = m->sent_info.data_length; out->dlc = MockRtd_DlcForLength(out->length);
            memcpy(out->data, m->sent_data, out->length);
        }
        m->sent_in_abort = false;
        return (int)mb;
    }
    if (c->ack_missing) return -1;
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb)
        if (c->mb[mb].tx_pending && (best < 0 || c->mb[mb].tx_id < c->mb[best].tx_id)) best = (int)mb;
    if (best < 0) return -1;
    if (out != NULL) {
        MockMb *m = &c->mb[best];
        memset(out, 0, sizeof(*out));
        out->id = m->tx_id; out->fd = m->tx_info.fd_enable != FALSE; out->brs = m->tx_info.enable_brs != FALSE;
        out->length = m->tx_info.data_length; out->dlc = MockRtd_DlcForLength(out->length);
        memcpy(out->data, m->tx_data, out->length);
    }
    c->mb[best].tx_pending = false;
    if (c->cfg->Callback != NULL) c->cfg->Callback(i, FLEXCAN_EVENT_TX_COMPLETE, (uint32)best, c->state);
    return best;
}

void MockRtd_CanSetAckMissing(uint8 i, bool missing) { MockCan *c = get(i); if (c) c->ack_missing = missing; }
void MockRtd_CanRaiseBusOff(uint8 i)
{
    MockCan *c = get(i);
    if (c && c->cfg && c->cfg->ErrorCallback) { c->tec = 255u; c->cfg->ErrorCallback(i, FLEXCAN_EVENT_BUSOFF, 0x4u, c->state); }
}
uint32 MockRtd_CanAborts(uint8 i) { MockCan *c = get(i); return c ? c->aborts : 0u; }
void MockRtd_CanSetAbortResult(uint8 i, Flexcan_Ip_StatusType result) { MockCan *c = get(i); if (c) c->abort_result = result; }
void MockRtd_CanFinishAbort(uint8 i)
{
    MockCan *c = get(i);
    uint8 mb;
    if (c == NULL || c->cfg == NULL) return;
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb)
        if (c->mb[mb].abort_pending) { c->mb[mb].abort_pending = false; c->mb[mb].iflag = true; }
}
void MockRtd_CanFinishAbortFlagLost(uint8 i)
{
    MockCan *c = get(i);
    uint8 mb;
    if (c == NULL || c->cfg == NULL) return;
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb)
        if (c->mb[mb].abort_pending) { c->mb[mb].abort_pending = false; c->mb[mb].iflag = false; }
}
uint32 MockRtd_CanSendsOnAbortingMb(uint8 i) { MockCan *c = get(i); return c ? c->sends_on_aborting : 0u; }
void MockRtd_CanSetReceiveRefused(uint8 i, bool refused) { MockCan *c = get(i); if (c) c->rx_refused = refused; }
uint32 MockRtd_CanRxArmed(uint8 i)
{
    MockCan *c = get(i);
    uint32 n = 0u;
    uint8 mb;
    if (c == NULL || c->cfg == NULL) return 0u;
    for (mb = 0u; mb < c->cfg->max_num_mb; ++mb) if (c->mb[mb].rx_configured && c->mb[mb].rx_armed) ++n;
    return n;
}

/* -------------------------------------------------------------- LPUART -- */
static MockUart *uget(uint8 i)
{
    return (i < MOCK_UART_INSTANCES && uart[i].cfg != NULL && uart[i].st != NULL) ? &uart[i] : NULL;
}
static void uevent(uint8 i, const MockUart *u, Lpuart_Uart_Ip_EventType ev)
{
    if (u->cfg->Callback != NULL) u->cfg->Callback(i, ev, u->cfg->CallbackParam);
}

void Lpuart_Uart_Ip_Init(const uint8 Instance, const Lpuart_Uart_Ip_UserConfigType *UserConfig)
{
    MockUart *u;
    if (Instance >= MOCK_UART_INSTANCES || UserConfig == NULL || UserConfig->StateStruct == NULL) return;
    u = &uart[Instance];
    u->cfg = UserConfig;
    u->st = UserConfig->StateStruct;
    memset(u->st, 0, sizeof(*u->st));
    u->st->BaudRate = UserConfig->BaudRate;
    u->st->TransmitStatus = LPUART_UART_IP_STATUS_SUCCESS;
    u->st->ReceiveStatus = LPUART_UART_IP_STATUS_SUCCESS;
    ++u->inits;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_Deinit(const uint8 Instance)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return LPUART_UART_IP_STATUS_ERROR;
    if (u->st->IsTxBusy || u->st->IsRxBusy) return LPUART_UART_IP_STATUS_BUSY;
    u->cfg = NULL; u->st = NULL;
    return LPUART_UART_IP_STATUS_SUCCESS;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncSend(const uint8 Instance, const uint8 *TxBuff, const uint32 TxSize)
{
    MockUart *u = uget(Instance);
    if (u == NULL || TxBuff == NULL || TxSize == 0u) return LPUART_UART_IP_STATUS_ERROR;
    if (u->st->IsTxBusy) return LPUART_UART_IP_STATUS_BUSY;
    u->st->TxBuff = TxBuff;
    u->st->TxSize = TxSize;
    u->st->IsTxBusy = TRUE;
    u->st->TransmitStatus = LPUART_UART_IP_STATUS_BUSY;
    ++u->sends;
    return LPUART_UART_IP_STATUS_SUCCESS;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetTransmitStatus(const uint8 Instance, uint32 *BytesRemaining)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return LPUART_UART_IP_STATUS_ERROR;
    if (BytesRemaining != NULL) *BytesRemaining = u->st->TxSize;
    return u->st->TransmitStatus;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AbortSendingData(const uint8 Instance)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return LPUART_UART_IP_STATUS_ERROR;
    if (u->st->IsTxBusy) {
        u->st->IsTxBusy = FALSE;
        u->st->TxSize = 0u;
        u->st->TransmitStatus = LPUART_UART_IP_STATUS_ABORTED;
        ++u->aborts;
    }
    return LPUART_UART_IP_STATUS_SUCCESS;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AsyncReceive(const uint8 Instance, uint8 *RxBuff, const uint32 RxSize)
{
    MockUart *u = uget(Instance);
    if (u == NULL || RxBuff == NULL || RxSize == 0u || u->rx_refused) return LPUART_UART_IP_STATUS_ERROR;
    if (u->st->IsRxBusy) return LPUART_UART_IP_STATUS_BUSY;
    u->st->RxBuff = RxBuff;
    u->st->RxSize = RxSize;
    u->st->IsRxBusy = TRUE;
    u->st->ReceiveStatus = LPUART_UART_IP_STATUS_BUSY;
    return LPUART_UART_IP_STATUS_SUCCESS;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_GetReceiveStatus(const uint8 Instance, uint32 *BytesRemaining)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return LPUART_UART_IP_STATUS_ERROR;
    if (BytesRemaining != NULL) *BytesRemaining = u->st->RxSize;
    return u->st->ReceiveStatus;
}
Lpuart_Uart_Ip_StatusType Lpuart_Uart_Ip_AbortReceivingData(const uint8 Instance)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return LPUART_UART_IP_STATUS_ERROR;
    if (u->st->IsRxBusy) {
        u->st->IsRxBusy = FALSE;
        u->st->ReceiveStatus = LPUART_UART_IP_STATUS_ABORTED;
    }
    return LPUART_UART_IP_STATUS_SUCCESS;
}
void Lpuart_Uart_Ip_SetRxBuffer(const uint8 Instance, uint8 *RxBuff, const uint32 RxSize)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return;
    u->st->RxBuff = RxBuff;
    u->st->RxSize = RxSize;
}
void Lpuart_Uart_Ip_SetTxBuffer(const uint8 Instance, const uint8 *TxBuff, const uint32 TxSize)
{
    MockUart *u = uget(Instance);
    if (u == NULL) return;
    u->st->TxBuff = TxBuff;
    u->st->TxSize = TxSize;
}

/* ------------------------------------------------------- UART line hooks -- */
uint32 MockRtd_UartInject(uint8 i, const uint8 *bytes, uint32 count)
{
    MockUart *u = uget(i);
    uint32 n, taken = 0u;
    if (u == NULL || bytes == NULL) return 0u;
    for (n = 0u; n < count; ++n) {
        Lpuart_Uart_Ip_StateStructureType *st = u->st;
        if (!st->IsRxBusy || st->RxSize == 0u || st->RxBuff == NULL) { ++u->rx_lost; continue; }
        *st->RxBuff = bytes[n];
        ++st->RxBuff;
        --st->RxSize;
        ++taken;
        if (st->RxSize == 0u) {
            uevent(i, u, LPUART_UART_IP_EVENT_RX_FULL);  /* may re-arm with SetRxBuffer */
            if (st->RxSize == 0u) {
                st->IsRxBusy = FALSE;
                st->ReceiveStatus = LPUART_UART_IP_STATUS_SUCCESS;
                uevent(i, u, LPUART_UART_IP_EVENT_END_TRANSFER);
            }
        }
    }
    return taken;
}

uint32 MockRtd_UartTransmitTake(uint8 i, uint8 *out, uint32 max)
{
    MockUart *u = uget(i);
    Lpuart_Uart_Ip_StateStructureType *st;
    uint32 n;
    if (u == NULL || u->tx_stall) return 0u;
    st = u->st;
    if (!st->IsTxBusy || st->TxBuff == NULL) return 0u;
    n = st->TxSize < max ? st->TxSize : max;
    if (out != NULL) memcpy(out, st->TxBuff, n);
    st->TxBuff += n;
    st->TxSize -= n;
    if (st->TxSize == 0u) {
        uevent(i, u, LPUART_UART_IP_EVENT_TX_EMPTY);       /* may chain with SetTxBuffer */
        if (st->TxSize == 0u) {
            st->IsTxBusy = FALSE;
            st->TransmitStatus = LPUART_UART_IP_STATUS_SUCCESS;
            uevent(i, u, LPUART_UART_IP_EVENT_END_TRANSFER);
        }
    }
    return n;
}

void MockRtd_UartRaiseError(uint8 i, Lpuart_Uart_Ip_StatusType status)
{
    MockUart *u = uget(i);
    if (u == NULL) return;
    u->st->ReceiveStatus = status;
    u->st->IsRxBusy = FALSE;      /* the RTD error handler completes the reception */
    uevent(i, u, LPUART_UART_IP_EVENT_ERROR);
}

#if (LPUART_UART_IP_ENABLE_TIMEOUT_INTERRUPT == STD_ON)
void MockRtd_UartRaiseIdle(uint8 i)
{
    MockUart *u = uget(i);
    if (u == NULL) return;
    u->st->ReceiveStatus = LPUART_UART_IP_STATUS_RX_IDLE_STATE;
    u->st->IsRxBusy = FALSE;      /* completed before the idle event, as for ERROR */
    uevent(i, u, LPUART_UART_IP_EVENT_IDLE_STATE);
}
#endif

void MockRtd_UartSetTxStall(uint8 i, bool stall) { if (i < MOCK_UART_INSTANCES) uart[i].tx_stall = stall; }
void MockRtd_UartSetReceiveRefused(uint8 i, bool refused) { if (i < MOCK_UART_INSTANCES) uart[i].rx_refused = refused; }
bool MockRtd_UartRxArmed(uint8 i)
{
    MockUart *u = uget(i);
    return u != NULL && u->st->IsRxBusy && u->st->RxSize != 0u;
}
uint32 MockRtd_UartTxRemaining(uint8 i) { MockUart *u = uget(i); return (u != NULL && u->st->IsTxBusy) ? u->st->TxSize : 0u; }
uint32 MockRtd_UartSends(uint8 i) { return i < MOCK_UART_INSTANCES ? uart[i].sends : 0u; }
uint32 MockRtd_UartAborts(uint8 i) { return i < MOCK_UART_INSTANCES ? uart[i].aborts : 0u; }
uint32 MockRtd_UartRxLost(uint8 i) { return i < MOCK_UART_INSTANCES ? uart[i].rx_lost : 0u; }
uint32 MockRtd_UartInits(uint8 i) { return i < MOCK_UART_INSTANCES ? uart[i].inits : 0u; }
