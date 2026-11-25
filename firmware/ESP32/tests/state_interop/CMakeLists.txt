cmake_minimum_required(VERSION 3.20)
project(vehicle_state_interop_tests C)

set(NETWORK_SOURCE_DIR "" CACHE PATH
    "Read-only firmware/network tree at the reviewed Network commit")

if(NOT EXISTS "${NETWORK_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "NETWORK_SOURCE_DIR must name an exact Network source tree")
endif()

# Build the exact external encoder by path; the peer tree remains read-only.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(NETWORK_TARGET host CACHE STRING "" FORCE)
set(NETWORK_NODE_PROFILE HOST CACHE STRING "" FORCE)
set(NETWORK_BUILD_WIRE_CANDIDATE ON CACHE BOOL "" FORCE)
add_subdirectory("${NETWORK_SOURCE_DIR}" network)

add_executable(vehicle_state_interop_tests
    test_bcm_door_state_interop.c
    ../../components/gateway/gateway_mapper.c
    ../../components/uart/uart_m_crc16.c
    ../../components/uart/uart_m_frame.c
    ../../components/uart/uart_m_codec.c
    ../../components/ble/ble_gatt_protocol.c
)
target_include_directories(vehicle_state_interop_tests PRIVATE
    ../../components/gateway/include
    ../../components/uart/include
    ../../components/ble/include
)
target_link_libraries(vehicle_state_interop_tests PRIVATE network_wire_candidate)
target_compile_features(vehicle_state_interop_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(vehicle_state_interop_tests PRIVATE /W4 /WX)
else()
    target_compile_options(vehicle_state_interop_tests PRIVATE
        -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME vehicle_state_interop_tests COMMAND vehicle_state_interop_tests)
