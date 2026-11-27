#include "uart_m_transport.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "uart_m_transport_engine.h"

#define UART_M_TRANSPORT_MAX_RX_CHUNK 256U
#define UART_M_RX_TASK_DONE BIT0
#define UART_M_TX_TASK_DONE BIT1
#define UART_M_DISPATCH_TASK_DONE BIT2
#define UART_M_ALL_TASKS_DONE (UART_M_RX_TASK_DONE | UART_M_TX_TASK_DONE | UART_M_DISPATCH_TASK_DONE)

typedef struct
{
    bool stop;
    uart_m_message_t message;
} tx_queue_item_t;

typedef struct
{
    bool stop;
    uart_m_transport_rx_message_t received;
} rx_queue_item_t;

struct uart_m_transport
{
    uart_m_transport_config_t config;
    uart_m_transport_rx_callback_t rx_callback;
    void *rx_user_context;
    uart_m_transport_engine_t engine;
    QueueHandle_t uart_event_queue;
    QueueHandle_t tx_queue;
    QueueHandle_t rx_delivery_queue;
    EventGroupHandle_t task_events;
    TaskHandle_t rx_task;
    TaskHandle_t tx_task;
    TaskHandle_t dispatch_task;
    portMUX_TYPE state_lock;
    uart_m_transport_stats_t stats;
    bool driver_installed;
    bool stopping;
};

static TickType_t ticks_from_ms(uint32_t milliseconds)
{
    TickType_t ticks;

    if (milliseconds == 0U) {
        return 0U;
    }
    ticks = pdMS_TO_TICKS(milliseconds);
    return (ticks == 0U) ? 1U : ticks;
}

static void stats_add(uart_m_transport_t *transport, uint32_t *counter, uint32_t value)
{
    portENTER_CRITICAL(&transport->state_lock);
    *counter += value;
    portEXIT_CRITICAL(&transport->state_lock);
}

static bool is_stopping(uart_m_transport_t *transport)
{
    bool stopping;

    portENTER_CRITICAL(&transport->state_lock);
    stopping = transport->stopping;
    portEXIT_CRITICAL(&transport->state_lock);
    return stopping;
}

static void add_feed_result(
    uart_m_transport_t *transport,
    const uart_m_transport_feed_result_t *result)
{
    portENTER_CRITICAL(&transport->state_lock);
    transport->stats.rx_frames_valid += result->frames_valid;
    transport->stats.rx_messages_queued += result->messages_accepted;
    transport->stats.rx_bytes_discarded += result->bytes_discarded;
    transport->stats.rx_version_errors += result->version_errors;
    transport->stats.rx_length_errors += result->length_errors;
    transport->stats.rx_crc_errors += result->crc_errors;
    transport->stats.rx_parser_overflow_errors += result->parser_overflow_errors;
    transport->stats.rx_codec_errors += result->codec_errors;
    transport->stats.rx_direction_errors += result->direction_errors;
    transport->stats.rx_schema_only_dropped += result->schema_only_dropped;
    transport->stats.rx_delivery_queue_full += result->delivery_rejected;
    portEXIT_CRITICAL(&transport->state_lock);
}

static bool enqueue_received(
    const uart_m_transport_rx_message_t *message,
    void *user_context)
{
    uart_m_transport_t *transport = (uart_m_transport_t *)user_context;
    rx_queue_item_t item;

    memset(&item, 0, sizeof(item));
    item.received = *message;
    return xQueueSend(transport->rx_delivery_queue, &item, 0U) == pdTRUE;
}

static uart_word_length_t map_data_bits(uint8_t data_bits)
{
    switch (data_bits) {
    case 5U: return UART_DATA_5_BITS;
    case 6U: return UART_DATA_6_BITS;
    case 7U: return UART_DATA_7_BITS;
    default: return UART_DATA_8_BITS;
    }
}

