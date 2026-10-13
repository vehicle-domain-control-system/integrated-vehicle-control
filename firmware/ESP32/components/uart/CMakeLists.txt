idf_component_register(
    SRCS
        "uart_m_crc16.c"
        "uart_m_frame.c"
        "uart_m_parser.c"
        "uart_m_codec.c"
        "uart_m_transport_engine.c"
        "uart_m_transport_idf.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart esp_timer
)
