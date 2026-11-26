cmake_minimum_required(VERSION 3.20)
project(gateway_adapter_tests C)

function(add_gateway_adapter_test target_name dev_registration_enabled)
    add_executable(${target_name}
        test_gateway_adapter.c
        ../uart_transport/mock_idf.c
        ../../components/gateway/gateway_adapter.c
        ../../components/gateway/gateway_core.c
        ../../components/gateway/gateway_mapper.c
        ../../components/gateway/gateway_context_dto.c
        ../../components/gateway/gateway_context_source_metadata.c
        ../../components/gateway/gateway_request_admission.c
        ../../components/uart/uart_m_crc16.c
        ../../components/uart/uart_m_frame.c
        ../../components/uart/uart_m_parser.c
        ../../components/uart/uart_m_codec.c
        ../../components/uart/uart_m_transport_engine.c
        ../../components/ble/ble_gatt_protocol.c
        ../../components/ble/ble_connection_state.c
        ../../components/session/p6_session_manager.c
        ../../components/session/p6_ble_lifecycle.c
    )
    target_compile_definitions(${target_name} PRIVATE
        GATEWAY_ADAPTER_HOST_TEST=1
        CONFIG_P6_DEV_REGISTRATION_APPROVAL=${dev_registration_enabled}
    )
    target_include_directories(${target_name} PRIVATE
        mocks ../uart_transport ../uart_transport/mocks
        ../../components/gateway/include ../../components/uart/include ../../components/ble/include
        ../../components/session/include
    )
    target_compile_features(${target_name} PRIVATE c_std_11)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

enable_testing()
add_gateway_adapter_test(gateway_adapter_tests 0)
add_gateway_adapter_test(gateway_registration_approval_tests 1)
add_test(NAME gateway_context_queue_tests COMMAND gateway_adapter_tests --context-queue)
add_test(NAME gateway_context_queue_dev_tests COMMAND gateway_registration_approval_tests --context-queue)

add_executable(p6_registration_console_parser_tests
    test_p6_registration_console_parser.c
    ../../main/p6_registration_console_parser.c
)
target_include_directories(p6_registration_console_parser_tests PRIVATE
    ../../main
)
target_compile_features(p6_registration_console_parser_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(p6_registration_console_parser_tests PRIVATE /W4 /WX)
else()
    target_compile_options(p6_registration_console_parser_tests PRIVATE
        -Wall -Wextra -Werror -pedantic
    )
endif()
add_test(NAME p6_registration_console_parser_tests
         COMMAND p6_registration_console_parser_tests)

add_executable(gateway_context_source_metadata_tests
    test_gateway_context_source_metadata.c
    ../../components/gateway/gateway_context_source_metadata.c
)
target_include_directories(gateway_context_source_metadata_tests PRIVATE
    ../../components/gateway/include ../../components/uart/include
    ../../components/session/include
)
target_compile_features(gateway_context_source_metadata_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(gateway_context_source_metadata_tests PRIVATE /W4 /WX)
else()
    target_compile_options(gateway_context_source_metadata_tests PRIVATE
        -Wall -Wextra -Werror -pedantic
    )
endif()
add_test(NAME gateway_context_source_metadata_tests
         COMMAND gateway_context_source_metadata_tests)
