cmake_minimum_required(VERSION 3.16)
project(network_scaffold LANGUAGES C)

set(NETWORK_TARGET "host" CACHE STRING "Portable platform selection; SDK binding is external")
set_property(CACHE NETWORK_TARGET PROPERTY STRINGS host s32k144 s32k344)
if(NOT NETWORK_TARGET MATCHES "^(host|s32k144|s32k344)$")
    message(FATAL_ERROR "NETWORK_TARGET=${NETWORK_TARGET}: NOT_CONFIGURED (unknown platform)")
endif()
option(NETWORK_BUILD_BOARD_HAL "Link a real board SDK backend (not supplied by portable Network)" OFF)
if(NETWORK_BUILD_BOARD_HAL)
    message(FATAL_ERROR "HAL_EXTERNAL: NOT_CONFIGURED. Supply and link your board backend in the owning firmware project; portable Network needs no SDK.")
endif()
if(NOT NETWORK_TARGET STREQUAL "host")
    option(BUILD_TESTING "Native Host tests" OFF)
    if(BUILD_TESTING)
        message(FATAL_ERROR "NOT_CONFIGURED: MCU portable library excludes Host test executables; select BUILD_TESTING=OFF")
    endif()
    # Portable core and the injected backend bridge are the same product files.
    set(NETWORK_BUILD_WIRE_CANDIDATE ON CACHE BOOL "Build wire codecs" FORCE)
    set(NETWORK_BUILD_UPPER_STACK ON CACHE BOOL "Build product core" FORCE)
    set(NETWORK_BUILD_HAL_PORT_ADAPTER ON CACHE BOOL "Build SDK-free backend bridge" FORCE)
endif()

# Explicit isolated candidate opt-in; no network_common/runtime/HAL dependency.
option(NETWORK_BUILD_WIRE_CANDIDATE "Build unadopted Wave1/Wave2A/Wave2B wire candidate" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NETWORK_BUILD_WIRE_CANDIDATE)
    add_library(network_wire_candidate STATIC
        src/l04_frame_codec/net_wire_common.c
        src/l04_frame_codec/net_wire_registry.c
        src/l04_frame_codec/m05_can_codec/net_wire_can.c
        src/l04_frame_codec/m05_can_codec/net_wire_can_common.c
        src/l04_frame_codec/m05_can_codec/net_wire_can_bcm_raw.c
        src/l04_frame_codec/m05_can_codec/net_wire_can_nodes_raw.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart_dto.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart_typed_internal.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart_m_core.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart_m_projection.c
        src/l04_frame_codec/m06_uart_codec/net_wire_uart_c_status.c
        src/l04_frame_codec/m06_uart_codec/net_wire_stream.c)
    target_include_directories(network_wire_candidate PUBLIC include PRIVATE src/l04_frame_codec)
    # Warning function is defined below, so options are applied after declarations.
endif()

add_library(network_common STATIC
    src/l01_node_context/m01_source_context/net_source.c
    src/l02_typed_handoff/m03_typed_handoff/net_handoff.c
    src/l03_message_service/m04_message_service/net_runtime.c
    src/l04_frame_codec/m05_can_codec/net_can_codec.c
    src/l04_frame_codec/m06_uart_codec/net_uart_stream.c
    src/l05_transport_port/m07_buffer_outcome/net_buffer.c
    src/l05_transport_port/m08_device_port/net_port.c
    src/l05_transport_port/m09_observation/net_observation.c)
target_include_directories(network_common PUBLIC include PRIVATE
    src/l02_typed_handoff/m03_typed_handoff
    src/l05_transport_port/m07_buffer_outcome
    src/l05_transport_port/m09_observation)
if(NETWORK_TARGET STREQUAL "host")
add_library(network_host_mock STATIC src/platform/host_mock/net_host_mock.c)
target_include_directories(network_host_mock PUBLIC include)
target_link_libraries(network_host_mock PUBLIC network_common)
endif()