static uart_parity_t map_parity(uart_m_transport_parity_t parity)
{
    switch (parity) {
    case UART_M_TRANSPORT_PARITY_EVEN: return UART_PARITY_EVEN;
    case UART_M_TRANSPORT_PARITY_ODD: return UART_PARITY_ODD;
    default: return UART_PARITY_DISABLE;
    }
}

static uart_stop_bits_t map_stop_bits(uart_m_transport_stop_bits_t stop_bits)
{
    return (stop_bits == UART_M_TRANSPORT_STOP_BITS_2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
}

static uart_hw_flowcontrol_t map_flow_control(uart_m_transport_flow_control_t flow_control)
{
    switch (flow_control) {
    case UART_M_TRANSPORT_FLOW_RTS: return UART_HW_FLOWCTRL_RTS;
    case UART_M_TRANSPORT_FLOW_CTS: return UART_HW_FLOWCTRL_CTS;
    case UART_M_TRANSPORT_FLOW_RTS_CTS: return UART_HW_FLOWCTRL_CTS_RTS;
    default: return UART_HW_FLOWCTRL_DISABLE;
    }
}

static bool size_fits_int(size_t value)
{
    return value <= (size_t)INT_MAX;
}

static bool config_is_valid(const uart_m_transport_config_t *config)
{
    bool rts_required;
    bool cts_required;

    if ((config == NULL) || (config->uart_port < 0) ||
        (config->uart_port >= (int)UART_NUM_MAX) || (config->tx_gpio < 0) ||
        (config->rx_gpio < 0) || (config->tx_gpio == config->rx_gpio) ||
        (config->baud_rate == 0U) || (config->baud_rate > (uint32_t)UART_BITRATE_MAX) ||
        (config->data_bits < 5U) || (config->data_bits > 8U) ||
        (config->parity > UART_M_TRANSPORT_PARITY_ODD) ||
        ((config->stop_bits != UART_M_TRANSPORT_STOP_BITS_1) &&
         (config->stop_bits != UART_M_TRANSPORT_STOP_BITS_2)) ||
        (config->flow_control > UART_M_TRANSPORT_FLOW_RTS_CTS) ||
        ((size_t)config->rx_flow_control_threshold >
         (size_t)UART_HW_FIFO_LEN(config->uart_port)) ||
        (config->driver_rx_buffer_size <= (size_t)UART_HW_FIFO_LEN(config->uart_port)) ||
        ((config->driver_tx_buffer_size != 0U) &&
         (config->driver_tx_buffer_size <= (size_t)UART_HW_FIFO_LEN(config->uart_port))) ||
        !size_fits_int(config->driver_rx_buffer_size) ||
        !size_fits_int(config->driver_tx_buffer_size) ||
        !size_fits_int(config->driver_event_queue_depth) ||
        (config->tx_queue_depth > (size_t)UINT_MAX) ||
        (config->rx_delivery_queue_depth > (size_t)UINT_MAX) ||
        (config->driver_event_queue_depth == 0U) || (config->tx_queue_depth == 0U) ||
        (config->rx_delivery_queue_depth == 0U) || (config->rx_chunk_size == 0U) ||
        (config->rx_chunk_size > UART_M_TRANSPORT_MAX_RX_CHUNK) ||
        (config->uart_event_wait_ms == 0U) || (config->frame_assembly_timeout_ms == 0U) ||
        (config->uart_event_wait_ms > config->frame_assembly_timeout_ms) ||
        (config->tx_complete_timeout_ms == 0U) || (config->shutdown_timeout_ms == 0U) ||
        (config->rx_task_stack_size == 0U) || (config->tx_task_stack_size == 0U) ||
        (config->dispatch_task_stack_size == 0U) || (config->rx_task_priority == 0U) ||
        (config->tx_task_priority == 0U) || (config->dispatch_task_priority == 0U) ||
        (config->rx_task_priority >= configMAX_PRIORITIES) ||
        (config->tx_task_priority >= configMAX_PRIORITIES) ||
        (config->dispatch_task_priority >= configMAX_PRIORITIES)) {
        return false;
    }

    rts_required = (config->flow_control == UART_M_TRANSPORT_FLOW_RTS) ||
                   (config->flow_control == UART_M_TRANSPORT_FLOW_RTS_CTS);
    cts_required = (config->flow_control == UART_M_TRANSPORT_FLOW_CTS) ||
                   (config->flow_control == UART_M_TRANSPORT_FLOW_RTS_CTS);
    return (!rts_required || (config->rts_gpio >= 0)) &&
           (!cts_required || (config->cts_gpio >= 0));
}

static void handle_data_event(uart_m_transport_t *transport, size_t event_size)
{
    uint8_t chunk[UART_M_TRANSPORT_MAX_RX_CHUNK];
    size_t remaining = event_size;

    while ((remaining != 0U) && !is_stopping(transport)) {
        size_t wanted = (remaining < transport->config.rx_chunk_size) ?
                            remaining : transport->config.rx_chunk_size;
        int read_count = uart_read_bytes((uart_port_t)transport->config.uart_port,
                                         chunk, (uint32_t)wanted, 0U);
        if (read_count <= 0) {
            stats_add(transport, &transport->stats.uart_read_errors, 1U);
            break;
        }
        stats_add(transport, &transport->stats.rx_bytes, (uint32_t)read_count);
        uart_m_transport_feed_result_t result;
        if (uart_m_transport_engine_feed(&transport->engine, chunk, (size_t)read_count,
                                         esp_timer_get_time(), &result) == UART_M_TRANSPORT_OK) {
            add_feed_result(transport, &result);
        } else {
            stats_add(transport, &transport->stats.uart_frame_errors, 1U);
        }
        remaining -= (size_t)read_count;
    }
}

static void rx_task_entry(void *argument)
{
    uart_m_transport_t *transport = (uart_m_transport_t *)argument;
    const TickType_t wait_ticks = ticks_from_ms(transport->config.uart_event_wait_ms);
    const TickType_t assembly_ticks = ticks_from_ms(transport->config.frame_assembly_timeout_ms);
    TickType_t last_data_tick = xTaskGetTickCount();
    uart_event_t event;

    while (!is_stopping(transport)) {
        if (xQueueReceive(transport->uart_event_queue, &event, wait_ticks) == pdTRUE) {
            if ((event.type == UART_EVENT_MAX) && is_stopping(transport)) {
                break;
            }
            switch (event.type) {
            case UART_DATA:
                handle_data_event(transport, event.size);
                if (uart_m_transport_engine_pending_bytes(&transport->engine) != 0U) {
                    last_data_tick = xTaskGetTickCount();
                }
                break;
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL: {
                size_t discarded = uart_m_transport_engine_reset_partial(&transport->engine);
                stats_add(transport, &transport->stats.uart_rx_overflow_errors, 1U);
                stats_add(transport, &transport->stats.rx_bytes_discarded, (uint32_t)discarded);
                (void)uart_flush_input((uart_port_t)transport->config.uart_port);
                (void)xQueueReset(transport->uart_event_queue);
                break;
            }
            case UART_FRAME_ERR:
            case UART_PARITY_ERR:
            case UART_BREAK:
                stats_add(transport, &transport->stats.uart_frame_errors, 1U);
                break;
            default:
                break;
            }
        }

        if (uart_m_transport_engine_pending_bytes(&transport->engine) != 0U) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_data_tick) >= assembly_ticks) {
                size_t discarded = uart_m_transport_engine_reset_partial(&transport->engine);
                stats_add(transport, &transport->stats.rx_bytes_discarded, (uint32_t)discarded);
                stats_add(transport, &transport->stats.frame_assembly_timeouts, 1U);
            }
        }
    }

    xEventGroupSetBits(transport->task_events, UART_M_RX_TASK_DONE);
    vTaskDelete(NULL);
}

