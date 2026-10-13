cmake_minimum_required(VERSION 3.20)
project(uart_m_transport_tests C)

add_executable(uart_m_transport_tests
    test_uart_m_transport.c
    mock_idf.c
    ../../components/uart/uart_m_crc16.c
    ../../components/uart/uart_m_frame.c
    ../../components/uart/uart_m_parser.c
    ../../components/uart/uart_m_codec.c
    ../../components/uart/uart_m_transport_engine.c
    ../../components/uart/uart_m_transport_idf.c
)

target_include_directories(uart_m_transport_tests PRIVATE
    mocks
    ../../components/uart/include
)
target_compile_features(uart_m_transport_tests PRIVATE c_std_11)
if(MSVC)
    target_compile_options(uart_m_transport_tests PRIVATE /W4 /WX)
else()
    target_compile_options(uart_m_transport_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME uart_m_transport_tests COMMAND uart_m_transport_tests)
