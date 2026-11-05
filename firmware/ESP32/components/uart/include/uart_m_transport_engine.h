#ifndef UART_M_TRANSPORT_ENGINE_H
#define UART_M_TRANSPORT_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uart_m_parser.h"
#include "uart_m_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t frames_valid;
    uint32_t messages_accepted;
    uint32_t bytes_discarded;
    uint32_t version_errors;
    uint32_t length_errors;
    uint32_t crc_errors;
    uint32_t parser_overflow_errors;
    uint32_t codec_errors;
    uint32_t direction_errors;
    uint32_t schema_only_dropped;
    uint32_t delivery_rejected;
} uart_m_transport_feed_result_t;

/* Called synchronously while feed() is running. Return false on backpressure. */
typedef bool (*uart_m_transport_engine_handler_t)(
    const uart_m_transport_rx_message_t *message,
    void *user_context);

typedef struct
{
    uart_m_parser_t parser;
    uart_m_sequence_t tx_sequence;
    uart_m_transport_engine_handler_t receive_handler;
    void *receive_user_context;
    int64_t current_receive_time_us;
    uart_m_transport_feed_result_t *current_result;
} uart_m_transport_engine_t;

uart_m_transport_status_t uart_m_transport_engine_init(
    uart_m_transport_engine_t *engine,
    uint16_t first_tx_sequence,
    uart_m_transport_engine_handler_t receive_handler,
    void *receive_user_context);

uart_m_transport_status_t uart_m_transport_engine_prepare_tx(
    uart_m_transport_engine_t *engine,
    const uart_m_message_t *message,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint16_t *link_sequence);

uart_m_transport_status_t uart_m_transport_engine_feed(
    uart_m_transport_engine_t *engine,
    const uint8_t *bytes,
    size_t byte_count,
    int64_t received_at_us,
    uart_m_transport_feed_result_t *result);

size_t uart_m_transport_engine_reset_partial(uart_m_transport_engine_t *engine);
size_t uart_m_transport_engine_pending_bytes(const uart_m_transport_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