static void tx_task_entry(void *argument)
{
    uart_m_transport_t *transport = (uart_m_transport_t *)argument;
    tx_queue_item_t item;

    while (xQueueReceive(transport->tx_queue, &item, portMAX_DELAY) == pdTRUE) {
        uint8_t frame[UART_M_MAX_FRAME_SIZE];
        size_t frame_length = 0U;
        uint16_t sequence = 0U;
        uart_m_transport_status_t status;

        if (item.stop) {
            break;
        }
        status = uart_m_transport_engine_prepare_tx(&transport->engine, &item.message,
                                                    frame, sizeof(frame), &frame_length,
                                                    &sequence);
        if (status != UART_M_TRANSPORT_OK) {
            stats_add(transport, &transport->stats.tx_encode_errors, 1U);
            continue;
        }
        (void)sequence;
        if (uart_write_bytes((uart_port_t)transport->config.uart_port,
                             frame, frame_length) != (int)frame_length) {
            stats_add(transport, &transport->stats.tx_write_errors, 1U);
            continue;
        }
        if (uart_wait_tx_done((uart_port_t)transport->config.uart_port,
                              ticks_from_ms(transport->config.tx_complete_timeout_ms)) != ESP_OK) {
            stats_add(transport, &transport->stats.tx_complete_timeouts, 1U);
            continue;
        }
        stats_add(transport, &transport->stats.tx_frames_completed, 1U);
    }

    xEventGroupSetBits(transport->task_events, UART_M_TX_TASK_DONE);
    vTaskDelete(NULL);
}

