#include "vss_bench.h"

#include "network/app/net_if_common.h"
#include "network/app/net_if_vss.h"
#include "network/net_wire.h"
#include "network/net_wire_can.h"
#include "network/net_wire_uart.h"
#include "network/net_wire_uart_c_status.h"
#include "network/net_wire_uart_dto.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROFILE NET_WIRE_CANDIDATE_ONLY
/* S32K144 debug console limits (one staged data line, one input line). */
#define CONSOLE_DATA_CHUNK 16u
/* Bench DOMAIN BOOT_ID used by the canned scripts ("DOM1"). */
#define SCRIPT_BOOT 0x444F4D31u

/* ---- text output ------------------------------------------------------- */

void vss_bench_text_init(VssBenchText *text, char *storage, size_t capacity)
{
    text->data = storage;
    text->capacity = capacity;
    text->length = 0u;
    text->truncated = false;
    if (capacity > 0u) storage[0] = '\0';
}

#if defined(__GNUC__)
static void say(VssBenchText *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#endif
static void say(VssBenchText *t, const char *fmt, ...)
{
    va_list ap;
    int n;
    size_t room;
    if (t->capacity == 0u || t->length >= t->capacity - 1u) {
        t->truncated = true;
        return;
    }
    room = t->capacity - t->length;
    va_start(ap, fmt);
    n = vsnprintf(t->data + t->length, room, fmt, ap);
    va_end(ap);
    if (n < 0) {
        t->truncated = true;
        t->data[t->length] = '\0';
    } else if ((size_t)n >= room) {
        t->truncated = true;
        t->length = t->capacity - 1u;
    } else {
        t->length += (size_t)n;
    }
}

static void say_bytes(VssBenchText *t, const uint8_t *b, size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) say(t, i == 0u ? "%02X" : " %02X", b[i]);
}

static const char *status_name(NetWireStatus s)
{
    switch (s) {
    case NET_WIRE_OK: return "OK";
    case NET_WIRE_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case NET_WIRE_NOT_BOUND_STATUS: return "NOT_BOUND";
    case NET_WIRE_MALFORMED: return "MALFORMED";
    case NET_WIRE_UNSUPPORTED: return "UNSUPPORTED";
    case NET_WIRE_CONTRACT_HOLD: return "CONTRACT_HOLD";
    case NET_WIRE_NEED_MORE: return "NEED_MORE";
    case NET_WIRE_NO_CAPACITY: return "NO_CAPACITY";
    case NET_WIRE_TIME_HOLD: return "TIME_HOLD";
    default: return "UNKNOWN";
    }
}

/* ---- number and hex parsing ------------------------------------------- */

static bool parse_number(const char *s, int base, uint32_t *out)
{
    char *end = NULL;
    unsigned long long v;
    if (s == NULL || *s == '\0' || *s == '-' || *s == '+' || *s == ' ') return false;
    v = strtoull(s, &end, base);
    if (end == s || *end != '\0' || v > 0xFFFFFFFFull) return false;
    *out = (uint32_t)v;
    return true;
}

