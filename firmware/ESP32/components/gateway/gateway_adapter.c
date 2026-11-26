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
#include "gateway_context_dto.h"
#include "gateway_context_source_metadata.h"
#include "gateway_core.h"
#include "gateway_request_admission.h"
#include "p6_ble_lifecycle.h"
#include "uart_m_frame.h"

#define GATEWAY_ADAPTER_DEFAULT_QUEUE_DEPTH 12U
#define GATEWAY_ADAPTER_DEFAULT_TASK_STACK 6144U
#define GATEWAY_ADAPTER_DEFAULT_TASK_PRIORITY 5U
#define GATEWAY_ADAPTER_DEFAULT_TICK_MS 100U
#define GATEWAY_ADAPTER_STOPPED_BIT BIT0
#define GATEWAY_ADAPTER_OBSERVATION_SESSION_TIMEOUT_MS UINT64_C(60000)
#define GATEWAY_ADAPTER_REGISTRATION_REQUEST_TTL_MS UINT64_C(30000)
#define GATEWAY_ADAPTER_VEHICLE_DATA_QUEUE_DEPTH 8U
static const gateway_context_source_policy_t monitor_source_policy = {
    .registration_connection_ttl_ms = GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS,
    .app_ttl_ms = GATEWAY_MONITOR_APP_STATE_TTL_MS,
};

typedef enum {
    ADAPTER_EVENT_STOP = 0,
    ADAPTER_EVENT_BLE,
    ADAPTER_EVENT_UART,
    ADAPTER_EVENT_CONTEXT_TX_BIND,
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    ADAPTER_EVENT_REGISTRATION_REQUEST,
    ADAPTER_EVENT_REGISTRATION_APPROVAL,
#endif
} adapter_event_type_t;

#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
typedef enum {
    REGISTRATION_INPUT_NONE = 0,
    REGISTRATION_INPUT_UART0_DEVELOPMENT_CONSOLE,
} registration_input_source_t;

typedef struct {
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
    registration_input_source_t source;
} registration_input_event_t;
#endif

typedef struct {
    adapter_event_type_t type;
    uint64_t received_at_ms;
    union {
        ble_gatt_server_event_t ble;
        uart_m_transport_rx_message_t uart;
        struct {
            uart_m_transport_t *transport;
            gateway_monitor_context_tx_policy_t policy;
        } context_tx_bind;
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        registration_input_event_t registration;
#endif
    } data;
} adapter_event_t;

typedef enum {
    STATUS_PENDING_NONE = 0,
    STATUS_PENDING_CONNECTION,
    STATUS_PENDING_SECURITY,
    STATUS_PENDING_SUBSCRIPTION,
    STATUS_PENDING_MTU,
} status_pending_reason_t;

typedef struct {
    gateway_ble_message_t message;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
} vehicle_data_candidate_t;

typedef struct {
    uart_m_query_t query;
    uint16_t connection_handle;
    uint32_t connection_generation;
    uint32_t connection_epoch;
    uint32_t domain_boot_id;
    uint64_t received_at_ms;
    bool valid;
} mobile_query_candidate_t;

struct gateway_adapter {
    bool started;
    gateway_adapter_config_t config;
    gateway_core_t core;
    bool monitor_context_bound;
    uart_m_transport_t *monitor_context_transport;
    gateway_monitor_context_tx_policy_t monitor_context_tx_policy;
    uint64_t monitor_context_last_attempt_ms;
    bool monitor_context_attempted;
    atomic_uint_least32_t monitor_context_tx_queued;
    atomic_uint_least32_t monitor_context_tx_backpressure;
    atomic_uint_least32_t monitor_context_tx_errors;
    atomic_bool monitor_context_binding_fault;
#ifdef GATEWAY_ADAPTER_HOST_TEST
    bool host_monitor_binding_enabled;
    bool host_monitor_observation_enabled;
#endif
    uint64_t last_peer_observed_at_ms;
    bool peer_observation_seen;
    atomic_uint_least32_t monitor_peer_observations_accepted;
    atomic_uint_least32_t monitor_peer_observations_rejected;
    p6_session_manager_t p6_manager;
    p6_ble_lifecycle_t p6_lifecycle;
    gateway_request_admission_t request_admission;
    uint64_t p6_clock_ms;
    QueueHandle_t queue;
    EventGroupHandle_t lifecycle;
    TaskHandle_t task;
    uint32_t connection_generation;
    uint32_t connection_epoch;
    uint32_t last_invalidated_generation;
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
    bool domain_link_seen_up;
    uint32_t status_snapshot_domain_boot_id;
    uint8_t status_snapshot_domain_state;
    uint32_t status_snapshot_device_context_id;
    uint64_t status_snapshot_session_id;
    bool gateway_status_notify_pending;
    status_pending_reason_t last_pending_reason;
    bool app_state_seen;
    bool last_app_active;
    gateway_context_source_metadata_t context_sources;
    uart_m_context_t context_candidate;
    uint32_t context_candidate_generation;
    bool context_candidate_valid;
    bool context_tx_candidate_ready;
    bool request_capture_active;
    uint64_t request_capture_received_at_ms;
    mobile_query_candidate_t query_candidate;
    vehicle_data_candidate_t
        vehicle_data_candidates[GATEWAY_ADAPTER_VEHICLE_DATA_QUEUE_DEPTH];
    size_t vehicle_data_head;
    size_t vehicle_data_tail;
    size_t vehicle_data_count;
    atomic_uint_least32_t ble_events_enqueued;
    atomic_uint_least32_t uart_events_enqueued;
    atomic_uint_least32_t uart_messages_processed;
    atomic_uint_least32_t uart_messages_rejected;
    atomic_uint_least32_t uart_queue_full;
    atomic_uint_least32_t uart_domain_alive_decoded;
    atomic_uint_least32_t uart_domain_alive_accepted;
    atomic_uint_least32_t uart_domain_boot_observed_changes;
    atomic_uint_least32_t uart_last_link_sequence;
    atomic_uint_least32_t uart_last_domain_boot_id;
    atomic_uint_least32_t uart_last_update_sequence;
    atomic_uint_least32_t uart_last_source_age_ms;
    atomic_uint_least32_t uart_last_domain_state;
    atomic_uint_least32_t uart_last_quality;
    atomic_bool uart_domain_link_up;
    atomic_uint_least32_t uart_downlink_held;
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
    atomic_uint_least32_t p6_identity_rejections;
    atomic_uint_least32_t p6_stale_events;
    atomic_uint_least32_t p6_session_start_attempts;
    atomic_uint_least32_t p6_bond_reconciliation_events;
    atomic_uint_least32_t p6_bond_reconciliation_failures;
    atomic_uint_least32_t ble_epoch_invalidations;
    atomic_uint_least32_t ble_snapshot_fail_closed;
    atomic_uint_least32_t context_dto_attempts;
    atomic_uint_least32_t context_dto_rejections;
    atomic_uint_least32_t context_dto_generation_rejections;
    atomic_bool context_dto_ready;
    atomic_uint_least32_t vehicle_data_candidates_queued;
    atomic_uint_least32_t vehicle_data_candidate_queue_full;
    atomic_uint_least32_t vehicle_data_notify_deferred;
    atomic_uint_least32_t vehicle_data_notify_api_accepted;
    atomic_uint_least32_t vehicle_data_notify_failures;
    atomic_uint_least32_t vehicle_data_candidates_invalidated;
    atomic_uint_least32_t vehicle_data_pending_count;
    atomic_uint_least32_t query_candidates_queued;
    atomic_uint_least32_t query_candidate_queue_full;
    atomic_uint_least32_t query_candidates_invalidated;
    atomic_bool query_candidate_pending;
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    atomic_uint_least32_t registration_requests_enqueued;
    atomic_uint_least32_t registration_approvals_enqueued;
    atomic_uint_least32_t registration_request_rejections;
    atomic_uint_least32_t registration_approval_rejections;
    atomic_uint_least32_t registration_commits;
    atomic_uint_least32_t registration_input_queue_failures;
    atomic_bool registration_input_fault;
    bool registration_request_pending;
    uint16_t registration_request_handle;
    uint32_t registration_request_generation;
    uint32_t registration_request_epoch;
    uint64_t registration_request_started_ms;
    p6_bond_identity_t registration_request_identity;
    p6_ble_peer_observation_t current_peer;
    bool current_peer_available;
#endif
    atomic_uint_least32_t p6_authorization_epoch;
    atomic_uint_least32_t p6_authorization_generation;
    atomic_uint_least32_t p6_authorization_handle;
    atomic_bool p6_authorization_verified;
    atomic_bool p6_registration_available;
    atomic_bool p6_registration_bond_reconciled;
    bool ble_snapshot_fault_latched;
};

static const char *TAG = "gateway_adapter";
static gateway_adapter_t state;

static p6_store_read_result_t unavailable_store_load(
    void *context,
    p6_registration_record_t *out_record)
{
    (void)context;
    if (out_record != NULL) {
        memset(out_record, 0, sizeof(*out_record));
    }
    return P6_STORE_READ_EMPTY;
}

static p6_store_commit_result_t unavailable_store_save(
    void *context,
    const p6_registration_record_t *record)
{
    (void)context;
    (void)record;
    return P6_STORE_NOT_COMMITTED;
}

static p6_store_commit_result_t unavailable_store_erase(void *context)
{
    (void)context;
    return P6_STORE_NOT_COMMITTED;
}

static bool unavailable_entropy_u32(void *context, uint32_t *out_value)
{
    (void)context;
    (void)out_value;
    return false;
}

static bool unavailable_entropy_u64(void *context, uint64_t *out_value)
{
    (void)context;
    (void)out_value;
    return false;
}

static uint64_t p6_observation_clock(void *context)
{
    return ((const gateway_adapter_t *)context)->p6_clock_ms;
}

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

static void write_u64_le(uint8_t *out, uint64_t value)
{
    write_u32_le(out, (uint32_t)value);
    write_u32_le(out + 4U, (uint32_t)(value >> 32U));
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *data)
{
    return (uint64_t)data[0] |
           ((uint64_t)data[1] << 8U) |
           ((uint64_t)data[2] << 16U) |
           ((uint64_t)data[3] << 24U) |
           ((uint64_t)data[4] << 32U) |
           ((uint64_t)data[5] << 40U) |
           ((uint64_t)data[6] << 48U) |
           ((uint64_t)data[7] << 56U);
}

static void clear_vehicle_data_candidates(gateway_adapter_t *adapter)
{
    if (adapter->vehicle_data_count != 0U) {
        (void)atomic_fetch_add_explicit(
            &adapter->vehicle_data_candidates_invalidated,
            (uint32_t)adapter->vehicle_data_count, memory_order_relaxed);
    }
    memset(adapter->vehicle_data_candidates, 0,
           sizeof(adapter->vehicle_data_candidates));
    adapter->vehicle_data_head = 0U;
    adapter->vehicle_data_tail = 0U;
    adapter->vehicle_data_count = 0U;
    atomic_store_explicit(&adapter->vehicle_data_pending_count, 0U,
                          memory_order_release);
}

static void clear_query_candidate(gateway_adapter_t *adapter,
                                  bool invalidated);

static void clear_context_candidate(gateway_adapter_t *adapter)
{
    adapter->context_candidate_valid = false;
    adapter->context_tx_candidate_ready = false;
    atomic_store_explicit(&adapter->context_dto_ready, false,
                          memory_order_release);
    adapter->context_candidate_generation = 0U;
    memset(&adapter->context_candidate, 0, sizeof(adapter->context_candidate));
}

static void clear_request_candidates(gateway_adapter_t *adapter)
{
    gateway_request_admission_invalidate(&adapter->request_admission);
    adapter->request_capture_active = false;
    adapter->request_capture_received_at_ms = 0U;
    clear_query_candidate(adapter, true);
}

static void clear_query_candidate(gateway_adapter_t *adapter,
                                  bool invalidated)
{
    if (adapter->query_candidate.valid && invalidated) {
        increment(&adapter->query_candidates_invalidated);
    }
    memset(&adapter->query_candidate, 0, sizeof(adapter->query_candidate));
    atomic_store_explicit(&adapter->query_candidate_pending, false,
                          memory_order_release);
}

static void handle_domain_link_down(gateway_adapter_t *adapter);
static void observe_accepted_domain_boot(gateway_adapter_t *adapter,
                                         uint32_t domain_boot_id);
static void synchronize_monitor_context(gateway_adapter_t *adapter,
                                         uint64_t now_ms);
