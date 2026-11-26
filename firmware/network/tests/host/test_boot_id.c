/* BOOT_ID of the Network run, computed by the board layer at start-up
 * (include/network/net_boot_id.h). Expected values come from the rules, not
 * from the implementation:
 *   - docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md (identifier
 *     table) and 03_COMMUNICATION/10_CAN_FD.md (common header): BOOT_ID 0 is
 *     reserved; a new run must not carry the BOOT_ID of the previous run;
 *   - a warm reset with a valid record continues at the previous value + 1
 *     (0 skipped); a power-on reset or an invalid record starts from the
 *     seed; a seeded value equal to the valid previous one gets + 1;
 *   - a valid record: magic NET_BOOT_ID_MAGIC, check = CRC-32 of
 *     (magic, boot_id) little-endian, boot_id != 0. The CRC-32 below is
 *     written for this test (IEEE 802.3, checked with its standard vector).
 * The mixing of the seed is not fixed by the rules: the cases check only what
 * the rules fix (nonzero, a function of the seed alone, distinct seeds give
 * distinct values, the record is ignored on power-on).
 * One case per process. */
#include "network/net_boot_id.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)

static const char *case_name;

static uint32_t crc32_bytes(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int b;
    for (i = 0u; i < n; ++i) {
        crc ^= p[i];
        for (b = 0; b < 8; ++b) crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}
static uint32_t expected_check(uint32_t magic, uint32_t boot_id)
{
    uint8_t b[8];
    unsigned i;
    for (i = 0u; i < 4u; ++i) {
        b[i] = (uint8_t)(magic >> (8u * i));
        b[4u + i] = (uint8_t)(boot_id >> (8u * i));
    }
    return crc32_bytes(b, sizeof(b));
}
static NetBootIdRecord valid_record(uint32_t boot_id)
{
    NetBootIdRecord r;
    r.magic = NET_BOOT_ID_MAGIC;
    r.boot_id = boot_id;
    r.check = expected_check(NET_BOOT_ID_MAGIC, boot_id);
    return r;
}
static NetBootIdRecord garbage_record(void)
{
    NetBootIdRecord r;
    memset(&r, 0, sizeof(r)); /* power-on RAM pattern: no magic */
    return r;
}
static bool written(const NetBootIdRecord *r, uint32_t id)
{
    return r->magic == NET_BOOT_ID_MAGIC && r->boot_id == id && r->check == expected_check(NET_BOOT_ID_MAGIC, id);
}
/* The seeded value of a seed (power-on, no valid record). */
static uint32_t seeded(uint32_t seed)
{
    NetBootIdRecord r = garbage_record();
    uint32_t id = 0u;
    if (net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, seed, &id) != NET_OK) return 0u;
    return id;
}

static int crc_vector(void)
{
    static const uint8_t v[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    CHECK(crc32_bytes(v, sizeof(v)) == 0xCBF43926u);
    return 0;
}

/* Warm reset with a valid record: the next run is the previous value + 1. */
static int warm_increment(void)
{
    NetBootIdRecord r = valid_record(41u);
    uint32_t id = 0u;
    CHECK(crc_vector() == 0);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 0xA5A5A5A5u, &id) == NET_OK);
    CHECK(id == 42u && written(&r, 42u));
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 0x12345678u, &id) == NET_OK);
    CHECK(id == 43u && written(&r, 43u));     /* the seed plays no part */
    return 0;
}

/* 0 is reserved: after 0xFFFFFFFF the next run is 1. */
static int warm_wrap_skips_zero(void)
{
    NetBootIdRecord r = valid_record(UINT32_MAX - 1u);
    uint32_t id = 0u;
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 7u, &id) == NET_OK && id == UINT32_MAX);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 7u, &id) == NET_OK && id == 1u && written(&r, 1u));
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 7u, &id) == NET_OK && id == 2u);
    return 0;
}

/* Power-on reset: a value from the seed alone, nonzero; a valid record of an
 * earlier run is not counted on; distinct seeds give distinct values. */
static int por_seeded(void)
{
    static const uint32_t seeds[] = { 1u, 2u, 3u, 0x1234u, 0x80000000u, 0xFFFFFFFFu, 0x0BADF00Du, 0xDEADBEEFu };
    uint32_t got[sizeof(seeds) / sizeof(seeds[0])];
    size_t i, j;
    for (i = 0u; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        NetBootIdRecord r = garbage_record();
        uint32_t id = 0u;
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, seeds[i], &id) == NET_OK);
        CHECK(id != 0u && written(&r, id));
        CHECK(seeded(seeds[i]) == id);        /* a function of the seed */
        got[i] = id;
        for (j = 0u; j < i; ++j) CHECK(got[j] != id);
    }
    /* A valid record of an earlier run: power-on still starts from the seed. */
    for (i = 0u; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        uint32_t prev = got[i] == 1000u ? 2000u : 1000u;
        NetBootIdRecord r = valid_record(prev);
        uint32_t id = 0u;
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, seeds[i], &id) == NET_OK);
        CHECK(id == got[i] && id != prev + 1u && written(&r, id));
    }
    return 0;
}

/* Power-on with the seed that gives the valid previous value: + 1, so the new
 * run never carries the previous BOOT_ID. */
