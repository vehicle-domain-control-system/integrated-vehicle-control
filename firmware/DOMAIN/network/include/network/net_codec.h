#ifndef NETWORK_NET_CODEC_H
#define NETWORK_NET_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "network/net_types.h"

/* Scaffold capture bounds only. This does not validate a CAN22 message, DLC,
 * controller flags, receiver, product profile, or application fields. */
NetStatus net_can_validate_capture(const NetFrame *frame);

/* HOST TEST FIXTURE ONLY. This artificial format is deliberately independent
 * of the proposed UART-M23 / UART-C3 contracts, which remain HOLD:
 *   caller sync[2], payload length u16LE, payload, caller checksum u16LE.
 * The checksum covers the two length bytes and payload. No default sync,
 * checksum, payload bound, or timeout is provided. This is not a product API.
 * All objects are caller owned and used by one serialized consumer. Input,
 * output, and parser storage must not overlap. */
#define NET_UART_FIXTURE_OVERHEAD 6u

typedef uint16_t (*NetUartFixtureChecksum)(const uint8_t *bytes, size_t length);

typedef struct {
    uint8_t sync[2];
    size_t max_payload;
    uint64_t assembly_limit_ms;
    bool expires_at_equality;
    NetUartFixtureChecksum checksum;
} NetUartFixtureProfile;

typedef struct {
    const uint8_t *bytes;
    const uint64_t *captured_at_ms;
    size_t length;
    uint64_t now_ms;
    bool timer_check_permitted;
    bool timestamps_unambiguous;
} NetUartStreamInput;

typedef struct {
    NetLink link;
    NetUartFixtureProfile profile;
    bool configured;
    bool has_last_capture;
    uint64_t last_capture_ms;
    size_t count;
    uint8_t bytes[NET_FRAME_CAPACITY];
    uint64_t captured_at_ms[NET_FRAME_CAPACITY];
    uint32_t discarded_bytes;
    uint32_t malformed_lengths;
    uint32_t bad_checksums;
    uint32_t assembly_timeouts;
    uint32_t byte_losses;
} NetUartStream;

/* Failure leaves stream unchanged. A NULL profile means NOT_CONFIGURED. */
NetStatus net_uart_stream_init(NetUartStream *stream, NetLink link,
                               const NetUartFixtureProfile *profile);

/* Produces at most one stable, complete raw frame; out changes only on OK.
 * consumed counts input bytes copied, including on NO_WORK/HOLD. Call again
 * with the unconsumed suffix; zero-input calls can drain a retained suffix.
 * Complete-frame timeout uses original last-byte time, never worker now_ms.
 * Time must be unambiguous, monotonic uint64_t in one caller-defined clock.
 * Ambiguous or backward time returns HOLD rather than inventing an epoch. */
NetStatus net_uart_stream_next(NetUartStream *stream,
                               const NetUartStreamInput *input,
                               size_t *consumed, NetFrame *out);

/* Explicit caller-observed gap/reset invalidates partial bytes only. It
 * preserves diagnostic counters and cannot release any outstanding TX. */
NetStatus net_uart_stream_reset(NetUartStream *stream, bool bytes_lost);

/* Serializes only the artificial fixture, with a stable output on success. */
NetStatus net_uart_fixture_encode(const NetUartFixtureProfile *profile,
                                  NetLink link, const uint8_t *payload,
                                  size_t payload_length,
                                  uint64_t captured_at_ms, NetFrame *out);

#endif