static bool monitor_context_binding_enabled(const gateway_adapter_t *adapter);
static void publish_p6_observation_stats(gateway_adapter_t *adapter);
static void publish_safe_gateway_status(gateway_adapter_t *adapter,
                                         bool try_notify, uint64_t now_ms);
static bool observe_uart_request_candidate(const uart_m_message_t *message,
                                           void *user_context);

static bool vehicle_data_candidate_matches_current_context(
    const gateway_adapter_t *adapter,
    const vehicle_data_candidate_t *candidate)
{
    const gateway_ble_message_t *message = &candidate->message;
    size_t expected_length = ble_gatt_protocol_payload_length(
        message->message_type, false);

    return expected_length >= 20U &&
           message->payload_length == expected_length &&
           adapter->core.has_context &&
           adapter->core.context.identity.verified &&
           adapter->core.context.connected &&
           adapter->core.context.encrypted &&
           adapter->core.context.registered &&
           adapter->core.context.authorization_verified &&
           adapter->core.context.connection_generation ==
               candidate->connection_generation &&
           read_u32_le(&message->payload[0]) ==
               adapter->core.context.identity.device_context_id &&
           read_u64_le(&message->payload[4]) ==
               adapter->core.context.identity.session_id &&
            /* Any request_session after DOWN_CONTEXT is historical evidence. */
           read_u32_le(&message->payload[12]) == adapter->core.domain_boot_id;
}

static bool result_candidate_connection_is_current(
    const gateway_adapter_t *adapter,
    const vehicle_data_candidate_t *candidate)
{
    ble_connection_snapshot_t snapshot;

    return ble_gatt_server_get_connection_snapshot(&snapshot) == ESP_OK &&
           snapshot.available && snapshot.connected && adapter->connected &&
           snapshot.connection_handle == candidate->connection_handle &&
           snapshot.connection_generation == candidate->connection_generation &&
           snapshot.connection_epoch == candidate->connection_epoch &&
           adapter->connection_handle == candidate->connection_handle &&
           adapter->connection_generation == candidate->connection_generation &&
           adapter->connection_epoch == candidate->connection_epoch;
}

static bool observe_ble_vehicle_data_candidate(
    const gateway_ble_message_t *message,
    void *user_context)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)user_context;
    vehicle_data_candidate_t *candidate;
    size_t expected_length;

    if (adapter == NULL || message == NULL || !adapter->started) {
        return false;
    }
    expected_length = ble_gatt_protocol_payload_length(
        message->message_type, false);
    if (expected_length < 20U || message->payload_length != expected_length ||
        !adapter->connected || adapter->connection_handle == UINT16_MAX ||
        adapter->connection_generation == 0U ||
        adapter->connection_epoch == 0U) {
        return false;
    }
    if (adapter->vehicle_data_count >=
        GATEWAY_ADAPTER_VEHICLE_DATA_QUEUE_DEPTH) {
        increment(&adapter->vehicle_data_candidate_queue_full);
        return false;
    }

    candidate = &adapter->vehicle_data_candidates[adapter->vehicle_data_tail];
    memset(candidate, 0, sizeof(*candidate));
    candidate->message = *message;
    candidate->connection_handle = adapter->connection_handle;
    candidate->connection_generation = adapter->connection_generation;
    candidate->connection_epoch = adapter->connection_epoch;
    adapter->vehicle_data_tail =
        (adapter->vehicle_data_tail + 1U) %
        GATEWAY_ADAPTER_VEHICLE_DATA_QUEUE_DEPTH;
    adapter->vehicle_data_count++;
    atomic_store_explicit(&adapter->vehicle_data_pending_count,
                          (uint32_t)adapter->vehicle_data_count,
                          memory_order_release);
    increment(&adapter->vehicle_data_candidates_queued);
    return true;
}

static bool observe_uart_query_candidate(const uart_m_message_t *message,
                                         gateway_adapter_t *adapter)
{
    p6_authorization_snapshot_t authorization;
    ble_connection_snapshot_t connection;
    mobile_query_candidate_t *candidate;

    if (message == NULL || adapter == NULL || !adapter->started ||
        message->type != UART_M_TYPE_QUERY ||
        !adapter->request_capture_active || !adapter->connected ||
        !gateway_core_is_domain_link_up(&adapter->core) ||
        adapter->domain_boot_id == 0U ||
        !gateway_adapter_p6_authorization_is_current(adapter) ||
        p6_session_manager_get_snapshot(&adapter->p6_manager,
                                        &authorization) != P6_SESSION_OK ||
        !p6_session_manager_context_is_current(
            &adapter->p6_manager,
            message->body.query.context.device_context_id,
            message->body.query.context.session_id,
            adapter->connection_generation) ||
        ble_gatt_server_get_connection_snapshot(&connection) != ESP_OK ||
        !connection.available || !connection.connected ||
        connection.connection_handle != adapter->connection_handle ||
        connection.connection_generation != adapter->connection_generation ||
        connection.connection_epoch != adapter->connection_epoch ||
        authorization.domain_boot_id != adapter->domain_boot_id) {
        return false;
    }
    if (adapter->query_candidate.valid) {
        increment(&adapter->query_candidate_queue_full);
        return false;
    }

    candidate = &adapter->query_candidate;
    memset(candidate, 0, sizeof(*candidate));
    candidate->query = message->body.query;
    candidate->connection_handle = adapter->connection_handle;
    candidate->connection_generation = adapter->connection_generation;
    candidate->connection_epoch = adapter->connection_epoch;
    candidate->domain_boot_id = adapter->domain_boot_id;
    candidate->received_at_ms = adapter->request_capture_received_at_ms;
    candidate->valid = true;
    atomic_store_explicit(&adapter->query_candidate_pending, true,
                          memory_order_release);
    increment(&adapter->query_candidates_queued);
    return true;
}

static bool observe_uart_mobile_candidate(const uart_m_message_t *message,
                                          void *user_context)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)user_context;

    if (message == NULL) {
        return false;
    }
    if (message->type == UART_M_TYPE_QUERY) {
        return observe_uart_query_candidate(message, adapter);
    }
    return observe_uart_request_candidate(message, user_context);
}

static bool observe_uart_request_candidate(const uart_m_message_t *message,
                                           void *user_context)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)user_context;
    gateway_request_candidate_t candidate;
    p6_authorization_snapshot_t authorization;
    ble_connection_snapshot_t connection;

    if (adapter == NULL || message == NULL || !adapter->started ||
        message->type != UART_M_TYPE_REQUEST ||
        !adapter->request_capture_active || !adapter->connected ||
        !gateway_core_is_domain_link_up(&adapter->core) ||
        adapter->domain_boot_id == 0U ||
        !gateway_adapter_p6_authorization_is_current(adapter) ||
        p6_session_manager_get_snapshot(&adapter->p6_manager,
                                        &authorization) != P6_SESSION_OK ||
        !p6_session_manager_context_is_current(
            &adapter->p6_manager,
            message->body.request.context.device_context_id,
            message->body.request.context.session_id,
            adapter->connection_generation) ||
        ble_gatt_server_get_connection_snapshot(&connection) != ESP_OK ||
        !connection.available || !connection.connected ||
        connection.connection_handle != adapter->connection_handle ||
        connection.connection_generation != adapter->connection_generation ||
        connection.connection_epoch != adapter->connection_epoch ||
        authorization.domain_boot_id != adapter->domain_boot_id) {
        return false;
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.request = message->body.request;
    candidate.received_at_known = true;
    candidate.received_at_ms = adapter->request_capture_received_at_ms;
    candidate.connection_handle = adapter->connection_handle;
    candidate.connection_generation = adapter->connection_generation;
    candidate.connection_epoch = adapter->connection_epoch;
    candidate.domain_boot_id = adapter->domain_boot_id;
    return gateway_request_admission_capture(&adapter->request_admission,
                                             &candidate) ==
           GATEWAY_REQUEST_ADMISSION_OK;
}

static void pop_vehicle_data_candidate(gateway_adapter_t *adapter)
{
    memset(&adapter->vehicle_data_candidates[adapter->vehicle_data_head], 0,
           sizeof(adapter->vehicle_data_candidates[adapter->vehicle_data_head]));
    adapter->vehicle_data_head =
        (adapter->vehicle_data_head + 1U) %
        GATEWAY_ADAPTER_VEHICLE_DATA_QUEUE_DEPTH;
    adapter->vehicle_data_count--;
    atomic_store_explicit(&adapter->vehicle_data_pending_count,
                          (uint32_t)adapter->vehicle_data_count,
                          memory_order_release);
}

static void try_vehicle_data_notify(gateway_adapter_t *adapter, uint64_t now_ms)
{
    synchronize_monitor_context(adapter, now_ms);
    /* Queue the matching Status ahead of Vehicle Data on the BLE link. */
    if (monitor_context_binding_enabled(adapter)) {
        publish_safe_gateway_status(adapter, true, now_ms);
    }
    if (adapter->monitor_context_bound && adapter->gateway_status_notify_pending) {
        if (adapter->vehicle_data_count != 0U) {
            increment(&adapter->vehicle_data_notify_deferred);
        }
        return;
    }
    while (adapter->vehicle_data_count != 0U) {
        vehicle_data_candidate_t *candidate =
            &adapter->vehicle_data_candidates[adapter->vehicle_data_head];
        esp_err_t result;

        if (!result_candidate_connection_is_current(adapter, candidate) ||
            !vehicle_data_candidate_matches_current_context(adapter, candidate) ||
            !adapter->encrypted || !adapter->bonded) {
            pop_vehicle_data_candidate(adapter);
            increment(&adapter->vehicle_data_candidates_invalidated);
            continue;
        }
        if (!adapter->vehicle_data_subscribed ||
            adapter->mtu < BLE_GATT_MIN_APP_MTU) {
            increment(&adapter->vehicle_data_notify_deferred);
            return;
        }

        result = ble_gatt_server_send_vehicle_data(
            candidate->message.message_type, candidate->message.payload,
            candidate->message.payload_length, BLE_GATT_SEND_NOTIFY);
        pop_vehicle_data_candidate(adapter);
        if (result == ESP_OK) {
            increment(&adapter->vehicle_data_notify_api_accepted);
            ESP_LOGI(TAG, "Vehicle Data Notify API accepted; ATT delivery unconfirmed");
            continue;
        }

        increment(&adapter->vehicle_data_notify_failures);
        ESP_LOGW(TAG, "Vehicle Data Notify API failed: rc=%s; vehicle result unchanged and no automatic replay",
                 esp_err_to_name(result));
        return;
    }
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

static void publish_safe_gateway_status(gateway_adapter_t *adapter,
                                         bool try_notify, uint64_t now_ms)
{
    uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH] = {0};
    esp_err_t result;
    bool domain_link_up = gateway_core_is_domain_link_up(&adapter->core);
    bool changed;
    uint32_t device_context_id = 0U;
    uint64_t session_id = 0U;

    if (adapter->status_snapshot_initialized &&
        adapter->status_snapshot_domain_link_up && !domain_link_up) {
        handle_domain_link_down(adapter);
    }
    synchronize_monitor_context(adapter, now_ms);
    if (adapter->monitor_context_bound) {
        device_context_id = adapter->core.context.identity.device_context_id;
        session_id = adapter->core.context.identity.session_id;
    }
    changed = !adapter->status_snapshot_initialized ||
              adapter->status_snapshot_domain_link_up != domain_link_up ||
              adapter->status_snapshot_domain_boot_id != adapter->domain_boot_id ||
              adapter->status_snapshot_domain_state != adapter->domain_state ||
              adapter->status_snapshot_device_context_id != device_context_id ||
              adapter->status_snapshot_session_id != session_id;

    atomic_store(&adapter->uart_domain_link_up, domain_link_up);

    status[0] = BLE_GATT_PROTOCOL_VERSION;
    status[1] = adapter->monitor_context_bound ? 1U : UINT8_MAX;
    status[2] = domain_link_up ? 1U : 0U;
    status[3] = 0U; /* SessionReady is never synthesized in safe mode. */
    write_u32_le(&status[4], device_context_id);
    write_u64_le(&status[8], session_id);
    write_u32_le(&status[16], adapter->domain_boot_id);
    status[20] = domain_link_up ? adapter->domain_state : UINT8_MAX;
    result = ble_gatt_server_set_gateway_status(status);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Gateway Status snapshot update failed; rc=%s",
                 esp_err_to_name(result));
        return;
    }
    if (changed) {
        if (adapter->status_snapshot_initialized &&
            adapter->status_snapshot_domain_link_up != domain_link_up) {
            if (domain_link_up) {
                ESP_LOGI(TAG, "Domain UART link %s",
                         adapter->domain_link_seen_up ? "RECOVERED" : "UP");
                adapter->domain_link_seen_up = true;
            } else {
                ESP_LOGW(TAG, "Domain UART link DOWN");
            }
        }
        adapter->status_snapshot_initialized = true;
        adapter->status_snapshot_domain_link_up = domain_link_up;
        adapter->status_snapshot_domain_boot_id = adapter->domain_boot_id;
        adapter->status_snapshot_domain_state = adapter->domain_state;
        adapter->status_snapshot_device_context_id = device_context_id;
        adapter->status_snapshot_session_id = session_id;
        mark_gateway_status_pending(adapter);
    }
    if (try_notify) {
        try_gateway_status_notify(adapter);
    }
}

