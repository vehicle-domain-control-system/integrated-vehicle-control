#include "network/net_wire_can.h"
#include "net_wire_internal.h"

enum { REAR_ID = 0x0a0, PERMISSION_ID = 0x080, FRAME_LENGTH = 20, FRAME_DLC = 11 };

static NetWireStatus arguments(NetWireProfile profile, const void *input,
    size_t input_size, const void *output, size_t output_size)
{
    if (input == NULL || output == NULL ||
        net_wire_ranges_overlap(input, input_size, output, output_size)) {
        return NET_WIRE_INVALID_ARGUMENT;
    }
    return net_wire_profile_check(profile);
}

static NetWireStatus envelope_valid(const NetWireCanEnvelope *frame, uint32_t id)
{
    /* Validate all dimensions before accessing any payload byte. */
    if (frame->standard_id != id || frame->extended || !frame->fdf ||
        !frame->brs || frame->rtr || frame->driver_error ||
        frame->dlc != FRAME_DLC || frame->length != FRAME_LENGTH) {
        return NET_WIRE_MALFORMED;
    }
    if (frame->bytes[19] != 0u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

static NetWireCanEnvelope new_envelope(uint32_t id)
{
    NetWireCanEnvelope result = {0};
    result.standard_id = id;
    result.fdf = true;
    result.brs = true;
    result.dlc = FRAME_DLC;
    result.length = FRAME_LENGTH;
    return result;
}

static NetWireStatus rear_valid(const NetWireCisRearRaw *message)
{
    if (message->boot_id == 0u || message->validity > 1u ||
        message->quality_reason > 9u || message->proximity_status > 5u) {
        return NET_WIRE_MALFORMED;
    }
    /* Explicit contradiction in FINAL_98 CIS_REAR_144_TO_344.md. */
    if (message->proximity_status == 0u &&
        message->rear_distance_deci_cm == UINT16_MAX) {
        return NET_WIRE_MALFORMED;
    }
    /* The supplied source does not provide a complete quality/status truth
     * table. Preserve caller data and HOLD these unresolved combinations;
     * never normalize a reason or synthesize a valid distance/NO_OBJECT. */
    if ((message->validity == 1u && message->quality_reason != 0u) ||
        (message->validity == 0u && message->quality_reason == 0u) ||
        (message->proximity_status == 1u &&
         message->rear_distance_deci_cm != UINT16_MAX) ||
        (message->validity == 1u && message->proximity_status >= 2u &&
         message->proximity_status <= 4u)) {
        return NET_WIRE_CONTRACT_HOLD;
    }
    /* TX/UPDATE zero/wrap, age/TTL and sensor operating range are not decided
     * by raw codec. Inactive meaning and all actual consumption stay HOLD. */
    return NET_WIRE_OK;
}

static bool tri_state(uint8_t value)
{
    return value == 0u || value == 1u || value == UINT8_MAX;
}

static NetWireStatus permission_valid(const NetWireDomainPermissionRaw *message)
{
    if (message->boot_id == 0u || !tri_state(message->vehicle_power_permission) ||
        message->power_permission_quality > 3u ||
        !tri_state(message->power_operation_state) ||
        message->operation_allowed > 1u || message->operation_quality > 3u) {
        return NET_WIRE_MALFORMED;
    }
    /* The canonical DOMAIN_PERMISSION codec validates scalar membership.
     * UNKNOWN+OK projection and independent Power/Operation/Alive synthesis
     * require owner contracts: raw representation does not grant permission.
     * Keep raw1+INVALID and saturated AGE intact, including on repeated TX. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_rear(NetWireProfile profile,
    const NetWireCisRearRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = rear_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = new_envelope(REAR_ID);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    nw_write_u32(local.bytes + 8u, input->update_sequence);
    nw_write_u16(local.bytes + 12u, input->source_age_ms);
    nw_write_u16(local.bytes + 14u, input->rear_distance_deci_cm);
    local.bytes[16] = input->validity;
    local.bytes[17] = input->quality_reason;
    local.bytes[18] = input->proximity_status;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_rear(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireCisRearRaw *output)
{
    NetWireStatus status = arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCisRearRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = envelope_valid(input, REAR_ID);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.update_sequence = nw_read_u32(input->bytes + 8u);
    local.source_age_ms = nw_read_u16(input->bytes + 12u);
    local.rear_distance_deci_cm = nw_read_u16(input->bytes + 14u);
    local.validity = input->bytes[16];
    local.quality_reason = input->bytes[17];
    local.proximity_status = input->bytes[18];
    status = rear_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_permission(NetWireProfile profile,
    const NetWireDomainPermissionRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = permission_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = new_envelope(PERMISSION_ID);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8] = input->vehicle_power_permission;
    local.bytes[9] = input->power_permission_quality;
    local.bytes[10] = input->power_operation_state;
    local.bytes[11] = input->operation_allowed;
    local.bytes[12] = input->operation_quality;
    nw_write_u32(local.bytes + 13u, input->update_sequence);
    nw_write_u16(local.bytes + 17u, input->source_age_ms);
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_permission(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireDomainPermissionRaw *output)
{
    NetWireStatus status = arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireDomainPermissionRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = envelope_valid(input, PERMISSION_ID);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.vehicle_power_permission = input->bytes[8];
    local.power_permission_quality = input->bytes[9];
    local.power_operation_state = input->bytes[10];
    local.operation_allowed = input->bytes[11];
    local.operation_quality = input->bytes[12];
    local.update_sequence = nw_read_u32(input->bytes + 13u);
    local.source_age_ms = nw_read_u16(input->bytes + 17u);
    status = permission_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}
