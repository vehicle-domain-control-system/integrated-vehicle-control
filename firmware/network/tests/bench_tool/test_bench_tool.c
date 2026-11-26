/* Host tests for tools/vss_bench: encode -> 144 console command -> decode
 * round trips, UART CRC check value and the 144 command formats.
 * Expected bytes come from the repository codecs and documented offsets
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md, 20_UART_M.md,
 * 30_UART_C.md), never from the tool output itself. */
#include "vss_bench.h"

#include "network/net_wire.h"
#include "network/net_wire_can.h"
#include "network/net_wire_uart.h"
#include "network/net_wire_uart_c_status.h"
#include "network/net_wire_uart_dto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    fprintf(stderr, "--- tool output ---\n%s\n", g_text); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define ARGC(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define CONSOLE_LINE_MAX 80u /* S32K144 debug console line capacity */

static char g_text[32768];
static VssBenchText g_out;

static int run(int argc, const char *const *argv)
{
    vss_bench_text_init(&g_out, g_text, sizeof(g_text));
    return vss_bench_run(argc, argv, &g_out);
}

/* ---- output scanning --------------------------------------------------- */

static const char *line_at(const char *p, char *line, size_t cap)
{
    size_t n = 0u;
    if (p == NULL || *p == '\0') return NULL;
    while (p[n] != '\0' && p[n] != '\n') ++n;
    if (n >= cap) n = cap - 1u;
    memcpy(line, p, n);
    line[n] = '\0';
    while (*p != '\0' && *p != '\n') ++p;
    return *p == '\n' ? p + 1 : p;
}

static int count_lines_with_prefix(const char *text, const char *prefix)
{
    char line[512];
    const char *p = text;
    int n = 0;
    while ((p = line_at(p, line, sizeof(line))) != NULL)
        if (strncmp(line, prefix, strlen(prefix)) == 0) ++n;
    return n;
}

static int is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static uint8_t hex_byte(const char *s)
{
    char b[3] = { s[0], s[1], '\0' };
    return (uint8_t)strtoul(b, NULL, 16);
}

/* Every non-comment line must fit the 144 console line. */
static int console_lines_fit(const char *text)
{
    char line[512];
    const char *p = text;
    while ((p = line_at(p, line, sizeof(line))) != NULL)
        if (line[0] != '#' && line[0] != '\0' && strlen(line) >= CONSOLE_LINE_MAX) return 0;
    return 1;
}

/* One 144 named CAN send: "can tx begin NAME", "can tx data .." lines and
 * "can tx commit" (S32K144 debug node named send). */
typedef struct {
    char name[32];
    uint8_t data[64];
    size_t used;
    int commits;
} NamedBlock;

#define MAX_BLOCKS 8

/* Checks every output line against the 144 console rules and collects the
 * named send blocks. Lines are either notes ("# ...") or commands; every
 * line is printable ASCII, at most CONSOLE_LINE_MAX - 1 characters, without
 * leading or trailing spaces. Returns the block count, or a negative code. */
static int parse_named_blocks(const char *text, NamedBlock *blocks, int max)
{
    char line[512];
    const char *p = text;
    int count = 0;
    NamedBlock *cur = NULL;
    while ((p = line_at(p, line, sizeof(line))) != NULL) {
        size_t len = strlen(line), i;
        if (len == 0u || len >= CONSOLE_LINE_MAX) return -1;
        for (i = 0u; i < len; ++i)
            if (line[i] < 0x20 || line[i] > 0x7E) return -2;
        if (line[0] == ' ' || line[len - 1u] == ' ') return -3;
        if (line[0] == '#') {
            if (strncmp(line, "# ", 2u) != 0) return -4;
            continue;
        }
        if (strncmp(line, "can tx begin ", 13u) == 0) {
            const char *name = line + 13;
            if (cur != NULL || count >= max) return -5;
            if (strchr(name, ' ') != NULL || strncmp(name, "0x", 2u) == 0 ||
                strlen(name) >= sizeof(blocks[0].name)) return -6;
            cur = &blocks[count];
            memset(cur, 0, sizeof(*cur));
            strcpy(cur->name, name);
        } else if (strncmp(line, "can tx data ", 12u) == 0) {
            const char *s = line + 12;
            size_t tokens = 0u;
            if (cur == NULL) return -7;
            for (;;) {
                if (!is_hex_char(s[0]) || !is_hex_char(s[1])) return -8;
                if (cur->used >= sizeof(cur->data)) return -9;
                cur->data[cur->used++] = hex_byte(s);
                ++tokens;
                s += 2;
                if (*s == '\0') break;
                if (*s != ' ') return -10;
                ++s;
            }
            if (tokens > 16u) return -11;
        } else if (strcmp(line, "can tx commit") == 0) {
            if (cur == NULL) return -12;
            cur->commits = 1;
            cur = NULL;
            ++count;
        } else if (strcmp(line, "can tx status") == 0) {
            if (cur != NULL) return -13;
        } else {
            return -14; /* an unknown command or an unmarked note */
        }
    }
    return cur != NULL ? -15 : count;
}

/* Collects the k-th (0-based) named block for name, checks that it carries
 * exactly the used length and rebuilds the zero-padded CAN FD frame the 144
 * sends. Returns 0 on success. */
