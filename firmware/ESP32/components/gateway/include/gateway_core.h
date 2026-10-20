#ifndef GATEWAY_CORE_H
#define GATEWAY_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gateway_mapper.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_CORE_MAX_QUEUE_DEPTH 16U
#define GATEWAY_CORE_REQUEST_CACHE_DEPTH 8U
#define GATEWAY_CORE_MAX_QUERY_RESPONSES 32U
#define GATEWAY_CORE_MAX_BLE_UPLINK_PAYLOAD 20U
#define GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED 0

typedef enum
{
    GATEWAY_CORE_OK = 0,
    GATEWAY_CORE_NO_EVENT,
    GATEWAY_CORE_INVALID_ARGUMENT,
    GATEWAY_CORE_QUEUE_FULL,
    GATEWAY_CORE_INVALID_MESSAGE,
    GATEWAY_CORE_NOT_READY,
    GATEWAY_CORE_STALE,
    GATEWAY_CORE_CONTEXT_MISMATCH,
    GATEWAY_CORE_DUPLICATE_REQUEST,
    GATEWAY_CORE_REQUEST_CACHE_FULL,
    GATEWAY_CORE_DUPLICATE_QUERY,
    GATEWAY_CORE_DUPLICATE_RESPONSE,
    GATEWAY_CORE_QUERY_CAPACITY_OVERFLOW,
    GATEWAY_CORE_QUERY_BUSY,
    GATEWAY_CORE_QUERY_COUNT_MISMATCH,
    GATEWAY_CORE_MOCK_PORT_REJECTED,
    GATEWAY_CORE_ROUTING_HOLD
} gateway_core_status_t;

typedef enum
{
    GATEWAY_QUERY_OUTCOME_NONE = 0,
    GATEWAY_QUERY_OUTCOME_COMPLETE_CANDIDATE_HOLD,
    GATEWAY_QUERY_OUTCOME_NOT_FOUND_HOLD,
    GATEWAY_QUERY_OUTCOME_PARTIAL,
    GATEWAY_QUERY_OUTCOME_CONTEXT_REJECTED,
    GATEWAY_QUERY_OUTCOME_COUNT_MISMATCH,
    GATEWAY_QUERY_OUTCOME_CAPACITY_OVERFLOW,
    GATEWAY_QUERY_OUTCOME_TIMEOUT,
    GATEWAY_QUERY_OUTCOME_DISCONNECTED,
    GATEWAY_QUERY_OUTCOME_DOMAIN_BOOT_CHANGED,
    GATEWAY_QUERY_OUTCOME_SESSION_ROTATED
} gateway_query_outcome_t;

typedef struct
{
    gateway_verified_context_t identity;
    uint32_t connection_generation;
    bool connected;
    bool encrypted;
    bool authenticated; /* BLE SMP MITM/authenticated-link fact only. */
    bool registered;
    bool session_ready;
    /* Explicit P6 or isolated-Mock authorization; never inferred from BLE. */
    bool authorization_verified;
    uint64_t observed_at_ms;
} gateway_core_context_t;

/*
 * These are isolated Host-observer ports, not production transport ports.
 * Candidate pointers are borrowed and valid only for the callback duration;
 * an observer retaining data must copy it before returning. A true return
 * means only that the test observer accepted the candidate. It never means
 * UART TX, Android receipt, Domain ACCEPTED or vehicle DONE.
 */
typedef bool (*gateway_core_uart_candidate_fn)(const uart_m_message_t *message,
                                               void *user_context);
typedef bool (*gateway_core_ble_candidate_fn)(const gateway_ble_message_t *message,
                                              void *user_context);

typedef struct
{
    gateway_core_uart_candidate_fn observe_uart_candidate;
    gateway_core_ble_candidate_fn observe_ble_candidate;
    void *user_context;
} gateway_core_mock_ports_t;

typedef struct
{
    size_t queue_depth;
    uint32_t query_timeout_ms;
    uint32_t context_stale_after_ms;
    uint32_t event_stale_after_ms;
    uint32_t domain_alive_timeout_ms;
    gateway_core_mock_ports_t mock_ports;
} gateway_core_config_t;

typedef struct
{
    bool active;
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t domain_boot_id;
    uint32_t query_id;
    uart_m_query_scope_t scope;
    uint64_t started_at_ms;
    uint16_t unique_response_count;
} gateway_query_state_t;

