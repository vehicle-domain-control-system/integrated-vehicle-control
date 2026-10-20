/* Host mock of RTD FlexCAN_Ip + OsIf behaviour that the Network HAL relies on:
 *  - Receive() arms an MB with a user buffer; an injected frame matching the
 *    MB (individual mask) is copied into that buffer, the MB goes idle and the
 *    configured Callback runs with FLEXCAN_EVENT_RX_COMPLETE (as in the IRQ).
 *  - Send() copies the payload into the MB; the frame stays pending until the
 *    bus side pulls it, then TX_COMPLETE runs. BUSY if the MB is pending.
 *  - OsIf system counter: 1 tick/us, 32-bit wrap; disabled timer -> 0 ticks/ms.
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
    uint32 tx_id;
    Flexcan_Ip_DataInfoType tx_info;
    uint8 tx_data[64];
} MockMb;

typedef struct {
    bool init, started, ack_missing;
    const Flexcan_Ip_ConfigType *cfg;
    Flexcan_Ip_StateType *state;
    Flexcan_Ip_RxMaskType mask_type;
    MockMb mb[MOCK_MB];
    uint32 aborts;
    uint8 tec, rec;
} MockCan;

static MockCan can[MOCK_INSTANCES];
static uint64_t now_us;
static bool timer_enabled = true;
static const uint8 dlc_len[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};

void MockRtd_Reset(void)
{
    memset(can, 0, sizeof(can));
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
    c->mb[mb].buf = data; c->mb[mb].rx_armed = true;
    return FLEXCAN_STATUS_SUCCESS;
}
Flexcan_Ip_StatusType FlexCAN_Ip_Send(uint8 i, uint8 mb, const Flexcan_Ip_DataInfoType *info, uint32 id, const uint8 *data)
{
    MockCan *c = get(i);
    if (!mb_ok(c, mb) || info == NULL || data == NULL || info->data_length > 64u) return FLEXCAN_STATUS_ERROR;
    if (!c->started) return FLEXCAN_STATUS_ERROR;
    if (c->mb[mb].tx_pending) return FLEXCAN_STATUS_BUSY;
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
    if (!mb_ok(c, mb)) return FLEXCAN_STATUS_ERROR;
    if (!c->mb[mb].tx_pending && !c->mb[mb].rx_armed) return FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS;
    c->mb[mb].tx_pending = false; c->mb[mb].rx_armed = false; ++c->aborts;
    return FLEXCAN_STATUS_SUCCESS;
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
    if (c == NULL || !c->started || c->ack_missing) return -1;
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
