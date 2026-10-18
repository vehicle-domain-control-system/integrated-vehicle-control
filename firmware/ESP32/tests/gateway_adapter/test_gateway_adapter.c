#include "gateway_adapter.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mock_idf.h"

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return false; } } while (0)

static uint8_t s_status[BLE_GATT_GATEWAY_STATUS_LENGTH];
static esp_err_t s_notify_result;
static unsigned s_gateway_notify_calls;
static unsigned s_vehicle_notify_calls;

const char *esp_err_to_name(esp_err_t error) { (void)error; return "mock"; }
esp_err_t ble_gatt_server_set_gateway_status(const uint8_t status[BLE_GATT_GATEWAY_STATUS_LENGTH])
{ memcpy(s_status, status, sizeof(s_status)); return ESP_OK; }
esp_err_t ble_gatt_server_send_gateway_status(ble_gatt_send_mode_t mode)
{
    if (mode != BLE_GATT_SEND_NOTIFY) return ESP_FAIL;
    s_gateway_notify_calls++;
    return s_notify_result;
}
esp_err_t ble_gatt_server_send_vehicle_data(uint8_t type, const uint8_t *payload,
                                             size_t length, ble_gatt_send_mode_t mode)
{
    (void)type; (void)payload; (void)length; (void)mode;
    s_vehicle_notify_calls++;
    return ESP_OK;
}

static gateway_adapter_t *start_adapter(size_t depth)
{
    gateway_adapter_t *adapter = NULL;
    gateway_adapter_config_t config = gateway_adapter_safe_config();
    config.queue_depth = depth;
    memset(s_status, 0, sizeof(s_status));
    s_notify_result = ESP_OK;
    s_gateway_notify_calls = 0U;
    s_vehicle_notify_calls = 0U;
    return gateway_adapter_start(&adapter, &config) == ESP_OK ? adapter : NULL;
}

static ble_gatt_server_event_t ble_event(ble_gatt_event_type_t type,
                                         uint16_t handle, uint32_t generation)
{
    ble_gatt_server_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.connection_handle = handle;
    event.connection_generation = generation;
    return event;
}

static bool process_ble_event(gateway_adapter_t *adapter,
                              ble_gatt_server_event_t *event,
                              uint64_t now_ms)
{
    gateway_adapter_on_ble_event(event, adapter);
    return gateway_adapter_host_process_one(adapter, now_ms);
}

static void make_alive(uart_m_transport_rx_message_t *rx, uint32_t boot_id)
{
    memset(rx, 0, sizeof(*rx));
    rx->message.type = UART_M_TYPE_DOMAIN_ALIVE;
    rx->message.body.domain_alive.domain_boot_id = boot_id;
    rx->message.body.domain_alive.update_sequence = 10U;
    rx->message.body.domain_alive.source_age_ms = 1U;
    rx->message.body.domain_alive.domain_state = UART_M_CONTROLLER_READY;
    rx->message.body.domain_alive.quality = UART_M_QUALITY_OK;
    rx->received_at_us = INT64_C(5000000000000);
}

