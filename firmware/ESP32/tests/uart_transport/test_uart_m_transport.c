#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mock_idf.h"
#include "uart_m_frame.h"
#include "uart_m_transport.h"
#include "uart_m_transport_engine.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition) do { if (!(condition)) { \
    printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); return false; \
} } while (0)

typedef struct
{
    uart_m_transport_rx_message_t messages[8];
    size_t count;
    bool accept;
} receive_log_t;

/* Mirrors the private queue value solely to inspect the host FreeRTOS stub FIFO. */
typedef struct
{
    bool stop;
    uart_m_message_t message;
} mock_tx_queue_item_t;

static bool engine_receive(const uart_m_transport_rx_message_t *message, void *context)
{
    receive_log_t *log = (receive_log_t *)context;
    if (!log->accept || (log->count == ARRAY_SIZE(log->messages))) return false;
    log->messages[log->count++] = *message;
    return true;
}

static void transport_receive(const uart_m_transport_rx_message_t *message, void *context)
{
    (void)message;
    (void)context;
}

static void make_query(uart_m_message_t *message, uint32_t query_id)
{
    memset(message, 0, sizeof(*message));
    message->type = UART_M_TYPE_QUERY;
    message->body.query.context.device_context_id = UINT32_C(0x11223344);
    message->body.query.context.session_id = UINT64_C(0x0102030405060708);
    message->body.query.query_id = query_id;
    message->body.query.scope = UART_M_QUERY_STATE;
}

static void make_alive(uart_m_message_t *message, uint32_t boot_id)
{
    memset(message, 0, sizeof(*message));
    message->type = UART_M_TYPE_DOMAIN_ALIVE;
    message->body.domain_alive.domain_boot_id = boot_id;
    message->body.domain_alive.update_sequence = boot_id + 10U;
    message->body.domain_alive.source_age_ms = 5U;
    message->body.domain_alive.domain_state = UART_M_CONTROLLER_READY;
    message->body.domain_alive.quality = UART_M_QUALITY_OK;
}

static void make_window_state(uart_m_message_t *message)
{
    uart_m_window_state_t *window;
    memset(message, 0, sizeof(*message));
    message->type = UART_M_TYPE_WINDOW_STATE;
    window = &message->body.window_state;
    window->context.recipient.device_context_id = 1U;
    window->context.recipient.session_id = 2U;
    window->context.domain_boot_id = 3U;
    window->source.boot_id = 4U;
    window->source.tx_sequence = 5U;
    window->motion = UART_M_WINDOW_STOPPED;
    window->ecu_state = UART_M_CONTROLLER_READY;
    window->position = 50U;
    window->quality = UART_M_QUALITY_OK;
    window->fully_open = UART_M_TRI_NO;
    window->fully_closed = UART_M_TRI_NO;
    window->antipinch = UART_M_ANTIPINCH_IDLE;
    window->reverse = UART_M_TRI_NO;
    window->update_sequence = 6U;
}

static bool encode_rx_frame(const uart_m_message_t *message, uint16_t sequence,
                            uint8_t *frame, size_t *frame_length)
{
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t payload_length = 0U;
    return (uart_m_codec_encode(message, payload, sizeof(payload), &payload_length) ==
            UART_M_CODEC_OK) &&
           (uart_m_frame_encode((uint8_t)message->type, sequence, payload, payload_length,
                                frame, UART_M_MAX_FRAME_SIZE, frame_length) ==
            UART_M_FRAME_STATUS_OK);
}

static uart_m_transport_config_t mock_config(void)
{
    uart_m_transport_config_t config;
    memset(&config, 0, sizeof(config));
    config.uart_port = 1;
    config.tx_gpio = 17;
    config.rx_gpio = 16;
    config.rts_gpio = -1;
    config.cts_gpio = -1;
    config.baud_rate = 115200U;
    config.data_bits = 8U;
    config.parity = UART_M_TRANSPORT_PARITY_NONE;
    config.stop_bits = UART_M_TRANSPORT_STOP_BITS_1;
    config.flow_control = UART_M_TRANSPORT_FLOW_NONE;
    config.driver_rx_buffer_size = 512U;
    config.driver_tx_buffer_size = 512U;
    config.driver_event_queue_depth = 8U;
    config.tx_queue_depth = 2U;
    config.rx_delivery_queue_depth = 3U;
    config.rx_chunk_size = 64U;
    config.uart_event_wait_ms = 10U;
    config.frame_assembly_timeout_ms = 50U;
    config.tx_complete_timeout_ms = 100U;
    config.shutdown_timeout_ms = 100U;
    config.rx_task_stack_size = 2048U;
    config.tx_task_stack_size = 2048U;
    config.dispatch_task_stack_size = 2048U;
    config.rx_task_priority = 5U;
    config.tx_task_priority = 5U;
    config.dispatch_task_priority = 4U;
    config.first_tx_sequence = UINT16_MAX;
    return config;
}

