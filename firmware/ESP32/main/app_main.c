#include "driver/uart.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "nvs_flash.h"

#include "ble_gatt_server.h"
#include "gateway_adapter.h"
#include "p6_nvs_registration_store.h"
#include "uart_m_transport.h"

#if CONFIG_P6_NVS_DEV_TEST
#include "p6_nvs_dev_test.h"
#endif

#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
#include "p6_registration_dev_console.h"
#endif

static const char *TAG = "gateway_main";
#define P6_DEV_REGISTRATION_NAMESPACE "p6_regdev"
static p6_nvs_registration_store_t registration_store;
static p6_registration_store_t registration_port;

#if CONFIG_P6_UART_RX_DIAGNOSTICS
#define UART_RX_DIAG_PERIOD_MS 5000U
#define UART_RX_DIAG_TASK_STACK 3072U

typedef struct {
    uart_m_transport_t *transport;
    gateway_adapter_t *adapter;
} uart_rx_diag_context_t;

static uart_rx_diag_context_t uart_rx_diag_context;

static void uart_rx_diag_task(void *argument)
{
    const uart_rx_diag_context_t *context =
        (const uart_rx_diag_context_t *)argument;

    while (true) {
        uart_m_transport_stats_t transport = {0};
        gateway_adapter_uart_rx_diagnostics_t gateway = {0};

        vTaskDelay(pdMS_TO_TICKS(UART_RX_DIAG_PERIOD_MS));
        uart_m_transport_get_stats(context->transport, &transport);
        gateway_adapter_get_uart_rx_diagnostics(context->adapter, &gateway);

        ESP_LOGI("uart_rx_diag",
                 "RX bytes=%" PRIu32 " frames=%" PRIu32
                 " delivered=%" PRIu32 " alive_decoded=%" PRIu32
                 " supervisor_ok=%" PRIu32 " link=%s queue_full=%" PRIu32,
                 transport.rx_bytes, transport.rx_frames_valid,
                 transport.rx_messages_delivered,
                 gateway.uart_domain_alive_decoded,
                 gateway.uart_domain_alive_accepted,
                 gateway.uart_domain_link_up ? "UP" : "DOWN",
                 gateway.uart_queue_full);
        ESP_LOGI("uart_rx_diag",
                 "Last Alive link_seq=%" PRIu32 " boot=0x%08" PRIX32
                 " update_seq=%" PRIu32 " age_ms=%" PRIu32
                 " state=%" PRIu32 " quality=%" PRIu32
                 " boot_changes=%" PRIu32,
                 gateway.uart_last_link_sequence,
                 gateway.uart_last_domain_boot_id,
                 gateway.uart_last_update_sequence,
                 gateway.uart_last_source_age_ms,
                 gateway.uart_last_domain_state,
                 gateway.uart_last_quality,
                 gateway.uart_domain_boot_observed_changes);
        ESP_LOGI("uart_rx_diag",
                 "Errors version=%" PRIu32 " length=%" PRIu32
                 " crc=%" PRIu32 " codec=%" PRIu32
                 " direction=%" PRIu32 " parser_overflow=%" PRIu32
                 " uart_overflow=%" PRIu32 " uart_frame=%" PRIu32
                 " assembly_timeout=%" PRIu32 " delivery_full=%" PRIu32
                 " gateway_rejected=%" PRIu32,
                 transport.rx_version_errors, transport.rx_length_errors,
                 transport.rx_crc_errors, transport.rx_codec_errors,
                 transport.rx_direction_errors,
                 transport.rx_parser_overflow_errors,
                 transport.uart_rx_overflow_errors,
                 transport.uart_frame_errors,
                 transport.frame_assembly_timeouts,
                 transport.rx_delivery_queue_full,
                 gateway.uart_messages_rejected);
    }
}
#endif

static bool entropy_u32(void *context, uint32_t *out_value)
{
    (void)context;
    if (out_value == NULL) {
        return false;
    }
    esp_fill_random(out_value, sizeof(*out_value));
    return true;
}

static bool entropy_u64(void *context, uint64_t *out_value)
{
    (void)context;
    if (out_value == NULL) {
        return false;
    }
    esp_fill_random(out_value, sizeof(*out_value));
    return true;
}

static const p6_entropy_source_t entropy_source = {
    .next_u32 = entropy_u32,
    .next_u64 = entropy_u64,
    .context = NULL,
};

static uart_m_transport_config_t uart2_config(void)
{
    uart_m_transport_config_t config = {0};

    config.uart_port = UART_NUM_2;
    config.tx_gpio = 27;
    config.rx_gpio = 26;
    config.rts_gpio = UART_PIN_NO_CHANGE;
    config.cts_gpio = UART_PIN_NO_CHANGE;
    config.baud_rate = 115200U;
    config.data_bits = 8U;
    config.parity = UART_M_TRANSPORT_PARITY_NONE;
    config.stop_bits = UART_M_TRANSPORT_STOP_BITS_1;
    config.flow_control = UART_M_TRANSPORT_FLOW_NONE;
    config.driver_rx_buffer_size = 512U;
    config.driver_tx_buffer_size = 512U;
    config.driver_event_queue_depth = 16U;
    config.tx_queue_depth = 8U;
    config.rx_delivery_queue_depth = 8U;
    config.rx_chunk_size = 64U;
    config.uart_event_wait_ms = 50U;
    config.frame_assembly_timeout_ms = 250U;
    config.tx_complete_timeout_ms = 250U;
    config.shutdown_timeout_ms = 1000U;
    config.rx_task_stack_size = 3072U;
    config.tx_task_stack_size = 3072U;
    config.dispatch_task_stack_size = 3072U;
    config.rx_task_priority = 6U;
    config.tx_task_priority = 6U;
    config.dispatch_task_priority = 5U;
    config.first_tx_sequence = 0U;
    return config;
}

