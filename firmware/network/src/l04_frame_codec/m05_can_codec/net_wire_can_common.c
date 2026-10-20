#include "net_wire_can_internal.h"

NetWireStatus nw_can_arguments(NetWireProfile profile, const void *input,
    size_t input_size, const void *output, size_t output_size)
{
    if (input == NULL || output == NULL ||
        net_wire_ranges_overlap(input, input_size, output, output_size))
        return NET_WIRE_INVALID_ARGUMENT;
    return net_wire_profile_check(profile);
}

NetWireStatus nw_can_envelope_valid(const NetWireCanEnvelope *frame, uint16_t id)
{
    const NetWireRegistryEntry *entry = net_wire_registry_find(NET_WIRE_CAN, id);
    size_t i;
    if (frame == NULL || entry == NULL) return NET_WIRE_INVALID_ARGUMENT;
    if (entry->frame_length > sizeof(frame->bytes) ||
        entry->payload_length > entry->frame_length) return NET_WIRE_CONTRACT_HOLD;
    if (frame->standard_id != id || frame->extended || !frame->fdf ||
        !frame->brs || frame->rtr || frame->driver_error ||
        frame->dlc != entry->dlc || frame->length != entry->frame_length)
        return NET_WIRE_MALFORMED;
    for (i = entry->payload_length; i < entry->frame_length; ++i)
        if (frame->bytes[i] != 0u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireCanEnvelope nw_can_new_envelope(uint16_t id)
{
    NetWireCanEnvelope frame = {0};
    const NetWireRegistryEntry *entry = net_wire_registry_find(NET_WIRE_CAN, id);
    if (entry != NULL) {
        frame.standard_id = id; frame.fdf = true; frame.brs = true;
        frame.dlc = entry->dlc; frame.length = entry->frame_length;
    }
    return frame;
}
