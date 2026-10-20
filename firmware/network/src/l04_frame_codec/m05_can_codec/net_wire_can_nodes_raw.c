#include "network/net_wire_can_nodes.h"
#include "net_wire_can_internal.h"

/* Raw enum membership is separate from product policy and C2 projections. */
static bool tri(uint8_t v) { return v <= 1u || v == UINT8_MAX; }
static NetWireStatus sensor_quality(uint8_t valid, uint8_t reason, bool sentinel)
{
    if (valid > 1u || reason > 9u || (valid == 1u && sentinel))
        return NET_WIRE_MALFORMED;
    if ((valid == 1u && reason != 0u) || (valid == 0u && reason == 0u))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}
static NetWireStatus valid_window_command(const NetWireWindowCommandRaw *p)
{
    if (p->boot_id == 0u ||
        p->window_command < 1u || p->window_command > 5u ||
        p->target_channel != 0u || p->target_position > 100u ||
        p->source_context > 4u) return NET_WIRE_MALFORMED;
    if (p->target_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    /* VENT 70/80 and ACCEPT_LIMIT 200/1000 are unresolved policies.
     * Preserve explicit raw values; this API cannot execute any command. */
    return NET_WIRE_OK;
}
static NetWireStatus valid_window_state(const NetWireWindowStateRaw *p)
{
    if (p->boot_id == 0u || (p->motion_state > 3u && p->motion_state != UINT8_MAX) ||
        p->window_ecu_state > 3u || (p->window_position > 100u && p->window_position != UINT8_MAX) ||
        p->window_value_quality > 3u || !tri(p->fully_open_state) || !tri(p->fully_closed_state) ||
        p->antipinch_protection_status > 3u || !tri(p->reverse_state)) return NET_WIRE_MALFORMED;
    if (p->window_value_quality == 0u &&
        (p->window_position == UINT8_MAX || p->motion_state == UINT8_MAX ||
         p->fully_open_state == UINT8_MAX || p->fully_closed_state == UINT8_MAX ||
         p->reverse_state == UINT8_MAX ||
         (p->fully_open_state == 1u && p->window_position != 0u) ||
         (p->fully_closed_state == 1u && p->window_position != 100u)))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}
static NetWireStatus valid_window_event(const NetWireWindowEventRaw *p)
{
    if (p->boot_id == 0u || p->event_kind < 1u || p->event_kind > 3u ||
        p->target_channel != 0u || p->origin > 5u || p->event_quality > 3u)
        return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}
static NetWireStatus valid_window_result(const NetWireWindowResultRaw *p)
{
    if (p->boot_id == 0u || p->target_channel != 0u ||
        (p->result > 5u && p->result != UINT8_MAX) || p->result_reason > 25u) return NET_WIRE_MALFORMED;
    if (p->request_domain_boot == 0u || p->result == UINT8_MAX) return NET_WIRE_CONTRACT_HOLD;
    /* request_sequence is not reissued or matched to a C2 command here. */
    return NET_WIRE_OK;
}
static NetWireStatus valid_window_fault(const NetWireWindowFaultRaw *p)
{
    if (p->boot_id == 0u || p->fault_category > 3u || p->fault_status > 2u ||
        (p->fault_code > 6u && p->fault_code != 255u)) return NET_WIRE_MALFORMED;
    /* Current clear and a retained occurrence can coexist. */
    return NET_WIRE_OK;
}
static NetWireStatus valid_cis_environment(const NetWireCisEnvironmentRaw *p)
{
    NetWireStatus s;
    if (p->boot_id == 0u ||
        ((p->temperature_centi_c < -4000 || p->temperature_centi_c > 12500) &&
         p->temperature_centi_c != INT16_MIN) ||
        (p->humidity_centi_percent > 10000u && p->humidity_centi_percent != UINT16_MAX) ||
        (p->illuminance_lux > UINT32_C(200000) && p->illuminance_lux != UINT32_MAX))
        return NET_WIRE_MALFORMED;
    s = sensor_quality(p->temperature_validity, p->temperature_reason,
        p->temperature_centi_c == INT16_MIN);
    if (s != NET_WIRE_OK) return s;
    s = sensor_quality(p->humidity_validity, p->humidity_reason,
        p->humidity_centi_percent == UINT16_MAX);
    if (s != NET_WIRE_OK) return s;
    return sensor_quality(p->illuminance_validity, p->illuminance_reason,
        p->illuminance_lux == UINT32_MAX);
}
static NetWireStatus valid_cis_occupant(const NetWireCisOccupantRaw *p)
{
    NetWireStatus s;
    if (p->boot_id == 0u || !tri(p->occupant_presence) ||
        (p->occupant_count > 5u && p->occupant_count != UINT8_MAX) ||
        p->vision_function_status > 4u) return NET_WIRE_MALFORMED;
    s = sensor_quality(p->presence_validity, p->presence_reason, p->occupant_presence == UINT8_MAX);
    if (s != NET_WIRE_OK) return s;
    s = sensor_quality(p->count_validity, p->count_reason, p->occupant_count == UINT8_MAX);
    if (s != NET_WIRE_OK) return s;
    if (p->presence_validity == 1u && p->count_validity == 1u &&
        ((p->occupant_presence == 0u && p->occupant_count != 0u) ||
         (p->occupant_presence == 1u && p->occupant_count == 0u))) return NET_WIRE_MALFORMED;
    /* No sourceBoot0 production/absence contract is available for this DTO. */
    if (p->pi_boot_id == 0u) return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}
static NetWireStatus valid_cis_status(const NetWireCisStatusRaw *p)
{
    if (p->boot_id == 0u || p->cis_state > 3u || p->vision_status > 4u ||
        p->temperature_status > 4u || p->humidity_status > 4u || p->illuminance_status > 4u ||
        p->rear_status > 4u || p->pi_uart_status > 2u || p->can_interface_status > 2u ||
        (p->fault_mask & UINT16_C(0xfe00)) != 0u || p->active_fault > 9u ||
        p->recovering > 1u || p->last_fault > 9u || (p->affected_function & 0xc0u) != 0u)
        return NET_WIRE_MALFORMED;
    /* Major fault priority and health synthesis remain owner contracts. */
    return NET_WIRE_OK;
}
static NetWireStatus valid_vss_event(const NetWireVssEventRaw *p)
{
    if (p->boot_id == 0u || p->event_kind < 1u || p->event_kind > 5u ||
        p->value_quality > 3u || p->input_availability > 2u) return NET_WIRE_MALFORMED;
    /* USE_LIMIT0 is a representable unusable event, not a playback grant. */
    return NET_WIRE_OK;
}
static NetWireStatus warning_lane(uint32_t boot, uint32_t sequence, uint16_t age,
    uint8_t state, uint8_t quality, uint8_t reason, uint8_t availability, uint8_t maximum)
{
    if (state > maximum || quality > 3u || reason > 9u || availability > 2u)
        return NET_WIRE_MALFORMED;
    if (boot == 0u) {
        /* Canonical A1 absent/unassessed tuple; this is never CLEAR+OK. */
        if (sequence == 0u && age == UINT16_MAX && state == 0u &&
            quality == 3u && reason == 6u && availability == 2u) return NET_WIRE_OK;
        return NET_WIRE_CONTRACT_HOLD;
    }
    if ((quality == 0u && reason != 0u) || (quality != 0u && reason == 0u))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}
static NetWireStatus valid_vss_warning_state(const NetWireVssWarningStateRaw *p)
{
    NetWireStatus s;
    if (p->boot_id == 0u || p->rear_detection_activation > 1u || p->activation_meta_format > 1u)
        return NET_WIRE_MALFORMED;
    s = warning_lane(p->antipinch_source_boot, p->antipinch_source_sequence,
        p->antipinch_original_age_ms, p->antipinch_state, p->antipinch_quality,
        p->antipinch_reason, p->antipinch_availability, 1u);
    if (s != NET_WIRE_OK) return s;
    s = warning_lane(p->occupant_hazard_source_boot, p->occupant_hazard_source_sequence,
        p->occupant_hazard_original_age_ms, p->occupant_hazard_state, p->occupant_hazard_quality,
        p->occupant_hazard_reason, p->occupant_hazard_availability, 1u);
    if (s != NET_WIRE_OK) return s;
    s = warning_lane(p->rear_obstacle_source_boot, p->rear_obstacle_source_sequence,
        p->rear_obstacle_original_age_ms, p->rear_obstacle_state, p->rear_obstacle_quality,
        p->rear_obstacle_reason, p->rear_obstacle_availability, 2u);
    if (s != NET_WIRE_OK) return s;
    if (p->activation_meta_format == 0u) {
        if (p->rear_detection_activation != 0u || p->rear_activation_quality != 0u ||
            p->rear_activation_reason != 0u || p->rear_activation_sequence != 0u ||
            p->rear_activation_age_ms != 0u) return NET_WIRE_MALFORMED;
        return NET_WIRE_OK;
    }
    if (p->rear_activation_quality > 3u || p->rear_activation_reason > 9u)
        return NET_WIRE_MALFORMED;
    if ((p->rear_activation_quality == 0u && p->rear_activation_reason != 0u) ||
        (p->rear_activation_quality != 0u && p->rear_activation_reason == 0u))
        return NET_WIRE_CONTRACT_HOLD;
    return NET_WIRE_OK;
}
static NetWireStatus valid_vss_status(const NetWireVssStatusRaw *p)
{
    if (p->boot_id == 0u || p->vss_state > 3u || p->vss_availability > 2u ||
        p->vss_accepting_events > 1u || p->vss_fault_active > 1u ||
        (p->fault_mask & 0xe0u) != 0u || p->vss_last_fault > 5u ||
        (p->last_fault_quality != 0u && p->last_fault_quality != 3u)) return NET_WIRE_MALFORMED;
    if (p->last_fault_quality == 3u && (p->vss_last_fault != 0u ||
        p->last_fault_occurrence != 0u || p->last_fault_age_ms != 0u)) return NET_WIRE_MALFORMED;
    if (p->last_fault_quality == 0u && p->vss_last_fault == 0u) return NET_WIRE_CONTRACT_HOLD;
    /* History0/3 is not State/currentFault quality. No same-K A2 commit here. */
    return NET_WIRE_OK;
}

static void write_window_command(uint8_t *b, const NetWireWindowCommandRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    nw_write_u32(b + 8u, p->target_boot_id);
    nw_write_u32(b + 12u, p->command_id);
    nw_write_u16(b + 16u, p->command_age_ms);
    nw_write_u16(b + 18u, p->accept_limit_ms);
    b[20] = p->window_command;
    b[21] = p->target_channel;
    b[22] = p->target_position;
    b[23] = p->source_context;
}
static void read_window_command(const uint8_t *b, NetWireWindowCommandRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->target_boot_id = nw_read_u32(b + 8u);
    p->command_id = nw_read_u32(b + 12u);
    p->command_age_ms = nw_read_u16(b + 16u);
    p->accept_limit_ms = nw_read_u16(b + 18u);
    p->window_command = b[20];
    p->target_channel = b[21];
    p->target_position = b[22];
    p->source_context = b[23];
}

static void write_window_state(uint8_t *b, const NetWireWindowStateRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->motion_state;
    b[9] = p->window_ecu_state;
    b[10] = p->window_position;
    b[11] = p->window_value_quality;
    b[12] = p->fully_open_state;
    b[13] = p->fully_closed_state;
    b[14] = p->antipinch_protection_status;
    b[15] = p->reverse_state;
    nw_write_u32(b + 16u, p->update_sequence);
    nw_write_u16(b + 20u, p->source_age_ms);
}
static void read_window_state(const uint8_t *b, NetWireWindowStateRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->motion_state = b[8];
    p->window_ecu_state = b[9];
    p->window_position = b[10];
    p->window_value_quality = b[11];
    p->fully_open_state = b[12];
    p->fully_closed_state = b[13];
    p->antipinch_protection_status = b[14];
    p->reverse_state = b[15];
    p->update_sequence = nw_read_u32(b + 16u);
    p->source_age_ms = nw_read_u16(b + 20u);
}

static void write_window_event(uint8_t *b, const NetWireWindowEventRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->event_kind;
    b[9] = p->target_channel;
    nw_write_u32(b + 10u, p->occurrence_id);
    nw_write_u32(b + 14u, p->related_domain_boot);
    nw_write_u32(b + 18u, p->related_request_sequence);
    b[22] = p->origin;
    nw_write_u16(b + 23u, p->event_age_ms);
    b[25] = p->event_quality;
}
static void read_window_event(const uint8_t *b, NetWireWindowEventRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->event_kind = b[8];
    p->target_channel = b[9];
    p->occurrence_id = nw_read_u32(b + 10u);
    p->related_domain_boot = nw_read_u32(b + 14u);
    p->related_request_sequence = nw_read_u32(b + 18u);
    p->origin = b[22];
    p->event_age_ms = nw_read_u16(b + 23u);
    p->event_quality = b[25];
}

static void write_window_result(uint8_t *b, const NetWireWindowResultRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    nw_write_u32(b + 8u, p->request_domain_boot);
    nw_write_u32(b + 12u, p->request_sequence);
    b[16] = p->target_channel;
    b[17] = p->result;
    nw_write_u16(b + 18u, p->result_reason);
}
static void read_window_result(const uint8_t *b, NetWireWindowResultRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->request_domain_boot = nw_read_u32(b + 8u);
    p->request_sequence = nw_read_u32(b + 12u);
    p->target_channel = b[16];
    p->result = b[17];
    p->result_reason = nw_read_u16(b + 18u);
}

static void write_window_fault(uint8_t *b, const NetWireWindowFaultRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->fault_category;
    nw_write_u16(b + 9u, p->fault_code);
    b[11] = p->fault_status;
    nw_write_u32(b + 12u, p->occurrence_id);
    nw_write_u32(b + 16u, p->update_sequence);
    nw_write_u16(b + 20u, p->source_age_ms);
}
static void read_window_fault(const uint8_t *b, NetWireWindowFaultRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->fault_category = b[8];
    p->fault_code = nw_read_u16(b + 9u);
    p->fault_status = b[11];
    p->occurrence_id = nw_read_u32(b + 12u);
    p->update_sequence = nw_read_u32(b + 16u);
    p->source_age_ms = nw_read_u16(b + 20u);
}

static void write_cis_environment(uint8_t *b, const NetWireCisEnvironmentRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    nw_write_i16(b + 8u, p->temperature_centi_c);
    nw_write_u32(b + 10u, p->temperature_sequence);
    nw_write_u16(b + 14u, p->temperature_age_ms);
    b[16] = p->temperature_validity;
    b[17] = p->temperature_reason;
    nw_write_u16(b + 18u, p->humidity_centi_percent);
    nw_write_u32(b + 20u, p->humidity_sequence);
    nw_write_u16(b + 24u, p->humidity_age_ms);
    b[26] = p->humidity_validity;
    b[27] = p->humidity_reason;
    nw_write_u32(b + 28u, p->illuminance_lux);
    nw_write_u32(b + 32u, p->illuminance_sequence);
    nw_write_u16(b + 36u, p->illuminance_age_ms);
    b[38] = p->illuminance_validity;
    b[39] = p->illuminance_reason;
}
static void read_cis_environment(const uint8_t *b, NetWireCisEnvironmentRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->temperature_centi_c = nw_read_i16(b + 8u);
    p->temperature_sequence = nw_read_u32(b + 10u);
    p->temperature_age_ms = nw_read_u16(b + 14u);
    p->temperature_validity = b[16];
    p->temperature_reason = b[17];
    p->humidity_centi_percent = nw_read_u16(b + 18u);
    p->humidity_sequence = nw_read_u32(b + 20u);
    p->humidity_age_ms = nw_read_u16(b + 24u);
    p->humidity_validity = b[26];
    p->humidity_reason = b[27];
    p->illuminance_lux = nw_read_u32(b + 28u);
    p->illuminance_sequence = nw_read_u32(b + 32u);
    p->illuminance_age_ms = nw_read_u16(b + 36u);
    p->illuminance_validity = b[38];
    p->illuminance_reason = b[39];
}

static void write_cis_occupant(uint8_t *b, const NetWireCisOccupantRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    nw_write_u32(b + 8u, p->pi_boot_id);
    nw_write_u32(b + 12u, p->vision_sequence);
    nw_write_u16(b + 16u, p->vision_age_ms);
    b[18] = p->occupant_presence;
    b[19] = p->presence_validity;
    b[20] = p->presence_reason;
    b[21] = p->occupant_count;
    b[22] = p->count_validity;
    b[23] = p->count_reason;
    b[24] = p->vision_function_status;
}
static void read_cis_occupant(const uint8_t *b, NetWireCisOccupantRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->pi_boot_id = nw_read_u32(b + 8u);
    p->vision_sequence = nw_read_u32(b + 12u);
    p->vision_age_ms = nw_read_u16(b + 16u);
    p->occupant_presence = b[18];
    p->presence_validity = b[19];
    p->presence_reason = b[20];
    p->occupant_count = b[21];
    p->count_validity = b[22];
    p->count_reason = b[23];
    p->vision_function_status = b[24];
}

static void write_cis_status(uint8_t *b, const NetWireCisStatusRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->cis_state;
    b[9] = p->vision_status;
    b[10] = p->temperature_status;
    b[11] = p->humidity_status;
    b[12] = p->illuminance_status;
    b[13] = p->rear_status;
    b[14] = p->pi_uart_status;
    b[15] = p->can_interface_status;
    nw_write_u16(b + 16u, p->fault_mask);
    nw_write_u16(b + 18u, p->active_fault);
    b[20] = p->recovering;
    nw_write_u16(b + 21u, p->last_fault);
    nw_write_u32(b + 23u, p->last_fault_occurrence);
    b[27] = p->affected_function;
    nw_write_u32(b + 28u, p->update_sequence);
    nw_write_u16(b + 32u, p->source_age_ms);
}
static void read_cis_status(const uint8_t *b, NetWireCisStatusRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->cis_state = b[8];
    p->vision_status = b[9];
    p->temperature_status = b[10];
    p->humidity_status = b[11];
    p->illuminance_status = b[12];
    p->rear_status = b[13];
    p->pi_uart_status = b[14];
    p->can_interface_status = b[15];
    p->fault_mask = nw_read_u16(b + 16u);
    p->active_fault = nw_read_u16(b + 18u);
    p->recovering = b[20];
    p->last_fault = nw_read_u16(b + 21u);
    p->last_fault_occurrence = nw_read_u32(b + 23u);
    p->affected_function = b[27];
    p->update_sequence = nw_read_u32(b + 28u);
    p->source_age_ms = nw_read_u16(b + 32u);
}

static void write_vss_event(uint8_t *b, const NetWireVssEventRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->event_kind;
    nw_write_u32(b + 9u, p->occurrence_id);
    nw_write_u16(b + 13u, p->original_age_ms);
    nw_write_u16(b + 15u, p->use_limit_ms);
    b[17] = p->value_quality;
    b[18] = p->input_availability;
    nw_write_u32(b + 19u, p->related_command_id);
}
static void read_vss_event(const uint8_t *b, NetWireVssEventRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->event_kind = b[8];
    p->occurrence_id = nw_read_u32(b + 9u);
    p->original_age_ms = nw_read_u16(b + 13u);
    p->use_limit_ms = nw_read_u16(b + 15u);
    p->value_quality = b[17];
    p->input_availability = b[18];
    p->related_command_id = nw_read_u32(b + 19u);
}

static void write_vss_warning_state(uint8_t *b, const NetWireVssWarningStateRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    nw_write_u32(b + 8u, p->antipinch_source_boot);
    nw_write_u32(b + 12u, p->antipinch_source_sequence);
    nw_write_u16(b + 16u, p->antipinch_original_age_ms);
    b[18] = p->antipinch_state;
    b[19] = p->antipinch_quality;
    b[20] = p->antipinch_reason;
    b[21] = p->antipinch_availability;
    nw_write_u32(b + 22u, p->occupant_hazard_source_boot);
    nw_write_u32(b + 26u, p->occupant_hazard_source_sequence);
    nw_write_u16(b + 30u, p->occupant_hazard_original_age_ms);
    b[32] = p->occupant_hazard_state;
    b[33] = p->occupant_hazard_quality;
    b[34] = p->occupant_hazard_reason;
    b[35] = p->occupant_hazard_availability;
    nw_write_u32(b + 36u, p->rear_obstacle_source_boot);
    nw_write_u32(b + 40u, p->rear_obstacle_source_sequence);
    nw_write_u16(b + 44u, p->rear_obstacle_original_age_ms);
    b[46] = p->rear_obstacle_state;
    b[47] = p->rear_obstacle_quality;
    b[48] = p->rear_obstacle_reason;
    b[49] = p->rear_obstacle_availability;
    b[50] = p->rear_detection_activation;
    b[51] = p->activation_meta_format;
    b[52] = p->rear_activation_quality;
    b[53] = p->rear_activation_reason;
    nw_write_u32(b + 54u, p->rear_activation_sequence);
    nw_write_u16(b + 58u, p->rear_activation_age_ms);
}
static void read_vss_warning_state(const uint8_t *b, NetWireVssWarningStateRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->antipinch_source_boot = nw_read_u32(b + 8u);
    p->antipinch_source_sequence = nw_read_u32(b + 12u);
    p->antipinch_original_age_ms = nw_read_u16(b + 16u);
    p->antipinch_state = b[18];
    p->antipinch_quality = b[19];
    p->antipinch_reason = b[20];
    p->antipinch_availability = b[21];
    p->occupant_hazard_source_boot = nw_read_u32(b + 22u);
    p->occupant_hazard_source_sequence = nw_read_u32(b + 26u);
    p->occupant_hazard_original_age_ms = nw_read_u16(b + 30u);
    p->occupant_hazard_state = b[32];
    p->occupant_hazard_quality = b[33];
    p->occupant_hazard_reason = b[34];
    p->occupant_hazard_availability = b[35];
    p->rear_obstacle_source_boot = nw_read_u32(b + 36u);
    p->rear_obstacle_source_sequence = nw_read_u32(b + 40u);
    p->rear_obstacle_original_age_ms = nw_read_u16(b + 44u);
    p->rear_obstacle_state = b[46];
    p->rear_obstacle_quality = b[47];
    p->rear_obstacle_reason = b[48];
    p->rear_obstacle_availability = b[49];
    p->rear_detection_activation = b[50];
    p->activation_meta_format = b[51];
    p->rear_activation_quality = b[52];
    p->rear_activation_reason = b[53];
    p->rear_activation_sequence = nw_read_u32(b + 54u);
    p->rear_activation_age_ms = nw_read_u16(b + 58u);
}

static void write_vss_status(uint8_t *b, const NetWireVssStatusRaw *p)
{
    nw_write_u32(b + 0u, p->boot_id);
    nw_write_u32(b + 4u, p->tx_sequence);
    b[8] = p->vss_state;
    b[9] = p->vss_availability;
    b[10] = p->vss_accepting_events;
    b[11] = p->vss_fault_active;
    b[12] = p->fault_mask;
    nw_write_u16(b + 13u, p->vss_last_fault);
    nw_write_u32(b + 15u, p->last_fault_occurrence);
    nw_write_u16(b + 19u, p->last_fault_age_ms);
    b[21] = p->last_fault_quality;
    nw_write_u32(b + 22u, p->update_sequence);
    nw_write_u16(b + 26u, p->source_age_ms);
}
static void read_vss_status(const uint8_t *b, NetWireVssStatusRaw *p)
{
    p->boot_id = nw_read_u32(b + 0u);
    p->tx_sequence = nw_read_u32(b + 4u);
    p->vss_state = b[8];
    p->vss_availability = b[9];
    p->vss_accepting_events = b[10];
    p->vss_fault_active = b[11];
    p->fault_mask = b[12];
    p->vss_last_fault = nw_read_u16(b + 13u);
    p->last_fault_occurrence = nw_read_u32(b + 15u);
    p->last_fault_age_ms = nw_read_u16(b + 19u);
    p->last_fault_quality = b[21];
    p->update_sequence = nw_read_u32(b + 22u);
    p->source_age_ms = nw_read_u16(b + 26u);
}

/* Share only mechanical validation and atomic publication. */
#define DEFINE_RAW_CODEC(NAME, TYPE, ID) \
NetWireStatus net_wire_can_encode_##NAME(NetWireProfile profile, const TYPE *input, NetWireCanEnvelope *output) \
{ \
    NetWireStatus s = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output)); \
    NetWireCanEnvelope local; \
    if (s != NET_WIRE_OK) return s; \
    s = valid_##NAME(input); \
    if (s != NET_WIRE_OK) return s; \
    local = nw_can_new_envelope(ID); \
    write_##NAME(local.bytes, input); \
    *output = local; \
    return NET_WIRE_OK; \
} \
NetWireStatus net_wire_can_decode_##NAME(NetWireProfile profile, const NetWireCanEnvelope *input, TYPE *output) \
{ \
    NetWireStatus s = nw_can_arguments(profile, input, sizeof(*input), output, sizeof(*output)); \
    TYPE local = {0}; \
    if (s != NET_WIRE_OK) return s; \
    s = nw_can_envelope_valid(input, ID); \
    if (s != NET_WIRE_OK) return s; \
    read_##NAME(input->bytes, &local); \
    s = valid_##NAME(&local); \
    if (s != NET_WIRE_OK) return s; \
    *output = local; \
    return NET_WIRE_OK; \
}

DEFINE_RAW_CODEC(window_command, NetWireWindowCommandRaw, 0x090u)
DEFINE_RAW_CODEC(window_state, NetWireWindowStateRaw, 0x0b0u)
DEFINE_RAW_CODEC(window_event, NetWireWindowEventRaw, 0x091u)
DEFINE_RAW_CODEC(window_result, NetWireWindowResultRaw, 0x092u)
DEFINE_RAW_CODEC(window_fault, NetWireWindowFaultRaw, 0x330u)
DEFINE_RAW_CODEC(cis_environment, NetWireCisEnvironmentRaw, 0x300u)
DEFINE_RAW_CODEC(cis_occupant, NetWireCisOccupantRaw, 0x310u)
DEFINE_RAW_CODEC(cis_status, NetWireCisStatusRaw, 0x320u)
DEFINE_RAW_CODEC(vss_event, NetWireVssEventRaw, 0x160u)
DEFINE_RAW_CODEC(vss_warning_state, NetWireVssWarningStateRaw, 0x0c0u)
DEFINE_RAW_CODEC(vss_status, NetWireVssStatusRaw, 0x340u)

#undef DEFINE_RAW_CODEC
