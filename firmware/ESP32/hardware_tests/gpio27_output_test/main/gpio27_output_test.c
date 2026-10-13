#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO27_TEST_PIN GPIO_NUM_27
#define GPIO27_TOGGLE_PERIOD_MS 3000U

static const char *TAG = "gpio27_test";

void app_main(void)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = UINT64_C(1) << GPIO27_TEST_PIN,
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    bool drive_low = false;

    /* In open-drain mode, level 1 releases the pin instead of driving it HIGH. */
    ESP_ERROR_CHECK(gpio_set_level(GPIO27_TEST_PIN, 1));
    ESP_ERROR_CHECK(gpio_config(&output_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO27_TEST_PIN, 1));

    ESP_LOGI(TAG, "GPIO27-only open-drain test; UART-M and Gateway are not started");
    ESP_LOGI(TAG, "GPIO27 open-drain=RELEASED (set level=1)");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(GPIO27_TOGGLE_PERIOD_MS));
        drive_low = !drive_low;
        ESP_ERROR_CHECK(gpio_set_level(GPIO27_TEST_PIN, drive_low ? 0U : 1U));
        ESP_LOGI(TAG, "GPIO27 open-drain=%s", drive_low ? "LOW" : "RELEASED (set level=1)");
    }
}
