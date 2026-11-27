#include "esp_idf_version.h"
#include "esp_log.h"

static const char *TAG = "gateway_main";

void app_main(void)
{
    ESP_LOGI(TAG, "Vehicle gateway base firmware started on ESP-IDF %s", IDF_VER);
    ESP_LOGI(TAG, "BLE, UART, Gateway, and Digital Key functions are not enabled");
}