static bool test_tx_queue_fifo_and_sequence(void)
{
    uart_m_transport_t *transport = NULL;
    uart_m_transport_config_t config = mock_config();
    uart_m_message_t first;
    uart_m_message_t second;
    uart_m_message_t third;
    mock_tx_queue_item_t queued_first;
    mock_tx_queue_item_t queued_second;
    uart_m_transport_engine_t engine;
    receive_log_t log = {.accept = true};
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t frame_length;
    uint16_t sequence;
    uart_m_frame_t decoded;
    uart_m_transport_stats_t stats;

    mock_idf_reset();
    make_query(&first, 1U); make_query(&second, 2U); make_query(&third, 3U);
    CHECK(uart_m_transport_init(&transport, &config, transport_receive, NULL) ==
          UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_send(transport, &first, 0U) == UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_send(transport, &second, 0U) == UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_send(transport, &third, 0U) == UART_M_TRANSPORT_NO_CAPACITY);
    uart_m_transport_get_stats(transport, &stats);
    CHECK(stats.tx_messages_queued == 2U);
    CHECK(stats.tx_queue_full == 1U);

    CHECK(mock_idf_queue_pop(0U, &queued_first, sizeof(queued_first)));
    CHECK(mock_idf_queue_pop(0U, &queued_second, sizeof(queued_second)));
    CHECK(queued_first.message.body.query.query_id == 1U);
    CHECK(queued_second.message.body.query.query_id == 2U);

    CHECK(uart_m_transport_engine_init(&engine, UINT16_MAX, engine_receive, &log) ==
          UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_engine_prepare_tx(&engine, &queued_first.message, frame,
                                              sizeof(frame), &frame_length, &sequence) ==
          UART_M_TRANSPORT_OK);
    CHECK(sequence == UINT16_MAX);
    CHECK(uart_m_frame_decode(frame, frame_length, &decoded) == UART_M_FRAME_STATUS_OK);
    CHECK(decoded.link_sequence == UINT16_MAX);
    CHECK(uart_m_transport_engine_prepare_tx(&engine, &queued_second.message, frame,
                                              sizeof(frame), &frame_length, &sequence) ==
          UART_M_TRANSPORT_OK);
    CHECK(sequence == 0U);
    CHECK(uart_m_transport_deinit(&transport) == UART_M_TRANSPORT_OK);
    CHECK(mock_idf_live_queue_count() == 0U);
    CHECK(mock_idf_live_event_group_count() == 0U);
    return true;
}

static bool test_partial_and_multiple_frames(void)
{
    uart_m_transport_engine_t engine;
    receive_log_t log = {.accept = true};
    uart_m_message_t first;
    uart_m_message_t second;
    uint8_t frame_a[UART_M_MAX_FRAME_SIZE];
    uint8_t frame_b[UART_M_MAX_FRAME_SIZE];
    uint8_t joined[UART_M_MAX_FRAME_SIZE * 2U];
    size_t length_a;
    size_t length_b;

    make_alive(&first, 1U); make_alive(&second, 2U);
    CHECK(encode_rx_frame(&first, 10U, frame_a, &length_a));
    CHECK(encode_rx_frame(&second, 11U, frame_b, &length_b));
    CHECK(uart_m_transport_engine_init(&engine, 1U, engine_receive, &log) ==
          UART_M_TRANSPORT_OK);

    for (size_t i = 0U; i < length_a; ++i) {
        uart_m_transport_feed_result_t result;
        CHECK(uart_m_transport_engine_feed(&engine, &frame_a[i], 1U, (int64_t)i,
                                            &result) == UART_M_TRANSPORT_OK);
    }
    CHECK(log.count == 1U);
    CHECK(log.messages[0].link_sequence == 10U);

    memcpy(joined, frame_a, length_a);
    memcpy(&joined[length_a], frame_b, length_b);
    uart_m_transport_feed_result_t result;
    CHECK(uart_m_transport_engine_feed(&engine, joined, length_a + length_b, 999,
                                        &result) == UART_M_TRANSPORT_OK);
    CHECK(result.frames_valid == 2U);
    CHECK(result.messages_accepted == 2U);
    CHECK(log.count == 3U);
    CHECK(log.messages[2].message.body.domain_alive.domain_boot_id == 2U);
    return true;
}

static bool test_error_recovery_and_two_stage_length(void)
{
    uart_m_transport_engine_t engine;
    receive_log_t log = {.accept = true};
    uart_m_message_t alive;
    uint8_t good[UART_M_MAX_FRAME_SIZE];
    uint8_t bad[UART_M_MAX_FRAME_SIZE];
    uint8_t joined[UART_M_MAX_FRAME_SIZE * 2U];
    uint8_t short_payload[11] = {0};
    size_t good_length;
    size_t bad_length;
    uart_m_transport_feed_result_t result;

    make_alive(&alive, 3U);
    CHECK(encode_rx_frame(&alive, 20U, good, &good_length));
    memcpy(bad, good, good_length);
    bad[good_length - 1U] ^= UINT8_C(0x80);
    memcpy(joined, bad, good_length);
    memcpy(&joined[good_length], good, good_length);
    CHECK(uart_m_transport_engine_init(&engine, 1U, engine_receive, &log) ==
          UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_engine_feed(&engine, joined, good_length * 2U, 10,
                                        &result) == UART_M_TRANSPORT_OK);
    CHECK(result.crc_errors == 1U);
    CHECK(result.messages_accepted == 1U);
    CHECK(log.count == 1U);

    CHECK(uart_m_frame_encode(UART_M_TYPE_DOMAIN_ALIVE, 21U, short_payload,
                              sizeof(short_payload), bad, sizeof(bad), &bad_length) ==
          UART_M_FRAME_STATUS_OK);
    memcpy(joined, bad, bad_length);
    memcpy(&joined[bad_length], good, good_length);
    CHECK(uart_m_transport_engine_feed(&engine, joined, bad_length + good_length, 20,
                                        &result) == UART_M_TRANSPORT_OK);
    CHECK(result.frames_valid == 2U);
    CHECK(result.length_errors == 1U);
    CHECK(result.messages_accepted == 1U);
    CHECK(log.count == 2U);
    return true;
}

