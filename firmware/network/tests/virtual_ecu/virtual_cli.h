#ifndef NETWORK_TEST_VIRTUAL_CLI_H
#define NETWORK_TEST_VIRTUAL_CLI_H
/* Line protocol helpers shared by the HAL-less node workers (virtual_node.c,
 * virtual_peer.c). Test code only: text <-> numbers and bytes, JSON output. */
#include "network/net_types.h"
#include "network/net_upper.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline const char *vcli_status(NetStatus s)
{
    static const char *names[] = {"OK", "INVALID_ARGUMENT", "NO_CAPACITY", "NOT_CONFIGURED", "UNSUPPORTED",
                                  "HOLD", "NO_WORK", "EXPIRED", "REJECTED", "DUPLICATE", "STALE", "BUSY"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "UNKNOWN";
}

static inline void vcli_hex(const uint8_t *bytes, size_t n)
{
    size_t i;
    putchar('"');
    for (i = 0u; i < n; ++i) printf("%02x", (unsigned)bytes[i]);
    putchar('"');
}

/* Even-length hex text -> bytes. SIZE_MAX on a bad digit or too long input. */
static inline size_t vcli_from_hex(const char *hex, uint8_t *out, size_t capacity)
{
    size_t n = strlen(hex), i;
    if (n % 2u != 0u || n / 2u > capacity) return SIZE_MAX;
    for (i = 0u; i < n / 2u; ++i) {
        unsigned j, value = 0u;
        for (j = 0u; j < 2u; ++j) {
            char c = hex[2u * i + j];
            unsigned digit;
            if (c >= '0' && c <= '9') digit = (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (unsigned)(c - 'a') + 10u;
            else if (c >= 'A' && c <= 'F') digit = (unsigned)(c - 'A') + 10u;
            else return SIZE_MAX;
            value = value * 16u + digit;
        }
        out[i] = (uint8_t)value;
    }
    return n / 2u;
}

static inline bool vcli_number(const char *text, uint64_t *out)
{
    char *end;
    unsigned long long value;
    if (text == NULL || *text < '0' || *text > '9') return false;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != 0) return false;
    *out = (uint64_t)value;
    return true;
}

/* Wire output of the virtual backend: one JSON line per frame on the wire. */
static inline void vcli_wire(const char *node, const NetUpperPacket *p, uint64_t at)
{
    printf("{\"event\":\"wire\",\"node\":\"%s\",\"at\":%llu,\"link\":%u,", node, (unsigned long long)at,
           (unsigned)p->link);
    if (p->link == NET_WIRE_CAN) {
        unsigned flags = (p->can.fdf ? 1u : 0u) | (p->can.brs ? 2u : 0u) | (p->can.extended ? 4u : 0u) |
                         (p->can.rtr ? 8u : 0u) | (p->can.driver_error ? 16u : 0u) | (p->can.esi ? 32u : 0u);
        printf("\"id\":%u,\"dlc\":%u,\"flags\":%u,\"hex\":", (unsigned)p->can.standard_id, (unsigned)p->can.dlc,
               flags);
        vcli_hex(p->can.bytes, p->can.length);
    } else {
        printf("\"type\":%u,\"hex\":", (unsigned)p->uart.bytes[3]);
        vcli_hex(p->uart.bytes, p->uart.length);
    }
    puts("}");
}

#endif
