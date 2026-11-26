# The historical test name is retained. Its physical-HAL rejection contract is
# unchanged; SDK-free upper compilation is now additionally required to pass.
set(args -S "${NETWORK_SOURCE}" -G "${NETWORK_GENERATOR}"
    "-DCMAKE_C_COMPILER=${NETWORK_COMPILER}" "-DCMAKE_MAKE_PROGRAM=${NETWORK_MAKE}"
    "-DNETWORK_TARGET=${NETWORK_BOARD}" -DBUILD_TESTING=OFF)
if(NETWORK_BOARD STREQUAL "s32k144" OR NETWORK_BOARD STREQUAL "s32k344")
    if(NETWORK_BOARD STREQUAL "s32k144")
        set(profile CIS)
    else()
        set(profile DOMAIN)
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -B "${NETWORK_BINARY}/portable"
        "-DNETWORK_NODE_PROFILE=${profile}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${NETWORK_BINARY}/portable-configure.log" "${output}\n${error}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Portable ${NETWORK_BOARD} configure failed: ${output}\n${error}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${NETWORK_BINARY}/portable" --target portable_network
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${NETWORK_BINARY}/portable-build.log" "${output}\n${error}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Portable ${NETWORK_BOARD} build failed: ${output}\n${error}")
    endif()
    list(APPEND args -DNETWORK_BUILD_BOARD_HAL=ON)
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" ${args} -B "${NETWORK_BINARY}/physical"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
file(WRITE "${NETWORK_BINARY}/expected-rejection.log" "${output}\n${error}")
if(status EQUAL 0 OR NOT "${output}${error}" MATCHES "NOT_CONFIGURED")
    message(FATAL_ERROR "Expected explicit external HAL/unknown target rejection; rc=${status}\n${output}\n${error}")
endif()
message(STATUS "${NETWORK_BOARD}: portable core verified where applicable; physical HAL remains NOT_CONFIGURED")
