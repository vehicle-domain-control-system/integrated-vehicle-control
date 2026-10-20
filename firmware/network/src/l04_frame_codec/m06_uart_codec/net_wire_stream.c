#include "network/net_wire_uart.h"
#include "net_wire_internal.h"
#include <string.h>

static bool valid_link_direction(NetWireLink link, NetWireDirection direction)
{
    return (link == NET_WIRE_UART_M || link == NET_WIRE_UART_C) &&
           (direction == NET_WIRE_UPLINK || direction == NET_WIRE_DOWNLINK);
}
NetWireStatus nw_uart_stream_config_check(NetWireProfile profile,
    NetWireLink link, NetWireDirection direction, const NetWireStreamPolicy *policy)
{
    NetWireStatus status;
    if(policy==NULL || !valid_link_direction(link,direction)) return NET_WIRE_INVALID_ARGUMENT;
    status=net_wire_profile_check(profile); if(status!=NET_WIRE_OK) return status;
    if(!policy->candidate_time_policy_selected) return NET_WIRE_CONTRACT_HOLD;
    if(policy->assembly_limit_ms==0u) return NET_WIRE_INVALID_ARGUMENT;
    return NET_WIRE_OK;
}
NetWireStatus net_wire_uart_stream_init(NetWireUartStream *stream,
    NetWireProfile profile, NetWireLink link, NetWireDirection direction,
    const NetWireStreamPolicy *policy)
{
    NetWireStatus status;
    if (stream == NULL || policy == NULL || !valid_link_direction(link, direction) ||
        net_wire_ranges_overlap(stream, sizeof(*stream), policy, sizeof(*policy)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = nw_uart_stream_config_check(profile,link,direction,policy);
    if (status != NET_WIRE_OK) return status;
    /* Every failure is above this point; policy cannot alias the destination. */
    memset(stream,0,sizeof(*stream));
    stream->profile = profile; stream->link = link; stream->direction = direction;
    stream->policy = *policy; stream->configured = true;
    return NET_WIRE_OK;
}
static void discard(NetWireUartStream *s, size_t n, bool noise)
{
    size_t i;
    for (i = n; i < s->count; ++i) {
        s->bytes[i - n] = s->bytes[i];
        s->captured_at_ms[i - n] = s->captured_at_ms[i];
    }
    s->count -= n;
    if (noise) for (i = 0u; i < n; ++i) nw_sat_inc(&s->discarded_bytes);
}
static bool overlap(const void *a, size_t an, const void *b, size_t bn)
{
    return net_wire_ranges_overlap(a, an, b, bn);
}
NetWireStatus net_wire_uart_stream_next(NetWireUartStream *stream,
    const NetWireStreamInput *input, size_t *consumed, NetWireUartFrame *out)
{
    size_t used = 0u, i, time_bytes;
    uint64_t previous;
    bool has_previous;
    if (stream == NULL || input == NULL || consumed == NULL || out == NULL)
        return NET_WIRE_INVALID_ARGUMENT;
    if (input->length > NET_WIRE_STREAM_INPUT_MAX ||
        (input->length != 0u && (input->bytes == NULL || input->captured_at_ms == NULL)))
        return NET_WIRE_INVALID_ARGUMENT;
    time_bytes = input->length * sizeof(*input->captured_at_ms);
    if (overlap(stream, sizeof(*stream), input, sizeof(*input)) ||
        overlap(stream, sizeof(*stream), out, sizeof(*out)) ||
        overlap(stream, sizeof(*stream), consumed, sizeof(*consumed)) ||
        overlap(stream, sizeof(*stream), input->bytes, input->length) ||
        overlap(stream, sizeof(*stream), input->captured_at_ms, time_bytes) ||
        overlap(out, sizeof(*out), consumed, sizeof(*consumed)) ||
        overlap(out, sizeof(*out), input, sizeof(*input)) ||
        overlap(out, sizeof(*out), input->bytes, input->length) ||
        overlap(out, sizeof(*out), input->captured_at_ms, time_bytes) ||
        overlap(consumed, sizeof(*consumed), input, sizeof(*input)) ||
        overlap(consumed, sizeof(*consumed), input->bytes, input->length) ||
        overlap(consumed, sizeof(*consumed), input->captured_at_ms, time_bytes))
        return NET_WIRE_INVALID_ARGUMENT;
    if (!stream->configured) return NET_WIRE_NOT_BOUND_STATUS;
    if (net_wire_profile_check(stream->profile) != NET_WIRE_OK ||
        !valid_link_direction(stream->link, stream->direction) ||
        stream->count > NET_WIRE_UART_FRAME_MAX || stream->policy.assembly_limit_ms == 0u)
        return NET_WIRE_INVALID_ARGUMENT;
    if (!stream->policy.candidate_time_policy_selected) return NET_WIRE_CONTRACT_HOLD;
    if (!input->timestamps_unambiguous) return NET_WIRE_TIME_HOLD;
    /* Retained state is part of the transaction too. Its timeline must remain
     * ordered before any noise/CRC discard can mutate the live buffer. */
    for(i=1u;i<stream->count;++i)
        if(stream->captured_at_ms[i]<stream->captured_at_ms[i-1u]) return NET_WIRE_TIME_HOLD;
    if(stream->count!=0u && (!stream->has_last_capture ||
       stream->captured_at_ms[stream->count-1u]>stream->last_capture_ms)) return NET_WIRE_TIME_HOLD;
    /* Immutable registry bounds prove a full buffer is processed before append.
     * If a future registry violates that premise, reject before any mutation. */
    for(i=0u;i<net_wire_registry_count();++i) {
        const NetWireRegistryEntry *entry=net_wire_registry_at(i);
        if(entry->link==stream->link && (entry->frame_length<10u ||
           entry->frame_length>NET_WIRE_UART_FRAME_MAX)) return NET_WIRE_NO_CAPACITY;
    }
    previous = stream->last_capture_ms; has_previous = stream->has_last_capture;
    if (stream->has_last_check && (!has_previous || stream->last_check_ms > previous)) {
        previous = stream->last_check_ms; has_previous = true;
    }
    /* Validate the full borrowed batch first: TIME_HOLD is transactional. */
    for (i = 0u; i < input->length; ++i) {
        if (has_previous && input->captured_at_ms[i] < previous) return NET_WIRE_TIME_HOLD;
        previous = input->captured_at_ms[i]; has_previous = true;
    }
    if (input->timer_check_permitted && has_previous && input->now_ms < previous)
        return NET_WIRE_TIME_HOLD;
    /* Commit boundary: only OK/NEED_MORE exits remain. Monotone buffered/input
     * timestamps make later subtraction safe; registry bounds make append fit. */
    for (;;) {
        size_t wanted = 0u;
        bool complete;
        if ((stream->count >= 1u && stream->bytes[0] != 0xa5u) ||
            (stream->count >= 2u && stream->bytes[1] != 0x5au)) {
            discard(stream, 1u, true); continue;
        }
        if (stream->count >= 6u) {
            const NetWireRegistryEntry *entry;
            if (nw_uart_header(stream->link, stream->direction, stream->bytes, &entry) != NET_WIRE_OK) {
                nw_sat_inc(&stream->bad_headers); discard(stream, 1u, true); continue;
            }
            wanted = entry->frame_length;
        }
        complete = wanted != 0u && stream->count >= wanted;
        if (stream->count != 0u) {
            uint64_t at = stream->captured_at_ms[complete ? wanted - 1u : stream->count - 1u];
            uint64_t elapsed;
            if (!complete && used == input->length && input->timer_check_permitted)
                at = input->now_ms;
            elapsed = at - stream->captured_at_ms[0];
            if (elapsed > stream->policy.assembly_limit_ms ||
                (stream->policy.expires_at_equality && elapsed == stream->policy.assembly_limit_ms)) {
                nw_sat_inc(&stream->assembly_timeouts); discard(stream, 1u, true); continue;
            }
        }
        if (complete) {
            NetWireUartFrame frame = {0};
            NetWireUartPayload payload;
            frame.length = wanted;
            frame.captured_at_ms = stream->captured_at_ms[wanted - 1u];
            memcpy(frame.bytes, stream->bytes, wanted);
            if (net_wire_uart_decode(stream->profile, stream->link, stream->direction,
                                     &frame, &payload) != NET_WIRE_OK) {
                nw_sat_inc(&stream->bad_crc); discard(stream, 1u, true); continue;
            }
            discard(stream, wanted, false);
            if (input->timer_check_permitted && used == input->length) {
                stream->last_check_ms = input->now_ms; stream->has_last_check = true;
            }
            *consumed = used; *out = frame;
            return NET_WIRE_OK;
        }
        if (used == input->length) {
            if (input->timer_check_permitted) {
                stream->last_check_ms = input->now_ms; stream->has_last_check = true;
            }
            *consumed = used;
            return NET_WIRE_NEED_MORE;
        }
        /* count==FRAME_MAX necessarily took complete/header-discard above. */
        stream->bytes[stream->count] = input->bytes[used];
        stream->captured_at_ms[stream->count] = input->captured_at_ms[used];
        stream->last_capture_ms = input->captured_at_ms[used]; stream->has_last_capture = true;
        ++stream->count; ++used;
    }
}
NetWireStatus net_wire_uart_stream_reset(NetWireUartStream *stream, bool byte_loss)
{
    if (stream == NULL) return NET_WIRE_INVALID_ARGUMENT;
    if (!stream->configured) return NET_WIRE_NOT_BOUND_STATUS;
    stream->count = 0u;
    if (byte_loss) nw_sat_inc(&stream->byte_losses);
    return NET_WIRE_OK;
}