static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* One whitespace-free word. */
static bool parse_hex_word(const char *p, size_t len, uint8_t *out, size_t capacity, size_t *n)
{
    size_t i;
    for (i = len; i > 0u; --i) {
        if (p[i - 1u] == '=') { /* "data=..", "hex=.." */
            p += i;
            len -= i;
            break;
        }
    }
    while (len > 0u && p[len - 1u] == ',') --len;
    if (len == 0u) return true;
    if (p[len - 1u] == ':') return true; /* offset label "00:" */
    if (len >= 2u && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        len -= 2u;
        if (len == 0u) return false;
    }
    if (len % 2u != 0u) return false;
    for (i = 0u; i < len; i += 2u) {
        int hi = nibble(p[i]), lo = nibble(p[i + 1u]);
        if (hi < 0 || lo < 0 || *n >= capacity) return false;
        out[(*n)++] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool vss_bench_parse_hex(int argc, const char *const *argv,
    uint8_t *out, size_t capacity, size_t *length)
{
    size_t n = 0u;
    int a;
    for (a = 0; a < argc; ++a) {
        const char *p = argv[a];
        while (*p != '\0') { /* a quoted argument may hold several words */
            size_t len = 0u;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
            while (p[len] != '\0' && p[len] != ' ' && p[len] != '\t' &&
                   p[len] != '\r' && p[len] != '\n') ++len;
            if (len > 0u && !parse_hex_word(p, len, out, capacity, &n)) return false;
            p += len;
        }
    }
    *length = n;
    return true;
}

/* ---- field tables ------------------------------------------------------ */

typedef struct {
    const char *name;
    size_t offset;
    uint8_t width; /* 1, 2 or 4 bytes; every bench field is unsigned */
    bool settable; /* identity fields come from --boot/--tx/--seq only */
    bool hex;
} Field;

#define F(type, m, set) { #m, offsetof(type, m), (uint8_t)sizeof(((type *)0)->m), set, false }
#define FH(type, m, set) { #m, offsetof(type, m), (uint8_t)sizeof(((type *)0)->m), set, true }
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t field_get(const void *raw, const Field *f)
{
    const uint8_t *p = (const uint8_t *)raw + f->offset;
    if (f->width == 1u) return *p;
    if (f->width == 2u) { uint16_t v; memcpy(&v, p, 2u); return v; }
    { uint32_t v; memcpy(&v, p, 4u); return v; }
}

static void field_put(void *raw, const Field *f, uint32_t value)
{
    uint8_t *p = (uint8_t *)raw + f->offset;
    if (f->width == 1u) { *p = (uint8_t)value; return; }
    if (f->width == 2u) { uint16_t v = (uint16_t)value; memcpy(p, &v, 2u); return; }
    memcpy(p, &value, 4u);
}

static uint32_t field_max(const Field *f)
{
    return f->width == 1u ? 0xFFu : f->width == 2u ? 0xFFFFu : 0xFFFFFFFFu;
}

static void say_fields(VssBenchText *t, const char *prefix, const void *raw,
    const Field *fields, size_t count)
{
    size_t i;
    say(t, "%s", prefix);
    for (i = 0u; i < count; ++i) {
        uint32_t v = field_get(raw, &fields[i]);
        if (fields[i].hex) say(t, " %s=0x%08lX", fields[i].name, (unsigned long)v);
        else say(t, " %s=%lu", fields[i].name, (unsigned long)v);
    }
    say(t, "\n");
}

static void say_field_names(VssBenchText *t, const Field *fields, size_t count)
{
    size_t i;
    say(t, "  settable fields:");
    for (i = 0u; i < count; ++i)
        if (fields[i].settable) say(t, " %s", fields[i].name);
    say(t, "\n");
}

/* Applies "name=value" words. Returns false after printing the reason. */
static bool apply_fields(VssBenchText *t, int argc, const char *const *argv,
    void *raw, const Field *fields, size_t count)
{
    int a;
    for (a = 0; a < argc; ++a) {
        const char *eq = strchr(argv[a], '=');
        size_t i, nlen;
        uint32_t v;
        const Field *f = NULL;
        if (eq == NULL || eq == argv[a]) {
            say(t, "error: expected field=value, got '%s'\n", argv[a]);
            say_field_names(t, fields, count);
            return false;
        }
        nlen = (size_t)(eq - argv[a]);
        for (i = 0u; i < count; ++i)
            if (strlen(fields[i].name) == nlen && strncmp(fields[i].name, argv[a], nlen) == 0)
                f = &fields[i];
        if (f == NULL) {
            say(t, "error: unknown field '%.*s'\n", (int)nlen, argv[a]);
            say_field_names(t, fields, count);
            return false;
        }
        if (!f->settable) {
            say(t, "error: field '%s' is set by --boot/--tx/--seq\n", f->name);
            return false;
        }
        if (!parse_number(eq + 1, 0, &v) || v > field_max(f)) {
            say(t, "error: value '%s' for %s is not a %u-byte unsigned number\n",
                eq + 1, f->name, (unsigned)f->width);
            return false;
        }
        field_put(raw, f, v);
    }
    return true;
}

/* ---- CAN messages ------------------------------------------------------ */

typedef union {
    NetWireDomainPermissionRaw permission;
    NetWireVssEventRaw event;
    NetWireVssWarningStateRaw warning;
    NetWireVssStatusRaw status;
} CanRaw;

static const Field permission_fields[] = {
    FH(NetWireDomainPermissionRaw, boot_id, false),
    F(NetWireDomainPermissionRaw, tx_sequence, false),
    F(NetWireDomainPermissionRaw, vehicle_power_permission, true),
    F(NetWireDomainPermissionRaw, power_permission_quality, true),
    F(NetWireDomainPermissionRaw, power_operation_state, true),
    F(NetWireDomainPermissionRaw, operation_allowed, true),
    F(NetWireDomainPermissionRaw, operation_quality, true),
    F(NetWireDomainPermissionRaw, update_sequence, true),
    F(NetWireDomainPermissionRaw, source_age_ms, true),
};

static const Field event_fields[] = {
    FH(NetWireVssEventRaw, boot_id, false),
    F(NetWireVssEventRaw, tx_sequence, false),
    F(NetWireVssEventRaw, event_kind, true),
    F(NetWireVssEventRaw, occurrence_id, true),
    F(NetWireVssEventRaw, original_age_ms, true),
    F(NetWireVssEventRaw, use_limit_ms, true),
    F(NetWireVssEventRaw, value_quality, true),
    F(NetWireVssEventRaw, input_availability, true),
    FH(NetWireVssEventRaw, related_command_id, true),
};

static const Field warning_fields[] = {
    FH(NetWireVssWarningStateRaw, boot_id, false),
    F(NetWireVssWarningStateRaw, tx_sequence, false),
    FH(NetWireVssWarningStateRaw, antipinch_source_boot, true),
    F(NetWireVssWarningStateRaw, antipinch_source_sequence, true),
    F(NetWireVssWarningStateRaw, antipinch_original_age_ms, true),
    F(NetWireVssWarningStateRaw, antipinch_state, true),
    F(NetWireVssWarningStateRaw, antipinch_quality, true),
    F(NetWireVssWarningStateRaw, antipinch_reason, true),
    F(NetWireVssWarningStateRaw, antipinch_availability, true),
    FH(NetWireVssWarningStateRaw, occupant_hazard_source_boot, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_source_sequence, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_original_age_ms, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_state, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_quality, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_reason, true),
    F(NetWireVssWarningStateRaw, occupant_hazard_availability, true),
    FH(NetWireVssWarningStateRaw, rear_obstacle_source_boot, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_source_sequence, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_original_age_ms, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_state, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_quality, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_reason, true),
    F(NetWireVssWarningStateRaw, rear_obstacle_availability, true),
    F(NetWireVssWarningStateRaw, rear_detection_activation, true),
    F(NetWireVssWarningStateRaw, activation_meta_format, true),
    F(NetWireVssWarningStateRaw, rear_activation_quality, true),
    F(NetWireVssWarningStateRaw, rear_activation_reason, true),
    F(NetWireVssWarningStateRaw, rear_activation_sequence, true),
    F(NetWireVssWarningStateRaw, rear_activation_age_ms, true),
};

static const Field status_fields[] = {
    FH(NetWireVssStatusRaw, boot_id, false),
    F(NetWireVssStatusRaw, tx_sequence, false),
    F(NetWireVssStatusRaw, vss_state, false),
    F(NetWireVssStatusRaw, vss_availability, false),
    F(NetWireVssStatusRaw, vss_accepting_events, false),
    F(NetWireVssStatusRaw, vss_fault_active, false),
    F(NetWireVssStatusRaw, fault_mask, false),
    F(NetWireVssStatusRaw, vss_last_fault, false),
    F(NetWireVssStatusRaw, last_fault_occurrence, false),
    F(NetWireVssStatusRaw, last_fault_age_ms, false),
    F(NetWireVssStatusRaw, last_fault_quality, false),
    F(NetWireVssStatusRaw, update_sequence, false),
    F(NetWireVssStatusRaw, source_age_ms, false),
};

/* Defaults are documented-valid values (docs/network/interface/
 * 03_COMMUNICATION/10_CAN_FD.md); BOOT_ID/TX_SEQUENCE come from the caller. */
static void permission_defaults(CanRaw *r)
{
    NetWireDomainPermissionRaw *p = &r->permission;
    p->vehicle_power_permission = NET_TRISTATE_YES;
    p->power_permission_quality = NET_QUALITY_OK;
    p->power_operation_state = NET_TRISTATE_YES;
    p->operation_allowed = NET_FLAG_YES;
    p->operation_quality = NET_QUALITY_OK;
    p->update_sequence = 1u;
    p->source_age_ms = 10u;
}

static void event_defaults(CanRaw *r)
{
    NetWireVssEventRaw *p = &r->event;
    p->event_kind = NET_VSS_EVENT_WELCOME;
    p->occurrence_id = 1u;
    p->original_age_ms = 10u;
    p->use_limit_ms = 500u;
    p->value_quality = NET_QUALITY_OK;
    p->input_availability = NET_AVAILABILITY_AVAILABLE;
    p->related_command_id = 0u;
}

static void warning_defaults(CanRaw *r)
{
    NetWireVssWarningStateRaw *w = &r->warning;
    w->antipinch_source_boot = 0x57494E31u;
    w->antipinch_source_sequence = 11u;
    w->antipinch_original_age_ms = 120u;
    w->antipinch_state = NET_FLAG_YES;
    w->antipinch_quality = NET_QUALITY_OK;
    w->antipinch_reason = NET_QUALITY_REASON_NONE;
    w->antipinch_availability = NET_AVAILABILITY_AVAILABLE;
    w->occupant_hazard_source_boot = 0x43495331u;
    w->occupant_hazard_source_sequence = 22u;
    w->occupant_hazard_original_age_ms = 230u;
    w->occupant_hazard_state = NET_FLAG_NO;
    w->occupant_hazard_quality = NET_QUALITY_OK;
    w->occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    w->occupant_hazard_availability = NET_AVAILABILITY_AVAILABLE;
    w->rear_obstacle_source_boot = 0x43495332u;
    w->rear_obstacle_source_sequence = 33u;
    w->rear_obstacle_original_age_ms = 340u;
    w->rear_obstacle_state = NET_REAR_WARNING_CAUTION;
    w->rear_obstacle_quality = NET_QUALITY_OK;
    w->rear_obstacle_reason = NET_QUALITY_REASON_NONE;
    w->rear_obstacle_availability = NET_AVAILABILITY_AVAILABLE;
    w->rear_detection_activation = NET_FLAG_YES;
    w->activation_meta_format = 1u;
    w->rear_activation_quality = NET_QUALITY_OK;
    w->rear_activation_reason = NET_QUALITY_REASON_NONE;
    w->rear_activation_sequence = 44u;
    w->rear_activation_age_ms = 55u;
}

static NetWireStatus permission_encode(const CanRaw *r, NetWireCanEnvelope *e)
{ return net_wire_can_encode_permission(PROFILE, &r->permission, e); }
static NetWireStatus permission_decode(const NetWireCanEnvelope *e, CanRaw *r)
{ return net_wire_can_decode_permission(PROFILE, e, &r->permission); }
static NetWireStatus event_encode(const CanRaw *r, NetWireCanEnvelope *e)
{ return net_wire_can_encode_vss_event(PROFILE, &r->event, e); }
static NetWireStatus event_decode(const NetWireCanEnvelope *e, CanRaw *r)
{ return net_wire_can_decode_vss_event(PROFILE, e, &r->event); }
static NetWireStatus warning_encode(const CanRaw *r, NetWireCanEnvelope *e)
{ return net_wire_can_encode_vss_warning_state(PROFILE, &r->warning, e); }
static NetWireStatus warning_decode(const NetWireCanEnvelope *e, CanRaw *r)
{ return net_wire_can_decode_vss_warning_state(PROFILE, e, &r->warning); }

typedef struct {
    const char *word;
    uint16_t id;
    const Field *fields;
    size_t field_count;
    void (*defaults)(CanRaw *r);
    NetWireStatus (*encode)(const CanRaw *r, NetWireCanEnvelope *e);
    NetWireStatus (*decode)(const NetWireCanEnvelope *e, CanRaw *r);
} CanMsg;

static const CanMsg can_msgs[] = {
    { "perm", 0x080u, permission_fields, COUNT(permission_fields),
      permission_defaults, permission_encode, permission_decode },
    { "event", 0x160u, event_fields, COUNT(event_fields),
      event_defaults, event_encode, event_decode },
    { "warning", 0x0C0u, warning_fields, COUNT(warning_fields),
      warning_defaults, warning_encode, warning_decode },
};

static const CanMsg *find_can(const char *word)
{
    size_t i;
    for (i = 0u; i < COUNT(can_msgs); ++i)
        if (strcmp(can_msgs[i].word, word) == 0) return &can_msgs[i];
    return NULL;
}

/* BOOT_ID and TX_SEQUENCE share offsets 0 and 4 in every CAN DTO. */
static void put_identity(const Field *fields, void *raw, uint32_t boot, uint32_t seq)
{
    field_put(raw, &fields[0], boot);
    field_put(raw, &fields[1], seq);
}

/* Builds the frame and prints the payload and the 144 command bundle. */
static int emit_can(VssBenchText *t, const CanMsg *m, uint32_t boot, uint32_t tx,
    int argc, const char *const *argv)
{
    CanRaw raw, back;
    NetWireCanEnvelope env;
    NetWireStatus st;
    const NetWireRegistryEntry *reg = net_wire_registry_find(NET_WIRE_CAN, m->id);
    size_t i;
    if (reg == NULL) {
        say(t, "error: 0x%03X is not in the wire registry\n", (unsigned)m->id);
        return VSS_BENCH_USAGE;
    }
    memset(&raw, 0, sizeof(raw));
    m->defaults(&raw);
    put_identity(m->fields, &raw, boot, tx);
    if (!apply_fields(t, argc, argv, &raw, m->fields, m->field_count)) return VSS_BENCH_USAGE;
    memset(&env, 0, sizeof(env));
    st = m->encode(&raw, &env);
    if (st != NET_WIRE_OK) {
        say(t, "error: codec rejected the %s values (%s); nothing to send\n",
            reg->name, status_name(st));
        return VSS_BENCH_USAGE;
    }
    if (env.standard_id != m->id || env.length != reg->frame_length || !env.fdf) {
        say(t, "error: codec frame shape differs from the registry\n");
        return VSS_BENCH_USAGE;
    }
    for (i = reg->payload_length; i < env.length; ++i) {
        if (env.bytes[i] != 0u) {
            say(t, "error: padding byte %u is not zero\n", (unsigned)i);
            return VSS_BENCH_USAGE;
        }
    }
    memset(&back, 0, sizeof(back));
    st = m->decode(&env, &back);
    if (st != NET_WIRE_OK) {
        say(t, "error: codec cannot decode its own %s frame (%s)\n", reg->name, status_name(st));
        return VSS_BENCH_USAGE;
    }
    say(t, "# %s id=0x%03X payload=%u B frame=%u B (zero padded)\n", reg->name,
        (unsigned)m->id, (unsigned)reg->payload_length, (unsigned)reg->frame_length);
    say(t, "# NETBENCH: the 144 accepts this arm only after its NETBENCH send extension "
           "is approved and flashed; stock firmware rejects registry IDs\n");
    say(t, "# payload: ");
    say_bytes(t, env.bytes, reg->payload_length);
    say(t, "\n");
    say_fields(t, "# fields:", &back, m->fields, m->field_count);
    say(t, "can tx arm 0x%03X NETBENCH\n", (unsigned)m->id);
    say(t, "can tx begin 0x%03X fd %s %u\n", (unsigned)m->id, env.brs ? "brs" : "nobrs",
        (unsigned)env.length);
    for (i = 0u; i < env.length; i += CONSOLE_DATA_CHUNK) {
        size_t n = env.length - i < CONSOLE_DATA_CHUNK ? env.length - i : CONSOLE_DATA_CHUNK;
        say(t, "can tx data ");
        say_bytes(t, env.bytes + i, n);
        say(t, "\n");
    }
    say(t, "can tx commit\n");
    return VSS_BENCH_OK;
}

/* Parses "--boot <hex> --tx|--seq <n>" and returns the index of the first
 * remaining word, or -1 after printing the reason. */
static int parse_identity(VssBenchText *t, int argc, const char *const *argv,
    const char *seq_option, uint32_t *boot, uint32_t *seq)
{
    bool have_boot = false, have_seq = false;
    int a = 0;
    while (a < argc && strncmp(argv[a], "--", 2u) == 0) {
        if (a + 1 >= argc) {
            say(t, "error: %s needs a value\n", argv[a]);
            return -1;
        }
        if (strcmp(argv[a], "--boot") == 0) {
            const char *v = argv[a + 1];
            if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) v += 2;
            if (strlen(v) > 8u || !parse_number(v, 16, boot)) {
                say(t, "error: --boot '%s' is not a 32-bit hex number\n", argv[a + 1]);
                return -1;
            }
            have_boot = true;
        } else if (strcmp(argv[a], seq_option) == 0) {
            if (!parse_number(argv[a + 1], 0, seq)) {
                say(t, "error: %s '%s' is not a 32-bit number\n", seq_option, argv[a + 1]);
                return -1;
            }
            have_seq = true;
        } else {
            say(t, "error: unknown option %s\n", argv[a]);
            return -1;
        }
        a += 2;
    }
    if (!have_boot || !have_seq) {
        say(t, "error: --boot <hex> and %s <n> are required\n", seq_option);
        return -1;
    }
    return a;
}

static int can_generate(VssBenchText *t, const CanMsg *m, int argc, const char *const *argv)
{
    uint32_t boot = 0u, tx = 0u;
    int rest = parse_identity(t, argc, argv, "--tx", &boot, &tx);
    if (rest < 0) return VSS_BENCH_USAGE;
    /* BOOT_ID 0 is reserved and TX_SEQUENCE starts at 1
     * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md, common header). */
    if (boot == 0u || tx == 0u) {
        say(t, "error: BOOT_ID 0 is reserved and TX_SEQUENCE starts at 1\n");
        return VSS_BENCH_USAGE;
    }
    return emit_can(t, m, boot, tx, argc - rest, argv + rest);
}

static int can_decode_status(VssBenchText *t, int argc, const char *const *argv)
{
    uint8_t bytes[64];
    size_t n = 0u;
    NetWireCanEnvelope env;
    NetWireVssStatusRaw raw;
    NetWireStatus st;
    const NetWireRegistryEntry *reg = net_wire_registry_find(NET_WIRE_CAN, 0x340u);
    if (reg == NULL) return VSS_BENCH_USAGE;
    if (!vss_bench_parse_hex(argc, argv, bytes, sizeof(bytes), &n) || n == 0u) {
        say(t, "error: expected the 0x340 data bytes in hex\n");
        return VSS_BENCH_USAGE;
    }
    if (n != reg->payload_length && n != reg->frame_length) {
        say(t, "error: VSS_STATUS has %u used / %u frame bytes, got %u\n",
            (unsigned)reg->payload_length, (unsigned)reg->frame_length, (unsigned)n);
        return VSS_BENCH_USAGE;
    }
    memset(&env, 0, sizeof(env));
    env.standard_id = 0x340u;
    env.fdf = true;
    env.brs = true;
    env.dlc = (uint8_t)reg->dlc;
    env.length = reg->frame_length;
    memcpy(env.bytes, bytes, n); /* a used-length copy is zero padded */
    memset(&raw, 0, sizeof(raw));
    st = net_wire_can_decode_vss_status(PROFILE, &env, &raw);
    say(t, "# VSS_STATUS id=0x340 input=%u B frame=%u B\n", (unsigned)n, (unsigned)env.length);
    say(t, "# header: boot_id=0x%02X%02X%02X%02X tx_sequence=%lu\n",
        bytes[3], bytes[2], bytes[1], bytes[0],
        (unsigned long)((uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8) |
                        ((uint32_t)bytes[6] << 16) | ((uint32_t)bytes[7] << 24)));
    if (st != NET_WIRE_OK) {
        say(t, "codec: INVALID (%s)\n", status_name(st));
        return VSS_BENCH_INVALID;
    }
    say(t, "codec: OK\n");
    say_fields(t, "fields:", &raw, status_fields, COUNT(status_fields));
    return VSS_BENCH_OK;
}

/* Canned VSS input scenarios for the board CAN receive test (S1..S4). */
static int can_script(VssBenchText *t, int argc, const char *const *argv)
{
    const char *const occurrence2[] = { "occurrence_id=2" };
    uint32_t boot = SCRIPT_BOOT;
    int a = 0, rc = VSS_BENCH_OK, done = 0;
    if (argc >= 2 && strcmp(argv[0], "--boot") == 0) {
        const char *v = argv[1];
        if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) v += 2;
        if (strlen(v) > 8u || !parse_number(v, 16, &boot) || boot == 0u) {
            say(t, "error: --boot '%s' is not a non-zero 32-bit hex number\n", argv[1]);
            return VSS_BENCH_USAGE;
        }
        a = 2;
    }
    if (a >= argc) {
        say(t, "error: name a scenario: S1 S2 S3 S4 or all\n");
        return VSS_BENCH_USAGE;
    }
    for (; a < argc && rc == VSS_BENCH_OK; ++a) {
        bool all = strcmp(argv[a], "all") == 0;
        bool known = all;
        if (all || strcmp(argv[a], "S1") == 0) {
            known = true;
            say(t, "# ---- S1: VSS_EVENT TX=1 (boot 0x%08lX)\n", (unsigned long)boot);
            say(t, "# expect: g_vss_bench event_count=1, fields equal to the line below\n");
            rc = emit_can(t, find_can("event"), boot, 1u, 0, NULL);
            ++done;
        }
        if (rc == VSS_BENCH_OK && (all || strcmp(argv[a], "S2") == 0)) {
            known = true;
            say(t, "# ---- S2a: exact duplicate of S1 (same TX=1, same bytes)\n");
            say(t, "# expect: no change (event_count stays 1)\n");
            rc = emit_can(t, find_can("event"), boot, 1u, 0, NULL);
            if (rc == VSS_BENCH_OK) {
                /* A new occurrence: a same-occurrence repeat must also age. */
                say(t, "# ---- S2b: VSS_EVENT TX=2, new occurrence_id=2\n");
                say(t, "# expect: event_count=2\n");
                rc = emit_can(t, find_can("event"), boot, 2u, 1, occurrence2);
            }
            ++done;
        }
        if (rc == VSS_BENCH_OK && (all || strcmp(argv[a], "S3") == 0)) {
            known = true;
            say(t, "# ---- S3: VSS_WARNING_STATE TX=1\n");
            say(t, "# expect: warning_count +1\n");
            rc = emit_can(t, find_can("warning"), boot, 1u, 0, NULL);
            ++done;
        }
        if (rc == VSS_BENCH_OK && (all || strcmp(argv[a], "S4") == 0)) {
            known = true;
            say(t, "# ---- S4: DOMAIN_PERMISSION TX=1\n");
            say(t, "# expect: g_net_diag app.permission_received +1, then the "
                   "150 ms receive supervision reports a timeout\n");
            rc = emit_can(t, find_can("perm"), boot, 1u, 0, NULL);
            ++done;
        }
        if (!known) {
            say(t, "error: unknown scenario '%s' (S1 S2 S3 S4 all)\n", argv[a]);
            return VSS_BENCH_USAGE;
        }
    }
    return done > 0 ? rc : VSS_BENCH_USAGE;
}

