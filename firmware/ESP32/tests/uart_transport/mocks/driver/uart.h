#ifndef MOCK_DRIVER_UART_H
#define MOCK_DRIVER_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/queue.h"

typedef int esp_err_t;
typedef int uart_port_t;
typedef int uart_word_length_t;
typedef int uart_parity_t;
typedef int uart_stop_bits_t;
typedef int uart_hw_flowcontrol_t;
typedef int uart_sclk_t;

#define ESP_OK 0
#define ESP_FAIL (-1)
#define UART_NUM_MAX 3
#define UART_BITRATE_MAX 5000000
#define UART_HW_FIFO_LEN(port) ((void)(port), 128)
#define UART_DATA_5_BITS 5
#define UART_DATA_6_BITS 6
#define UART_DATA_7_BITS 7
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_PARITY_EVEN 1
#define UART_PARITY_ODD 2
#define UART_STOP_BITS_1 1
#define UART_STOP_BITS_2 2
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_HW_FLOWCTRL_RTS 1
#define UART_HW_FLOWCTRL_CTS 2
#define UART_HW_FLOWCTRL_CTS_RTS 3
#define UART_SCLK_DEFAULT 0

typedef struct {
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uint8_t rx_flow_ctrl_thresh;
    uart_sclk_t source_clk;
} uart_config_t;

typedef enum {
    UART_DATA,
    UART_BREAK,
    UART_BUFFER_FULL,
    UART_FIFO_OVF,
    UART_FRAME_ERR,
    UART_PARITY_ERR,
    UART_DATA_BREAK,
    UART_PATTERN_DET,
    UART_EVENT_MAX
} uart_event_type_t;

typedef struct {
    uart_event_type_t type;
    size_t size;
    bool timeout_flag;
} uart_event_t;

esp_err_t uart_param_config(uart_port_t port, const uart_config_t *config);
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(uart_port_t port, int rx_size, int tx_size, int queue_size,
                              QueueHandle_t *queue, int flags);
esp_err_t uart_driver_delete(uart_port_t port);
int uart_read_bytes(uart_port_t port, void *buffer, uint32_t length, TickType_t wait);
int uart_write_bytes(uart_port_t port, const void *buffer, size_t length);
esp_err_t uart_wait_tx_done(uart_port_t port, TickType_t wait);
esp_err_t uart_flush_input(uart_port_t port);

#endif
