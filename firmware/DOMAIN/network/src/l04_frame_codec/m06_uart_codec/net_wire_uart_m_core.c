#include "network/net_wire_uart_m_core.h"
#include "net_wire_uart_typed_internal.h"

/* Explicit frozen raw constraints only. No C2 casting, clock reads, sequence
 * allocation, owner binding, producer policy or vehicle execution is here. */
static bool tristate(uint8_t value)
{
    return value <= 1U || value == UINT8_MAX;
}

static NetWireStatus query_check(const NetWireMQueryRaw *m)
{
    if (m->device_context_id == 0U || m->query_id == 0U ||
        m->query_scope > 3U || m->reserved != 0U || m->cursor != 0U)
        return NET_WIRE_MALFORMED;
    if (m->query_scope == 0U && (m->request_session != 0U || m->request_id != 0U))
        return NET_WIRE_MALFORMED;
    /* Scope1 supports a raw recent-query pair 0/0 or an explicit pair.
     * Mixed absence or request filtering of warning/all scopes needs owner
     * confirmation. Session issuance/zero admissibility itself stays OPEN. */
    if ((m->query_scope == 1U && ((m->request_session == 0U) != (m->request_id == 0U))) ||
        (m->query_scope >= 2U && (m->request_session != 0U || m->request_id != 0U)))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

static NetWireStatus warning_ack_check(const NetWireMWarningAckRaw *m)
{
    if (m->device_context_id == 0U || m->warning_type < 1U || m->warning_type > 8U)
        return NET_WIRE_MALFORMED;
    /* A missing referenced boot has no defined absence representation. */
    if (m->warning_domain_boot == 0U) return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

static NetWireStatus domain_alive_check(const NetWireMDomainAliveRaw *m)
{
    if (m->domain_boot_id == 0U || m->domain_state > 3U || m->value_quality > 3U)
        return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

static NetWireStatus warning_check(const NetWireMWarningRaw *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U ||
        m->warning_type < 1U || m->warning_type > 8U || m->severity > 2U ||
        m->warning_state > 1U || m->read_state > 1U || m->value_quality > 3U ||
        m->quality_reason > 9U)
        return NET_WIRE_MALFORMED;
    /* Unknown referenced source identity and contradictory quality require
     * owner evidence; do not replace source boot/sequence/age with Domain's. */
    if (m->source_boot == 0U || (m->value_quality == 0U && m->quality_reason != 0U))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}

static NetWireStatus availability_check(const NetWireMAvailabilityRaw *m)
{
    size_t i;
    if (m->device_context_id == 0U || m->domain_boot_id == 0U)
        return NET_WIRE_MALFORMED;
    for (i = 0U; i < 9U; ++i) {
        if (m->items[i].function_id != i + 1U || m->items[i].availability > 2U ||
            m->items[i].reason > 25U) return NET_WIRE_MALFORMED;
    }
    /* Reasons are preserved, not inferred from availability or C2 indices. */
    return NET_WIRE_OK;
}

static NetWireStatus digital_status_check(const NetWireMDigitalStatusRaw *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U ||
        m->digital_key_setting_state > 1U || m->digital_key_availability > 2U ||
        m->reason > 25U) return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

static NetWireStatus digital_result_check(const NetWireMDigitalResultRaw *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U || m->origin != 2U ||
        (m->result > 5U && m->result != UINT8_MAX) || m->result_reason > 25U ||
        m->result_confirmation > 1U ||
        (m->result == UINT8_MAX && m->result_confirmation != 0U))
        return NET_WIRE_MALFORMED;
    /* DONE/ALREADY_AT_TARGET never synthesizes an actual unlock transition. */
    return NET_WIRE_OK;
}

static NetWireStatus user_settings_check(const NetWireMUserSettingsRaw *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U ||
        !tristate(m->climate_auto) || !tristate(m->interior_light_enabled) ||
        (m->user_fan_level > 3U && m->user_fan_level != UINT8_MAX) ||
        m->normal_light_level_percent > 100U || m->value_quality > 3U)
        return NET_WIRE_MALFORMED;
    if (m->value_quality == 0U && (m->target_temperature_centi_c == INT16_MIN ||
        m->climate_auto == UINT8_MAX || m->user_fan_level == UINT8_MAX ||
        m->interior_light_enabled == UINT8_MAX)) return NET_WIRE_CONTRACT_HOLD;
    /* Full signed raw domain, including UNKNOWN -32768, is preserved. No
     * functional setpoint range or missing NORMAL composite is invented. */
    return NET_WIRE_OK;
}

