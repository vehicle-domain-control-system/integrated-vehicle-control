idf_component_register(
    SRCS
        "gateway_mapper.c"
        "gateway_core.c"
        "gateway_adapter.c"
    INCLUDE_DIRS "include"
    REQUIRES ble uart
)
