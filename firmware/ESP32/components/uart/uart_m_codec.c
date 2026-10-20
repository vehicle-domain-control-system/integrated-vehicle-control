#include "uart_m_codec.h"

#include <limits.h>
#include <string.h>

static const uart_m_message_descriptor_t s_descriptors[] = {
    {UART_M_TYPE_CONTEXT, UART_M_DIRECTION_ESP32_TX, 38U, false},
    {UART_M_TYPE_DOMAIN_ALIVE, UART_M_DIRECTION_ESP32_RX, 12U, false},
    {UART_M_TYPE_REQUEST, UART_M_DIRECTION_ESP32_TX, 24U, false},
    {UART_M_TYPE_QUERY, UART_M_DIRECTION_ESP32_TX, 32U, false},
    {UART_M_TYPE_WARNING_ACK, UART_M_DIRECTION_ESP32_TX, 21U, false},
    {UART_M_TYPE_RESULT, UART_M_DIRECTION_ESP32_RX, 44U, false},
    {UART_M_TYPE_WARNING, UART_M_DIRECTION_ESP32_RX, 40U, false},
    {UART_M_TYPE_AVAILABILITY, UART_M_DIRECTION_ESP32_RX, 62U, false},
    {UART_M_TYPE_DIGITAL_STATUS, UART_M_DIRECTION_ESP32_RX, 30U, false},
    {UART_M_TYPE_DIGITAL_RESULT, UART_M_DIRECTION_ESP32_RX, 35U, false},
    {UART_M_TYPE_USER_SETTINGS, UART_M_DIRECTION_ESP32_RX, 36U, false},
    {UART_M_TYPE_BCM_DOOR_STATE, UART_M_DIRECTION_ESP32_RX, 40U, false},
    {UART_M_TYPE_BCM_CLIMATE_STATE, UART_M_DIRECTION_ESP32_RX, 44U, false},
    {UART_M_TYPE_BCM_LIGHT_STATE, UART_M_DIRECTION_ESP32_RX, 45U, false},
    {UART_M_TYPE_BCM_STATUS, UART_M_DIRECTION_ESP32_RX, 42U, false},
    {UART_M_TYPE_CIS_ENVIRONMENT, UART_M_DIRECTION_ESP32_RX, 60U, false},
    {UART_M_TYPE_CIS_OCCUPANT, UART_M_DIRECTION_ESP32_RX, 45U, false},
    {UART_M_TYPE_CIS_REAR, UART_M_DIRECTION_ESP32_RX, 39U, false},
    {UART_M_TYPE_CIS_STATUS, UART_M_DIRECTION_ESP32_RX, 54U, false},
    {UART_M_TYPE_WINDOW_STATE, UART_M_DIRECTION_ESP32_RX, 42U, true},
    {UART_M_TYPE_WINDOW_FAULT, UART_M_DIRECTION_ESP32_RX, 42U, true},
    {UART_M_TYPE_VSS_STATUS, UART_M_DIRECTION_ESP32_RX, 48U, false},
    {UART_M_TYPE_QUERY_END, UART_M_DIRECTION_ESP32_RX, 24U, false},
};

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static int16_t read_i16_le(const uint8_t *p)
{
    return (int16_t)read_u16_le(p);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return (uint64_t)read_u32_le(p) | ((uint64_t)read_u32_le(&p[4]) << 32U);
}

static void write_u16_le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & UINT16_C(0x00FF));
    p[1] = (uint8_t)(value >> 8U);
}

static void write_i16_le(uint8_t *p, int16_t value)
{
    write_u16_le(p, (uint16_t)value);
}

static void write_u32_le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & UINT32_C(0x000000FF));
    p[1] = (uint8_t)((value >> 8U) & UINT32_C(0x000000FF));
    p[2] = (uint8_t)((value >> 16U) & UINT32_C(0x000000FF));
    p[3] = (uint8_t)(value >> 24U);
}

static void write_u64_le(uint8_t *p, uint64_t value)
{
    write_u32_le(p, (uint32_t)(value & UINT64_C(0xFFFFFFFF)));
    write_u32_le(&p[4], (uint32_t)(value >> 32U));
}

