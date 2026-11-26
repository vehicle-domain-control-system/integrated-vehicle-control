/* R4_PROPOSED_HOST; offsets from R4 10_CAN_FD.md and 20_UART_M.md.
 * Pure byte codec. This source does not implement any ECU runtime.
 */
#include "i1_host_codec.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{ return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr16(uint8_t *p, uint16_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v)
{ size_t i; for (i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8u * i)); }
static int version_ok(uint8_t v) { return v == I1_UART_C || v == I1_UART_M; }

uint16_t i1_crc16(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_C(0xFFFF);
    size_t i;
    unsigned bit;
    for (i = 0; i < length; ++i) {
        crc ^= (uint32_t)data[i] << 8;
        for (bit = 0; bit < 8; ++bit)
            crc = ((crc << 1) ^ ((crc & UINT32_C(0x8000)) ? UINT32_C(0x1021) : 0u)) & UINT32_C(0xFFFF);
    }
    return (uint16_t)crc;
}

static i1_status schema(uint8_t version, uint8_t type, size_t length)
{
    size_t wanted;
    if (!version_ok(version)) return I1_ERR_VERSION;
    if (length > I1_UART_PAYLOAD_MAX) return I1_ERR_LENGTH;
    if (version == I1_UART_C) {
        if (type == 0x60u) wanted = 19;
        else if (type == 0x61u) wanted = 16;
        else if (type == 0x62u) wanted = 15;
        else return I1_ERR_TYPE;
    } else {
        if (type != 0x10u) return I1_ERR_TYPE;
        wanted = 24;
    }
    return length == wanted ? I1_OK : I1_ERR_LENGTH;
}

i1_status i1_uart_envelope_encode(uint8_t version, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity, size_t *written)
{
    if (written != NULL) *written = 0;
    if (written == NULL || out == NULL || (length != 0 && payload == NULL)) return I1_ERR_ARGUMENT;
    if (!version_ok(version)) return I1_ERR_VERSION;
    if (length > I1_UART_PAYLOAD_MAX) return I1_ERR_LENGTH;
    if (capacity < length + I1_UART_OVERHEAD) return I1_ERR_CAPACITY;
    out[0] = 0xA5u; out[1] = 0x5Au; out[2] = version; out[3] = type;
    wr16(out + 4, (uint16_t)length); wr16(out + 6, sequence);
    if (length != 0) memcpy(out + 8, payload, length);
    wr16(out + 8 + length, i1_crc16(out + 2, length + 6));
    *written = length + I1_UART_OVERHEAD;
    return I1_OK;
}

i1_status i1_uart_envelope_decode(uint8_t expected_version,
    const uint8_t *data, size_t length, i1_uart_frame *out)
{
    size_t p;
    i1_uart_frame temp = {0};
    if (data == NULL || out == NULL) return I1_ERR_ARGUMENT;
    if (!version_ok(expected_version)) return I1_ERR_VERSION;
    if (length < 2) return I1_ERR_LENGTH;
    if (data[0] != 0xA5u || data[1] != 0x5Au) return I1_ERR_SYNC;
    if (length < I1_UART_OVERHEAD) return I1_ERR_LENGTH;
    if (data[2] != expected_version) return I1_ERR_VERSION;
    p = rd16(data + 4);
    if (p > I1_UART_PAYLOAD_MAX || length != p + I1_UART_OVERHEAD) return I1_ERR_LENGTH;
    if (rd16(data + 8 + p) != i1_crc16(data + 2, p + 6)) return I1_ERR_CRC;
    temp.version = data[2]; temp.type = data[3];
    temp.link_sequence = rd16(data + 6); temp.payload_length = (uint16_t)p;
    if (p != 0) memcpy(temp.payload, data + 8, p);
    *out = temp;
    return I1_OK;
}

i1_status i1_uart_encode(uint8_t version, uint8_t type, uint16_t sequence,
    const uint8_t *payload, size_t length, uint8_t *out, size_t capacity, size_t *written)
{
    i1_status result;
    if (written != NULL) *written = 0;
    result = schema(version, type, length);
    if (result != I1_OK) return result;
    return i1_uart_envelope_encode(version, type, sequence, payload, length, out, capacity, written);
}

