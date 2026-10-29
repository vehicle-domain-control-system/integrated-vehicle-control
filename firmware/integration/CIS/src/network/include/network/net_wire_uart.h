#ifndef NETWORK_NET_WIRE_UART_H
#define NETWORK_NET_WIRE_UART_H
#include "network/net_wire.h"
#define NET_WIRE_UART_PAYLOAD_MAX 128u
#define NET_WIRE_UART_FRAME_MAX 138u
#define NET_WIRE_STREAM_INPUT_MAX 512u
typedef struct {
    size_t length;
    uint64_t captured_at_ms;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
} NetWireUartFrame;
typedef struct {
    uint8_t type;
    uint16_t sequence;
    size_t length;
    uint64_t captured_at_ms;
    uint8_t bytes[NET_WIRE_UART_PAYLOAD_MAX];
} NetWireUartPayload;
/* CCITT-FALSE: poly1021 initFFFF no reflection xorout0. Result is a number;
   frame stores it little-endian. Bounded to 138 bytes per call; no output on
   invalid arguments/overlap. */
NetWireStatus net_wire_crc16(const uint8_t *bytes, size_t length, uint16_t *out);
/* Header/registry/CRC only. Typed validation is a separate operation.
   Reject overlapping input/output storage. Failure leaves outputs untouched. */
NetWireStatus net_wire_uart_encode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, NetWireUartFrame *out);
NetWireStatus net_wire_uart_decode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, const NetWireUartFrame *frame, NetWireUartPayload *out);

typedef struct {
    /* 50ms is a proposed bound only. Equality behavior remains explicit. */
    bool candidate_time_policy_selected;
    uint64_t assembly_limit_ms;
    bool expires_at_equality;
} NetWireStreamPolicy;
typedef struct {
    const uint8_t *bytes;
    const uint64_t *captured_at_ms;
    size_t length;
    uint64_t now_ms;
    bool timestamps_unambiguous;
    bool timer_check_permitted;
} NetWireStreamInput;
typedef struct {
    NetWireProfile profile;
    NetWireLink link;
    NetWireDirection direction;
    NetWireStreamPolicy policy;
    bool configured, has_last_capture, has_last_check;
    uint64_t last_capture_ms, last_check_ms;
    size_t count;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    uint64_t captured_at_ms[NET_WIRE_UART_FRAME_MAX];
    uint32_t discarded_bytes, bad_headers, bad_crc, assembly_timeouts, byte_losses;
} NetWireUartStream;
NetWireStatus net_wire_uart_stream_init(NetWireUartStream *stream,
    NetWireProfile profile, NetWireLink link, NetWireDirection direction,
    const NetWireStreamPolicy *policy);
/* Single-owner, outside ISR. <=512 input bytes per call. OK yields one complete
   structural frame; NEED_MORE commits consumed data/discard diagnostics but
   leaves out untouched. Other failures leave stream/consumed/out untouched.
   Original per-byte times survive suffix resync; timer permission means no
   earlier queued RX remains. All mutable outputs and input ranges must be
   disjoint. Profile/time policy do not authorize product consumption. */
NetWireStatus net_wire_uart_stream_next(NetWireUartStream *stream,
    const NetWireStreamInput *input, size_t *consumed, NetWireUartFrame *out);
/* Explicit observed link gap: invalidates partial data, keeps counters and
   monotonic time watermarks. Clock epoch changes need explicit re-init. */
NetWireStatus net_wire_uart_stream_reset(NetWireUartStream *stream, bool byte_loss);
#endif