function(network_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
network_warnings(network_common)
if(TARGET network_host_mock)
    network_warnings(network_host_mock)
endif()
if(NETWORK_BUILD_WIRE_CANDIDATE)
    network_warnings(network_wire_candidate)
endif()

if(NETWORK_TARGET STREQUAL "host")
add_executable(network_host_demo tests/host/host_demo.c)
target_link_libraries(network_host_demo PRIVATE network_host_mock)
network_warnings(network_host_demo)
endif()

include(CTest)
if(BUILD_TESTING AND NETWORK_BUILD_WIRE_CANDIDATE)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    foreach(suite IN ITEMS uart_m_core uart_projection uart_c_status uart_wave2b_stream typed48)
        add_executable(test_wire_${suite} tests/host/test_wire_${suite}.c)
        target_link_libraries(test_wire_${suite} PRIVATE network_wire_candidate)
        network_warnings(test_wire_${suite})
    endforeach()
    add_test(NAME wire2b.typed48_capability COMMAND test_wire_typed48)
    foreach(case IN ITEMS query warning_ack domain_alive warning availability
            digital_status digital_result user_settings query_end)
        add_test(NAME wire2b.m_${case} COMMAND test_wire_uart_m_core ${case})
    endforeach()
    foreach(case IN ITEMS alias semantics settings)
        add_test(NAME wire2b.core_${case} COMMAND test_wire_uart_m_core ${case})
    endforeach()
    foreach(case IN ITEMS m_bcm_door_state m_bcm_climate_state m_bcm_light_state
            m_bcm_status m_cis_environment m_cis_occupant m_cis_rear m_cis_status
            m_vss_status m_window_state m_window_fault)
        add_test(NAME wire2b.${case} COMMAND test_wire_uart_projection ${case})
    endforeach()
    foreach(case IN ITEMS p_vision_status_golden p_vision_status_ranges
            p_vision_status_framing p_vision_status_arguments
            p_vision_status_overlap p_vision_status_boundaries)
        add_test(NAME wire2b.${case} COMMAND test_wire_uart_c_status ${case})
    endforeach()
    foreach(case IN ITEMS structural42 fragmented42 suffix_and_timeout42)
        add_test(NAME wire2b.${case} COMMAND test_wire_uart_wave2b_stream ${case})
    endforeach()
    add_test(NAME wire.wave1_golden_readonly
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_wave1_golden.py --check)
    add_test(NAME wire.wave1_verifier_immutable
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/host/test_wave1_verifier.py)
    foreach(suite IN ITEMS can_bcm can_nodes can_coverage i1_can_vectors)
        add_executable(test_wire_${suite} tests/host/test_wire_${suite}.c)
        target_link_libraries(test_wire_${suite} PRIVATE network_wire_candidate)
        network_warnings(test_wire_${suite})
    endforeach()
    add_test(NAME wire.can22_typed_coverage COMMAND test_wire_can_coverage)
    add_test(NAME wire.i1_can_vectors COMMAND test_wire_i1_can_vectors)
    foreach(case IN ITEMS bcm_door_command bcm_climate_command bcm_light_command
            bcm_door_state bcm_climate_state bcm_light_state bcm_result bcm_event
            bcm_status bcm_semantic_holds bcm_seeded_boundaries)
        add_test(NAME wire2a.${case} COMMAND test_wire_can_bcm ${case})
    endforeach()
    foreach(case IN ITEMS window_command window_state window_event window_result window_fault
            cis_environment cis_occupant cis_status vss_event vss_warning_state vss_status
            nodes_environment_boundaries nodes_warning60_formats nodes_vss_status_history_u8
            nodes_window_execution_hold nodes_alias_atomicity nodes_seeded_metadata)
        add_test(NAME wire2a.${case} COMMAND test_wire_can_nodes ${case})
    endforeach()
    foreach(suite IN ITEMS can uart_dto stream registry)
        add_executable(test_wire_${suite} tests/host/test_wire_${suite}.c)
        target_link_libraries(test_wire_${suite} PRIVATE network_wire_candidate)
        network_warnings(test_wire_${suite})
    endforeach()
    foreach(case IN ITEMS can_golden_rear can_golden_permission can_envelope_rejection
            can_rear_semantics can_permission_raw can_arguments_alias can_randomized)
        add_test(NAME wire.${case} COMMAND test_wire_can ${case})
    endforeach()
    foreach(case IN ITEMS p_vision_result_golden p_permission_golden m_context_golden
            m_request_golden m_result_golden uart_dto_negative m_request_arguments
            uart_dto_alias_atomicity uart_dto_sentinels)
        add_test(NAME wire.${case} COMMAND test_wire_uart_dto ${case})
    endforeach()
    foreach(case IN ITEMS crc_and_golden frame_rejections frame_arguments fragments_and_isolation
            concatenation_and_resync expiry_and_original_times stream_atomicity reset_and_saturation
            deterministic_stream_replay)
        add_test(NAME wire.${case} COMMAND test_wire_stream ${case})
    endforeach()
    foreach(case IN ITEMS registry_layout registry_routes registry_coverage)
        add_test(NAME wire.${case} COMMAND test_wire_registry ${case})
    endforeach()
endif()
if(BUILD_TESTING)
    add_executable(test_runtime tests/host/test_runtime.c)
    target_link_libraries(test_runtime PRIVATE network_host_mock)
    network_warnings(test_runtime)
    foreach(case IN ITEMS rx_stable_deferred rx_ring_overflow rx_invalid_lengths
            age_boundaries time_hold tx_release_before_terminal tx_terminal_before_release
            tx_all_event_orders tx_capacity_and_stale tx_rejection_and_conflicts
            source_reservation_and_token_wrap consumer_and_product_hold port_errors_and_saturation
            callback_reentry port_swap)
        add_test(NAME runtime.${case} COMMAND test_runtime ${case})
    endforeach()

    add_executable(test_codec tests/host/test_codec.c)
    target_link_libraries(test_codec PRIVATE network_common)
    network_warnings(test_codec)
    foreach(case IN ITEMS can_capture_bounds explicit_configuration fixture_golden_and_atomic_output
            byte_fragments_and_worker_delay concatenation_and_stable_output uart_m_c_independent_instances
            noise_and_overlapping_sync bad_checksum_valid_suffix valid_boundary_payload_sync_opaque
            bad_length_before_body timeout_suffix_original_capture explicit_expiry_equality
            ambiguous_or_backward_time_hold gap_reset_and_saturating_counters maximum_frame_and_bounds_guard
            null_input_and_checksum_reject_output)
        add_test(NAME codec.${case} COMMAND test_codec ${case})
    endforeach()

    # Deterministic stress replay from the independent review; synthetic UART
    # fixture only. Instrumentation is determined by the selected toolchain.
    add_executable(test_uart_random_stress tests/host/test_uart_random_stress.c)
    target_link_libraries(test_uart_random_stress PRIVATE network_common)
    network_warnings(test_uart_random_stress)
    add_test(NAME codec.uart_random_stress COMMAND test_uart_random_stress)
    set_tests_properties(codec.uart_random_stress PROPERTIES TIMEOUT 30)

    # These are SDK-free fail-closed boundary tests, NOT MCU target builds.
    add_executable(test_platform tests/host/test_platform.c
        src/platform/s32k144/net_s32k144_port.c
        src/platform/s32k344/net_s32k344_port.c)
    target_link_libraries(test_platform PRIVATE network_host_mock)
    network_warnings(test_platform)
    foreach(case IN ITEMS clocks_and_instances stable_rx ring_overflow_and_wrap
                          explicit_tx_lifetime boards_fail_closed)
        add_test(NAME platform.${case} COMMAND test_platform ${case})
    endforeach()
    add_test(NAME host.demo COMMAND network_host_demo)
    foreach(board IN ITEMS s32k144 s32k344 unknown)
        add_test(NAME target.${board}_not_configured
            COMMAND ${CMAKE_COMMAND} -DNETWORK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}
                    -DNETWORK_BINARY=${CMAKE_CURRENT_BINARY_DIR}/negative-${board}
                    -DNETWORK_GENERATOR=${CMAKE_GENERATOR}
                    -DNETWORK_COMPILER=${CMAKE_C_COMPILER}
                    -DNETWORK_MAKE=${CMAKE_MAKE_PROGRAM}
                    -DNETWORK_BOARD=${board}
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/host/check_unconfigured.cmake)
    endforeach()
