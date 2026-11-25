cmake_minimum_required(VERSION 3.20)
project(m_context_interop_tests C)

set(NETWORK_SOURCE_DIR "" CACHE PATH
    "Read-only firmware/network tree at the reviewed Network commit")
set(DOMAIN_SOURCE_DIR "" CACHE PATH
    "Read-only Domain src directory containing application/ and interface/")

if(NOT EXISTS "${NETWORK_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "NETWORK_SOURCE_DIR must name an exact Network source tree")
endif()
if(NOT EXISTS "${DOMAIN_SOURCE_DIR}/application/digital_key_context_manager.c")
    message(FATAL_ERROR "DOMAIN_SOURCE_DIR must name the reviewed Domain src tree")
endif()

# Build exact external implementations by path; neither peer tree is modified.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(NETWORK_TARGET host CACHE STRING "" FORCE)
set(NETWORK_NODE_PROFILE HOST CACHE STRING "" FORCE)
set(NETWORK_BUILD_WIRE_CANDIDATE ON CACHE BOOL "" FORCE)
set(NETWORK_BUILD_UPPER_STACK ON CACHE BOOL "" FORCE)
set(NETWORK_BUILD_DOMAIN_C2 ON CACHE BOOL "" FORCE)
set(NETWORK_DOMAIN_C2_INCLUDE_DIR "${DOMAIN_SOURCE_DIR}/interface"
    CACHE PATH "" FORCE)
add_subdirectory("${NETWORK_SOURCE_DIR}" network)

add_executable(m_context_interop_tests
    test_m_context_interop.c
    ../../components/gateway/gateway_mapper.c
    ../../components/gateway/gateway_context_dto.c
    ../../components/gateway/gateway_context_source_metadata.c
    ../../components/uart/uart_m_crc16.c
    ../../components/uart/uart_m_frame.c
    ../../components/uart/uart_m_codec.c
    ../../components/ble/ble_gatt_protocol.c
    "${DOMAIN_SOURCE_DIR}/interface/domain_interface_core.c"
    "${DOMAIN_SOURCE_DIR}/application/digital_key_context_manager.c"
    "${DOMAIN_SOURCE_DIR}/application/domain_interface_digital_key_binding.c"
)
target_include_directories(m_context_interop_tests PRIVATE
    ../../components/gateway/include
    ../../components/uart/include
    ../../components/ble/include
    ../../components/session/include
    "${DOMAIN_SOURCE_DIR}/application"
    "${DOMAIN_SOURCE_DIR}/interface"
)
target_link_libraries(m_context_interop_tests PRIVATE network_domain_c2)
target_compile_features(m_context_interop_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(m_context_interop_tests PRIVATE /W4 /WX)
else()
    target_compile_options(m_context_interop_tests PRIVATE
        -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME m_context_interop_tests COMMAND m_context_interop_tests)