static bool ble_event_is_current(const gateway_adapter_t *adapter,
                                 const ble_gatt_server_event_t *event)
{
    ble_connection_snapshot_t snapshot;

    return ble_gatt_server_get_connection_snapshot(&snapshot) == ESP_OK &&
           snapshot.available && snapshot.connected && adapter->connected &&
           event->connection_handle == adapter->connection_handle &&
           event->connection_generation == adapter->connection_generation &&
           snapshot.connection_handle == adapter->connection_handle &&
           snapshot.connection_generation == adapter->connection_generation &&
           snapshot.connection_epoch == adapter->connection_epoch;
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

static gateway_core_status_t process_core_event(
    gateway_adapter_t *adapter,
    gateway_core_status_t enqueue_status,
    uint64_t now_ms)
{
    gateway_core_status_t process_status;

    if (enqueue_status != GATEWAY_CORE_OK) {
        record_core_result(adapter, enqueue_status);
        return enqueue_status;
    }
    process_status = gateway_core_process_next(&adapter->core, now_ms);
    if (adapter->query_candidate.valid) {
        gateway_query_state_t query;
        gateway_query_outcome_t outcome;

        gateway_core_get_query_state(&adapter->core, &query, &outcome);
        (void)outcome;
        if (!query.active ||
            query.device_context_id !=
                adapter->query_candidate.query.context.device_context_id ||
            query.session_id !=
                adapter->query_candidate.query.context.session_id ||
            query.domain_boot_id != adapter->query_candidate.domain_boot_id ||
            query.query_id != adapter->query_candidate.query.query_id ||
            query.scope != adapter->query_candidate.query.scope) {
            clear_query_candidate(adapter, true);
        }
    }
    record_core_result(adapter, process_status);
    return process_status;
}

static bool monitor_context_binding_enabled(const gateway_adapter_t *adapter)
{
#ifdef GATEWAY_ADAPTER_HOST_TEST
    /* Explicit isolated test opt-in; never a firmware authorization source. */
    return adapter->host_monitor_binding_enabled;
#else
    (void)adapter;
    /* Read-only opt-in does not enable the separate vehicle-control binding. */
#if defined(CONFIG_P6_READ_ONLY_MONITORING) && CONFIG_P6_READ_ONLY_MONITORING
    return true;
#else
    return P6_PRODUCTION_BINDING_ENABLED != 0;
#endif
#endif
}

static bool monitor_observation_enabled(const gateway_adapter_t *adapter)
{
#ifdef GATEWAY_ADAPTER_HOST_TEST
    return adapter->host_monitor_observation_enabled;
#else
    return monitor_context_binding_enabled(adapter);
#endif
}

static void revoke_monitor_context(gateway_adapter_t *adapter, uint64_t now_ms)
{
    if (!adapter->monitor_context_bound) {
        return;
    }
    adapter->monitor_context_bound = false;
    if (adapter->monitor_context_transport != NULL) {
        bool in_progress;
        (void)uart_m_transport_invalidate_context(
            adapter->monitor_context_transport, &in_progress);
        /* In-progress/already submitted frames cannot be recalled. */
    }
    clear_context_candidate(adapter);
    clear_vehicle_data_candidates(adapter);
    clear_request_candidates(adapter);
    (void)process_core_event(adapter,
        gateway_core_enqueue_disconnect(&adapter->core,
            adapter->core.context.connection_generation, now_ms), now_ms);
}

/* GatewayTask only. This consumes existing P6 evidence and never issues it.
 * The binding is read-only monitoring; it does not submit any UART candidate. */
static void synchronize_monitor_context(gateway_adapter_t *adapter,
                                         uint64_t now_ms)
{
    p6_authorization_snapshot_t p6;
    ble_connection_snapshot_t ble;
    gateway_core_context_t context;
    p6_session_status_t tick_status;

    if (!monitor_context_binding_enabled(adapter)) {
        return;
    }
    adapter->p6_clock_ms = now_ms;
    tick_status = p6_session_manager_tick(&adapter->p6_manager);
    publish_p6_observation_stats(adapter);
    if (tick_status != P6_SESSION_OK ||
        p6_session_manager_get_snapshot(&adapter->p6_manager, &p6) !=
            P6_SESSION_OK ||
        p6.state != P6_SESSION_STATE_AUTHORIZED ||
        p6.reset_state != P6_RESET_STATE_NONE || p6.storage_fault_latched ||
        !p6.registered || !p6.registration_bond_reconciled ||
        !p6.registration_bond_present || !p6.connected ||
        !p6.bonded || !p6.encrypted || !p6.authorization_verified ||
        !p6.session_valid || p6.device_context_id == 0U || p6.session_id == 0U ||
        !p6_session_manager_context_is_current(&adapter->p6_manager,
            p6.device_context_id, p6.session_id, adapter->connection_generation) ||
        !gateway_adapter_p6_authorization_is_current(adapter) ||
        !adapter->connected || !adapter->encrypted || !adapter->bonded ||
        ble_gatt_server_get_connection_snapshot(&ble) != ESP_OK ||
        !ble.available || !ble.connected ||
        ble.connection_handle != adapter->connection_handle ||
        ble.connection_generation != adapter->connection_generation ||
        ble.connection_epoch != adapter->connection_epoch ||
        !gateway_core_is_domain_link_up(&adapter->core) ||
        !p6.domain_boot_known || p6.domain_boot_id == 0U ||
        p6.domain_boot_id != adapter->domain_boot_id ||
        p6.domain_boot_id != adapter->core.domain_boot_id) {
        revoke_monitor_context(adapter, now_ms);
        return;
    }

    if (adapter->monitor_context_bound &&
        (adapter->core.context.identity.device_context_id != p6.device_context_id ||
         adapter->core.context.identity.session_id != p6.session_id ||
         adapter->core.context.connection_generation != p6.connection_generation)) {
        revoke_monitor_context(adapter, now_ms);
    }
    memset(&context, 0, sizeof(context));
    context.identity.device_context_id = p6.device_context_id;
    context.identity.session_id = p6.session_id;
    context.identity.verified = p6.authorization_verified;
    context.connection_generation = p6.connection_generation;
    context.connected = p6.connected;
    context.encrypted = p6.encrypted;
    context.authenticated = adapter->authenticated; /* SMP MITM fact only. */
    context.registered = p6.registered;
    context.authorization_verified = p6.authorization_verified;
    context.session_ready = false; /* Monitoring does not grant command readiness. */
    context.observed_at_ms = now_ms; /* Local validation time, not Source AGE. */
    if (process_core_event(adapter,
            gateway_core_enqueue_context(&adapter->core, &context, now_ms),
            now_ms) != GATEWAY_CORE_OK) {
        revoke_monitor_context(adapter, now_ms);
        return;
    }
    adapter->monitor_context_bound = true;
}

static p6_ble_peer_observation_t p6_peer_observation(
    const ble_gatt_peer_identity_t *identity)
{
    p6_ble_peer_observation_t observation;

    memset(&observation, 0, sizeof(observation));
    observation.identity_resolved = identity->identity_resolved;
    observation.ota_address_is_rpa = identity->ota_address_is_rpa;
    observation.bond_store_present = identity->bond_store_present;
    observation.lookup_error = identity->lookup_error;
    observation.identity.address_type = identity->address_type;
    memcpy(observation.identity.bytes, identity->address,
           sizeof(observation.identity.bytes));
    return observation;
}

static bool bond_snapshot_contains_registered_identity(
    const gateway_adapter_t *adapter,
    const ble_gatt_server_event_t *event)
{
    uint8_t index;

    for (index = 0U; index < event->data.bond_store.count; ++index) {
        const ble_gatt_peer_identity_t *peer =
            &event->data.bond_store.peers[index];
        p6_bond_identity_t identity;

        if (!peer->identity_resolved || !peer->bond_store_present ||
            peer->lookup_error) {
            continue;
        }
        memset(&identity, 0, sizeof(identity));
        identity.address_type = peer->address_type;
        memcpy(identity.bytes, peer->address, sizeof(identity.bytes));
        if (p6_bond_identity_equal(&adapter->p6_manager.registered_identity,
                                   &identity)) {
            return true;
        }
    }
    return false;
}

static const char *p6_lifecycle_status_name(p6_ble_lifecycle_status_t status)
{
    switch (status) {
    case P6_BLE_LIFECYCLE_OK: return "OK";
    case P6_BLE_LIFECYCLE_IDENTITY_UNRESOLVED: return "IDENTITY_UNRESOLVED";
    case P6_BLE_LIFECYCLE_IDENTITY_LOOKUP_ERROR: return "IDENTITY_LOOKUP_ERROR";
    case P6_BLE_LIFECYCLE_BOND_NOT_FOUND: return "BOND_NOT_FOUND";
    case P6_BLE_LIFECYCLE_PEER_MISMATCH: return "PEER_MISMATCH";
    case P6_BLE_LIFECYCLE_NOT_SECURE: return "NOT_SECURE";
    case P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE:
        return "REGISTRATION_NOT_AVAILABLE";
    case P6_BLE_LIFECYCLE_SESSION_NOT_AUTHORIZED:
        return "SESSION_NOT_AUTHORIZED";
    case P6_BLE_LIFECYCLE_STALE_EVENT: return "STALE_EVENT";
    default: return "REJECTED";
    }
}

static void publish_p6_observation_stats(gateway_adapter_t *adapter)
{
    p6_ble_lifecycle_snapshot_t snapshot;
    ble_connection_snapshot_t connection;
    bool authorization_current = false;

    atomic_store_explicit(&adapter->p6_authorization_verified, false,
                          memory_order_release);
    p6_ble_lifecycle_get_snapshot(&adapter->p6_lifecycle, &snapshot);
    atomic_store(&adapter->p6_registration_available,
                 adapter->p6_manager.registered);
    atomic_store(&adapter->p6_registration_bond_reconciled,
                 adapter->p6_manager.registration_bond_reconciled &&
                 adapter->p6_manager.registration_bond_present);
    atomic_store(&adapter->p6_identity_rejections,
                 snapshot.identity_rejections);
    atomic_store(&adapter->p6_stale_events, snapshot.stale_events);
    atomic_store(&adapter->p6_session_start_attempts,
                 snapshot.session_start_attempts);
    if (ble_gatt_server_get_connection_snapshot(&connection) == ESP_OK) {
        authorization_current =
            p6_ble_lifecycle_authorization_is_current(
                &adapter->p6_lifecycle, connection.available,
                connection.connected, connection.connection_generation,
                connection.connection_epoch) &&
            adapter->connected &&
            adapter->connection_handle == connection.connection_handle;
    }
    if (authorization_current) {
        atomic_store_explicit(&adapter->p6_authorization_epoch,
                              connection.connection_epoch,
                              memory_order_relaxed);
        atomic_store_explicit(&adapter->p6_authorization_generation,
                              connection.connection_generation,
                              memory_order_relaxed);
        atomic_store_explicit(&adapter->p6_authorization_handle,
                              connection.connection_handle,
                              memory_order_relaxed);
        atomic_store_explicit(&adapter->p6_authorization_verified, true,
                              memory_order_release);
    }
}

static void observe_accepted_domain_boot(gateway_adapter_t *adapter,
                                         uint32_t domain_boot_id)
{
    p6_authorization_snapshot_t snapshot;
    p6_session_status_t status;

    if (!adapter->connected || adapter->connection_generation == 0U ||
        p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot) !=
            P6_SESSION_OK ||
        !snapshot.authorization_verified || !snapshot.session_valid ||
        (snapshot.domain_boot_known &&
         snapshot.domain_boot_id == domain_boot_id)) {
        return;
    }

    status = p6_session_manager_observe_domain_boot(
        &adapter->p6_manager, adapter->connection_generation, domain_boot_id);
    if ((status != P6_SESSION_OK) &&
        (status != P6_SESSION_ROTATION_REQUIRED)) {
        clear_context_candidate(adapter);
        clear_vehicle_data_candidates(adapter);
        (void)p6_session_manager_domain_link_down(
            &adapter->p6_manager, adapter->connection_generation);
    }
    publish_p6_observation_stats(adapter);
}