static int collect_can_bundle(const char *text, int k, const char *name, uint16_t id,
    size_t used, size_t frame_len, NetWireCanEnvelope *env)
{
    NamedBlock blocks[MAX_BLOCKS];
    int n = parse_named_blocks(text, blocks, MAX_BLOCKS), i, seen = -1;
    if (n < 0) return 100 - n;
    for (i = 0; i < n; ++i)
        if (strcmp(blocks[i].name, name) == 0 && ++seen == k) break;
    if (i == n) return 1;
    if (blocks[i].used != used) return 2;
    memset(env, 0, sizeof(*env));
    memcpy(env->bytes, blocks[i].data, used);
    env->standard_id = id;
    env->fdf = true;
    env->brs = true;
    env->length = frame_len;
    env->dlc = frame_len == 20u ? 11u : frame_len == 24u ? 12u : frame_len == 32u ? 13u : 15u;
    return 0;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The single "uart send NAME HEX" line; returns the HEX text length. */
static size_t uart_send_hex(const char *text, const char *name, uint8_t *payload, size_t cap)
{
    char line[512], prefix[64];
    const char *p = text;
    size_t n = 0u, i;
    snprintf(prefix, sizeof(prefix), "uart send %s ", name);
    while ((p = line_at(p, line, sizeof(line))) != NULL) {
        if (strncmp(line, prefix, strlen(prefix)) == 0) {
            const char *hex = line + strlen(prefix);
            n = strlen(hex);
            for (i = 0u; i < n; ++i) if (!is_hex_char(hex[i])) return 0u;
            if (n % 2u != 0u || n / 2u > cap) return 0u;
            for (i = 0u; i < n / 2u; ++i) payload[i] = hex_byte(hex + 2u * i);
            return n;
        }
    }
    return 0u;
}

static int first_line_has(const char *text, const char *needle)
{
    char line[512];
    return line_at(text, line, sizeof(line)) != NULL && strstr(line, needle) != NULL;
}

/* Reference CRC: bitwise CCITT-FALSE, as the 144 DebugFrame_Crc16 computes
 * it (poly 0x1021, init 0xFFFF, no reflection, no xorout). */
static uint16_t reference_crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    size_t i;
    int bit;
    for (i = 0u; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0; bit < 8; ++bit)
            crc = (uint16_t)(((uint32_t)crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u));
    }
    return crc;
}

static void put_le(uint8_t *p, uint32_t v, size_t width)
{
    size_t i;
    for (i = 0u; i < width; ++i) p[i] = (uint8_t)(v >> (8u * i));
}

static void spaced_hex(char *dst, size_t cap, const uint8_t *b, size_t n)
{
    size_t i, used = 0u;
    dst[0] = '\0';
    for (i = 0u; i < n && used + 4u < cap; ++i)
        used += (size_t)snprintf(dst + used, cap - used, i ? " %02X" : "%02X", b[i]);
}

static void packed_hex(char *dst, size_t cap, const char *prefix, const uint8_t *b, size_t n)
{
    size_t i, used = (size_t)snprintf(dst, cap, "%s", prefix);
    for (i = 0u; i < n && used + 3u < cap; ++i)
        used += (size_t)snprintf(dst + used, cap - used, "%02X", b[i]);
}

/* ---- CAN cases --------------------------------------------------------- */

static int can_event_roundtrip(void)
{
    const char *const argv[] = { "can", "event", "--boot", "0x1A2B3C4D", "--tx", "7",
        "event_kind=3", "occurrence_id=0x11223344", "original_age_ms=37", "use_limit_ms=400",
        "related_command_id=0x01020304" };
    NetWireVssEventRaw want, got;
    NetWireCanEnvelope ref, env;
    size_t i;
    CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    CHECK(!g_out.truncated);
    memset(&want, 0, sizeof(want));
    want.boot_id = 0x1A2B3C4Du;
    want.tx_sequence = 7u;
    want.event_kind = 3u;
    want.occurrence_id = 0x11223344u;
    want.original_age_ms = 37u;
    want.use_limit_ms = 400u;
    want.value_quality = 0u;
    want.input_availability = 0u;
    want.related_command_id = 0x01020304u;
    CHECK(net_wire_can_encode_vss_event(PROFILE, &want, &ref) == NET_WIRE_OK);
    CHECK(collect_can_bundle(g_text, 0, "VSS_EVENT", 0x160u, 23u, 24u, &env) == 0);
    CHECK(memcmp(env.bytes, ref.bytes, 24u) == 0);
    for (i = 23u; i < 24u; ++i) CHECK(env.bytes[i] == 0u);
    CHECK(net_wire_can_decode_vss_event(PROFILE, &env, &got) == NET_WIRE_OK);
    CHECK(got.boot_id == want.boot_id && got.tx_sequence == want.tx_sequence);
    CHECK(got.event_kind == 3u && got.occurrence_id == 0x11223344u);
    CHECK(got.original_age_ms == 37u && got.use_limit_ms == 400u);
    CHECK(got.related_command_id == 0x01020304u);
    /* The data lines hold exactly the 23 used bytes, in codec order. */
    CHECK(strstr(g_text, "can tx data 4D 3C 2B 1A 07 00 00 00 03 44 33 22 11") != NULL);
    CHECK(count_lines_with_prefix(g_text, "can tx begin VSS_EVENT") == 1);
    CHECK(count_lines_with_prefix(g_text, "can tx commit") == 1);
    CHECK(count_lines_with_prefix(g_text, "can tx data ") == 2);
    CHECK(console_lines_fit(g_text));
    return 0;
}

static int can_permission_roundtrip(void)
{
    const char *const argv[] = { "can", "perm", "--boot", "444F4D31", "--tx", "3",
        "update_sequence=77", "source_age_ms=10", "vehicle_power_permission=0" };
    NetWireDomainPermissionRaw got;
    NetWireCanEnvelope env;
    CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    CHECK(collect_can_bundle(g_text, 0, "DOMAIN_PERMISSION", 0x080u, 19u, 20u, &env) == 0);
    CHECK(net_wire_can_decode_permission(PROFILE, &env, &got) == NET_WIRE_OK);
    CHECK(got.boot_id == 0x444F4D31u && got.tx_sequence == 3u);
    CHECK(got.vehicle_power_permission == 0u && got.power_permission_quality == 0u);
    CHECK(got.power_operation_state == 1u && got.operation_allowed == 1u);
    CHECK(got.update_sequence == 77u && got.source_age_ms == 10u);
    CHECK(count_lines_with_prefix(g_text, "can tx data ") == 2);
    CHECK(console_lines_fit(g_text));
    return 0;
}

