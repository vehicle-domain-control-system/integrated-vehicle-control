idf_component_register(
    SRCS
        "p6_session_manager.c"
        "p6_authorization_provider.c"
        "p6_ble_lifecycle.c"
        "p6_registration_record_codec.c"
        "p6_nvs_registration_store.c"
        "p6_bond_reset_coordinator.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)
