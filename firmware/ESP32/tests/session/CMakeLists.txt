cmake_minimum_required(VERSION 3.16)

project(p6_session_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(p6_session_tests
    test_p6_session_manager.c
    mock_registration_store.c
    mock_entropy.c
    mock_monotonic_clock.c
    ../../components/session/p6_session_manager.c
    ../../components/session/p6_authorization_provider.c
)

target_include_directories(p6_session_tests PRIVATE
    .
    ../../components/session/include
)

if(MSVC)
    target_compile_options(p6_session_tests PRIVATE /W4 /WX)
else()
    target_compile_options(p6_session_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME p6_session_tests COMMAND p6_session_tests)

add_executable(p6_ble_lifecycle_tests
    test_p6_ble_lifecycle.c
    mock_registration_store.c
    mock_entropy.c
    mock_monotonic_clock.c
    ../../components/session/p6_session_manager.c
    ../../components/session/p6_ble_lifecycle.c
)

target_include_directories(p6_ble_lifecycle_tests PRIVATE
    .
    ../../components/session/include
)

if(MSVC)
    target_compile_options(p6_ble_lifecycle_tests PRIVATE /W4 /WX)
else()
    target_compile_options(p6_ble_lifecycle_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

add_test(NAME p6_ble_lifecycle_tests COMMAND p6_ble_lifecycle_tests)

add_executable(p6_nvs_registration_store_tests
    test_p6_nvs_registration_store.c
    mock_nvs.c
    ../../components/session/p6_session_manager.c
    ../../components/session/p6_registration_record_codec.c
    ../../components/session/p6_nvs_registration_store.c
)

target_include_directories(p6_nvs_registration_store_tests PRIVATE
    mocks
    .
    ../../components/session/include
)

if(MSVC)
    target_compile_options(p6_nvs_registration_store_tests PRIVATE /W4 /WX)
else()
    target_compile_options(p6_nvs_registration_store_tests PRIVATE
        -Wall -Wextra -Werror -pedantic)
endif()

add_test(NAME p6_nvs_registration_store_tests
         COMMAND p6_nvs_registration_store_tests)

add_executable(p6_bond_reset_coordinator_tests
    test_p6_bond_reset_coordinator.c
    mock_registration_store.c
    mock_entropy.c
    mock_monotonic_clock.c
    ../../components/session/p6_session_manager.c
    ../../components/session/p6_bond_reset_coordinator.c
)

target_include_directories(p6_bond_reset_coordinator_tests PRIVATE
    .
    ../../components/session/include
)

if(MSVC)
    target_compile_options(p6_bond_reset_coordinator_tests PRIVATE /W4 /WX)
else()
    target_compile_options(p6_bond_reset_coordinator_tests PRIVATE
        -Wall -Wextra -Werror -pedantic)
endif()

add_test(NAME p6_bond_reset_coordinator_tests
         COMMAND p6_bond_reset_coordinator_tests)
