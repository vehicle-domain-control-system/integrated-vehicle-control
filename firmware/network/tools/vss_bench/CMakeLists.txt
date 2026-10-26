# Host bench helper (vss_bench): builds CAN / UART-C frames with the wire
# codecs, prints S32K144 debug-console commands and decodes copied frames.
add_library(network_vss_bench STATIC vss_bench.c)
target_include_directories(network_vss_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(network_vss_bench PUBLIC network_wire_candidate)
network_warnings(network_vss_bench)

add_executable(vss_bench vss_bench_main.c)
target_link_libraries(vss_bench PRIVATE network_vss_bench)
network_warnings(vss_bench)

if(BUILD_TESTING)
    add_executable(test_bench_tool ${PROJECT_SOURCE_DIR}/tests/bench_tool/test_bench_tool.c)
    target_link_libraries(test_bench_tool PRIVATE network_vss_bench)
    network_warnings(test_bench_tool)
    foreach(case IN ITEMS can_event_roundtrip can_permission_roundtrip can_warning_roundtrip
            can_decode_status can_script can_field_errors uart_crc_check
            uart_vision_status_command uart_vision_result_command uart_zero_boot_seq
            uart_decode_types hex_parser
            t_can_named_format t_can_payload_le t_can_script_consistent t_can_roundtrip_named)
        add_test(NAME bench_tool.${case} COMMAND test_bench_tool ${case})
    endforeach()
    # The executable itself: a generated bundle and a usage failure.
    add_test(NAME bench_tool.cli_can_event COMMAND vss_bench can event --boot 0x42454E43 --tx 1)
    set_tests_properties(bench_tool.cli_can_event PROPERTIES
        PASS_REGULAR_EXPRESSION "can tx begin VSS_EVENT[\r\n]+can tx data 43 4E 45 42 01 00 00 00 .*can tx commit")
    add_test(NAME bench_tool.cli_usage_error COMMAND vss_bench uart vision-status --boot 0 --seq 1)
    set_tests_properties(bench_tool.cli_usage_error PROPERTIES WILL_FAIL TRUE)
    # The Pi stand-in of the CIS board test (tools/pi_bench/pi_bench.py, Python):
    # its UART-C bytes against this codec through vss_bench, both ways, its
    # damaged frames, receive stream, offline commands and a serial session on a
    # stand-in port (no hardware, no pyserial).
    foreach(case IN ITEMS pi_crc pi_payload_vs_codec pi_codec_decodes_tool pi_tool_decodes_codec
            pi_damaged_frames pi_stream pi_offline_cli pi_session)
        add_test(NAME bench_tool.${case}
            COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/tests/host/test_pi_bench.py ${case}
                    --vss-bench $<TARGET_FILE:vss_bench>)
        set_tests_properties(bench_tool.${case} PROPERTIES TIMEOUT 120)
    endforeach()
endif()
