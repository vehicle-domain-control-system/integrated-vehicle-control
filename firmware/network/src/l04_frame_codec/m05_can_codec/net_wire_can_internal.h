#ifndef NETWORK_PRIVATE_WIRE_CAN_INTERNAL_H
#define NETWORK_PRIVATE_WIRE_CAN_INTERNAL_H
#include "network/net_wire_can.h"
#include "net_wire_internal.h"
NetWireStatus nw_can_arguments(NetWireProfile profile, const void *input,
    size_t input_size, const void *output, size_t output_size);
/* Exact ID/length/DLC/padding from the single static Registry48. */
NetWireStatus nw_can_envelope_valid(const NetWireCanEnvelope *frame, uint16_t id);
/* Fixed known message IDs only; an unknown ID yields an invalid zero envelope. */
NetWireCanEnvelope nw_can_new_envelope(uint16_t id);
#endif
