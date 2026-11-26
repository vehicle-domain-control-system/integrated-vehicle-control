#include "p6_registration_record_codec.h"

#include <string.h>

static const uint8_t record_magic[4] = {'P', '6', 'R', '2'};

static void write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8U);
    out[2] = (uint8_t)(value >> 16U);
    out[3] = (uint8_t)(value >> 24U);
}

static uint32_t read_u32_le(const uint8_t *in)
{
    return (uint32_t)in[0] |
           ((uint32_t)in[1] << 8U) |
           ((uint32_t)in[2] << 16U) |
           ((uint32_t)in[3] << 24U);
}

static uint32_t crc32_ieee(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    size_t index;

    for (index = 0U; index < length; ++index) {
        uint8_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

bool p6_registration_record_encode(
    const p6_registration_record_t *record,
    uint8_t out_blob[P6_REGISTRATION_RECORD_BLOB_SIZE])
{
    if (out_blob == NULL || !p6_registration_record_is_valid(record)) {
        return false;
    }
    memset(out_blob, 0, P6_REGISTRATION_RECORD_BLOB_SIZE);
    memcpy(&out_blob[0], record_magic, sizeof(record_magic));
    write_u32_le(&out_blob[4], record->schema_version);
    out_blob[8] = (uint8_t)record->state;
    out_blob[9] = record->bond_identity.address_type;
    memcpy(&out_blob[10], record->bond_identity.bytes,
           P6_BOND_IDENTITY_BYTES);
    write_u32_le(&out_blob[16], record->device_context_id);
    write_u32_le(&out_blob[20], record->registration_epoch);
    write_u32_le(&out_blob[24], crc32_ieee(out_blob, 24U));
    return true;
}

bool p6_registration_record_decode(
    const uint8_t *blob,
    size_t blob_length,
    p6_registration_record_t *out_record)
{
    p6_registration_record_t record;

    if (blob == NULL || out_record == NULL ||
        blob_length != P6_REGISTRATION_RECORD_BLOB_SIZE ||
        memcmp(blob, record_magic, sizeof(record_magic)) != 0 ||
        read_u32_le(&blob[24]) != crc32_ieee(blob, 24U)) {
        return false;
    }
    memset(&record, 0, sizeof(record));
    record.schema_version = read_u32_le(&blob[4]);
    record.state = (p6_registration_record_state_t)blob[8];
    record.bond_identity.address_type = blob[9];
    memcpy(record.bond_identity.bytes, &blob[10], P6_BOND_IDENTITY_BYTES);
    record.device_context_id = read_u32_le(&blob[16]);
    record.registration_epoch = read_u32_le(&blob[20]);
    if (!p6_registration_record_is_valid(&record)) {
        return false;
    }
    *out_record = record;
    return true;
}
