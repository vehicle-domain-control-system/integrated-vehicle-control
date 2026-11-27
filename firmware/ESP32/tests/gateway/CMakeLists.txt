cmake_minimum_required(VERSION 3.16)

project(gateway_core_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(gateway_core_tests
    test_gateway.c
    ../../components/gateway/gateway_mapper.c
    ../../components/gateway/gateway_core.c
    ../../components/uart/uart_m_codec.c
    ../../components/ble/ble_gatt_protocol.c
)

target_include_directories(gateway_core_tests PRIVATE
    ../../components/gateway/include
    ../../components/uart/include
    ../../components/ble/include
)

if(MSVC)
    target_compile_options(gateway_core_tests PRIVATE /W4 /WX)
else()
    target_compile_options(gateway_core_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME gateway_core_tests COMMAND gateway_core_tests)