static void dispatch_task_entry(void *argument)
{
    uart_m_transport_t *transport = (uart_m_transport_t *)argument;
    rx_queue_item_t item;

    while (xQueueReceive(transport->rx_delivery_queue, &item, portMAX_DELAY) == pdTRUE) {
        if (item.stop) {
            break;
        }
        transport->rx_callback(&item.received, transport->rx_user_context);
        stats_add(transport, &transport->stats.rx_messages_delivered, 1U);
    }

    xEventGroupSetBits(transport->task_events, UART_M_DISPATCH_TASK_DONE);
    vTaskDelete(NULL);
}

static void request_task_stop(uart_m_transport_t *transport)
{
    tx_queue_item_t tx_stop;
    rx_queue_item_t rx_stop;
    uart_event_t uart_stop;

    memset(&tx_stop, 0, sizeof(tx_stop));
    memset(&rx_stop, 0, sizeof(rx_stop));
    memset(&uart_stop, 0, sizeof(uart_stop));
    tx_stop.stop = true;
    rx_stop.stop = true;
    uart_stop.type = UART_EVENT_MAX;

    if (transport->tx_queue != NULL) {
        (void)xQueueReset(transport->tx_queue);
        (void)xQueueSendToFront(transport->tx_queue, &tx_stop, 0U);
    }
    if (transport->rx_delivery_queue != NULL) {
        (void)xQueueReset(transport->rx_delivery_queue);
        (void)xQueueSendToFront(transport->rx_delivery_queue, &rx_stop, 0U);
    }
    if (transport->uart_event_queue != NULL) {
        (void)xQueueReset(transport->uart_event_queue);
        (void)xQueueSendToFront(transport->uart_event_queue, &uart_stop, 0U);
    }
}

static uint32_t stop_tasks(uart_m_transport_t *transport)
{
    EventBits_t expected = 0U;
    EventBits_t completed;
    uint32_t forced = 0U;

    if (transport->rx_task != NULL) expected |= UART_M_RX_TASK_DONE;
    if (transport->tx_task != NULL) expected |= UART_M_TX_TASK_DONE;
    if (transport->dispatch_task != NULL) expected |= UART_M_DISPATCH_TASK_DONE;
    if (expected == 0U) {
        return 0U;
    }

    request_task_stop(transport);
    completed = xEventGroupWaitBits(transport->task_events, expected, pdFALSE, pdTRUE,
                                    ticks_from_ms(transport->config.shutdown_timeout_ms));
    if ((completed & expected) != expected) {
        if (((completed & UART_M_RX_TASK_DONE) == 0U) && (transport->rx_task != NULL)) {
            vTaskDelete(transport->rx_task);
            forced++;
        }
        if (((completed & UART_M_TX_TASK_DONE) == 0U) && (transport->tx_task != NULL)) {
            vTaskDelete(transport->tx_task);
            forced++;
        }
        if (((completed & UART_M_DISPATCH_TASK_DONE) == 0U) &&
            (transport->dispatch_task != NULL)) {
            vTaskDelete(transport->dispatch_task);
            forced++;
        }
    }
    transport->rx_task = NULL;
    transport->tx_task = NULL;
    transport->dispatch_task = NULL;
    return forced;
}

