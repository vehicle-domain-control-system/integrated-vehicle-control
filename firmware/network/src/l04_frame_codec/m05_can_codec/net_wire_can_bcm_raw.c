#include "network/net_wire_can_bcm.h"
#include "net_wire_can_internal.h"

/* FINAL_98 CAN offsets + BCM Data and per-message codec validation.
 * No packed overlays or application CRC. Only explicit bytes and shared LE. */
static bool tri(uint8_t v)
{
    return v <= 1u || v == UINT8_MAX;
}

static NetWireStatus bcm_door_command_valid(const NetWireBcmDoorCommandRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    /* Required secondary boot has no documented zero identity binding.
     * Conservative candidate HOLD; not a new malformed wire reservation. */
    if (m->target_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    if (m->door_lock_target > 1u) return NET_WIRE_MALFORMED;
    if (m->origin > 5u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_door_command(NetWireProfile profile,
    const NetWireBcmDoorCommandRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_door_command_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x100u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    nw_write_u32(local.bytes + 8u, input->target_boot_id);
    nw_write_u32(local.bytes + 12u, input->command_id);
    nw_write_u16(local.bytes + 16u, input->command_age_ms);
    nw_write_u16(local.bytes + 18u, input->accept_limit_ms);
    local.bytes[20u] = input->door_lock_target;
    local.bytes[21u] = input->origin;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_door_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmDoorCommandRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmDoorCommandRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x100u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.target_boot_id = nw_read_u32(input->bytes + 8u);
    local.command_id = nw_read_u32(input->bytes + 12u);
    local.command_age_ms = nw_read_u16(input->bytes + 16u);
    local.accept_limit_ms = nw_read_u16(input->bytes + 18u);
    local.door_lock_target = input->bytes[20u];
    local.origin = input->bytes[21u];
    status = bcm_door_command_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_climate_command_valid(const NetWireBcmClimateCommandRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    /* Required secondary boot has no documented zero identity binding.
     * Conservative candidate HOLD; not a new malformed wire reservation. */
    if (m->target_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    if (m->fan_target_level > 3u) return NET_WIRE_MALFORMED;
    if (m->thermal_direction > 2u) return NET_WIRE_MALFORMED;
    if (m->thermal_target_percent > 100u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_climate_command(NetWireProfile profile,
    const NetWireBcmClimateCommandRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_climate_command_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x110u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    nw_write_u32(local.bytes + 8u, input->target_boot_id);
    nw_write_u32(local.bytes + 12u, input->command_id);
    nw_write_u16(local.bytes + 16u, input->command_age_ms);
    nw_write_u16(local.bytes + 18u, input->accept_limit_ms);
    local.bytes[20u] = input->fan_target_level;
    local.bytes[21u] = input->thermal_direction;
    local.bytes[22u] = input->thermal_target_percent;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_climate_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmClimateCommandRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmClimateCommandRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x110u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.target_boot_id = nw_read_u32(input->bytes + 8u);
    local.command_id = nw_read_u32(input->bytes + 12u);
    local.command_age_ms = nw_read_u16(input->bytes + 16u);
    local.accept_limit_ms = nw_read_u16(input->bytes + 18u);
    local.fan_target_level = input->bytes[20u];
    local.thermal_direction = input->bytes[21u];
    local.thermal_target_percent = input->bytes[22u];
    status = bcm_climate_command_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_light_command_valid(const NetWireBcmLightCommandRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    /* Required secondary boot has no documented zero identity binding.
     * Conservative candidate HOLD; not a new malformed wire reservation. */
    if (m->target_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    if (m->interior_light_type > 3u) return NET_WIRE_MALFORMED;
    if (m->light_level_percent > 100u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_light_command(NetWireProfile profile,
    const NetWireBcmLightCommandRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_light_command_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x120u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    nw_write_u32(local.bytes + 8u, input->target_boot_id);
    nw_write_u32(local.bytes + 12u, input->command_id);
    nw_write_u16(local.bytes + 16u, input->command_age_ms);
    nw_write_u16(local.bytes + 18u, input->accept_limit_ms);
    local.bytes[20u] = input->interior_light_type;
    local.bytes[21u] = input->light_level_percent;
    local.bytes[22u] = input->red;
    local.bytes[23u] = input->green;
    local.bytes[24u] = input->blue;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_light_command(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmLightCommandRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmLightCommandRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x120u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.target_boot_id = nw_read_u32(input->bytes + 8u);
    local.command_id = nw_read_u32(input->bytes + 12u);
    local.command_age_ms = nw_read_u16(input->bytes + 16u);
    local.accept_limit_ms = nw_read_u16(input->bytes + 18u);
    local.interior_light_type = input->bytes[20u];
    local.light_level_percent = input->bytes[21u];
    local.red = input->bytes[22u];
    local.green = input->bytes[23u];
    local.blue = input->bytes[24u];
    status = bcm_light_command_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_door_state_valid(const NetWireBcmDoorStateRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    if (!tri(m->door_lock_state)) return NET_WIRE_MALFORMED;
    if (!tri(m->door_open_state)) return NET_WIRE_MALFORMED;
    if (m->door_composite_state > 2u) return NET_WIRE_MALFORMED;
    if (m->lock_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->open_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->composite_quality > 3u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_door_state(NetWireProfile profile,
    const NetWireBcmDoorStateRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_door_state_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x200u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8u] = input->door_lock_state;
    local.bytes[9u] = input->door_open_state;
    local.bytes[10u] = input->door_composite_state;
    local.bytes[11u] = input->lock_quality;
    local.bytes[12u] = input->open_quality;
    local.bytes[13u] = input->composite_quality;
    nw_write_u32(local.bytes + 14u, input->update_sequence);
    nw_write_u16(local.bytes + 18u, input->source_age_ms);
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_door_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmDoorStateRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmDoorStateRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x200u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.door_lock_state = input->bytes[8u];
    local.door_open_state = input->bytes[9u];
    local.door_composite_state = input->bytes[10u];
    local.lock_quality = input->bytes[11u];
    local.open_quality = input->bytes[12u];
    local.composite_quality = input->bytes[13u];
    local.update_sequence = nw_read_u32(input->bytes + 14u);
    local.source_age_ms = nw_read_u16(input->bytes + 18u);
    status = bcm_door_state_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_climate_state_valid(const NetWireBcmClimateStateRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    if (m->fan_command_level > 3u) return NET_WIRE_MALFORMED;
    if ((m->fan_measured_level > 3u && m->fan_measured_level != UINT8_MAX)) return NET_WIRE_MALFORMED;
    if (m->thermal_direction_state > 2u) return NET_WIRE_MALFORMED;
    if (m->thermal_output_percent > 100u) return NET_WIRE_MALFORMED;
    if (!tri(m->heat_removal_state)) return NET_WIRE_MALFORMED;
    if (m->fan_command_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->fan_measured_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->thermal_direction_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->thermal_output_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->heat_removal_quality > 3u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_climate_state(NetWireProfile profile,
    const NetWireBcmClimateStateRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_climate_state_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x210u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8u] = input->fan_command_level;
    local.bytes[9u] = input->fan_measured_level;
    local.bytes[10u] = input->thermal_direction_state;
    local.bytes[11u] = input->thermal_output_percent;
    local.bytes[12u] = input->heat_removal_state;
    local.bytes[13u] = input->fan_command_quality;
    local.bytes[14u] = input->fan_measured_quality;
    local.bytes[15u] = input->thermal_direction_quality;
    local.bytes[16u] = input->thermal_output_quality;
    local.bytes[17u] = input->heat_removal_quality;
    nw_write_u32(local.bytes + 18u, input->update_sequence);
    nw_write_u16(local.bytes + 22u, input->source_age_ms);
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_climate_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmClimateStateRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmClimateStateRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x210u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.fan_command_level = input->bytes[8u];
    local.fan_measured_level = input->bytes[9u];
    local.thermal_direction_state = input->bytes[10u];
    local.thermal_output_percent = input->bytes[11u];
    local.heat_removal_state = input->bytes[12u];
    local.fan_command_quality = input->bytes[13u];
    local.fan_measured_quality = input->bytes[14u];
    local.thermal_direction_quality = input->bytes[15u];
    local.thermal_output_quality = input->bytes[16u];
    local.heat_removal_quality = input->bytes[17u];
    local.update_sequence = nw_read_u32(input->bytes + 18u);
    local.source_age_ms = nw_read_u16(input->bytes + 22u);
    status = bcm_climate_state_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_light_state_valid(const NetWireBcmLightStateRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    if (m->interior_light_type_state > 3u) return NET_WIRE_MALFORMED;
    if (m->light_level_percent > 100u) return NET_WIRE_MALFORMED;
    if (!tri(m->interior_light_apply_result)) return NET_WIRE_MALFORMED;
    if (m->light_physical_feedback_capability > 1u) return NET_WIRE_MALFORMED;
    if (m->light_type_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->light_level_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->light_color_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->light_apply_quality > 3u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_light_state(NetWireProfile profile,
    const NetWireBcmLightStateRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_light_state_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x220u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8u] = input->interior_light_type_state;
    local.bytes[9u] = input->light_level_percent;
    local.bytes[10u] = input->red;
    local.bytes[11u] = input->green;
    local.bytes[12u] = input->blue;
    local.bytes[13u] = input->interior_light_apply_result;
    local.bytes[14u] = input->light_physical_feedback_capability;
    local.bytes[15u] = input->light_type_quality;
    local.bytes[16u] = input->light_level_quality;
    local.bytes[17u] = input->light_color_quality;
    local.bytes[18u] = input->light_apply_quality;
    nw_write_u32(local.bytes + 19u, input->update_sequence);
    nw_write_u16(local.bytes + 23u, input->source_age_ms);
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_light_state(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmLightStateRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmLightStateRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x220u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.interior_light_type_state = input->bytes[8u];
    local.light_level_percent = input->bytes[9u];
    local.red = input->bytes[10u];
    local.green = input->bytes[11u];
    local.blue = input->bytes[12u];
    local.interior_light_apply_result = input->bytes[13u];
    local.light_physical_feedback_capability = input->bytes[14u];
    local.light_type_quality = input->bytes[15u];
    local.light_level_quality = input->bytes[16u];
    local.light_color_quality = input->bytes[17u];
    local.light_apply_quality = input->bytes[18u];
    local.update_sequence = nw_read_u32(input->bytes + 19u);
    local.source_age_ms = nw_read_u16(input->bytes + 23u);
    status = bcm_light_state_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_result_valid(const NetWireBcmResultRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    /* Required secondary boot has no documented zero identity binding.
     * Conservative candidate HOLD; not a new malformed wire reservation. */
    if (m->command_domain_boot == 0u) return NET_WIRE_CONTRACT_HOLD;
    /* CAN RESULT absence 255 is explicitly unbound, not a reserved result. */
    if (m->result == UINT8_MAX) return NET_WIRE_CONTRACT_HOLD;
    if ((m->command_kind < 1u || m->command_kind > 3u)) return NET_WIRE_MALFORMED;
    if (m->result > 5u) return NET_WIRE_MALFORMED;
    if (m->result_reason > 25u) return NET_WIRE_MALFORMED;
    if (m->door_event > 5u) return NET_WIRE_MALFORMED;
    if (m->event_quality > 3u) return NET_WIRE_MALFORMED;
    if (!tri(m->door_target)) return NET_WIRE_MALFORMED;
    if ((m->command_kind != 1u && (m->door_event != 0u || m->occurrence_id != 0u || m->door_target != UINT8_MAX))) return NET_WIRE_MALFORMED;
    if ((m->command_kind == 1u && m->door_target == UINT8_MAX)) return NET_WIRE_MALFORMED;
    if ((m->door_event == 0u && m->occurrence_id != 0u)) return NET_WIRE_MALFORMED;
    /* Existing adapter requires an occurrence for door evidence. Conservatively
     * HOLD its missing identity; never fabricate an occurrence or confirmation. */
    if (m->door_event != 0u && m->occurrence_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_result(NetWireProfile profile,
    const NetWireBcmResultRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_result_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x140u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    nw_write_u32(local.bytes + 8u, input->command_domain_boot);
    nw_write_u32(local.bytes + 12u, input->command_id);
    local.bytes[16u] = input->command_kind;
    local.bytes[17u] = input->result;
    nw_write_u16(local.bytes + 18u, input->result_reason);
    local.bytes[20u] = input->door_event;
    nw_write_u32(local.bytes + 21u, input->occurrence_id);
    nw_write_u16(local.bytes + 25u, input->event_age_ms);
    local.bytes[27u] = input->event_quality;
    local.bytes[28u] = input->door_target;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_result(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmResultRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmResultRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x140u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.command_domain_boot = nw_read_u32(input->bytes + 8u);
    local.command_id = nw_read_u32(input->bytes + 12u);
    local.command_kind = input->bytes[16u];
    local.result = input->bytes[17u];
    local.result_reason = nw_read_u16(input->bytes + 18u);
    local.door_event = input->bytes[20u];
    local.occurrence_id = nw_read_u32(input->bytes + 21u);
    local.event_age_ms = nw_read_u16(input->bytes + 25u);
    local.event_quality = input->bytes[27u];
    local.door_target = input->bytes[28u];
    status = bcm_result_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_event_valid(const NetWireBcmEventRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    if ((m->event_kind < 1u || m->event_kind > 3u)) return NET_WIRE_MALFORMED;
    if (m->event_quality > 3u) return NET_WIRE_MALFORMED;
    if (m->affected_function > 7u) return NET_WIRE_MALFORMED;
    /* Optional related identity is paired. Half-absence has no agreed meaning. */
    if ((m->related_domain_boot == 0u) != (m->related_command_id == 0u))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_event(NetWireProfile profile,
    const NetWireBcmEventRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_event_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x141u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8u] = input->event_kind;
    nw_write_u32(local.bytes + 9u, input->occurrence_id);
    nw_write_u16(local.bytes + 13u, input->event_age_ms);
    local.bytes[15u] = input->event_quality;
    nw_write_u32(local.bytes + 16u, input->related_domain_boot);
    nw_write_u32(local.bytes + 20u, input->related_command_id);
    local.bytes[24u] = input->affected_function;
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_event(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmEventRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmEventRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x141u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.event_kind = input->bytes[8u];
    local.occurrence_id = nw_read_u32(input->bytes + 9u);
    local.event_age_ms = nw_read_u16(input->bytes + 13u);
    local.event_quality = input->bytes[15u];
    local.related_domain_boot = nw_read_u32(input->bytes + 16u);
    local.related_command_id = nw_read_u32(input->bytes + 20u);
    local.affected_function = input->bytes[24u];
    status = bcm_event_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

static NetWireStatus bcm_status_valid(const NetWireBcmStatusRaw *m)
{
    if (m->boot_id == 0u) return NET_WIRE_MALFORMED;
    if (m->bcm_ecu_state > 3u) return NET_WIRE_MALFORMED;
    if (m->fault_mask > 255u) return NET_WIRE_MALFORMED;
    if (m->active_fault_category > 3u) return NET_WIRE_MALFORMED;
    if (m->active_fault_code > 8u) return NET_WIRE_MALFORMED;
    if (m->affected_function > 7u) return NET_WIRE_MALFORMED;
    if (m->recovering > 1u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_encode_bcm_status(NetWireProfile profile,
    const NetWireBcmStatusRaw *input, NetWireCanEnvelope *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireCanEnvelope local;
    if (status != NET_WIRE_OK) return status;
    status = bcm_status_valid(input);
    if (status != NET_WIRE_OK) return status;
    local = nw_can_new_envelope(0x230u);
    nw_write_u32(local.bytes + 0u, input->boot_id);
    nw_write_u32(local.bytes + 4u, input->tx_sequence);
    local.bytes[8u] = input->bcm_ecu_state;
    nw_write_u16(local.bytes + 9u, input->fault_mask);
    local.bytes[11u] = input->active_fault_category;
    nw_write_u16(local.bytes + 12u, input->active_fault_code);
    local.bytes[14u] = input->affected_function;
    local.bytes[15u] = input->recovering;
    nw_write_u32(local.bytes + 16u, input->update_sequence);
    nw_write_u16(local.bytes + 20u, input->source_age_ms);
    *output = local;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_can_decode_bcm_status(NetWireProfile profile,
    const NetWireCanEnvelope *input, NetWireBcmStatusRaw *output)
{
    NetWireStatus status = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output));
    NetWireBcmStatusRaw local = {0};
    if (status != NET_WIRE_OK) return status;
    status = nw_can_envelope_valid(input, 0x230u);
    if (status != NET_WIRE_OK) return status;
    local.boot_id = nw_read_u32(input->bytes + 0u);
    local.tx_sequence = nw_read_u32(input->bytes + 4u);
    local.bcm_ecu_state = input->bytes[8u];
    local.fault_mask = nw_read_u16(input->bytes + 9u);
    local.active_fault_category = input->bytes[11u];
    local.active_fault_code = nw_read_u16(input->bytes + 12u);
    local.affected_function = input->bytes[14u];
    local.recovering = input->bytes[15u];
    local.update_sequence = nw_read_u32(input->bytes + 16u);
    local.source_age_ms = nw_read_u16(input->bytes + 20u);
    status = bcm_status_valid(&local);
    if (status != NET_WIRE_OK) return status;
    *output = local;
    return NET_WIRE_OK;
}

