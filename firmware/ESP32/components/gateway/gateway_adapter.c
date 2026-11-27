#include "gateway_adapter.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gateway_core.h"

#define GATEWAY_ADAPTER_DEFAULT_QUEUE_DEPTH 12U
#define GATEWAY_ADAPTER_DEFAULT_TASK_STACK 6144U
#define GATEWAY_ADAPTER_DEFAULT_TASK_PRIORITY 5U
#define GATEWAY_ADAPTER_DEFAULT_TICK_MS 100U
#define GATEWAY_ADAPTER_STOPPED_BIT BIT0

typedef enum {
    ADAPTER_EVENT_STOP = 0,
    ADAPTER_EVENT_BLE,
    ADAPTER_EVENT_UART,
} adapter_event_type_t;

typedef struct {
    adapter_event_type_t type;
    uint64_t received_at_ms;
    union {
        ble_gatt_server_event_t ble;
        uart_m_transport_rx_message_t uart;
    } data;
} adapter_event_t;

typedef enum {
    STATUS_PENDING_NONE = 0,
    STATUS_PENDING_CONNECTION,
    STATUS_PENDING_SECURITY,
    STATUS_PENDING_SUBSCRIPTION,
    STATUS_PENDING_MTU,
} status_pending_reason_t;

struct gateway_adapter {
    bool started;
    gateway_adapter_config_t config;
    gateway_core_t core;
    QueueHandle_t queue;
    EventGroupHandle_t lifecycle;
    TaskHandle_t task;
    uint32_t connection_generation;
    uint16_t connection_handle;
    bool connected;
    bool encrypted;
    bool authenticated;
    bool bonded;
    bool gateway_status_subscribed;
    bool vehicle_data_subscribed;
    uint16_t mtu;
    uint32_t domain_boot_id;
    uint8_t domain_state;
    bool status_snapshot_initialized;
    bool status_snapshot_domain_link_up;
    uint32_t status_snapshot_domain_boot_id;
    uint8_t status_snapshot_domain_state;
    bool gateway_status_notify_pending;
    status_pending_reason_t last_pending_reason;
    bool app_state_seen;
    bool last_app_active;
    atomic_uint_least32_t ble_events_enqueued;
    atomic_uint_least32_t uart_events_enqueued;
    atomic_uint_least32_t events_processed;
    atomic_uint_least32_t queue_full;
    atomic_uint_least32_t invalid_callbacks;
    atomic_uint_least32_t core_rejections;
    atomic_uint_least32_t app_commands_blocked;
    atomic_uint_least32_t disconnects_processed;
    atomic_uint_least32_t domain_boot_changes;
    atomic_uint_least32_t gateway_status_notify_deferred;
    atomic_uint_least32_t gateway_status_notify_api_accepted;
    atomic_uint_least32_t gateway_status_notify_failures;
    atomic_uint_least32_t app_state_events_processed;
    atomic_uint_least32_t stale_ble_events;
};

static const char *TAG = "gateway_adapter";
static gateway_adapter_t state;

static uint64_t monotonic_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / INT64_C(1000));
}

static void increment(atomic_uint_least32_t *value)
{
    (void)atomic_fetch_add_explicit(value, 1U, memory_order_relaxed);
}

static void write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8U);
    out[2] = (uint8_t)(value >> 16U);
    out[3] = (uint8_t)(value >> 24U);
}

static void mark_gateway_status_pending(gateway_adapter_t *adapter)
{
    if (!adapter->gateway_status_notify_pending) {
        adapter->gateway_status_notify_pending = true;
        increment(&adapter->gateway_status_notify_deferred);
    }
}

static status_pending_reason_t gateway_status_pending_reason(
    const gateway_adapter_t *adapter)
{
    if (!adapter->connected || adapter->connection_handle == UINT16_MAX ||
        adapter->connection_generation == 0U) {
        return STATUS_PENDING_CONNECTION;
    }
    if (!adapter->encrypted || !adapter->bonded) {
        return STATUS_PENDING_SECURITY;
    }
    if (!adapter->gateway_status_subscribed) {
        return STATUS_PENDING_SUBSCRIPTION;
    }
    if (adapter->mtu < BLE_GATT_MIN_APP_MTU) {
        return STATUS_PENDING_MTU;
    }
    return STATUS_PENDING_NONE;
}