/* ---- UART-C messages --------------------------------------------------- */

typedef union {
    NetWirePVisionResult result;
    NetWirePVisionStatusRaw status;
    NetWirePPermission permission;
} UartRaw;

static const Field vision_result_fields[] = {
    FH(NetWirePVisionResult, pi_boot_id, false),
    F(NetWirePVisionResult, vision_sequence, false),
    F(NetWirePVisionResult, vision_age_ms, true),
    F(NetWirePVisionResult, occupant_presence, true),
    F(NetWirePVisionResult, presence_validity, true),
    F(NetWirePVisionResult, presence_reason, true),
    F(NetWirePVisionResult, occupant_count, true),
    F(NetWirePVisionResult, count_validity, true),
    F(NetWirePVisionResult, count_reason, true),
    F(NetWirePVisionResult, vision_status, true),
    F(NetWirePVisionResult, vision_fault_code, true),
};

static const Field vision_status_fields[] = {
    FH(NetWirePVisionStatusRaw, pi_boot_id, false),
    F(NetWirePVisionStatusRaw, status_sequence, false),
    F(NetWirePVisionStatusRaw, vision_status, true),
    F(NetWirePVisionStatusRaw, active_fault, true),
    F(NetWirePVisionStatusRaw, last_fault, true),
    F(NetWirePVisionStatusRaw, status_age_ms, true),
    F(NetWirePVisionStatusRaw, affected_function, true),
};

