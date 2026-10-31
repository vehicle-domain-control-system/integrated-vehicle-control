#include "pi_uart.h"
#include "regs_s32k144.h"

/* 115200 baud from the 8 MHz SOSCDIV2 clock: baud = 8 MHz / ((OSR + 1) * SBR)
 * OSR = 22, SBR = 3 -> 8 MHz / 69 = 115942 baud (+0.64 %). */
#define BAUD_VALUE      ((22u << 24) | 3u)

static pi_parser_t s_parser;
static uint32_t s_overruns;
static uint32_t s_line_errors;

/* Transmit queue: pi_uart_send() only copies the frame here and pi_uart_poll() hands the bytes
 * to the UART whenever it is ready, so a long frame never keeps the loop from reading the
 * 4-byte receive FIFO. A frame is queued completely or not at all. */
#define TX_QUEUE_SIZE   256u                        /* power of two */
static uint8_t  s_tx_buf[TX_QUEUE_SIZE];
static uint32_t s_tx_head;                          /* next byte to write */
static uint32_t s_tx_tail;                          /* next byte to send */
static uint32_t s_tx_dropped;

void pi_uart_init(void)
{
    PCC_PORTC |= PCC_CGC;
    PORTC_PCR(6) = PCR_MUX(2);                      /* LPUART1_RX */
    PORTC_PCR(7) = PCR_MUX(2);                      /* LPUART1_TX */

    PCC_LPUART1 &= ~PCC_CGC;
    PCC_LPUART1 = PCC_PCS(1) | PCC_CGC;             /* functional clock: SOSCDIV2 (8 MHz) */

    LPUART1_CTRL = 0u;                              /* receiver and transmitter off while configuring */
    LPUART1_BAUD = BAUD_VALUE;                      /* one stop bit, 8 data bits */
    LPUART1_FIFO = LPUART_FIFO_RXFE | LPUART_FIFO_RXFLUSH | LPUART_FIFO_TXFLUSH;
    LPUART1_STAT = LPUART_STAT_ERRORS;              /* clear old error flags (write 1) */
    LPUART1_CTRL = LPUART_CTRL_RE | LPUART_CTRL_TE; /* no parity, 8 bits */

    pi_parser_init(&s_parser);
    s_overruns = 0;
    s_line_errors = 0;
    s_tx_head = 0;
    s_tx_tail = 0;
    s_tx_dropped = 0;
}

/* Hands queued bytes to the transmitter while it can take one. */
static void tx_service(void)
{
    while (s_tx_tail != s_tx_head && (LPUART1_STAT & LPUART_STAT_TDRE)) {
        LPUART1_DATA = s_tx_buf[s_tx_tail & (TX_QUEUE_SIZE - 1u)];
        s_tx_tail++;
    }
}

bool pi_uart_poll(pi_frame_t *frame, uint32_t now_ms)
{
    tx_service();
    pi_parser_expire(&s_parser, now_ms);              /* drop a frame that is too late */
    for (;;) {
        uint32_t stat;

        if (pi_parser_step(&s_parser, frame)) {
            return true;
        }

        stat = LPUART1_STAT;
        if (stat & LPUART_STAT_OR) {
            s_overruns++;
            LPUART1_STAT = LPUART_STAT_OR;          /* clear (write 1); bytes were lost */
        }
        if (stat & (LPUART_STAT_FE | LPUART_STAT_NF | LPUART_STAT_PF)) {
            s_line_errors++;
            LPUART1_STAT = stat & (LPUART_STAT_FE | LPUART_STAT_NF | LPUART_STAT_PF);
        }
        if (!(stat & LPUART_STAT_RDRF)) {
            tx_service();
            return false;                           /* nothing more received */
        }
        pi_parser_push(&s_parser, (uint8_t)(LPUART1_DATA & 0xFFu), now_ms);
        tx_service();
    }
}

void pi_uart_send(const uint8_t *data, size_t len)
{
    size_t i;

    if (len > TX_QUEUE_SIZE - (s_tx_head - s_tx_tail)) {
        s_tx_dropped++;                             /* no room: the whole frame is dropped */
        return;
    }
    for (i = 0; i < len; i++) {
        s_tx_buf[s_tx_head & (TX_QUEUE_SIZE - 1u)] = data[i];
        s_tx_head++;
    }
    tx_service();
}

uint32_t pi_uart_frames_ok(void)      { return s_parser.frames_ok; }
uint32_t pi_uart_bad_crc(void)        { return s_parser.bad_crc; }
uint32_t pi_uart_bad_format(void)     { return s_parser.bad_format; }
uint32_t pi_uart_assembly_timeouts(void) { return s_parser.assembly_timeouts; }
uint32_t pi_uart_skipped_bytes(void)  { return s_parser.skipped_bytes; }
uint32_t pi_uart_overruns(void)       { return s_overruns; }
uint32_t pi_uart_line_errors(void)    { return s_line_errors; }
uint32_t pi_uart_tx_dropped(void)     { return s_tx_dropped; }
