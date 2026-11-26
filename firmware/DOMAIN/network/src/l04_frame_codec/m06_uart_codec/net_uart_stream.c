#include "network/net_codec.h"

#include <string.h>

static bool uart_link(NetLink link)
{
    return link == NET_LINK_UART_M || link == NET_LINK_UART_C;
}

static NetStatus validate_profile(const NetUartFixtureProfile *profile)
{
    if (profile == NULL) {
        return NET_NOT_CONFIGURED;
    }
    if (profile->max_payload > NET_FRAME_CAPACITY - NET_UART_FIXTURE_OVERHEAD ||
        profile->assembly_limit_ms == 0u || profile->checksum == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    return NET_OK;
}

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static void write_le16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
}

static void increment(uint32_t *counter)
{
    if (*counter != UINT32_MAX) {
        ++*counter;
    }
}

/* Keep each suffix byte paired with its original capture time. */
static void discard_prefix(NetUartStream *stream, size_t count, bool noise)
{
    size_t remain = stream->count - count;
    size_t i;
    for (i = 0u; i < remain; ++i) {
        stream->bytes[i] = stream->bytes[i + count];
        stream->captured_at_ms[i] = stream->captured_at_ms[i + count];
    }
    stream->count = remain;
    if (noise) {
        for (i = 0u; i < count; ++i) {
            increment(&stream->discarded_bytes);
        }
    }
}

NetStatus net_uart_stream_init(NetUartStream *stream, NetLink link,
                               const NetUartFixtureProfile *profile)
{
    NetUartStream temporary = {0};
    NetStatus status;
    if (stream == NULL || !uart_link(link)) {
        return NET_INVALID_ARGUMENT;
    }
    status = validate_profile(profile);
    if (status != NET_OK) {
        return status;
    }
    temporary.link = link;
    temporary.profile = *profile;
    temporary.configured = true;
    *stream = temporary;
    return NET_OK;
}

NetStatus net_uart_stream_next(NetUartStream *stream,
                               const NetUartStreamInput *input,
                               size_t *consumed, NetFrame *out)
{
    if (stream == NULL || input == NULL || consumed == NULL || out == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    *consumed = 0u;
    if (input->length != 0u &&
        (input->bytes == NULL || input->captured_at_ms == NULL)) {
        return NET_INVALID_ARGUMENT;
    }
    if (!stream->configured) {
        return NET_NOT_CONFIGURED;
    }
    if (!uart_link(stream->link) || validate_profile(&stream->profile) != NET_OK ||
        stream->count > NET_FRAME_CAPACITY) {
        return NET_INVALID_ARGUMENT;
    }
    if (!input->timestamps_unambiguous) {
        return NET_HOLD;
    }

    for (;;) {
        size_t wanted = 0u;
        bool complete;
        if (stream->count >= 1u && stream->bytes[0] != stream->profile.sync[0]) {
            discard_prefix(stream, 1u, true);
            continue;
        }
        if (stream->count >= 2u && stream->bytes[1] != stream->profile.sync[1]) {
            discard_prefix(stream, 1u, true);
            continue;
        }
        if (stream->count >= 4u) {
            size_t payload_length = read_le16(&stream->bytes[2]);
            if (payload_length > stream->profile.max_payload) {
                increment(&stream->malformed_lengths);
                discard_prefix(stream, 1u, true);
                continue;
            }
            wanted = payload_length + NET_UART_FIXTURE_OVERHEAD;
        }
        complete = wanted != 0u && stream->count >= wanted;
        if (stream->count != 0u) {
            uint64_t at = stream->captured_at_ms[complete ? wanted - 1u : stream->count - 1u];
            uint64_t elapsed;
            bool expired;
            /* Timer expiry is only permitted after queued input is drained. */
            if (!complete && *consumed == input->length && input->timer_check_permitted) {
                if (input->now_ms < at) {
                    return NET_HOLD;
                }
                at = input->now_ms;
            }
            if (at < stream->captured_at_ms[0]) {
                return NET_HOLD;
            }
            elapsed = at - stream->captured_at_ms[0];
            expired = stream->profile.expires_at_equality
                          ? elapsed >= stream->profile.assembly_limit_ms
                          : elapsed > stream->profile.assembly_limit_ms;
            if (expired) {
                increment(&stream->assembly_timeouts);
                discard_prefix(stream, 1u, true);
                continue;
            }
        }
        if (complete) {
            uint16_t checksum = stream->profile.checksum(&stream->bytes[2], wanted - 4u);
            NetFrame temporary = {0};
            if (checksum != read_le16(&stream->bytes[wanted - 2u])) {
                increment(&stream->bad_checksums);
                discard_prefix(stream, 1u, true);
                continue;
            }
            temporary.link = stream->link;
            temporary.length = wanted;
            temporary.captured_at_ms = stream->captured_at_ms[wanted - 1u];
            memcpy(temporary.bytes, stream->bytes, wanted);
            *out = temporary;
            discard_prefix(stream, wanted, false);
            return NET_OK;
        }
        if (*consumed == input->length) {
            return NET_NO_WORK;
        }
        if (stream->count == NET_FRAME_CAPACITY) {
            return NET_NO_CAPACITY;
        }
        if (stream->has_last_capture &&
            input->captured_at_ms[*consumed] < stream->last_capture_ms) {
            return NET_HOLD;
        }
        stream->bytes[stream->count] = input->bytes[*consumed];
        stream->captured_at_ms[stream->count] = input->captured_at_ms[*consumed];
        stream->last_capture_ms = input->captured_at_ms[*consumed];
        stream->has_last_capture = true;
        ++stream->count;
        ++*consumed;
    }
}

NetStatus net_uart_stream_reset(NetUartStream *stream, bool bytes_lost)
{
    if (stream == NULL) {
        return NET_INVALID_ARGUMENT;
    }
    if (!stream->configured) {
        return NET_NOT_CONFIGURED;
    }
    stream->count = 0u;
    stream->has_last_capture = false;
    stream->last_capture_ms = 0u;
    if (bytes_lost) {
        increment(&stream->byte_losses);
    }
    return NET_OK;
}

NetStatus net_uart_fixture_encode(const NetUartFixtureProfile *profile,
                                  NetLink link, const uint8_t *payload,
                                  size_t payload_length,
                                  uint64_t captured_at_ms, NetFrame *out)
{
    NetFrame temporary = {0};
    NetStatus status;
    if (out == NULL || !uart_link(link) || (payload_length != 0u && payload == NULL)) {
        return NET_INVALID_ARGUMENT;
    }
    status = validate_profile(profile);
    if (status != NET_OK) {
        return status;
    }
    if (payload_length > profile->max_payload) {
        return NET_REJECTED;
    }
    temporary.link = link;
    temporary.length = payload_length + NET_UART_FIXTURE_OVERHEAD;
    temporary.captured_at_ms = captured_at_ms;
    temporary.bytes[0] = profile->sync[0];
    temporary.bytes[1] = profile->sync[1];
    write_le16(&temporary.bytes[2], (uint16_t)payload_length);
    if (payload_length != 0u) {
        memcpy(&temporary.bytes[4], payload, payload_length);
    }
    write_le16(&temporary.bytes[temporary.length - 2u],
               profile->checksum(&temporary.bytes[2], payload_length + 2u));
    *out = temporary;
    return NET_OK;
}