static const Field p_permission_fields[] = {
    FH(NetWirePPermission, cis_boot_id, false),
    F(NetWirePPermission, update_sequence, false),
    F(NetWirePPermission, vehicle_power_permission, false),
    F(NetWirePPermission, value_quality, false),
    F(NetWirePPermission, quality_reason, false),
    F(NetWirePPermission, use_limit_ms, false),
    F(NetWirePPermission, source_age_ms, false),
};

/* Documented-valid defaults (docs/network/interface/03_COMMUNICATION/30_UART_C.md). */
static void vision_result_defaults(UartRaw *r)
{
    NetWirePVisionResult *m = &r->result;
    m->vision_age_ms = 20u;
    m->occupant_presence = 1u; /* present */
    m->presence_validity = 1u; /* VALID */
    m->presence_reason = NET_QUALITY_REASON_NONE;
    m->occupant_count = 1u;
    m->count_validity = 1u;
    m->count_reason = NET_QUALITY_REASON_NONE;
    m->vision_status = NET_SENSOR_FUNCTION_ACTIVE;
    m->vision_fault_code = 0u; /* none */
}

static void vision_status_defaults(UartRaw *r)
{
    NetWirePVisionStatusRaw *m = &r->status;
    m->vision_status = NET_SENSOR_FUNCTION_READY;
    m->active_fault = 0u;
    m->last_fault = 0u;
    m->status_age_ms = 20u;
    m->affected_function = 0u;
}