static bool test_direction_backpressure_and_timeout_reset(void)
{
    uart_m_transport_engine_t engine;
    receive_log_t log = {.accept = false};
    uart_m_message_t query;
    uart_m_message_t alive;
    uart_m_message_t window;
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    uint8_t payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t payload_length;
    size_t frame_length;
    uart_m_transport_feed_result_t result;

    CHECK(uart_m_transport_engine_init(&engine, 1U, engine_receive, &log) ==
          UART_M_TRANSPORT_OK);
    make_query(&query, 5U);
    CHECK(uart_m_codec_encode(&query, payload, sizeof(payload), &payload_length) ==
          UART_M_CODEC_OK);
    CHECK(uart_m_frame_encode(UART_M_TYPE_QUERY, 1U, payload, payload_length,
                              frame, sizeof(frame), &frame_length) == UART_M_FRAME_STATUS_OK);
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length, 1, &result) ==
          UART_M_TRANSPORT_OK);
    CHECK(result.direction_errors == 1U);

    make_alive(&alive, 4U);
    CHECK(encode_rx_frame(&alive, 2U, frame, &frame_length));
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length, 2, &result) ==
          UART_M_TRANSPORT_OK);
    CHECK(result.delivery_rejected == 1U);

    make_window_state(&window);
    CHECK(encode_rx_frame(&window, 3U, frame, &frame_length));
    CHECK(uart_m_transport_engine_feed(&engine, frame, frame_length, 3, &result) ==
          UART_M_TRANSPORT_OK);
    CHECK(result.schema_only_dropped == 1U);
    CHECK(uart_m_transport_engine_feed(&engine, frame, 5U, 3, &result) ==
          UART_M_TRANSPORT_OK);
    CHECK(uart_m_transport_engine_pending_bytes(&engine) == 5U);
    CHECK(uart_m_transport_engine_reset_partial(&engine) == 5U);
    CHECK(uart_m_transport_engine_pending_bytes(&engine) == 0U);
    return true;
}

static bool test_init_failure_cleanup(void)
{
    uart_m_transport_t *transport = NULL;
    uart_m_transport_config_t config = mock_config();

    mock_idf_reset();
    config.tx_gpio = -1;
    CHECK(uart_m_transport_init(&transport, &config, transport_receive, NULL) ==
          UART_M_TRANSPORT_INVALID_ARGUMENT);
    CHECK(mock_idf_live_queue_count() == 0U);

    mock_idf_reset();
    config = mock_config();
    mock_idf_fail_driver_install(true);
    CHECK(uart_m_transport_init(&transport, &config, transport_receive, NULL) ==
          UART_M_TRANSPORT_DRIVER_ERROR);
    CHECK(mock_idf_live_queue_count() == 0U);
    CHECK(mock_idf_live_event_group_count() == 0U);

    mock_idf_reset();
    mock_idf_fail_task_create_call(2U);
    CHECK(uart_m_transport_init(&transport, &config, transport_receive, NULL) ==
          UART_M_TRANSPORT_NO_CAPACITY);
    CHECK(!mock_idf_driver_is_installed());
    CHECK(mock_idf_driver_delete_count() == 1U);
    CHECK(mock_idf_live_queue_count() == 0U);
    CHECK(mock_idf_live_event_group_count() == 0U);
    return true;
}

typedef bool (*test_function_t)(void);
typedef struct { const char *name; test_function_t function; } test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"tx_queue_fifo_sequence", test_tx_queue_fifo_and_sequence},
        {"partial_multiple_frames", test_partial_and_multiple_frames},
        {"error_recovery_two_stage_length", test_error_recovery_and_two_stage_length},
        {"direction_backpressure_timeout", test_direction_backpressure_and_timeout_reset},
        {"init_failure_cleanup", test_init_failure_cleanup},
    };

    for (size_t i = 0U; i < ARRAY_SIZE(tests); ++i) {
        if (!tests[i].function()) {
            printf("[FAIL] %s\n", tests[i].name);
            return 1;
        }
        printf("[PASS] %s\n", tests[i].name);
    }
    printf("All %zu UART-M transport test groups passed.\n", ARRAY_SIZE(tests));
    return 0;
}