endif()

# Wave3A joins existing libraries only inside an explicitly selected Host test.
# All original test registration above remains in its original order.
option(NETWORK_BUILD_HOST_WIRE_STUB_TESTS "Build non-product Host wire integration tests" OFF)
if(NETWORK_BUILD_HOST_WIRE_STUB_TESTS)
    if(NOT NETWORK_TARGET STREQUAL "host")
        message(FATAL_ERROR "Wave3A requires NETWORK_TARGET=host; MCU binding is NOT_CONFIGURED")
    endif()
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "Wave3A requires BUILD_TESTING=ON")
    endif()
    if(NOT NETWORK_BUILD_WIRE_CANDIDATE)
        message(FATAL_ERROR "Wave3A requires NETWORK_BUILD_WIRE_CANDIDATE=ON")
    endif()
    add_executable(test_wave3a_host_bridge
        tests/host/test_wave3a_host_bridge.c
        tests/host/net_wave3a_test_rx.c
        tests/host/net_wave3a_test_tx.c)
    target_link_libraries(test_wave3a_host_bridge PRIVATE network_host_mock network_wire_candidate)
    network_warnings(test_wave3a_host_bridge)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/wave3a_link_targets.txt" CONTENT
"network_common=$<TARGET_PROPERTY:network_common,LINK_LIBRARIES>
network_host_mock=$<TARGET_PROPERTY:network_host_mock,LINK_LIBRARIES>
network_wire_candidate=$<TARGET_PROPERTY:network_wire_candidate,LINK_LIBRARIES>
test_wave3a_host_bridge=$<TARGET_PROPERTY:test_wave3a_host_bridge,LINK_LIBRARIES>
")
    foreach(case IN ITEMS
            optin_default_off optin_invalid_combinations
            rear_golden_observer rear_copy_lifetime rear_sidecar_missing_or_mismatch
            rear_envelope_flags rear_hold_and_not_bound rear_clock_and_backlog
            rear_singleflight_capacity permission_golden_borrow permission_caller_mutation
            permission_six_outcome_orders permission_rejection_release
            permission_stale_conflict permission_submit_failures permission_age_boundaries
            permission_retire_reuse callback_reentry product_holds_unchanged
            regression_names_and_instrumentation)
        if(case STREQUAL "optin_default_off" OR case STREQUAL "optin_invalid_combinations" OR
           case STREQUAL "product_holds_unchanged" OR case STREQUAL "regression_names_and_instrumentation")
            add_test(NAME wave3a.${case} COMMAND ${CMAKE_COMMAND}
                "-DNETWORK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DNETWORK_PARENT_BINARY=${CMAKE_CURRENT_BINARY_DIR}"
                "-DNETWORK_GENERATOR=${CMAKE_GENERATOR}"
                "-DNETWORK_COMPILER=${CMAKE_C_COMPILER}"
                "-DNETWORK_MAKE=${CMAKE_MAKE_PROGRAM}"
                "-DNETWORK_NM=${CMAKE_NM}"
                "-DNETWORK_CTEST=${CMAKE_CTEST_COMMAND}"
                "-DNETWORK_STUB_EXECUTABLE=$<TARGET_FILE:test_wave3a_host_bridge>"
                "-DNETWORK_COMMON_LIBRARY=$<TARGET_FILE:network_common>"
                "-DNETWORK_MOCK_LIBRARY=$<TARGET_FILE:network_host_mock>"
                "-DNETWORK_WIRE_LIBRARY=$<TARGET_FILE:network_wire_candidate>"
                "-DNETWORK_CASE=${case}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/host/test_wave3a_build_gates.cmake)
            set_tests_properties(wave3a.${case} PROPERTIES TIMEOUT 300)
        else()
            add_test(NAME wave3a.${case} COMMAND test_wave3a_host_bridge ${case})
        endif()
    endforeach()
endif()

# Compile-time Network storage role. This does not select any MCU/RTD binding.
set(NETWORK_NODE_PROFILE "HOST" CACHE STRING "HOST, CIS, CIS_CAN_ONLY, BCM, WINDOW, DOMAIN, VSS")
set_property(CACHE NETWORK_NODE_PROFILE PROPERTY STRINGS HOST CIS CIS_CAN_ONLY BCM WINDOW DOMAIN VSS)
set(_network_profiles HOST CIS CIS_CAN_ONLY BCM WINDOW DOMAIN VSS)
list(FIND _network_profiles "${NETWORK_NODE_PROFILE}" _network_profile_id)
if(_network_profile_id LESS 0)
    message(FATAL_ERROR "Unknown NETWORK_NODE_PROFILE")
endif()
# HAL-independent product implementation; wire adoption remains explicitly selected.
option(NETWORK_BUILD_UPPER_STACK "Build the upper Network product service" OFF)
if(NETWORK_BUILD_UPPER_STACK)
    if(NOT NETWORK_BUILD_WIRE_CANDIDATE)
        message(FATAL_ERROR "Upper Network requires explicit NETWORK_BUILD_WIRE_CANDIDATE=ON")
    endif()
    add_library(network_product STATIC
        src/l03_message_service/m04_message_service/net_upper_service.c
        src/l03_message_service/m04_message_service/net_upper_profile.c
        src/l01_node_context/m01_source_context/net_upper_source.c
        src/l01_node_context/m01_source_context/net_upper_receipt.c
        src/l01_node_context/m01_source_context/net_upper_monitor.c
        src/l03_message_service/m04_message_service/net_upper_can.c
        src/l03_message_service/m04_message_service/net_upper_uart.c
        src/l01_node_context/m02_query_control/net_upper_query.c
        src/l02_typed_handoff/m03_typed_handoff/net_upper_adapter.c
        src/l02_typed_handoff/m03_typed_handoff/net_ecu_adapter.c
        src/l05_transport_port/m07_buffer_outcome/net_upper_outcome.c
        src/l05_transport_port/m07_buffer_outcome/net_upper_detail.c
        src/l03_message_service/m04_message_service/net_upper_control.c
        src/l05_transport_port/m08_device_port/net_upper_port.c
        src/l05_transport_port/m09_observation/net_upper_observation.c
        src/l05_transport_port/m09_observation/net_upper_diagnostics.c)
    target_compile_definitions(network_product PUBLIC NETWORK_UPPER_NODE_PROFILE=${_network_profile_id})
    target_include_directories(network_product PUBLIC include PRIVATE
        src/l03_message_service/m04_message_service)
    target_link_libraries(network_product PUBLIC network_wire_candidate network_common)
    network_warnings(network_product)
    if(BUILD_TESTING AND NETWORK_NODE_PROFILE STREQUAL "HOST")
        foreach(suite IN ITEMS adversarial uart query can_routes e2e rx_identity control epoch)
            add_executable(test_upper_${suite} tests/product/test_upper_${suite}.c)
            target_link_libraries(test_upper_${suite} PRIVATE network_product)
            network_warnings(test_upper_${suite})
        endforeach()
        foreach(case IN ITEMS product_rear event_orders capacity reentry can_metadata receive_order
                owner_reject age_boundaries time_failures overflow_health reset_borrow conflict permission_revoke
                source_rejuvenation tx_age_mismatch uart_source_rejuvenation wrong_link_outcome)
            add_test(NAME product.core_${case} COMMAND test_upper_adversarial ${case})
        endforeach()
        foreach(case IN ITEMS dispatch26 age26 query_context17 receiver26 stream_errors buffered_suffix projection11 product_routes product_holds)
            add_test(NAME product.uart_${case} COMMAND test_upper_uart ${case})
        endforeach()
        foreach(case IN ITEMS recent5 capacity3 getter_failure getter_overflow final_count final_release
                empty_absence reentry window_hold plan_hold session_change rear_projection
                passive_revocation reset_hold queue_pressure end_sequence all_getters specific_result)
            add_test(NAME product.query_${case} COMMAND test_upper_query ${case})
        endforeach()
        foreach(case IN ITEMS domain_permission bcm_door_command bcm_climate_command bcm_light_command
                window_command bcm_door_state bcm_climate_state bcm_light_state bcm_result bcm_event
                bcm_status cis_environment cis_occupant cis_rear cis_status window_state window_event
                window_result window_fault vss_event vss_warning_state vss_status permission_fanout partial_lanes)
            add_test(NAME product.can_${case} COMMAND test_upper_can_routes ${case})
        endforeach()
        foreach(case IN ITEMS bcm_commands sensors_vision permission_fanout diagnostics_warning_ack
                window_disabled metadata_age recovery_failures)
            add_test(NAME product.e2e_${case} COMMAND test_upper_e2e ${case})
        endforeach()
        foreach(case IN ITEMS can_rear can_permission uart_c_vision uart_m_request uart_m_context
                uart_m_result rejected_owner envelope_boundaries)
            add_test(NAME product.rx_identity_${case} COMMAND test_upper_rx_identity ${case})
        endforeach()
        foreach(case IN ITEMS uart_receipt uart_receipt_no_age receipt_epoch tx_detail failure_notice
                failure_notice_versions failure_notice_capacity failure_notice_default_off
                quiesce_empty quiesce_queued quiesce_six_orders quiesce_conflict quiesce_uart
                permission_relay permission_hold permission_epoch diagnostics control_arguments
                quiesce_independent failure_notice_exhaustion)
            add_test(NAME product.control_${case} COMMAND test_upper_control ${case})
        endforeach()
        foreach(case IN ITEMS old_receipt_read old_receipt_enqueue queued_rear_reset queued_permission_reset
                queued_vision_reset newer_receipt_immutable source_error epoch_recovery buffered_error)
            add_test(NAME product.epoch_${case} COMMAND test_upper_epoch ${case})
        endforeach()
    endif()
endif()

# SDK-independent injected backend adapters. The same code builds natively or
# with a caller-selected target toolchain; no RTD/board peripheral is selected.
option(NETWORK_BUILD_HAL_PORT_ADAPTER "Build SDK-free injected Port Adapters" OFF)
if(NETWORK_BUILD_HAL_PORT_ADAPTER)
    if(NOT NETWORK_BUILD_UPPER_STACK)
        message(FATAL_ERROR "HAL Port Adapter requires NETWORK_BUILD_UPPER_STACK=ON")
    endif()
    add_library(network_hal_port_adapter STATIC
        src/l05_transport_port/m08_device_port/net_hal_adapter.c
        src/platform/s32k144/net_s32k144_upper_port.c
        src/platform/s32k344/net_s32k344_upper_port.c)
    target_link_libraries(network_hal_port_adapter PUBLIC network_product)
    network_warnings(network_hal_port_adapter)
    if(BUILD_TESTING AND NETWORK_NODE_PROFILE STREQUAL "HOST")
        add_executable(test_hal_adapter tests/product/test_hal_adapter.c)
        target_link_libraries(test_hal_adapter PRIVATE network_hal_port_adapter)
        network_warnings(test_hal_adapter)
        foreach(case IN ITEMS binding s32k144_rear s32k344_rear uart_product uart_bytes can_metadata
                borrow_copy tx_order submit_reject stale_key tx_conflict rx_pressure tx_pressure
                reset_borrow clock callback_order backend_violation tx_rx_pressure loss_error footprint
                reset_then_facts settled_then_reset error_reset_error reset_pressure order_exhaustion
                tx_metadata_fault tx_capture_error)
            add_test(NAME hal.${case} COMMAND test_hal_adapter ${case})
        endforeach()
    endif()
endif()

# Memory regressions are appended after the exact existing273 test inventory.
if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    add_executable(test_init_memory tests/product/test_init_memory.c)
    target_link_libraries(test_init_memory PRIVATE network_product)
    network_warnings(test_init_memory)
    foreach(case IN ITEMS invalid_arguments uart_m_policy uart_c_policy config_overlap ops_overlap
            context_overlap poisoned_success active_borrow live_reinit two_instances inactive_uart_policy)
        add_test(NAME memory.init_${case} COMMAND test_init_memory ${case})
    endforeach()
endif()

if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    add_executable(test_uart_memory tests/memory/test_uart_memory.c)
    target_link_libraries(test_uart_memory PRIVATE network_product)
    target_include_directories(test_uart_memory PRIVATE src/l03_message_service/m04_message_service)
    network_warnings(test_uart_memory)
    foreach(case IN ITEMS stream_init_failure stream_input_atomic stream_retained_atomic stream_split
            stream_crc_suffix receiver_init_failure receiver_history_atomic receiver_interleave
            receiver_typed_reject receiver_signal_failure receiver_signal_epoch query_begin_failure
            query_begin_alias query_begin_replace control_quiesce_atomic control_resume_atomic
            stream_capacity receiver_output_alias)
        add_test(NAME memory.uart_${case} COMMAND test_uart_memory ${case})
    endforeach()
endif()
if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NOT NETWORK_NODE_PROFILE STREQUAL "HOST")
    add_executable(test_node_profile tests/product/test_node_profile.c)
    target_link_libraries(test_node_profile PRIVATE network_product)
    target_include_directories(test_node_profile PRIVATE src/l03_message_service/m04_message_service)
    network_warnings(test_node_profile)
    foreach(case IN ITEMS storage wrong_node nonowned_receipts can_routes tx_capacity rx_capacity uart_boundary footprint)
        add_test(NAME profile.${NETWORK_NODE_PROFILE}_${case} COMMAND test_node_profile ${case})
    endforeach()