static NetWireStatus vision_result_encode(uint16_t seq, const UartRaw *r, NetWireUartFrame *f)
{ return net_wire_p_vision_result_encode(PROFILE, seq, &r->result, f); }
static NetWireStatus vision_result_decode(const NetWireUartFrame *f, UartRaw *r, uint16_t *seq)
{ return net_wire_p_vision_result_decode(PROFILE, f, &r->result, seq); }
static NetWireStatus vision_status_encode(uint16_t seq, const UartRaw *r, NetWireUartFrame *f)
{ return net_wire_p_vision_status_encode(PROFILE, seq, &r->status, f); }
static NetWireStatus vision_status_decode(const NetWireUartFrame *f, UartRaw *r, uint16_t *seq)
{ return net_wire_p_vision_status_decode(PROFILE, f, &r->status, seq); }
static NetWireStatus p_permission_decode(const NetWireUartFrame *f, UartRaw *r, uint16_t *seq)
{ return net_wire_p_permission_decode(PROFILE, f, &r->permission, seq); }

typedef struct {
    const char *word; /* NULL: decode only */
    uint8_t type;
    NetWireDirection direction;
    const Field *fields;
    size_t field_count;
    void (*defaults)(UartRaw *r);
    NetWireStatus (*encode)(uint16_t seq, const UartRaw *r, NetWireUartFrame *f);
    NetWireStatus (*decode)(const NetWireUartFrame *f, UartRaw *r, uint16_t *seq);
} UartMsg;

