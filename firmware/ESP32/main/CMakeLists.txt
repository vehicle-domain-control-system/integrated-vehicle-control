idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES ble esp_system gateway log nvs_flash uart
)