static void handle_domain_link_down(gateway_adapter_t *adapter)
{
    p6_authorization_snapshot_t snapshot;

    clear_context_candidate(adapter);
    clear_vehicle_data_candidates(adapter);
    clear_request_candidates(adapter);
    if (!adapter->connected || adapter->connection_generation == 0U ||
        p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot) !=
            P6_SESSION_OK ||
        (!snapshot.authorization_verified && !snapshot.session_valid)) {
        return;
    }

    (void)p6_session_manager_domain_link_down(
        &adapter->p6_manager, adapter->connection_generation);
    publish_p6_observation_stats(adapter);
}

static void observe_registration_connection_source(
    gateway_adapter_t *adapter,
    uint64_t observed_at_ms)
{
    p6_authorization_snapshot_t snapshot;

    if (p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot) ==
        P6_SESSION_OK) {
        (void)gateway_context_source_observe_registration_connection(
            &adapter->context_sources, &snapshot, true, observed_at_ms);
    }
}

static void observe_app_source(gateway_adapter_t *adapter,
                               uint64_t observed_at_ms)
{
    p6_authorization_snapshot_t snapshot;

    if (p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot) ==
        P6_SESSION_OK) {
        (void)gateway_context_source_observe_app(
            &adapter->context_sources, &snapshot, true, observed_at_ms);
    }
}

static void refresh_context_candidate(gateway_adapter_t *adapter,
                                      uint64_t now_ms)
{
    p6_authorization_snapshot_t snapshot;
    p6_session_status_t snapshot_status;
    gateway_context_sources_t sources;
    uart_m_context_t candidate;

    synchronize_monitor_context(adapter, now_ms);
    clear_context_candidate(adapter);
    increment(&adapter->context_dto_attempts);

    snapshot_status =
        p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot);
    if (snapshot_status == P6_SESSION_OK) {
        gateway_context_source_reconcile(&adapter->context_sources,
                                         &snapshot);
    } else {
        gateway_context_source_invalidate(&adapter->context_sources);
    }

    if (!adapter->connected || adapter->connection_generation == 0U ||
        !gateway_adapter_p6_authorization_is_current(adapter) ||
        snapshot_status != P6_SESSION_OK) {
        increment(&adapter->context_dto_rejections);
        return;
    }
    if (snapshot.connection_generation != adapter->connection_generation) {
        increment(&adapter->context_dto_generation_rejections);
        increment(&adapter->context_dto_rejections);
        return;
    }

    gateway_context_source_build(&adapter->context_sources, &snapshot, true,
                                 now_ms, &sources);
    if (gateway_context_dto_from_p6_snapshot(&snapshot, &sources,
                                             &candidate) !=
        GATEWAY_MAPPER_OK) {
        increment(&adapter->context_dto_rejections);
        return;
    }

    adapter->context_candidate = candidate;
    adapter->context_candidate_generation = snapshot.connection_generation;
    adapter->context_candidate_valid = true;
    /*
     * One bounded, GatewayTask-owned deep copy is also the pre-TX candidate.
     * An unavailable policy/source may still produce a diagnostic DTO carrying
     * NO_DATA, but it is never eligible for submission. The monitoring sender
     * additionally requires explicit policy, fresh OK bases and current P6.
     */
    adapter->context_tx_candidate_ready =
        sources.registration_connection.available && sources.app.available;
    atomic_store_explicit(&adapter->context_dto_ready, true,
                          memory_order_release);
}

/* Single GatewayTask owner; no retained stack pointer and no UART blocking.
 * Only M_CONTEXT can reach this port. Enqueue is not driver/Domain completion. */
static void try_monitor_context_tx(gateway_adapter_t *adapter, uint64_t now_ms)
{
    uart_m_message_t message;
    uart_m_context_tx_limits_t limits;
    uart_m_transport_status_t result;
    const gateway_monitor_context_tx_policy_t *policy =
        &adapter->monitor_context_tx_policy;

    if (atomic_load_explicit(&adapter->monitor_context_binding_fault,
                              memory_order_acquire)) {
        if (adapter->monitor_context_transport != NULL &&
            adapter->monitor_context_tx_policy.approved) {
            bool in_progress;
            (void)uart_m_transport_invalidate_context(adapter->monitor_context_transport, &in_progress);
        }
        adapter->monitor_context_tx_policy.approved = false;
        clear_context_candidate(adapter);
        return; /* Lost binding input requires Adapter restart, not replay. */
    }
    if (!monitor_context_binding_enabled(adapter) ||
        adapter->monitor_context_transport == NULL || !policy->approved) {
        return;
    }
    if (adapter->monitor_context_attempted) {
        if (now_ms < adapter->monitor_context_last_attempt_ms) {
            adapter->monitor_context_tx_policy.approved = false;
            clear_context_candidate(adapter);
            increment(&adapter->monitor_context_tx_errors);
            return;
        }
        if (now_ms - adapter->monitor_context_last_attempt_ms <
            policy->refresh_period_ms) {
            return;
        }
    }
    /* Rebuild from current P6/connection/Domain evidence immediately before
     * queue admission, without renewing any producer timestamp/sequence. */
    refresh_context_candidate(adapter, now_ms);
    if (!adapter->monitor_context_bound ||
        !adapter->context_tx_candidate_ready ||
        adapter->context_candidate_generation != adapter->connection_generation ||
        adapter->context_candidate.context.device_context_id !=
            adapter->status_snapshot_device_context_id ||
        adapter->context_candidate.context.session_id !=
            adapter->status_snapshot_session_id ||
        adapter->context_candidate.registration_connection_quality != UART_M_QUALITY_OK ||
        adapter->context_candidate.app_quality != UART_M_QUALITY_OK ||
        adapter->context_candidate.registration_connection_age_ms == UINT16_MAX ||
        adapter->context_candidate.app_age_ms == UINT16_MAX) {
        return;
    }
    memset(&message, 0, sizeof(message));
    message.type = UART_M_TYPE_CONTEXT;
    message.body.context = adapter->context_candidate;
    memset(&limits, 0, sizeof(limits));
    limits.created_at_ms = now_ms;
    limits.context_ttl_ms = policy->queued_context_ttl_ms;
    limits.registration_connection_ttl_ms = policy->sources.registration_connection_ttl_ms;
    limits.app_ttl_ms = policy->sources.app_ttl_ms;
    limits.proximity_ttl_ms = policy->proximity_ttl_ms;
    adapter->monitor_context_last_attempt_ms = now_ms;
    adapter->monitor_context_attempted = true;
    result = uart_m_transport_send_context(adapter->monitor_context_transport,
                                           &message, &limits);
    if (result == UART_M_TRANSPORT_OK) {
        increment(&adapter->monitor_context_tx_queued);
    } else if (result == UART_M_TRANSPORT_NO_CAPACITY) {
        increment(&adapter->monitor_context_tx_backpressure);
    } else {
        increment(&adapter->monitor_context_tx_errors);
    }
    /* No retained/replayed message: a later approved period rebuilds evidence. */
}

static void bind_monitor_context_tx(gateway_adapter_t *adapter,
                                    const adapter_event_t *event)
{
    if (adapter->monitor_context_transport != NULL) {
        bool in_progress;
        (void)uart_m_transport_invalidate_context(adapter->monitor_context_transport, &in_progress);
    }
    adapter->monitor_context_transport = event->data.context_tx_bind.transport;
    adapter->monitor_context_tx_policy = event->data.context_tx_bind.policy;
    adapter->monitor_context_attempted = false;
    gateway_context_source_metadata_init(&adapter->context_sources,
        adapter->monitor_context_tx_policy.approved
            ? &adapter->monitor_context_tx_policy.sources
            : (monitor_observation_enabled(adapter) ? &monitor_source_policy : NULL));
    if (!adapter->monitor_context_tx_policy.approved && monitor_observation_enabled(adapter)) {
        adapter->monitor_context_tx_policy.refresh_period_ms = GATEWAY_MONITOR_CONTEXT_PERIOD_MS;
        adapter->monitor_context_tx_policy.sources = monitor_source_policy;
        /* Domain Provider / queue dwell contract unverified: approval and
         * queued Context TTL remain zero; observation alone never opens TX. */
    }
    clear_context_candidate(adapter);
}

static void clear_ble_facts(gateway_adapter_t *adapter)
{
    clear_vehicle_data_candidates(adapter);
    clear_request_candidates(adapter);
    adapter->connected = false;
    adapter->connection_handle = UINT16_MAX;
    adapter->connection_generation = 0U;
    adapter->connection_epoch = 0U;
    adapter->encrypted = false;
    adapter->authenticated = false;
    adapter->bonded = false;
    adapter->gateway_status_subscribed = false;
    adapter->vehicle_data_subscribed = false;
    adapter->mtu = 23U;
    adapter->gateway_status_notify_pending = false;
    adapter->last_pending_reason = STATUS_PENDING_NONE;
    adapter->app_state_seen = false;
    gateway_context_source_invalidate(&adapter->context_sources);
    clear_context_candidate(adapter);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    adapter->registration_request_pending = false;
    adapter->registration_request_handle = UINT16_MAX;
    adapter->registration_request_generation = 0U;
    adapter->registration_request_epoch = 0U;
    adapter->registration_request_started_ms = 0U;
    memset(&adapter->registration_request_identity, 0,
           sizeof(adapter->registration_request_identity));
    memset(&adapter->current_peer, 0, sizeof(adapter->current_peer));
    adapter->current_peer_available = false;
#endif
}

static void invalidate_ble_context(gateway_adapter_t *adapter,
                                   uint64_t now_ms,
                                   const char *reason)
{
    uint32_t old_generation = adapter->connection_generation;
    p6_ble_lifecycle_status_t p6_status = P6_BLE_LIFECYCLE_OK;

    atomic_store_explicit(&adapter->p6_authorization_verified, false,
                          memory_order_release);
    if (adapter->p6_lifecycle.connected) {
        old_generation = adapter->p6_lifecycle.connection_generation;
        p6_status = p6_ble_lifecycle_on_disconnect(
            &adapter->p6_lifecycle, old_generation);
    }
    adapter->last_invalidated_generation = old_generation;
    if (adapter->core.has_context && old_generation != 0U) {
        process_core_event(adapter,
            gateway_core_enqueue_disconnect(&adapter->core,
                old_generation, now_ms), now_ms);
    }
    clear_ble_facts(adapter);
    publish_p6_observation_stats(adapter);
    increment(&adapter->disconnects_processed);
    increment(&adapter->ble_epoch_invalidations);
    (void)p6_status;
    (void)reason;
    ESP_LOGW(TAG, "BLE context invalidated by %s; P6=%s",
             reason, p6_lifecycle_status_name(p6_status));
}

static void reconcile_ble_connection(gateway_adapter_t *adapter,
                                     uint64_t now_ms)
{
    ble_connection_snapshot_t snapshot;
    bool readable = ble_gatt_server_get_connection_snapshot(&snapshot) ==
                    ESP_OK && snapshot.available;
    bool lifecycle_active = adapter->connected ||
                            adapter->p6_lifecycle.connected;

    if (!readable) {
        if (lifecycle_active) {
            if (!adapter->ble_snapshot_fault_latched) {
                increment(&adapter->ble_snapshot_fail_closed);
            }
            invalidate_ble_context(adapter, now_ms,
                                   "unavailable BLE snapshot");
            adapter->ble_snapshot_fault_latched = true;
        }
        return;
    }
    adapter->ble_snapshot_fault_latched = false;
    if (!lifecycle_active) {
        return;
    }
    if (!snapshot.connected || !adapter->connected ||
        snapshot.connection_handle != adapter->connection_handle ||
        snapshot.connection_generation != adapter->connection_generation ||
        snapshot.connection_epoch != adapter->connection_epoch) {
        invalidate_ble_context(adapter, now_ms,
                               "connection epoch mismatch");
    }
}

static bool connect_event_matches_snapshot(
    const ble_gatt_server_event_t *event,
    ble_connection_snapshot_t *out_snapshot)
{
    return ble_gatt_server_get_connection_snapshot(out_snapshot) == ESP_OK &&
           out_snapshot->available && out_snapshot->connected &&
           out_snapshot->connection_handle == event->connection_handle &&
           out_snapshot->connection_generation ==
               event->connection_generation &&
           out_snapshot->connection_epoch != 0U;
}

