# Host integration harness: the real VSS firmware sources (boot sequence,
# super-loop, INPUT, STORE, playback, health, flow) linked with this Network
# node (VSS profile, mock RTD, application tick hooks = time base A of
# src/platform/rtd/net_board_cfg.h). Included by the top-level CMakeLists.txt
# only when NETWORK_VSS_FW_DIR is given together with NETWORK_NODE_PROFILE=VSS
# and NETWORK_BUILD_RTD_MOCK=ON.
#
# NETWORK_VSS_FW_DIR is the firmware/vss folder of a VSS worktree. Its files
# are only read and compiled into the build folder; nothing of VSS is copied
# into this repository.
if(NOT BUILD_TESTING)
    message(STATUS "vss_integ: BUILD_TESTING is OFF, harness not built")
    return()
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "vss_integ: needs GCC or Clang (VSS host flags, -Wl,--wrap)")
endif()

get_filename_component(_vss_dir "${NETWORK_VSS_FW_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${_vss_dir}")
    message(FATAL_ERROR "NETWORK_VSS_FW_DIR=${NETWORK_VSS_FW_DIR}: folder not found "
        "(expected the firmware/vss folder of a VSS worktree)")
endif()
foreach(_need src/net_if/vss_net_api.h src/app/vss_main.h tests/host/hal_mock.c tests/host/hal_mock.h)
    if(NOT EXISTS "${_vss_dir}/${_need}")
        message(FATAL_ERROR "NETWORK_VSS_FW_DIR=${NETWORK_VSS_FW_DIR}: ${_need} not found "
            "(expected the firmware/vss folder of a VSS worktree)")
    endif()
endforeach()
find_program(_vss_git git)
if(_vss_git)
    execute_process(COMMAND "${_vss_git}" -C "${_vss_dir}" rev-parse --short HEAD
        OUTPUT_VARIABLE _vss_head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _vss_rc)
    if(_vss_rc EQUAL 0)
        message(STATUS "vss_integ: VSS sources ${_vss_dir} at ${_vss_head}")
    endif()
endif()

get_filename_component(_net_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# ---- VSS library: as tools/host_test.py of the VSS firmware builds it ------
# src/**/*.c without the vendor-driver adapters (hal/s32k344), plus the HAL
# stand-in tests/host/hal_mock.c. Same flags as host_test.py CFLAGS, with the
# real Network instead of net_stub.c and the CAN input only (PRODUCT origin).
file(GLOB_RECURSE _vss_sources CONFIGURE_DEPENDS "${_vss_dir}/src/*.c")
list(FILTER _vss_sources EXCLUDE REGEX "/src/hal/s32k344/")
add_library(vss_fw_host STATIC ${_vss_sources} "${_vss_dir}/tests/host/hal_mock.c")
set_target_properties(vss_fw_host PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
set(_vss_cflags -funsigned-char -Wall -Wextra -Werror -Wstrict-prototypes -Wundef)
set(_vss_defs VSS_CFG_HOST_TEST=1 VSS_CFG_NET_STUB=0 VSS_CFG_TEST_INPUT=0)
target_compile_options(vss_fw_host PRIVATE ${_vss_cflags})
target_compile_definitions(vss_fw_host PRIVATE ${_vss_defs})

# ---- Network library: the board source list, time base A with VSS's clock --
# Same files as tools/s32ds_sources.txt (the list a board project copies) and
# the mock SDK. The CAN ISR reads HalTimer_GetTicks(); the main context
# converts with VssTime_TickToMs32() (VSS contract: received_at_ms on the VSS
# clock axis). Node configuration and the microsecond counter come from the
# harness. Plain char is unsigned, as on the target and in the VSS build.
set(_integ_net_sources ${_node_core_sources} ${_node_rtd_sources})
list(TRANSFORM _integ_net_sources PREPEND "${_net_root}/")
set(_integ_net_includes ${_node_includes})
list(TRANSFORM _integ_net_includes PREPEND "${_net_root}/")
add_library(network_node_vss_integ STATIC ${_integ_net_sources} ${_rtd_mock_sources})
target_include_directories(network_node_vss_integ PUBLIC ${_integ_net_includes} "${_net_root}/sdk_mock/rtd/include")
target_compile_definitions(network_node_vss_integ PUBLIC NETWORK_UPPER_NODE_PROFILE=${_network_profile_id})
target_compile_definitions(network_node_vss_integ PRIVATE
    NET_BOARD_ISR_TICK_HOOK=HalTimer_GetTicks
    NET_BOARD_TICK_TO_MS_HOOK=VssTime_TickToMs32
    NET_BOARD_NODE_CONFIG_HOOK=VssInteg_NodeConfig
    NET_BOARD_US_HOOK=VssInteg_Us)
target_compile_options(network_node_vss_integ PRIVATE -funsigned-char)
network_warnings(network_node_vss_integ)

# The two libraries call each other (VSS: Net_Init, Net_MainFunction,
# VssNet_PublishStatus; Network: VssApp_On*, HalTimer_GetTicks,
# VssTime_TickToMs32): a static library cycle, resolved by CMake.
target_link_libraries(network_node_vss_integ INTERFACE vss_fw_host)
target_link_libraries(vss_fw_host INTERFACE network_node_vss_integ)

# ---- Test program: two halves that never share a translation unit ----------
# vss_net_api.h (VSS) and network/app/net_if_*.h (Network) declare the same
# application types, so the VSS half sees only VSS headers and the bus half
# (DOMAIN frames, bus, Network diagnostics) only Network headers. They meet in
# vss_integ_bus.h, which uses plain C types.
add_library(vss_integ_vss_half OBJECT test_vss_integ.c)
set_target_properties(vss_integ_vss_half PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(vss_integ_vss_half PRIVATE ${_vss_cflags})
target_compile_definitions(vss_integ_vss_half PRIVATE ${_vss_defs})
target_include_directories(vss_integ_vss_half PRIVATE "${_vss_dir}/src" "${_vss_dir}/tests/host")

add_library(vss_integ_bus_half OBJECT vss_integ_bus.c)
target_link_libraries(vss_integ_bus_half PRIVATE network_node_vss_integ)
target_compile_options(vss_integ_bus_half PRIVATE -funsigned-char)
network_warnings(vss_integ_bus_half)

add_executable(test_vss_integ $<TARGET_OBJECTS:vss_integ_vss_half> $<TARGET_OBJECTS:vss_integ_bus_half>)
target_link_libraries(test_vss_integ PRIVATE network_node_vss_integ vss_fw_host)
# Link spies on the Network <-> VSS calls: they record, then call the real
# function (__real_*), so both sides run unchanged.
target_link_options(test_vss_integ PRIVATE
    -Wl,--wrap=VssApp_OnDomainEvent
    -Wl,--wrap=VssApp_OnDomainWarningState
    -Wl,--wrap=VssNet_PublishStatus)

# One case per process: Net_Init acts once per process, VSS boots once.
# Cases follow "VSS 연동 확인 시험" (VSS_NODE_INTEGRATION.md) and the VSS
# contracts C1..C12 (VSS_NODE_INTEGRATION.md).
foreach(case IN ITEMS vi1_boot_order vi2_clock_axis vi3_tx_per_message vi4_domain_reboot
        vi5_same_occurrence vi6_structure_rejected vi7_warning_rx_loss vi8_status_period
        vi9_link_locked_resubmit vi10_rx_flood_budget vi11_warning_quality_drop
        vi12_warning_repeat_keeps_vss)
    add_test(NAME vss_integ.${case} COMMAND test_vss_integ ${case})
endforeach()