endif()

# Buildable aggregate of real portable static libraries; no empty placeholder C.
if(NETWORK_BUILD_UPPER_STACK)
    add_custom_target(portable_network DEPENDS network_product)
    if(TARGET network_hal_port_adapter)
        add_dependencies(portable_network network_hal_port_adapter)
    endif()
endif()

option(NETWORK_BUILD_DOMAIN_C2 "Compile against explicit pinned actual DOMAIN C2 headers" OFF)
set(NETWORK_DOMAIN_C2_INCLUDE_DIR "" CACHE PATH "Directory containing actual Domain_Interface_v0.5_C2_target.h and Vehicle_Types_v0.2.h")
if(NETWORK_BUILD_DOMAIN_C2)
    if(NOT NETWORK_BUILD_UPPER_STACK OR NOT EXISTS "${NETWORK_DOMAIN_C2_INCLUDE_DIR}/Domain_Interface_v0.5_C2_target.h")
        message(FATAL_ERROR "C2 Adapter requires upper stack and explicit actual NETWORK_DOMAIN_C2_INCLUDE_DIR")
    endif()
    add_library(network_domain_c2 STATIC
        src/l02_typed_handoff/m03_typed_handoff/net_domain_c2.c
        src/l02_typed_handoff/m03_typed_handoff/net_domain_rx.c
        src/l02_typed_handoff/m03_typed_handoff/net_domain_tx.c
        src/l02_typed_handoff/m03_typed_handoff/net_domain_uart.c
        src/l02_typed_handoff/m03_typed_handoff/net_domain_query.c
        src/l02_typed_handoff/m03_typed_handoff/net_domain_owner_binding.c)
    target_include_directories(network_domain_c2 PUBLIC "${NETWORK_DOMAIN_C2_INCLUDE_DIR}")
    target_link_libraries(network_domain_c2 PUBLIC network_product)
    network_warnings(network_domain_c2)
    add_dependencies(portable_network network_domain_c2)
    include(tests/product/domain_pinned_owner.cmake)
    if(BUILD_TESTING AND NETWORK_NODE_PROFILE STREQUAL "HOST")
        add_executable(test_domain_c2 tests/product/test_domain_c2.c)
        target_link_libraries(test_domain_c2 PRIVATE network_domain_c2)
        network_warnings(test_domain_c2)
        foreach(case IN ITEMS units observer_failure request_tags command_lifetime expiry context_cancel
                failed_sink result_mapping query_binding missing_getters route_slots window_and_warning)
            add_test(NAME domain_c2_${case} COMMAND test_domain_c2 ${case})
        endforeach()
        add_executable(test_domain_routes tests/product/test_domain_routes.c)
        target_link_libraries(test_domain_routes PRIVATE network_domain_c2)
        network_warnings(test_domain_routes)
        add_test(NAME domain_c2_all_routes COMMAND test_domain_routes)
    endif()