static const char *pending_reason_name(status_pending_reason_t reason)
{
    switch (reason) {
    case STATUS_PENDING_CONNECTION: return "CONNECTION";
    case STATUS_PENDING_SECURITY: return "SECURITY";
    case STATUS_PENDING_SUBSCRIPTION: return "SUBSCRIPTION";
    case STATUS_PENDING_MTU: return "MTU";
    default: return "NONE";
    }
}

static void try_gateway_status_notify(gateway_adapter_t *adapter)
{
    status_pending_reason_t reason;
    esp_err_t result;

    if (!adapter->gateway_status_notify_pending) {
        return;
    }
    reason = gateway_status_pending_reason(adapter);
    if (reason != STATUS_PENDING_NONE) {
        if (reason != adapter->last_pending_reason) {
            (void)pending_reason_name(reason);
            ESP_LOGI(TAG, "Gateway Status pending: connection=%u generation=%" PRIu32
                     " reason=%s mtu=%u",
                     adapter->connection_handle, adapter->connection_generation,
                     pending_reason_name(reason), adapter->mtu);
            adapter->last_pending_reason = reason;
        }
        return;
    }

    result = ble_gatt_server_send_gateway_status(BLE_GATT_SEND_NOTIFY);
    if (result == ESP_OK) {
        adapter->gateway_status_notify_pending = false;
        adapter->last_pending_reason = STATUS_PENDING_NONE;
        increment(&adapter->gateway_status_notify_api_accepted);
        ESP_LOGI(TAG, "Gateway Status Notify API accepted: connection=%u generation=%" PRIu32
                 "; ATT delivery unconfirmed",
                 adapter->connection_handle, adapter->connection_generation);
        return;
    }

    increment(&adapter->gateway_status_notify_failures);
    ESP_LOGW(TAG, "Gateway Status Notify API failed: connection=%u generation=%" PRIu32
             " rc=%s; pending retained",
             adapter->connection_handle, adapter->connection_generation,
             esp_err_to_name(result));
}

static void publish_safe_gateway_status(gateway_adapter_t *adapter, bool try_notify)
{
    uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH] = {0};
    esp_err_t result;
    bool domain_link_up = gateway_core_is_domain_link_up(&adapter->core);
    bool changed = !adapter->status_snapshot_initialized ||
                   adapter->status_snapshot_domain_link_up != domain_link_up ||
                   adapter->status_snapshot_domain_boot_id != adapter->domain_boot_id ||
                   adapter->status_snapshot_domain_state != adapter->domain_state;

    status[0] = BLE_GATT_PROTOCOL_VERSION;
    status[1] = UINT8_MAX; /* Registration remains unknown without P6 evidence. */
    status[2] = domain_link_up ? 1U : 0U;
    status[3] = 0U; /* SessionReady is never synthesized in safe mode. */
    write_u32_le(&status[16], adapter->domain_boot_id);
    status[20] = domain_link_up ? adapter->domain_state : UINT8_MAX;
    result = ble_gatt_server_set_gateway_status(status);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Gateway Status snapshot update failed; rc=%s",
                 esp_err_to_name(result));
        return;
    }
    if (changed) {
        adapter->status_snapshot_initialized = true;
        adapter->status_snapshot_domain_link_up = domain_link_up;
        adapter->status_snapshot_domain_boot_id = adapter->domain_boot_id;
        adapter->status_snapshot_domain_state = adapter->domain_state;
        mark_gateway_status_pending(adapter);
    }
    if (try_notify) {
        try_gateway_status_notify(adapter);
    }
}

static bool ble_event_is_current(const gateway_adapter_t *adapter,
                                 const ble_gatt_server_event_t *event)
{
    return adapter->connected &&
           event->connection_handle == adapter->connection_handle &&
           event->connection_generation == adapter->connection_generation;
}

static bool require_current_ble_event(gateway_adapter_t *adapter,
                                      const ble_gatt_server_event_t *event)
{
    if (ble_event_is_current(adapter, event)) {
        return true;
    }
    increment(&adapter->stale_ble_events);
    ESP_LOGW(TAG, "Ignoring stale BLE event: type=%d connection=%u generation=%" PRIu32,
             (int)event->type, event->connection_handle,
             event->connection_generation);
    return false;
}

