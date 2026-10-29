#include "network/net_wire_uart_dto.h"
#include "../net_wire_internal.h"
#include <string.h>

static NetWireStatus encode_args(NetWireProfile profile, const void *message,
    size_t size, NetWireUartFrame *out)
{
    if (message == NULL || out == NULL ||
        net_wire_ranges_overlap(message, size, out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    return net_wire_profile_check(profile);
}

static NetWireStatus decode_payload(NetWireProfile profile,
    const NetWireUartFrame *frame, void *out, size_t size, uint16_t *sequence_out,
    NetWireLink link, NetWireDirection direction, uint8_t type,
    NetWireUartPayload *payload)
{
    NetWireStatus status;
    if (frame == NULL || out == NULL || sequence_out == NULL ||
        net_wire_ranges_overlap(frame, sizeof(*frame), out, size) ||
        net_wire_ranges_overlap(frame, sizeof(*frame), sequence_out, sizeof(*sequence_out)) ||
        net_wire_ranges_overlap(out, size, sequence_out, sizeof(*sequence_out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = net_wire_uart_decode(profile, link, direction, frame, payload);
    if (status != NET_WIRE_OK) return status;
    return payload->type == type ? NET_WIRE_OK : NET_WIRE_MALFORMED;
}

static bool tristate(uint8_t value)
{
    return value <= 1U || value == UINT8_MAX;
}

/* The byte contract distinguishes unsupported codes from contradictory meaning.
 * Never repair a contradiction into a fresh/known/allowed value. */
static NetWireStatus validity_pair(uint8_t value, uint8_t validity, uint8_t reason)
{
    if (validity > 1U || reason > 9U) return NET_WIRE_MALFORMED;
    if (validity == 1U && (value == UINT8_MAX || reason != 0U))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

static NetWireStatus vision_check(const NetWirePVisionResult *m)
{
    NetWireStatus status;
    if (m->pi_boot_id == 0U || !tristate(m->occupant_presence) ||
        (m->occupant_count > 5U && m->occupant_count != UINT8_MAX) ||
        m->vision_status > 4U || m->vision_fault_code > 4U)
        return NET_WIRE_MALFORMED;
    status = validity_pair(m->occupant_presence, m->presence_validity, m->presence_reason);
    if (status != NET_WIRE_OK) return status;
    status = validity_pair(m->occupant_count, m->count_validity, m->count_reason);
    if (status != NET_WIRE_OK) return status;
    if (m->presence_validity == 1U && m->count_validity == 1U &&
        ((m->occupant_presence == 0U && m->occupant_count != 0U) ||
         (m->occupant_presence == 1U && m->occupant_count == 0U)))
        return NET_WIRE_CONTRACT_HOLD;
    /* A usable field with incompatible function evidence needs an owner
     * decision; never silently select validity over the reported fault. */
    if ((m->presence_validity == 1U || m->count_validity == 1U) &&
        (m->vision_fault_code != 0U || m->vision_status == 0U ||
         m->vision_status == 3U || m->vision_status == 4U))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_p_vision_result_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePVisionResult *m, NetWireUartFrame *out)
{
    uint8_t p[19] = {0};
    NetWireStatus status = encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = vision_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->pi_boot_id); nw_write_u32(p + 4, m->vision_sequence);
    nw_write_u16(p + 8, m->vision_age_ms);
    p[10] = m->occupant_presence; p[11] = m->presence_validity;
    p[12] = m->presence_reason; p[13] = m->occupant_count;
    p[14] = m->count_validity; p[15] = m->count_reason;
    p[16] = m->vision_status; nw_write_u16(p + 17, m->vision_fault_code);
    return net_wire_uart_encode(profile, NET_WIRE_UART_C, NET_WIRE_UPLINK,
        0x60U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_p_vision_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePVisionResult *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWirePVisionResult m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_C, NET_WIRE_UPLINK, 0x60U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.pi_boot_id = nw_read_u32(p); m.vision_sequence = nw_read_u32(p + 4);
    m.vision_age_ms = nw_read_u16(p + 8);
    m.occupant_presence = p[10]; m.presence_validity = p[11];
    m.presence_reason = p[12]; m.occupant_count = p[13];
    m.count_validity = p[14]; m.count_reason = p[15];
    m.vision_status = p[16]; m.vision_fault_code = nw_read_u16(p + 17);
    status = vision_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus permission_check(const NetWirePPermission *m)
{
    if (m->cis_boot_id == 0U || !tristate(m->vehicle_power_permission) ||
        m->value_quality > 3U || m->quality_reason > 9U)
        return NET_WIRE_MALFORMED;
    if (m->value_quality == 0U &&
        (m->vehicle_power_permission == UINT8_MAX || m->quality_reason != 0U))
        return NET_WIRE_CONTRACT_HOLD;
    /* USE_LIMIT and AGE are raw u16. 1500 vs 500 is NOT selected here. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_p_permission_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePPermission *m, NetWireUartFrame *out)
{
    uint8_t p[15] = {0};
    NetWireStatus status = encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = permission_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->cis_boot_id); nw_write_u32(p + 4, m->update_sequence);
    p[8] = m->vehicle_power_permission; p[9] = m->value_quality; p[10] = m->quality_reason;
    nw_write_u16(p + 11, m->use_limit_ms); nw_write_u16(p + 13, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_C, NET_WIRE_DOWNLINK,
        0x62U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_p_permission_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePPermission *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWirePPermission m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_C, NET_WIRE_DOWNLINK, 0x62U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.cis_boot_id = nw_read_u32(p); m.update_sequence = nw_read_u32(p + 4);
    m.vehicle_power_permission = p[8]; m.value_quality = p[9]; m.quality_reason = p[10];
    m.use_limit_ms = nw_read_u16(p + 11); m.source_age_ms = nw_read_u16(p + 13);
    status = permission_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus context_check(const NetWireMContext *m)
{
    NetWireStatus status;
    if (m->device_context_id == 0U || !tristate(m->device_registration_state) ||
        !tristate(m->bt_connection_state) || !tristate(m->app_active_state) ||
        !tristate(m->proximity_state) || m->app_quality > 3U ||
        m->registration_connection_quality > 3U)
        return NET_WIRE_MALFORMED;
    status = validity_pair(m->proximity_state, m->proximity_validity, m->proximity_reason);
    if (status != NET_WIRE_OK) return status;
    if ((m->app_quality == 0U && m->app_active_state == UINT8_MAX) ||
        (m->registration_connection_quality == 0U &&
         (m->device_registration_state == UINT8_MAX || m->bt_connection_state == UINT8_MAX)))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_context_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMContext *m, NetWireUartFrame *out)
{
    uint8_t p[38] = {0};
    NetWireStatus status = encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = context_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->device_context_id); nw_write_u64(p + 4, m->session_id);
    p[12] = m->device_registration_state; p[13] = m->bt_connection_state;
    p[14] = m->app_active_state; p[15] = m->app_quality;
    nw_write_u32(p + 16, m->app_update_sequence); nw_write_u16(p + 20, m->app_age_ms);
    p[22] = m->proximity_state; p[23] = m->proximity_validity; p[24] = m->proximity_reason;
    nw_write_u32(p + 25, m->proximity_update_sequence); nw_write_u16(p + 29, m->proximity_age_ms);
    p[31] = m->registration_connection_quality;
    nw_write_u32(p + 32, m->registration_connection_update_sequence);
    nw_write_u16(p + 36, m->registration_connection_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_UPLINK,
        0x01U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_context_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMContext *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMContext m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x01U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p); m.session_id = nw_read_u64(p + 4);
    m.device_registration_state = p[12]; m.bt_connection_state = p[13];
    m.app_active_state = p[14]; m.app_quality = p[15];
    m.app_update_sequence = nw_read_u32(p + 16); m.app_age_ms = nw_read_u16(p + 20);
    m.proximity_state = p[22]; m.proximity_validity = p[23]; m.proximity_reason = p[24];
    m.proximity_update_sequence = nw_read_u32(p + 25); m.proximity_age_ms = nw_read_u16(p + 29);
    m.registration_connection_quality = p[31];
    m.registration_connection_update_sequence = nw_read_u32(p + 32);
    m.registration_connection_age_ms = nw_read_u16(p + 36);
    status = context_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

static NetWireStatus request_check(const NetWireMRequest *m)
{
    size_t used = 0U;
    size_t i;
    if (m->device_context_id == 0U) return NET_WIRE_MALFORMED;
    switch (m->kind) {
    case 1U:
        if (m->operation > 1U) return NET_WIRE_MALFORMED;
        break;
    case 2U:
        if (m->operation == 1U) used = 2U; /* Entire signed i16 raw domain. */
        else if (m->operation == 2U && m->args[0] <= 1U) used = 1U;
        else if (m->operation == 3U && m->args[0] <= 3U) used = 1U;
        else return NET_WIRE_MALFORMED;
        break;
    case 3U:
        if (m->operation == 1U && m->args[0] <= 1U) used = 1U;
        else if (m->operation == 2U && m->args[0] <= 100U) used = 1U;
        else if (m->operation == 3U) used = 3U; /* 255 is a valid RGB component. */
        else return NET_WIRE_MALFORMED;
        break;
    case 4U:
        if (m->operation != 1U || m->args[0] > 1U) return NET_WIRE_MALFORMED;
        used = 1U;
        break;
    default: return NET_WIRE_MALFORMED;
    }
    for (i = used; i < sizeof(m->args); ++i)
        if (m->args[i] != 0U) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_request_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMRequest *m, NetWireUartFrame *out)
{
    uint8_t p[24] = {0};
    NetWireStatus status = encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = request_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->device_context_id); nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->request_id); p[16] = m->kind; p[17] = m->operation;
    memcpy(p + 18, m->args, sizeof(m->args));
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_UPLINK,
        0x10U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_request_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMRequest *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMRequest m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x10U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p); m.session_id = nw_read_u64(p + 4);
    m.request_id = nw_read_u32(p + 12); m.kind = p[16]; m.operation = p[17];
    memcpy(m.args, p + 18, sizeof(m.args));
    status = request_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_request_temperature(const NetWireMRequest *m, int16_t *out)
{
    uint16_t raw;
    NetWireStatus status;
    if (m == NULL || out == NULL || net_wire_ranges_overlap(m, sizeof(*m), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = request_check(m);
    if (status != NET_WIRE_OK) return status;
    if (m->kind != 2U || m->operation != 1U) return NET_WIRE_UNSUPPORTED;
    raw = nw_read_u16(m->args);
    *out = (int16_t)(raw <= INT16_MAX ? (int32_t)raw : (int32_t)raw - INT32_C(65536));
    return NET_WIRE_OK;
}

static NetWireStatus result_check(const NetWireMResult *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U ||
        (m->result > 5U && m->result != UINT8_MAX) || m->result_reason > 25U ||
        m->result_confirmation > 1U || m->command_target > 2U || m->origin > 5U ||
        (m->result == UINT8_MAX && m->result_confirmation != 0U))
        return NET_WIRE_MALFORMED;
    if (m->command_target == 0U && m->command_id != 0U)
        return NET_WIRE_CONTRACT_HOLD;
    /* Raw WINDOW/ORIGIN values are representable, not a C2 mapping approval. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_result_encode(NetWireProfile profile,
    uint16_t sequence, const NetWireMResult *m, NetWireUartFrame *out)
{
    uint8_t p[44] = {0};
    NetWireStatus status = encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = result_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->device_context_id); nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id); nw_write_u32(p + 16, m->query_id);
    nw_write_u64(p + 20, m->request_session); nw_write_u32(p + 28, m->request_id);
    p[32] = m->result; nw_write_u16(p + 33, m->result_reason);
    p[35] = m->result_confirmation; p[36] = m->command_target;
    nw_write_u32(p + 37, m->command_id); p[41] = m->origin;
    nw_write_u16(p + 42, m->result_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x20U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMResult *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMResult m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x20U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p); m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12); m.query_id = nw_read_u32(p + 16);
    m.request_session = nw_read_u64(p + 20); m.request_id = nw_read_u32(p + 28);
    m.result = p[32]; m.result_reason = nw_read_u16(p + 33);
    m.result_confirmation = p[35]; m.command_target = p[36];
    m.command_id = nw_read_u32(p + 37); m.origin = p[41];
    m.result_age_ms = nw_read_u16(p + 42);
    status = result_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m; *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}