i1_status i1_uart_decode(uint8_t expected_version,
    const uint8_t *data, size_t length, i1_uart_frame *out)
{
    i1_uart_frame temp;
    i1_status result;
    if (out == NULL) return I1_ERR_ARGUMENT;
    result = i1_uart_envelope_decode(expected_version, data, length, &temp);
    if (result != I1_OK) return result;
    result = schema(temp.version, temp.type, temp.payload_length);
    if (result != I1_OK) return result;
    *out = temp;
    return I1_OK;
}

void i1_uart_stream_init(i1_uart_stream *stream, uint8_t expected_version)
{
    if (stream != NULL) {
        memset(stream, 0, sizeof(*stream));
        stream->expected_version = expected_version;
    }
}

i1_status i1_uart_stream_push(i1_uart_stream *stream, uint8_t byte)
{
    if (stream == NULL) return I1_ERR_ARGUMENT;
    if (!version_ok(stream->expected_version)) return I1_ERR_VERSION;
    if (stream->used >= I1_UART_FRAME_MAX) {
        stream->used = 0; ++stream->rejected;
        return I1_ERR_CAPACITY; /* overflow cannot retain a plausible partial message */
    }
    stream->bytes[stream->used++] = byte;
    return I1_OK;
}

static void drop(i1_uart_stream *s, size_t count)
{
    s->used -= count;
    memmove(s->bytes, s->bytes + count, s->used);
}

i1_status i1_uart_stream_next(i1_uart_stream *stream, i1_uart_frame *out)
{
    size_t p, total;
    i1_status result;
    if (stream == NULL || out == NULL) return I1_ERR_ARGUMENT;
    if (!version_ok(stream->expected_version)) return I1_ERR_VERSION;
    if (stream->used > I1_UART_FRAME_MAX) return I1_ERR_ARGUMENT;
    for (;;) {
        if (stream->used == 0) return I1_NEED_MORE;
        if (stream->bytes[0] != 0xA5u ||
            (stream->used >= 2 && stream->bytes[1] != 0x5Au)) {
            drop(stream, 1); ++stream->skipped; continue;
        }
        if (stream->used >= 3 && stream->bytes[2] != stream->expected_version) {
            drop(stream, 1); ++stream->rejected; continue;
        }
        if (stream->used >= 4 && schema(stream->expected_version, stream->bytes[3],
            stream->expected_version == I1_UART_M ? 24u :
            stream->bytes[3] == 0x60u ? 19u : stream->bytes[3] == 0x61u ? 16u : 15u) != I1_OK) {
            drop(stream, 1); ++stream->rejected; continue;
        }
        if (stream->used < 6) return I1_NEED_MORE;
        p = rd16(stream->bytes + 4);
        if (schema(stream->expected_version, stream->bytes[3], p) != I1_OK) {
            drop(stream, 1); ++stream->rejected; continue;
        }
        total = p + I1_UART_OVERHEAD;
        if (stream->used < total) return I1_NEED_MORE;
        result = i1_uart_decode(stream->expected_version, stream->bytes, total, out);
        if (result != I1_OK) { drop(stream, 1); ++stream->rejected; continue; }
        drop(stream, total); ++stream->frames;
        return I1_OK;
    }
}

void i1_uart_stream_abandon(i1_uart_stream *stream)
{
    if (stream != NULL && stream->used != 0 && stream->used <= I1_UART_FRAME_MAX) {
        drop(stream, 1); ++stream->rejected;
    }
}

static i1_status rear_valid(const i1_cis_rear *f)
{
    if (f->boot_id == 0) return I1_ERR_RESERVED;
    return f->validity <= 1 && f->quality_reason <= 9 && f->proximity_status <= 5 ? I1_OK : I1_ERR_VALUE;
}
static int tri_valid(uint8_t v) { return v == 0 || v == 1 || v == 255; }
static i1_status permission_valid(const i1_domain_permission *f)
{
    if (f->boot_id == 0) return I1_ERR_RESERVED;
    return tri_valid(f->vehicle_power_permission) && tri_valid(f->power_operation_state) &&
        f->power_permission_quality <= 3 && f->operation_allowed <= 1 &&
        f->operation_quality <= 3 ? I1_OK : I1_ERR_VALUE;
}
static i1_status can_envelope(uint32_t id, uint32_t wanted, uint8_t dlc,
    const uint8_t *data, size_t length, const void *out)
{
    if (data == NULL || out == NULL) return I1_ERR_ARGUMENT;
    if (id != wanted) return I1_ERR_ID;
    if (dlc != I1_CAN_DLC) return I1_ERR_DLC;
    if (length != I1_CAN_BYTES) return I1_ERR_LENGTH;
    return data[19] == 0 ? I1_OK : I1_ERR_PADDING;
}

