# Explicit read-only external Owner source integration; never vendors or alters it.
set(NETWORK_DOMAIN_C2_OWNER_SOURCE_DIR "" CACHE PATH "Pinned DOMAIN src directory for real Owner host integration")
if(NETWORK_DOMAIN_C2_OWNER_SOURCE_DIR AND BUILD_TESTING AND NETWORK_NODE_PROFILE STREQUAL "HOST")
    if(NOT TARGET network_domain_c2)
        message(FATAL_ERROR "Pinned Owner integration requires network_domain_c2")
    endif()
    set(_domain_owner "${NETWORK_DOMAIN_C2_OWNER_SOURCE_DIR}")
    add_executable(test_domain_pinned_owner
        tests/product/test_domain_pinned_owner.c
        "${_domain_owner}/interface/domain_interface_core.c"
        "${_domain_owner}/application/vehicle_state_manager.c"
        "${_domain_owner}/application/domain_interface_state_binding.c"
        "${_domain_owner}/application/domain_interface_query_binding.c"
        "${_domain_owner}/application/result_manager.c"
        "${_domain_owner}/application/domain_interface_result_binding.c"
        "${_domain_owner}/application/cis_environment_manager.c"
        "${_domain_owner}/application/domain_cis_environment_private.c"
        "${_domain_owner}/application/domain_interface_cis_environment_binding.c")
    # The pinned Owner API binds RxCisOccupant / QueryGetCisOccupant as well.
    target_sources(test_domain_pinned_owner PRIVATE
        "${_domain_owner}/application/cis_occupant_manager.c"
        "${_domain_owner}/application/domain_cis_occupant_private.c"
        "${_domain_owner}/application/domain_interface_cis_occupant_binding.c")
    target_include_directories(test_domain_pinned_owner PRIVATE
        "${_domain_owner}/application" "${_domain_owner}/interface")
    target_link_libraries(test_domain_pinned_owner PRIVATE network_domain_c2)
    network_warnings(test_domain_pinned_owner)
    add_test(NAME domain_c2_actual_pinned_owner COMMAND test_domain_pinned_owner)
    # CIS environment / Occupant transitions through the actual DOMAIN runtime:
    # the test-only binding calls the DOMAIN private BOOT transitions.
    add_executable(test_domain_transition_owner
        tests/product/test_domain_transition_owner.c
        tests/product/domain_transition_binding.c
        "${_domain_owner}/interface/domain_interface_core.c"
        "${_domain_owner}/application/domain_runtime.c"
        "${_domain_owner}/application/vehicle_state_manager.c"
        "${_domain_owner}/application/command_manager.c"
        "${_domain_owner}/application/result_manager.c"
        "${_domain_owner}/application/permission_manager.c"
        "${_domain_owner}/application/cis_occupant_manager.c"
        "${_domain_owner}/application/domain_cis_occupant_private.c"
        "${_domain_owner}/application/cis_environment_manager.c"
        "${_domain_owner}/application/domain_cis_environment_private.c"
        "${_domain_owner}/application/domain_interface_state_binding.c"
        "${_domain_owner}/application/domain_interface_result_binding.c"
        "${_domain_owner}/application/domain_interface_query_binding.c"
        "${_domain_owner}/application/domain_interface_cis_occupant_binding.c"
        "${_domain_owner}/application/domain_interface_cis_environment_binding.c")
    target_include_directories(test_domain_transition_owner PRIVATE
        "${_domain_owner}/application" "${_domain_owner}/interface")
    target_link_libraries(test_domain_transition_owner PRIVATE network_domain_c2)
    network_warnings(test_domain_transition_owner)
    foreach(case IN ITEMS env_transition occupant_transition)
        add_test(NAME domain_c2_actual_${case} COMMAND test_domain_transition_owner ${case})
    endforeach()
endif()