static void record_core_result(gateway_adapter_t *adapter,
                               gateway_core_status_t status)
{
    if ((status != GATEWAY_CORE_OK) && (status != GATEWAY_CORE_NO_EVENT) &&
        (status != GATEWAY_CORE_ROUTING_HOLD)) {
        increment(&adapter->core_rejections);
    }
}

static void process_core_event(gateway_adapter_t *adapter,
                               gateway_core_status_t enqueue_status,
                               uint64_t now_ms)
{
    if (enqueue_status != GATEWAY_CORE_OK) {
        record_core_result(adapter, enqueue_status);
        return;
    }
    record_core_result(adapter, gateway_core_process_next(&adapter->core, now_ms));
}

static void process_ble(gateway_adapter_t *adapter,
                        const ble_gatt_server_event_t *event,
                        uint64_t received_at_ms,
                        uint64_t now_ms)
{
    switch (event->type) {
    case BLE_GATT_EVENT_CONNECTED:
        if (event->connection_generation == 0U) {
            increment(&adapter->stale_ble_events);
            ESP_LOGW(TAG, "Ignoring BLE connect event with zero generation");
            break;
        }
        adapter->connection_generation = event->connection_generation;
        adapter->connection_handle = event->connection_handle;
        adapter->connected = true;
        adapter->encrypted = false;
        adapter->authenticated = false;
        adapter->bonded = false;
        adapter->gateway_status_subscribed = false;
        adapter->vehicle_data_subscribed = false;
        adapter->mtu = 23U;
        adapter->app_state_seen = false;
        mark_gateway_status_pending(adapter);
        ESP_LOGI(TAG, "BLE link connected; generation=%" PRIu32 "; P6 authorization absent",
                 adapter->connection_generation);
        break;
    case BLE_GATT_EVENT_DISCONNECTED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        adapter->connected = false;
        adapter->connection_handle = UINT16_MAX;
        adapter->encrypted = false;
        adapter->authenticated = false;
        adapter->bonded = false;
        adapter->gateway_status_subscribed = false;
        adapter->vehicle_data_subscribed = false;
        adapter->mtu = 23U;
        adapter->gateway_status_notify_pending = false;
        adapter->last_pending_reason = STATUS_PENDING_NONE;
        adapter->app_state_seen = false;
        increment(&adapter->disconnects_processed);
        if (adapter->core.has_context) {
            process_core_event(adapter,
                gateway_core_enqueue_disconnect(&adapter->core,
                    adapter->connection_generation, received_at_ms), now_ms);
        }
        ESP_LOGI(TAG, "BLE link disconnected; no command replay");
        break;
    case BLE_GATT_EVENT_SECURITY_CHANGED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        adapter->encrypted = event->data.security.encrypted;
        adapter->authenticated = event->data.security.authenticated;
        adapter->bonded = event->data.security.bonded;
        ESP_LOGI(TAG, "BLE link security: encrypted=%d authenticated=%d bonded=%d; not P6 authorization",
                 adapter->encrypted, adapter->authenticated, adapter->bonded);
        try_gateway_status_notify(adapter);
        break;
    case BLE_GATT_EVENT_SUBSCRIPTION_CHANGED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        if (adapter->gateway_status_subscribed &&
            !event->data.subscription.gateway_status_notify) {
            mark_gateway_status_pending(adapter);
        }
        adapter->gateway_status_subscribed =
            event->data.subscription.gateway_status_notify;
        adapter->vehicle_data_subscribed =
            event->data.subscription.vehicle_data_notify;
        ESP_LOGI(TAG, "GatewayTask subscription: connection=%u generation=%" PRIu32
                 " gateway_status=%d vehicle_data=%d",
                 event->connection_handle, event->connection_generation,
                 adapter->gateway_status_subscribed,
                 adapter->vehicle_data_subscribed);
        try_gateway_status_notify(adapter);
        break;
    case BLE_GATT_EVENT_MTU_CHANGED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        adapter->mtu = event->data.mtu.mtu;
        ESP_LOGI(TAG, "BLE MTU=%u connection=%u", adapter->mtu,
                  event->connection_handle);
        try_gateway_status_notify(adapter);
        break;
    case BLE_GATT_EVENT_APP_COMMAND:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        increment(&adapter->app_commands_blocked);
        process_core_event(adapter,
            gateway_core_enqueue_ble_command(&adapter->core,
                event->data.app_command.message_type,
                event->data.app_command.payload,
                event->data.app_command.payload_length,
                adapter->connection_generation, received_at_ms), now_ms);
        ESP_LOGW(TAG, "App Command held: production UART routing and P6 authorization are disabled");
        break;
    case BLE_GATT_EVENT_APP_STATE:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        increment(&adapter->app_state_events_processed);
        if (!adapter->app_state_seen ||
            adapter->last_app_active != event->data.app_state.active) {
            ESP_LOGI(TAG, "GatewayTask App State: connection=%u active=%d; P6 handling HOLD",
                     event->connection_handle, event->data.app_state.active);
        }
        adapter->app_state_seen = true;
        adapter->last_app_active = event->data.app_state.active;
        /* P6 consumes App State later; no Registration/Session fact is inferred. */
        break;
    default:
        increment(&adapter->invalid_callbacks);
        break;
    }
}