static const UartMsg uart_msgs[] = {
    { "vision-result", 0x60u, NET_WIRE_UPLINK, vision_result_fields, COUNT(vision_result_fields),
      vision_result_defaults, vision_result_encode, vision_result_decode },
    { "vision-status", 0x61u, NET_WIRE_UPLINK, vision_status_fields, COUNT(vision_status_fields),
      vision_status_defaults, vision_status_encode, vision_status_decode },
    { NULL, 0x62u, NET_WIRE_DOWNLINK, p_permission_fields, COUNT(p_permission_fields),
      NULL, NULL, p_permission_decode },
};

static const UartMsg *find_uart_word(const char *word)
{
    size_t i;
    for (i = 0u; i < COUNT(uart_msgs); ++i)
        if (uart_msgs[i].word != NULL && strcmp(uart_msgs[i].word, word) == 0) return &uart_msgs[i];
    return NULL;
}

static const UartMsg *find_uart_type(uint8_t type)
{
    size_t i;
    for (i = 0u; i < COUNT(uart_msgs); ++i)
        if (uart_msgs[i].type == type) return &uart_msgs[i];
    return NULL;
}

static int uart_generate(VssBenchText *t, const UartMsg *m, int argc, const char *const *argv)
{
    uint32_t boot = 0u, seq = 0u;
    UartRaw raw, back;
    NetWireUartFrame frame;
    NetWireStatus st;
    uint16_t link_seq = 0u;
    size_t i;
    const NetWireRegistryEntry *reg = net_wire_registry_find(NET_WIRE_UART_C, m->type);
    int rest = parse_identity(t, argc, argv, "--seq", &boot, &seq);
    if (reg == NULL) return VSS_BENCH_USAGE;
    if (rest < 0) return VSS_BENCH_USAGE;
    /* The 144 named send rejects a zero BOOT or a zero sequence word. */
    if (boot == 0u || seq == 0u) {
        say(t, "error: BOOT and SEQ must not be 0 (the 144 rejects the send)\n");
        return VSS_BENCH_USAGE;
    }
    memset(&raw, 0, sizeof(raw));
    m->defaults(&raw);
    put_identity(m->fields, &raw, boot, seq);
    if (!apply_fields(t, argc - rest, argv + rest, &raw, m->fields, m->field_count))
        return VSS_BENCH_USAGE;
    memset(&frame, 0, sizeof(frame));
    st = m->encode(1u, &raw, &frame);
    if (st != NET_WIRE_OK) {
        say(t, "error: codec rejected the %s values (%s); nothing to send\n",
            reg->name, status_name(st));
        return VSS_BENCH_USAGE;
    }
    memset(&back, 0, sizeof(back));
    st = m->decode(&frame, &back, &link_seq);
    if (st != NET_WIRE_OK || frame.length != reg->frame_length) {
        say(t, "error: codec cannot decode its own %s frame (%s)\n", reg->name, status_name(st));
        return VSS_BENCH_USAGE;
    }
    say(t, "# FIRST on the 144: uart profile c1  (a HEX 'uart send' is rejected "
           "unless the profile is c1)\n");
    say(t, "# %s type=0x%02X payload=%u B frame=%u B\n", reg->name, (unsigned)m->type,
        (unsigned)reg->payload_length, (unsigned)reg->frame_length);
    say(t, "# payload: ");
    say_bytes(t, frame.bytes + 8, reg->payload_length);
    say(t, "\n");
    say_fields(t, "# fields:", &back, m->fields, m->field_count);
    say(t, "# the 144 adds SYNC, VERSION, TYPE, LEN, its own LINK_SEQ and CRC16; "
           "reference frame with LINK_SEQ=1:\n# ");
    say_bytes(t, frame.bytes, frame.length);
    say(t, "\n");
    say(t, "uart send %s ", reg->name);
    for (i = 0u; i < reg->payload_length; ++i) say(t, "%02X", frame.bytes[8u + i]);
    say(t, "\n");
    return VSS_BENCH_OK;
}

