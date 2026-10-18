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
    ../../src/application/permission_manager.c
    ../../src/application/cis_environment_manager.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/domain_interface_result_binding.c
    ../../src/application/domain_interface_query_binding.c
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

add_executable(test_domain_board_timebase
    test_domain_board_timebase.c
    ../../src/board/domain_board_timebase.c
    ../../src/application/domain_runtime.c
    ../../src/application/vehicle_state_manager.c
    ../../src/application/command_manager.c
    ../../src/application/result_manager.c
    ../../src/application/permission_manager.c
    ../../src/application/cis_environment_manager.c
    ../../src/application/domain_cis_environment_private.c
    ../../src/application/domain_interface_state_binding.c
    ../../src/application/domain_interface_result_binding.c
    ../../src/application/domain_interface_query_binding.c
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
