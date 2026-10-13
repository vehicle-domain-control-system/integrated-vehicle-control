idf_component_register(
    SRCS "uart2_loopback_test.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_uart log
)
