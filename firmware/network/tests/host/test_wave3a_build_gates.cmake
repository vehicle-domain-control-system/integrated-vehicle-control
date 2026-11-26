# NON-PRODUCT test-only build isolation and regression gates.
# Original ordered test names and exact source locks remain enforced.
# Six source hashes reflect the authorized full-product revision: current docs,
# CAN ESI capture metadata, and portable-library/physical-HAL build boundary.
# Prior/current hashes and a tamper rejection are retained in delivery proof.
cmake_minimum_required(VERSION 3.16)
foreach(required NETWORK_SOURCE NETWORK_PARENT_BINARY NETWORK_GENERATOR NETWORK_COMPILER
        NETWORK_CTEST NETWORK_CASE NETWORK_NM)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Wave3A gate missing ${required}")
    endif()
endforeach()

function(w3a_run label)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${work}/${label}.log" "${output}\nSTDERR:\n${error}")
    string(REPLACE ";" "\n  " command_lines "${ARGN}")
    file(APPEND "${work}/commands.log" "${label}\n  ${command_lines}\nexit=${rc}\n")
    set(last_rc "${rc}" PARENT_SCOPE)
    set(last_output "${output}" PARENT_SCOPE)
    set(last_error "${error}" PARENT_SCOPE)
endfunction()

function(w3a_ok label)
    w3a_run("${label}" ${ARGN})
    if(NOT last_rc EQUAL 0)
        message(FATAL_ERROR "${label} failed exit=${last_rc}: ${last_output}\n${last_error}")
    endif()
    set(last_output "${last_output}" PARENT_SCOPE)
endfunction()

function(w3a_configure tag candidate stub testing target expected)
    set(args "${CMAKE_COMMAND}" -S "${NETWORK_SOURCE}" -B "${work}/${tag}"
        -G "${NETWORK_GENERATOR}" "-DCMAKE_C_COMPILER=${NETWORK_COMPILER}"
        "-DNETWORK_TARGET=${target}" "-DBUILD_TESTING=${testing}")
    if(DEFINED NETWORK_MAKE AND NOT NETWORK_MAKE STREQUAL "")
        list(APPEND args "-DCMAKE_MAKE_PROGRAM=${NETWORK_MAKE}")
    endif()
    if(NOT candidate STREQUAL "DEFAULT")
        list(APPEND args "-DNETWORK_BUILD_WIRE_CANDIDATE=${candidate}")
    endif()
    if(NOT stub STREQUAL "DEFAULT")
        list(APPEND args "-DNETWORK_BUILD_HOST_WIRE_STUB_TESTS=${stub}")
    endif()
    w3a_run("${tag}_configure" ${args})
    if(expected STREQUAL "PASS")
        if(NOT last_rc EQUAL 0)
            message(FATAL_ERROR "${tag} configure failed: ${last_output}\n${last_error}")
        endif()
    else()
        if(last_rc EQUAL 0)
            message(FATAL_ERROR "${tag} unexpectedly configured an invalid option combination")
        endif()
        string(FIND "${last_output}${last_error}" "${expected}" found)
        if(found LESS 0)
            message(FATAL_ERROR "${tag} failed for an unexpected reason: ${last_output}\n${last_error}")
        endif()
        # The explicit OFF input must remain OFF even when configure is rejected.
        if(EXISTS "${work}/${tag}/CMakeCache.txt")
            file(READ "${work}/${tag}/CMakeCache.txt" cache)
            if(candidate STREQUAL "OFF" AND NOT cache MATCHES "NETWORK_BUILD_WIRE_CANDIDATE:BOOL=OFF")
                message(FATAL_ERROR "${tag}: candidate input was changed")
            endif()
            if(testing STREQUAL "OFF" AND NOT cache MATCHES "BUILD_TESTING:BOOL=OFF")
                message(FATAL_ERROR "${tag}: BUILD_TESTING input was changed")
            endif()
        endif()
        message(STATUS "W3A_TRACE {\"test\":\"wave3a.${NETWORK_CASE}\",\"phase\":\"${tag}\",\"actual_exit\":${last_rc},\"expected_configure_rejection\":true}")
    endif()
endfunction()

