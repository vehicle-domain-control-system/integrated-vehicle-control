#include "network/net_runtime.h"
#include "network/net_codec.h"
#include "net_buffer.h"
#include "net_handoff.h"
#include "net_observation.h"
#include <string.h>

static NetStatus validate_frame(const NetFrame *frame)
{
    if (frame == NULL || (frame->length == 0u && frame->link != NET_LINK_CAN) || frame->length > NET_FRAME_CAPACITY ||
        (uint32_t)frame->link > (uint32_t)NET_LINK_UART_C)
        return NET_INVALID_ARGUMENT;
    if (frame->link == NET_LINK_CAN) return net_can_validate_capture(frame);
    return NET_OK; /* Raw UART capture only; never a product decoded message. */
}
NetStatus net_runtime_init(NetRuntime *runtime, const NetRuntimeConfig *config)
{
    if (runtime == NULL || config == NULL || config->local_epoch == 0u ||
        net_port_validate(&config->port) != NET_OK) return NET_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    runtime->config = *config;
    runtime->next_token = 1u;
    runtime->initialized = true;
    return NET_OK;
}
NetStatus net_runtime_capture_rx(NetRuntime *runtime, const NetFrame *frame)
{
    if (runtime == NULL || !runtime->initialized) return NET_INVALID_ARGUMENT;
    if (runtime->in_call) return NET_BUSY;
    return net_buffer_capture(runtime, frame);
}
NetStatus net_runtime_step(NetRuntime *runtime, size_t receive_budget)
{
    size_t i;
    NetStatus result = NET_NO_WORK;
    if (runtime == NULL || !runtime->initialized || receive_budget > NET_RX_CAPACITY)
        return NET_INVALID_ARGUMENT;
    if (runtime->in_call) return NET_BUSY;
    runtime->in_call = true;
    for (i = 0u; i < NET_POLL_BUDGET; ++i) {
        NetPortEvent event;
        NetStatus status = net_port_poll(&runtime->config.port, &event);
        if (status == NET_NO_WORK) break;
        if (status != NET_OK) { result = status; goto done; }
        if (event.kind == NET_PORT_RX) status = net_buffer_capture(runtime, &event.frame);
        else if (event.kind == NET_PORT_TX) status = net_buffer_observe_tx(runtime, &event.tx);
        else if (event.kind == NET_PORT_ERROR) {
            net_observe_increment(&runtime->observation.port_errors);
            status = NET_OK;
        } else status = NET_INVALID_ARGUMENT;
        if (result == NET_NO_WORK || result == NET_OK) result = status;
    }
    for (i = 0u; i < receive_budget; ++i) {
        NetFrame frame;
        NetStatus status = net_buffer_pop(runtime, &frame);
        if (status == NET_NO_WORK) break;
        status = validate_frame(&frame);
        if (status != NET_OK) net_observe_increment(&runtime->observation.rx_invalid);
        else {
            status = net_handoff_deliver(&runtime->config, &frame);
            if (status == NET_OK) net_observe_increment(&runtime->observation.rx_delivered);
            else if (status == NET_HOLD) net_observe_increment(&runtime->observation.rx_held);
            else net_observe_increment(&runtime->observation.consumer_rejected);
        }
        if (result == NET_NO_WORK || result == NET_OK) result = status;
    }
done:
    runtime->in_call = false;
    return result;
}
NetStatus net_runtime_submit(NetRuntime *runtime, const NetFrame *frame,
                             const NetSourceStamp *source,
                             const NetAgePolicy *policy, NetTxKey *out_key)
{
    size_t i;
    uint64_t now_ms, age_ms;
    NetStatus status;
    NetTxRecord *record = NULL;
    if (runtime == NULL || !runtime->initialized || source == NULL || policy == NULL ||
        out_key == NULL || validate_frame(frame) != NET_OK) return NET_INVALID_ARGUMENT;
    if (runtime->in_call) return NET_BUSY;
    if (!runtime->config.host_fixture_enabled || !policy->bound) return NET_HOLD;
    runtime->in_call = true;
    status = net_port_now(&runtime->config.port, &now_ms);
    runtime->in_call = false;
    if (status != NET_OK) return status;
    if (runtime->clock_seen && now_ms < runtime->last_now_ms) return NET_HOLD;
    runtime->last_now_ms = now_ms;
    runtime->clock_seen = true;
    status = net_source_age(source, now_ms, &age_ms);
    if (status != NET_OK) return status;
    if (age_ms > policy->maximum_age_ms ||
        (policy->expires_at_equality && age_ms == policy->maximum_age_ms)) return NET_EXPIRED;
    for (i = 0u; i < NET_TX_CAPACITY; ++i) {
        if (runtime->tx[i].used && runtime->tx[i].source.observation_id == source->observation_id)
            return NET_BUSY;
        if (!runtime->tx[i].used && record == NULL) record = &runtime->tx[i];
    }
    if (record == NULL) return NET_NO_CAPACITY;
    if (runtime->next_token == 0u) return NET_HOLD; /* No wrap or token reuse. */
    memset(record, 0, sizeof(*record));
    record->key.epoch = runtime->config.local_epoch;
    record->key.token = runtime->next_token++;
    record->frame = *frame;
    record->source = *source;
    record->used = true;
    runtime->in_call = true;
    status = net_port_submit(&runtime->config.port, &record->frame, record->key);
    runtime->in_call = false;
    if (status != NET_OK) {
        if (status == NET_REJECTED) net_observe_increment(&runtime->observation.tx_rejected);
        memset(record, 0, sizeof(*record)); /* Port contract: no borrow on non-OK. */
        return status;
    }
    net_observe_increment(&runtime->observation.tx_submitted);
    *out_key = record->key;
    return NET_OK;
}