static void process_uart(gateway_adapter_t *adapter,
                         const uart_m_transport_rx_message_t *message,
                         uint64_t now_ms)
{
    if (message->message.type == UART_M_TYPE_DOMAIN_ALIVE) {
        if ((adapter->domain_boot_id != 0U) &&
            (adapter->domain_boot_id != message->message.body.domain_alive.domain_boot_id)) {
            increment(&adapter->domain_boot_changes);
        }
        adapter->domain_boot_id = message->message.body.domain_alive.domain_boot_id;
        adapter->domain_state = (uint8_t)message->message.body.domain_alive.domain_state;
    }
    process_core_event(adapter,
        gateway_core_enqueue_uart_message(&adapter->core, &message->message,
                                           message->received_at_us >= 0
                                               ? (uint64_t)message->received_at_us / 1000U
                                               : now_ms),
        now_ms);
    if (message->message.type == UART_M_TYPE_DOMAIN_ALIVE) {
        publish_safe_gateway_status(adapter, true);
    }
}

#ifdef GATEWAY_ADAPTER_HOST_TEST
static bool process_one(gateway_adapter_t *adapter, uint64_t now_ms,
                        TickType_t wait_ticks)
{
    adapter_event_t event;

    if ((adapter == NULL) || !adapter->started ||
        (xQueueReceive(adapter->queue, &event, wait_ticks) != pdTRUE)) {
        if ((adapter != NULL) && adapter->started && (wait_ticks != 0U)) {
            process_core_event(adapter,
                gateway_core_enqueue_tick(&adapter->core, now_ms), now_ms);
            publish_safe_gateway_status(adapter, true);
        }
        return false;
    }
    if (event.type == ADAPTER_EVENT_STOP) {
        return false;
    }
    increment(&adapter->events_processed);
    if (event.type == ADAPTER_EVENT_BLE) {
        process_ble(adapter, &event.data.ble, event.received_at_ms, now_ms);
    } else if (event.type == ADAPTER_EVENT_UART) {
        process_uart(adapter, &event.data.uart, now_ms);
    } else {
        increment(&adapter->invalid_callbacks);
    }
    return true;
}
#endif

static void gateway_task(void *argument)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)argument;
    adapter_event_t event;

    while (true) {
        if (xQueueReceive(adapter->queue, &event,
                          pdMS_TO_TICKS(adapter->config.tick_period_ms)) != pdTRUE) {
            uint64_t now_ms = monotonic_ms();
            process_core_event(adapter,
                gateway_core_enqueue_tick(&adapter->core, now_ms), now_ms);
            publish_safe_gateway_status(adapter, true);
            continue;
        }
        if (event.type == ADAPTER_EVENT_STOP) {
            break;
        }
        increment(&adapter->events_processed);
        if (event.type == ADAPTER_EVENT_BLE) {
            process_ble(adapter, &event.data.ble, event.received_at_ms,
                        monotonic_ms());
        } else if (event.type == ADAPTER_EVENT_UART) {
            process_uart(adapter, &event.data.uart, monotonic_ms());
        } else {
            increment(&adapter->invalid_callbacks);
        }
    }
    xEventGroupSetBits(adapter->lifecycle, GATEWAY_ADAPTER_STOPPED_BIT);
    vTaskDelete(NULL);
}

