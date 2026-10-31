/*
 * UART link to the Raspberry Pi through the OpenSDA USB virtual COM port.
 *   LPUART1: PTC6 = RX, PTC7 = TX (the board connects them to the OpenSDA USB, J7)
 *   115200 baud, 8N1. Needs timebase_init() first (SOSCDIV2, 8 MHz, clocks LPUART1).
 *
 * Frames: UART-C v1 (see pi_protocol.h).
 * Reception is polled and uses the 4-byte receive FIFO: keep calling
 * pi_uart_poll() often. Long blocking jobs (DHT22, I2C) can let the FIFO
 * overflow; a damaged frame is dropped by the CRC check and the Pi repeats its
 * result every 100 ms, so a lost frame only costs one repetition.
 */
#ifndef PI_UART_H
#define PI_UART_H

#include "pi_protocol.h"

void pi_uart_init(void);

/* Reads the received bytes. Returns true and fills `frame` when a complete
 * valid frame is available; call again until it returns false.
 * now_ms is the current time (the 50 ms frame assembly limit uses it). */
bool pi_uart_poll(pi_frame_t *frame, uint32_t now_ms);

/* Queues a frame (whole or not at all, up to 256 bytes). The bytes go out inside pi_uart_poll(),
 * about 87 us per byte, so call that often. */
void pi_uart_send(const uint8_t *data, size_t len);

/* Counters for the debugger */
uint32_t pi_uart_frames_ok(void);
uint32_t pi_uart_bad_crc(void);
uint32_t pi_uart_bad_format(void);
uint32_t pi_uart_assembly_timeouts(void);
uint32_t pi_uart_skipped_bytes(void);
uint32_t pi_uart_overruns(void);        /* receiver overruns: bytes lost */
uint32_t pi_uart_line_errors(void);     /* framing / noise / parity errors */
uint32_t pi_uart_tx_dropped(void);      /* frames dropped because the transmit queue was full */

#endif /* PI_UART_H */
