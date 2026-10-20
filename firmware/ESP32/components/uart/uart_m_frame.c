#include "uart_m_frame.h"

#include <string.h>

#include "uart_m_crc16.h"

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & UINT16_C(0x00FF));
    bytes[1] = (uint8_t)(value >> 8U);
}

uart_m_frame_status_t uart_m_frame_encode(
    uint8_t message_type,
    uint16_t link_sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length)
{
    size_t required_length;
    uint16_t crc;

    if ((output == NULL) || (output_length == NULL) ||
        ((payload == NULL) && (payload_length != 0U))) {
        return UART_M_FRAME_STATUS_INVALID_ARGUMENT;
    }
    if (payload_length > UART_M_MAX_PAYLOAD_SIZE) {
        return UART_M_FRAME_STATUS_INVALID_LENGTH;
    }

    required_length = UART_M_MIN_FRAME_SIZE + payload_length;
    if (output_capacity < required_length) {
        return UART_M_FRAME_STATUS_OUTPUT_TOO_SMALL;
    }

    output[0] = UART_M_SYNC_FIRST;
    output[1] = UART_M_SYNC_SECOND;
    output[2] = UART_M_VERSION;
    output[3] = message_type;
    write_u16_le(&output[4], (uint16_t)payload_length);
    write_u16_le(&output[6], link_sequence);
    if (payload_length != 0U) {
        memcpy(&output[UART_M_HEADER_SIZE], payload, payload_length);
    }

    crc = uart_m_crc16_ccitt_false(&output[2], 6U + payload_length);
    write_u16_le(&output[UART_M_HEADER_SIZE + payload_length], crc);
    *output_length = required_length;

    return UART_M_FRAME_STATUS_OK;
}

uart_m_frame_status_t uart_m_frame_decode(
    const uint8_t *frame,
    size_t frame_length,
    uart_m_frame_t *decoded)
{
    uint16_t payload_length;
    size_t expected_length;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((frame == NULL) || (decoded == NULL)) {
        return UART_M_FRAME_STATUS_INVALID_ARGUMENT;
    }
    if (frame_length < UART_M_MIN_FRAME_SIZE) {
        return UART_M_FRAME_STATUS_INCOMPLETE;
    }
    if ((frame[0] != UART_M_SYNC_FIRST) || (frame[1] != UART_M_SYNC_SECOND)) {
        return UART_M_FRAME_STATUS_INVALID_SYNC;
    }
    if (frame[2] != UART_M_VERSION) {
        return UART_M_FRAME_STATUS_INVALID_VERSION;
    }

    payload_length = read_u16_le(&frame[4]);
    if (payload_length > UART_M_MAX_PAYLOAD_SIZE) {
        return UART_M_FRAME_STATUS_INVALID_LENGTH;
    }

    expected_length = UART_M_MIN_FRAME_SIZE + (size_t)payload_length;
    if (frame_length < expected_length) {
        return UART_M_FRAME_STATUS_INCOMPLETE;
    }
    if (frame_length != expected_length) {
        return UART_M_FRAME_STATUS_INVALID_LENGTH;
    }

    expected_crc = read_u16_le(&frame[UART_M_HEADER_SIZE + payload_length]);
    actual_crc = uart_m_crc16_ccitt_false(&frame[2], 6U + payload_length);
    if (actual_crc != expected_crc) {
        return UART_M_FRAME_STATUS_INVALID_CRC;
    }

    decoded->message_type = frame[3];
    decoded->payload_length = payload_length;
    decoded->link_sequence = read_u16_le(&frame[6]);
    if (payload_length != 0U) {
        memcpy(decoded->payload, &frame[UART_M_HEADER_SIZE], payload_length);
    }

    return UART_M_FRAME_STATUS_OK;
}

void uart_m_sequence_init(uart_m_sequence_t *sequence, uint16_t first_value)
{
    if (sequence != NULL) {
        sequence->next_value = first_value;
    }
}

uint16_t uart_m_sequence_take(uart_m_sequence_t *sequence)
{
    uint16_t value;

    if (sequence == NULL) {
        return 0U;
    }

    value = sequence->next_value;
    sequence->next_value = (uint16_t)(value + 1U);
    return value;
}