static int uart_decode(VssBenchText *t, int argc, const char *const *argv)
{
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    size_t n = 0u;
    NetWireUartFrame frame;
    const UartMsg *m;
    const NetWireRegistryEntry *reg;
    UartRaw raw;
    uint16_t seq = 0u;
    NetWireStatus st;
    bool crc_ok = true;
    if (!vss_bench_parse_hex(argc, argv, bytes, sizeof(bytes), &n) || n == 0u) {
        say(t, "error: expected a UART-C frame (A5 5A ...) or payload in hex\n");
        return VSS_BENCH_USAGE;
    }
    memset(&frame, 0, sizeof(frame));
    if (n >= 2u && bytes[0] == 0xA5u && bytes[1] == 0x5Au) {
        uint16_t len, stored, computed = 0u;
        if (n < 10u) {
            say(t, "frame: INVALID (%u bytes is shorter than the 10-byte frame overhead)\n",
                (unsigned)n);
            return VSS_BENCH_INVALID;
        }
        len = (uint16_t)(bytes[4] | (bytes[5] << 8));
        say(t, "# frame: %u B version=%u type=0x%02X len=%u link_seq=%u\n", (unsigned)n,
            (unsigned)bytes[2], (unsigned)bytes[3], (unsigned)len,
            (unsigned)(bytes[6] | (bytes[7] << 8)));
        if ((size_t)len + 10u != n) {
            say(t, "frame: INVALID (LEN %u needs %u bytes, got %u)\n", (unsigned)len,
                (unsigned)len + 10u, (unsigned)n);
            return VSS_BENCH_INVALID;
        }
        stored = (uint16_t)(bytes[n - 2u] | (bytes[n - 1u] << 8));
        if (net_wire_crc16(bytes + 2, n - 4u, &computed) != NET_WIRE_OK) return VSS_BENCH_USAGE;
        crc_ok = stored == computed;
        if (crc_ok) say(t, "crc: OK 0x%04X\n", (unsigned)computed);
        else say(t, "crc: BAD stored=0x%04X computed=0x%04X\n", (unsigned)stored, (unsigned)computed);
        m = find_uart_type(bytes[3]);
        if (m == NULL) {
            say(t, "codec: INVALID (type 0x%02X is not a UART-C message)\n", (unsigned)bytes[3]);
            return VSS_BENCH_INVALID;
        }
        frame.length = n;
        memcpy(frame.bytes, bytes, n);
    } else {
        uint8_t type;
        if (n == 19u) type = 0x60u;
        else if (n == 16u) type = 0x61u;
        else if (n == 15u) type = 0x62u;
        else {
            say(t, "error: %u payload bytes match no UART-C type (19, 16, 15)\n", (unsigned)n);
            return VSS_BENCH_USAGE;
        }
        m = find_uart_type(type);
        st = net_wire_uart_encode(PROFILE, NET_WIRE_UART_C, m->direction, type, 1u,
            bytes, n, &frame);
        if (st != NET_WIRE_OK) {
            say(t, "codec: INVALID (%s)\n", status_name(st));
            return VSS_BENCH_INVALID;
        }
        say(t, "# payload only: type 0x%02X inferred from %u bytes\n", (unsigned)type, (unsigned)n);
        say(t, "crc: n/a (payload only)\n");
    }
    reg = net_wire_registry_find(NET_WIRE_UART_C, m->type);
    memset(&raw, 0, sizeof(raw));
    st = m->decode(&frame, &raw, &seq);
    if (st != NET_WIRE_OK) {
        say(t, "codec: INVALID %s (%s)\n", reg != NULL ? reg->name : "?", status_name(st));
        return VSS_BENCH_INVALID;
    }
    say(t, "codec: OK %s\n", reg != NULL ? reg->name : "?");
    say_fields(t, "fields:", &raw, m->fields, m->field_count);
    return crc_ok ? VSS_BENCH_OK : VSS_BENCH_INVALID;
}

