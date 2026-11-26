#ifndef NETWORK_PRIVATE_UART_TYPED_INTERNAL_H
#define NETWORK_PRIVATE_UART_TYPED_INTERNAL_H

#include "network/net_wire_uart.h"
#include "../net_wire_internal.h"

/* M06-only argument boundaries. Framing/CRC/registry remain in the original
 * UART codec. Callers parse to local DTOs and commit both outputs only after
 * their message validation succeeds. No source context or product admission. */
NetWireStatus nw_uart_typed_encode_args(NetWireProfile profile,
    const void *message, size_t size, NetWireUartFrame *out);
NetWireStatus nw_uart_typed_decode_payload(NetWireProfile profile,
    const NetWireUartFrame *frame, void *out, size_t size, uint16_t *sequence_out,
    NetWireLink link, NetWireDirection direction, uint8_t type,
    NetWireUartPayload *payload);

#endif