function(w3a_inventory binary expected_names label)
    w3a_ok("${label}_inventory" "${NETWORK_CTEST}" --test-dir "${binary}" --show-only=json-v1)
    # CTest inventory JSON is kept verbatim; generated add_test order is checked
    # without requiring CMake's newer JSON parser (project supports CMake 3.16).
    file(STRINGS "${binary}/CTestTestfile.cmake" registrations REGEX "^add_test\\(")
    set(actual)
    foreach(line IN LISTS registrations)
        string(REGEX REPLACE "^add_test\\(([^ ]+) .*" "\\1" name "${line}")
        string(REPLACE "[=[" "" name "${name}")
        string(REPLACE "]=]" "" name "${name}")
        string(REPLACE "\"" "" name "${name}")
        list(APPEND actual "${name}")
    endforeach()
    if(NOT "${actual}" STREQUAL "${expected_names}")
        message(FATAL_ERROR "${label}: exact ordered CTest names changed: ${actual}")
    endif()
    list(LENGTH actual count)
    message(STATUS "W3A_TRACE {\"test\":\"wave3a.${NETWORK_CASE}\",\"phase\":\"${label}\",\"actual_test_count\":${count},\"ordered_names_match\":true}")
endfunction()

function(w3a_no_stub binary label)
    file(READ "${binary}/CMakeCache.txt" cache)
    if(NOT cache MATCHES "NETWORK_BUILD_HOST_WIRE_STUB_TESTS:BOOL=OFF")
        message(FATAL_ERROR "${label}: stub default is not OFF")
    endif()
    file(READ "${binary}/compile_commands.json" compile_commands)
    if(compile_commands MATCHES "net_wave3a_test_|test_wave3a_host_bridge")
        message(FATAL_ERROR "${label}: stub source compiled while OFF")
    endif()
    w3a_ok("${label}_targets" "${CMAKE_COMMAND}" --build "${binary}" --target help)
    if(last_output MATCHES "test_wave3a_host_bridge")
        message(FATAL_ERROR "${label}: stub target exists while OFF")
    endif()
    file(GLOB binaries "${binary}/*.a" "${binary}/*.exe" "${binary}/network_host_demo" "${binary}/test_*")
    set(symbol_files 0)
    foreach(b IN LISTS binaries)
        if(IS_DIRECTORY "${b}" OR b MATCHES "\\.(cmake|json|txt)$")
            continue()
        endif()
        get_filename_component(leaf "${b}" NAME)
        w3a_ok("${label}_nm_${leaf}" "${NETWORK_NM}" -g --defined-only "${b}")
        if(last_output MATCHES "net_wave3a_test_|test_wave3a_host_bridge")
            message(FATAL_ERROR "${label}: stub symbol exists while OFF")
        endif()
        math(EXPR symbol_files "${symbol_files}+1")
    endforeach()
    if(symbol_files LESS 2)
        message(FATAL_ERROR "${label}: no actual built symbol evidence")
    endif()
    message(STATUS "W3A_TRACE {\"test\":\"wave3a.${NETWORK_CASE}\",\"phase\":\"${label}_isolation\",\"symbol_files_checked\":${symbol_files},\"stub_symbols\":0,\"stub_target\":false}")
endfunction()

set(W3A_OFF_NAMES
    runtime.rx_stable_deferred
    runtime.rx_ring_overflow
    runtime.rx_invalid_lengths
    runtime.age_boundaries
    runtime.time_hold
    runtime.tx_release_before_terminal
    runtime.tx_terminal_before_release
    runtime.tx_all_event_orders
    runtime.tx_capacity_and_stale
    runtime.tx_rejection_and_conflicts
    runtime.source_reservation_and_token_wrap
    runtime.consumer_and_product_hold
    runtime.port_errors_and_saturation
    runtime.callback_reentry
    runtime.port_swap
    codec.can_capture_bounds
    codec.explicit_configuration
    codec.fixture_golden_and_atomic_output
    codec.byte_fragments_and_worker_delay
    codec.concatenation_and_stable_output
    codec.uart_m_c_independent_instances
    codec.noise_and_overlapping_sync
    codec.bad_checksum_valid_suffix
    codec.valid_boundary_payload_sync_opaque
    codec.bad_length_before_body
    codec.timeout_suffix_original_capture
    codec.explicit_expiry_equality
    codec.ambiguous_or_backward_time_hold
    codec.gap_reset_and_saturating_counters
    codec.maximum_frame_and_bounds_guard
    codec.null_input_and_checksum_reject_output
    codec.uart_random_stress
    platform.clocks_and_instances
    platform.stable_rx
    platform.ring_overflow_and_wrap
    platform.explicit_tx_lifetime
    platform.boards_fail_closed
    host.demo
    target.s32k144_not_configured
    target.s32k344_not_configured
    target.unknown_not_configured
)

