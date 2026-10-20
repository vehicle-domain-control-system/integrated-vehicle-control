#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define GPIO_INPUT_TEST_PIN_26 GPIO_NUM_26
#define GPIO_INPUT_TEST_PIN_27 GPIO_NUM_27
#define GPIO_INPUT_SAMPLE_PERIOD_MS 1000U

static const char *TAG = "GPIO_TEST";

void app_main(void)
{
    const gpio_config_t input_config = {
        .pin_bit_mask = (UINT64_C(1) << GPIO_INPUT_TEST_PIN_26) |
                        (UINT64_C(1) << GPIO_INPUT_TEST_PIN_27),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&input_config));
    ESP_LOGI(TAG, "GPIO26/27 input test started; internal pulls disabled");

    while (true) {
        const int gpio26_level = gpio_get_level(GPIO_INPUT_TEST_PIN_26);
        const int gpio27_level = gpio_get_level(GPIO_INPUT_TEST_PIN_27);

        ESP_LOGI(TAG, "GPIO26=%d GPIO27=%d", gpio26_level, gpio27_level);
        vTaskDelay(pdMS_TO_TICKS(GPIO_INPUT_SAMPLE_PERIOD_MS));
    }
}