#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
static bool peer_is_registration_candidate(
    const p6_ble_peer_observation_t *peer)
{
    return peer != NULL && peer->identity_resolved &&
           peer->bond_store_present && !peer->lookup_error &&
           p6_bond_identity_is_valid(&peer->identity);
}

static void invalidate_registration_request(gateway_adapter_t *adapter,
                                            const char *reason)
{
    (void)reason;
    if (!adapter->registration_request_pending) {
        return;
    }
    adapter->registration_request_pending = false;
    adapter->registration_request_handle = UINT16_MAX;
    adapter->registration_request_generation = 0U;
    adapter->registration_request_epoch = 0U;
    adapter->registration_request_started_ms = 0U;
    memset(&adapter->registration_request_identity, 0,
           sizeof(adapter->registration_request_identity));
    ESP_LOGW(TAG, "Development Registration request invalidated: %s",
             reason);
}

static bool registration_event_is_current(
    const gateway_adapter_t *adapter,
    const registration_input_event_t *event)
{
    ble_connection_snapshot_t snapshot;

    return event->source == REGISTRATION_INPUT_UART0_DEVELOPMENT_CONSOLE &&
           adapter->connected &&
           adapter->connection_handle == event->connection_handle &&
           adapter->connection_generation == event->connection_generation &&
           adapter->connection_epoch == event->connection_epoch &&
           ble_gatt_server_get_connection_snapshot(&snapshot) == ESP_OK &&
           snapshot.available && snapshot.connected &&
           snapshot.connection_handle == event->connection_handle &&
           snapshot.connection_generation == event->connection_generation &&
           snapshot.connection_epoch == event->connection_epoch;
}

static void consume_registration_input_fault(gateway_adapter_t *adapter)
{
    if (atomic_exchange_explicit(&adapter->registration_input_fault, false,
                                 memory_order_acq_rel)) {
        invalidate_registration_request(adapter, "input queue loss");
    }
}

static bool registration_time_is_current(uint64_t started_ms,
                                         uint64_t observed_ms)
{
    return observed_ms >= started_ms &&
           (observed_ms - started_ms) <
               GATEWAY_ADAPTER_REGISTRATION_REQUEST_TTL_MS;
}

static void expire_registration_request(gateway_adapter_t *adapter,
                                        uint64_t now_ms)
{
    if (adapter->registration_request_pending &&
        !registration_time_is_current(
            adapter->registration_request_started_ms, now_ms)) {
        invalidate_registration_request(adapter,
                                        "approval TTL expired or clock invalid");
    }
}

static void process_registration_request(
    gateway_adapter_t *adapter,
    const registration_input_event_t *event,
    uint64_t requested_at_ms,
    uint64_t now_ms)
{
    if (!registration_time_is_current(requested_at_ms, now_ms) ||
        !registration_event_is_current(adapter, event) ||
        adapter->registration_request_pending ||
        adapter->p6_manager.registered ||
        adapter->p6_manager.state != P6_SESSION_STATE_UNREGISTERED ||
        adapter->p6_manager.storage_fault_latched ||
        !adapter->encrypted || !adapter->bonded ||
        !ble_gatt_server_is_secure() ||
        !adapter->current_peer_available ||
        !peer_is_registration_candidate(&adapter->current_peer) ||
        adapter->p6_lifecycle.generation_blocked ||
        adapter->p6_lifecycle.connection_generation !=
            event->connection_generation ||
        adapter->p6_lifecycle.connection_epoch != event->connection_epoch) {
        increment(&adapter->registration_request_rejections);
        ESP_LOGW(TAG, "Development Registration request rejected; no state change");
        return;
    }

    adapter->registration_request_pending = true;
    adapter->registration_request_handle = event->connection_handle;
    adapter->registration_request_generation = event->connection_generation;
    adapter->registration_request_epoch = event->connection_epoch;
    adapter->registration_request_started_ms = requested_at_ms;
    adapter->registration_request_identity = adapter->current_peer.identity;
    ESP_LOGW(TAG, "Development Registration request pending explicit UART0 approval");
}

static void process_registration_approval(
    gateway_adapter_t *adapter,
    const registration_input_event_t *event,
    uint64_t approved_at_ms,
    uint64_t now_ms)
{
    p6_session_status_t session_status;
    p6_ble_lifecycle_status_t lifecycle_status;

    if (!adapter->registration_request_pending ||
        !registration_time_is_current(
            adapter->registration_request_started_ms, approved_at_ms) ||
        !registration_time_is_current(
            adapter->registration_request_started_ms, now_ms) ||
        now_ms < approved_at_ms ||
        !registration_event_is_current(adapter, event) ||
        adapter->registration_request_handle != event->connection_handle ||
        adapter->registration_request_generation !=
            event->connection_generation ||
        adapter->registration_request_epoch != event->connection_epoch ||
        !adapter->encrypted || !adapter->bonded ||
        !ble_gatt_server_is_secure() ||
        !adapter->current_peer_available ||
        !peer_is_registration_candidate(&adapter->current_peer) ||
        !p6_bond_identity_equal(&adapter->registration_request_identity,
                                &adapter->current_peer.identity)) {
        increment(&adapter->registration_approval_rejections);
        invalidate_registration_request(adapter, "stale or unverifiable approval");
        ESP_LOGW(TAG, "Development Registration approval rejected");
        return;
    }

    /* One-shot approval: consume before any persistent mutation is attempted. */
    invalidate_registration_request(adapter, "approval consumed");
    session_status = p6_session_manager_begin_registration(
        &adapter->p6_manager);
    if (session_status != P6_SESSION_OK) {
        increment(&adapter->registration_approval_rejections);
        ESP_LOGW(TAG, "Development Registration begin rejected: status=%d",
                 (int)session_status);
        return;
    }

    lifecycle_status = p6_ble_lifecycle_on_security(
        &adapter->p6_lifecycle, event->connection_generation,
        &adapter->current_peer, adapter->encrypted, adapter->bonded,
        adapter->authenticated);
    if (lifecycle_status != P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE ||
        !adapter->p6_lifecycle.manager_connected) {
        increment(&adapter->registration_approval_rejections);
        publish_p6_observation_stats(adapter);
        ESP_LOGW(TAG, "Development Registration peer attach rejected: status=%s",
                 p6_lifecycle_status_name(lifecycle_status));
        return;
    }

    session_status = p6_session_manager_register_confirmed_bond(
        &adapter->p6_manager, event->connection_generation);
    publish_p6_observation_stats(adapter);
    if (session_status != P6_SESSION_OK) {
        increment(&adapter->registration_approval_rejections);
        ESP_LOGE(TAG, "Development Registration commit rejected: status=%d",
                 (int)session_status);
        return;
    }

    increment(&adapter->registration_commits);
    ESP_LOGW(TAG, "Development Registration committed; Production authorization remains disabled");
}
#endif

