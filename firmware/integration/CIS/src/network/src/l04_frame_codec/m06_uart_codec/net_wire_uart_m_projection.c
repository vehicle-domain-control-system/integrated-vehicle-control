#include "network/net_wire_uart_m_projection.h"
#include "net_wire_uart_typed_internal.h"

/* Frozen FINAL_98 20_UART_M.md:386..653. Explicit field serialization;
 * no CAN framing dependency, HAL, C2 projection, heap or runtime state. */
static bool tri(uint8_t value) { return value <= 1u || value == UINT8_MAX; }
static NetWireStatus sensor_quality(uint8_t valid, uint8_t reason, bool sentinel)
{
    if (valid > 1u || reason > 9u || (valid == 1u && sentinel)) return NET_WIRE_MALFORMED;
    if ((valid == 1u && reason != 0u) || (valid == 0u && reason == 0u))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_bcm_door_state(const NetWireMBcmDoorStateRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (!tri(m->door_lock_state) || !tri(m->door_open_state) ||
        m->door_composite_state > 2u || m->lock_quality > 3u ||
        m->open_quality > 3u || m->composite_quality > 3u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_bcm_door_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmDoorStateRaw *m, NetWireUartFrame *out)
{
    uint8_t p[40];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_bcm_door_state(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->door_lock_state;
    p[29] = m->door_open_state;
    p[30] = m->door_composite_state;
    p[31] = m->lock_quality;
    p[32] = m->open_quality;
    p[33] = m->composite_quality;
    nw_write_u32(p + 34u, m->update_sequence);
    nw_write_u16(p + 38u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x30u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_bcm_door_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmDoorStateRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMBcmDoorStateRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x30u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.door_lock_state = p[28];
    m.door_open_state = p[29];
    m.door_composite_state = p[30];
    m.lock_quality = p[31];
    m.open_quality = p[32];
    m.composite_quality = p[33];
    m.update_sequence = nw_read_u32(p + 34u);
    m.source_age_ms = nw_read_u16(p + 38u);
    status = valid_m_bcm_door_state(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_bcm_climate_state(const NetWireMBcmClimateStateRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->fan_command_level > 3u ||
        (m->fan_measured_level > 3u && m->fan_measured_level != UINT8_MAX) ||
        m->thermal_direction_state > 2u || m->thermal_output_percent > 100u ||
        !tri(m->heat_removal_state) || m->fan_command_quality > 3u ||
        m->fan_measured_quality > 3u || m->thermal_direction_quality > 3u ||
        m->thermal_output_quality > 3u || m->heat_removal_quality > 3u)
        return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_bcm_climate_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmClimateStateRaw *m, NetWireUartFrame *out)
{
    uint8_t p[44];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_bcm_climate_state(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->fan_command_level;
    p[29] = m->fan_measured_level;
    p[30] = m->thermal_direction_state;
    p[31] = m->thermal_output_percent;
    p[32] = m->heat_removal_state;
    p[33] = m->fan_command_quality;
    p[34] = m->fan_measured_quality;
    p[35] = m->thermal_direction_quality;
    p[36] = m->thermal_output_quality;
    p[37] = m->heat_removal_quality;
    nw_write_u32(p + 38u, m->update_sequence);
    nw_write_u16(p + 42u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x31u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_bcm_climate_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmClimateStateRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMBcmClimateStateRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x31u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.fan_command_level = p[28];
    m.fan_measured_level = p[29];
    m.thermal_direction_state = p[30];
    m.thermal_output_percent = p[31];
    m.heat_removal_state = p[32];
    m.fan_command_quality = p[33];
    m.fan_measured_quality = p[34];
    m.thermal_direction_quality = p[35];
    m.thermal_output_quality = p[36];
    m.heat_removal_quality = p[37];
    m.update_sequence = nw_read_u32(p + 38u);
    m.source_age_ms = nw_read_u16(p + 42u);
    status = valid_m_bcm_climate_state(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_bcm_light_state(const NetWireMBcmLightStateRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->interior_light_type_state > 3u || m->light_level_percent > 100u ||
        !tri(m->interior_light_apply_result) || m->light_physical_feedback_capability > 1u ||
        m->light_type_quality > 3u || m->light_level_quality > 3u ||
        m->light_color_quality > 3u || m->light_apply_quality > 3u)
        return NET_WIRE_MALFORMED;
    /* RGB255 is a value. APPLIED/capability1 does not prove physical light. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_bcm_light_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmLightStateRaw *m, NetWireUartFrame *out)
{
    uint8_t p[45];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_bcm_light_state(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->interior_light_type_state;
    p[29] = m->light_level_percent;
    p[30] = m->color_r;
    p[31] = m->color_g;
    p[32] = m->color_b;
    p[33] = m->interior_light_apply_result;
    p[34] = m->light_physical_feedback_capability;
    p[35] = m->light_type_quality;
    p[36] = m->light_level_quality;
    p[37] = m->light_color_quality;
    p[38] = m->light_apply_quality;
    nw_write_u32(p + 39u, m->update_sequence);
    nw_write_u16(p + 43u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x32u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_bcm_light_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmLightStateRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMBcmLightStateRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x32u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.interior_light_type_state = p[28];
    m.light_level_percent = p[29];
    m.color_r = p[30];
    m.color_g = p[31];
    m.color_b = p[32];
    m.interior_light_apply_result = p[33];
    m.light_physical_feedback_capability = p[34];
    m.light_type_quality = p[35];
    m.light_level_quality = p[36];
    m.light_color_quality = p[37];
    m.light_apply_quality = p[38];
    m.update_sequence = nw_read_u32(p + 39u);
    m.source_age_ms = nw_read_u16(p + 43u);
    status = valid_m_bcm_light_state(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_bcm_status(const NetWireMBcmStatusRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->bcm_ecu_state > 3u || m->fault_mask > 255u ||
        m->active_fault_category > 3u || m->active_fault_code > 8u ||
        m->affected_function > 7u || m->recovering > 1u) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_bcm_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMBcmStatusRaw *m, NetWireUartFrame *out)
{
    uint8_t p[42];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_bcm_status(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->bcm_ecu_state;
    nw_write_u16(p + 29u, m->fault_mask);
    p[31] = m->active_fault_category;
    nw_write_u16(p + 32u, m->active_fault_code);
    p[34] = m->affected_function;
    p[35] = m->recovering;
    nw_write_u32(p + 36u, m->update_sequence);
    nw_write_u16(p + 40u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x33u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_bcm_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMBcmStatusRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMBcmStatusRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x33u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.bcm_ecu_state = p[28];
    m.fault_mask = nw_read_u16(p + 29u);
    m.active_fault_category = p[31];
    m.active_fault_code = nw_read_u16(p + 32u);
    m.affected_function = p[34];
    m.recovering = p[35];
    m.update_sequence = nw_read_u32(p + 36u);
    m.source_age_ms = nw_read_u16(p + 40u);
    status = valid_m_bcm_status(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_cis_environment(const NetWireMCisEnvironmentRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    NetWireStatus status;
    if (((m->temperature_centi_c < -4000 || m->temperature_centi_c > 12500) &&
         m->temperature_centi_c != INT16_MIN) ||
        (m->humidity_centi_percent > 10000u && m->humidity_centi_percent != UINT16_MAX) ||
        (m->illuminance_lux > UINT32_C(200000) && m->illuminance_lux != UINT32_MAX))
        return NET_WIRE_MALFORMED;
    status = sensor_quality(m->temperature_validity, m->temperature_reason,
        m->temperature_centi_c == INT16_MIN);
    if (status != NET_WIRE_OK) return status;
    status = sensor_quality(m->humidity_validity, m->humidity_reason,
        m->humidity_centi_percent == UINT16_MAX);
    if (status != NET_WIRE_OK) return status;
    return sensor_quality(m->illuminance_validity, m->illuminance_reason,
        m->illuminance_lux == UINT32_MAX);
}

NetWireStatus net_wire_m_cis_environment_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisEnvironmentRaw *m, NetWireUartFrame *out)
{
    uint8_t p[60];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_cis_environment(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    nw_write_i16(p + 28u, m->temperature_centi_c);
    nw_write_u32(p + 30u, m->temperature_sequence);
    nw_write_u16(p + 34u, m->temperature_age_ms);
    p[36] = m->temperature_validity;
    p[37] = m->temperature_reason;
    nw_write_u16(p + 38u, m->humidity_centi_percent);
    nw_write_u32(p + 40u, m->humidity_sequence);
    nw_write_u16(p + 44u, m->humidity_age_ms);
    p[46] = m->humidity_validity;
    p[47] = m->humidity_reason;
    nw_write_u32(p + 48u, m->illuminance_lux);
    nw_write_u32(p + 52u, m->illuminance_sequence);
    nw_write_u16(p + 56u, m->illuminance_age_ms);
    p[58] = m->illuminance_validity;
    p[59] = m->illuminance_reason;
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x34u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_cis_environment_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisEnvironmentRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMCisEnvironmentRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x34u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.temperature_centi_c = nw_read_i16(p + 28u);
    m.temperature_sequence = nw_read_u32(p + 30u);
    m.temperature_age_ms = nw_read_u16(p + 34u);
    m.temperature_validity = p[36];
    m.temperature_reason = p[37];
    m.humidity_centi_percent = nw_read_u16(p + 38u);
    m.humidity_sequence = nw_read_u32(p + 40u);
    m.humidity_age_ms = nw_read_u16(p + 44u);
    m.humidity_validity = p[46];
    m.humidity_reason = p[47];
    m.illuminance_lux = nw_read_u32(p + 48u);
    m.illuminance_sequence = nw_read_u32(p + 52u);
    m.illuminance_age_ms = nw_read_u16(p + 56u);
    m.illuminance_validity = p[58];
    m.illuminance_reason = p[59];
    status = valid_m_cis_environment(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_cis_occupant(const NetWireMCisOccupantRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    NetWireStatus status;
    if (!tri(m->occupant_presence) ||
        (m->occupant_count > 5u && m->occupant_count != UINT8_MAX) ||
        m->vision_function_status > 4u) return NET_WIRE_MALFORMED;
    status = sensor_quality(m->presence_validity, m->presence_reason, m->occupant_presence == UINT8_MAX);
    if (status != NET_WIRE_OK) return status;
    status = sensor_quality(m->count_validity, m->count_reason, m->occupant_count == UINT8_MAX);
    if (status != NET_WIRE_OK) return status;
    if (m->presence_validity == 1u && m->count_validity == 1u &&
        ((m->occupant_presence == 0u && m->occupant_count != 0u) ||
         (m->occupant_presence == 1u && m->occupant_count == 0u))) return NET_WIRE_MALFORMED;
    /* Missing original Pi identity has no complete absence contract. */
    if (m->pi_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_cis_occupant_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisOccupantRaw *m, NetWireUartFrame *out)
{
    uint8_t p[45];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_cis_occupant(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    nw_write_u32(p + 28u, m->pi_boot_id);
    nw_write_u32(p + 32u, m->vision_sequence);
    nw_write_u16(p + 36u, m->vision_age_ms);
    p[38] = m->occupant_presence;
    p[39] = m->presence_validity;
    p[40] = m->presence_reason;
    p[41] = m->occupant_count;
    p[42] = m->count_validity;
    p[43] = m->count_reason;
    p[44] = m->vision_function_status;
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x35u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_cis_occupant_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisOccupantRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMCisOccupantRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x35u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.pi_boot_id = nw_read_u32(p + 28u);
    m.vision_sequence = nw_read_u32(p + 32u);
    m.vision_age_ms = nw_read_u16(p + 36u);
    m.occupant_presence = p[38];
    m.presence_validity = p[39];
    m.presence_reason = p[40];
    m.occupant_count = p[41];
    m.count_validity = p[42];
    m.count_reason = p[43];
    m.vision_function_status = p[44];
    status = valid_m_cis_occupant(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_cis_rear(const NetWireMCisRearRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->validity > 1u || m->quality_reason > 9u || m->proximity_status > 5u)
        return NET_WIRE_MALFORMED;
    if (m->proximity_status == 0u && m->rear_distance_deci_cm == UINT16_MAX)
        return NET_WIRE_MALFORMED;
    if ((m->validity == 1u && m->quality_reason != 0u) ||
        (m->validity == 0u && m->quality_reason == 0u) ||
        (m->proximity_status == 1u && m->rear_distance_deci_cm != UINT16_MAX) ||
        (m->validity == 1u && m->proximity_status >= 2u && m->proximity_status <= 4u))
        return NET_WIRE_CONTRACT_HOLD;
    /* Preserve deci-cm raw: 123 is 123mm, no intermediate cm rounding. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_cis_rear_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisRearRaw *m, NetWireUartFrame *out)
{
    uint8_t p[39];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_cis_rear(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    nw_write_u32(p + 28u, m->update_sequence);
    nw_write_u16(p + 32u, m->source_age_ms);
    nw_write_u16(p + 34u, m->rear_distance_deci_cm);
    p[36] = m->validity;
    p[37] = m->quality_reason;
    p[38] = m->proximity_status;
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x36u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_cis_rear_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisRearRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMCisRearRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x36u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.update_sequence = nw_read_u32(p + 28u);
    m.source_age_ms = nw_read_u16(p + 32u);
    m.rear_distance_deci_cm = nw_read_u16(p + 34u);
    m.validity = p[36];
    m.quality_reason = p[37];
    m.proximity_status = p[38];
    status = valid_m_cis_rear(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_cis_status(const NetWireMCisStatusRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->cis_state > 3u || m->vision_status > 4u || m->temperature_status > 4u ||
        m->humidity_status > 4u || m->illuminance_status > 4u || m->rear_status > 4u ||
        m->pi_uart_status > 2u || m->can_interface_status > 2u ||
        (m->fault_mask & UINT16_C(0xfe00)) != 0u || m->active_fault > 9u ||
        m->recovering > 1u || m->last_fault > 9u || (m->affected_function & 0xc0u) != 0u)
        return NET_WIRE_MALFORMED;
    /* Current and historical faults are independent; no healthy synthesis. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_cis_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMCisStatusRaw *m, NetWireUartFrame *out)
{
    uint8_t p[54];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_cis_status(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->cis_state;
    p[29] = m->vision_status;
    p[30] = m->temperature_status;
    p[31] = m->humidity_status;
    p[32] = m->illuminance_status;
    p[33] = m->rear_status;
    p[34] = m->pi_uart_status;
    p[35] = m->can_interface_status;
    nw_write_u16(p + 36u, m->fault_mask);
    nw_write_u16(p + 38u, m->active_fault);
    p[40] = m->recovering;
    nw_write_u16(p + 41u, m->last_fault);
    nw_write_u32(p + 43u, m->last_fault_occurrence);
    p[47] = m->affected_function;
    nw_write_u32(p + 48u, m->update_sequence);
    nw_write_u16(p + 52u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x37u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_cis_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMCisStatusRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMCisStatusRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x37u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.cis_state = p[28];
    m.vision_status = p[29];
    m.temperature_status = p[30];
    m.humidity_status = p[31];
    m.illuminance_status = p[32];
    m.rear_status = p[33];
    m.pi_uart_status = p[34];
    m.can_interface_status = p[35];
    m.fault_mask = nw_read_u16(p + 36u);
    m.active_fault = nw_read_u16(p + 38u);
    m.recovering = p[40];
    m.last_fault = nw_read_u16(p + 41u);
    m.last_fault_occurrence = nw_read_u32(p + 43u);
    m.affected_function = p[47];
    m.update_sequence = nw_read_u32(p + 48u);
    m.source_age_ms = nw_read_u16(p + 52u);
    status = valid_m_cis_status(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_vss_status(const NetWireMVssStatusRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->vss_state > 3u || m->vss_availability > 2u ||
        m->vss_accepting_events > 1u || m->vss_fault_active > 1u ||
        (m->fault_mask & 0xe0u) != 0u || m->vss_last_fault > 5u ||
        (m->last_fault_quality != 0u && m->last_fault_quality != 3u))
        return NET_WIRE_MALFORMED;
    if (m->last_fault_quality == 3u && (m->vss_last_fault != 0u ||
        m->last_fault_occurrence != 0u || m->last_fault_age_ms != 0u)) return NET_WIRE_MALFORMED;
    if (m->last_fault_quality == 0u && m->vss_last_fault == 0u) return NET_WIRE_CONTRACT_HOLD;
    /* FAULT_MASK is u8; retained history is not current fault or A2 commit. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_vss_status_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMVssStatusRaw *m, NetWireUartFrame *out)
{
    uint8_t p[48];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_vss_status(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->vss_state;
    p[29] = m->vss_availability;
    p[30] = m->vss_accepting_events;
    p[31] = m->vss_fault_active;
    p[32] = m->fault_mask;
    nw_write_u16(p + 33u, m->vss_last_fault);
    nw_write_u32(p + 35u, m->last_fault_occurrence);
    nw_write_u16(p + 39u, m->last_fault_age_ms);
    p[41] = m->last_fault_quality;
    nw_write_u32(p + 42u, m->update_sequence);
    nw_write_u16(p + 46u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x3au, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_vss_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMVssStatusRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMVssStatusRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x3au, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.vss_state = p[28];
    m.vss_availability = p[29];
    m.vss_accepting_events = p[30];
    m.vss_fault_active = p[31];
    m.fault_mask = p[32];
    m.vss_last_fault = nw_read_u16(p + 33u);
    m.last_fault_occurrence = nw_read_u32(p + 35u);
    m.last_fault_age_ms = nw_read_u16(p + 39u);
    m.last_fault_quality = p[41];
    m.update_sequence = nw_read_u32(p + 42u);
    m.source_age_ms = nw_read_u16(p + 46u);
    status = valid_m_vss_status(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_window_state(const NetWireMWindowStateRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if ((m->motion_state > 3u && m->motion_state != UINT8_MAX) || m->window_ecu_state > 3u ||
        (m->window_position > 100u && m->window_position != UINT8_MAX) ||
        m->window_value_quality > 3u || !tri(m->fully_open_state) || !tri(m->fully_closed_state) ||
        m->antipinch_protection_status > 3u || !tri(m->reverse_state)) return NET_WIRE_MALFORMED;
    if (m->window_value_quality == 0u &&
        (m->window_position == UINT8_MAX || m->motion_state == UINT8_MAX ||
         m->fully_open_state == UINT8_MAX || m->fully_closed_state == UINT8_MAX ||
         m->reverse_state == UINT8_MAX ||
         (m->fully_open_state == 1u && m->window_position != 0u) ||
         (m->fully_closed_state == 1u && m->window_position != 100u)))
        return NET_WIRE_CONTRACT_HOLD;
    /* WINDOW_EXECUTION_HOLD: raw representation never enables execution. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_window_state_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMWindowStateRaw *m, NetWireUartFrame *out)
{
    uint8_t p[42];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_window_state(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->motion_state;
    p[29] = m->window_ecu_state;
    p[30] = m->window_position;
    p[31] = m->window_value_quality;
    p[32] = m->fully_open_state;
    p[33] = m->fully_closed_state;
    p[34] = m->antipinch_protection_status;
    p[35] = m->reverse_state;
    nw_write_u32(p + 36u, m->update_sequence);
    nw_write_u16(p + 40u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x38u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_window_state_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWindowStateRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMWindowStateRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x38u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.motion_state = p[28];
    m.window_ecu_state = p[29];
    m.window_position = p[30];
    m.window_value_quality = p[31];
    m.fully_open_state = p[32];
    m.fully_closed_state = p[33];
    m.antipinch_protection_status = p[34];
    m.reverse_state = p[35];
    m.update_sequence = nw_read_u32(p + 36u);
    m.source_age_ms = nw_read_u16(p + 40u);
    status = valid_m_window_state(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus valid_m_window_fault(const NetWireMWindowFaultRaw *m)
{
    if (m->device_context_id == 0u || m->domain_boot_id == 0u || m->original_can_boot_id == 0u)
        return NET_WIRE_MALFORMED;
    if (m->fault_category > 3u || m->fault_status > 2u ||
        (m->fault_code > 6u && m->fault_code != 255u)) return NET_WIRE_MALFORMED;
    /* Code255 is OTHER, not UNKNOWN. No monitoring/alarms are enabled. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_window_fault_encode(NetWireProfile profile,
    uint16_t link_sequence, const NetWireMWindowFaultRaw *m, NetWireUartFrame *out)
{
    uint8_t p[42];
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = valid_m_window_fault(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0u, m->device_context_id);
    nw_write_u64(p + 4u, m->session_id);
    nw_write_u32(p + 12u, m->domain_boot_id);
    nw_write_u32(p + 16u, m->query_id);
    nw_write_u32(p + 20u, m->original_can_boot_id);
    nw_write_u32(p + 24u, m->original_can_tx_sequence);
    p[28] = m->fault_category;
    nw_write_u16(p + 29u, m->fault_code);
    p[31] = m->fault_status;
    nw_write_u32(p + 32u, m->occurrence_id);
    nw_write_u32(p + 36u, m->update_sequence);
    nw_write_u16(p + 40u, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x39u, link_sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_window_fault_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWindowFaultRaw *out, uint16_t *link_sequence_out)
{
    NetWireUartPayload payload;
    NetWireMWindowFaultRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        link_sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x39u, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0u);
    m.session_id = nw_read_u64(p + 4u);
    m.domain_boot_id = nw_read_u32(p + 12u);
    m.query_id = nw_read_u32(p + 16u);
    m.original_can_boot_id = nw_read_u32(p + 20u);
    m.original_can_tx_sequence = nw_read_u32(p + 24u);
    m.fault_category = p[28];
    m.fault_code = nw_read_u16(p + 29u);
    m.fault_status = p[31];
    m.occurrence_id = nw_read_u32(p + 32u);
    m.update_sequence = nw_read_u32(p + 36u);
    m.source_age_ms = nw_read_u16(p + 40u);
    status = valid_m_window_fault(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *link_sequence_out = payload.sequence;
    return NET_WIRE_OK;
}
