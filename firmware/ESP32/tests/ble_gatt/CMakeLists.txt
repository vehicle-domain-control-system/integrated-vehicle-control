cmake_minimum_required(VERSION 3.16)

project(ble_gatt_protocol_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ble_gatt_protocol_tests
    test_ble_gatt_protocol.c
    ../../components/ble/ble_gatt_protocol.c
)

target_include_directories(ble_gatt_protocol_tests PRIVATE
    ../../components/ble/include
)

if(MSVC)
    target_compile_options(ble_gatt_protocol_tests PRIVATE /W4 /WX)
else()
    target_compile_options(ble_gatt_protocol_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME ble_gatt_protocol_tests COMMAND ble_gatt_protocol_tests)