/* ---- command line ------------------------------------------------------ */

static void usage(VssBenchText *t)
{
    say(t,
        "usage:\n"
        "  vss_bench can perm|event|warning --boot <hex> --tx <n> [field=value ...]\n"
        "  vss_bench can decode-status <hex bytes of 0x340>\n"
        "  vss_bench can script [--boot <hex>] S1|S2|S3|S4|all ...\n"
        "  vss_bench uart vision-status|vision-result --boot <hex> --seq <n> [field=value ...]\n"
        "  vss_bench uart decode <hex frame A5 5A ... or payload>\n");
}

int vss_bench_run(int argc, const char *const *argv, VssBenchText *out)
{
    if (argc >= 2 && strcmp(argv[0], "can") == 0) {
        const CanMsg *m = find_can(argv[1]);
        if (m != NULL) return can_generate(out, m, argc - 2, argv + 2);
        if (strcmp(argv[1], "decode-status") == 0) return can_decode_status(out, argc - 2, argv + 2);
        if (strcmp(argv[1], "script") == 0) return can_script(out, argc - 2, argv + 2);
    } else if (argc >= 2 && strcmp(argv[0], "uart") == 0) {
        const UartMsg *m = find_uart_word(argv[1]);
        if (m != NULL) return uart_generate(out, m, argc - 2, argv + 2);
        if (strcmp(argv[1], "decode") == 0) return uart_decode(out, argc - 2, argv + 2);
    }
    usage(out);
    return VSS_BENCH_USAGE;
}