static void process_ble(gateway_adapter_t *adapter,
                        const ble_gatt_server_event_t *event,
                        uint64_t received_at_ms,
                        uint64_t now_ms)
{
    p6_ble_peer_observation_t peer;
    p6_ble_lifecycle_status_t p6_status;
    ble_connection_snapshot_t connection_snapshot;

    adapter->p6_clock_ms = now_ms;
    switch (event->type) {
    case BLE_GATT_EVENT_PEER_OBSERVED: {
        p6_authorization_snapshot_t snapshot;
        if (!monitor_observation_enabled(adapter) ||
            !require_current_ble_event(adapter, event) ||
            event->observation_epoch != adapter->connection_epoch ||
            event->observed_at_ms > now_ms ||
            now_ms - event->observed_at_ms >= GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS ||
            (adapter->peer_observation_seen &&
             event->observed_at_ms <= adapter->last_peer_observed_at_ms)) {
            increment(&adapter->monitor_peer_observations_rejected);
            break;
        }
        peer = p6_peer_observation(&event->peer_identity);
        /* Initial encryption/Registration/Session may still be pending. A
         * periodic observation must not synthesize a disconnect and prevent
         * the existing SECURITY_CHANGED path from completing normally. */
        if (p6_session_manager_tick(&adapter->p6_manager) != P6_SESSION_OK ||
            p6_session_manager_get_snapshot(&adapter->p6_manager, &snapshot) != P6_SESSION_OK ||
            !p6_session_manager_context_is_current(&adapter->p6_manager,
                snapshot.device_context_id, snapshot.session_id, adapter->connection_generation) ||
            !gateway_adapter_p6_authorization_is_current(adapter)) {
            increment(&adapter->monitor_peer_observations_rejected);
            break;
        }
        if (!peer.identity_resolved || peer.lookup_error || !peer.bond_store_present ||
            !event->data.security.encrypted || !event->data.security.bonded ||
            !p6_bond_identity_equal(&peer.identity, &adapter->p6_manager.registered_identity) ||
            !p6_bond_identity_equal(&peer.identity, &adapter->p6_manager.current_identity) ||
            !p6_bond_identity_equal(&peer.identity, &adapter->p6_lifecycle.bound_identity)) {
            increment(&adapter->monitor_peer_observations_rejected);
            invalidate_ble_context(adapter, now_ms, "peer re-observation rejected");
            break;
        }
        /* A timer cannot reach this update: it needs a successful live Host
         * observation plus the existing current P6/Registration evidence. */
        observe_registration_connection_source(adapter, event->observed_at_ms);
        adapter->last_peer_observed_at_ms = event->observed_at_ms;
        adapter->peer_observation_seen = true;
        increment(&adapter->monitor_peer_observations_accepted);
        break;
    }
    case BLE_GATT_EVENT_BOND_STORE_READY:
        increment(&adapter->p6_bond_reconciliation_events);
        if (adapter->p6_manager.registered) {
            bool lookup_succeeded =
                !event->data.bond_store.lookup_error &&
                !event->data.bond_store.overflow &&
                event->data.bond_store.count <=
                    BLE_GATT_BOND_SNAPSHOT_CAPACITY;
            bool exact_identity_present =
                lookup_succeeded &&
                bond_snapshot_contains_registered_identity(adapter, event);
            p6_session_status_t reconcile_status =
                p6_session_manager_reconcile_registered_bond(
                    &adapter->p6_manager, lookup_succeeded,
                    exact_identity_present);

            if (reconcile_status != P6_SESSION_OK) {
                increment(&adapter->p6_bond_reconciliation_failures);
                ESP_LOGW(TAG, "Registration/Bond reconciliation failed closed: status=%d",
                         (int)reconcile_status);
            } else {
                ESP_LOGI(TAG, "Registration/Bond reconciliation verified; identity withheld");
            }
            publish_p6_observation_stats(adapter);
        } else {
            ESP_LOGI(TAG, "Bond Store ready with no ACTIVE Registration; authorization absent");
        }
        break;
    case BLE_GATT_EVENT_CONNECTED:
        if (event->connection_generation == 0U || adapter->connected ||
            !connect_event_matches_snapshot(event, &connection_snapshot)) {
            increment(&adapter->stale_ble_events);
            ESP_LOGW(TAG, "Ignoring BLE connect event inconsistent with latest epoch");
            break;
        }
        adapter->connection_generation = event->connection_generation;
        adapter->peer_observation_seen = false;
        adapter->connection_epoch = connection_snapshot.connection_epoch;
        adapter->connection_handle = event->connection_handle;
        adapter->connected = true;
        adapter->encrypted = false;
        adapter->authenticated = false;
        adapter->bonded = false;
        adapter->gateway_status_subscribed = false;
        adapter->vehicle_data_subscribed = false;
        adapter->mtu = 23U;
        adapter->app_state_seen = false;
        gateway_context_source_invalidate(&adapter->context_sources);
        clear_request_candidates(adapter);
        mark_gateway_status_pending(adapter);
        peer = p6_peer_observation(&event->peer_identity);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        adapter->current_peer = peer;
        adapter->current_peer_available =
            peer_is_registration_candidate(&peer);
#endif
        p6_status = p6_ble_lifecycle_on_connect(
            &adapter->p6_lifecycle, adapter->connection_generation,
            adapter->connection_epoch, &peer);
        publish_p6_observation_stats(adapter);
        (void)p6_lifecycle_status_name(p6_status);
        ESP_LOGI(TAG, "BLE_CONNECTED generation=%" PRIu32
                 " P6=%s; authorization absent",
                 adapter->connection_generation,
                 p6_lifecycle_status_name(p6_status));
        break;
    case BLE_GATT_EVENT_DISCONNECTED:
        if (!adapter->connected && event->connection_generation != 0U &&
            event->connection_generation ==
                adapter->last_invalidated_generation) {
            break;
        }
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
        clear_vehicle_data_candidates(adapter);
        clear_request_candidates(adapter);
        adapter->gateway_status_notify_pending = false;
        adapter->last_pending_reason = STATUS_PENDING_NONE;
        adapter->app_state_seen = false;
        gateway_context_source_invalidate(&adapter->context_sources);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        invalidate_registration_request(adapter, "BLE disconnect");
        memset(&adapter->current_peer, 0, sizeof(adapter->current_peer));
        adapter->current_peer_available = false;
#endif
        p6_status = p6_ble_lifecycle_on_disconnect(
            &adapter->p6_lifecycle, event->connection_generation);
        publish_p6_observation_stats(adapter);
        (void)p6_lifecycle_status_name(p6_status);
        increment(&adapter->disconnects_processed);
        if (adapter->core.has_context) {
            process_core_event(adapter,
                gateway_core_enqueue_disconnect(&adapter->core,
                    adapter->connection_generation, received_at_ms), now_ms);
        }
        ESP_LOGI(TAG, "BLE_DISCONNECTED P6=%s; no command replay",
                 p6_lifecycle_status_name(p6_status));
        break;
    case BLE_GATT_EVENT_SECURITY_CHANGED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        peer = p6_peer_observation(&event->peer_identity);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        if (adapter->registration_request_pending &&
            (adapter->encrypted != event->data.security.encrypted ||
             adapter->authenticated != event->data.security.authenticated ||
             adapter->bonded != event->data.security.bonded ||
             !peer_is_registration_candidate(&peer) ||
             !p6_bond_identity_equal(&adapter->registration_request_identity,
                                     &peer.identity))) {
            invalidate_registration_request(adapter, "BLE security changed");
        }
        adapter->current_peer = peer;
        adapter->current_peer_available =
            peer_is_registration_candidate(&peer);
#endif
        adapter->encrypted = event->data.security.encrypted;
        adapter->authenticated = event->data.security.authenticated;
        adapter->bonded = event->data.security.bonded;
        if (!adapter->encrypted || !adapter->bonded) {
            clear_vehicle_data_candidates(adapter);
            clear_request_candidates(adapter);
        }
        p6_status = p6_ble_lifecycle_on_security(
            &adapter->p6_lifecycle, adapter->connection_generation, &peer,
            adapter->encrypted, adapter->bonded, adapter->authenticated);
        publish_p6_observation_stats(adapter);
        if (p6_status == P6_BLE_LIFECYCLE_OK) {
            observe_registration_connection_source(adapter, received_at_ms);
        }
        (void)p6_lifecycle_status_name(p6_status);
        ESP_LOGI(TAG, "BLE security: encrypted=%d authenticated=%d bonded=%d"
                 " P6=%s; not Production authorization",
                 adapter->encrypted, adapter->authenticated, adapter->bonded,
                 p6_lifecycle_status_name(p6_status));
        if (p6_status == P6_BLE_LIFECYCLE_REGISTRATION_UNAVAILABLE) {
            ESP_LOGI(TAG, "P6_REGISTRATION_NOT_AVAILABLE; SESSION_NOT_AUTHORIZED");
        }
        try_gateway_status_notify(adapter);
        try_vehicle_data_notify(adapter, now_ms);
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
        try_vehicle_data_notify(adapter, now_ms);
        break;
    case BLE_GATT_EVENT_MTU_CHANGED:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        adapter->mtu = event->data.mtu.mtu;
        ESP_LOGI(TAG, "BLE MTU=%u connection=%u", adapter->mtu,
                  event->connection_handle);
        try_gateway_status_notify(adapter);
        try_vehicle_data_notify(adapter, now_ms);
        break;
    case BLE_GATT_EVENT_APP_COMMAND:
        if (!require_current_ble_event(adapter, event)) {
            break;
        }
        increment(&adapter->app_commands_blocked);
        if (monitor_context_binding_enabled(adapter)) {
            /* Read-only Context admission must not create UART candidates. */
            break;
        }
        adapter->request_capture_active = true;
        adapter->request_capture_received_at_ms = received_at_ms;
        process_core_event(adapter,
            gateway_core_enqueue_ble_command(&adapter->core,
                event->data.app_command.message_type,
                event->data.app_command.payload,
                event->data.app_command.payload_length,
                adapter->connection_generation, received_at_ms), now_ms);
        adapter->request_capture_active = false;
        adapter->request_capture_received_at_ms = 0U;
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
        p6_status = p6_ble_lifecycle_on_app_state(
            &adapter->p6_lifecycle, adapter->connection_generation,
            event->data.app_state.app_instance_id,
            event->data.app_state.active);
        publish_p6_observation_stats(adapter);
        if (p6_status == P6_BLE_LIFECYCLE_OK) {
            observe_app_source(adapter, received_at_ms);
        }
        (void)p6_lifecycle_status_name(p6_status);
        ESP_LOGI(TAG, "App State delivered to P6 observer: result=%s; no authority elevation",
                 p6_lifecycle_status_name(p6_status));
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
    const uart_m_message_descriptor_t *descriptor;
    gateway_core_status_t core_status;
    uint8_t encoded[UART_M_MAX_PAYLOAD_SIZE];
    size_t encoded_length = 0U;
    int message_type = (int)message->message.type;

    increment(&adapter->uart_messages_processed);
    if ((message_type < 0) || (message_type > (int)UINT8_MAX)) {
        increment(&adapter->uart_messages_rejected);
        ESP_LOGW(TAG, "UART message rejected in GatewayTask: out-of-range type=%d",
                 message_type);
        return;
    }
    descriptor = uart_m_codec_descriptor((uint8_t)message_type);
    if ((descriptor == NULL) ||
        (descriptor->direction != UART_M_DIRECTION_ESP32_RX) ||
        descriptor->schema_only ||
        (uart_m_codec_encode(&message->message, encoded, sizeof(encoded),
                             &encoded_length) != UART_M_CODEC_OK) ||
        (encoded_length != descriptor->payload_length)) {
        increment(&adapter->uart_messages_rejected);
        ESP_LOGW(TAG, "UART message rejected in GatewayTask: type=0x%02X schema/direction invalid",
                 (unsigned)(uint8_t)message_type);
        return;
    }

    if (message->message.type == UART_M_TYPE_DOMAIN_ALIVE) {
        const uart_m_domain_alive_t *alive =
            &message->message.body.domain_alive;
        uint32_t previous_boot =
            atomic_load(&adapter->uart_last_domain_boot_id);

        increment(&adapter->uart_domain_alive_decoded);
        if ((previous_boot != 0U) &&
            (previous_boot != alive->domain_boot_id)) {
            increment(&adapter->uart_domain_boot_observed_changes);
        }
        atomic_store(&adapter->uart_last_link_sequence,
                     (uint32_t)message->link_sequence);
        atomic_store(&adapter->uart_last_domain_boot_id,
                     alive->domain_boot_id);
        atomic_store(&adapter->uart_last_update_sequence,
                     alive->update_sequence);
        atomic_store(&adapter->uart_last_source_age_ms,
                     (uint32_t)alive->source_age_ms);
        atomic_store(&adapter->uart_last_domain_state,
                     (uint32_t)alive->domain_state);
        atomic_store(&adapter->uart_last_quality,
                     (uint32_t)alive->quality);
    }

    synchronize_monitor_context(adapter, now_ms);
    core_status = process_core_event(adapter,
        gateway_core_enqueue_uart_message(&adapter->core, &message->message,
                                           message->received_at_us >= 0
                                               ? (uint64_t)message->received_at_us / 1000U
                                               : now_ms),
        now_ms);
    if (message->message.type == UART_M_TYPE_DOMAIN_ALIVE) {
        if (core_status == GATEWAY_CORE_OK) {
            uint32_t accepted_boot =
                message->message.body.domain_alive.domain_boot_id;
            bool boot_changed = (adapter->domain_boot_id != 0U) &&
                                (adapter->domain_boot_id != accepted_boot);

            if (boot_changed) {
                increment(&adapter->domain_boot_changes);
                clear_context_candidate(adapter);
                clear_vehicle_data_candidates(adapter);
                clear_request_candidates(adapter);
            }
            observe_accepted_domain_boot(adapter, accepted_boot);
            adapter->domain_boot_id = accepted_boot;
            adapter->domain_state =
                (uint8_t)message->message.body.domain_alive.domain_state;
            increment(&adapter->uart_domain_alive_accepted);
        } else {
            increment(&adapter->uart_messages_rejected);
        }
        publish_safe_gateway_status(adapter, true, now_ms);
        return;
    }
    if ((core_status == GATEWAY_CORE_OK) ||
        (core_status == GATEWAY_CORE_ROUTING_HOLD)) {
        try_vehicle_data_notify(adapter, now_ms);
        if (core_status == GATEWAY_CORE_OK) {
            return;
        }
    }
    increment(&adapter->uart_downlink_held);
    if ((core_status != GATEWAY_CORE_OK) &&
        (core_status != GATEWAY_CORE_ROUTING_HOLD)) {
        increment(&adapter->uart_messages_rejected);
    }
}

#ifdef GATEWAY_ADAPTER_HOST_TEST
static bool process_one(gateway_adapter_t *adapter, uint64_t now_ms,
                        TickType_t wait_ticks)
{
    adapter_event_t event;

    if ((adapter == NULL) || !adapter->started) {
        return false;
    }
    reconcile_ble_connection(adapter, now_ms);
    synchronize_monitor_context(adapter, now_ms);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    consume_registration_input_fault(adapter);
    expire_registration_request(adapter, now_ms);
#endif
    if (xQueueReceive(adapter->queue, &event, wait_ticks) != pdTRUE) {
        if ((adapter != NULL) && adapter->started && (wait_ticks != 0U)) {
            process_core_event(adapter,
                gateway_core_enqueue_tick(&adapter->core, now_ms), now_ms);
            publish_safe_gateway_status(adapter, true, now_ms);
        }
        if (monitor_context_binding_enabled(adapter)) {
            publish_safe_gateway_status(adapter, true, now_ms);
        }
        return false;
    }
    if (event.type == ADAPTER_EVENT_STOP) {
        if (adapter->monitor_context_transport != NULL) {
            bool in_progress;
            (void)uart_m_transport_invalidate_context(adapter->monitor_context_transport, &in_progress);
        }
        return false;
    }
    reconcile_ble_connection(adapter, now_ms);
    increment(&adapter->events_processed);
    if (event.type == ADAPTER_EVENT_BLE) {
        process_ble(adapter, &event.data.ble, event.received_at_ms, now_ms);
    } else if (event.type == ADAPTER_EVENT_UART) {
        process_uart(adapter, &event.data.uart, now_ms);
    } else if (event.type == ADAPTER_EVENT_CONTEXT_TX_BIND) {
        bind_monitor_context_tx(adapter, &event);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    } else if (event.type == ADAPTER_EVENT_REGISTRATION_REQUEST) {
        process_registration_request(adapter, &event.data.registration,
                                     event.received_at_ms, now_ms);
    } else if (event.type == ADAPTER_EVENT_REGISTRATION_APPROVAL) {
        process_registration_approval(adapter, &event.data.registration,
                                      event.received_at_ms, now_ms);
#endif
    } else {
        increment(&adapter->invalid_callbacks);
    }
    refresh_context_candidate(adapter, now_ms);
    if (monitor_context_binding_enabled(adapter)) {
        publish_safe_gateway_status(adapter, true, now_ms);
    }
    try_monitor_context_tx(adapter, now_ms);
    return true;
}
#endif

static void gateway_task(void *argument)
{
    gateway_adapter_t *adapter = (gateway_adapter_t *)argument;
    adapter_event_t event;

    while (true) {
        uint64_t now_ms = monotonic_ms();

        reconcile_ble_connection(adapter, now_ms);
        synchronize_monitor_context(adapter, now_ms);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        consume_registration_input_fault(adapter);
        expire_registration_request(adapter, now_ms);
#endif
        if (xQueueReceive(adapter->queue, &event,
                          pdMS_TO_TICKS(adapter->config.tick_period_ms)) != pdTRUE) {
            now_ms = monotonic_ms();
            reconcile_ble_connection(adapter, now_ms);
            process_core_event(adapter,
                gateway_core_enqueue_tick(&adapter->core, now_ms), now_ms);
            publish_safe_gateway_status(adapter, true, now_ms);
            refresh_context_candidate(adapter, now_ms);
            try_monitor_context_tx(adapter, now_ms);
            continue;
        }
        if (event.type == ADAPTER_EVENT_STOP) {
            if (adapter->monitor_context_transport != NULL) {
                bool in_progress;
                (void)uart_m_transport_invalidate_context(adapter->monitor_context_transport, &in_progress);
            }
            break;
        }
        now_ms = monotonic_ms();
        reconcile_ble_connection(adapter, now_ms);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        expire_registration_request(adapter, now_ms);
#endif
        increment(&adapter->events_processed);
        if (event.type == ADAPTER_EVENT_BLE) {
            process_ble(adapter, &event.data.ble, event.received_at_ms,
                        now_ms);
        } else if (event.type == ADAPTER_EVENT_UART) {
            process_uart(adapter, &event.data.uart, now_ms);
        } else if (event.type == ADAPTER_EVENT_CONTEXT_TX_BIND) {
            bind_monitor_context_tx(adapter, &event);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
        } else if (event.type == ADAPTER_EVENT_REGISTRATION_REQUEST) {
            process_registration_request(adapter, &event.data.registration,
                                         event.received_at_ms, now_ms);
        } else if (event.type == ADAPTER_EVENT_REGISTRATION_APPROVAL) {
            process_registration_approval(adapter, &event.data.registration,
                                          event.received_at_ms, now_ms);
#endif
        } else {
            increment(&adapter->invalid_callbacks);
        }
        refresh_context_candidate(adapter, now_ms);
        if (monitor_context_binding_enabled(adapter)) {
            publish_safe_gateway_status(adapter, true, now_ms);
        }
        try_monitor_context_tx(adapter, now_ms);
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
    p6_session_config_t p6_config;
    p6_registration_store_t p6_store;
    p6_entropy_source_t p6_entropy;
    p6_monotonic_clock_t p6_clock;
    p6_session_status_t p6_init_status;

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
    gateway_context_source_metadata_init(&state.context_sources,
        monitor_observation_enabled(&state) ? &monitor_source_policy : NULL);
    gateway_request_admission_init(&state.request_admission, NULL);
    core_config = gateway_core_isolated_mock_config();
    core_config.mock_ports.observe_uart_candidate =
        observe_uart_mobile_candidate;
    core_config.mock_ports.observe_ble_candidate =
        observe_ble_vehicle_data_candidate;
    core_config.mock_ports.user_context = &state;
    if (gateway_core_init(&state.core, &core_config) != GATEWAY_CORE_OK) {
        return ESP_FAIL;
    }
    memset(&p6_config, 0, sizeof(p6_config));
    memset(&p6_store, 0, sizeof(p6_store));
    memset(&p6_entropy, 0, sizeof(p6_entropy));
    memset(&p6_clock, 0, sizeof(p6_clock));
    p6_config.session_timeout_ms =
        GATEWAY_ADAPTER_OBSERVATION_SESSION_TIMEOUT_MS;
    p6_config.id_generation_attempts = P6_SESSION_MAX_ID_ATTEMPTS;
    if (config->registration_store != NULL) {
        p6_store = *config->registration_store;
    } else {
        p6_store.load = unavailable_store_load;
        p6_store.save = unavailable_store_save;
        p6_store.erase = unavailable_store_erase;
    }
    if (config->entropy_source != NULL) {
        p6_entropy = *config->entropy_source;
    } else {
        p6_entropy.next_u32 = unavailable_entropy_u32;
        p6_entropy.next_u64 = unavailable_entropy_u64;
    }
    p6_clock.now_ms = p6_observation_clock;
    p6_clock.context = &state;
    p6_init_status = p6_session_manager_init(
        &state.p6_manager, &p6_config, &p6_store, &p6_entropy, &p6_clock);
    if ((p6_init_status != P6_SESSION_OK &&
         p6_init_status != P6_SESSION_RESET_REQUIRED &&
         p6_init_status != P6_SESSION_STORE_CORRUPT &&
         p6_init_status != P6_SESSION_STORE_ERROR) ||
        p6_ble_lifecycle_init(&state.p6_lifecycle, &state.p6_manager) !=
            P6_BLE_LIFECYCLE_OK) {
        memset(&state, 0, sizeof(state));
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
    publish_p6_observation_stats(&state);
    *out_adapter = &state;
    publish_safe_gateway_status(&state, false, monotonic_ms());
    ESP_LOGI(TAG, "GatewayTask queue=%u events (%u bytes each), stack=%" PRIu32 " bytes",
             (unsigned)config->queue_depth, (unsigned)sizeof(adapter_event_t),
             config->task_stack_size);
    ESP_LOGW(TAG, "Safe Integration Mode: UART TX, P6 Context, Query completion and auto Unlock disabled");
    ESP_LOGI(TAG, "P6 persistent Registration Store=%s init_status=%d",
             config->registration_store != NULL ? "configured" : "unavailable",
             (int)p6_init_status);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    ESP_LOGW(TAG, "Development UART0 Registration approval enabled; not Production authorization");
#else
    ESP_LOGW(TAG, "Registration approval and developer Bond Reset inputs are not runtime-bound");
#endif
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

static bool enqueue(adapter_event_t *queued, gateway_adapter_t *adapter,
                    atomic_uint_least32_t *accepted_counter)
{
    if ((adapter == NULL) || !adapter->started ||
        (xQueueSend(adapter->queue, queued, 0U) != pdTRUE)) {
        if (adapter != NULL) {
            increment(&adapter->queue_full);
        }
        return false;
    }
    increment(accepted_counter);
    return true;
}

esp_err_t gateway_adapter_bind_monitor_context_tx(
    gateway_adapter_t *adapter, uart_m_transport_t *transport,
    const gateway_monitor_context_tx_policy_t *policy)
{
    adapter_event_t event;

    if (adapter == NULL || !adapter->started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (policy != NULL && policy->approved &&
        (transport == NULL || policy->refresh_period_ms == 0U ||
         policy->queued_context_ttl_ms == 0U ||
         policy->sources.registration_connection_ttl_ms == 0U ||
         policy->sources.app_ttl_ms == 0U)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&event, 0, sizeof(event));
    event.type = ADAPTER_EVENT_CONTEXT_TX_BIND;
    event.received_at_ms = monotonic_ms();
    event.data.context_tx_bind.transport = transport;
    if (policy != NULL && policy->approved) {
        event.data.context_tx_bind.policy = *policy;
    }
    if (xQueueSend(adapter->queue, &event, 0U) != pdTRUE) {
        atomic_store_explicit(&adapter->monitor_context_binding_fault, true,
                              memory_order_release);
        increment(&adapter->queue_full);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
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
    (void)enqueue(&queued, adapter, &adapter->ble_events_enqueued);
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
    if (!enqueue(&queued, adapter, &adapter->uart_events_enqueued) &&
        adapter != NULL) {
        increment(&adapter->uart_queue_full);
    }
}

#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
static esp_err_t enqueue_registration_input(
    gateway_adapter_t *adapter,
    adapter_event_type_t type,
    atomic_uint_least32_t *accepted_counter,
    atomic_uint_least32_t *rejected_counter)
{
    ble_connection_snapshot_t snapshot;
    adapter_event_t queued;

    if (adapter == NULL || !adapter->started ||
        ble_gatt_server_get_connection_snapshot(&snapshot) != ESP_OK ||
        !snapshot.available || !snapshot.connected ||
        snapshot.connection_generation == 0U ||
        snapshot.connection_epoch == 0U) {
        if (adapter != NULL) {
            increment(rejected_counter);
        }
        return ESP_ERR_INVALID_STATE;
    }

    memset(&queued, 0, sizeof(queued));
    queued.type = type;
    queued.received_at_ms = monotonic_ms();
    queued.data.registration.connection_handle = snapshot.connection_handle;
    queued.data.registration.connection_generation =
        snapshot.connection_generation;
    queued.data.registration.connection_epoch = snapshot.connection_epoch;
    queued.data.registration.source =
        REGISTRATION_INPUT_UART0_DEVELOPMENT_CONSOLE;
    if (xQueueSend(adapter->queue, &queued, 0U) != pdTRUE) {
        increment(&adapter->queue_full);
        increment(&adapter->registration_input_queue_failures);
        atomic_store_explicit(&adapter->registration_input_fault, true,
                              memory_order_release);
        return ESP_ERR_TIMEOUT;
    }
    increment(accepted_counter);
    return ESP_OK;
}

esp_err_t gateway_adapter_development_request_registration(
    gateway_adapter_t *adapter)
{
    if (adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return enqueue_registration_input(
        adapter, ADAPTER_EVENT_REGISTRATION_REQUEST,
        &adapter->registration_requests_enqueued,
        &adapter->registration_request_rejections);
}

esp_err_t gateway_adapter_development_approve_registration(
    gateway_adapter_t *adapter)
{
    if (adapter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return enqueue_registration_input(
        adapter, ADAPTER_EVENT_REGISTRATION_APPROVAL,
        &adapter->registration_approvals_enqueued,
        &adapter->registration_approval_rejections);
}
#endif

void gateway_adapter_get_stats(const gateway_adapter_t *adapter,
                               gateway_adapter_stats_t *out_stats)
{
    if ((adapter == NULL) || (out_stats == NULL)) {
        return;
    }
    out_stats->ble_events_enqueued = atomic_load(&adapter->ble_events_enqueued);
    out_stats->uart_events_enqueued = atomic_load(&adapter->uart_events_enqueued);
    out_stats->uart_messages_processed =
        atomic_load(&adapter->uart_messages_processed);
    out_stats->uart_messages_rejected =
        atomic_load(&adapter->uart_messages_rejected);
    out_stats->uart_queue_full = atomic_load(&adapter->uart_queue_full);
    out_stats->uart_domain_alive_accepted =
        atomic_load(&adapter->uart_domain_alive_accepted);
    out_stats->uart_downlink_held =
        atomic_load(&adapter->uart_downlink_held);
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
    out_stats->production_m_context_tx_count = 0U;
    out_stats->digital_key_auto_unlock_count = 0U;
    out_stats->monitor_context_tx_queued = atomic_load(&adapter->monitor_context_tx_queued);
    out_stats->monitor_context_tx_backpressure = atomic_load(&adapter->monitor_context_tx_backpressure);
    out_stats->monitor_context_tx_errors = atomic_load(&adapter->monitor_context_tx_errors);
    out_stats->monitor_peer_observations_accepted = atomic_load(&adapter->monitor_peer_observations_accepted);
    out_stats->monitor_peer_observations_rejected = atomic_load(&adapter->monitor_peer_observations_rejected);
    out_stats->gateway_status_notify_pending =
        adapter->gateway_status_notify_pending;
    out_stats->app_state_seen = adapter->app_state_seen;
    out_stats->app_state_active = adapter->last_app_active;
    out_stats->p6_identity_rejections =
        atomic_load(&adapter->p6_identity_rejections);
    out_stats->p6_stale_events = atomic_load(&adapter->p6_stale_events);
    out_stats->p6_session_start_attempts =
        atomic_load(&adapter->p6_session_start_attempts);
    out_stats->p6_bond_reconciliation_events =
        atomic_load(&adapter->p6_bond_reconciliation_events);
    out_stats->p6_bond_reconciliation_failures =
        atomic_load(&adapter->p6_bond_reconciliation_failures);
    out_stats->ble_epoch_invalidations =
        atomic_load(&adapter->ble_epoch_invalidations);
    out_stats->ble_snapshot_fail_closed =
        atomic_load(&adapter->ble_snapshot_fail_closed);
    out_stats->context_dto_attempts =
        atomic_load(&adapter->context_dto_attempts);
    out_stats->context_dto_rejections =
        atomic_load(&adapter->context_dto_rejections);
    out_stats->context_dto_generation_rejections =
        atomic_load(&adapter->context_dto_generation_rejections);
    out_stats->vehicle_data_candidates_queued =
        atomic_load(&adapter->vehicle_data_candidates_queued);
    out_stats->vehicle_data_candidate_queue_full =
        atomic_load(&adapter->vehicle_data_candidate_queue_full);
    out_stats->vehicle_data_notify_deferred =
        atomic_load(&adapter->vehicle_data_notify_deferred);
    out_stats->vehicle_data_notify_api_accepted =
        atomic_load(&adapter->vehicle_data_notify_api_accepted);
    out_stats->vehicle_data_notify_failures =
        atomic_load(&adapter->vehicle_data_notify_failures);
    out_stats->vehicle_data_candidates_invalidated =
        atomic_load(&adapter->vehicle_data_candidates_invalidated);
    out_stats->query_candidates_queued =
        atomic_load(&adapter->query_candidates_queued);
    out_stats->query_candidate_queue_full =
        atomic_load(&adapter->query_candidate_queue_full);
    out_stats->query_candidates_invalidated =
        atomic_load(&adapter->query_candidates_invalidated);
    out_stats->query_candidate_pending =
        atomic_load_explicit(&adapter->query_candidate_pending,
                             memory_order_acquire);
    out_stats->vehicle_data_pending_count =
        atomic_load_explicit(&adapter->vehicle_data_pending_count,
                             memory_order_acquire);
    out_stats->context_dto_ready =
        atomic_load_explicit(&adapter->context_dto_ready,
                             memory_order_acquire);
#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
    out_stats->registration_requests_enqueued =
        atomic_load(&adapter->registration_requests_enqueued);
    out_stats->registration_approvals_enqueued =
        atomic_load(&adapter->registration_approvals_enqueued);
    out_stats->registration_request_rejections =
        atomic_load(&adapter->registration_request_rejections);
    out_stats->registration_approval_rejections =
        atomic_load(&adapter->registration_approval_rejections);
    out_stats->registration_commits =
        atomic_load(&adapter->registration_commits);
    out_stats->registration_input_queue_failures =
        atomic_load(&adapter->registration_input_queue_failures);
    out_stats->registration_request_pending =
        adapter->registration_request_pending;
#else
    out_stats->registration_requests_enqueued = 0U;
    out_stats->registration_approvals_enqueued = 0U;
    out_stats->registration_request_rejections = 0U;
    out_stats->registration_approval_rejections = 0U;
    out_stats->registration_commits = 0U;
    out_stats->registration_input_queue_failures = 0U;
    out_stats->registration_request_pending = false;
#endif
    out_stats->p6_registration_available =
        atomic_load(&adapter->p6_registration_available);
    out_stats->p6_registration_bond_reconciled =
        atomic_load(&adapter->p6_registration_bond_reconciled);
    out_stats->p6_authorization_verified =
        gateway_adapter_p6_authorization_is_current(adapter);
}

void gateway_adapter_get_uart_rx_diagnostics(
    const gateway_adapter_t *adapter,
    gateway_adapter_uart_rx_diagnostics_t *out_diagnostics)
{
    if ((adapter == NULL) || (out_diagnostics == NULL)) {
        return;
    }
    out_diagnostics->uart_messages_rejected =
        atomic_load(&adapter->uart_messages_rejected);
    out_diagnostics->uart_queue_full =
        atomic_load(&adapter->uart_queue_full);
    out_diagnostics->uart_domain_alive_decoded =
        atomic_load(&adapter->uart_domain_alive_decoded);
    out_diagnostics->uart_domain_alive_accepted =
        atomic_load(&adapter->uart_domain_alive_accepted);
    out_diagnostics->uart_domain_boot_observed_changes =
        atomic_load(&adapter->uart_domain_boot_observed_changes);
    out_diagnostics->uart_last_link_sequence =
        atomic_load(&adapter->uart_last_link_sequence);
    out_diagnostics->uart_last_domain_boot_id =
        atomic_load(&adapter->uart_last_domain_boot_id);
    out_diagnostics->uart_last_update_sequence =
        atomic_load(&adapter->uart_last_update_sequence);
    out_diagnostics->uart_last_source_age_ms =
        atomic_load(&adapter->uart_last_source_age_ms);
    out_diagnostics->uart_last_domain_state =
        atomic_load(&adapter->uart_last_domain_state);
    out_diagnostics->uart_last_quality =
        atomic_load(&adapter->uart_last_quality);
    out_diagnostics->uart_domain_link_up =
        atomic_load(&adapter->uart_domain_link_up);
}

bool gateway_adapter_is_safe_mode(const gateway_adapter_t *adapter)
{
    return (adapter != NULL) && adapter->started &&
           !gateway_core_production_routing_enabled();
}

bool gateway_adapter_p6_authorization_is_current(
    const gateway_adapter_t *adapter)
{
    ble_connection_snapshot_t snapshot;
    uint32_t epoch;
    uint32_t generation;
    uint32_t handle;

    if (adapter == NULL || !adapter->started ||
        !atomic_load_explicit(&adapter->p6_authorization_verified,
                              memory_order_acquire) ||
        !ble_gatt_server_is_secure() ||
        ble_gatt_server_get_connection_snapshot(&snapshot) != ESP_OK ||
        !snapshot.available || !snapshot.connected) {
        return false;
    }
    epoch = atomic_load_explicit(&adapter->p6_authorization_epoch,
                                 memory_order_relaxed);
    generation = atomic_load_explicit(&adapter->p6_authorization_generation,
                                      memory_order_relaxed);
    handle = atomic_load_explicit(&adapter->p6_authorization_handle,
                                  memory_order_relaxed);
    return epoch != 0U && generation != 0U &&
           epoch == snapshot.connection_epoch &&
           generation == snapshot.connection_generation &&
           handle == snapshot.connection_handle;
}

#ifdef GATEWAY_ADAPTER_HOST_TEST
void gateway_adapter_host_enable_monitor_observation(gateway_adapter_t *adapter)
{
    if (adapter != NULL && adapter->started) {
        adapter->host_monitor_observation_enabled = true;
        gateway_context_source_metadata_init(&adapter->context_sources, &monitor_source_policy);
    }
}

void gateway_adapter_host_enable_monitor_binding(gateway_adapter_t *adapter)
{
    if (adapter != NULL && adapter->started) {
        adapter->host_monitor_binding_enabled = true;
    }
}

bool gateway_adapter_host_get_monitor_context(
    const gateway_adapter_t *adapter, gateway_core_context_t *out_context)
{
    if (adapter == NULL || out_context == NULL ||
        !adapter->monitor_context_bound) {
        return false;
    }
    *out_context = adapter->core.context;
    return true;
}

bool gateway_adapter_host_process_one(gateway_adapter_t *adapter,
                                      uint64_t now_ms)
{
    return process_one(adapter, now_ms, 0U);
}

bool gateway_adapter_host_tick(gateway_adapter_t *adapter, uint64_t now_ms)
{
    gateway_core_status_t status;

    if ((adapter == NULL) || !adapter->started) {
        return false;
    }
    status = process_core_event(
        adapter, gateway_core_enqueue_tick(&adapter->core, now_ms), now_ms);
    publish_safe_gateway_status(adapter, true, now_ms);
    refresh_context_candidate(adapter, now_ms);
    try_monitor_context_tx(adapter, now_ms);
    return status == GATEWAY_CORE_OK;
}

bool gateway_adapter_host_get_context_candidate(
    const gateway_adapter_t *adapter,
    uart_m_context_t *out_context)
{
    if (adapter == NULL || out_context == NULL || !adapter->started ||
        !adapter->context_candidate_valid ||
        adapter->context_candidate_generation == 0U ||
        adapter->context_candidate_generation !=
            adapter->connection_generation) {
        return false;
    }
    *out_context = adapter->context_candidate;
    return true;
}

bool gateway_adapter_host_get_context_tx_candidate(
    const gateway_adapter_t *adapter,
    uart_m_context_t *out_context)
{
    if (adapter == NULL || out_context == NULL || !adapter->started ||
        !adapter->context_tx_candidate_ready ||
        !adapter->context_candidate_valid ||
        adapter->context_candidate_generation == 0U ||
        adapter->context_candidate_generation !=
            adapter->connection_generation ||
        !gateway_adapter_p6_authorization_is_current(adapter)) {
        return false;
    }
    *out_context = adapter->context_candidate;
    return true;
}

bool gateway_adapter_host_set_core_context(
    gateway_adapter_t *adapter,
    const gateway_core_context_t *context,
    uint64_t now_ms)
{
    if (adapter == NULL || context == NULL || !adapter->started) {
        return false;
    }
    return process_core_event(
               adapter,
               gateway_core_enqueue_context(&adapter->core, context, now_ms),
               now_ms) == GATEWAY_CORE_OK;
}

bool gateway_adapter_host_configure_context_source_policy(
    gateway_adapter_t *adapter,
    uint32_t registration_connection_ttl_ms,
    uint32_t app_ttl_ms)
{
    gateway_context_source_policy_t policy;

    if (adapter == NULL || !adapter->started) {
        return false;
    }
    memset(&policy, 0, sizeof(policy));
    policy.registration_connection_ttl_ms =
        registration_connection_ttl_ms;
    policy.app_ttl_ms = app_ttl_ms;
    gateway_context_source_metadata_init(&adapter->context_sources, &policy);
    clear_context_candidate(adapter);
    return true;
}

bool gateway_adapter_host_configure_request_admission(
    gateway_adapter_t *adapter,
    uint32_t ttl_ms)
{
    gateway_request_admission_config_t config;

    if (adapter == NULL || !adapter->started) {
        return false;
    }
    memset(&config, 0, sizeof(config));
    config.ttl_ms = ttl_ms;
    gateway_request_admission_init(&adapter->request_admission, &config);
    return true;
}

gateway_request_admission_status_t
gateway_adapter_host_prepare_request_candidate(
    gateway_adapter_t *adapter,
    uint64_t now_ms,
    uart_m_request_t *out_request)
{
    gateway_request_admission_evidence_t evidence;
    gateway_request_admission_status_t status;

    if (adapter == NULL || out_request == NULL || !adapter->started) {
        return GATEWAY_REQUEST_ADMISSION_INVALID_ARGUMENT;
    }
    memset(&evidence, 0, sizeof(evidence));
    evidence.now_known = true;
    evidence.now_ms = now_ms;
    evidence.domain_link_up = gateway_core_is_domain_link_up(&adapter->core);
    evidence.domain_boot_id = adapter->domain_boot_id;
    if (ble_gatt_server_get_connection_snapshot(&evidence.connection) !=
            ESP_OK ||
        p6_session_manager_get_snapshot(&adapter->p6_manager,
                                        &evidence.authorization) !=
            P6_SESSION_OK) {
        gateway_request_admission_invalidate(&adapter->request_admission);
        return GATEWAY_REQUEST_ADMISSION_NOT_AUTHORIZED;
    }

    status = gateway_request_admission_prepare(
        &adapter->request_admission, &evidence, out_request);
    if (status == GATEWAY_REQUEST_ADMISSION_OK &&
        !p6_session_manager_context_is_current(
            &adapter->p6_manager, out_request->context.device_context_id,
            out_request->context.session_id,
            evidence.connection.connection_generation)) {
        memset(out_request, 0, sizeof(*out_request));
        return GATEWAY_REQUEST_ADMISSION_NOT_AUTHORIZED;
    }
    return status;
}

bool gateway_adapter_host_take_query_candidate(
    gateway_adapter_t *adapter,
    uint64_t now_ms,
    uart_m_query_t *out_query)
{
    mobile_query_candidate_t candidate;
    gateway_query_state_t query;
    gateway_query_outcome_t outcome;
    p6_authorization_snapshot_t authorization;
    ble_connection_snapshot_t connection;
    bool valid;

    if (adapter == NULL || out_query == NULL || !adapter->started) {
        return false;
    }
    memset(out_query, 0, sizeof(*out_query));
    if (!adapter->query_candidate.valid) {
        return false;
    }
    candidate = adapter->query_candidate;
    gateway_core_get_query_state(&adapter->core, &query, &outcome);
    (void)outcome;
    valid = now_ms >= candidate.received_at_ms &&
        (now_ms - candidate.received_at_ms) <=
            adapter->core.config.query_timeout_ms &&
        adapter->connected && gateway_core_is_domain_link_up(&adapter->core) &&
        adapter->domain_boot_id == candidate.domain_boot_id &&
        query.active &&
        query.device_context_id == candidate.query.context.device_context_id &&
        query.session_id == candidate.query.context.session_id &&
        query.domain_boot_id == candidate.domain_boot_id &&
        query.query_id == candidate.query.query_id &&
        query.scope == candidate.query.scope &&
        gateway_adapter_p6_authorization_is_current(adapter) &&
        p6_session_manager_get_snapshot(&adapter->p6_manager,
                                        &authorization) == P6_SESSION_OK &&
        p6_session_manager_context_is_current(
            &adapter->p6_manager,
            candidate.query.context.device_context_id,
            candidate.query.context.session_id,
            candidate.connection_generation) &&
        authorization.domain_boot_id == candidate.domain_boot_id &&
        ble_gatt_server_get_connection_snapshot(&connection) == ESP_OK &&
        connection.available && connection.connected &&
        connection.connection_handle == candidate.connection_handle &&
        connection.connection_generation == candidate.connection_generation &&
        connection.connection_epoch == candidate.connection_epoch &&
        adapter->connection_handle == candidate.connection_handle &&
        adapter->connection_generation == candidate.connection_generation &&
        adapter->connection_epoch == candidate.connection_epoch;
    if (!valid) {
        clear_query_candidate(adapter, true);
        return false;
    }

    *out_query = candidate.query;
    clear_query_candidate(adapter, false);
    return true;
}
#endif
