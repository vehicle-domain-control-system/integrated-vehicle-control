idf_component_register(
    SRCS
        "ble_gatt_protocol.c"
        "ble_gatt_server.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_timer
)