gateway_adapter_config_t gateway_adapter_safe_config(void)
{
    gateway_adapter_config_t config = {
        .queue_depth = GATEWAY_ADAPTER_DEFAULT_QUEUE_DEPTH,
        .task_stack_size = GATEWAY_ADAPTER_DEFAULT_TASK_STACK,
        .task_priority = GATEWAY_ADAPTER_DEFAULT_TASK_PRIORITY,
        .tick_period_ms = GATEWAY_ADAPTER_DEFAULT_TICK_MS,
    };
    return config;
}

esp_err_t gateway_adapter_start(gateway_adapter_t **out_adapter,
                                const gateway_adapter_config_t *config)
{
    gateway_core_config_t core_config;

    if ((out_adapter == NULL) || (*out_adapter != NULL) || (config == NULL) ||
        (config->queue_depth == 0U) ||
        (config->queue_depth > GATEWAY_ADAPTER_MAX_QUEUE_DEPTH) ||
        (config->task_stack_size < 4096U) || (config->task_priority == 0U) ||
        (config->tick_period_ms == 0U) || state.started) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&state, 0, sizeof(state));
    state.config = *config;
    state.domain_state = UINT8_MAX;
    state.mtu = 23U;
    state.connection_handle = UINT16_MAX;
    core_config = gateway_core_isolated_mock_config();
    core_config.mock_ports.observe_uart_candidate = NULL;
    core_config.mock_ports.observe_ble_candidate = NULL;
    if (gateway_core_init(&state.core, &core_config) != GATEWAY_CORE_OK) {
        return ESP_FAIL;
    }
    state.queue = xQueueCreate((UBaseType_t)config->queue_depth,
                               (UBaseType_t)sizeof(adapter_event_t));
    state.lifecycle = xEventGroupCreate();
    if ((state.queue == NULL) || (state.lifecycle == NULL) ||
        (xTaskCreate(gateway_task, "gateway", config->task_stack_size, &state,
                     config->task_priority, &state.task) != pdPASS)) {
        if (state.queue != NULL) {
            vQueueDelete(state.queue);
        }
        if (state.lifecycle != NULL) {
            vEventGroupDelete(state.lifecycle);
        }
        memset(&state, 0, sizeof(state));
        return ESP_ERR_NO_MEM;
    }
    state.started = true;
    *out_adapter = &state;
    publish_safe_gateway_status(&state, false);
    ESP_LOGI(TAG, "GatewayTask queue=%u events (%u bytes each), stack=%" PRIu32 " bytes",
             (unsigned)config->queue_depth, (unsigned)sizeof(adapter_event_t),
             config->task_stack_size);
    ESP_LOGW(TAG, "Safe Integration Mode: UART TX, P6 Context, Query completion and auto Unlock disabled");
    return ESP_OK;
}