set(W3A_ON_NAMES
    wire2b.typed48_capability
    wire2b.m_query
    wire2b.m_warning_ack
    wire2b.m_domain_alive
    wire2b.m_warning
    wire2b.m_availability
    wire2b.m_digital_status
    wire2b.m_digital_result
    wire2b.m_user_settings
    wire2b.m_query_end
    wire2b.core_alias
    wire2b.core_semantics
    wire2b.core_settings
    wire2b.m_bcm_door_state
    wire2b.m_bcm_climate_state
    wire2b.m_bcm_light_state
    wire2b.m_bcm_status
    wire2b.m_cis_environment
    wire2b.m_cis_occupant
    wire2b.m_cis_rear
    wire2b.m_cis_status
    wire2b.m_vss_status
    wire2b.m_window_state
    wire2b.m_window_fault
    wire2b.p_vision_status_golden
    wire2b.p_vision_status_ranges
    wire2b.p_vision_status_framing
    wire2b.p_vision_status_arguments
    wire2b.p_vision_status_overlap
    wire2b.p_vision_status_boundaries
    wire2b.structural42
    wire2b.fragmented42
    wire2b.suffix_and_timeout42
    wire.wave1_golden_readonly
    wire.wave1_verifier_immutable
    wire.can22_typed_coverage
    wire.i1_can_vectors
    wire2a.bcm_door_command
    wire2a.bcm_climate_command
    wire2a.bcm_light_command
    wire2a.bcm_door_state
    wire2a.bcm_climate_state
    wire2a.bcm_light_state
    wire2a.bcm_result
    wire2a.bcm_event
    wire2a.bcm_status
    wire2a.bcm_semantic_holds
    wire2a.bcm_seeded_boundaries
    wire2a.window_command
    wire2a.window_state
    wire2a.window_event
    wire2a.window_result
    wire2a.window_fault
    wire2a.cis_environment
    wire2a.cis_occupant
    wire2a.cis_status
    wire2a.vss_event
    wire2a.vss_warning_state
    wire2a.vss_status
    wire2a.nodes_environment_boundaries
    wire2a.nodes_warning60_formats
    wire2a.nodes_vss_status_history_u8
    wire2a.nodes_window_execution_hold
    wire2a.nodes_alias_atomicity
    wire2a.nodes_seeded_metadata
    wire.can_golden_rear
    wire.can_golden_permission
    wire.can_envelope_rejection
    wire.can_rear_semantics
    wire.can_permission_raw
    wire.can_arguments_alias
    wire.can_randomized
    wire.p_vision_result_golden
    wire.p_permission_golden
    wire.m_context_golden
    wire.m_request_golden
    wire.m_result_golden
    wire.uart_dto_negative
    wire.m_request_arguments
    wire.uart_dto_alias_atomicity
    wire.uart_dto_sentinels
    wire.crc_and_golden
    wire.frame_rejections
    wire.frame_arguments
    wire.fragments_and_isolation
    wire.concatenation_and_resync
    wire.expiry_and_original_times
    wire.stream_atomicity
    wire.reset_and_saturation
    wire.deterministic_stream_replay
    wire.registry_layout
    wire.registry_routes
    wire.registry_coverage
    runtime.rx_stable_deferred
    runtime.rx_ring_overflow
    runtime.rx_invalid_lengths
    runtime.age_boundaries
    runtime.time_hold
    runtime.tx_release_before_terminal
    runtime.tx_terminal_before_release
    runtime.tx_all_event_orders
    runtime.tx_capacity_and_stale
    runtime.tx_rejection_and_conflicts
    runtime.source_reservation_and_token_wrap
    runtime.consumer_and_product_hold
    runtime.port_errors_and_saturation
    runtime.callback_reentry
    runtime.port_swap
    codec.can_capture_bounds
    codec.explicit_configuration
    codec.fixture_golden_and_atomic_output
    codec.byte_fragments_and_worker_delay
    codec.concatenation_and_stable_output
    codec.uart_m_c_independent_instances
    codec.noise_and_overlapping_sync
    codec.bad_checksum_valid_suffix
    codec.valid_boundary_payload_sync_opaque
    codec.bad_length_before_body
    codec.timeout_suffix_original_capture
    codec.explicit_expiry_equality
    codec.ambiguous_or_backward_time_hold
    codec.gap_reset_and_saturating_counters
    codec.maximum_frame_and_bounds_guard
    codec.null_input_and_checksum_reject_output
    codec.uart_random_stress
    platform.clocks_and_instances
    platform.stable_rx
    platform.ring_overflow_and_wrap
    platform.explicit_tx_lifetime
    platform.boards_fail_closed
    host.demo
    target.s32k144_not_configured
    target.s32k344_not_configured
    target.unknown_not_configured
)