static void release_resources(uart_m_transport_t *transport)
{
    if (transport->driver_installed) {
        (void)uart_driver_delete((uart_port_t)transport->config.uart_port);
        transport->driver_installed = false;
    }
    if (transport->tx_queue != NULL) {
        vQueueDelete(transport->tx_queue);
    }
    if (transport->rx_delivery_queue != NULL) {
        vQueueDelete(transport->rx_delivery_queue);
    }
    if (transport->task_events != NULL) {
        vEventGroupDelete(transport->task_events);
    }
}

uart_m_transport_status_t uart_m_transport_init(
    uart_m_transport_t **transport,
    const uart_m_transport_config_t *config,
    uart_m_transport_rx_callback_t rx_callback,
    void *rx_user_context)
{
    uart_m_transport_t *instance;
    uart_config_t uart_config;
    BaseType_t task_result;

    if ((transport == NULL) || (*transport != NULL) || (rx_callback == NULL) ||
        !config_is_valid(config)) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }

    instance = (uart_m_transport_t *)calloc(1U, sizeof(*instance));
    if (instance == NULL) {
        return UART_M_TRANSPORT_NO_CAPACITY;
    }
    instance->config = *config;
    instance->rx_callback = rx_callback;
    instance->rx_user_context = rx_user_context;
    portMUX_INITIALIZE(&instance->state_lock);

    if (uart_m_transport_engine_init(&instance->engine, config->first_tx_sequence,
                                     enqueue_received, instance) != UART_M_TRANSPORT_OK) {
        free(instance);
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }

    instance->tx_queue = xQueueCreate((UBaseType_t)config->tx_queue_depth,
                                      (UBaseType_t)sizeof(tx_queue_item_t));
    instance->rx_delivery_queue = xQueueCreate(
        (UBaseType_t)config->rx_delivery_queue_depth,
        (UBaseType_t)sizeof(rx_queue_item_t));
    instance->task_events = xEventGroupCreate();
    if ((instance->tx_queue == NULL) || (instance->rx_delivery_queue == NULL) ||
        (instance->task_events == NULL)) {
        release_resources(instance);
        free(instance);
        return UART_M_TRANSPORT_NO_CAPACITY;
    }

    memset(&uart_config, 0, sizeof(uart_config));
    uart_config.baud_rate = (int)config->baud_rate;
    uart_config.data_bits = map_data_bits(config->data_bits);
    uart_config.parity = map_parity(config->parity);
    uart_config.stop_bits = map_stop_bits(config->stop_bits);
    uart_config.flow_ctrl = map_flow_control(config->flow_control);
    uart_config.rx_flow_ctrl_thresh = config->rx_flow_control_threshold;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    if ((uart_param_config((uart_port_t)config->uart_port, &uart_config) != ESP_OK) ||
        (uart_set_pin((uart_port_t)config->uart_port, config->tx_gpio, config->rx_gpio,
                      config->rts_gpio, config->cts_gpio) != ESP_OK) ||
        (uart_driver_install((uart_port_t)config->uart_port,
                             (int)config->driver_rx_buffer_size,
                             (int)config->driver_tx_buffer_size,
                             (int)config->driver_event_queue_depth,
                             &instance->uart_event_queue, 0) != ESP_OK)) {
        release_resources(instance);
        free(instance);
        return UART_M_TRANSPORT_DRIVER_ERROR;
    }
    instance->driver_installed = true;

    task_result = xTaskCreate(rx_task_entry, "uart_m_rx", config->rx_task_stack_size,
                              instance, config->rx_task_priority, &instance->rx_task);
    if (task_result == pdPASS) {
        task_result = xTaskCreate(tx_task_entry, "uart_m_tx", config->tx_task_stack_size,
                                  instance, config->tx_task_priority, &instance->tx_task);
    }
    if (task_result == pdPASS) {
        task_result = xTaskCreate(dispatch_task_entry, "uart_m_dispatch",
                                  config->dispatch_task_stack_size, instance,
                                  config->dispatch_task_priority, &instance->dispatch_task);
    }
    if (task_result != pdPASS) {
        portENTER_CRITICAL(&instance->state_lock);
        instance->stopping = true;
        portEXIT_CRITICAL(&instance->state_lock);
        (void)stop_tasks(instance);
        release_resources(instance);
        free(instance);
        return UART_M_TRANSPORT_NO_CAPACITY;
    }

    *transport = instance;
    return UART_M_TRANSPORT_OK;
}

