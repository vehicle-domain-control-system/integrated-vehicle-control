#ifndef NETWORK_NET_WIRE_UART_C_STATUS_H
#define NETWORK_NET_WIRE_UART_C_STATUS_H

#include "network/net_wire_uart.h"

/* M06 Host raw candidate, not Pi/CIS ABI or runtime adoption. Wire values are
 * preserved without evaluating permission, timeouts or previous vision data.
 * Current ACTIVE_FAULT and LAST_FAULT history are independent. Struct padding
 * is not wire data. All input/output ranges must be disjoint; every output is
 * unchanged on failure, including the independent link sequence output. */
typedef struct {
    uint32_t pi_boot_id, status_sequence;
    uint8_t vision_status;
    uint16_t active_fault, last_fault, status_age_ms;
    uint8_t affected_function;
} NetWirePVisionStatusRaw;

NetWireStatus net_wire_p_vision_status_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePVisionStatusRaw *message,
    NetWireUartFrame *out);
NetWireStatus net_wire_p_vision_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePVisionStatusRaw *out,
    uint16_t *sequence_out);

#endif
