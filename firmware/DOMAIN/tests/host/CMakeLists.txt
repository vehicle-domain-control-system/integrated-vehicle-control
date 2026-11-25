cmake_minimum_required(VERSION 3.16)

project(vehicle_state_manager_host_tests C)

enable_testing()

add_executable(test_vehicle_state_manager
    test_vehicle_state_manager.c
    ../../src/application/vehicle_state_manager.c
)

target_include_directories(test_vehicle_state_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_vehicle_state_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_vehicle_state_manager PRIVATE /W4)
else()
    target_compile_options(test_vehicle_state_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME vehicle_state_manager COMMAND test_vehicle_state_manager)

add_executable(test_request_manager
    test_request_manager.c
    ../../src/application/request_manager.c
)

target_include_directories(test_request_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_request_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_request_manager PRIVATE /W4)
else()
    target_compile_options(test_request_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME request_manager COMMAND test_request_manager)

add_executable(test_command_manager
    test_command_manager.c
    ../../src/application/command_manager.c
)

target_include_directories(test_command_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_command_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_command_manager PRIVATE /W4)
else()
    target_compile_options(test_command_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME command_manager COMMAND test_command_manager)

add_executable(test_result_manager
    test_result_manager.c
    ../../src/application/result_manager.c
)

target_include_directories(test_result_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_result_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_result_manager PRIVATE /W4)
else()
    target_compile_options(test_result_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME result_manager COMMAND test_result_manager)

add_executable(test_domain_request_coordinator
    test_domain_request_coordinator.c
    ../../src/application/domain_request_coordinator.c
    ../../src/application/request_manager.c
    ../../src/application/command_manager.c
    ../../src/application/result_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_request_coordinator PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_request_coordinator PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_request_coordinator PRIVATE /W4)