esp_err_t gateway_adapter_stop(gateway_adapter_t **adapter)
{
    adapter_event_t stop = {.type = ADAPTER_EVENT_STOP};
    EventBits_t bits;
    gateway_adapter_t *instance;

    if ((adapter == NULL) || (*adapter != &state) || !state.started) {
        return ESP_ERR_INVALID_ARG;
    }
    instance = *adapter;
    if (xQueueSendToFront(instance->queue, &stop, 0U) != pdTRUE) {
        (void)xQueueReset(instance->queue);
        if (xQueueSendToFront(instance->queue, &stop, 0U) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
    }
    bits = xEventGroupWaitBits(instance->lifecycle, GATEWAY_ADAPTER_STOPPED_BIT,
                               pdFALSE, pdTRUE, pdMS_TO_TICKS(1000U));
    if ((bits & GATEWAY_ADAPTER_STOPPED_BIT) == 0U) {
        return ESP_ERR_TIMEOUT;
    }
    instance->started = false;
    vQueueDelete(instance->queue);
    vEventGroupDelete(instance->lifecycle);
    gateway_core_reset(&instance->core);
    memset(instance, 0, sizeof(*instance));
    *adapter = NULL;
    return ESP_OK;
}

static void enqueue(adapter_event_t *queued, gateway_adapter_t *adapter,
                    atomic_uint_least32_t *accepted_counter)
{
    if ((adapter == NULL) || !adapter->started ||
        (xQueueSend(adapter->queue, queued, 0U) != pdTRUE)) {
        if (adapter != NULL) {
            increment(&adapter->queue_full);
        }
        return;
    }
    increment(accepted_counter);
}

void gateway_adapter_on_ble_event(const ble_gatt_server_event_t *event,
                                  void *context)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)context;
    adapter_event_t queued;

    if ((adapter == NULL) || (event == NULL)) {
        if (adapter != NULL) {
            increment(&adapter->invalid_callbacks);
        }
        return;
    }
    memset(&queued, 0, sizeof(queued));
    queued.type = ADAPTER_EVENT_BLE;
    queued.received_at_ms = monotonic_ms();
    queued.data.ble = *event;
    enqueue(&queued, adapter, &adapter->ble_events_enqueued);
}

void gateway_adapter_on_uart_message(const uart_m_transport_rx_message_t *message,
                                     void *context)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)context;
    adapter_event_t queued;

    if ((adapter == NULL) || (message == NULL)) {
        if (adapter != NULL) {
            increment(&adapter->invalid_callbacks);
        }
        return;
    }
    memset(&queued, 0, sizeof(queued));
    queued.type = ADAPTER_EVENT_UART;
    queued.received_at_ms = message->received_at_us >= 0
                                ? (uint64_t)message->received_at_us / 1000U
                                : monotonic_ms();
    queued.data.uart = *message;
    enqueue(&queued, adapter, &adapter->uart_events_enqueued);
}

void gateway_adapter_get_stats(const gateway_adapter_t *adapter,
                               gateway_adapter_stats_t *out_stats)
{
    if ((adapter == NULL) || (out_stats == NULL)) {
        return;
    }
    out_stats->ble_events_enqueued = atomic_load(&adapter->ble_events_enqueued);
    out_stats->uart_events_enqueued = atomic_load(&adapter->uart_events_enqueued);
    out_stats->events_processed = atomic_load(&adapter->events_processed);
    out_stats->queue_full = atomic_load(&adapter->queue_full);
    out_stats->invalid_callbacks = atomic_load(&adapter->invalid_callbacks);
    out_stats->core_rejections = atomic_load(&adapter->core_rejections);
    out_stats->app_commands_blocked = atomic_load(&adapter->app_commands_blocked);
    out_stats->disconnects_processed = atomic_load(&adapter->disconnects_processed);
    out_stats->domain_boot_changes = atomic_load(&adapter->domain_boot_changes);
    out_stats->gateway_status_notify_deferred =
        atomic_load(&adapter->gateway_status_notify_deferred);
    out_stats->gateway_status_notify_api_accepted =
        atomic_load(&adapter->gateway_status_notify_api_accepted);
    out_stats->gateway_status_notify_failures =
        atomic_load(&adapter->gateway_status_notify_failures);
    out_stats->app_state_events_processed =
        atomic_load(&adapter->app_state_events_processed);
    out_stats->stale_ble_events = atomic_load(&adapter->stale_ble_events);
    out_stats->production_uart_tx_count = 0U;
    out_stats->gateway_status_notify_pending =
        adapter->gateway_status_notify_pending;
    out_stats->app_state_seen = adapter->app_state_seen;
    out_stats->app_state_active = adapter->last_app_active;
}

bool gateway_adapter_is_safe_mode(const gateway_adapter_t *adapter)
{
    return (adapter != NULL) && adapter->started &&
           !gateway_core_production_routing_enabled();
}

#ifdef GATEWAY_ADAPTER_HOST_TEST
bool gateway_adapter_host_process_one(gateway_adapter_t *adapter,
                                      uint64_t now_ms)
{
    return process_one(adapter, now_ms, 0U);
}
#endif
