#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOOPBACK_UART UART_NUM_2
#define LOOPBACK_TX_GPIO GPIO_NUM_27
#define LOOPBACK_RX_GPIO GPIO_NUM_26
#define LOOPBACK_BAUD_RATE 115200
#define LOOPBACK_PACKET_SIZE 16U
#define LOOPBACK_RX_BUFFER_SIZE 256
#define LOOPBACK_READ_TIMEOUT_MS 250U
#define LOOPBACK_TX_TIMEOUT_MS 100U
#define LOOPBACK_PERIOD_MS 1000U

static const char *TAG = "UART2_LOOPBACK";

static void make_test_packet(uint8_t packet[LOOPBACK_PACKET_SIZE], uint32_t sequence)
{
    packet[0] = 0xA5U;
    packet[1] = 0x5AU;
    packet[2] = 'L';
    packet[3] = 'B';
    packet[4] = (uint8_t)(sequence & UINT32_C(0xFF));
    packet[5] = (uint8_t)((sequence >> 8U) & UINT32_C(0xFF));
    packet[6] = (uint8_t)((sequence >> 16U) & UINT32_C(0xFF));
    packet[7] = (uint8_t)((sequence >> 24U) & UINT32_C(0xFF));

    for (size_t index = 8U; index < LOOPBACK_PACKET_SIZE; ++index) {
        packet[index] = (uint8_t)(sequence + (uint32_t)(index * 17U));
    }
}

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = LOOPBACK_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0U,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uint32_t sequence = 0U;
    uint32_t success_count = 0U;
    uint32_t failure_count = 0U;
    TickType_t last_wake_time;

    ESP_ERROR_CHECK(uart_driver_install(LOOPBACK_UART, LOOPBACK_RX_BUFFER_SIZE, 0,
                                        0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LOOPBACK_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LOOPBACK_UART, LOOPBACK_TX_GPIO, LOOPBACK_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART2 loopback started: TX=GPIO27 RX=GPIO26 115200 8N1");
    ESP_LOGW(TAG, "GPIO27 is push-pull UART TX; board-level conflict is not ruled out");
    last_wake_time = xTaskGetTickCount();

    while (true) {
        uint8_t transmitted[LOOPBACK_PACKET_SIZE];
        uint8_t received[LOOPBACK_PACKET_SIZE];
        const esp_err_t flush_status = uart_flush_input(LOOPBACK_UART);
        int written;
        int received_length;

        make_test_packet(transmitted, sequence);
        memset(received, 0, sizeof(received));

        if (flush_status != ESP_OK) {
            failure_count++;
            ESP_LOGE(TAG, "FAIL seq=%" PRIu32 " RX flush error=%s success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, esp_err_to_name(flush_status), success_count, failure_count);
            goto next_cycle;
        }

        written = uart_write_bytes(LOOPBACK_UART, transmitted, LOOPBACK_PACKET_SIZE);
        if (written != (int)LOOPBACK_PACKET_SIZE) {
            failure_count++;
            ESP_LOGE(TAG, "FAIL seq=%" PRIu32 " TX length=%d expected=%u success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, written, (unsigned)LOOPBACK_PACKET_SIZE,
                     success_count, failure_count);
            goto next_cycle;
        }

        if (uart_wait_tx_done(LOOPBACK_UART,
                              pdMS_TO_TICKS(LOOPBACK_TX_TIMEOUT_MS)) != ESP_OK) {
            failure_count++;
            ESP_LOGE(TAG, "FAIL seq=%" PRIu32 " TX completion timeout success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, success_count, failure_count);
            goto next_cycle;
        }

        received_length = uart_read_bytes(LOOPBACK_UART, received, LOOPBACK_PACKET_SIZE,
                                          pdMS_TO_TICKS(LOOPBACK_READ_TIMEOUT_MS));
        if (received_length == 0) {
            failure_count++;
            ESP_LOGW(TAG, "TIMEOUT seq=%" PRIu32 " received=0 expected=%u success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, (unsigned)LOOPBACK_PACKET_SIZE,
                     success_count, failure_count);
        } else if (received_length != (int)LOOPBACK_PACKET_SIZE) {
            failure_count++;
            ESP_LOGE(TAG, "FAIL seq=%" PRIu32 " RX length=%d expected=%u success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, received_length, (unsigned)LOOPBACK_PACKET_SIZE,
                     success_count, failure_count);
        } else if (memcmp(transmitted, received, LOOPBACK_PACKET_SIZE) != 0) {
            size_t mismatch = 0U;

            while ((mismatch < LOOPBACK_PACKET_SIZE) &&
                   (transmitted[mismatch] == received[mismatch])) {
                mismatch++;
            }
            failure_count++;
            ESP_LOGE(TAG, "FAIL seq=%" PRIu32
                          " byte[%u] tx=0x%02X rx=0x%02X success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, (unsigned)mismatch, transmitted[mismatch], received[mismatch],
                     success_count, failure_count);
        } else {
            success_count++;
            ESP_LOGI(TAG, "SUCCESS seq=%" PRIu32 " length=%u success=%" PRIu32
                          " failure=%" PRIu32,
                     sequence, (unsigned)LOOPBACK_PACKET_SIZE,
                     success_count, failure_count);
        }

next_cycle:
        sequence++;
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(LOOPBACK_PERIOD_MS));
    }
}
