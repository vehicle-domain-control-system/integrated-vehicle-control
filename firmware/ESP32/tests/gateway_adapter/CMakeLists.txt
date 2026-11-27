cmake_minimum_required(VERSION 3.20)
project(gateway_adapter_tests C)
add_executable(gateway_adapter_tests
    test_gateway_adapter.c
    ../uart_transport/mock_idf.c
    ../../components/gateway/gateway_adapter.c
    ../../components/gateway/gateway_core.c
    ../../components/gateway/gateway_mapper.c
    ../../components/uart/uart_m_codec.c
    ../../components/ble/ble_gatt_protocol.c
)
target_compile_definitions(gateway_adapter_tests PRIVATE GATEWAY_ADAPTER_HOST_TEST=1)
target_include_directories(gateway_adapter_tests PRIVATE
    mocks ../uart_transport ../uart_transport/mocks
    ../../components/gateway/include ../../components/uart/include ../../components/ble/include
)
target_compile_features(gateway_adapter_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(gateway_adapter_tests PRIVATE /W4 /WX)
else()
    target_compile_options(gateway_adapter_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
enable_testing()
add_test(NAME gateway_adapter_tests COMMAND gateway_adapter_tests)