void app_main(void)
{
    gateway_adapter_t *adapter = NULL;
    uart_m_transport_t *uart_transport = NULL;
    gateway_adapter_config_t adapter_config;
    uart_m_transport_config_t uart_config;
    ble_gatt_server_config_t ble_config;
    uart_m_transport_status_t uart_result;
    esp_err_t result;

    ESP_LOGI(TAG, "Vehicle gateway started on ESP-IDF %s", IDF_VER);
    ESP_LOGW(TAG, "Safe mode: vehicle Request/Query TX, command SessionReady and auto Unlock disabled");

    result = nvs_flash_init();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed without destructive erase: %s",
                 esp_err_to_name(result));
        return;
    }

#if CONFIG_P6_NVS_DEV_TEST
    result = p6_nvs_dev_test_run();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "P6 development NVS test failed closed: %s",
                 esp_err_to_name(result));
        return;
    }
#endif

#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
    result = p6_nvs_registration_store_open_namespace(
        &registration_store, P6_DEV_REGISTRATION_NAMESPACE);
#else
    result = p6_nvs_registration_store_open(&registration_store);
#endif
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "P6 Registration NVS open failed without erase: %s",
                 esp_err_to_name(result));
        return;
    }
    registration_port = p6_nvs_registration_store_port(&registration_store);

    adapter_config = gateway_adapter_safe_config();
    adapter_config.registration_store = &registration_port;
    adapter_config.entropy_source = &entropy_source;
    result = gateway_adapter_start(&adapter, &adapter_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Gateway Adapter start failed: %s", esp_err_to_name(result));
        p6_nvs_registration_store_close(&registration_store);
        return;
    }

    ble_config = ble_gatt_server_development_config();
    ble_config.event_callback = gateway_adapter_on_ble_event;
    ble_config.event_context = adapter;
    result = ble_gatt_server_start(&ble_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "BLE GATT Server start failed: %s", esp_err_to_name(result));
        (void)gateway_adapter_stop(&adapter);
        return;
    }

    uart_config = uart2_config();
    uart_result = uart_m_transport_init(&uart_transport, &uart_config,
                                        gateway_adapter_on_uart_message, adapter);
    if (uart_result != UART_M_TRANSPORT_OK) {
        ESP_LOGE(TAG, "UART-M Transport start failed: status=%d", (int)uart_result);
        (void)ble_gatt_server_stop();
        (void)gateway_adapter_stop(&adapter);
        return;
    }

#if CONFIG_P6_READ_ONLY_MONITORING
    /* Verified local Domain InitMonitoring/BindApi/WrapOwner contract.
     * This selects only the read-only demo channel, never P6/control binding.
     * Queue lifetime cannot exceed its required 600ms Source lifetime;
     * original Source AGE makes the Transport discard earlier when needed.
     * Domain's independent 750ms receive lease is not a queue TTL. */
    const gateway_monitor_context_tx_policy_t monitoring_policy = {
        .approved = true,
        .refresh_period_ms = GATEWAY_MONITOR_CONTEXT_PERIOD_MS,
        .queued_context_ttl_ms = GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS,
        .proximity_ttl_ms = 0U, /* No Proximity producer: UNKNOWN/NO_DATA. */
        .sources = {
            .registration_connection_ttl_ms = GATEWAY_MONITOR_REGISTRATION_CONNECTION_TTL_MS,
            .app_ttl_ms = GATEWAY_MONITOR_APP_STATE_TTL_MS,
        },
    };
    result = gateway_adapter_bind_monitor_context_tx(adapter, uart_transport, &monitoring_policy);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Monitoring Context port not bound: %s; TX remains closed",
                 esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "Monitoring policy queued: M_CONTEXT=200ms, connection=600ms, APP_STATE=1000ms; SessionReady=0");
    }
#endif

#if CONFIG_P6_DEV_REGISTRATION_APPROVAL
    result = p6_registration_dev_console_start(adapter);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Development Registration console unavailable: %s; approval remains disabled",
                 esp_err_to_name(result));
    }
#endif

    ESP_LOGI(TAG, "P6-B4-A safe UART RX active: UART2 TX=GPIO27 RX=GPIO26 115200 8N1");
    ESP_LOGW(TAG, "UART RX validated; read-only M_CONTEXT follows explicit monitoring policy; vehicle TX disabled");

#if CONFIG_P6_UART_RX_DIAGNOSTICS
    uart_rx_diag_context.transport = uart_transport;
    uart_rx_diag_context.adapter = adapter;
    if (xTaskCreate(uart_rx_diag_task, "uart_rx_diag",
                    UART_RX_DIAG_TASK_STACK, &uart_rx_diag_context, 3U,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "UART RX diagnostic task unavailable; gateway remains fail-closed");
    } else {
        ESP_LOGW(TAG, "Development UART RX diagnostics enabled; read-only 5s summaries");
    }
#endif
}