else()
    target_compile_options(test_domain_request_coordinator PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_test(NAME domain_request_coordinator COMMAND test_domain_request_coordinator)

add_executable(test_domain_interface_core
    test_domain_interface_core.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_interface_core PRIVATE
    ../../src/interface
)

target_compile_definitions(test_domain_interface_core PRIVATE
    DOMAIN_IF_CORE_TEST=1
)

set_target_properties(test_domain_interface_core PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_interface_core PRIVATE /W4)
else()
    target_compile_options(test_domain_interface_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_core COMMAND test_domain_interface_core)

add_executable(test_c2_rx_binding
    test_domain_interface_result_binding.c
    ../../src/application/domain_interface_result_binding.c
    ../../src/application/result_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_c2_rx_binding PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_c2_rx_binding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_c2_rx_binding PRIVATE /W4)
else()
    target_compile_options(test_c2_rx_binding PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_result_binding
         COMMAND test_c2_rx_binding)

add_executable(test_c2_state_binding
    test_domain_interface_state_binding.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/vehicle_state_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_c2_state_binding PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_c2_state_binding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_c2_state_binding PRIVATE /W4)
else()
    target_compile_options(test_c2_state_binding PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_state_binding
         COMMAND test_c2_state_binding)

add_executable(test_c2_query_binding
    test_domain_interface_query_binding.c
    ../../src/application/domain_interface_query_binding.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/vehicle_state_manager.c
    ../../src/application/result_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_c2_query_binding PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_c2_query_binding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_c2_query_binding PRIVATE /W4)
else()
    target_compile_options(test_c2_query_binding PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_query_binding
         COMMAND test_c2_query_binding)

add_executable(test_permission_manager
    test_permission_manager.c
    ../../src/application/permission_manager.c
)

target_include_directories(test_permission_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_permission_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_permission_manager PRIVATE /W4)
else()
    target_compile_options(test_permission_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME permission_manager COMMAND test_permission_manager)

add_executable(test_permission_adapter
    test_permission_adapter.c
    ../../src/application/permission_adapter.c
    ../../src/application/permission_manager.c
)

target_include_directories(test_permission_adapter PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_permission_adapter PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_permission_adapter PRIVATE /W4)
else()
    target_compile_options(test_permission_adapter PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME permission_adapter COMMAND test_permission_adapter)

add_executable(test_permission_coordinator
    test_permission_coordinator.c
    ../../src/application/permission_coordinator.c
    ../../src/application/permission_adapter.c
    ../../src/application/permission_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_permission_coordinator PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_permission_coordinator PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_permission_coordinator PRIVATE /W4)
else()
    target_compile_options(test_permission_coordinator PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
endif()

add_test(NAME permission_coordinator COMMAND test_permission_coordinator)

add_executable(test_cis_environment_manager
    test_cis_environment_manager.c
    ../../src/application/cis_environment_manager.c
)

target_include_directories(test_cis_environment_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_cis_environment_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_cis_environment_manager PRIVATE /W4)
else()
    target_compile_options(test_cis_environment_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME cis_environment_manager COMMAND test_cis_environment_manager)

add_executable(test_cis_occupant_manager
    test_cis_occupant_manager.c
    ../../src/application/cis_occupant_manager.c
)

target_include_directories(test_cis_occupant_manager PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_cis_occupant_manager PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_cis_occupant_manager PRIVATE /W4)
else()
    target_compile_options(test_cis_occupant_manager PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME cis_occupant_manager COMMAND test_cis_occupant_manager)

add_executable(test_domain_cis_occupant_private
    test_domain_cis_occupant_private.c
    ../../src/application/domain_cis_occupant_private.c
    ../../src/application/cis_occupant_manager.c
)

target_include_directories(test_domain_cis_occupant_private PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_cis_occupant_private PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_cis_occupant_private PRIVATE /W4)
else()
    target_compile_options(test_domain_cis_occupant_private PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_cis_occupant_private
    COMMAND test_domain_cis_occupant_private)

add_executable(test_domain_interface_cis_occupant_binding
    test_domain_interface_cis_occupant_binding.c
    ../../src/application/domain_interface_cis_occupant_binding.c
    ../../src/application/domain_cis_occupant_private.c
    ../../src/application/cis_occupant_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_interface_cis_occupant_binding PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_interface_cis_occupant_binding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_interface_cis_occupant_binding PRIVATE /W4)
else()
    target_compile_options(test_domain_interface_cis_occupant_binding PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_cis_occupant_binding
    COMMAND test_domain_interface_cis_occupant_binding)

add_executable(test_domain_cis_environment_private
    test_domain_cis_environment_private.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/cis_environment_manager.c
)

target_include_directories(test_domain_cis_environment_private PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_cis_environment_private PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_cis_environment_private PRIVATE /W4)
else()
    target_compile_options(test_domain_cis_environment_private PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_cis_environment_private
    COMMAND test_domain_cis_environment_private)

add_executable(test_domain_interface_cis_environment_binding
    test_domain_interface_cis_environment_binding.c
    ../../src/application/domain_interface_cis_environment_binding.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/cis_environment_manager.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_interface_cis_environment_binding PRIVATE
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_interface_cis_environment_binding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_interface_cis_environment_binding PRIVATE /W4)
else()
    target_compile_options(test_domain_interface_cis_environment_binding PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_interface_cis_environment_binding
    COMMAND test_domain_interface_cis_environment_binding)

add_executable(test_domain_runtime
    test_domain_runtime.c
    ../../src/application/domain_runtime.c
    ../../src/application/vehicle_state_manager.c
    ../../src/application/command_manager.c
    ../../src/application/result_manager.c
    ../../src/application/request_manager.c
    ../../src/application/domain_request_coordinator.c
    ../../src/application/permission_manager.c
    ../../src/application/permission_adapter.c
    ../../src/application/permission_coordinator.c
    ../../src/application/cis_occupant_manager.c
    ../../src/application/domain_cis_occupant_private.c
    ../../src/application/cis_environment_manager.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/domain_interface_result_binding.c
    ../../src/application/domain_interface_query_binding.c
    ../../src/application/domain_interface_cis_occupant_binding.c
    ../../src/application/domain_interface_cis_environment_binding.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_runtime PRIVATE
    ../../src/application
    ../../src/interface
)

target_compile_definitions(test_domain_runtime PRIVATE
    DOMAIN_IF_CORE_TEST=1
)

set_target_properties(test_domain_runtime PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_runtime PRIVATE /W4)
else()
    target_compile_options(test_domain_runtime PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_runtime COMMAND test_domain_runtime)

# Optional cross-owner E2E. The Network team remains the owner of these
# product libraries and headers; no Network source or production stub is
# copied into Domain. Configure both paths to enable this target.
set(DOMAIN_NETWORK_SOURCE_DIR "" CACHE PATH
    "Network firmware source directory containing include/ and nodes/")
set(DOMAIN_NETWORK_LIBRARY_DIR "" CACHE PATH
    "Directory containing the four Network product static libraries")

if(DOMAIN_NETWORK_SOURCE_DIR AND DOMAIN_NETWORK_LIBRARY_DIR)
    set(_domain_network_libraries
        "${DOMAIN_NETWORK_LIBRARY_DIR}/libnetwork_domain_c2.a"
        "${DOMAIN_NETWORK_LIBRARY_DIR}/libnetwork_product.a"
        "${DOMAIN_NETWORK_LIBRARY_DIR}/libnetwork_wire_candidate.a"
        "${DOMAIN_NETWORK_LIBRARY_DIR}/libnetwork_common.a"
    )
    foreach(_library IN LISTS _domain_network_libraries)
        if(NOT EXISTS "${_library}")
            message(FATAL_ERROR "Missing Network E2E library: ${_library}")
        endif()
    endforeach()

    add_executable(test_domain_network_adapter_e2e
        test_domain_network_adapter_e2e.c
        ../../src/application/domain_network_admission_bridge_private.c
        ../../src/application/domain_runtime.c
        ../../src/application/vehicle_state_manager.c
        ../../src/application/command_manager.c
        ../../src/application/result_manager.c
        ../../src/application/request_manager.c
        ../../src/application/domain_request_coordinator.c
        ../../src/application/permission_manager.c
        ../../src/application/permission_adapter.c
        ../../src/application/permission_coordinator.c
        ../../src/application/cis_occupant_manager.c
        ../../src/application/domain_cis_occupant_private.c
        ../../src/application/cis_environment_manager.c
        ../../src/application/domain_cis_environment_private.c
        ../../src/application/domain_interface_state_binding.c
        ../../src/application/domain_interface_result_binding.c
        ../../src/application/domain_interface_query_binding.c
        ../../src/application/domain_interface_cis_occupant_binding.c
        ../../src/application/domain_interface_cis_environment_binding.c
        ../../src/interface/domain_interface_core.c
    )
    target_include_directories(test_domain_network_adapter_e2e PRIVATE
        ../../src/application
        ../../src/interface
        "${DOMAIN_NETWORK_SOURCE_DIR}/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/vss/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/cis/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/bcm/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/domain/include"
    )
    target_compile_definitions(test_domain_network_adapter_e2e PRIVATE
        DOMAIN_IF_CORE_TEST=1
        NETWORK_UPPER_NODE_PROFILE=5
    )
    target_link_libraries(test_domain_network_adapter_e2e PRIVATE
        ${_domain_network_libraries})
    set_target_properties(test_domain_network_adapter_e2e PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    if(MSVC)
        target_compile_options(test_domain_network_adapter_e2e PRIVATE /W4)
    else()
        target_compile_options(test_domain_network_adapter_e2e PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    foreach(_case IN ITEMS lock unlock bcm_result_duplicate timeout unavailable
                           uart_missing_source_meta)
        add_test(NAME domain_network_adapter_e2e.${_case}
            COMMAND test_domain_network_adapter_e2e ${_case})
    endforeach()

    # Host-only private admission seam. Product pinned owner callbacks remain
    # NULL; this target wraps the real DOMAIN product decoder only in tests.
    add_executable(test_domain_network_admission_bridge
        test_domain_network_admission_bridge.c
        domain_network_admission_bridge_host.c
        ../../src/application/domain_request_coordinator.c
        ../../src/application/request_manager.c
        ../../src/application/command_manager.c
        ../../src/application/result_manager.c
        ../../src/interface/domain_interface_core.c
    )
    target_include_directories(test_domain_network_admission_bridge PRIVATE
        ../../src/application
        ../../src/interface
        "${DOMAIN_NETWORK_SOURCE_DIR}/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/vss/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/cis/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/bcm/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/domain/include"
    )
    target_compile_definitions(test_domain_network_admission_bridge PRIVATE
        NETWORK_UPPER_NODE_PROFILE=5
    )
    target_link_libraries(test_domain_network_admission_bridge PRIVATE
        ${_domain_network_libraries})
    set_target_properties(test_domain_network_admission_bridge PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    if(MSVC)
        target_compile_options(test_domain_network_admission_bridge PRIVATE /W4)
    else()
        target_compile_options(test_domain_network_admission_bridge PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    foreach(_case IN ITEMS valid_copy context_mismatch context_change
                           lifecycle_invalidation p6_absent timestamp_expiry
                           capacity_replay_rollover)
        add_test(NAME domain_network_admission_bridge.${_case}
            COMMAND test_domain_network_admission_bridge ${_case})
    endforeach()

    add_executable(test_domain_network_admission_bridge_private
        test_domain_network_admission_bridge_private.c
        ../../src/application/domain_network_admission_bridge_private.c
        ../../src/application/domain_request_coordinator.c
        ../../src/application/request_manager.c
        ../../src/application/command_manager.c
        ../../src/application/result_manager.c
        ../../src/interface/domain_interface_core.c
    )
    target_include_directories(test_domain_network_admission_bridge_private PRIVATE
        ../../src/application
        ../../src/interface
        "${DOMAIN_NETWORK_SOURCE_DIR}/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/vss/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/cis/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/bcm/include"
        "${DOMAIN_NETWORK_SOURCE_DIR}/nodes/domain/include"
    )
    target_compile_definitions(test_domain_network_admission_bridge_private PRIVATE
        NETWORK_UPPER_NODE_PROFILE=5
    )
    target_link_libraries(test_domain_network_admission_bridge_private PRIVATE
        ${_domain_network_libraries})
    set_target_properties(test_domain_network_admission_bridge_private PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    if(MSVC)
        target_compile_options(test_domain_network_admission_bridge_private PRIVATE /W4)
    else()
        target_compile_options(test_domain_network_admission_bridge_private PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    foreach(_case IN ITEMS default_disabled valid_copy context_mismatch
                           context_change lifecycle_invalidation p6_absent
                           timestamp_expiry capacity_replay_rollover
                           passive_query)
        add_test(NAME domain_network_admission_bridge_private.${_case}
            COMMAND test_domain_network_admission_bridge_private ${_case})
    endforeach()
endif()

add_executable(test_domain_board_timebase
    test_domain_board_timebase.c
    ../../src/board/domain_board_timebase.c
    ../../src/application/domain_runtime.c
    ../../src/application/vehicle_state_manager.c
    ../../src/application/command_manager.c
    ../../src/application/result_manager.c
    ../../src/application/request_manager.c
    ../../src/application/domain_request_coordinator.c
    ../../src/application/permission_manager.c
    ../../src/application/permission_adapter.c
    ../../src/application/permission_coordinator.c
    ../../src/application/cis_occupant_manager.c
    ../../src/application/domain_cis_occupant_private.c
    ../../src/application/cis_environment_manager.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/domain_interface_result_binding.c
    ../../src/application/domain_interface_query_binding.c
    ../../src/application/domain_interface_cis_occupant_binding.c
    ../../src/application/domain_interface_cis_environment_binding.c
    ../../src/interface/domain_interface_core.c
)

target_include_directories(test_domain_board_timebase PRIVATE
    ../../src/board
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_board_timebase PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_board_timebase PRIVATE /W4)
else()
    target_compile_options(test_domain_board_timebase PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_board_timebase COMMAND test_domain_board_timebase)

add_executable(test_domain_stm0_tick_reader
    test_domain_stm0_tick_reader.c
    ../../src/board/domain_stm0_tick_reader.c
    ../../src/board/domain_board_timebase.c
)

target_include_directories(test_domain_stm0_tick_reader PRIVATE
    stubs
    ../../src/board
)

set_target_properties(test_domain_stm0_tick_reader PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_stm0_tick_reader PRIVATE /W4)
else()
    target_compile_options(test_domain_stm0_tick_reader PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_stm0_tick_reader COMMAND test_domain_stm0_tick_reader)

add_executable(test_domain_board_runtime
    test_domain_board_runtime.c
    ../../src/board/domain_board_runtime.c
    ../../src/board/domain_board_timebase.c
    ../../src/board/domain_stm0_tick_reader.c
)

target_include_directories(test_domain_board_runtime PRIVATE
    stubs
    ../../src/board
    ../../src/application
    ../../src/interface
)

set_target_properties(test_domain_board_runtime PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_board_runtime PRIVATE /W4)
else()
    target_compile_options(test_domain_board_runtime PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_board_runtime COMMAND test_domain_board_runtime)

add_executable(test_domain_application_entry
    test_domain_application_entry.c
    ../../src/application/domain_application_entry.c
)

target_include_directories(test_domain_application_entry PRIVATE
    stubs
    ../../src/application
    ../../src/board
    ../../src/interface
)

set_target_properties(test_domain_application_entry PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_options(test_domain_application_entry PRIVATE /W4)
else()
    target_compile_options(test_domain_application_entry PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME domain_application_entry COMMAND test_domain_application_entry)
add_test(
    NAME domain_application_entry_init_failure
    COMMAND test_domain_application_entry init-failure
)

# Dependencies added by the validated Runtime/Result integration. Keep the
# narrow module targets; no Network or RTD backend enters these native tests.
foreach(_target test_c2_rx_binding test_domain_runtime test_domain_application_entry test_domain_network_adapter_e2e)
    if(TARGET ${_target})
        target_sources(${_target} PRIVATE ../../src/application/vss_event_manager.c)
        if(NOT _target STREQUAL "test_c2_rx_binding")
            target_sources(${_target} PRIVATE ../../src/application/vss_warning_manager.c)
        endif()
    endif()
endforeach()

foreach(_module vss_warning_manager vss_event_manager domain_network_bringup domain_network_bootstrap domain_uart_diagnostic_bootstrap)
    if(_module MATCHES "^vss_")
        set(_source ../../src/application/${_module}.c)
    else()
        set(_source ../../src/board/${_module}.c)
    endif()
    add_executable(test_${_module} test_${_module}.c ${_source})
    target_include_directories(test_${_module} PRIVATE ../../src/application ../../src/interface ../../src/board fakes)
    set_target_properties(test_${_module} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(test_${_module} PRIVATE /W4)
    else()
        target_compile_options(test_${_module} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${_module} COMMAND test_${_module})
endforeach()
target_sources(test_vss_warning_manager PRIVATE ../../src/application/vehicle_state_manager.c)
target_sources(test_domain_network_bootstrap PRIVATE ../../src/board/domain_network_bringup.c)