set(W3A_NEW_NAMES
    wave3a.optin_default_off
    wave3a.optin_invalid_combinations
    wave3a.rear_golden_observer
    wave3a.rear_copy_lifetime
    wave3a.rear_sidecar_missing_or_mismatch
    wave3a.rear_envelope_flags
    wave3a.rear_hold_and_not_bound
    wave3a.rear_clock_and_backlog
    wave3a.rear_singleflight_capacity
    wave3a.permission_golden_borrow
    wave3a.permission_caller_mutation
    wave3a.permission_six_outcome_orders
    wave3a.permission_rejection_release
    wave3a.permission_stale_conflict
    wave3a.permission_submit_failures
    wave3a.permission_age_boundaries
    wave3a.permission_retire_reuse
    wave3a.callback_reentry
    wave3a.product_holds_unchanged
    wave3a.regression_names_and_instrumentation
)

set(W3A_LOCKED_SOURCE
    ".gitignore|ded160b83ba8aca635482ef1eb04985c6ad305b2f4a3d205247fe932306c0ff5"
    "BOARD_BINDING_STATUS.md|6589fcbcd9e5075a6903b00d20c64b0adc55bb7615d28b1aa597846007386fef"
    "IMPLEMENTATION_MATRIX.csv|b343b0f5e9e82db6506d87bd533fe69f7dcd98e0797e88cae84ec7fbb10ca6cb"
    "README.md|537a0a575b5a36f8bfad67ddfe2c040e343e605e9b3273270eec950f1dd12cbf"
    "SOURCE_MAPPING.md|a7b0a67f41dae190afcb8b8e50f0f0ada54531d7c9c3a541a8055731fb60e387"
    "WIRE_CANDIDATE_STATUS.md|4a068a9e99ba3c9e4d4480ad08fd175c25437c86a083bc3e9c0ce5e8de6aba9e"
    "WIRE_WAVE2A_STATUS.md|51bc3006fe8e002f2456fbd84c8a880e567a7195b57058c7c629611ea4fd3afa"
    "WIRE_WAVE2B_STATUS.md|c62ba2763fa4bbf47b577cef3add789c7f2d1395fb42f74e8a90baab118f3611"
    "include/network/net_board_ports.h|99400f36a96ed6a679ab3ca006477e5efb237fdf5579c99751ce25d69e7c2993"
    "include/network/net_codec.h|4a4863b2e0a2862a1dfa7b3afaf112b70b8da7995d003363b5ba077921b58be3"
    "include/network/net_host_mock.h|f1f7da68ebd89af417d5e37f37b0111e83d366a4ac945339fa2c117bf3c0c810"
    "include/network/net_port.h|326212709fbfe406ec10fec29e6cb94924b38e52d8d5d166414ff2c02437cf92"
    "include/network/net_runtime.h|8807253e3c15fc2dbb5f218016abb346cdee29981fc44756014b13fc8f462fbf"
    "include/network/net_source.h|26dc56ec5af58833d482d6e55f4d2f35e649981614abecfd8fafc8b7e98195d0"
    "include/network/net_types.h|b42082f0e2466f819881a35897d19aa33bd7b32089d8163e84d2660813ce21b2"
    "include/network/net_wire.h|006328669ebbf0665deec0f0e2ed98fe1d8f20fb2084fd2278eccf43926695e3"
    "include/network/net_wire_can.h|4029344e98aee5cb0acd4eba38bbad5d694ba8524a28db1de2d087e2d58bd524"
    "include/network/net_wire_can_bcm.h|63fe8ab488678ca3df6cfeec263b5092357b46faad385208bb015651ae67ea2f"
    "include/network/net_wire_can_nodes.h|f0ec8b746e90d1b962d5c39f9fa52426332cebf1498363d06c42d47112bb701a"
    "include/network/net_wire_uart.h|eefeacf0246ac2ba2b851caddefcc993fb82c10eee40acd642bb1b31bcbabc93"
    "include/network/net_wire_uart_c_status.h|3a15b8a6f7ecd62c540c7527a5a2906c96660183b98a6ce99f6dfd60455daecf"
    "include/network/net_wire_uart_dto.h|91647ea711f599f603de52dc61efaaf1390fb8364f9f742eaaf53dae68cb82ae"
    "include/network/net_wire_uart_m_core.h|98e4abc70d52dabf6a0cf7d15cd3c86a6f77564d6e8cb5bc32343468aea7ab43"
    "include/network/net_wire_uart_m_projection.h|85804b41156e1e1af8d83b24b10c3de0608c39dbed9f210691b3d5775053b0e1"
    "src/adapters/README.md|beccdc84c35a1e8e8965e21a05418da8a8c1251ce4ebd7044eb915d28b4ab6df"
    "src/l01_node_context/m01_source_context/net_source.c|0134a62d96287c544ed5443de48f41ada50ed6c8c67a70d1bcfc36cc848e6d11"
    "src/l01_node_context/m02_query_control/README.md|f5a9c1e75b98ad63ba3e52bee8c7c80e86f7f903ab4e7449bf4f4edc5e5beab1"
    "src/l02_typed_handoff/m03_typed_handoff/net_handoff.c|a9da22758eb76b9eefdc19d87388a6ea1972470d7ef8dcc62e478a6f970c7054"
    "src/l02_typed_handoff/m03_typed_handoff/net_handoff.h|324eef9fb1eb6a611dd488f57f1d400fd4576cc9a36d34102e38a3b29f8da090"
    "src/l03_message_service/m04_message_service/net_runtime.c|4e668e41d7495627afc47b3b317f98d16de1492925185148875b27fab062eea7"
    "src/l04_frame_codec/m05_can_codec/net_can_codec.c|ef105b2e4eae1a1f40749782aae105eccbf664c9facc648f20d35e418b4d4b2c"
    "src/l04_frame_codec/m05_can_codec/net_wire_can.c|90f25b8024ecfdeec325652b87d4f5e42f3e53c1ad7e41123287933e7b747155"
    "src/l04_frame_codec/m05_can_codec/net_wire_can_bcm_raw.c|aafe3b975a775284bbe047a3ddcfe7890d757b60160536c44bd5d946b802b21b"
    "src/l04_frame_codec/m05_can_codec/net_wire_can_common.c|3119b58a52476a3e5a1b1f1db1112ac88b3ea00a042c17a4702d2708926e9c2f"
    "src/l04_frame_codec/m05_can_codec/net_wire_can_internal.h|8e53d2cee7b062eb15e32be9351398cf216f72daa6d03a5268ddee8f8fc8949a"
    "src/l04_frame_codec/m05_can_codec/net_wire_can_nodes_raw.c|1c95270f5d54b8ca55063252535c5087ae3ddd0eaee5c445096f7cf5096a52aa"
    "src/l04_frame_codec/m06_uart_codec/net_uart_stream.c|b86374da663ed2f924ad55df615acc0ba8b0ca60cffebf375736a98dc7ce7369"
    "src/l04_frame_codec/m06_uart_codec/net_wire_stream.c|202ee6f5f4a69b65c5e2906cc4531819af1615ac08e44d0901982f8d3a96ce5c"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart.c|648436cfe1609a0a38e65fdfe466c9cc278b57756d5cc5ff176a20d7e1c5c1b0"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_c_status.c|49c268171b84e5d33b4c85dd9e4ac1fe1775b2182ad6f9cfcf5202dc6040085a"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_dto.c|3c5dce27ec1136c97d97cd5efdd8525541fe8d640af2dad4fb4dbee70c9c3282"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_m_core.c|4708c8158de2b8d46c38bef396a894bc04156a911afed5fb5609c1bba4458a62"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_m_projection.c|9f1f7b800e6f213d661986e8120f1256cf9084ad51d94d56695ed62d5bcc4f83"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_typed_internal.c|05e1b1fda800bef1028889ab7e52843cc29a641e3eda9560c42426a9d8a83bbe"
    "src/l04_frame_codec/m06_uart_codec/net_wire_uart_typed_internal.h|07d6aa147c8bfcba4f0c6d6668fcc1b7ca9e02a7d6dcbbdad15954168010d658"
    "src/l04_frame_codec/net_wire_common.c|f21d34b74ae3ef4e0623fa43f4ff0e43d59d740d47bd949a3a1620252b11e5cb"
    "src/l04_frame_codec/net_wire_internal.h|670c758ea7917541a3ba2881048a6791b662940604b3cf0d6c4533186ffa3ef9"
    "src/l04_frame_codec/net_wire_registry.c|618098c6cde939e4d71cc4288bc3bac968e6d222465b8ba3982bf6b90724a685"
    "src/l05_transport_port/m07_buffer_outcome/net_buffer.c|c25d304e969f8de57908a56782ea924283abc77cdabfd405919bbfd2b5cc663f"
    "src/l05_transport_port/m07_buffer_outcome/net_buffer.h|9f5504721084404cc34c6aa38487ea8215e0de06da9edbb7245abbe69c291d95"
    "src/l05_transport_port/m08_device_port/net_port.c|b1e33e393e5dae8ad098f944dc69e85e16c5e69ad278dda56a235da2b14dbe34"
    "src/l05_transport_port/m09_observation/net_observation.c|2fbfac378678516e7cec00ef90f8cf184cf97de89133a7fa9d9b552efe821f5d"
    "src/l05_transport_port/m09_observation/net_observation.h|bb4c76e96f1f2e23991a08a962852d80d204d57d988bb68c5c437b6c7f1f06d9"
    "src/platform/host_mock/net_host_mock.c|4fd813ec1a994cabf1769704d0fc9db2f58a21cc28605c0e9ff5114ac6ba4b75"
    "src/platform/s32k144/net_s32k144_port.c|04b23a05e962c341b1299afa83d6ab4b5cbe04736dcc83964ceafbc48e9c299b"
    "src/platform/s32k144/net_s32k144_port.h|d81ece2880e883efb4146b3e1588d1b08be0e01c6f857d810940898161ffa08b"
    "src/platform/s32k344/net_s32k344_port.c|b7f520ba0c083099da377fbb087a085898e0f70c721345b2f118c7bdd7cdf486"
    "src/platform/s32k344/net_s32k344_port.h|81592016a9d98889ef1d1289a00028d55a543cf8f2a935d4f0822cfa35b177ac"
    "tests/host/check_unconfigured.cmake|0df31005548eed2559915f5432ca9c17a3902398c608e35cbad6d4a809f6a40d"
    "tests/host/fixtures/wave2b_structural_vectors.h|6fa21a2b61a0de02f43f8de39350b6067ea39578b25be004bd9bc3a12e6699c3"
    "tests/host/host_demo.c|35046eae72c7da27e37384c150dbc85e5c27decb959f8cc777cdd39bf476df59"
    "tests/host/test_codec.c|e237f5792ea85ee587fd7931f9c493f77f69611415e3d3a3bfe9eadd068f6560"
    "tests/host/test_platform.c|0e0cbbb17869df61b2f6b56aab7ddb048b63382c3985ae47bc169a6f544467e3"
    "tests/host/test_runtime.c|397d4fe051c1a16f10b750ceba806646166f2f75f21dfe00ecc5f6b7f8f93dc6"
    "tests/host/test_uart_random_stress.c|3333b0a6544f22fad5cc8a4d67311fdcf47a7c9b695c069b1d7bc63504e3fb57"
    "tests/host/test_wave1_verifier.py|430d406142327f6613c32b50a10323ed18d08c3a987253d922a4d828ad880a10"
    "tests/host/test_wire_can.c|c903db6f9cff89da073662f708de33cd30dd9fcd66d0286965e4ef459bb0bcb4"
    "tests/host/test_wire_can_bcm.c|b6afdd550cbd59e2f49d016f9515ee81271e09c34bd36017f38c1adf85499cb2"
    "tests/host/test_wire_can_coverage.c|dd84fac2dfd85a3a75adb2476b790641b097a8847307cb6ddc0ba4021894b87e"
    "tests/host/test_wire_can_nodes.c|d4e8df6f7c891062ac05331dca5912b2240ba4d58cdc2e09a62f228dc19c8cd1"
    "tests/host/test_wire_i1_can_vectors.c|2b0b0f2fe2f16a431a20872373fc7cc020751956b8c0ade329470969bc708083"
    "tests/host/test_wire_registry.c|edebce87dad095bcb48c9fb43f509cb4b115c7ab8aa14c26983da06ec039ceed"
    "tests/host/test_wire_stream.c|a7e586f1a3e51f23606c96324338c3f16017d95675d249de0772c0f706e01e48"
    "tests/host/test_wire_typed48.c|5218cda1d8d2ab12878969cd1c4cd2e81d683dc76b3e210147302e30604c9a09"
    "tests/host/test_wire_uart_c_status.c|cffe7b799a94ba7f42f40d965a8ad3a4289ad9ffde52bbb94f97d45eaf7a8e5c"
    "tests/host/test_wire_uart_dto.c|6ab9b6cffe8bd3c6ec3e68ff916cf9e7bb46a81dc4b0f1be880be73a56164cc5"
    "tests/host/test_wire_uart_m_core.c|9f86d06f40d755f6547a11c4f5377c3b303f40554cd8bde05eea305e9587887f"
    "tests/host/test_wire_uart_projection.c|810f0a391e563e5d434190e26fdc37fb511190d7360050f3ff626cc023f004c3"
    "tests/host/test_wire_uart_wave2b_stream.c|94b9240f7e4637fad31731bdf0067bd33122d2f3c65d5a11835a26a4fd9dab3b"
    "tools/fixtures/wave1_golden.json|4c979747887b76a0aed9a295309cb050650ea83da145b9d321569d8f475bd07e"
    "tools/verify_wave1_golden.py|dac67e808aa208f909d981891cf6382f6bb5a72f90ccbf1133daffb6fc8ae696"
)