i1_status i1_cis_rear_encode(const i1_cis_rear *fields, uint8_t *out,
    size_t capacity, size_t *written)
{
    i1_status result;
    if (written != NULL) *written = 0;
    if (fields == NULL || out == NULL || written == NULL) return I1_ERR_ARGUMENT;
    result = rear_valid(fields);
    if (result != I1_OK) return result;
    if (capacity < I1_CAN_BYTES) return I1_ERR_CAPACITY;
    wr32(out, fields->boot_id); wr32(out + 4, fields->tx_sequence);
    wr32(out + 8, fields->update_sequence); wr16(out + 12, fields->source_age_ms);
    wr16(out + 14, fields->rear_distance_deci_cm);
    out[16] = fields->validity; out[17] = fields->quality_reason;
    out[18] = fields->proximity_status; out[19] = 0;
    *written = I1_CAN_BYTES;
    return I1_OK;
}
i1_status i1_cis_rear_decode(uint32_t id, uint8_t dlc, const uint8_t *data,
    size_t length, i1_cis_rear *out)
{
    i1_cis_rear temp = {0};
    i1_status result = can_envelope(id, I1_CIS_REAR_ID, dlc, data, length, out);
    if (result != I1_OK) return result;
    temp.boot_id = rd32(data); temp.tx_sequence = rd32(data + 4);
    temp.update_sequence = rd32(data + 8); temp.source_age_ms = rd16(data + 12);
    temp.rear_distance_deci_cm = rd16(data + 14);
    temp.validity = data[16]; temp.quality_reason = data[17]; temp.proximity_status = data[18];
    result = rear_valid(&temp);
    if (result != I1_OK) return result;
    *out = temp;
    return I1_OK;
}
i1_status i1_domain_permission_encode(const i1_domain_permission *fields,
    uint8_t *out, size_t capacity, size_t *written)
{
    i1_status result;
    if (written != NULL) *written = 0;
    if (fields == NULL || out == NULL || written == NULL) return I1_ERR_ARGUMENT;
    result = permission_valid(fields);
    if (result != I1_OK) return result;
    if (capacity < I1_CAN_BYTES) return I1_ERR_CAPACITY;
    wr32(out, fields->boot_id); wr32(out + 4, fields->tx_sequence);
    out[8] = fields->vehicle_power_permission; out[9] = fields->power_permission_quality;
    out[10] = fields->power_operation_state; out[11] = fields->operation_allowed;
    out[12] = fields->operation_quality; wr32(out + 13, fields->update_sequence);
    wr16(out + 17, fields->source_age_ms); out[19] = 0;
    *written = I1_CAN_BYTES;
    return I1_OK;
}
i1_status i1_domain_permission_decode(uint32_t id, uint8_t dlc,
    const uint8_t *data, size_t length, i1_domain_permission *out)
{
    i1_domain_permission temp = {0};
    i1_status result = can_envelope(id, I1_DOMAIN_PERMISSION_ID, dlc, data, length, out);
    if (result != I1_OK) return result;
    temp.boot_id = rd32(data); temp.tx_sequence = rd32(data + 4);
    temp.vehicle_power_permission = data[8]; temp.power_permission_quality = data[9];
    temp.power_operation_state = data[10]; temp.operation_allowed = data[11]; temp.operation_quality = data[12];
    temp.update_sequence = rd32(data + 13); temp.source_age_ms = rd16(data + 17);
    result = permission_valid(&temp);
    if (result != I1_OK) return result;
    *out = temp;
    return I1_OK;
}
