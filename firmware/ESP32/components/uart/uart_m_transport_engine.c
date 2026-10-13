#include "uart_m_transport_engine.h"

#include <string.h>

static uint32_t delta_u32(uint32_t after, uint32_t before)
{
    return after - before;
}

static void on_frame(const uart_m_frame_t *frame, void *user_context)
{
    uart_m_transport_engine_t *engine = (uart_m_transport_engine_t *)user_context;
    uart_m_transport_feed_result_t *result = engine->current_result;
    const uart_m_message_descriptor_t *descriptor;
    uart_m_transport_rx_message_t received;
    uart_m_codec_status_t codec_status;

    result->frames_valid++;
    descriptor = uart_m_codec_descriptor(frame->message_type);
    if (descriptor == NULL) {
        result->codec_errors++;
        return;
    }
    if (descriptor->direction != UART_M_DIRECTION_ESP32_RX) {
        result->direction_errors++;
        return;
    }
    if (frame->payload_length != descriptor->payload_length) {
        result->length_errors++;
        return;
    }
    codec_status = uart_m_codec_decode(frame->message_type, frame->payload,
                                       frame->payload_length, &received.message);
    if (codec_status != UART_M_CODEC_OK) {
        result->codec_errors++;
        return;
    }
    if (descriptor->schema_only) {
        result->schema_only_dropped++;
        return;
    }

    received.link_sequence = frame->link_sequence;
    received.received_at_us = engine->current_receive_time_us;
    if (!engine->receive_handler(&received, engine->receive_user_context)) {
        result->delivery_rejected++;
        return;
    }
    result->messages_accepted++;
}

uart_m_transport_status_t uart_m_transport_engine_init(
    uart_m_transport_engine_t *engine,
    uint16_t first_tx_sequence,
    uart_m_transport_engine_handler_t receive_handler,
    void *receive_user_context)
{
    if ((engine == NULL) || (receive_handler == NULL)) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    memset(engine, 0, sizeof(*engine));
    uart_m_parser_init(&engine->parser);
    uart_m_sequence_init(&engine->tx_sequence, first_tx_sequence);
    engine->receive_handler = receive_handler;
    engine->receive_user_context = receive_user_context;
    return UART_M_TRANSPORT_OK;
}

uart_m_transport_status_t uart_m_transport_engine_prepare_tx(
    uart_m_transport_engine_t *engine,
    const uart_m_message_t *message,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint16_t *link_sequence)
{
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t payload_length = 0U;
    uint16_t sequence;
    uart_m_codec_status_t codec_status;
    uart_m_frame_status_t frame_status;

    if ((engine == NULL) || (message == NULL) || (frame == NULL) ||
        (frame_length == NULL) || (link_sequence == NULL)) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    if (uart_m_codec_validate_direction((uint8_t)message->type,
                                        UART_M_DIRECTION_ESP32_TX) != UART_M_CODEC_OK) {
        return UART_M_TRANSPORT_CODEC_ERROR;
    }
    codec_status = uart_m_codec_encode(message, payload, sizeof(payload), &payload_length);
    if (codec_status != UART_M_CODEC_OK) {
        return UART_M_TRANSPORT_CODEC_ERROR;
    }
    if (frame_capacity < (UART_M_MIN_FRAME_SIZE + payload_length)) {
        return UART_M_TRANSPORT_FRAME_ERROR;
    }

    sequence = uart_m_sequence_take(&engine->tx_sequence);
    frame_status = uart_m_frame_encode((uint8_t)message->type, sequence, payload,
                                       payload_length, frame, frame_capacity, frame_length);
    if (frame_status != UART_M_FRAME_STATUS_OK) {
        return UART_M_TRANSPORT_FRAME_ERROR;
    }
    *link_sequence = sequence;
    return UART_M_TRANSPORT_OK;
}

uart_m_transport_status_t uart_m_transport_engine_feed(
    uart_m_transport_engine_t *engine,
    const uint8_t *bytes,
    size_t byte_count,
    int64_t received_at_us,
    uart_m_transport_feed_result_t *result)
{
    uart_m_parser_counters_t before;
    size_t emitted = 0U;

    if ((engine == NULL) || (result == NULL) ||
        ((bytes == NULL) && (byte_count != 0U))) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }

    memset(result, 0, sizeof(*result));
    before = engine->parser.counters;
    engine->current_receive_time_us = received_at_us;
    engine->current_result = result;
    if (uart_m_parser_feed(&engine->parser, bytes, byte_count, on_frame, engine,
                           &emitted) != UART_M_PARSER_STATUS_OK) {
        engine->current_result = NULL;
        return UART_M_TRANSPORT_FRAME_ERROR;
    }
    engine->current_result = NULL;

    result->bytes_discarded += delta_u32(engine->parser.counters.bytes_discarded,
                                         before.bytes_discarded);
    result->version_errors += delta_u32(engine->parser.counters.version_errors,
                                        before.version_errors);
    result->length_errors += delta_u32(engine->parser.counters.length_errors,
                                       before.length_errors);
    result->crc_errors += delta_u32(engine->parser.counters.crc_errors,
                                    before.crc_errors);
    result->parser_overflow_errors += delta_u32(engine->parser.counters.overflow_errors,
                                                before.overflow_errors);
    (void)emitted;
    return UART_M_TRANSPORT_OK;
}

size_t uart_m_transport_engine_reset_partial(uart_m_transport_engine_t *engine)
{
    size_t pending;

    if (engine == NULL) {
        return 0U;
    }
    pending = uart_m_parser_pending_bytes(&engine->parser);
    uart_m_parser_reset_partial(&engine->parser);
    return pending;
}

size_t uart_m_transport_engine_pending_bytes(const uart_m_transport_engine_t *engine)
{
    return (engine == NULL) ? 0U : uart_m_parser_pending_bytes(&engine->parser);
}
