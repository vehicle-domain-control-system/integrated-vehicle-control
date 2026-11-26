#include "network/net_wire_uart_c_status.h"
#include "net_wire_uart_typed_internal.h"

/* FINAL_98 UART-C lines 83..89, common BOOT rule line 66, and Pi Data
 * NetVisionStatus: state 0..4, fault 0..4, affected bit0/bit1 only.
 * No cross-field state/fault policy or observation freshness is synthesized. */
static NetWireStatus vision_status_check(const NetWirePVisionStatusRaw *m)
{
    if (m->pi_boot_id == 0U || m->vision_status > 4U ||
        m->active_fault > 4U || m->last_fault > 4U ||
        (m->affected_function & 0xfcU) != 0U)
        return NET_WIRE_MALFORMED;
    return NET_WIRE_OK;
}

NetWireStatus net_wire_p_vision_status_encode(NetWireProfile profile,
    uint16_t sequence, const NetWirePVisionStatusRaw *m, NetWireUartFrame *out)
{
    uint8_t p[16] = {0};
    NetWireStatus status = nw_uart_typed_encode_args(profile, m, sizeof(*m), out);
    if (status != NET_WIRE_OK) return status;
    status = vision_status_check(m);
    if (status != NET_WIRE_OK) return status;
    nw_write_u32(p, m->pi_boot_id);
    nw_write_u32(p + 4, m->status_sequence);
    p[8] = m->vision_status;
    nw_write_u16(p + 9, m->active_fault);
    nw_write_u16(p + 11, m->last_fault);
    nw_write_u16(p + 13, m->status_age_ms);
    p[15] = m->affected_function;
    return net_wire_uart_encode(profile, NET_WIRE_UART_C, NET_WIRE_UPLINK,
        0x61U, sequence, p, sizeof(p), out);
}

NetWireStatus net_wire_p_vision_status_decode(NetWireProfile profile,
    const NetWireUartFrame *frame, NetWirePVisionStatusRaw *out,
    uint16_t *sequence_out)
{
    NetWireUartPayload payload;
    NetWirePVisionStatusRaw m = {0};
    const uint8_t *p = payload.bytes;
    NetWireStatus status = nw_uart_typed_decode_payload(profile, frame, out,
        sizeof(*out), sequence_out, NET_WIRE_UART_C, NET_WIRE_UPLINK, 0x61U,
        &payload);
    if (status != NET_WIRE_OK) return status;
    m.pi_boot_id = nw_read_u32(p);
    m.status_sequence = nw_read_u32(p + 4);
    m.vision_status = p[8];
    m.active_fault = nw_read_u16(p + 9);
    m.last_fault = nw_read_u16(p + 11);
    m.status_age_ms = nw_read_u16(p + 13);
    m.affected_function = p[15];
    status = vision_status_check(&m);
    if (status != NET_WIRE_OK) return status;
    *out = m;
    *sequence_out = payload.sequence;
    return NET_WIRE_OK;
}