if(NETWORK_CASE STREQUAL "optin_default_off")
    set(work "${NETWORK_PARENT_BINARY}/w3a/def")
elseif(NETWORK_CASE STREQUAL "optin_invalid_combinations")
    set(work "${NETWORK_PARENT_BINARY}/w3a/neg")
elseif(NETWORK_CASE STREQUAL "product_holds_unchanged")
    set(work "${NETWORK_PARENT_BINARY}/w3a/hold")
elseif(NETWORK_CASE STREQUAL "regression_names_and_instrumentation")
    set(work "${NETWORK_PARENT_BINARY}/w3a/reg")
else()
    message(FATAL_ERROR "Unknown Wave3A build gate ${NETWORK_CASE}")
endif()
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef gate_run_id)
set(work "${work}_${gate_run_id}")
file(MAKE_DIRECTORY "${work}")
file(WRITE "${work}/commands.log" "Wave3A ${NETWORK_CASE}; non-product test only\n")

if(NETWORK_CASE STREQUAL "optin_default_off")
    w3a_configure(off DEFAULT DEFAULT ON host PASS)
    w3a_ok(off_build "${CMAKE_COMMAND}" --build "${work}/off" --parallel 4)
    w3a_inventory("${work}/off" "${W3A_OFF_NAMES}" off)
    w3a_no_stub("${work}/off" off)
    w3a_configure(on ON DEFAULT ON host PASS)
    w3a_ok(on_build "${CMAKE_COMMAND}" --build "${work}/on" --parallel 4)
    w3a_inventory("${work}/on" "${W3A_ON_NAMES}" on)
    w3a_no_stub("${work}/on" on)
