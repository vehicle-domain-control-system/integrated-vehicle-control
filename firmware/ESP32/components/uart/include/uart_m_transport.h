#ifndef UART_M_TRANSPORT_H
#define UART_M_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "uart_m_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    UART_M_TRANSPORT_OK = 0,
    UART_M_TRANSPORT_INVALID_ARGUMENT,
    UART_M_TRANSPORT_NOT_READY,
    UART_M_TRANSPORT_NO_CAPACITY,
    UART_M_TRANSPORT_CODEC_ERROR,
    UART_M_TRANSPORT_FRAME_ERROR,
    UART_M_TRANSPORT_DRIVER_ERROR,
    UART_M_TRANSPORT_TIMEOUT
} uart_m_transport_status_t;

typedef enum
{
    UART_M_TRANSPORT_PARITY_NONE = 0,
    UART_M_TRANSPORT_PARITY_EVEN,
    UART_M_TRANSPORT_PARITY_ODD
} uart_m_transport_parity_t;

typedef enum
{
    UART_M_TRANSPORT_STOP_BITS_1 = 1,
    UART_M_TRANSPORT_STOP_BITS_2 = 2
} uart_m_transport_stop_bits_t;

typedef enum
{
    UART_M_TRANSPORT_FLOW_NONE = 0,
    UART_M_TRANSPORT_FLOW_RTS,
    UART_M_TRANSPORT_FLOW_CTS,
    UART_M_TRANSPORT_FLOW_RTS_CTS
} uart_m_transport_flow_control_t;

typedef struct
{
    int uart_port;
    int tx_gpio;
    int rx_gpio;
    int rts_gpio;
    int cts_gpio;
    uint32_t baud_rate;
    uint8_t data_bits;
    uart_m_transport_parity_t parity;
    uart_m_transport_stop_bits_t stop_bits;
    uart_m_transport_flow_control_t flow_control;
    uint8_t rx_flow_control_threshold;
    size_t driver_rx_buffer_size;
    size_t driver_tx_buffer_size;
    size_t driver_event_queue_depth;
    size_t tx_queue_depth;
    size_t rx_delivery_queue_depth;
    size_t rx_chunk_size;
    uint32_t uart_event_wait_ms;
    uint32_t frame_assembly_timeout_ms;
    uint32_t tx_complete_timeout_ms;
    uint32_t shutdown_timeout_ms;
    uint32_t rx_task_stack_size;
    uint32_t tx_task_stack_size;
    uint32_t dispatch_task_stack_size;
    unsigned rx_task_priority;
    unsigned tx_task_priority;
    unsigned dispatch_task_priority;
    uint16_t first_tx_sequence;
} uart_m_transport_config_t;

typedef struct
{
    uart_m_message_t message;
    uint16_t link_sequence;
    int64_t received_at_us;
} uart_m_transport_rx_message_t;

/*
 * message and its nested value fields are borrowed and remain valid only for
 * this callback. The callback runs on the transport dispatch task, never on
 * the UART RX task. Copy the value before returning if it is needed later.
 */
typedef void (*uart_m_transport_rx_callback_t)(
    const uart_m_transport_rx_message_t *message,
    void *user_context);

typedef struct
{
    uint32_t rx_bytes;
    uint32_t rx_frames_valid;
    uint32_t rx_messages_queued;
    uint32_t rx_messages_delivered;
    uint32_t rx_bytes_discarded;
    uint32_t rx_version_errors;
    uint32_t rx_length_errors;
    uint32_t rx_crc_errors;
    uint32_t rx_parser_overflow_errors;
    uint32_t rx_codec_errors;
    uint32_t rx_direction_errors;
    uint32_t rx_schema_only_dropped;
    uint32_t rx_delivery_queue_full;
    uint32_t uart_rx_overflow_errors;
    uint32_t uart_frame_errors;
    uint32_t uart_read_errors;
    uint32_t frame_assembly_timeouts;
    uint32_t tx_messages_queued;
    uint32_t tx_queue_full;
    uint32_t tx_encode_errors;
    uint32_t tx_write_errors;
    uint32_t tx_complete_timeouts;
    uint32_t tx_frames_completed;
    uint32_t forced_task_stops;
} uart_m_transport_stats_t;

typedef struct uart_m_transport uart_m_transport_t;

/* No default hardware binding is supplied. Every field must be provided. */
uart_m_transport_status_t uart_m_transport_init(
    uart_m_transport_t **transport,
    const uart_m_transport_config_t *config,
    uart_m_transport_rx_callback_t rx_callback,
    void *rx_user_context);

/* Copies the typed message before returning. OK means local queue admission only. */
uart_m_transport_status_t uart_m_transport_send(
    uart_m_transport_t *transport,
    const uart_m_message_t *message,
    uint32_t queue_wait_ms);

void uart_m_transport_get_stats(
    const uart_m_transport_t *transport,
    uart_m_transport_stats_t *stats);

/*
 * Stops tasks, removes the ESP-IDF driver and releases all owned resources.
 * The lifecycle owner must not call this from rx_callback or concurrently with
 * another public transport call.
 */
uart_m_transport_status_t uart_m_transport_deinit(uart_m_transport_t **transport);

#ifdef __cplusplus
}
#endif

#endif