uart_m_transport_status_t uart_m_transport_send(
    uart_m_transport_t *transport,
    const uart_m_message_t *message,
    uint32_t queue_wait_ms)
{
    uint8_t validation_payload[UART_M_MAX_PAYLOAD_SIZE];
    size_t validation_length = 0U;
    tx_queue_item_t item;

    if ((transport == NULL) || (message == NULL)) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    if (is_stopping(transport)) {
        return UART_M_TRANSPORT_NOT_READY;
    }
    if ((uart_m_codec_validate_direction((uint8_t)message->type,
                                         UART_M_DIRECTION_ESP32_TX) != UART_M_CODEC_OK) ||
        (uart_m_codec_encode(message, validation_payload, sizeof(validation_payload),
                             &validation_length) != UART_M_CODEC_OK)) {
        stats_add(transport, &transport->stats.tx_encode_errors, 1U);
        return UART_M_TRANSPORT_CODEC_ERROR;
    }
    (void)validation_length;

    memset(&item, 0, sizeof(item));
    item.message = *message;
    if (xQueueSend(transport->tx_queue, &item, ticks_from_ms(queue_wait_ms)) != pdTRUE) {
        stats_add(transport, &transport->stats.tx_queue_full, 1U);
        return UART_M_TRANSPORT_NO_CAPACITY;
    }
    stats_add(transport, &transport->stats.tx_messages_queued, 1U);
    return UART_M_TRANSPORT_OK;
}

void uart_m_transport_get_stats(
    const uart_m_transport_t *transport,
    uart_m_transport_stats_t *stats)
{
    uart_m_transport_t *mutable_transport = (uart_m_transport_t *)transport;

    if ((transport == NULL) || (stats == NULL)) {
        return;
    }
    portENTER_CRITICAL(&mutable_transport->state_lock);
    *stats = transport->stats;
    portEXIT_CRITICAL(&mutable_transport->state_lock);
}

uart_m_transport_status_t uart_m_transport_deinit(uart_m_transport_t **transport)
{
    uart_m_transport_t *instance;
    uint32_t forced;

    if ((transport == NULL) || (*transport == NULL)) {
        return UART_M_TRANSPORT_INVALID_ARGUMENT;
    }
    instance = *transport;
    portENTER_CRITICAL(&instance->state_lock);
    instance->stopping = true;
    portEXIT_CRITICAL(&instance->state_lock);

    forced = stop_tasks(instance);
    if (forced != 0U) {
        stats_add(instance, &instance->stats.forced_task_stops, forced);
    }
    release_resources(instance);
    free(instance);
    *transport = NULL;
    return (forced == 0U) ? UART_M_TRANSPORT_OK : UART_M_TRANSPORT_TIMEOUT;
}
