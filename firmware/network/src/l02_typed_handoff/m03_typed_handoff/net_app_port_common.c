#include "network/net_app_port.h"
#include <string.h>

NetSubmitResult net_app_port_submit_result(NetStatus status)
{
    switch (status) {
    case NET_OK: return NET_SUBMIT_OK;
    case NET_NO_CAPACITY: return NET_SUBMIT_NO_CAPACITY;
    case NET_NOT_CONFIGURED:
    case NET_HOLD:
    case NET_BUSY:
    case NET_NO_WORK: return NET_SUBMIT_NOT_READY;
    case NET_INVALID_ARGUMENT:
    case NET_REJECTED:
    case NET_UNSUPPORTED:
    case NET_EXPIRED:
    case NET_STALE:
    case NET_DUPLICATE:
    default: return NET_SUBMIT_INVALID_ARGUMENT;
    }
}

void net_app_port_rx_meta(uint32_t boot_id, uint32_t tx_sequence,
                          const NetUpperDelivery *delivery, NetCanRxMeta *out)
{
    out->source.boot_id = boot_id;
    out->source.tx_sequence = tx_sequence;
    /* Contract field is uint32 local monotonic ms; wrap is the receiver's
     * interval arithmetic, never a cross-ECU timestamp. */
    out->received_at_ms = (uint32_t)delivery->capture.captured_at_ms;
}

void net_app_port_uart_rx_meta(const NetUpperDelivery *delivery, NetUartRxMeta *out)
{
    /* The service delivers the frame's LINK_SEQUENCE as the transport sequence. */
    out->link_sequence = (uint16_t)delivery->source_sequence;
    out->received_at_ms = (uint32_t)delivery->capture.captured_at_ms;
}

void net_app_port_track(NetAppPortTxTracker *t, NetTxKey key)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) {
        if (!t->used[i]) { t->used[i] = true; t->key[i] = key; return; }
    }
    /* Cannot happen while the engine has the same capacity; counted, never hidden. */
    if (t->untracked_full != UINT32_MAX) ++t->untracked_full;
}

void net_app_port_retire_completed(NetAppPortTxTracker *t, NetUpper *service)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) {
        NetStatus s;
        if (!t->used[i]) continue;
        s = net_upper_retire(service, t->key[i]);
        if (s == NET_OK || s == NET_STALE) {
            /* STALE: the engine no longer knows the key (already retired). */
            t->used[i] = false;
            if (s == NET_OK && t->retired != UINT32_MAX) ++t->retired;
        }
        /* HOLD/BUSY: still in flight or awaiting facts; keep tracking. */
    }
}

bool net_app_port_cancel_if_unsent(NetUpper *service, NetTxKey key)
{
    NetUpperTxSnapshot snap;
    if (net_upper_tx_snapshot(service, key, &snap) != NET_OK) return false;
    if (!snap.used || !snap.queued || snap.submitted) return false;
    return net_upper_cancel(service, key) == NET_OK;
}

/* ------------------------------------------------- discrete records ---- */
void net_app_port_discrete_init(NetAppPortDiscreteQueue *q, uint16_t id)
{
    memset(q, 0, sizeof(*q));
    q->id = id;
}

static bool slot_idle(const NetUpperPublisher *p, uint16_t id)
{
    NetUpperPublishView v;
    if (net_upper_publish_view(p, NET_WIRE_CAN, id, &v) != NET_OK) return false;
    return !v.fresh && !v.in_flight;
}

bool net_app_port_discrete_direct(const NetAppPortDiscreteQueue *q, const NetUpperPublisher *p)
{
    return q->count == 0u && slot_idle(p, q->id);
}

NetStatus net_app_port_discrete_enqueue(NetAppPortDiscreteQueue *q, const NetUpperCanMessage *m, uint64_t now_ms)
{
    NetAppPortDiscreteRecord *r;
    if (q->count >= NET_APP_PORT_DISCRETE_DEPTH) {
        if (q->queue_full != UINT32_MAX) ++q->queue_full;
        return NET_NO_CAPACITY;
    }
    if (q->last_observation == UINT64_MAX) return NET_HOLD;
    r = &q->record[(q->head + q->count) % NET_APP_PORT_DISCRETE_DEPTH];
    r->message = *m;
    /* The record's own stamp: M04 counts the wait from here, once. */
    r->source.observation_id = ++q->last_observation;
    r->source.observed_at_ms = now_ms;
    r->source.original_age_ms = net_upper_can_max_age(m);
    ++q->count;
    if (q->queued != UINT32_MAX) ++q->queued;
    if (q->count > q->depth_max) q->depth_max = (uint32_t)q->count;
    return NET_OK;
}

void net_app_port_discrete_service(NetAppPortDiscreteQueue *q, NetUpperPublisher *p)
{
    while (q->count != 0u && slot_idle(p, q->id)) {
        const NetAppPortDiscreteRecord *r = &q->record[q->head];
        NetStatus st = net_upper_publish_can(p, &r->message, &r->source, NULL);
        if (st == NET_HOLD) return; /* link down or sequence exhausted: keep waiting */
        q->head = (q->head + 1u) % NET_APP_PORT_DISCRETE_DEPTH;
        --q->count;
        if (st == NET_OK) return;   /* the slot holds it now: one record per frame */
        if (q->offer_refused != UINT32_MAX) ++q->offer_refused;
    }
}
