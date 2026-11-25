idf_component_register(
    SRCS
        "gateway_mapper.c"
        "gateway_context_dto.c"
        "gateway_context_source_metadata.c"
        "gateway_request_admission.c"
        "gateway_core.c"
        "gateway_adapter.c"
    INCLUDE_DIRS "include"
    REQUIRES ble session uart
)
