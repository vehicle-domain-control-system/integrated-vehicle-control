cmake_minimum_required(VERSION 3.16)

project(uart_m_codec_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(uart_m_codec_tests
    test_uart_m_codec.c
    ../../components/uart/uart_m_crc16.c
    ../../components/uart/uart_m_frame.c
    ../../components/uart/uart_m_codec.c
)

target_include_directories(uart_m_codec_tests PRIVATE
    ../../components/uart/include
)

if(MSVC)
    target_compile_options(uart_m_codec_tests PRIVATE /W4 /WX)
else()
    target_compile_options(uart_m_codec_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME uart_m_codec_tests COMMAND uart_m_codec_tests)
