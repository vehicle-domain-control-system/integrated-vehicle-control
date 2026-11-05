#ifndef UART_M_CRC16_H
#define UART_M_CRC16_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UART_M_CRC16_CCITT_FALSE_INIT UINT16_C(0xFFFF)
#define UART_M_CRC16_CCITT_FALSE_POLY UINT16_C(0x1021)

uint16_t uart_m_crc16_ccitt_false(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