static int can_warning_roundtrip(void)
{
    const char *const argv[] = { "can", "warning", "--boot", "0x00000042", "--tx", "0x10",
        "rear_obstacle_state=2", "rear_activation_sequence=4000000000" };
    NetWireVssWarningStateRaw got;
    NetWireCanEnvelope env;
    CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    CHECK(collect_can_bundle(g_text, 0, "VSS_WARNING_STATE", 0x0C0u, 60u, 64u, &env) == 0);
    CHECK(net_wire_can_decode_vss_warning_state(PROFILE, &env, &got) == NET_WIRE_OK);
    CHECK(got.boot_id == 0x42u && got.tx_sequence == 16u);
    CHECK(got.rear_obstacle_state == 2u && got.rear_activation_sequence == 4000000000u);
    CHECK(got.antipinch_state == 1u && got.rear_detection_activation == 1u);
    CHECK(count_lines_with_prefix(g_text, "can tx data ") == 4);
    CHECK(console_lines_fit(g_text));
    return 0;
}

static int can_decode_status(void)
{
    NetWireVssStatusRaw s;
    NetWireCanEnvelope e;
    char packed[160], spaced[160];
    memset(&s, 0, sizeof(s));
    s.boot_id = 0x56535331u;
    s.tx_sequence = 12u;
    s.vss_state = 2u;
    s.vss_availability = 1u;
    s.vss_accepting_events = 1u;
    s.vss_last_fault = 3u;
    s.last_fault_occurrence = 5u;
    s.last_fault_age_ms = 65535u;
    s.update_sequence = 12u;
    s.source_age_ms = 4u;
    CHECK(net_wire_can_encode_vss_status(PROFILE, &s, &e) == NET_WIRE_OK);
    CHECK(e.length == 32u);
    /* 144 "can log hex" compact form: data=<hex> */
    packed_hex(packed, sizeof(packed), "data=", e.bytes, e.length);
    {
        const char *const argv[] = { "can", "decode-status", packed };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "codec: OK") != NULL);
        CHECK(strstr(g_text, "boot_id=0x56535331") != NULL);
        CHECK(strstr(g_text, " tx_sequence=12 ") != NULL);
        CHECK(strstr(g_text, " update_sequence=12 source_age_ms=4") != NULL);
    }
    /* Simple-console form: offset labels and spaced bytes; used length only. */
    spaced_hex(spaced, sizeof(spaced), e.bytes, 28u);
    {
        const char *const argv[] = { "can", "decode-status", "00:", spaced };
        char *words[40];
        const char *argv2[44];
        int n = 0, i;
        char *tok = strtok(spaced, " ");
        while (tok != NULL && n < 40) { words[n++] = tok; tok = strtok(NULL, " "); }
        argv2[0] = argv[0]; argv2[1] = argv[1]; argv2[2] = argv[2];
        for (i = 0; i < n; ++i) argv2[3 + i] = words[i];
        CHECK(n == 28);
        CHECK(run(3 + n, argv2) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "codec: OK") != NULL);
    }
    /* Out-of-range VSS_STATE byte (B8) -> codec verdict INVALID. */
    e.bytes[8] = 0x77u;
    packed_hex(packed, sizeof(packed), "", e.bytes, e.length);
    {
        const char *const argv[] = { "can", "decode-status", packed };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_INVALID);
        CHECK(strstr(g_text, "codec: INVALID") != NULL);
    }
    /* Wrong byte count -> usage error. */
    {
        const char *const argv[] = { "can", "decode-status", "01", "02", "03" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    return 0;
}

static int can_script(void)
{
    NetWireCanEnvelope s1, s2a, s2b, w, p;
    NetWireVssEventRaw ev;
    NetWireVssWarningStateRaw wr;
    NetWireDomainPermissionRaw pr;
    {
        const char *const argv[] = { "can", "script", "all" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    }
    CHECK(strstr(g_text, "# ---- S1") != NULL && strstr(g_text, "# ---- S2a") != NULL);
    CHECK(strstr(g_text, "# ---- S2b") != NULL && strstr(g_text, "# ---- S3") != NULL);
    CHECK(strstr(g_text, "# ---- S4") != NULL);
    CHECK(count_lines_with_prefix(g_text, "# expect:") == 5);
    CHECK(collect_can_bundle(g_text, 0, "VSS_EVENT", 0x160u, 23u, 24u, &s1) == 0);
    CHECK(collect_can_bundle(g_text, 1, "VSS_EVENT", 0x160u, 23u, 24u, &s2a) == 0);
    CHECK(collect_can_bundle(g_text, 2, "VSS_EVENT", 0x160u, 23u, 24u, &s2b) == 0);
    CHECK(collect_can_bundle(g_text, 0, "VSS_WARNING_STATE", 0x0C0u, 60u, 64u, &w) == 0);
    CHECK(collect_can_bundle(g_text, 0, "DOMAIN_PERMISSION", 0x080u, 19u, 20u, &p) == 0);
    /* S2a is an exact duplicate of S1. */
    CHECK(memcmp(s1.bytes, s2a.bytes, 24u) == 0);
    CHECK(net_wire_can_decode_vss_event(PROFILE, &s1, &ev) == NET_WIRE_OK);
    CHECK(ev.tx_sequence == 1u && ev.boot_id == 0x42454E43u);
    CHECK(net_wire_can_decode_vss_event(PROFILE, &s2b, &ev) == NET_WIRE_OK);
    CHECK(ev.tx_sequence == 2u && ev.occurrence_id == 2u);
    CHECK(net_wire_can_decode_vss_warning_state(PROFILE, &w, &wr) == NET_WIRE_OK);
    CHECK(wr.tx_sequence == 1u);
    CHECK(net_wire_can_decode_permission(PROFILE, &p, &pr) == NET_WIRE_OK);
    CHECK(pr.tx_sequence == 1u);
    CHECK(console_lines_fit(g_text));
    {
        const char *const argv[] = { "can", "script", "--boot", "0xABCD0001", "S3" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(collect_can_bundle(g_text, 0, "VSS_WARNING_STATE", 0x0C0u, 60u, 64u, &w) == 0);
        CHECK(net_wire_can_decode_vss_warning_state(PROFILE, &w, &wr) == NET_WIRE_OK);
        CHECK(wr.boot_id == 0xABCD0001u);
        CHECK(count_lines_with_prefix(g_text, "can tx begin VSS_EVENT") == 0);
    }
    {
        const char *const argv[] = { "can", "script", "--tx-base", "0", "S1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(count_lines_with_prefix(g_text, "can tx") == 0);
    }
    {
        /* S2b needs base + 1, so the largest base is 0xFFFFFFFE. */
        const char *const argv[] = { "can", "script", "--tx-base", "0xFFFFFFFF", "S2" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "script", "--boot", "0", "S1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "script", "S1", "--tx-base" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "script", "S5" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "script" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    return 0;
}

static int can_field_errors(void)
{
    {
        const char *const argv[] = { "can", "event", "--boot", "1", "--tx", "1", "nope=1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(strstr(g_text, "unknown field") != NULL);
    }
    {
        const char *const argv[] = { "can", "event", "--boot", "1", "--tx", "1", "event_kind=256" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        /* In range for u8 but not a documented quality code: codec refuses. */
        const char *const argv[] = { "can", "event", "--boot", "1", "--tx", "1", "value_quality=99" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(strstr(g_text, "codec rejected") != NULL);
        CHECK(count_lines_with_prefix(g_text, "can tx") == 0);
    }
    {
        const char *const argv[] = { "can", "event", "--boot", "1", "--tx", "1", "tx_sequence=5" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "perm", "--boot", "0", "--tx", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(count_lines_with_prefix(g_text, "can tx") == 0);
    }
    {
        const char *const argv[] = { "can", "perm", "--boot", "1", "--tx", "0" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "perm", "--tx", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "perm", "--boot", "123456789", "--tx", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        const char *const argv[] = { "can", "bogus" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(strstr(g_text, "usage:") != NULL);
    }
    return 0;
}

/* ---- 144 named-send format --------------------------------------------- */

typedef struct {
    const char *kind;
    const char *name;
    size_t used; /* exact byte count the 144 named send accepts */
} NamedMsg;

/* Used lengths from docs/network/interface/03_COMMUNICATION/10_CAN_FD.md. */
static const NamedMsg named_msgs[] = {
    { "perm", "DOMAIN_PERMISSION", 19u },
    { "event", "VSS_EVENT", 23u },
    { "warning", "VSS_WARNING_STATE", 60u },
};

static const uint8_t bench_boot_le[4] = { 0x43u, 0x4Eu, 0x45u, 0x42u }; /* 0x42454E43 */

static int named_index(const char *name)
{
    int i;
    for (i = 0; i < ARGC(named_msgs); ++i)
        if (strcmp(named_msgs[i].name, name) == 0) return i;
    return -1;
}

/* Old 144 numeric-ID send words must never appear. */
static int has_old_format(const char *text)
{
    return strstr(text, "arm") != NULL || strstr(text, "NETBENCH") != NULL ||
           strstr(text, "begin 0x") != NULL;
}

static int t_can_named_format(void)
{
    NamedBlock blocks[MAX_BLOCKS];
    int k, n;
    for (k = 0; k < ARGC(named_msgs); ++k) {
        const NamedMsg *m = &named_msgs[k];
        const char *const argv[] = { "can", m->kind, "--boot", "0x42454E43", "--tx", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(!g_out.truncated);
        n = parse_named_blocks(g_text, blocks, MAX_BLOCKS);
        CHECK(n == 1);
        CHECK(strcmp(blocks[0].name, m->name) == 0);
        CHECK(blocks[0].used == m->used); /* no padding bytes */
        CHECK(blocks[0].commits == 1);
        CHECK(count_lines_with_prefix(g_text, "can tx begin ") == 1);
        CHECK(count_lines_with_prefix(g_text, "can tx commit") == 1);
        CHECK(count_lines_with_prefix(g_text, "can tx data ") == (int)((m->used + 15u) / 16u));
        CHECK(count_lines_with_prefix(g_text, "can tx status") == 1);
        CHECK(!has_old_format(g_text));
    }
    {
        /* The whole scenario output is pasteable as one block of text. */
        const char *const argv[] = { "can", "script", "all" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(!g_out.truncated);
        n = parse_named_blocks(g_text, blocks, MAX_BLOCKS);
        CHECK(n == 5);
        for (k = 0; k < n; ++k) {
            int idx = named_index(blocks[k].name);
            CHECK(idx >= 0);
            CHECK(blocks[k].used == named_msgs[idx].used);
        }
        CHECK(count_lines_with_prefix(g_text, "can tx commit") == 5);
        CHECK(!has_old_format(g_text));
    }
    return 0;
}

static int t_can_payload_le(void)
{
    static const char *const boots[] = { "0x42454E43", "42454E43" };
    static const uint8_t want[8] = { 0x43u, 0x4Eu, 0x45u, 0x42u, 0x01u, 0x00u, 0x00u, 0x00u };
    static const uint8_t want_tx[4] = { 0x04u, 0x03u, 0x02u, 0x01u };
    NamedBlock blocks[MAX_BLOCKS];
    int k, b;
    for (k = 0; k < ARGC(named_msgs); ++k) {
        for (b = 0; b < ARGC(boots); ++b) {
            const char *const argv[] = { "can", named_msgs[k].kind, "--boot", boots[b], "--tx", "1" };
            CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
            CHECK(parse_named_blocks(g_text, blocks, MAX_BLOCKS) == 1);
            CHECK(blocks[0].used >= 8u);
            CHECK(memcmp(blocks[0].data, want, 8u) == 0);
        }
        {
            const char *const argv[] = { "can", named_msgs[k].kind, "--boot", "42454E43",
                "--tx", "0x01020304" };
            CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
            CHECK(parse_named_blocks(g_text, blocks, MAX_BLOCKS) == 1);
            CHECK(memcmp(blocks[0].data, bench_boot_le, 4u) == 0);
            CHECK(memcmp(blocks[0].data + 4, want_tx, 4u) == 0);
        }
    }
    return 0;
}

/* Runs one scenario command and checks every block against one BOOT and a
 * per-message TX that never goes back. */
static int script_blocks(const char *const *argv, int argc, const uint8_t boot_le[4],
    uint32_t last_tx[3], NamedBlock *blocks, int *count)
{
    int k;
    if (run(argc, argv) != VSS_BENCH_OK) return 1;
    *count = parse_named_blocks(g_text, blocks, MAX_BLOCKS);
    if (*count <= 0) return 2;
    for (k = 0; k < *count; ++k) {
        int idx = named_index(blocks[k].name);
        uint32_t tx;
        if (idx < 0 || blocks[k].used != named_msgs[idx].used) return 3;
        if (memcmp(blocks[k].data, boot_le, 4u) != 0) return 4;
        tx = le32(blocks[k].data + 4);
        if (tx == 0u || tx < last_tx[idx]) return 5;
        last_tx[idx] = tx;
    }
    return 0;
}

static int t_can_script_consistent(void)
{
    static const char *const names[] = { "S1", "S2", "S3", "S4" };
    static const uint8_t other_boot_le[4] = { 0x44u, 0x33u, 0x22u, 0x11u };
    NamedBlock blocks[MAX_BLOCKS];
    uint8_t s1[23];
    uint32_t last_tx[3] = { 0u, 0u, 0u };
    int s, n = 0;
    /* Separate runs with the defaults, in the order a bench session uses. */
    for (s = 0; s < ARGC(names); ++s) {
        const char *const argv[] = { "can", "script", names[s] };
        CHECK(script_blocks(argv, ARGC(argv), bench_boot_le, last_tx, blocks, &n) == 0);
        CHECK(strstr(g_text, "# Keep one BOOT per session.") != NULL);
        CHECK(strstr(g_text, "--tx-base") != NULL);
        if (s == 0) {
            CHECK(n == 1 && strcmp(blocks[0].name, "VSS_EVENT") == 0);
            CHECK(le32(blocks[0].data + 4) == 1u);
            memcpy(s1, blocks[0].data, sizeof(s1));
        } else if (s == 1) {
            /* S2a repeats S1 byte for byte; S2b is TX + 1. */
            CHECK(n == 2);
            CHECK(memcmp(blocks[0].data, s1, sizeof(s1)) == 0);
            CHECK(le32(blocks[1].data + 4) == le32(blocks[0].data + 4) + 1u);
        } else {
            CHECK(n == 1);
            CHECK(strcmp(blocks[0].name, s == 2 ? "VSS_WARNING_STATE" : "DOMAIN_PERMISSION") == 0);
        }
    }
    /* "all" in one run gives the same blocks. */
    {
        const char *const argv[] = { "can", "script", "all" };
        uint32_t fresh[3] = { 0u, 0u, 0u };
        CHECK(script_blocks(argv, ARGC(argv), bench_boot_le, fresh, blocks, &n) == 0);
        CHECK(n == 5);
        CHECK(memcmp(blocks[0].data, s1, sizeof(s1)) == 0);
        CHECK(memcmp(blocks[1].data, s1, sizeof(s1)) == 0);
    }
    /* A rerun continues each message TX from --tx-base; --boot changes every block. */
    {
        const char *const argv[] = { "can", "script", "--tx-base", "3", "all" };
        uint32_t cont[3];
        memcpy(cont, last_tx, sizeof(cont));
        CHECK(script_blocks(argv, ARGC(argv), bench_boot_le, cont, blocks, &n) == 0);
        CHECK(n == 5);
        CHECK(le32(blocks[0].data + 4) == 3u && le32(blocks[1].data + 4) == 3u);
        CHECK(le32(blocks[2].data + 4) == 4u);
        CHECK(le32(blocks[3].data + 4) == 3u && le32(blocks[4].data + 4) == 3u);
    }
    {
        const char *const argv[] = { "can", "script", "--boot", "11223344", "--tx-base", "9", "S2", "S4" };
        uint32_t fresh[3] = { 0u, 0u, 0u };
        CHECK(script_blocks(argv, ARGC(argv), other_boot_le, fresh, blocks, &n) == 0);
        CHECK(n == 3);
        CHECK(le32(blocks[0].data + 4) == 9u && le32(blocks[1].data + 4) == 10u);
        CHECK(le32(blocks[2].data + 4) == 9u);
    }
    return 0;
}

#define EQ(f) CHECK(got.f == want.f)

static int t_can_roundtrip_named(void)
{
    NetWireCanEnvelope env, ref;
    {
        const char *const argv[] = { "can", "perm", "--boot", "0x42454E43", "--tx", "21",
            "vehicle_power_permission=0", "power_permission_quality=1", "power_operation_state=0",
            "operation_allowed=0", "operation_quality=0", "update_sequence=1234",
            "source_age_ms=55" };
        NetWireDomainPermissionRaw want, got;
        memset(&want, 0, sizeof(want));
        want.boot_id = 0x42454E43u; want.tx_sequence = 21u;
        want.vehicle_power_permission = 0u; want.power_permission_quality = 1u;
        want.power_operation_state = 0u; want.operation_allowed = 0u;
        want.operation_quality = 0u; want.update_sequence = 1234u; want.source_age_ms = 55u;
        CHECK(net_wire_can_encode_permission(PROFILE, &want, &ref) == NET_WIRE_OK);
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(collect_can_bundle(g_text, 0, "DOMAIN_PERMISSION", 0x080u, 19u, 20u, &env) == 0);
        CHECK(memcmp(env.bytes, ref.bytes, 20u) == 0);
        memset(&got, 0, sizeof(got));
        CHECK(net_wire_can_decode_permission(PROFILE, &env, &got) == NET_WIRE_OK);
        EQ(boot_id); EQ(tx_sequence); EQ(vehicle_power_permission); EQ(power_permission_quality);
        EQ(power_operation_state); EQ(operation_allowed); EQ(operation_quality);
        EQ(update_sequence); EQ(source_age_ms);
    }
    {
        const char *const argv[] = { "can", "event", "--boot", "42454E43", "--tx", "0x22",
            "event_kind=4", "occurrence_id=0x0A0B0C0D", "original_age_ms=99", "use_limit_ms=700",
            "value_quality=0", "input_availability=1", "related_command_id=0x777" };
        NetWireVssEventRaw want, got;
        memset(&want, 0, sizeof(want));
        want.boot_id = 0x42454E43u; want.tx_sequence = 0x22u; want.event_kind = 4u;
        want.occurrence_id = 0x0A0B0C0Du; want.original_age_ms = 99u; want.use_limit_ms = 700u;
        want.value_quality = 0u; want.input_availability = 1u; want.related_command_id = 0x777u;
        CHECK(net_wire_can_encode_vss_event(PROFILE, &want, &ref) == NET_WIRE_OK);
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(collect_can_bundle(g_text, 0, "VSS_EVENT", 0x160u, 23u, 24u, &env) == 0);
        CHECK(memcmp(env.bytes, ref.bytes, 24u) == 0);
        memset(&got, 0, sizeof(got));
        CHECK(net_wire_can_decode_vss_event(PROFILE, &env, &got) == NET_WIRE_OK);
        EQ(boot_id); EQ(tx_sequence); EQ(event_kind); EQ(occurrence_id); EQ(original_age_ms);
        EQ(use_limit_ms); EQ(value_quality); EQ(input_availability); EQ(related_command_id);
    }
    {
        const char *const argv[] = { "can", "warning", "--boot", "0x42454E43", "--tx", "5",
            "antipinch_source_boot=0x01010101", "antipinch_source_sequence=500",
            "antipinch_original_age_ms=40", "antipinch_state=0", "antipinch_quality=0",
            "antipinch_reason=0", "antipinch_availability=0",
            "occupant_hazard_source_boot=0x02020202", "occupant_hazard_source_sequence=600",
            "occupant_hazard_original_age_ms=41", "occupant_hazard_state=1",
            "occupant_hazard_quality=0", "occupant_hazard_reason=0",
            "occupant_hazard_availability=0",
            "rear_obstacle_source_boot=0x03030303", "rear_obstacle_source_sequence=700",
            "rear_obstacle_original_age_ms=900", "rear_obstacle_state=2",
            "rear_obstacle_quality=0", "rear_obstacle_reason=0", "rear_obstacle_availability=0",
            "rear_detection_activation=1", "activation_meta_format=1",
            "rear_activation_quality=0", "rear_activation_reason=0",
            "rear_activation_sequence=800", "rear_activation_age_ms=77" };
        NetWireVssWarningStateRaw want, got;
        memset(&want, 0, sizeof(want));
        want.boot_id = 0x42454E43u; want.tx_sequence = 5u;
        want.antipinch_source_boot = 0x01010101u; want.antipinch_source_sequence = 500u;
        want.antipinch_original_age_ms = 40u; want.antipinch_state = 0u;
        want.occupant_hazard_source_boot = 0x02020202u; want.occupant_hazard_source_sequence = 600u;
        want.occupant_hazard_original_age_ms = 41u; want.occupant_hazard_state = 1u;
        want.rear_obstacle_source_boot = 0x03030303u; want.rear_obstacle_source_sequence = 700u;
        want.rear_obstacle_original_age_ms = 900u; want.rear_obstacle_state = 2u;
        want.rear_detection_activation = 1u; want.activation_meta_format = 1u;
        want.rear_activation_sequence = 800u; want.rear_activation_age_ms = 77u;
        CHECK(net_wire_can_encode_vss_warning_state(PROFILE, &want, &ref) == NET_WIRE_OK);
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(collect_can_bundle(g_text, 0, "VSS_WARNING_STATE", 0x0C0u, 60u, 64u, &env) == 0);
        CHECK(memcmp(env.bytes, ref.bytes, 64u) == 0);
        memset(&got, 0, sizeof(got));
        CHECK(net_wire_can_decode_vss_warning_state(PROFILE, &env, &got) == NET_WIRE_OK);
        EQ(boot_id); EQ(tx_sequence);
        EQ(antipinch_source_boot); EQ(antipinch_source_sequence); EQ(antipinch_original_age_ms);
        EQ(antipinch_state); EQ(antipinch_quality); EQ(antipinch_reason); EQ(antipinch_availability);
        EQ(occupant_hazard_source_boot); EQ(occupant_hazard_source_sequence);
        EQ(occupant_hazard_original_age_ms); EQ(occupant_hazard_state);
        EQ(occupant_hazard_quality); EQ(occupant_hazard_reason); EQ(occupant_hazard_availability);
        EQ(rear_obstacle_source_boot); EQ(rear_obstacle_source_sequence);
        EQ(rear_obstacle_original_age_ms); EQ(rear_obstacle_state); EQ(rear_obstacle_quality);
        EQ(rear_obstacle_reason); EQ(rear_obstacle_availability);
        EQ(rear_detection_activation); EQ(activation_meta_format); EQ(rear_activation_quality);
        EQ(rear_activation_reason); EQ(rear_activation_sequence); EQ(rear_activation_age_ms);
    }
    return 0;
}

#undef EQ

/* ---- UART cases -------------------------------------------------------- */

static int uart_crc_check(void)
{
    const uint8_t ascii[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    uint8_t buf[64];
    uint16_t crc = 0u;
    size_t len, i;
    uint32_t x = 0x12345678u;
    CHECK(net_wire_crc16(ascii, sizeof(ascii), &crc) == NET_WIRE_OK);
    CHECK(crc == 0x29B1u);
    CHECK(reference_crc(ascii, sizeof(ascii)) == 0x29B1u);
    /* Codec CRC equals the 144 algorithm on deterministic pseudo-random data. */
    for (len = 1u; len <= sizeof(buf); ++len) {
        for (i = 0u; i < len; ++i) {
            x = x * 1103515245u + 12345u;
            buf[i] = (uint8_t)(x >> 16);
        }
        CHECK(net_wire_crc16(buf, len, &crc) == NET_WIRE_OK);
        CHECK(crc == reference_crc(buf, len));
    }
    /* A tool-decoded frame reports the same CRC; a flipped CRC is BAD. */
    {
        NetWirePVisionStatusRaw m;
        NetWireUartFrame f;
        char hex[200], want[32];
        memset(&m, 0, sizeof(m));
        m.pi_boot_id = 0x50493031u;
        m.status_sequence = 9u;
        m.vision_status = 1u;
        CHECK(net_wire_p_vision_status_encode(PROFILE, 5u, &m, &f) == NET_WIRE_OK);
        snprintf(want, sizeof(want), "crc: OK 0x%04X",
            (unsigned)reference_crc(f.bytes + 2, f.length - 4u));
        packed_hex(hex, sizeof(hex), "hex=", f.bytes, f.length);
        {
            const char *const argv[] = { "uart", "decode", hex };
            CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
            CHECK(strstr(g_text, want) != NULL);
        }
        f.bytes[f.length - 1u] ^= 0x01u;
        packed_hex(hex, sizeof(hex), "", f.bytes, f.length);
        {
            const char *const argv[] = { "uart", "decode", hex };
            CHECK(run(ARGC(argv), argv) == VSS_BENCH_INVALID);
            CHECK(strstr(g_text, "crc: BAD") != NULL);
        }
    }
    return 0;
}

static int uart_vision_status_command(void)
{
    const char *const argv[] = { "uart", "vision-status", "--boot", "50493031", "--seq", "42",
        "vision_status=2", "status_age_ms=120", "last_fault=4" };
    uint8_t payload[64];
    size_t hex_len;
    NetWireUartFrame f;
    NetWirePVisionStatusRaw got;
    uint16_t seq = 0u;
    CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    CHECK(first_line_has(g_text, "uart profile c1"));
    CHECK(count_lines_with_prefix(g_text, "uart send ") == 1);
    hex_len = uart_send_hex(g_text, "P_VISION_STATUS", payload, sizeof(payload));
    CHECK(hex_len == 32u); /* exactly 16 used bytes, no spaces */
    /* Documented offsets (30_UART_C.md P_VISION_STATUS). */
    CHECK(payload[0] == 0x31u && payload[1] == 0x30u && payload[2] == 0x49u && payload[3] == 0x50u);
    CHECK(payload[4] == 42u && payload[5] == 0u && payload[8] == 2u);
    CHECK(payload[11] == 4u && payload[13] == 120u && payload[14] == 0u);
    /* Rebuild the frame as the 144 does and decode it with the codec. */
    memset(&f, 0, sizeof(f));
    f.bytes[0] = 0xA5u; f.bytes[1] = 0x5Au; f.bytes[2] = 1u; f.bytes[3] = 0x61u;
    put_le(f.bytes + 4, 16u, 2u);
    put_le(f.bytes + 6, 300u, 2u);
    memcpy(f.bytes + 8, payload, 16u);
    put_le(f.bytes + 24, reference_crc(f.bytes + 2, 22u), 2u);
    f.length = 26u;
    CHECK(net_wire_p_vision_status_decode(PROFILE, &f, &got, &seq) == NET_WIRE_OK);
    CHECK(seq == 300u && got.pi_boot_id == 0x50493031u && got.status_sequence == 42u);
    CHECK(got.vision_status == 2u && got.status_age_ms == 120u && got.last_fault == 4u);
    CHECK(console_lines_fit(g_text));
    return 0;
}

static int uart_vision_result_command(void)
{
    const char *const argv[] = { "uart", "vision-result", "--boot", "0x00ABCDEF", "--seq", "7",
        "occupant_count=3", "vision_age_ms=55" };
    uint8_t payload[64];
    NetWireUartFrame f;
    NetWirePVisionResult got;
    uint16_t seq = 0u;
    CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
    CHECK(first_line_has(g_text, "uart profile c1"));
    CHECK(count_lines_with_prefix(g_text, "uart send ") == 1);
    CHECK(uart_send_hex(g_text, "P_VISION_RESULT", payload, sizeof(payload)) == 38u);
    CHECK(payload[0] == 0xEFu && payload[3] == 0x00u && payload[4] == 7u);
    CHECK(payload[8] == 55u && payload[13] == 3u);
    CHECK(net_wire_uart_encode(PROFILE, NET_WIRE_UART_C, NET_WIRE_UPLINK, 0x60u, 1u,
        payload, 19u, &f) == NET_WIRE_OK);
    CHECK(net_wire_p_vision_result_decode(PROFILE, &f, &got, &seq) == NET_WIRE_OK);
    CHECK(got.pi_boot_id == 0x00ABCDEFu && got.vision_sequence == 7u);
    CHECK(got.occupant_count == 3u && got.vision_age_ms == 55u);
    CHECK(got.presence_validity == 1u && got.count_validity == 1u);
    CHECK(console_lines_fit(g_text));
    return 0;
}

static int uart_zero_boot_seq(void)
{
    {
        const char *const argv[] = { "uart", "vision-status", "--boot", "0", "--seq", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(count_lines_with_prefix(g_text, "uart send") == 0);
    }
    {
        const char *const argv[] = { "uart", "vision-result", "--boot", "1", "--seq", "0" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(count_lines_with_prefix(g_text, "uart send") == 0);
    }
    {
        const char *const argv[] = { "uart", "vision-result", "--boot", "1" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    {
        /* Not a documented VALIDITY code: the codec refuses, nothing to send. */
        const char *const argv[] = { "uart", "vision-result", "--boot", "1", "--seq", "1",
            "presence_validity=7" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
        CHECK(count_lines_with_prefix(g_text, "uart send") == 0);
    }
    {
        const char *const argv[] = { "uart", "vision-status", "--boot", "1", "--seq", "1",
            "pi_boot_id=5" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    return 0;
}

static int uart_decode_types(void)
{
    NetWirePVisionResult r;
    NetWirePVisionStatusRaw s;
    NetWirePPermission p;
    NetWireUartFrame f;
    char hex[400];
    memset(&r, 0, sizeof(r));
    r.pi_boot_id = 0x11u; r.vision_sequence = 2u; r.vision_age_ms = 3u;
    r.occupant_presence = 1u; r.presence_validity = 1u; r.occupant_count = 2u;
    r.count_validity = 1u; r.vision_status = 2u;
    CHECK(net_wire_p_vision_result_encode(PROFILE, 0x5AA5u, &r, &f) == NET_WIRE_OK);
    spaced_hex(hex, sizeof(hex), f.bytes, f.length);
    {
        const char *const argv[] = { "uart", "decode", hex };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK); /* one quoted, spaced argument */
        CHECK(strstr(g_text, "codec: OK P_VISION_RESULT") != NULL);
    }
    {
        char *words[64];
        const char *argv[70];
        int n = 0;
        char *tok = strtok(hex, " ");
        argv[0] = "uart"; argv[1] = "decode";
        while (tok != NULL && n < 64) { words[n] = tok; argv[2 + n] = words[n]; ++n; tok = strtok(NULL, " "); }
        CHECK(run(2 + n, argv) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "codec: OK P_VISION_RESULT") != NULL);
        CHECK(strstr(g_text, "link_seq=23205") != NULL);
        CHECK(strstr(g_text, " occupant_count=2 ") != NULL);
    }
    memset(&s, 0, sizeof(s));
    s.pi_boot_id = 0x22u; s.status_sequence = 4u; s.vision_status = 3u; s.active_fault = 2u;
    s.last_fault = 2u; s.affected_function = 3u;
    CHECK(net_wire_p_vision_status_encode(PROFILE, 1u, &s, &f) == NET_WIRE_OK);
    packed_hex(hex, sizeof(hex), "", f.bytes, f.length);
    {
        const char *const argv[] = { "uart", "decode", hex };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "codec: OK P_VISION_STATUS") != NULL);
        CHECK(strstr(g_text, " affected_function=3") != NULL);
    }
    memset(&p, 0, sizeof(p));
    p.cis_boot_id = 0x43495331u; p.update_sequence = 9u; p.vehicle_power_permission = 1u;
    p.use_limit_ms = 1500u; p.source_age_ms = 30u;
    CHECK(net_wire_p_permission_encode(PROFILE, 2u, &p, &f) == NET_WIRE_OK);
    packed_hex(hex, sizeof(hex), "hex=", f.bytes, f.length);
    {
        const char *const argv[] = { "uart", "decode", hex };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "codec: OK P_PERMISSION") != NULL);
        CHECK(strstr(g_text, " use_limit_ms=1500 ") != NULL);
    }
    /* Payload only (15 bytes -> P_PERMISSION). */
    packed_hex(hex, sizeof(hex), "", f.bytes + 8, 15u);
    {
        const char *const argv[] = { "uart", "decode", hex };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_OK);
        CHECK(strstr(g_text, "crc: n/a") != NULL);
        CHECK(strstr(g_text, "codec: OK P_PERMISSION") != NULL);
    }
    /* Valid CRC but an undocumented permission code -> codec INVALID. */
    f.bytes[16] = 7u;
    put_le(f.bytes + f.length - 2u, reference_crc(f.bytes + 2, f.length - 4u), 2u);
    packed_hex(hex, sizeof(hex), "", f.bytes, f.length);
    {
        const char *const argv[] = { "uart", "decode", hex };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_INVALID);
        CHECK(strstr(g_text, "crc: OK") != NULL);
        CHECK(strstr(g_text, "codec: INVALID P_PERMISSION") != NULL);
    }
    /* Unknown TYPE and short frame. */
    {
        uint8_t b[12] = { 0xA5u, 0x5Au, 1u, 0x02u, 2u, 0u, 1u, 0u, 0u, 0u, 0u, 0u };
        put_le(b + 10, reference_crc(b + 2, 8u), 2u);
        packed_hex(hex, sizeof(hex), "", b, sizeof(b));
        {
            const char *const argv[] = { "uart", "decode", hex };
            CHECK(run(ARGC(argv), argv) == VSS_BENCH_INVALID);
            CHECK(strstr(g_text, "not a UART-C message") != NULL);
        }
    }
    {
        const char *const argv[] = { "uart", "decode", "A55A0161" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_INVALID);
    }
    {
        const char *const argv[] = { "uart", "decode", "0102030405" };
        CHECK(run(ARGC(argv), argv) == VSS_BENCH_USAGE);
    }
    return 0;
}

static int hex_parser(void)
{
    uint8_t b[8];
    size_t n = 0u;
    {
        const char *const words[] = { "00:", "0x0a", "data=0B0c", "hex=FF," };
        CHECK(vss_bench_parse_hex(4, words, b, sizeof(b), &n));
        CHECK(n == 4u && b[0] == 0x0Au && b[1] == 0x0Bu && b[2] == 0x0Cu && b[3] == 0xFFu);
    }
    {
        const char *const words[] = { "A5 5A\t01 ", "" };
        CHECK(vss_bench_parse_hex(2, words, b, sizeof(b), &n));
        CHECK(n == 3u && b[0] == 0xA5u && b[1] == 0x5Au && b[2] == 0x01u);
    }
    {
        const char *const words[] = { "ABC" };
        CHECK(!vss_bench_parse_hex(1, words, b, sizeof(b), &n));
    }
    {
        const char *const words[] = { "G0" };
        CHECK(!vss_bench_parse_hex(1, words, b, sizeof(b), &n));
    }
    {
        const char *const words[] = { "000102030405060708" };
        CHECK(!vss_bench_parse_hex(1, words, b, sizeof(b), &n));
    }
    return 0;
}

typedef struct {
    const char *name;
    int (*fn)(void);
} Case;

static const Case cases[] = {
    { "can_event_roundtrip", can_event_roundtrip },
    { "can_permission_roundtrip", can_permission_roundtrip },
    { "can_warning_roundtrip", can_warning_roundtrip },
    { "can_decode_status", can_decode_status },
    { "can_script", can_script },
    { "can_field_errors", can_field_errors },
    { "uart_crc_check", uart_crc_check },
    { "uart_vision_status_command", uart_vision_status_command },
    { "uart_vision_result_command", uart_vision_result_command },
    { "uart_zero_boot_seq", uart_zero_boot_seq },
    { "uart_decode_types", uart_decode_types },
    { "hex_parser", hex_parser },
    { "t_can_named_format", t_can_named_format },
    { "t_can_payload_le", t_can_payload_le },
    { "t_can_script_consistent", t_can_script_consistent },
    { "t_can_roundtrip_named", t_can_roundtrip_named },
};

int main(int argc, char **argv)
{
    size_t i;
    if (argc != 2) {
        fprintf(stderr, "usage: test_bench_tool <case>\n");
        return 2;
    }
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            int rc = cases[i].fn();
            if (rc == 0) printf("PASS %s\n", cases[i].name);
            return rc;
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