static int por_equal_previous_changes(void)
{
    static const uint32_t seeds[] = { 0x5EEDu, 0xC0FFEEu, 0xFFFFFFFFu };
    size_t i;
    for (i = 0u; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        uint32_t v = seeded(seeds[i]), id = 0u;
        NetBootIdRecord r = valid_record(v);
        CHECK(v != 0u);
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, seeds[i], &id) == NET_OK);
        CHECK(id != v && id != 0u);
        CHECK(id == (v == UINT32_MAX ? 1u : v + 1u) && written(&r, id));
    }
    /* Seed 0 gives a replaced value; that one equal to the record changes too. */
    {
        uint32_t v = seeded(0u), id = 0u;
        NetBootIdRecord r = valid_record(v);
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, 0u, &id) == NET_OK);
        CHECK(id != v && id != 0u && written(&r, id));
    }
    return 0;
}

/* A record that is not valid (no magic, wrong check, BOOT_ID 0) is no
 * previous run: a warm or unknown reset starts from the seed. */
static int invalid_record_reseeds(void)
{
    const uint32_t seed = 0x00C0FFEEu, v = seeded(seed);
    NetBootIdRecord bad[4];
    size_t i;
    CHECK(v != 0u);
    bad[0] = garbage_record();
    bad[1] = valid_record(41u); bad[1].magic ^= 1u;                 /* magic */
    bad[2] = valid_record(41u); bad[2].check ^= 0x00010000u;        /* check */
    bad[3].magic = NET_BOOT_ID_MAGIC; bad[3].boot_id = 0u;           /* BOOT_ID 0 with a fitting check */
    bad[3].check = expected_check(NET_BOOT_ID_MAGIC, 0u);
    for (i = 0u; i < 4u; ++i) {
        NetBootIdRecord r = bad[i];
        uint32_t id = 0u;
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, seed, &id) == NET_OK);
        CHECK(id == v && written(&r, v));
        r = bad[i];
        CHECK(net_boot_id_next(&r, NET_BOOT_RESET_UNKNOWN, seed, &id) == NET_OK);
        CHECK(id == v && written(&r, v));
    }
    return 0;
}

/* Unknown reset cause with a valid record: the record decides, + 1. */
static int unknown_cause_uses_record(void)
{
    NetBootIdRecord r = valid_record(41u);
    uint32_t id = 0u;
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_UNKNOWN, 0x0BADF00Du, &id) == NET_OK);
    CHECK(id == 42u && written(&r, 42u));
    r = valid_record(UINT32_MAX);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_UNKNOWN, 0u, &id) == NET_OK && id == 1u);
    return 0;
}

/* A seed of 0 (no entropy at all) still gives a nonzero BOOT_ID. */
static int seed_zero_nonzero(void)
{
    NetBootIdRecord r = garbage_record();
    uint32_t id = 0u, again = 0u;
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, 0u, &id) == NET_OK);
    CHECK(id != 0u && written(&r, id));
    r = garbage_record();
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 0u, &again) == NET_OK);
    CHECK(again == id && again != 0u);
    return 0;
}

/* The record holds the new value before the call returns: the next warm
 * reset counts on from it; a record changed afterwards (no fitting check) is
 * not trusted. */
static int record_written(void)
{
    NetBootIdRecord r = garbage_record();
    uint32_t first = 0u, id = 0u;
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_POWER_ON, 0x5EEDu, &first) == NET_OK);
    CHECK(written(&r, first));
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 0x5EEDu, &id) == NET_OK);
    CHECK(id == (first == UINT32_MAX ? 1u : first + 1u) && written(&r, id));
    r.boot_id ^= 0x40u;                    /* corrupted after the write */
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 0x5EEDu, &id) == NET_OK);
    CHECK(id == first && written(&r, first));
    return 0;
}

/* Argument errors change neither the record nor *out. */
static int arguments(void)
{
    NetBootIdRecord r = valid_record(41u), keep = r;
    uint32_t id = 0x5A5A5A5Au;
    CHECK(net_boot_id_next(NULL, NET_BOOT_RESET_WARM, 1u, &id) == NET_INVALID_ARGUMENT && id == 0x5A5A5A5Au);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 1u, NULL) == NET_INVALID_ARGUMENT);
    CHECK(memcmp(&r, &keep, sizeof(r)) == 0);
    CHECK(net_boot_id_next(&r, (NetBootResetCause)3, 1u, &id) == NET_INVALID_ARGUMENT);
    CHECK(net_boot_id_next(&r, (NetBootResetCause)-1, 1u, &id) == NET_INVALID_ARGUMENT);
    CHECK(memcmp(&r, &keep, sizeof(r)) == 0 && id == 0x5A5A5A5Au);
    /* *out inside the record */
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 1u, &r.boot_id) == NET_INVALID_ARGUMENT);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 1u, &r.magic) == NET_INVALID_ARGUMENT);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 1u, &r.check) == NET_INVALID_ARGUMENT);
    CHECK(memcmp(&r, &keep, sizeof(r)) == 0);
    CHECK(net_boot_id_next(&r, NET_BOOT_RESET_WARM, 1u, &id) == NET_OK && id == 42u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "warm_increment", warm_increment }, { "warm_wrap_skips_zero", warm_wrap_skips_zero },
        { "por_seeded", por_seeded }, { "por_equal_previous_changes", por_equal_previous_changes },
        { "invalid_record_reseeds", invalid_record_reseeds },
        { "unknown_cause_uses_record", unknown_cause_uses_record },
        { "seed_zero_nonzero", seed_zero_nonzero }, { "record_written", record_written },
        { "arguments", arguments }
    };
    size_t i;
    if (argc != 2) { fprintf(stderr, "usage: %s <case>\n", argv[0]); return 2; }
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            case_name = cases[i].name;
            return cases[i].run();
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