static bool test_callbacks_deep_copy_and_queue_full(void)
{
    gateway_adapter_t *adapter = start_adapter(1U);
    ble_gatt_server_event_t first = ble_event(BLE_GATT_EVENT_CONNECTED, 7U, 1U);
    ble_gatt_server_event_t second = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 7U, 1U);
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    gateway_adapter_on_ble_event(&first, adapter);
    first.type = (ble_gatt_event_type_t)99;
    gateway_adapter_on_ble_event(&second, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, UINT64_C(5000000000)));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.ble_events_enqueued == 1U);
    CHECK(stats.queue_full == 1U);
    CHECK(stats.invalid_callbacks == 0U);
    CHECK(stats.events_processed == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_subscription_mtu_then_security(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 7U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 7U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 7U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 7U, 1U);
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_pending);
    CHECK(s_gateway_notify_calls == 0U);

    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(s_gateway_notify_calls == 0U);
    subscription.data.subscription.gateway_status_notify = true;
    subscription.data.subscription.vehicle_data_notify = true;
    CHECK(process_ble_event(adapter, &subscription, 2U));
    CHECK(s_gateway_notify_calls == 0U);
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &mtu, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    security.data.security.encrypted = true;
    security.data.security.authenticated = false;
    security.data.security.bonded = true;
    CHECK(process_ble_event(adapter, &security, 4U));
    CHECK(s_gateway_notify_calls == 1U);

    CHECK(process_ble_event(adapter, &security, 5U));
    CHECK(process_ble_event(adapter, &subscription, 6U));
    CHECK(process_ble_event(adapter, &mtu, 7U));
    CHECK(s_gateway_notify_calls == 1U);
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(!stats.gateway_status_notify_pending);
    CHECK(stats.gateway_status_notify_api_accepted == 1U);
    CHECK(stats.gateway_status_notify_failures == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_security_then_subscription(void)
{
    gateway_adapter_t *adapter = start_adapter(6U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 8U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 8U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 8U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 8U, 1U);
    CHECK(adapter != NULL);
    mtu.data.mtu.mtu = 185U;
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &mtu, 2U));
    CHECK(process_ble_event(adapter, &security, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &subscription, 4U));
    CHECK(s_gateway_notify_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_gate_security_subscription_then_mtu(void)
{
    gateway_adapter_t *adapter = start_adapter(6U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 9U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 9U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 9U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 9U, 1U);
    CHECK(adapter != NULL);
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    mtu.data.mtu.mtu = 185U;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &security, 2U));
    CHECK(process_ble_event(adapter, &subscription, 3U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &mtu, 4U));
    CHECK(s_gateway_notify_calls == 1U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_disconnect_stale_events_and_app_state_copy(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 10U, 1U);
    ble_gatt_server_event_t app_state = ble_event(BLE_GATT_EVENT_APP_STATE, 10U, 1U);
    ble_gatt_server_event_t disconnected = ble_event(BLE_GATT_EVENT_DISCONNECTED, 10U, 1U);
    ble_gatt_server_event_t reconnect = ble_event(BLE_GATT_EVENT_CONNECTED, 10U, 2U);
    ble_gatt_server_event_t stale_mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 10U, 1U);
    ble_gatt_server_event_t stale_subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 10U, 1U);
    uart_m_transport_rx_message_t alive;
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    CHECK(process_ble_event(adapter, &connected, 1U));
    app_state.data.app_state.active = true;
    app_state.data.app_state.app_instance_id = 1234U;
    gateway_adapter_on_ble_event(&app_state, adapter);
    app_state.data.app_state.active = false;
    CHECK(gateway_adapter_host_process_one(adapter, 2U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.app_state_events_processed == 1U);
    CHECK(stats.app_state_seen);
    CHECK(stats.app_state_active);

    make_alive(&alive, 10U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 3U));
    make_alive(&alive, 11U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 4U));

    CHECK(process_ble_event(adapter, &disconnected, 5U));
    make_alive(&alive, 12U);
    gateway_adapter_on_uart_message(&alive, adapter);
    CHECK(gateway_adapter_host_process_one(adapter, 6U));
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(process_ble_event(adapter, &reconnect, 7U));
    stale_mtu.data.mtu.mtu = 185U;
    stale_subscription.data.subscription.gateway_status_notify = true;
    CHECK(process_ble_event(adapter, &stale_mtu, 8U));
    CHECK(process_ble_event(adapter, &stale_subscription, 9U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.stale_ble_events == 2U);
    CHECK(stats.disconnects_processed == 1U);
    CHECK(stats.domain_boot_changes == 2U);
    CHECK(s_gateway_notify_calls == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

static bool test_notify_failure_retains_pending_and_command_stays_blocked(void)
{
    gateway_adapter_t *adapter = start_adapter(8U);
    ble_gatt_server_event_t connected = ble_event(BLE_GATT_EVENT_CONNECTED, 11U, 1U);
    ble_gatt_server_event_t mtu = ble_event(BLE_GATT_EVENT_MTU_CHANGED, 11U, 1U);
    ble_gatt_server_event_t security = ble_event(BLE_GATT_EVENT_SECURITY_CHANGED, 11U, 1U);
    ble_gatt_server_event_t subscription = ble_event(BLE_GATT_EVENT_SUBSCRIPTION_CHANGED, 11U, 1U);
    ble_gatt_server_event_t command = ble_event(BLE_GATT_EVENT_APP_COMMAND, 11U, 1U);
    gateway_adapter_stats_t stats;
    CHECK(adapter != NULL);
    mtu.data.mtu.mtu = 185U;
    security.data.security.encrypted = true;
    security.data.security.bonded = true;
    subscription.data.subscription.gateway_status_notify = true;
    command.data.app_command.message_type = 0x10U;
    command.data.app_command.payload_length = 12U;
    command.data.app_command.payload[0] = 1U;
    CHECK(process_ble_event(adapter, &connected, 1U));
    CHECK(process_ble_event(adapter, &mtu, 2U));
    CHECK(process_ble_event(adapter, &security, 3U));
    s_notify_result = ESP_FAIL;
    CHECK(process_ble_event(adapter, &subscription, 4U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_failures == 1U);
    CHECK(stats.gateway_status_notify_pending);
    s_notify_result = ESP_OK;
    CHECK(process_ble_event(adapter, &security, 5U));
    CHECK(s_gateway_notify_calls == 2U);
    CHECK(process_ble_event(adapter, &command, 6U));
    gateway_adapter_get_stats(adapter, &stats);
    CHECK(stats.gateway_status_notify_api_accepted == 1U);
    CHECK(!stats.gateway_status_notify_pending);
    CHECK(stats.app_commands_blocked == 1U);
    CHECK(stats.core_rejections == 1U);
    CHECK(stats.production_uart_tx_count == 0U);
    CHECK(s_vehicle_notify_calls == 0U);
    CHECK(gateway_adapter_is_safe_mode(adapter));
    CHECK(gateway_adapter_stop(&adapter) == ESP_OK);
    return true;
}

int main(void)
{
    unsigned passed = 0U;
    const struct { const char *name; bool (*run)(void); } tests[] = {
        {"callbacks_deep_copy_and_queue_full", test_callbacks_deep_copy_and_queue_full},
        {"notify_gate_subscription_mtu_then_security", test_notify_gate_subscription_mtu_then_security},
        {"notify_gate_security_then_subscription", test_notify_gate_security_then_subscription},
        {"notify_gate_security_subscription_then_mtu", test_notify_gate_security_subscription_then_mtu},
        {"disconnect_stale_events_and_app_state_copy", test_disconnect_stale_events_and_app_state_copy},
        {"notify_failure_retains_pending_and_command_stays_blocked", test_notify_failure_retains_pending_and_command_stays_blocked},
    };
    size_t i;
    mock_idf_reset();
    for (i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!tests[i].run()) { printf("FAILED: %s\n", tests[i].name); return 1; }
        passed++;
    }
    printf("PASS: %u gateway adapter tests\n", passed);
    return 0;
}
