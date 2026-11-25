set(main_sources "app_main.c")

if(CONFIG_P6_NVS_DEV_TEST)
    list(APPEND main_sources "p6_nvs_dev_test.c")
endif()

if(CONFIG_P6_DEV_REGISTRATION_APPROVAL)
    list(APPEND main_sources
        "p6_registration_dev_console.c"
        "p6_registration_console_parser.c"
    )
endif()

idf_component_register(
    SRCS ${main_sources}
    INCLUDE_DIRS "."
    REQUIRES ble esp_system gateway log nvs_flash session uart
)
