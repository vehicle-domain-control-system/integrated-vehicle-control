#ifndef NETWORK_PRIVATE_WIRE_INTERNAL_H
#define NETWORK_PRIVATE_WIRE_INTERNAL_H
#include "network/net_wire_uart.h"
/* Pure preflight shared by in-place stream/receiver/service initialization. */
NetWireStatus nw_uart_stream_config_check(NetWireProfile profile,
    NetWireLink link, NetWireDirection direction, const NetWireStreamPolicy *policy);
uint16_t nw_read_u16(const uint8_t *p);
int16_t nw_read_i16(const uint8_t *p);
uint32_t nw_read_u32(const uint8_t *p);
uint64_t nw_read_u64(const uint8_t *p);
void nw_write_u16(uint8_t *p, uint16_t v);
void nw_write_i16(uint8_t *p, int16_t v);
void nw_write_u32(uint8_t *p, uint32_t v);
void nw_write_u64(uint8_t *p, uint64_t v);
void nw_sat_inc(uint32_t *counter);
NetWireStatus nw_uart_header(NetWireLink link, NetWireDirection direction,
    const uint8_t *bytes, const NetWireRegistryEntry **entry);
/* net_wire_uart_stream_next for a caller whose arguments are all its own,
 * disjoint storage (M04 receiver): same result, without the range checks. */
NetWireStatus nw_uart_stream_next_owned(NetWireUartStream *stream,
    const NetWireStreamInput *input, size_t *consumed, NetWireUartFrame *out);
#endif
