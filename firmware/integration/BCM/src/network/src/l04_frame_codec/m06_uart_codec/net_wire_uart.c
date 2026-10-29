#include "network/net_wire_uart.h"
#include "net_wire_internal.h"
#include <string.h>

NetWireStatus net_wire_crc16(const uint8_t *bytes, size_t length, uint16_t *out)
{
    size_t i;
    unsigned bit;
    uint16_t crc = UINT16_C(0xffff);
    if (out == NULL || (length != 0u && bytes == NULL) ||
        length > NET_WIRE_UART_FRAME_MAX ||
        net_wire_ranges_overlap(bytes, length, out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    for (i = 0u; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)bytes[i] << 8u);
        for (bit = 0u; bit < 8u; ++bit)
            crc = (uint16_t)((crc & UINT16_C(0x8000)) != 0u ?
                            ((uint32_t)crc << 1u) ^ UINT32_C(0x1021) :
                            (uint32_t)crc << 1u);
    }
    *out = crc;
    return NET_WIRE_OK;
}
NetWireStatus nw_uart_header(NetWireLink link, NetWireDirection direction,
    const uint8_t *bytes, const NetWireRegistryEntry **entry)
{
    const NetWireRegistryEntry *found;
    if ((link != NET_WIRE_UART_M && link != NET_WIRE_UART_C) ||
        (direction != NET_WIRE_UPLINK && direction != NET_WIRE_DOWNLINK))
        return NET_WIRE_INVALID_ARGUMENT;
    if (bytes[0] != 0xa5u || bytes[1] != 0x5au ||
        bytes[2] != (link == NET_WIRE_UART_M ? 2u : 1u)) return NET_WIRE_MALFORMED;
    found = net_wire_registry_find(link, bytes[3]);
    if (found == NULL) return NET_WIRE_UNSUPPORTED;
    if (found->direction != direction || nw_read_u16(bytes + 4u) != found->payload_length ||
        found->payload_length > NET_WIRE_UART_PAYLOAD_MAX) return NET_WIRE_MALFORMED;
    *entry = found;
    return NET_WIRE_OK;
}
NetWireStatus net_wire_uart_encode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, NetWireUartFrame *out)
{
    NetWireUartFrame frame = {0};
    const NetWireRegistryEntry *entry;
    uint16_t crc;
    NetWireStatus status;
    if (out == NULL || payload == NULL || length > NET_WIRE_UART_PAYLOAD_MAX ||
        net_wire_ranges_overlap(payload, length, out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = net_wire_profile_check(profile);
    if (status != NET_WIRE_OK) return status;
    if ((link != NET_WIRE_UART_M && link != NET_WIRE_UART_C) ||
        (direction != NET_WIRE_UPLINK && direction != NET_WIRE_DOWNLINK))
        return NET_WIRE_INVALID_ARGUMENT;
    entry = net_wire_registry_find(link, type);
    if (entry == NULL) return NET_WIRE_UNSUPPORTED;
    if (entry->direction != direction || length != entry->payload_length)
        return NET_WIRE_MALFORMED;
    frame.length = length + 10u;
    frame.bytes[0] = 0xa5u; frame.bytes[1] = 0x5au;
    frame.bytes[2] = entry->version; frame.bytes[3] = type;
    nw_write_u16(frame.bytes + 4u, (uint16_t)length);
    nw_write_u16(frame.bytes + 6u, sequence);
    memcpy(frame.bytes + 8u, payload, length);
    status = net_wire_crc16(frame.bytes + 2u, length + 6u, &crc);
    if (status != NET_WIRE_OK) return status;
    nw_write_u16(frame.bytes + frame.length - 2u, crc);
    *out = frame;
    return NET_WIRE_OK;
}
NetWireStatus net_wire_uart_decode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, const NetWireUartFrame *frame, NetWireUartPayload *out)
{
    NetWireUartPayload decoded = {0};
    const NetWireRegistryEntry *entry;
    NetWireStatus status;
    uint16_t crc;
    if (frame == NULL || out == NULL ||
        net_wire_ranges_overlap(frame, sizeof(*frame), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = net_wire_profile_check(profile);
    if (status != NET_WIRE_OK) return status;
    if (frame->length < 10u || frame->length > NET_WIRE_UART_FRAME_MAX)
        return NET_WIRE_MALFORMED;
    status = nw_uart_header(link, direction, frame->bytes, &entry);
    if (status != NET_WIRE_OK) return status;
    if (frame->length != entry->frame_length) return NET_WIRE_MALFORMED;
    status = net_wire_crc16(frame->bytes + 2u, frame->length - 4u, &crc);
    if (status != NET_WIRE_OK) return status;
    if (crc != nw_read_u16(frame->bytes + frame->length - 2u)) return NET_WIRE_MALFORMED;
    decoded.type = frame->bytes[3]; decoded.sequence = nw_read_u16(frame->bytes + 6u);
    decoded.length = entry->payload_length;
    decoded.captured_at_ms = frame->captured_at_ms;
    memcpy(decoded.bytes, frame->bytes + 8u, decoded.length);
    *out = decoded;
    return NET_WIRE_OK;
}