elseif(NETWORK_CASE STREQUAL "optin_invalid_combinations")
    w3a_configure(candidate_off OFF ON ON host "Wave3A requires NETWORK_BUILD_WIRE_CANDIDATE=ON")
    w3a_configure(testing_off ON ON OFF host "Wave3A requires BUILD_TESTING=ON")
    w3a_configure(s32k144 ON ON ON s32k144 NOT_CONFIGURED)
    w3a_configure(s32k344 ON ON ON s32k344 NOT_CONFIGURED)
    w3a_configure(unknown ON ON ON unknown NOT_CONFIGURED)
elseif(NETWORK_CASE STREQUAL "product_holds_unchanged")
    foreach(pair IN LISTS W3A_LOCKED_SOURCE)
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 path)
        list(GET parts 1 expected_sha)
        file(SHA256 "${NETWORK_SOURCE}/${path}" actual_sha)
        if(NOT actual_sha STREQUAL expected_sha)
            message(FATAL_ERROR "Locked source changed: ${path}")
        endif()
    endforeach()
    file(READ "${NETWORK_PARENT_BINARY}/wave3a_link_targets.txt" links)
    set(expected_links "network_common=\nnetwork_host_mock=network_common\nnetwork_wire_candidate=\ntest_wave3a_host_bridge=network_host_mock;network_wire_candidate\n")
    if(NOT links STREQUAL expected_links)
        message(FATAL_ERROR "Library dependencies changed: ${links}")
    endif()
    foreach(pair "common|${NETWORK_COMMON_LIBRARY}" "mock|${NETWORK_MOCK_LIBRARY}" "wire|${NETWORK_WIRE_LIBRARY}" "stub|${NETWORK_STUB_EXECUTABLE}")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 name)
        list(GET parts 1 path)
        w3a_ok("nm_${name}" "${NETWORK_NM}" -g --defined-only "${path}")
        if(last_output MATCHES "[ _](DomainIf_|NetRear_|CisApp_|HAL_|LPUART_|FlexCAN_|INT_SYS_|GPIO_|xTask)")
            message(FATAL_ERROR "Forbidden product/HAL execution symbol in ${name}")
        endif()
        if(name STREQUAL "stub")
            if(NOT last_output MATCHES "net_wave3a_test_rx" OR NOT last_output MATCHES "net_wave3a_test_tx")
                message(FATAL_ERROR "Stub fixture entry points missing")
            endif()
        elseif(last_output MATCHES "net_wave3a_test_")
            message(FATAL_ERROR "Stub symbol leaked into original library ${name}")
        endif()
    endforeach()
    w3a_configure(s32k144 ON ON ON s32k144 NOT_CONFIGURED)
    w3a_configure(s32k344 ON ON ON s32k344 NOT_CONFIGURED)
    message(STATUS "W3A_TRACE {\"test\":\"wave3a.${NETWORK_CASE}\",\"phase\":\"source_and_links\",\"unchanged_original_files\":81,\"original_library_links_unchanged\":true,\"product_gate\":\"HOLD\",\"physical_tests\":\"NOT_RUN\"}")
