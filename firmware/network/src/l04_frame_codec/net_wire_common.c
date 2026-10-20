#include "net_wire_internal.h"
NetWireStatus net_wire_profile_check(NetWireProfile profile)
{
    if (profile == NET_WIRE_CANDIDATE_ONLY) return NET_WIRE_OK;
    if (profile == NET_WIRE_NOT_BOUND) return NET_WIRE_NOT_BOUND_STATUS;
    return NET_WIRE_INVALID_ARGUMENT;
}
bool net_wire_ranges_overlap(const void *a, size_t an, const void *b, size_t bn)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (an == 0u || bn == 0u) return false;
    if (a == NULL || b == NULL) return false;
    /* Subtract instead of adding an untrusted length to an address. */
    return x <= y ? y - x < an : x - y < bn;
}
uint16_t nw_read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}
uint32_t nw_read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
           ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}
int16_t nw_read_i16(const uint8_t *p)
{
    uint16_t raw = nw_read_u16(p);
    int32_t value = raw <= INT16_MAX ? (int32_t)raw : (int32_t)raw - INT32_C(65536);
    return (int16_t)value;
}
void nw_write_i16(uint8_t *p, int16_t value)
{
    nw_write_u16(p, (uint16_t)value);
}
uint64_t nw_read_u64(const uint8_t *p)
{
    return (uint64_t)nw_read_u32(p) | ((uint64_t)nw_read_u32(p + 4u) << 32u);
}
void nw_write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8u);
}
void nw_write_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8u);
    p[2] = (uint8_t)(v >> 16u); p[3] = (uint8_t)(v >> 24u);
}
void nw_write_u64(uint8_t *p, uint64_t v)
{
    nw_write_u32(p, (uint32_t)v); nw_write_u32(p + 4u, (uint32_t)(v >> 32u));
}
void nw_sat_inc(uint32_t *counter)
{
    if (*counter < UINT32_MAX) ++*counter;
}
