#include "driver/uart.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "ble_gatt_server.h"
#include "gateway_adapter.h"
#include "uart_m_transport.h"

static const char *TAG = "gateway_main";

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
    ESP_LOGW(TAG, "Safe mode: production UART TX, P6 Session, Query completion and auto Unlock disabled");

    result = nvs_flash_init();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed without destructive erase: %s",
                 esp_err_to_name(result));
        return;
    }

    adapter_config = gateway_adapter_safe_config();
    result = gateway_adapter_start(&adapter, &adapter_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Gateway Adapter start failed: %s", esp_err_to_name(result));
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

    ESP_LOGI(TAG, "P5-B safe adapter active: UART2 TX=GPIO27 RX=GPIO26 115200 8N1");
    ESP_LOGW(TAG, "UART RX is accepted, but no runtime UART command TX API is connected");
}
