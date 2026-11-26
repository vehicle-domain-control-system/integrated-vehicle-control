#ifndef GATEWAY_ADAPTER_H
#define GATEWAY_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ble_gatt_server.h"
#include "esp_err.h"
#include "p6_session_manager.h"
#include "gateway_context_source_metadata.h"
#include "uart_m_transport.h"

#ifdef GATEWAY_ADAPTER_HOST_TEST
#include "gateway_core.h"
#include "gateway_request_admission.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_ADAPTER_MAX_QUEUE_DEPTH 16U
#define GATEWAY_MONITOR_CONTEXT_PERIOD_MS BLE_GATT_MONITOR_OBSERVATION_PERIOD_MS
#define GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS 600U
#define GATEWAY_MONITOR_APP_STATE_TTL_MS 1000U
/* Remote receive lifetime, NOT an approved UART Queue dwell policy. */
#define GATEWAY_MONITOR_DOMAIN_CONTEXT_TTL_MS 750U

typedef struct gateway_adapter gateway_adapter_t;

/* All values must come from an approved Domain/ESP32 contract. Zero/default
 * is HOLD. approved also asserts agreement on Domain revocation and UART
 * queue dwell/freshness treatment; it is never set by BLE or Registration. */
typedef struct {
    bool approved;
    uint32_t refresh_period_ms;
    uint32_t queued_context_ttl_ms;
    uint32_t proximity_ttl_ms;
    gateway_context_source_policy_t sources;
} gateway_monitor_context_tx_policy_t;

/* Nonblocking deep-copy event to GatewayTask. NULL policy installs the port
 * with TX closed. The Transport is borrowed: stop Adapter before deinit of
 * Transport; do not deinit concurrently or rebind a freed Transport.
 * A full binding queue latches TX closed until Adapter restart.
 * ESP_OK means binding event queued, not M_CONTEXT/UART/Domain acceptance. */
esp_err_t gateway_adapter_bind_monitor_context_tx(
    gateway_adapter_t *adapter, uart_m_transport_t *transport,
    const gateway_monitor_context_tx_policy_t *policy);

typedef struct {
    size_t queue_depth;
    uint32_t task_stack_size;
    unsigned task_priority;
    uint32_t tick_period_ms;
    /* Borrowed for the adapter lifetime; NULL keeps observation-only storage. */
    const p6_registration_store_t *registration_store;
    const p6_entropy_source_t *entropy_source;
} gateway_adapter_config_t;

typedef struct {
    uint32_t ble_events_enqueued;
    uint32_t uart_events_enqueued;
    uint32_t uart_messages_processed;
    uint32_t uart_messages_rejected;
    uint32_t uart_queue_full;
    uint32_t uart_domain_alive_accepted;
    uint32_t uart_downlink_held;
    uint32_t events_processed;
    uint32_t queue_full;
    uint32_t invalid_callbacks;
    uint32_t core_rejections;
    uint32_t app_commands_blocked;
    uint32_t disconnects_processed;
    uint32_t domain_boot_changes;
    uint32_t gateway_status_notify_deferred;
    uint32_t gateway_status_notify_api_accepted;
    uint32_t gateway_status_notify_failures;
    uint32_t app_state_events_processed;
    uint32_t stale_ble_events;
    uint32_t production_uart_tx_count;
    uint32_t production_m_context_tx_count;
    uint32_t digital_key_auto_unlock_count;
    uint32_t monitor_context_tx_queued;
    uint32_t monitor_context_tx_backpressure;
    uint32_t monitor_context_tx_errors;
    uint32_t monitor_peer_observations_accepted;
    uint32_t monitor_peer_observations_rejected;
    uint32_t p6_identity_rejections;
    uint32_t p6_stale_events;
    uint32_t p6_session_start_attempts;
    uint32_t p6_bond_reconciliation_events;
    uint32_t p6_bond_reconciliation_failures;
    uint32_t ble_epoch_invalidations;
    uint32_t ble_snapshot_fail_closed;
    uint32_t context_dto_attempts;
    uint32_t context_dto_rejections;
    uint32_t context_dto_generation_rejections;
    uint32_t vehicle_data_candidates_queued;
    uint32_t vehicle_data_candidate_queue_full;
    uint32_t vehicle_data_notify_deferred;
    uint32_t vehicle_data_notify_api_accepted;
    uint32_t vehicle_data_notify_failures;
    uint32_t vehicle_data_candidates_invalidated;
    uint32_t query_candidates_queued;
    uint32_t query_candidate_queue_full;
    uint32_t query_candidates_invalidated;
    uint32_t registration_requests_enqueued;
    uint32_t registration_approvals_enqueued;
    uint32_t registration_request_rejections;
    uint32_t registration_approval_rejections;
    uint32_t registration_commits;
    uint32_t registration_input_queue_failures;
    bool gateway_status_notify_pending;
    bool app_state_seen;
    bool app_state_active;
    bool p6_registration_available;
    bool p6_registration_bond_reconciled;
    bool p6_authorization_verified;
    bool registration_request_pending;
    bool context_dto_ready;
    bool query_candidate_pending;
    uint32_t vehicle_data_pending_count;
} gateway_adapter_stats_t;

