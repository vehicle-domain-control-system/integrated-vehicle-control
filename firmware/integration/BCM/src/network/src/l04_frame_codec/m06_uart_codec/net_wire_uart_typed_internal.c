#include "net_wire_uart_typed_internal.h"

NetWireStatus nw_uart_typed_encode_args(NetWireProfile profile,
    const void *message, size_t size, NetWireUartFrame *out)
{
    if (message == NULL || out == NULL ||
        net_wire_ranges_overlap(message, size, out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    return net_wire_profile_check(profile);
}

NetWireStatus nw_uart_typed_decode_payload(NetWireProfile profile,
    const NetWireUartFrame *frame, void *out, size_t size, uint16_t *sequence_out,
    NetWireLink link, NetWireDirection direction, uint8_t type,
    NetWireUartPayload *payload)
{
    NetWireStatus status;
    if (frame == NULL || out == NULL || sequence_out == NULL || payload == NULL ||
        net_wire_ranges_overlap(frame, sizeof(*frame), out, size) ||
        net_wire_ranges_overlap(frame, sizeof(*frame), sequence_out, sizeof(*sequence_out)) ||
        net_wire_ranges_overlap(out, size, sequence_out, sizeof(*sequence_out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = net_wire_uart_decode(profile, link, direction, frame, payload);
    if (status != NET_WIRE_OK) return status;
    return payload->type == type ? NET_WIRE_OK : NET_WIRE_MALFORMED;
}
