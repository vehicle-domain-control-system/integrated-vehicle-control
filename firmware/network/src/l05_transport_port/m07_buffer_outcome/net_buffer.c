#include "net_buffer.h"
#include "net_observation.h"
#include <string.h>

static bool same_key(NetTxKey a, NetTxKey b)
{
    return a.epoch == b.epoch && a.token == b.token;
}
NetStatus net_buffer_capture(NetRuntime *runtime, const NetFrame *frame)
{
    size_t tail;
    if (frame == NULL || (frame->length == 0u && frame->link != NET_LINK_CAN) || frame->length > NET_FRAME_CAPACITY ||
        frame->link < NET_LINK_CAN || frame->link > NET_LINK_UART_C) {
        net_observe_increment(&runtime->observation.rx_invalid);
        return NET_INVALID_ARGUMENT;
    }
    if (runtime->rx_count == NET_RX_CAPACITY) {
        net_observe_increment(&runtime->observation.rx_dropped);
        return NET_NO_CAPACITY;
    }
    tail = (runtime->rx_head + runtime->rx_count) % NET_RX_CAPACITY;
    runtime->rx[tail] = *frame;
    ++runtime->rx_count;
    net_observe_increment(&runtime->observation.rx_captured);
    return NET_OK;
}
NetStatus net_buffer_pop(NetRuntime *runtime, NetFrame *out)
{
    if (runtime->rx_count == 0u) return NET_NO_WORK;
    *out = runtime->rx[runtime->rx_head];
    runtime->rx_head = (runtime->rx_head + 1u) % NET_RX_CAPACITY;
    --runtime->rx_count;
    return NET_OK;
}
NetTxRecord *net_buffer_find(NetRuntime *runtime, NetTxKey key)
{
    size_t i;
    for (i = 0u; i < NET_TX_CAPACITY; ++i)
        if (runtime->tx[i].used && same_key(runtime->tx[i].key, key))
            return &runtime->tx[i];
    return NULL;
}
NetStatus net_buffer_observe_tx(NetRuntime *runtime, const NetTxEvent *event)
{
    NetTxRecord *record = net_buffer_find(runtime, event->key);
    bool changed = false;
    if (record == NULL) {
        net_observe_increment(&runtime->observation.stale_events);
        return NET_STALE;
    }
    if (event->outcome < NET_TX_UNKNOWN || event->outcome > NET_TX_FAILED ||
        (!event->admission_observed && !event->release_observed &&
         event->outcome == NET_TX_UNKNOWN)) return NET_INVALID_ARGUMENT;
    /* Conflicting evidence stays pinned for investigation, never guessed away. */
    if ((event->admission_observed && record->admission_known &&
         event->accepted != record->accepted) ||
        (event->outcome != NET_TX_UNKNOWN && record->outcome != NET_TX_UNKNOWN &&
         event->outcome != record->outcome) ||
        (event->admission_observed && !event->accepted &&
         (event->outcome != NET_TX_UNKNOWN || record->outcome != NET_TX_UNKNOWN)) ||
        (record->admission_known && !record->accepted && event->outcome != NET_TX_UNKNOWN)) {
        record->conflict = true;
        net_observe_increment(&runtime->observation.conflicting_events);
        return NET_HOLD;
    }
    if (record->conflict) return NET_HOLD;
    if (event->admission_observed && !record->admission_known) {
        record->admission_known = true;
        record->accepted = event->accepted;
        net_observe_increment(event->accepted ? &runtime->observation.tx_admitted :
                                               &runtime->observation.tx_rejected);
        changed = true;
    }
    if (event->outcome != NET_TX_UNKNOWN && record->outcome == NET_TX_UNKNOWN) {
        record->outcome = event->outcome;
        net_observe_increment(event->outcome == NET_TX_SUCCEEDED ?
                              &runtime->observation.tx_succeeded :
                              &runtime->observation.tx_failed);
        changed = true;
    }
    if (event->release_observed && !record->reference_released) {
        record->reference_released = true;
        net_observe_increment(&runtime->observation.tx_released);
        changed = true;
    }
    if (!changed) {
        net_observe_increment(&runtime->observation.duplicate_events);
        return NET_DUPLICATE;
    }
    return NET_OK;
}
NetStatus net_runtime_tx_snapshot(const NetRuntime *runtime, NetTxKey key,
                                  NetTxRecord *out)
{
    size_t i;
    if (runtime == NULL || !runtime->initialized || out == NULL)
        return NET_INVALID_ARGUMENT;
    for (i = 0u; i < NET_TX_CAPACITY; ++i) {
        if (runtime->tx[i].used && same_key(runtime->tx[i].key, key)) {
            *out = runtime->tx[i];
            return NET_OK;
        }
    }
    return NET_STALE;
}
NetStatus net_runtime_retire(NetRuntime *runtime, NetTxKey key)
{
    NetTxRecord *record;
    if (runtime == NULL || !runtime->initialized) return NET_INVALID_ARGUMENT;
    if (runtime->in_call) return NET_BUSY;
    record = net_buffer_find(runtime, key);
    if (record == NULL) return NET_STALE;
    if (record->conflict || !record->admission_known || !record->reference_released ||
        (record->accepted && record->outcome == NET_TX_UNKNOWN)) return NET_HOLD;
    memset(record, 0, sizeof(*record));
    return NET_OK;
}