endif()

if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    foreach(suite IN ITEMS can uart)
        add_executable(test_ecu_adapter_${suite} tests/product/test_ecu_adapter_${suite}.c)
        target_link_libraries(test_ecu_adapter_${suite} PRIVATE network_product)
        network_warnings(test_ecu_adapter_${suite})
    endforeach()
    foreach(case IN ITEMS can25 missing_alias)
        add_test(NAME product.ecu_adapter_${case} COMMAND test_ecu_adapter_can ${case})
    endforeach()
    foreach(case IN ITEMS uart26 uart_missing)
        add_test(NAME product.ecu_adapter_${case} COMMAND test_ecu_adapter_uart ${case})
    endforeach()
endif()

option(NETWORK_BUILD_VIRTUAL_ECU "Build executable Host virtual backend and selected ECU worker" OFF)
set(NETWORK_VIRTUAL_BIN_DIR "" CACHE PATH "Prepared directory containing VIRTUAL_<profile> ECU builds")
if(NETWORK_BUILD_VIRTUAL_ECU)
    if(NOT NETWORK_TARGET STREQUAL "host" OR NOT NETWORK_BUILD_HAL_PORT_ADAPTER)
        message(FATAL_ERROR "Virtual ECU requires Host and the injected Port Adapter")
    endif()
    add_library(network_host_virtual STATIC src/platform/host_virtual/net_virtual.c)
    target_link_libraries(network_host_virtual PUBLIC network_hal_port_adapter)
    network_warnings(network_host_virtual)
    set(_virtual_role ${NETWORK_NODE_PROFILE})
    if(_virtual_role STREQUAL "HOST")
        set(_virtual_role PEER)
    endif()
    add_executable(network_virtual_${_virtual_role} tests/virtual_ecu/virtual_ecu.c)
    target_link_libraries(network_virtual_${_virtual_role} PRIVATE network_host_virtual)
    network_warnings(network_virtual_${_virtual_role})
    if(_virtual_role STREQUAL "DOMAIN")
        target_sources(network_virtual_DOMAIN PRIVATE tests/virtual_ecu/virtual_domain_owner.c)
        target_link_libraries(network_virtual_DOMAIN PRIVATE network_domain_c2)
    endif()
    if(BUILD_TESTING AND NETWORK_NODE_PROFILE STREQUAL "HOST")
        add_executable(test_virtual_backend tests/virtual_ecu/test_virtual_backend.c)
        target_link_libraries(test_virtual_backend PRIVATE network_host_virtual)
        network_warnings(test_virtual_backend)
        foreach(case IN ITEMS phases saturation reset clock early_release deadline)
            add_test(NAME virtual.backend_${case} COMMAND test_virtual_backend ${case})
        endforeach()
        if(NETWORK_VIRTUAL_BIN_DIR)
            find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
            foreach(scenario IN ITEMS rear mobile permission vision query faults)
                add_test(NAME virtual.ecu_${scenario} COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/virtual_ecu/network_virtual_demo.py
                    --bin-dir ${NETWORK_VIRTUAL_BIN_DIR} --scenario ${scenario})
            endforeach()
        endif()
    endif()
endif()

if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    add_executable(test_upper_monitor tests/product/test_upper_monitor.c)
    target_link_libraries(test_upper_monitor PRIVATE network_product)
    network_warnings(test_upper_monitor)
    foreach(case IN ITEMS permission_age permission_owner route_watch held_route)
        add_test(NAME product.monitor_${case} COMMAND test_upper_monitor ${case})
    endforeach()
endif()

if(BUILD_TESTING AND NETWORK_BUILD_UPPER_STACK AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    add_executable(test_semantic_identity tests/product/test_semantic_identity.c)
    target_link_libraries(test_semantic_identity PRIVATE network_product)
    target_include_directories(test_semantic_identity PRIVATE src/l03_message_service/m04_message_service)
    network_warnings(test_semantic_identity)
    foreach(case IN ITEMS can_rear_collision can_permission_collision can_independent_lanes can_vss_lanes
            can_command_immutable can_result_progression uart_context_lanes uart_permission_collision
            uart_projection_collision uart_vision_callback rx_collision_atomic rx_duplicate_refresh)
        add_test(NAME product.semantic_${case} COMMAND test_semantic_identity ${case})
    endforeach()
endif()
