cmake_minimum_required(VERSION 3.20)
project(monitoring_runtime_interop C)
set(DOMAIN_PROJECT "" CACHE PATH "Read-only S32DS Production project")
set(DOMAIN_FIXTURE_DIR "" CACHE PATH "Existing Domain admission Host fixture")
if(NOT EXISTS "${DOMAIN_PROJECT}/src/application/domain_network_admission_bridge_private.c" OR
   NOT EXISTS "${DOMAIN_FIXTURE_DIR}/test_domain_network_admission_bridge.c")
    message(FATAL_ERROR "Exact Production project and existing Domain fixture are required")
endif()
# Reuse the ESP32 fixture source list, without changing it.
add_subdirectory(../gateway_adapter gateway_fixture EXCLUDE_FROM_ALL)
get_target_property(esp_sources gateway_adapter_tests SOURCES)
list(REMOVE_AT esp_sources 0)
set(sources test_monitoring_runtime.c domain_peer.c
    ../../components/uart/uart_m_transport_idf.c
    "${DOMAIN_PROJECT}/src/interface/domain_interface_core.c"
    "${DOMAIN_PROJECT}/network/board/s32k344_domain/net_domain_board.c"
    "${DOMAIN_PROJECT}/network/src/platform/net_boot_id.c")
foreach(source IN LISTS esp_sources)
    get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gateway_adapter")
    list(APPEND sources "${source}")
endforeach()
# Same portable source selection as the existing Domain test_host.ps1; no RTD.
file(GLOB domain_sources "${DOMAIN_PROJECT}/src/application/*.c")
list(FILTER domain_sources EXCLUDE REGEX "/domain_application_entry\\.c$")
file(GLOB_RECURSE network_sources "${DOMAIN_PROJECT}/network/src/l0*/*.c")
file(GLOB_RECURSE network_headers "${DOMAIN_PROJECT}/network/src/l0*/*.h")
set(network_includes)
foreach(header IN LISTS network_headers)
    get_filename_component(directory "${header}" DIRECTORY)
    list(APPEND network_includes "${directory}")
endforeach()
list(REMOVE_DUPLICATES network_includes)
add_executable(monitoring_runtime_interop ${sources} ${domain_sources} ${network_sources})
target_compile_definitions(monitoring_runtime_interop PRIVATE GATEWAY_ADAPTER_HOST_TEST=1
    CONFIG_P6_DEV_REGISTRATION_APPROVAL=0 NETWORK_UPPER_NODE_PROFILE=5
    NET_BOARD_DOMAIN_C2_HOOK=NetDomainBoard_C2Config)
target_include_directories(monitoring_runtime_interop PRIVATE
    ../gateway_adapter/mocks ../uart_transport ../uart_transport/mocks
    ../../components/gateway/include ../../components/session/include
    ../../components/uart/include ../../components/ble/include
    "${DOMAIN_PROJECT}/src/application" "${DOMAIN_PROJECT}/src/interface"
    "${DOMAIN_PROJECT}/src/board" "${DOMAIN_PROJECT}/network/include"
    "${DOMAIN_PROJECT}/network/board/s32k344_domain" "${DOMAIN_FIXTURE_DIR}"
    ${network_includes})
target_compile_features(monitoring_runtime_interop PRIVATE c_std_11)
target_compile_options(monitoring_runtime_interop PRIVATE -Wall -Wextra -Wpedantic -Werror)
enable_testing()
add_test(NAME monitoring_runtime_interop COMMAND monitoring_runtime_interop)