typedef struct
{
    uint32_t events_enqueued;
    uint32_t events_processed;
    uint32_t queue_full;
    uint32_t invalid_messages;
    uint32_t stale_events;
    uint32_t context_rejects;
    uint32_t duplicate_requests;
    uint32_t request_cache_full;
    uint32_t duplicate_queries;
    uint32_t duplicate_query_responses;
    uint32_t query_capacity_overflows;
    uint32_t query_count_mismatches;
    uint32_t query_timeouts;
    uint32_t domain_boot_changes;
    uint32_t disconnects;
    uint32_t session_rotations;
    uint32_t uart_candidates;
    uint32_t ble_candidates;
    uint32_t mock_port_rejects;
    uint32_t routing_holds;
} gateway_core_stats_t;

typedef enum
{
    GATEWAY_CORE_EVENT_CONTEXT = 0,
    GATEWAY_CORE_EVENT_BLE_COMMAND,
    GATEWAY_CORE_EVENT_UART_MESSAGE,
    GATEWAY_CORE_EVENT_DISCONNECT,
    GATEWAY_CORE_EVENT_TICK
} gateway_core_event_type_t;

typedef struct
{
    uint8_t message_type;
    size_t payload_length;
    uint32_t connection_generation;
    uint8_t payload[GATEWAY_CORE_MAX_BLE_UPLINK_PAYLOAD];
} gateway_core_ble_command_t;

typedef struct
{
    gateway_core_event_type_t type;
    uint64_t received_at_ms;
    union {
        gateway_core_context_t context;
        gateway_core_ble_command_t ble_command;
        uart_m_message_t uart_message;
        uint32_t disconnect_generation;
    } data;
} gateway_core_event_t;

typedef struct
{
    uint32_t device_context_id;
    uint64_t session_id;
    uint32_t request_id;
    bool used;
} gateway_request_key_t;

typedef struct
{
    uint64_t value;
    bool used;
} gateway_response_key_t;

/* Public storage is intentional: P5-B uses no heap allocation. */
typedef struct
{
    bool initialized;
    gateway_core_config_t config;
    gateway_core_context_t context;
    bool has_context;
    bool domain_link_up;
    uint32_t domain_boot_id;
    uint64_t domain_last_seen_ms;
    gateway_query_state_t query;
    gateway_query_outcome_t last_query_outcome;
    gateway_core_event_t events[GATEWAY_CORE_MAX_QUEUE_DEPTH];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
    gateway_request_key_t request_keys[GATEWAY_CORE_REQUEST_CACHE_DEPTH];
    size_t request_key_count;
    gateway_response_key_t response_keys[GATEWAY_CORE_MAX_QUERY_RESPONSES];
    size_t response_key_count;
    gateway_core_stats_t stats;
} gateway_core_t;

/* Proposed timeout fixtures for Host tests only; not a production policy. */
gateway_core_config_t gateway_core_isolated_mock_config(void);
gateway_core_status_t gateway_core_init(gateway_core_t *core,
                                        const gateway_core_config_t *config);
void gateway_core_reset(gateway_core_t *core);

/*
 * Enqueue APIs copy all input bytes before returning and never block. The P5-B
 * ring itself is single-owner Host code; a future FreeRTOS adapter must add a
 * short non-blocking producer synchronization boundary around these calls.
 */
gateway_core_status_t gateway_core_enqueue_context(gateway_core_t *core,
                                                   const gateway_core_context_t *context,
                                                   uint64_t received_at_ms);
gateway_core_status_t gateway_core_enqueue_ble_command(gateway_core_t *core,
                                                       uint8_t message_type,
                                                       const uint8_t *payload,
                                                       size_t payload_length,
                                                       uint32_t connection_generation,
                                                       uint64_t received_at_ms);
gateway_core_status_t gateway_core_enqueue_uart_message(gateway_core_t *core,
                                                        const uart_m_message_t *message,
                                                        uint64_t received_at_ms);
gateway_core_status_t gateway_core_enqueue_disconnect(gateway_core_t *core,
                                                      uint32_t connection_generation,
                                                      uint64_t received_at_ms);
gateway_core_status_t gateway_core_enqueue_tick(gateway_core_t *core,
                                                uint64_t now_ms);

/*
 * Processes at most one copied event on the single Gateway owner context.
 * Times must come from a non-wrapping uint64_t monotonic millisecond source
 * (for ESP-IDF, esp_timer_get_time()/1000 is suitable). A wrapping 32-bit
 * FreeRTOS tick must be extended before it is passed to this API.
 */
gateway_core_status_t gateway_core_process_next(gateway_core_t *core,
                                                uint64_t now_ms);

void gateway_core_get_stats(const gateway_core_t *core,
                            gateway_core_stats_t *out_stats);
void gateway_core_get_query_state(const gateway_core_t *core,
                                  gateway_query_state_t *out_query,
                                  gateway_query_outcome_t *out_last_outcome);
bool gateway_core_is_domain_link_up(const gateway_core_t *core);
bool gateway_core_production_routing_enabled(void);

#ifdef __cplusplus
}
#endif

#endif
