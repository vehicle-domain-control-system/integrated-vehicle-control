#include "gateway_mapper.h"

#include <string.h>

#include "ble_gatt_protocol.h"

#define GATEWAY_CONTEXT_PREFIX_LENGTH 12U

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void write_u64_le(uint8_t *data, uint64_t value)
{
    write_u32_le(data, (uint32_t)value);
    write_u32_le(&data[4], (uint32_t)(value >> 32U));
}

static gateway_mapper_status_t map_codec_status(uart_m_codec_status_t status)
{
    switch (status) {
    case UART_M_CODEC_OK:
        return GATEWAY_MAPPER_OK;
    case UART_M_CODEC_INVALID_ARGUMENT:
        return GATEWAY_MAPPER_INVALID_ARGUMENT;
    case UART_M_CODEC_UNSUPPORTED_TYPE:
    case UART_M_CODEC_INVALID_DIRECTION:
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    case UART_M_CODEC_INVALID_LENGTH:
        return GATEWAY_MAPPER_INVALID_LENGTH;
    case UART_M_CODEC_OUTPUT_TOO_SMALL:
        return GATEWAY_MAPPER_OUTPUT_TOO_SMALL;
    case UART_M_CODEC_INVALID_CONTEXT:
        return GATEWAY_MAPPER_INVALID_CONTEXT;
    case UART_M_CODEC_INVALID_ENUM:
    case UART_M_CODEC_INVALID_RESERVED:
    case UART_M_CODEC_INVALID_RANGE:
    default:
        return GATEWAY_MAPPER_INVALID_VALUE;
    }
}

static gateway_mapper_status_t map_ble_validation(uint8_t status)
{
    if (status == 0U) {
        return GATEWAY_MAPPER_OK;
    }
    if (status == BLE_GATT_ATT_ERR_MESSAGE_TYPE_UNSUPPORTED) {
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    }
    if (status == BLE_GATT_ATT_ERR_PAYLOAD_LENGTH_INVALID) {
        return GATEWAY_MAPPER_INVALID_LENGTH;
    }
    return GATEWAY_MAPPER_INVALID_VALUE;
}

gateway_mapper_status_t gateway_mapper_ble_uplink_to_uart(
    uint8_t message_type,
    const uint8_t *payload,
    size_t payload_length,
    const gateway_verified_context_t *context,
    uart_m_message_t *out_message)
{
    uint8_t uart_payload[32] = {0};
    const uart_m_message_descriptor_t *descriptor;
    gateway_mapper_status_t status;
    uart_m_codec_status_t codec_status;

    if ((payload == NULL) || (context == NULL) || (out_message == NULL)) {
        return GATEWAY_MAPPER_INVALID_ARGUMENT;
    }
    if (!context->verified || (context->device_context_id == 0U) ||
        (context->session_id == 0U)) {
        return GATEWAY_MAPPER_INVALID_CONTEXT;
    }

    status = map_ble_validation(
        ble_gatt_protocol_validate_uplink_payload(message_type, payload, payload_length));
    if (status != GATEWAY_MAPPER_OK) {
        return status;
    }

    descriptor = uart_m_codec_descriptor(message_type);
    if ((descriptor == NULL) || (descriptor->direction != UART_M_DIRECTION_ESP32_TX) ||
        (descriptor->payload_length != payload_length + GATEWAY_CONTEXT_PREFIX_LENGTH) ||
        (descriptor->payload_length > sizeof(uart_payload))) {
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    }

    write_u32_le(uart_payload, context->device_context_id);
    write_u64_le(&uart_payload[4], context->session_id);
    memcpy(&uart_payload[GATEWAY_CONTEXT_PREFIX_LENGTH], payload, payload_length);

    codec_status = uart_m_codec_decode(message_type, uart_payload,
                                       descriptor->payload_length, out_message);
    return map_codec_status(codec_status);
}

gateway_mapper_status_t gateway_mapper_uart_downlink_to_ble(
    const uart_m_message_t *message,
    gateway_ble_message_t *out_message)
{
    const uart_m_message_descriptor_t *descriptor;
    size_t expected_ble_length;
    size_t encoded_length = 0U;
    uart_m_codec_status_t codec_status;

    if ((message == NULL) || (out_message == NULL)) {
        return GATEWAY_MAPPER_INVALID_ARGUMENT;
    }

    descriptor = uart_m_codec_descriptor((uint8_t)message->type);
    if (descriptor == NULL) {
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    }
    if ((message->type == UART_M_TYPE_DOMAIN_ALIVE) ||
        (message->type == UART_M_TYPE_CONTEXT)) {
        return GATEWAY_MAPPER_PRIVATE_MESSAGE;
    }
    if (descriptor->schema_only) {
        return GATEWAY_MAPPER_SCHEMA_HOLD;
    }
    if (descriptor->direction != UART_M_DIRECTION_ESP32_RX) {
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    }

    expected_ble_length = ble_gatt_protocol_payload_length((uint8_t)message->type, false);
    if ((expected_ble_length == 0U) ||
        (expected_ble_length != descriptor->payload_length) ||
        (expected_ble_length > sizeof(out_message->payload))) {
        return GATEWAY_MAPPER_UNSUPPORTED_TYPE;
    }

    memset(out_message, 0, sizeof(*out_message));
    codec_status = uart_m_codec_encode(message, out_message->payload,
                                       sizeof(out_message->payload), &encoded_length);
    if (codec_status != UART_M_CODEC_OK) {
        return map_codec_status(codec_status);
    }
    if (encoded_length != expected_ble_length) {
        return GATEWAY_MAPPER_INVALID_LENGTH;
    }

    out_message->message_type = (uint8_t)message->type;
    out_message->payload_length = encoded_length;
    return GATEWAY_MAPPER_OK;
}

gateway_mapper_status_t gateway_mapper_encode_context_schema(
    const uart_m_context_t *context,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length)
{
    uart_m_message_t message;
    uart_m_codec_status_t codec_status;

    if ((context == NULL) || (payload == NULL) || (payload_length == NULL)) {
        return GATEWAY_MAPPER_INVALID_ARGUMENT;
    }
    if (payload_capacity < GATEWAY_MAPPER_CONTEXT_PAYLOAD_LENGTH) {
        return GATEWAY_MAPPER_OUTPUT_TOO_SMALL;
    }

    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_CONTEXT;
    message.body.context = *context;
    codec_status = uart_m_codec_encode(&message, payload, payload_capacity, payload_length);
    if (codec_status != UART_M_CODEC_OK) {
        return map_codec_status(codec_status);
    }
    return (*payload_length == GATEWAY_MAPPER_CONTEXT_PAYLOAD_LENGTH)
               ? GATEWAY_MAPPER_OK
               : GATEWAY_MAPPER_INVALID_LENGTH;
}