static uart_m_codec_status_t encode_direct_u8(int value, int maximum, uint8_t *wire)
{
    if ((value < 0) || (value > maximum)) {
        return UART_M_CODEC_INVALID_ENUM;
    }
    *wire = (uint8_t)value;
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t decode_direct_u8(uint8_t wire, uint8_t maximum, int *value)
{
    if (wire > maximum) {
        return UART_M_CODEC_INVALID_ENUM;
    }
    *value = (int)wire;
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t encode_unknown_u8(int value, int unknown, uint8_t *wire)
{
    if ((value >= 0) && (value < unknown)) {
        *wire = (uint8_t)value;
        return UART_M_CODEC_OK;
    }
    if (value == unknown) {
        *wire = UINT8_MAX;
        return UART_M_CODEC_OK;
    }
    return UART_M_CODEC_INVALID_ENUM;
}

static uart_m_codec_status_t decode_unknown_u8(uint8_t wire, uint8_t unknown, int *value)
{
    if (wire < unknown) {
        *value = (int)wire;
        return UART_M_CODEC_OK;
    }
    if (wire == UINT8_MAX) {
        *value = (int)unknown;
        return UART_M_CODEC_OK;
    }
    return UART_M_CODEC_INVALID_ENUM;
}

static uart_m_codec_status_t encode_one_based(int value, int maximum, uint8_t *wire)
{
    if ((value < 0) || (value > maximum)) {
        return UART_M_CODEC_INVALID_ENUM;
    }
    *wire = (uint8_t)(value + 1);
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t decode_one_based(uint8_t wire, uint8_t maximum, int *value)
{
    if ((wire == 0U) || (wire > (uint8_t)(maximum + 1U))) {
        return UART_M_CODEC_INVALID_ENUM;
    }
    *value = (int)(wire - 1U);
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t encode_result(uart_m_vehicle_result_t value, uint8_t *wire)
{
    return encode_unknown_u8((int)value, (int)UART_M_RESULT_NOT_PRESENT, wire);
}

static uart_m_codec_status_t decode_result(uint8_t wire, uart_m_vehicle_result_t *value)
{
    int decoded;
    uart_m_codec_status_t status = decode_unknown_u8(wire, (uint8_t)UART_M_RESULT_NOT_PRESENT, &decoded);
    if (status == UART_M_CODEC_OK) {
        *value = (uart_m_vehicle_result_t)decoded;
    }
    return status;
}

static uart_m_codec_status_t encode_app_context(const uart_m_app_context_t *context, uint8_t *p)
{
    if (context->device_context_id == 0U) {
        return UART_M_CODEC_INVALID_CONTEXT;
    }
    write_u32_le(&p[0], context->device_context_id);
    write_u64_le(&p[4], context->session_id);
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t decode_app_context(const uint8_t *p, uart_m_app_context_t *context)
{
    context->device_context_id = read_u32_le(&p[0]);
    context->session_id = read_u64_le(&p[4]);
    return (context->device_context_id == 0U) ? UART_M_CODEC_INVALID_CONTEXT : UART_M_CODEC_OK;
}

static uart_m_codec_status_t encode_down_context(const uart_m_down_context_t *context, uint8_t *p)
{
    uart_m_codec_status_t status = encode_app_context(&context->recipient, p);
    if (status != UART_M_CODEC_OK) {
        return status;
    }
    if (context->domain_boot_id == 0U) {
        return UART_M_CODEC_INVALID_CONTEXT;
    }
    write_u32_le(&p[12], context->domain_boot_id);
    write_u32_le(&p[16], context->query_id);
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t decode_down_context(const uint8_t *p, uart_m_down_context_t *context)
{
    uart_m_codec_status_t status = decode_app_context(p, &context->recipient);
    if (status != UART_M_CODEC_OK) {
        return status;
    }
    context->domain_boot_id = read_u32_le(&p[12]);
    context->query_id = read_u32_le(&p[16]);
    return (context->domain_boot_id == 0U) ? UART_M_CODEC_INVALID_CONTEXT : UART_M_CODEC_OK;
}

static uart_m_codec_status_t encode_can_source(const uart_m_can_source_t *source, uint8_t *p)
{
    if (source->boot_id == 0U) {
        return UART_M_CODEC_INVALID_CONTEXT;
    }
    write_u32_le(&p[0], source->boot_id);
    write_u32_le(&p[4], source->tx_sequence);
    return UART_M_CODEC_OK;
}

static uart_m_codec_status_t decode_can_source(const uint8_t *p, uart_m_can_source_t *source)
{
    source->boot_id = read_u32_le(&p[0]);
    source->tx_sequence = read_u32_le(&p[4]);
    return (source->boot_id == 0U) ? UART_M_CODEC_INVALID_CONTEXT : UART_M_CODEC_OK;
}

static bool valid_reason(uint16_t reason)
{
    return reason <= 25U;
}

const uart_m_message_descriptor_t *uart_m_codec_descriptor(uint8_t message_type)
{
    for (size_t i = 0U; i < (sizeof(s_descriptors) / sizeof(s_descriptors[0])); ++i) {
        if ((uint8_t)s_descriptors[i].type == message_type) {
            return &s_descriptors[i];
        }
    }
    return NULL;
}

uart_m_codec_status_t uart_m_codec_validate_direction(
    uint8_t message_type,
    uart_m_direction_t direction)
{
    const uart_m_message_descriptor_t *descriptor = uart_m_codec_descriptor(message_type);
    if (descriptor == NULL) {
        return UART_M_CODEC_UNSUPPORTED_TYPE;
    }
    if ((direction != UART_M_DIRECTION_ESP32_TX) &&
        (direction != UART_M_DIRECTION_ESP32_RX)) {
        return UART_M_CODEC_INVALID_ARGUMENT;
    }
    return (descriptor->direction == direction) ? UART_M_CODEC_OK : UART_M_CODEC_INVALID_DIRECTION;
}

#define RETURN_IF_ERROR(expression)                     \
    do {                                                \
        status = (expression);                          \
        if (status != UART_M_CODEC_OK) {                \
            return status;                              \
        }                                               \
    } while (0)

static uart_m_codec_status_t encode_message_body(const uart_m_message_t *message, uint8_t *p)
{
    uart_m_codec_status_t status;

    switch (message->type) {
    case UART_M_TYPE_CONTEXT: {
        const uart_m_context_t *m = &message->body.context;
        RETURN_IF_ERROR(encode_app_context(&m->context, p));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->registration, (int)UART_M_TRI_UNKNOWN, &p[12]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->connection, (int)UART_M_TRI_UNKNOWN, &p[13]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->app_active, (int)UART_M_TRI_UNKNOWN, &p[14]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->app_quality, (int)UART_M_QUALITY_NO_DATA, &p[15]));
        write_u32_le(&p[16], m->app_update_sequence);
        write_u16_le(&p[20], m->app_age_ms);
        RETURN_IF_ERROR(encode_unknown_u8((int)m->proximity, (int)UART_M_TRI_UNKNOWN, &p[22]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->proximity_validity, (int)UART_M_VALIDITY_VALID, &p[23]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->proximity_reason, (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[24]));
        write_u32_le(&p[25], m->proximity_update_sequence);
        write_u16_le(&p[29], m->proximity_age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->registration_connection_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[31]));
        write_u32_le(&p[32], m->registration_connection_update_sequence);
        write_u16_le(&p[36], m->registration_connection_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DOMAIN_ALIVE: {
        const uart_m_domain_alive_t *m = &message->body.domain_alive;
        if (m->domain_boot_id == 0U) {
            return UART_M_CODEC_INVALID_CONTEXT;
        }
        write_u32_le(&p[0], m->domain_boot_id);
        write_u32_le(&p[4], m->update_sequence);
        write_u16_le(&p[8], m->source_age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->domain_state, (int)UART_M_CONTROLLER_FAULT, &p[10]));
        return encode_direct_u8((int)m->quality, (int)UART_M_QUALITY_NO_DATA, &p[11]);
    }
    case UART_M_TYPE_REQUEST: {
        const uart_m_request_t *m = &message->body.request;
        if (m->request_id == 0U) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_app_context(&m->context, p));
        write_u32_le(&p[12], m->request_id);
        RETURN_IF_ERROR(encode_one_based((int)m->kind, (int)UART_M_REQUEST_DIGITAL_KEY, &p[16]));
        switch (m->kind) {
        case UART_M_REQUEST_DOOR:
            RETURN_IF_ERROR(encode_direct_u8((int)m->argument.door,
                                             (int)UART_M_DOOR_OPERATION_UNLOCK, &p[17]));
            break;
        case UART_M_REQUEST_CLIMATE:
            RETURN_IF_ERROR(encode_one_based((int)m->argument.climate.operation,
                                             (int)UART_M_CLIMATE_FAN, &p[17]));
            switch (m->argument.climate.operation) {
            case UART_M_CLIMATE_TARGET_TEMPERATURE:
                if (m->argument.climate.target_temperature_centi_c == INT16_MIN) {
                    return UART_M_CODEC_INVALID_RANGE;
                }
                write_i16_le(&p[18], m->argument.climate.target_temperature_centi_c);
                break;
            case UART_M_CLIMATE_AUTO:
                RETURN_IF_ERROR(encode_direct_u8((int)m->argument.climate.enabled,
                                                 (int)UART_M_FLAG_YES, &p[18]));
                break;
            case UART_M_CLIMATE_FAN:
                if (m->argument.climate.fan_level == UART_M_FAN_UNKNOWN) {
                    return UART_M_CODEC_INVALID_ENUM;
                }
                RETURN_IF_ERROR(encode_direct_u8((int)m->argument.climate.fan_level,
                                                 (int)UART_M_FAN_HIGH, &p[18]));
                break;
            default:
                return UART_M_CODEC_INVALID_ENUM;
            }
            break;
        case UART_M_REQUEST_INTERIOR_LIGHT:
            RETURN_IF_ERROR(encode_one_based((int)m->argument.light.operation,
                                             (int)UART_M_LIGHT_RGB, &p[17]));
            switch (m->argument.light.operation) {
            case UART_M_LIGHT_ENABLE:
                RETURN_IF_ERROR(encode_direct_u8((int)m->argument.light.enabled,
                                                 (int)UART_M_FLAG_YES, &p[18]));
                break;
            case UART_M_LIGHT_LEVEL:
                if (m->argument.light.level_percent > 100U) {
                    return UART_M_CODEC_INVALID_RANGE;
                }
                p[18] = m->argument.light.level_percent;
                break;
            case UART_M_LIGHT_RGB:
                p[18] = m->argument.light.rgb.r;
                p[19] = m->argument.light.rgb.g;
                p[20] = m->argument.light.rgb.b;
                break;
            default:
                return UART_M_CODEC_INVALID_ENUM;
            }
            break;
        case UART_M_REQUEST_DIGITAL_KEY:
            p[17] = 1U;
            RETURN_IF_ERROR(encode_direct_u8((int)m->argument.digital_key_enabled,
                                             (int)UART_M_FLAG_YES, &p[18]));
            break;
        default:
            return UART_M_CODEC_INVALID_ENUM;
        }
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_QUERY: {
        const uart_m_query_t *m = &message->body.query;
        if (m->query_id == 0U) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_app_context(&m->context, p));
        write_u32_le(&p[12], m->query_id);
        RETURN_IF_ERROR(encode_direct_u8((int)m->scope, (int)UART_M_QUERY_ALL, &p[16]));
        p[17] = 0U;
        write_u64_le(&p[18], m->request_session);
        write_u32_le(&p[26], m->request_id);
        write_u16_le(&p[30], 0U);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WARNING_ACK: {
        const uart_m_warning_ack_t *m = &message->body.warning_ack;
        if (m->warning_domain_boot == 0U) {
            return UART_M_CODEC_INVALID_CONTEXT;
        }
        RETURN_IF_ERROR(encode_app_context(&m->context, p));
        RETURN_IF_ERROR(encode_one_based((int)m->warning_type,
                                         (int)UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE, &p[12]));
        write_u32_le(&p[13], m->occurrence_id);
        write_u32_le(&p[17], m->warning_domain_boot);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_RESULT: {
        const uart_m_result_t *m = &message->body.result;
        if (m->request_id == 0U) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        if (!valid_reason(m->result_reason)) {
            return UART_M_CODEC_INVALID_ENUM;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        write_u64_le(&p[20], m->request_session);
        write_u32_le(&p[28], m->request_id);
        RETURN_IF_ERROR(encode_result(m->result, &p[32]));
        write_u16_le(&p[33], m->result_reason);
        RETURN_IF_ERROR(encode_direct_u8((int)m->confirmation, (int)UART_M_CONFIRMED, &p[35]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->command_target,
                                         (int)UART_M_COMMAND_TARGET_WINDOW, &p[36]));
        write_u32_le(&p[37], m->command_id);
        RETURN_IF_ERROR(encode_direct_u8((int)m->origin, (int)UART_M_ORIGIN_VEHICLE_POLICY, &p[41]));
        write_u16_le(&p[42], m->result_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WARNING: {
        const uart_m_warning_t *m = &message->body.warning;
        if (m->source_boot == 0U) {
            return UART_M_CODEC_INVALID_CONTEXT;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_one_based((int)m->warning_type,
                                         (int)UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE, &p[20]));
        write_u32_le(&p[21], m->occurrence_id);
        RETURN_IF_ERROR(encode_direct_u8((int)m->severity, (int)UART_M_SEVERITY_EMERGENCY, &p[25]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->state, (int)UART_M_WARNING_ACTIVE, &p[26]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->read, (int)UART_M_FLAG_YES, &p[27]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->quality, (int)UART_M_QUALITY_NO_DATA, &p[28]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->quality_reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[29]));
        write_u32_le(&p[30], m->source_boot);
        write_u32_le(&p[34], m->source_sequence);
        write_u16_le(&p[38], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_AVAILABILITY: {
        const uart_m_availability_message_t *m = &message->body.availability;
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        for (size_t i = 0U; i < UART_M_AVAILABILITY_ITEM_COUNT; ++i) {
            const size_t offset = 20U + (4U * i);
            if ((int)m->items[i].function_id != (int)i) {
                return UART_M_CODEC_INVALID_RESERVED;
            }
            if (!valid_reason(m->items[i].reason)) {
                return UART_M_CODEC_INVALID_ENUM;
            }
            RETURN_IF_ERROR(encode_one_based((int)m->items[i].function_id,
                                             (int)UART_M_FUNCTION_VSS, &p[offset]));
            RETURN_IF_ERROR(encode_direct_u8((int)m->items[i].availability,
                                             (int)UART_M_AVAILABILITY_UNAVAILABLE, &p[offset + 1U]));
            write_u16_le(&p[offset + 2U], m->items[i].reason);
        }
        write_u32_le(&p[56], m->update_sequence);
        write_u16_le(&p[60], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DIGITAL_STATUS: {
        const uart_m_digital_status_t *m = &message->body.digital_status;
        if (!valid_reason(m->reason)) {
            return UART_M_CODEC_INVALID_ENUM;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_direct_u8((int)m->setting, (int)UART_M_FLAG_YES, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->availability,
                                         (int)UART_M_AVAILABILITY_UNAVAILABLE, &p[21]));
        write_u16_le(&p[22], m->reason);
        write_u32_le(&p[24], m->update_sequence);
        write_u16_le(&p[28], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DIGITAL_RESULT: {
        const uart_m_digital_result_t *m = &message->body.digital_result;
        if (!valid_reason(m->result_reason) || (m->origin != UART_M_ORIGIN_PROXIMITY_AUTO)) {
            return UART_M_CODEC_INVALID_ENUM;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        write_u32_le(&p[20], m->auto_occurrence_id);
        write_u32_le(&p[24], m->related_door_command_id);
        RETURN_IF_ERROR(encode_direct_u8((int)m->origin, (int)UART_M_ORIGIN_VEHICLE_POLICY, &p[28]));
        RETURN_IF_ERROR(encode_result(m->result, &p[29]));
        write_u16_le(&p[30], m->result_reason);
        RETURN_IF_ERROR(encode_direct_u8((int)m->confirmation, (int)UART_M_CONFIRMED, &p[32]));
        write_u16_le(&p[33], m->result_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_USER_SETTINGS: {
        const uart_m_user_settings_t *m = &message->body.user_settings;
        if (m->normal_light_level > 100U) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        write_i16_le(&p[20], m->target_temperature_centi_c);
        RETURN_IF_ERROR(encode_unknown_u8((int)m->climate_auto, (int)UART_M_TRI_UNKNOWN, &p[22]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->fan_level, (int)UART_M_FAN_UNKNOWN, &p[23]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->light_enabled, (int)UART_M_TRI_UNKNOWN, &p[24]));
        p[25] = m->normal_light_level;
        p[26] = m->normal_light_rgb.r;
        p[27] = m->normal_light_rgb.g;
        p[28] = m->normal_light_rgb.b;
        RETURN_IF_ERROR(encode_direct_u8((int)m->quality, (int)UART_M_QUALITY_NO_DATA, &p[29]));
        write_u32_le(&p[30], m->update_sequence);
        write_u16_le(&p[34], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_QUERY_END: {
        const uart_m_query_end_t *m = &message->body.query_end;
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_direct_u8((int)m->scope, (int)UART_M_QUERY_ALL, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->status,
                                         (int)UART_M_QUERY_CONTEXT_REJECTED, &p[21]));
        write_u16_le(&p[22], m->item_count);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_DOOR_STATE: {
        const uart_m_bcm_door_state_t *m = &message->body.bcm_door_state;
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->lock_state,
                                          (int)UART_M_DOOR_LOCK_UNKNOWN, &p[28]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->open_state,
                                          (int)UART_M_DOOR_OPEN_UNKNOWN, &p[29]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->composite_state,
                                         (int)UART_M_DOOR_UNTRUSTED, &p[30]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->lock_quality, (int)UART_M_QUALITY_NO_DATA, &p[31]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->open_quality, (int)UART_M_QUALITY_NO_DATA, &p[32]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->composite_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[33]));
        write_u32_le(&p[34], m->update_sequence);
        write_u16_le(&p[38], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_CLIMATE_STATE: {
        const uart_m_bcm_climate_state_t *m = &message->body.bcm_climate_state;
        if ((m->fan_command == UART_M_FAN_UNKNOWN) || (m->thermal_output_level > 100U)) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->fan_command, (int)UART_M_FAN_HIGH, &p[28]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->fan_measured, (int)UART_M_FAN_UNKNOWN, &p[29]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->thermal_direction, (int)UART_M_THERMAL_HEAT, &p[30]));
        p[31] = m->thermal_output_level;
        RETURN_IF_ERROR(encode_unknown_u8((int)m->heat_removal,
                                          (int)UART_M_HEAT_REMOVAL_UNAVAILABLE, &p[32]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->fan_command_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[33]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->fan_measured_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[34]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->thermal_direction_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[35]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->thermal_output_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[36]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->heat_removal_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[37]));
        write_u32_le(&p[38], m->update_sequence);
        write_u16_le(&p[42], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_LIGHT_STATE: {
        const uart_m_bcm_light_state_t *m = &message->body.bcm_light_state;
        if (m->level > 100U) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->light_type, (int)UART_M_LIGHT_FAULT, &p[28]));
        p[29] = m->level;
        p[30] = m->rgb.r;
        p[31] = m->rgb.g;
        p[32] = m->rgb.b;
        RETURN_IF_ERROR(encode_unknown_u8((int)m->apply_result,
                                          (int)UART_M_LIGHT_APPLY_UNKNOWN, &p[33]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->physical_feedback, (int)UART_M_FLAG_YES, &p[34]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->type_quality, (int)UART_M_QUALITY_NO_DATA, &p[35]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->level_quality, (int)UART_M_QUALITY_NO_DATA, &p[36]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->color_quality, (int)UART_M_QUALITY_NO_DATA, &p[37]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->apply_quality, (int)UART_M_QUALITY_NO_DATA, &p[38]));
        write_u32_le(&p[39], m->update_sequence);
        write_u16_le(&p[43], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_STATUS: {
        const uart_m_bcm_status_t *m = &message->body.bcm_status;
        if (((m->fault_mask & UINT16_C(0xFF00)) != 0U) ||
            (m->active_fault_code > 8U) ||
            ((m->affected_function_mask & UINT8_C(0xF8)) != 0U)) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->ecu_state, (int)UART_M_CONTROLLER_FAULT, &p[28]));
        write_u16_le(&p[29], m->fault_mask);
        RETURN_IF_ERROR(encode_direct_u8((int)m->active_fault_category,
                                         (int)UART_M_FAULT_CATEGORY_COMM, &p[31]));
        write_u16_le(&p[32], m->active_fault_code);
        p[34] = m->affected_function_mask;
        RETURN_IF_ERROR(encode_direct_u8((int)m->recovering, (int)UART_M_FLAG_YES, &p[35]));
        write_u32_le(&p[36], m->update_sequence);
        write_u16_le(&p[40], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_ENVIRONMENT: {
        const uart_m_cis_environment_t *m = &message->body.cis_environment;
        if (!((m->temperature.value == INT16_MIN) ||
              ((m->temperature.value >= -4000) && (m->temperature.value <= 12500))) ||
            !((m->humidity.value <= 10000U) || (m->humidity.value == UINT16_MAX)) ||
            !((m->illuminance.value <= UINT32_C(200000)) ||
              (m->illuminance.value == UINT32_MAX))) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        write_i16_le(&p[28], m->temperature.value);
        write_u32_le(&p[30], m->temperature.sequence);
        write_u16_le(&p[34], m->temperature.age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->temperature.validity,
                                         (int)UART_M_VALIDITY_VALID, &p[36]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->temperature.reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[37]));
        write_u16_le(&p[38], m->humidity.value);
        write_u32_le(&p[40], m->humidity.sequence);
        write_u16_le(&p[44], m->humidity.age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->humidity.validity,
                                         (int)UART_M_VALIDITY_VALID, &p[46]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->humidity.reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[47]));
        write_u32_le(&p[48], m->illuminance.value);
        write_u32_le(&p[52], m->illuminance.sequence);
        write_u16_le(&p[56], m->illuminance.age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->illuminance.validity,
                                         (int)UART_M_VALIDITY_VALID, &p[58]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->illuminance.reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[59]));
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_OCCUPANT: {
        const uart_m_cis_occupant_t *m = &message->body.cis_occupant;
        if (!((m->occupant_count <= 5U) || (m->occupant_count == UINT8_MAX))) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        if (m->pi_boot_id == 0U) {
            return UART_M_CODEC_INVALID_CONTEXT;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        write_u32_le(&p[28], m->pi_boot_id);
        write_u32_le(&p[32], m->vision_sequence);
        write_u16_le(&p[36], m->vision_age_ms);
        RETURN_IF_ERROR(encode_unknown_u8((int)m->presence, (int)UART_M_TRI_UNKNOWN, &p[38]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->presence_validity,
                                         (int)UART_M_VALIDITY_VALID, &p[39]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->presence_reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[40]));
        p[41] = m->occupant_count;
        RETURN_IF_ERROR(encode_direct_u8((int)m->count_validity,
                                         (int)UART_M_VALIDITY_VALID, &p[42]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->count_reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[43]));
        return encode_direct_u8((int)m->vision_function_status,
                                (int)UART_M_SENSOR_RECOVERING, &p[44]);
    }
    case UART_M_TYPE_CIS_REAR: {
        const uart_m_cis_rear_t *m = &message->body.cis_rear;
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        write_u32_le(&p[28], m->update_sequence);
        write_u16_le(&p[32], m->source_age_ms);
        write_u16_le(&p[34], m->rear_distance);
        RETURN_IF_ERROR(encode_direct_u8((int)m->validity, (int)UART_M_VALIDITY_VALID, &p[36]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->quality_reason,
                                         (int)UART_M_QUALITY_REASON_DATA_INVALID, &p[37]));
        return encode_direct_u8((int)m->proximity_status, (int)UART_M_PROXIMITY_INACTIVE, &p[38]);
    }
    case UART_M_TYPE_CIS_STATUS: {
        const uart_m_cis_status_t *m = &message->body.cis_status;
        if (((m->fault_mask & UINT16_C(0xFE00)) != 0U) ||
            (m->active_fault > 9U) || (m->last_fault > 9U) ||
            ((m->affected_function_mask & UINT8_C(0xC0)) != 0U)) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->cis_state, (int)UART_M_CONTROLLER_FAULT, &p[28]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->vision_status, (int)UART_M_SENSOR_RECOVERING, &p[29]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->temperature_status, (int)UART_M_SENSOR_RECOVERING, &p[30]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->humidity_status, (int)UART_M_SENSOR_RECOVERING, &p[31]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->illuminance_status, (int)UART_M_SENSOR_RECOVERING, &p[32]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->rear_status, (int)UART_M_SENSOR_RECOVERING, &p[33]));
        if ((m->pi_uart_status > 2U) || (m->can_interface_status > 2U)) {
            return UART_M_CODEC_INVALID_ENUM;
        }
        p[34] = m->pi_uart_status;
        p[35] = m->can_interface_status;
        write_u16_le(&p[36], m->fault_mask);
        write_u16_le(&p[38], m->active_fault);
        RETURN_IF_ERROR(encode_direct_u8((int)m->recovering, (int)UART_M_FLAG_YES, &p[40]));
        write_u16_le(&p[41], m->last_fault);
        write_u32_le(&p[43], m->last_fault_occurrence);
        p[47] = m->affected_function_mask;
        write_u32_le(&p[48], m->update_sequence);
        write_u16_le(&p[52], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WINDOW_STATE: {
        const uart_m_window_state_t *m = &message->body.window_state;
        if (!((m->position <= 100U) || (m->position == UINT8_MAX))) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->motion,
                                          (int)UART_M_WINDOW_MOTION_UNKNOWN, &p[28]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->ecu_state, (int)UART_M_CONTROLLER_FAULT, &p[29]));
        p[30] = m->position;
        RETURN_IF_ERROR(encode_direct_u8((int)m->quality, (int)UART_M_QUALITY_NO_DATA, &p[31]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->fully_open, (int)UART_M_TRI_UNKNOWN, &p[32]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->fully_closed, (int)UART_M_TRI_UNKNOWN, &p[33]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->antipinch,
                                         (int)UART_M_ANTIPINCH_INTERRUPTED, &p[34]));
        RETURN_IF_ERROR(encode_unknown_u8((int)m->reverse, (int)UART_M_TRI_UNKNOWN, &p[35]));
        write_u32_le(&p[36], m->update_sequence);
        write_u16_le(&p[40], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WINDOW_FAULT: {
        const uart_m_window_fault_t *m = &message->body.window_fault;
        if (!((m->code <= 6U) || (m->code == UINT8_MAX))) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->category, (int)UART_M_FAULT_CATEGORY_COMM, &p[28]));
        write_u16_le(&p[29], m->code);
        RETURN_IF_ERROR(encode_direct_u8((int)m->status,
                                         (int)UART_M_WINDOW_FAULT_RECOVERING, &p[31]));
        write_u32_le(&p[32], m->occurrence_id);
        write_u32_le(&p[36], m->update_sequence);
        write_u16_le(&p[40], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_VSS_STATUS: {
        const uart_m_vss_status_t *m = &message->body.vss_status;
        if (((m->fault_mask & UINT8_C(0xE0)) != 0U) || (m->last_fault > 5U) ||
            !((m->last_fault_quality == UART_M_QUALITY_OK) ||
              (m->last_fault_quality == UART_M_QUALITY_NO_DATA))) {
            return UART_M_CODEC_INVALID_RANGE;
        }
        RETURN_IF_ERROR(encode_down_context(&m->context, p));
        RETURN_IF_ERROR(encode_can_source(&m->source, &p[20]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->state, (int)UART_M_VSS_FAULT, &p[28]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->availability, (int)UART_M_VSS_UNAVAILABLE, &p[29]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->accepting_events, (int)UART_M_FLAG_YES, &p[30]));
        RETURN_IF_ERROR(encode_direct_u8((int)m->fault_active, (int)UART_M_FLAG_YES, &p[31]));
        p[32] = m->fault_mask;
        write_u16_le(&p[33], m->last_fault);
        write_u32_le(&p[35], m->last_fault_occurrence);
        write_u16_le(&p[39], m->last_fault_age_ms);
        RETURN_IF_ERROR(encode_direct_u8((int)m->last_fault_quality,
                                         (int)UART_M_QUALITY_NO_DATA, &p[41]));
        write_u32_le(&p[42], m->update_sequence);
        write_u16_le(&p[46], m->source_age_ms);
        return UART_M_CODEC_OK;
    }
    default:
        return UART_M_CODEC_UNSUPPORTED_TYPE;
    }
}

uart_m_codec_status_t uart_m_codec_encode(
    const uart_m_message_t *message,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length)
{
    uint8_t encoded[128] = {0};
    const uart_m_message_descriptor_t *descriptor;
    uart_m_codec_status_t status;

    if ((message == NULL) || (payload == NULL) || (payload_length == NULL)) {
        return UART_M_CODEC_INVALID_ARGUMENT;
    }
    descriptor = uart_m_codec_descriptor((uint8_t)message->type);
    if (descriptor == NULL) {
        return UART_M_CODEC_UNSUPPORTED_TYPE;
    }
    if (payload_capacity < descriptor->payload_length) {
        return UART_M_CODEC_OUTPUT_TOO_SMALL;
    }
    status = encode_message_body(message, encoded);
    if (status != UART_M_CODEC_OK) {
        return status;
    }
    memcpy(payload, encoded, descriptor->payload_length);
    *payload_length = descriptor->payload_length;
    return UART_M_CODEC_OK;
}

#define DECODE_DIRECT(target, source, maximum, enum_type)      \
    do {                                                       \
        int decoded_value;                                     \
        status = decode_direct_u8((source), (maximum), &decoded_value); \
        if (status != UART_M_CODEC_OK) {                       \
            return status;                                     \
        }                                                       \
        (target) = (enum_type)decoded_value;                   \
    } while (0)

#define DECODE_UNKNOWN(target, source, unknown, enum_type)     \
    do {                                                       \
        int decoded_value;                                     \
        status = decode_unknown_u8((source), (unknown), &decoded_value); \
        if (status != UART_M_CODEC_OK) {                       \
            return status;                                     \
        }                                                       \
        (target) = (enum_type)decoded_value;                   \
    } while (0)

#define DECODE_ONE_BASED(target, source, maximum, enum_type)   \
    do {                                                       \
        int decoded_value;                                     \
        status = decode_one_based((source), (maximum), &decoded_value); \
        if (status != UART_M_CODEC_OK) {                       \
            return status;                                     \
        }                                                       \
        (target) = (enum_type)decoded_value;                   \
    } while (0)

static uart_m_codec_status_t decode_message_body(
    uart_m_message_t *message,
    const uint8_t *p)
{
    uart_m_codec_status_t status;

    switch (message->type) {
    case UART_M_TYPE_CONTEXT: {
        uart_m_context_t *m = &message->body.context;
        RETURN_IF_ERROR(decode_app_context(p, &m->context));
        DECODE_UNKNOWN(m->registration, p[12], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_UNKNOWN(m->connection, p[13], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_UNKNOWN(m->app_active, p[14], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_DIRECT(m->app_quality, p[15], (uint8_t)UART_M_QUALITY_NO_DATA, uart_m_value_quality_t);
        m->app_update_sequence = read_u32_le(&p[16]);
        m->app_age_ms = read_u16_le(&p[20]);
        DECODE_UNKNOWN(m->proximity, p[22], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_DIRECT(m->proximity_validity, p[23], (uint8_t)UART_M_VALIDITY_VALID, uart_m_validity_t);
        DECODE_DIRECT(m->proximity_reason, p[24],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        m->proximity_update_sequence = read_u32_le(&p[25]);
        m->proximity_age_ms = read_u16_le(&p[29]);
        DECODE_DIRECT(m->registration_connection_quality, p[31],
                      (uint8_t)UART_M_QUALITY_NO_DATA, uart_m_value_quality_t);
        m->registration_connection_update_sequence = read_u32_le(&p[32]);
        m->registration_connection_age_ms = read_u16_le(&p[36]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DOMAIN_ALIVE: {
        uart_m_domain_alive_t *m = &message->body.domain_alive;
        m->domain_boot_id = read_u32_le(&p[0]);
        m->update_sequence = read_u32_le(&p[4]);
        m->source_age_ms = read_u16_le(&p[8]);
        DECODE_DIRECT(m->domain_state, p[10], (uint8_t)UART_M_CONTROLLER_FAULT,
                      uart_m_controller_state_t);
        DECODE_DIRECT(m->quality, p[11], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_REQUEST: {
        uart_m_request_t *m = &message->body.request;
        RETURN_IF_ERROR(decode_app_context(p, &m->context));
        m->request_id = read_u32_le(&p[12]);
        DECODE_ONE_BASED(m->kind, p[16], (uint8_t)UART_M_REQUEST_DIGITAL_KEY,
                         uart_m_request_kind_t);
        switch (m->kind) {
        case UART_M_REQUEST_DOOR:
            DECODE_DIRECT(m->argument.door, p[17], (uint8_t)UART_M_DOOR_OPERATION_UNLOCK,
                          uart_m_door_operation_t);
            break;
        case UART_M_REQUEST_CLIMATE:
            DECODE_ONE_BASED(m->argument.climate.operation, p[17], (uint8_t)UART_M_CLIMATE_FAN,
                             uart_m_climate_operation_t);
            if (m->argument.climate.operation == UART_M_CLIMATE_TARGET_TEMPERATURE) {
                m->argument.climate.target_temperature_centi_c = read_i16_le(&p[18]);
            } else if (m->argument.climate.operation == UART_M_CLIMATE_AUTO) {
                DECODE_DIRECT(m->argument.climate.enabled, p[18], (uint8_t)UART_M_FLAG_YES,
                              uart_m_flag_t);
            } else {
                DECODE_DIRECT(m->argument.climate.fan_level, p[18], (uint8_t)UART_M_FAN_HIGH,
                              uart_m_fan_level_t);
            }
            break;
        case UART_M_REQUEST_INTERIOR_LIGHT:
            DECODE_ONE_BASED(m->argument.light.operation, p[17], (uint8_t)UART_M_LIGHT_RGB,
                             uart_m_light_operation_t);
            if (m->argument.light.operation == UART_M_LIGHT_ENABLE) {
                DECODE_DIRECT(m->argument.light.enabled, p[18], (uint8_t)UART_M_FLAG_YES,
                              uart_m_flag_t);
            } else if (m->argument.light.operation == UART_M_LIGHT_LEVEL) {
                m->argument.light.level_percent = p[18];
            } else {
                m->argument.light.rgb.r = p[18];
                m->argument.light.rgb.g = p[19];
                m->argument.light.rgb.b = p[20];
            }
            break;
        case UART_M_REQUEST_DIGITAL_KEY:
            if (p[17] != 1U) {
                return UART_M_CODEC_INVALID_ENUM;
            }
            DECODE_DIRECT(m->argument.digital_key_enabled, p[18], (uint8_t)UART_M_FLAG_YES,
                          uart_m_flag_t);
            break;
        default:
            return UART_M_CODEC_INVALID_ENUM;
        }
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_QUERY: {
        uart_m_query_t *m = &message->body.query;
        RETURN_IF_ERROR(decode_app_context(p, &m->context));
        m->query_id = read_u32_le(&p[12]);
        DECODE_DIRECT(m->scope, p[16], (uint8_t)UART_M_QUERY_ALL, uart_m_query_scope_t);
        m->request_session = read_u64_le(&p[18]);
        m->request_id = read_u32_le(&p[26]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WARNING_ACK: {
        uart_m_warning_ack_t *m = &message->body.warning_ack;
        RETURN_IF_ERROR(decode_app_context(p, &m->context));
        DECODE_ONE_BASED(m->warning_type, p[12],
                         (uint8_t)UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE, uart_m_warning_type_t);
        m->occurrence_id = read_u32_le(&p[13]);
        m->warning_domain_boot = read_u32_le(&p[17]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_RESULT: {
        uart_m_result_t *m = &message->body.result;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        m->request_session = read_u64_le(&p[20]);
        m->request_id = read_u32_le(&p[28]);
        RETURN_IF_ERROR(decode_result(p[32], &m->result));
        m->result_reason = read_u16_le(&p[33]);
        DECODE_DIRECT(m->confirmation, p[35], (uint8_t)UART_M_CONFIRMED, uart_m_confirmation_t);
        DECODE_DIRECT(m->command_target, p[36], (uint8_t)UART_M_COMMAND_TARGET_WINDOW,
                      uart_m_command_target_t);
        m->command_id = read_u32_le(&p[37]);
        DECODE_DIRECT(m->origin, p[41], (uint8_t)UART_M_ORIGIN_VEHICLE_POLICY, uart_m_origin_t);
        m->result_age_ms = read_u16_le(&p[42]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WARNING: {
        uart_m_warning_t *m = &message->body.warning;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        DECODE_ONE_BASED(m->warning_type, p[20],
                         (uint8_t)UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE, uart_m_warning_type_t);
        m->occurrence_id = read_u32_le(&p[21]);
        DECODE_DIRECT(m->severity, p[25], (uint8_t)UART_M_SEVERITY_EMERGENCY, uart_m_severity_t);
        DECODE_DIRECT(m->state, p[26], (uint8_t)UART_M_WARNING_ACTIVE, uart_m_warning_state_t);
        DECODE_DIRECT(m->read, p[27], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        DECODE_DIRECT(m->quality, p[28], (uint8_t)UART_M_QUALITY_NO_DATA, uart_m_value_quality_t);
        DECODE_DIRECT(m->quality_reason, p[29],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        m->source_boot = read_u32_le(&p[30]);
        m->source_sequence = read_u32_le(&p[34]);
        m->source_age_ms = read_u16_le(&p[38]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_AVAILABILITY: {
        uart_m_availability_message_t *m = &message->body.availability;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        for (size_t i = 0U; i < UART_M_AVAILABILITY_ITEM_COUNT; ++i) {
            const size_t offset = 20U + (4U * i);
            DECODE_ONE_BASED(m->items[i].function_id, p[offset], (uint8_t)UART_M_FUNCTION_VSS,
                             uart_m_function_id_t);
            DECODE_DIRECT(m->items[i].availability, p[offset + 1U],
                          (uint8_t)UART_M_AVAILABILITY_UNAVAILABLE, uart_m_availability_t);
            m->items[i].reason = read_u16_le(&p[offset + 2U]);
        }
        m->update_sequence = read_u32_le(&p[56]);
        m->source_age_ms = read_u16_le(&p[60]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DIGITAL_STATUS: {
        uart_m_digital_status_t *m = &message->body.digital_status;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        DECODE_DIRECT(m->setting, p[20], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        DECODE_DIRECT(m->availability, p[21], (uint8_t)UART_M_AVAILABILITY_UNAVAILABLE,
                      uart_m_availability_t);
        m->reason = read_u16_le(&p[22]);
        m->update_sequence = read_u32_le(&p[24]);
        m->source_age_ms = read_u16_le(&p[28]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_DIGITAL_RESULT: {
        uart_m_digital_result_t *m = &message->body.digital_result;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        m->auto_occurrence_id = read_u32_le(&p[20]);
        m->related_door_command_id = read_u32_le(&p[24]);
        DECODE_DIRECT(m->origin, p[28], (uint8_t)UART_M_ORIGIN_VEHICLE_POLICY, uart_m_origin_t);
        RETURN_IF_ERROR(decode_result(p[29], &m->result));
        m->result_reason = read_u16_le(&p[30]);
        DECODE_DIRECT(m->confirmation, p[32], (uint8_t)UART_M_CONFIRMED, uart_m_confirmation_t);
        m->result_age_ms = read_u16_le(&p[33]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_USER_SETTINGS: {
        uart_m_user_settings_t *m = &message->body.user_settings;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        m->target_temperature_centi_c = read_i16_le(&p[20]);
        DECODE_UNKNOWN(m->climate_auto, p[22], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_UNKNOWN(m->fan_level, p[23], (uint8_t)UART_M_FAN_UNKNOWN, uart_m_fan_level_t);
        DECODE_UNKNOWN(m->light_enabled, p[24], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        m->normal_light_level = p[25];
        m->normal_light_rgb.r = p[26];
        m->normal_light_rgb.g = p[27];
        m->normal_light_rgb.b = p[28];
        DECODE_DIRECT(m->quality, p[29], (uint8_t)UART_M_QUALITY_NO_DATA, uart_m_value_quality_t);
        m->update_sequence = read_u32_le(&p[30]);
        m->source_age_ms = read_u16_le(&p[34]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_QUERY_END: {
        uart_m_query_end_t *m = &message->body.query_end;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        DECODE_DIRECT(m->scope, p[20], (uint8_t)UART_M_QUERY_ALL, uart_m_query_scope_t);
        DECODE_DIRECT(m->status, p[21], (uint8_t)UART_M_QUERY_CONTEXT_REJECTED,
                      uart_m_query_status_t);
        m->item_count = read_u16_le(&p[22]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_DOOR_STATE: {
        uart_m_bcm_door_state_t *m = &message->body.bcm_door_state;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_UNKNOWN(m->lock_state, p[28], (uint8_t)UART_M_DOOR_LOCK_UNKNOWN,
                       uart_m_door_lock_state_t);
        DECODE_UNKNOWN(m->open_state, p[29], (uint8_t)UART_M_DOOR_OPEN_UNKNOWN,
                       uart_m_door_open_state_t);
        DECODE_DIRECT(m->composite_state, p[30], (uint8_t)UART_M_DOOR_UNTRUSTED,
                      uart_m_door_composite_t);
        DECODE_DIRECT(m->lock_quality, p[31], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->open_quality, p[32], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->composite_quality, p[33], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        m->update_sequence = read_u32_le(&p[34]);
        m->source_age_ms = read_u16_le(&p[38]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_CLIMATE_STATE: {
        uart_m_bcm_climate_state_t *m = &message->body.bcm_climate_state;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->fan_command, p[28], (uint8_t)UART_M_FAN_HIGH, uart_m_fan_level_t);
        DECODE_UNKNOWN(m->fan_measured, p[29], (uint8_t)UART_M_FAN_UNKNOWN, uart_m_fan_level_t);
        DECODE_DIRECT(m->thermal_direction, p[30], (uint8_t)UART_M_THERMAL_HEAT,
                      uart_m_thermal_direction_t);
        m->thermal_output_level = p[31];
        DECODE_UNKNOWN(m->heat_removal, p[32], (uint8_t)UART_M_HEAT_REMOVAL_UNAVAILABLE,
                       uart_m_heat_removal_t);
        DECODE_DIRECT(m->fan_command_quality, p[33], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->fan_measured_quality, p[34], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->thermal_direction_quality, p[35], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->thermal_output_quality, p[36], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->heat_removal_quality, p[37], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        m->update_sequence = read_u32_le(&p[38]);
        m->source_age_ms = read_u16_le(&p[42]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_LIGHT_STATE: {
        uart_m_bcm_light_state_t *m = &message->body.bcm_light_state;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->light_type, p[28], (uint8_t)UART_M_LIGHT_FAULT, uart_m_light_type_t);
        m->level = p[29];
        m->rgb.r = p[30];
        m->rgb.g = p[31];
        m->rgb.b = p[32];
        DECODE_UNKNOWN(m->apply_result, p[33], (uint8_t)UART_M_LIGHT_APPLY_UNKNOWN,
                       uart_m_light_apply_t);
        DECODE_DIRECT(m->physical_feedback, p[34], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        DECODE_DIRECT(m->type_quality, p[35], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->level_quality, p[36], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->color_quality, p[37], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_DIRECT(m->apply_quality, p[38], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        m->update_sequence = read_u32_le(&p[39]);
        m->source_age_ms = read_u16_le(&p[43]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_BCM_STATUS: {
        uart_m_bcm_status_t *m = &message->body.bcm_status;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->ecu_state, p[28], (uint8_t)UART_M_CONTROLLER_FAULT,
                      uart_m_controller_state_t);
        m->fault_mask = read_u16_le(&p[29]);
        DECODE_DIRECT(m->active_fault_category, p[31], (uint8_t)UART_M_FAULT_CATEGORY_COMM,
                      uart_m_fault_category_t);
        m->active_fault_code = read_u16_le(&p[32]);
        m->affected_function_mask = p[34];
        DECODE_DIRECT(m->recovering, p[35], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        m->update_sequence = read_u32_le(&p[36]);
        m->source_age_ms = read_u16_le(&p[40]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_ENVIRONMENT: {
        uart_m_cis_environment_t *m = &message->body.cis_environment;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        m->temperature.value = read_i16_le(&p[28]);
        m->temperature.sequence = read_u32_le(&p[30]);
        m->temperature.age_ms = read_u16_le(&p[34]);
        DECODE_DIRECT(m->temperature.validity, p[36], (uint8_t)UART_M_VALIDITY_VALID,
                      uart_m_validity_t);
        DECODE_DIRECT(m->temperature.reason, p[37],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        m->humidity.value = read_u16_le(&p[38]);
        m->humidity.sequence = read_u32_le(&p[40]);
        m->humidity.age_ms = read_u16_le(&p[44]);
        DECODE_DIRECT(m->humidity.validity, p[46], (uint8_t)UART_M_VALIDITY_VALID,
                      uart_m_validity_t);
        DECODE_DIRECT(m->humidity.reason, p[47],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        m->illuminance.value = read_u32_le(&p[48]);
        m->illuminance.sequence = read_u32_le(&p[52]);
        m->illuminance.age_ms = read_u16_le(&p[56]);
        DECODE_DIRECT(m->illuminance.validity, p[58], (uint8_t)UART_M_VALIDITY_VALID,
                      uart_m_validity_t);
        DECODE_DIRECT(m->illuminance.reason, p[59],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_OCCUPANT: {
        uart_m_cis_occupant_t *m = &message->body.cis_occupant;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        m->pi_boot_id = read_u32_le(&p[28]);
        m->vision_sequence = read_u32_le(&p[32]);
        m->vision_age_ms = read_u16_le(&p[36]);
        DECODE_UNKNOWN(m->presence, p[38], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_DIRECT(m->presence_validity, p[39], (uint8_t)UART_M_VALIDITY_VALID,
                      uart_m_validity_t);
        DECODE_DIRECT(m->presence_reason, p[40],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        m->occupant_count = p[41];
        DECODE_DIRECT(m->count_validity, p[42], (uint8_t)UART_M_VALIDITY_VALID,
                      uart_m_validity_t);
        DECODE_DIRECT(m->count_reason, p[43],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        DECODE_DIRECT(m->vision_function_status, p[44], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_REAR: {
        uart_m_cis_rear_t *m = &message->body.cis_rear;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        m->update_sequence = read_u32_le(&p[28]);
        m->source_age_ms = read_u16_le(&p[32]);
        m->rear_distance = read_u16_le(&p[34]);
        DECODE_DIRECT(m->validity, p[36], (uint8_t)UART_M_VALIDITY_VALID, uart_m_validity_t);
        DECODE_DIRECT(m->quality_reason, p[37],
                      (uint8_t)UART_M_QUALITY_REASON_DATA_INVALID, uart_m_quality_reason_t);
        DECODE_DIRECT(m->proximity_status, p[38], (uint8_t)UART_M_PROXIMITY_INACTIVE,
                      uart_m_proximity_status_t);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_CIS_STATUS: {
        uart_m_cis_status_t *m = &message->body.cis_status;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->cis_state, p[28], (uint8_t)UART_M_CONTROLLER_FAULT,
                      uart_m_controller_state_t);
        DECODE_DIRECT(m->vision_status, p[29], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        DECODE_DIRECT(m->temperature_status, p[30], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        DECODE_DIRECT(m->humidity_status, p[31], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        DECODE_DIRECT(m->illuminance_status, p[32], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        DECODE_DIRECT(m->rear_status, p[33], (uint8_t)UART_M_SENSOR_RECOVERING,
                      uart_m_sensor_state_t);
        m->pi_uart_status = p[34];
        m->can_interface_status = p[35];
        m->fault_mask = read_u16_le(&p[36]);
        m->active_fault = read_u16_le(&p[38]);
        DECODE_DIRECT(m->recovering, p[40], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        m->last_fault = read_u16_le(&p[41]);
        m->last_fault_occurrence = read_u32_le(&p[43]);
        m->affected_function_mask = p[47];
        m->update_sequence = read_u32_le(&p[48]);
        m->source_age_ms = read_u16_le(&p[52]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WINDOW_STATE: {
        uart_m_window_state_t *m = &message->body.window_state;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_UNKNOWN(m->motion, p[28], (uint8_t)UART_M_WINDOW_MOTION_UNKNOWN,
                       uart_m_window_motion_t);
        DECODE_DIRECT(m->ecu_state, p[29], (uint8_t)UART_M_CONTROLLER_FAULT,
                      uart_m_controller_state_t);
        m->position = p[30];
        DECODE_DIRECT(m->quality, p[31], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        DECODE_UNKNOWN(m->fully_open, p[32], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_UNKNOWN(m->fully_closed, p[33], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        DECODE_DIRECT(m->antipinch, p[34], (uint8_t)UART_M_ANTIPINCH_INTERRUPTED,
                      uart_m_antipinch_state_t);
        DECODE_UNKNOWN(m->reverse, p[35], (uint8_t)UART_M_TRI_UNKNOWN, uart_m_tri_state_t);
        m->update_sequence = read_u32_le(&p[36]);
        m->source_age_ms = read_u16_le(&p[40]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_WINDOW_FAULT: {
        uart_m_window_fault_t *m = &message->body.window_fault;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->category, p[28], (uint8_t)UART_M_FAULT_CATEGORY_COMM,
                      uart_m_fault_category_t);
        m->code = read_u16_le(&p[29]);
        DECODE_DIRECT(m->status, p[31], (uint8_t)UART_M_WINDOW_FAULT_RECOVERING,
                      uart_m_window_fault_state_t);
        m->occurrence_id = read_u32_le(&p[32]);
        m->update_sequence = read_u32_le(&p[36]);
        m->source_age_ms = read_u16_le(&p[40]);
        return UART_M_CODEC_OK;
    }
    case UART_M_TYPE_VSS_STATUS: {
        uart_m_vss_status_t *m = &message->body.vss_status;
        RETURN_IF_ERROR(decode_down_context(p, &m->context));
        RETURN_IF_ERROR(decode_can_source(&p[20], &m->source));
        DECODE_DIRECT(m->state, p[28], (uint8_t)UART_M_VSS_FAULT, uart_m_vss_state_t);
        DECODE_DIRECT(m->availability, p[29], (uint8_t)UART_M_VSS_UNAVAILABLE,
                      uart_m_vss_availability_t);
        DECODE_DIRECT(m->accepting_events, p[30], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        DECODE_DIRECT(m->fault_active, p[31], (uint8_t)UART_M_FLAG_YES, uart_m_flag_t);
        m->fault_mask = p[32];
        m->last_fault = read_u16_le(&p[33]);
        m->last_fault_occurrence = read_u32_le(&p[35]);
        m->last_fault_age_ms = read_u16_le(&p[39]);
        DECODE_DIRECT(m->last_fault_quality, p[41], (uint8_t)UART_M_QUALITY_NO_DATA,
                      uart_m_value_quality_t);
        m->update_sequence = read_u32_le(&p[42]);
        m->source_age_ms = read_u16_le(&p[46]);
        return UART_M_CODEC_OK;
    }
    default:
        return UART_M_CODEC_UNSUPPORTED_TYPE;
    }
}

uart_m_codec_status_t uart_m_codec_decode(
    uint8_t message_type,
    const uint8_t *payload,
    size_t payload_length,
    uart_m_message_t *message)
{
    uint8_t canonical[128] = {0};
    const uart_m_message_descriptor_t *descriptor;
    uart_m_codec_status_t status;

    if ((payload == NULL) || (message == NULL)) {
        return UART_M_CODEC_INVALID_ARGUMENT;
    }
    descriptor = uart_m_codec_descriptor(message_type);
    if (descriptor == NULL) {
        return UART_M_CODEC_UNSUPPORTED_TYPE;
    }
    if (payload_length != descriptor->payload_length) {
        return UART_M_CODEC_INVALID_LENGTH;
    }

    memset(message, 0, sizeof(*message));
    message->type = descriptor->type;
    status = decode_message_body(message, payload);
    if (status != UART_M_CODEC_OK) {
        return status;
    }
    status = encode_message_body(message, canonical);
    if (status != UART_M_CODEC_OK) {
        return status;
    }
    if (memcmp(canonical, payload, payload_length) != 0) {
        return UART_M_CODEC_INVALID_RESERVED;
    }
    return UART_M_CODEC_OK;
}
