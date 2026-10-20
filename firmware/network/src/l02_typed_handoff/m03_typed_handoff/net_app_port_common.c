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