typedef struct {
    uint32_t uart_messages_rejected;
    uint32_t uart_queue_full;
    uint32_t uart_domain_alive_decoded;
    uint32_t uart_domain_alive_accepted;
    uint32_t uart_domain_boot_observed_changes;
    uint32_t uart_last_link_sequence;
    uint32_t uart_last_domain_boot_id;
    uint32_t uart_last_update_sequence;
    uint32_t uart_last_source_age_ms;
    uint32_t uart_last_domain_state;
    uint32_t uart_last_quality;
    bool uart_domain_link_up;
} gateway_adapter_uart_rx_diagnostics_t;

gateway_adapter_config_t gateway_adapter_safe_config(void);

/*
 * Starts the single GatewayTask owner. This safe adapter intentionally has no
 * UART TX port and no P6 authorization provider. A persisted Registration is
 * only an observation candidate after boot-time exact Bond reconciliation; it
 * still cannot emit a vehicle command or create a production Context.
 */
esp_err_t gateway_adapter_start(gateway_adapter_t **out_adapter,
                                const gateway_adapter_config_t *config);

/* Stop producers (BLE/UART) before calling stop. */
esp_err_t gateway_adapter_stop(gateway_adapter_t **adapter);

/* Producer callbacks: bounded full-value copy and zero-wait queue admission. */
void gateway_adapter_on_ble_event(const ble_gatt_server_event_t *event,
                                  void *context);
void gateway_adapter_on_uart_message(const uart_m_transport_rx_message_t *message,
                                     void *context);

void gateway_adapter_get_stats(const gateway_adapter_t *adapter,
                               gateway_adapter_stats_t *out_stats);

/* Read-only atomic snapshot; it never mutates GatewayTask-owned state. */
void gateway_adapter_get_uart_rx_diagnostics(
    const gateway_adapter_t *adapter,
    gateway_adapter_uart_rx_diagnostics_t *out_diagnostics);

bool gateway_adapter_is_safe_mode(const gateway_adapter_t *adapter);

/* Observation-only admission gate; Production binding remains disabled. */
bool gateway_adapter_p6_authorization_is_current(
    const gateway_adapter_t *adapter);

#if defined(CONFIG_P6_DEV_REGISTRATION_APPROVAL) && \
    CONFIG_P6_DEV_REGISTRATION_APPROVAL
/*
 * Development UART-console input only.  Each call snapshots the current BLE
 * connection generation/epoch and enqueues a zero-wait event.  Neither call
 * directly mutates P6 state and neither is present in the default build.
 */
esp_err_t gateway_adapter_development_request_registration(
    gateway_adapter_t *adapter);
esp_err_t gateway_adapter_development_approve_registration(
    gateway_adapter_t *adapter);
#endif

#ifdef GATEWAY_ADAPTER_HOST_TEST
/* Exercise the gated Runtime bridge using the real FSM with isolated ports.
 * This does not supply Authorization or change the firmware Production gate. */
void gateway_adapter_host_enable_monitor_binding(gateway_adapter_t *adapter);
void gateway_adapter_host_enable_monitor_observation(gateway_adapter_t *adapter);
bool gateway_adapter_host_get_monitor_context(
    const gateway_adapter_t *adapter, gateway_core_context_t *out_context);
/* Host-only deterministic task pump; never compiled into ESP-IDF Firmware. */
bool gateway_adapter_host_process_one(gateway_adapter_t *adapter,
                                      uint64_t now_ms);
/* Host-only deterministic supervisor tick with no queued producer event. */
bool gateway_adapter_host_tick(gateway_adapter_t *adapter, uint64_t now_ms);
/* Host-only copy of the GatewayTask-owned DTO candidate; never transmits it. */
bool gateway_adapter_host_get_context_candidate(
    const gateway_adapter_t *adapter,
    uart_m_context_t *out_context);
/*
 * Host-only bounded pre-TX boundary. It returns a deep copy only when both
 * real Registration/Connection and App metadata policies are configured and
 * observed for the current authorized generation. It never calls UART TX.
 */
bool gateway_adapter_host_get_context_tx_candidate(
    const gateway_adapter_t *adapter,
    uart_m_context_t *out_context);
/* Host-only: installs an already verified Core fixture; never in Firmware. */
bool gateway_adapter_host_set_core_context(
    gateway_adapter_t *adapter,
    const gateway_core_context_t *context,
    uint64_t now_ms);
/* Host-only explicit metadata policy; zero TTL keeps a source NO_DATA. */
bool gateway_adapter_host_configure_context_source_policy(
    gateway_adapter_t *adapter,
    uint32_t registration_connection_ttl_ms,
    uint32_t app_ttl_ms);
/* Host-only explicit policy input; ttl_ms=0 restores fail-closed behavior. */
bool gateway_adapter_host_configure_request_admission(
    gateway_adapter_t *adapter,
    uint32_t ttl_ms);
/* Host-only pre-UART observation boundary; this never transmits. */
gateway_request_admission_status_t
gateway_adapter_host_prepare_request_candidate(
    gateway_adapter_t *adapter,
    uint64_t now_ms,
    uart_m_request_t *out_request);
/* Host-only pre-UART read-only Query boundary; never transmits. */
bool gateway_adapter_host_take_query_candidate(
    gateway_adapter_t *adapter,
    uint64_t now_ms,
    uart_m_query_t *out_query);
#endif

#ifdef __cplusplus
}
#endif

#endif
