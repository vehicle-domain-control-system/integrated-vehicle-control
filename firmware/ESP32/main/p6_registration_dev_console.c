#include "p6_registration_dev_console.h"
#include "p6_registration_console_parser.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define P6_REGISTRATION_CONSOLE_STACK_SIZE 3072U
#define P6_REGISTRATION_CONSOLE_PRIORITY 3U
#define P6_REGISTRATION_CONSOLE_RETRY_MS 20U

static const char *TAG = "p6_reg_console";
static TaskHandle_t console_task_handle;

static void console_task(void *argument)
{
    gateway_adapter_t *adapter = argument;
    p6_registration_console_parser_t parser;

    p6_registration_console_parser_init(&parser);
    ESP_LOGW(TAG, "DEVELOPMENT-ONLY UART0 approval console enabled");
    ESP_LOGW(TAG, "Commands: p6-reg-request, then p6-reg-approve");
    while (true) {
        p6_registration_console_parse_result_t parse_result;
        const char *line;
        esp_err_t result;
        int byte = fgetc(stdin);

        if (byte == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(P6_REGISTRATION_CONSOLE_RETRY_MS));
            continue;
        }
        parse_result = p6_registration_console_parser_feed(
            &parser, (uint8_t)byte, &line);
        if (parse_result == P6_REGISTRATION_CONSOLE_INPUT_PENDING) {
            continue;
        }
        if (parse_result == P6_REGISTRATION_CONSOLE_LINE_DISCARDED) {
            ESP_LOGW(TAG, "Overlong development command rejected; no state change");
            continue;
        }
        if (strcmp(line, "p6-reg-request") == 0) {
            result = gateway_adapter_development_request_registration(adapter);
            ESP_LOGW(TAG, "Registration request enqueue: %s",
                     esp_err_to_name(result));
        } else if (strcmp(line, "p6-reg-approve") == 0) {
            result = gateway_adapter_development_approve_registration(adapter);
            ESP_LOGW(TAG, "Registration approval enqueue: %s",
                     esp_err_to_name(result));
        } else if (line[0] != '\0') {
            ESP_LOGW(TAG, "Unknown development command; no state change");
        }
    }
}

esp_err_t p6_registration_dev_console_start(gateway_adapter_t *adapter)
{
    if (adapter == NULL || console_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskCreate(console_task, "p6_reg_console",
                    P6_REGISTRATION_CONSOLE_STACK_SIZE, adapter,
                    P6_REGISTRATION_CONSOLE_PRIORITY,
                    &console_task_handle) != pdPASS) {
        console_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
