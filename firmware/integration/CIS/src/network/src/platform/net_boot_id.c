/* BOOT_ID of the Network sending run (network/net_boot_id.h): the next run
 * after a warm reset, a seeded value after a power-on reset or a lost record.
 * No static state, no SDK: the board passes the record, the reset cause and
 * the seed. */
#include "network/net_boot_id.h"

/* CRC-32 (IEEE 802.3, reflected 0xEDB88320) of one word, least significant
 * byte first, continuing from crc. */
static uint32_t crc32_word(uint32_t crc, uint32_t value)
{
    unsigned i, bit;
    for (i = 0u; i < 4u; ++i) {
        crc ^= (value >> (8u * i)) & 0xFFu;
        for (bit = 0u; bit < 8u; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static uint32_t record_check(uint32_t magic, uint32_t boot_id)
{
    return ~crc32_word(crc32_word(0xFFFFFFFFu, magic), boot_id);
}

static bool record_valid(const NetBootIdRecord *r)
{
    return r->magic == NET_BOOT_ID_MAGIC && r->boot_id != 0u && r->check == record_check(r->magic, r->boot_id);
}

/* 32-bit finalizer of MurmurHash3: a bijection, so two seeds never give the
 * same value and only seed 0 gives 0. */
static uint32_t mix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

/* The value after v; 0 is reserved and skipped. */
static uint32_t after(uint32_t v) { return v == UINT32_MAX ? 1u : v + 1u; }

static bool overlap(const void *a, size_t na, const void *b, size_t nb)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x < y + nb && y < x + na;
}

NetStatus net_boot_id_next(NetBootIdRecord *record, NetBootResetCause cause, uint32_t seed, uint32_t *out)
{
    uint32_t id;
    bool valid;
    if (record == NULL || out == NULL || overlap(record, sizeof(*record), out, sizeof(*out)))
        return NET_INVALID_ARGUMENT;
    if (cause != NET_BOOT_RESET_UNKNOWN && cause != NET_BOOT_RESET_POWER_ON && cause != NET_BOOT_RESET_WARM)
        return NET_INVALID_ARGUMENT;
    valid = record_valid(record);
    if (valid && cause != NET_BOOT_RESET_POWER_ON) {
        /* Warm reset: the record survived, the next run counts on from it. An
         * unknown cause with a valid record is taken as warm (the record
         * check rules out a power-on pattern). */
        id = after(record->boot_id);
    } else {
        /* Power-on reset or a lost record: start from the seed. */
        id = mix(seed);
        if (id == 0u) id = 1u;
        if (valid && id == record->boot_id) id = after(id);
    }
    record->magic = NET_BOOT_ID_MAGIC;
    record->boot_id = id;
    record->check = record_check(NET_BOOT_ID_MAGIC, id);
    *out = id;
    return NET_OK;
}
