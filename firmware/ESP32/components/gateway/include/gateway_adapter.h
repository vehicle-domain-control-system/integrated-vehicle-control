#ifndef GATEWAY_ADAPTER_H
#define GATEWAY_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ble_gatt_server.h"
#include "esp_err.h"
#include "uart_m_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_ADAPTER_MAX_QUEUE_DEPTH 16U

typedef struct gateway_adapter gateway_adapter_t;

typedef struct {
    size_t queue_depth;
    uint32_t task_stack_size;
    unsigned task_priority;
    uint32_t tick_period_ms;
} gateway_adapter_config_t;

typedef struct {
    uint32_t ble_events_enqueued;
    uint32_t uart_events_enqueued;
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
    bool gateway_status_notify_pending;
    bool app_state_seen;
    bool app_state_active;
} gateway_adapter_stats_t;

gateway_adapter_config_t gateway_adapter_safe_config(void);

/*
 * Starts the single GatewayTask owner. This safe adapter intentionally has no
 * UART TX port and no P6 authorization provider, so it cannot emit a vehicle
 * command or create a production Context.
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

bool gateway_adapter_is_safe_mode(const gateway_adapter_t *adapter);

#ifdef GATEWAY_ADAPTER_HOST_TEST
/* Host-only deterministic task pump; never compiled into ESP-IDF Firmware. */
bool gateway_adapter_host_process_one(gateway_adapter_t *adapter,
                                      uint64_t now_ms);
#endif

#ifdef __cplusplus
}
#endif

#endif