elseif(NETWORK_CASE STREQUAL "regression_names_and_instrumentation")
    foreach(mode OFF ON)
        string(TOLOWER "${mode}" tag)
        w3a_configure("${tag}" "${mode}" OFF ON host PASS)
        w3a_ok("${tag}_build" "${CMAKE_COMMAND}" --build "${work}/${tag}" --parallel 4)
        w3a_inventory("${work}/${tag}" "${W3A_${mode}_NAMES}" "${tag}")
        w3a_ok("${tag}_ctest" "${NETWORK_CTEST}" --test-dir "${work}/${tag}"
            --output-on-failure --output-junit "${work}/${tag}.xml")
        file(READ "${work}/${tag}.xml" junit)
        if(junit MATCHES "<(failure|error|skipped)[ >]" OR
           junit MATCHES "(failures|errors|skipped|disabled)=\"[1-9][0-9]*\"")
            message(FATAL_ERROR "${tag}: regression XML contains failure/error/skip")
        endif()
        string(REGEX MATCHALL "<testcase[ \t\r\n]" test_cases "${junit}")
        string(REGEX MATCHALL "status=\"run\"" run_cases "${junit}")
        list(LENGTH test_cases test_count)
        list(LENGTH run_cases run_count)
        list(LENGTH W3A_${mode}_NAMES expected_count)
        if(NOT test_count EQUAL expected_count OR NOT run_count EQUAL expected_count)
            message(FATAL_ERROR "${tag}: expected ${expected_count} actually executed tests")
        endif()
    endforeach()
    set(all_names ${W3A_ON_NAMES} ${W3A_NEW_NAMES})
    w3a_inventory("${NETWORK_PARENT_BINARY}" "${all_names}" optin)
    w3a_ok(compiler_version "${NETWORK_COMPILER}" --version)
    w3a_ok(ctest_version "${NETWORK_CTEST}" --version)
    message(STATUS "W3A_TRACE {\"test\":\"wave3a.${NETWORK_CASE}\",\"phase\":\"executed_regressions\",\"on_tests\":134,\"off_tests\":41,\"optin_registered\":154,\"clang\":\"NOT_RUN_BY_THIS_GATE\",\"sanitizers\":\"NOT_RUN_BY_THIS_GATE\"}")
endif()
