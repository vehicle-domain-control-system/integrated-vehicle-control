# Prints what tools/s32ds_sources.cmake selects from one source list for one
# node profile, so tests/host/test_node_bundle.py can compare the CMake reader
# with tools/source_list.py (the reader of the Python tools).
#   cmake -DNETWORK_LIST=<list file> -DNETWORK_ROOT=<firmware/network copy>
#         -DNETWORK_NODE=<VSS|CIS|BCM|DOMAIN> -P node_bundle_list_probe.cmake
# Output lines (stderr): "SELECTED <path>" and "COMMON <path>", in list order.
# A malformed list stops with FATAL_ERROR (non-zero exit).
cmake_minimum_required(VERSION 3.16)
include(${CMAKE_CURRENT_LIST_DIR}/../../tools/s32ds_sources.cmake)
network_read_source_list("${NETWORK_LIST}" "${NETWORK_ROOT}" "${NETWORK_NODE}" _selected _common)
foreach(_path IN LISTS _selected)
    message("SELECTED ${_path}")
endforeach()
foreach(_path IN LISTS _common)
    message("COMMON ${_path}")
endforeach()