static NetWireStatus query_end_check(const NetWireMQueryEndRaw *m)
{
    if (m->device_context_id == 0U || m->domain_boot_id == 0U || m->query_id == 0U ||
        m->query_scope > 3U || m->query_status > 3U) return NET_WIRE_MALFORMED;
    /* All u16 counts are raw caller values. G06 completion/TX evidence HOLD. */
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_query_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMQueryRaw *m, NetWireUartFrame *out)
{
    uint8_t p[32] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = query_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->query_id);
    p[16] = m->query_scope;
    p[17] = m->reserved;
    nw_write_u64(p + 18, m->request_session);
    nw_write_u32(p + 26, m->request_id);
    nw_write_u16(p + 30, m->cursor);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_UPLINK,
        0x11U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_query_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMQueryRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMQueryRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x11U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.query_id = nw_read_u32(p + 12);
    m.query_scope = p[16];
    m.reserved = p[17];
    m.request_session = nw_read_u64(p + 18);
    m.request_id = nw_read_u32(p + 26);
    m.cursor = nw_read_u16(p + 30);
    status = query_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_warning_ack_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMWarningAckRaw *m, NetWireUartFrame *out)
{
    uint8_t p[21] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = warning_ack_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    p[12] = m->warning_type;
    nw_write_u32(p + 13, m->occurrence_id);
    nw_write_u32(p + 17, m->warning_domain_boot);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_UPLINK,
        0x12U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_warning_ack_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWarningAckRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMWarningAckRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_UPLINK, 0x12U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.warning_type = p[12];
    m.occurrence_id = nw_read_u32(p + 13);
    m.warning_domain_boot = nw_read_u32(p + 17);
    status = warning_ack_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_domain_alive_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDomainAliveRaw *m, NetWireUartFrame *out)
{
    uint8_t p[12] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = domain_alive_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->domain_boot_id);
    nw_write_u32(p + 4, m->update_sequence);
    nw_write_u16(p + 8, m->source_age_ms);
    p[10] = m->domain_state;
    p[11] = m->value_quality;
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x02U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_domain_alive_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDomainAliveRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMDomainAliveRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x02U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.domain_boot_id = nw_read_u32(p + 0);
    m.update_sequence = nw_read_u32(p + 4);
    m.source_age_ms = nw_read_u16(p + 8);
    m.domain_state = p[10];
    m.value_quality = p[11];
    status = domain_alive_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_warning_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMWarningRaw *m, NetWireUartFrame *out)
{
    uint8_t p[40] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = warning_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    p[20] = m->warning_type;
    nw_write_u32(p + 21, m->occurrence_id);
    p[25] = m->severity;
    p[26] = m->warning_state;
    p[27] = m->read_state;
    p[28] = m->value_quality;
    p[29] = m->quality_reason;
    nw_write_u32(p + 30, m->source_boot);
    nw_write_u32(p + 34, m->source_sequence);
    nw_write_u16(p + 38, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x21U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_warning_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMWarningRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMWarningRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x21U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.warning_type = p[20];
    m.occurrence_id = nw_read_u32(p + 21);
    m.severity = p[25];
    m.warning_state = p[26];
    m.read_state = p[27];
    m.value_quality = p[28];
    m.quality_reason = p[29];
    m.source_boot = nw_read_u32(p + 30);
    m.source_sequence = nw_read_u32(p + 34);
    m.source_age_ms = nw_read_u16(p + 38);
    status = warning_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_availability_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMAvailabilityRaw *m, NetWireUartFrame *out)
{
    uint8_t p[62] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = availability_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    p[20] = m->items[0].function_id;
    p[21] = m->items[0].availability;
    nw_write_u16(p + 22, m->items[0].reason);
    p[24] = m->items[1].function_id;
    p[25] = m->items[1].availability;
    nw_write_u16(p + 26, m->items[1].reason);
    p[28] = m->items[2].function_id;
    p[29] = m->items[2].availability;
    nw_write_u16(p + 30, m->items[2].reason);
    p[32] = m->items[3].function_id;
    p[33] = m->items[3].availability;
    nw_write_u16(p + 34, m->items[3].reason);
    p[36] = m->items[4].function_id;
    p[37] = m->items[4].availability;
    nw_write_u16(p + 38, m->items[4].reason);
    p[40] = m->items[5].function_id;
    p[41] = m->items[5].availability;
    nw_write_u16(p + 42, m->items[5].reason);
    p[44] = m->items[6].function_id;
    p[45] = m->items[6].availability;
    nw_write_u16(p + 46, m->items[6].reason);
    p[48] = m->items[7].function_id;
    p[49] = m->items[7].availability;
    nw_write_u16(p + 50, m->items[7].reason);
    p[52] = m->items[8].function_id;
    p[53] = m->items[8].availability;
    nw_write_u16(p + 54, m->items[8].reason);
    nw_write_u32(p + 56, m->update_sequence);
    nw_write_u16(p + 60, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x22U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_availability_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMAvailabilityRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMAvailabilityRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x22U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.items[0].function_id = p[20];
    m.items[0].availability = p[21];
    m.items[0].reason = nw_read_u16(p + 22);
    m.items[1].function_id = p[24];
    m.items[1].availability = p[25];
    m.items[1].reason = nw_read_u16(p + 26);
    m.items[2].function_id = p[28];
    m.items[2].availability = p[29];
    m.items[2].reason = nw_read_u16(p + 30);
    m.items[3].function_id = p[32];
    m.items[3].availability = p[33];
    m.items[3].reason = nw_read_u16(p + 34);
    m.items[4].function_id = p[36];
    m.items[4].availability = p[37];
    m.items[4].reason = nw_read_u16(p + 38);
    m.items[5].function_id = p[40];
    m.items[5].availability = p[41];
    m.items[5].reason = nw_read_u16(p + 42);
    m.items[6].function_id = p[44];
    m.items[6].availability = p[45];
    m.items[6].reason = nw_read_u16(p + 46);
    m.items[7].function_id = p[48];
    m.items[7].availability = p[49];
    m.items[7].reason = nw_read_u16(p + 50);
    m.items[8].function_id = p[52];
    m.items[8].availability = p[53];
    m.items[8].reason = nw_read_u16(p + 54);
    m.update_sequence = nw_read_u32(p + 56);
    m.source_age_ms = nw_read_u16(p + 60);
    status = availability_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_digital_status_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDigitalStatusRaw *m, NetWireUartFrame *out)
{
    uint8_t p[30] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = digital_status_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    p[20] = m->digital_key_setting_state;
    p[21] = m->digital_key_availability;
    nw_write_u16(p + 22, m->reason);
    nw_write_u32(p + 24, m->update_sequence);
    nw_write_u16(p + 28, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x23U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_digital_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDigitalStatusRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMDigitalStatusRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x23U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.digital_key_setting_state = p[20];
    m.digital_key_availability = p[21];
    m.reason = nw_read_u16(p + 22);
    m.update_sequence = nw_read_u32(p + 24);
    m.source_age_ms = nw_read_u16(p + 28);
    status = digital_status_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_digital_result_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMDigitalResultRaw *m, NetWireUartFrame *out)
{
    uint8_t p[35] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = digital_result_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    nw_write_u32(p + 20, m->auto_occurrence_id);
    nw_write_u32(p + 24, m->related_door_command_id);
    p[28] = m->origin;
    p[29] = m->result;
    nw_write_u16(p + 30, m->result_reason);
    p[32] = m->result_confirmation;
    nw_write_u16(p + 33, m->result_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x24U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_digital_result_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMDigitalResultRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMDigitalResultRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x24U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.auto_occurrence_id = nw_read_u32(p + 20);
    m.related_door_command_id = nw_read_u32(p + 24);
    m.origin = p[28];
    m.result = p[29];
    m.result_reason = nw_read_u16(p + 30);
    m.result_confirmation = p[32];
    m.result_age_ms = nw_read_u16(p + 33);
    status = digital_result_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_user_settings_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMUserSettingsRaw *m, NetWireUartFrame *out)
{
    uint8_t p[36] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = user_settings_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    nw_write_i16(p + 20, m->target_temperature_centi_c);
    p[22] = m->climate_auto;
    p[23] = m->user_fan_level;
    p[24] = m->interior_light_enabled;
    p[25] = m->normal_light_level_percent;
    p[26] = m->normal_light_rgb[0];
    p[27] = m->normal_light_rgb[1];
    p[28] = m->normal_light_rgb[2];
    p[29] = m->value_quality;
    nw_write_u32(p + 30, m->update_sequence);
    nw_write_u16(p + 34, m->source_age_ms);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x25U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_user_settings_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMUserSettingsRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMUserSettingsRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x25U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.target_temperature_centi_c = nw_read_i16(p + 20);
    m.climate_auto = p[22];
    m.user_fan_level = p[23];
    m.interior_light_enabled = p[24];
    m.normal_light_level_percent = p[25];
    m.normal_light_rgb[0] = p[26];
    m.normal_light_rgb[1] = p[27];
    m.normal_light_rgb[2] = p[28];
    m.value_quality = p[29];
    m.update_sequence = nw_read_u32(p + 30);
    m.source_age_ms = nw_read_u16(p + 34);
    status = user_settings_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_m_query_end_encode(NetWireProfile profile, uint16_t sequence,
    const NetWireMQueryEndRaw *m, NetWireUartFrame *out)
{
    uint8_t p[24] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = query_end_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p + 0, m->device_context_id);
    nw_write_u64(p + 4, m->session_id);
    nw_write_u32(p + 12, m->domain_boot_id);
    nw_write_u32(p + 16, m->query_id);
    p[20] = m->query_scope;
    p[21] = m->query_status;
    nw_write_u16(p + 22, m->item_count);
    return net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        0x40U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_m_query_end_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireMQueryEndRaw *out, uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWireMQueryEndRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out, sizeof(*out),
        sequence_out, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, 0x40U, &payload);
    if (status != NET_WIRE_OK) return status;
    m.device_context_id = nw_read_u32(p + 0);
    m.session_id = nw_read_u64(p + 4);
    m.domain_boot_id = nw_read_u32(p + 12);
    m.query_id = nw_read_u32(p + 16);
    m.query_scope = p[20];
    m.query_status = p[21];
    m.item_count = nw_read_u16(p + 22);
    status = query_end_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}
