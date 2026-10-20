#include "network/net_upper_can.h"
#include <string.h>
static uint16_t add_age(uint16_t age, uint64_t elapsed)
{ return elapsed >= (uint64_t)(UINT16_MAX - age) ? UINT16_MAX : (uint16_t)(age + elapsed); }
NetWireStatus net_upper_can_encode(NetWireProfile profile, const NetUpperCanMessage *m, NetWireCanEnvelope *out)
{
    if (m == NULL || out == NULL || net_wire_ranges_overlap(m, sizeof(*m), out, sizeof(*out))) return NET_WIRE_INVALID_ARGUMENT;
    switch (m->id) {
    case 0x080u: return net_wire_can_encode_permission(profile, &m->data.domain_permission, out);
    case 0x100u: return net_wire_can_encode_bcm_door_command(profile, &m->data.bcm_door_command, out);
    case 0x110u: return net_wire_can_encode_bcm_climate_command(profile, &m->data.bcm_climate_command, out);
    case 0x120u: return net_wire_can_encode_bcm_light_command(profile, &m->data.bcm_light_command, out);
    case 0x090u: return net_wire_can_encode_window_command(profile, &m->data.window_command, out);
    case 0x200u: return net_wire_can_encode_bcm_door_state(profile, &m->data.bcm_door_state, out);
    case 0x210u: return net_wire_can_encode_bcm_climate_state(profile, &m->data.bcm_climate_state, out);
    case 0x220u: return net_wire_can_encode_bcm_light_state(profile, &m->data.bcm_light_state, out);
    case 0x140u: return net_wire_can_encode_bcm_result(profile, &m->data.bcm_result, out);
    case 0x141u: return net_wire_can_encode_bcm_event(profile, &m->data.bcm_event, out);
    case 0x230u: return net_wire_can_encode_bcm_status(profile, &m->data.bcm_status, out);
    case 0x300u: return net_wire_can_encode_cis_environment(profile, &m->data.cis_environment, out);
    case 0x310u: return net_wire_can_encode_cis_occupant(profile, &m->data.cis_occupant, out);
    case 0x0A0u: return net_wire_can_encode_rear(profile, &m->data.cis_rear, out);
    case 0x320u: return net_wire_can_encode_cis_status(profile, &m->data.cis_status, out);
    case 0x0B0u: return net_wire_can_encode_window_state(profile, &m->data.window_state, out);
    case 0x091u: return net_wire_can_encode_window_event(profile, &m->data.window_event, out);
    case 0x092u: return net_wire_can_encode_window_result(profile, &m->data.window_result, out);
    case 0x330u: return net_wire_can_encode_window_fault(profile, &m->data.window_fault, out);
    case 0x160u: return net_wire_can_encode_vss_event(profile, &m->data.vss_event, out);
    case 0x0C0u: return net_wire_can_encode_vss_warning_state(profile, &m->data.vss_warning_state, out);
    case 0x340u: return net_wire_can_encode_vss_status(profile, &m->data.vss_status, out);
    default: return NET_WIRE_UNSUPPORTED;
    }
}
NetWireStatus net_upper_can_decode(NetWireProfile profile, const NetWireCanEnvelope *frame, NetUpperCanMessage *out)
{
    NetUpperCanMessage local = {0}; NetWireStatus status;
    if (frame == NULL || out == NULL || net_wire_ranges_overlap(frame, sizeof(*frame), out, sizeof(*out))) return NET_WIRE_INVALID_ARGUMENT;
    if (frame->standard_id > UINT16_MAX) return NET_WIRE_MALFORMED;
    local.id = (uint16_t)frame->standard_id;
    switch (local.id) {
    case 0x080u: status = net_wire_can_decode_permission(profile, frame, &local.data.domain_permission); break;
    case 0x100u: status = net_wire_can_decode_bcm_door_command(profile, frame, &local.data.bcm_door_command); break;
    case 0x110u: status = net_wire_can_decode_bcm_climate_command(profile, frame, &local.data.bcm_climate_command); break;
    case 0x120u: status = net_wire_can_decode_bcm_light_command(profile, frame, &local.data.bcm_light_command); break;
    case 0x090u: status = net_wire_can_decode_window_command(profile, frame, &local.data.window_command); break;
    case 0x200u: status = net_wire_can_decode_bcm_door_state(profile, frame, &local.data.bcm_door_state); break;
    case 0x210u: status = net_wire_can_decode_bcm_climate_state(profile, frame, &local.data.bcm_climate_state); break;
    case 0x220u: status = net_wire_can_decode_bcm_light_state(profile, frame, &local.data.bcm_light_state); break;
    case 0x140u: status = net_wire_can_decode_bcm_result(profile, frame, &local.data.bcm_result); break;
    case 0x141u: status = net_wire_can_decode_bcm_event(profile, frame, &local.data.bcm_event); break;
    case 0x230u: status = net_wire_can_decode_bcm_status(profile, frame, &local.data.bcm_status); break;
    case 0x300u: status = net_wire_can_decode_cis_environment(profile, frame, &local.data.cis_environment); break;
    case 0x310u: status = net_wire_can_decode_cis_occupant(profile, frame, &local.data.cis_occupant); break;
    case 0x0A0u: status = net_wire_can_decode_rear(profile, frame, &local.data.cis_rear); break;
    case 0x320u: status = net_wire_can_decode_cis_status(profile, frame, &local.data.cis_status); break;
    case 0x0B0u: status = net_wire_can_decode_window_state(profile, frame, &local.data.window_state); break;
    case 0x091u: status = net_wire_can_decode_window_event(profile, frame, &local.data.window_event); break;
    case 0x092u: status = net_wire_can_decode_window_result(profile, frame, &local.data.window_result); break;
    case 0x330u: status = net_wire_can_decode_window_fault(profile, frame, &local.data.window_fault); break;
    case 0x160u: status = net_wire_can_decode_vss_event(profile, frame, &local.data.vss_event); break;
    case 0x0C0u: status = net_wire_can_decode_vss_warning_state(profile, frame, &local.data.vss_warning_state); break;
    case 0x340u: status = net_wire_can_decode_vss_status(profile, frame, &local.data.vss_status); break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    if (status == NET_WIRE_OK) *out = local;
    return status;
}
NetWireStatus net_upper_can_add_age(NetUpperCanMessage *m, uint64_t elapsed)
{
    if (m == NULL) return NET_WIRE_INVALID_ARGUMENT;
    switch (m->id) {
    case 0x080u:
        m->data.domain_permission.source_age_ms = add_age(m->data.domain_permission.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x100u:
        m->data.bcm_door_command.command_age_ms = add_age(m->data.bcm_door_command.command_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x110u:
        m->data.bcm_climate_command.command_age_ms = add_age(m->data.bcm_climate_command.command_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x120u:
        m->data.bcm_light_command.command_age_ms = add_age(m->data.bcm_light_command.command_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x090u:
        m->data.window_command.command_age_ms = add_age(m->data.window_command.command_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x200u:
        m->data.bcm_door_state.source_age_ms = add_age(m->data.bcm_door_state.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x210u:
        m->data.bcm_climate_state.source_age_ms = add_age(m->data.bcm_climate_state.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x220u:
        m->data.bcm_light_state.source_age_ms = add_age(m->data.bcm_light_state.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x140u:
        m->data.bcm_result.event_age_ms = add_age(m->data.bcm_result.event_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x141u:
        m->data.bcm_event.event_age_ms = add_age(m->data.bcm_event.event_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x230u:
        m->data.bcm_status.source_age_ms = add_age(m->data.bcm_status.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x300u:
        m->data.cis_environment.temperature_age_ms = add_age(m->data.cis_environment.temperature_age_ms, elapsed);
        m->data.cis_environment.humidity_age_ms = add_age(m->data.cis_environment.humidity_age_ms, elapsed);
        m->data.cis_environment.illuminance_age_ms = add_age(m->data.cis_environment.illuminance_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x310u:
        m->data.cis_occupant.vision_age_ms = add_age(m->data.cis_occupant.vision_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x0A0u:
        m->data.cis_rear.source_age_ms = add_age(m->data.cis_rear.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x320u:
        m->data.cis_status.source_age_ms = add_age(m->data.cis_status.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x0B0u:
        m->data.window_state.source_age_ms = add_age(m->data.window_state.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x091u:
        m->data.window_event.event_age_ms = add_age(m->data.window_event.event_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x092u:
        return NET_WIRE_OK;
    case 0x330u:
        m->data.window_fault.source_age_ms = add_age(m->data.window_fault.source_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x160u:
        m->data.vss_event.original_age_ms = add_age(m->data.vss_event.original_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x0C0u:
        m->data.vss_warning_state.antipinch_original_age_ms = add_age(m->data.vss_warning_state.antipinch_original_age_ms, elapsed);
        m->data.vss_warning_state.occupant_hazard_original_age_ms = add_age(m->data.vss_warning_state.occupant_hazard_original_age_ms, elapsed);
        m->data.vss_warning_state.rear_obstacle_original_age_ms = add_age(m->data.vss_warning_state.rear_obstacle_original_age_ms, elapsed);
        if (m->data.vss_warning_state.activation_meta_format == 1u)
            m->data.vss_warning_state.rear_activation_age_ms = add_age(m->data.vss_warning_state.rear_activation_age_ms, elapsed);
        return NET_WIRE_OK;
    case 0x340u:
        if (m->data.vss_status.last_fault_quality == 0u)
            m->data.vss_status.last_fault_age_ms = add_age(m->data.vss_status.last_fault_age_ms, elapsed);
        m->data.vss_status.source_age_ms = add_age(m->data.vss_status.source_age_ms, elapsed);
        return NET_WIRE_OK;
    default: return NET_WIRE_UNSUPPORTED;
    }
}
NetWireStatus net_upper_can_identity(const NetUpperCanMessage *m, uint32_t *boot, uint32_t *sequence)
{
    uint32_t b, s;
    if (m == NULL || boot == NULL || sequence == NULL ||
        net_wire_ranges_overlap(m,sizeof(*m),boot,sizeof(*boot)) ||
        net_wire_ranges_overlap(m,sizeof(*m),sequence,sizeof(*sequence)) ||
        net_wire_ranges_overlap(boot,sizeof(*boot),sequence,sizeof(*sequence))) return NET_WIRE_INVALID_ARGUMENT;
    switch(m->id) {
    case 0x080u: b=m->data.domain_permission.boot_id; s=m->data.domain_permission.tx_sequence; break;
    case 0x100u: b=m->data.bcm_door_command.boot_id; s=m->data.bcm_door_command.tx_sequence; break;
    case 0x110u: b=m->data.bcm_climate_command.boot_id; s=m->data.bcm_climate_command.tx_sequence; break;
    case 0x120u: b=m->data.bcm_light_command.boot_id; s=m->data.bcm_light_command.tx_sequence; break;
    case 0x090u: b=m->data.window_command.boot_id; s=m->data.window_command.tx_sequence; break;
    case 0x200u: b=m->data.bcm_door_state.boot_id; s=m->data.bcm_door_state.tx_sequence; break;
    case 0x210u: b=m->data.bcm_climate_state.boot_id; s=m->data.bcm_climate_state.tx_sequence; break;
    case 0x220u: b=m->data.bcm_light_state.boot_id; s=m->data.bcm_light_state.tx_sequence; break;
    case 0x140u: b=m->data.bcm_result.boot_id; s=m->data.bcm_result.tx_sequence; break;
    case 0x141u: b=m->data.bcm_event.boot_id; s=m->data.bcm_event.tx_sequence; break;
    case 0x230u: b=m->data.bcm_status.boot_id; s=m->data.bcm_status.tx_sequence; break;
    case 0x300u: b=m->data.cis_environment.boot_id; s=m->data.cis_environment.tx_sequence; break;
    case 0x310u: b=m->data.cis_occupant.boot_id; s=m->data.cis_occupant.tx_sequence; break;
    case 0x0A0u: b=m->data.cis_rear.boot_id; s=m->data.cis_rear.tx_sequence; break;
    case 0x320u: b=m->data.cis_status.boot_id; s=m->data.cis_status.tx_sequence; break;
    case 0x0B0u: b=m->data.window_state.boot_id; s=m->data.window_state.tx_sequence; break;
    case 0x091u: b=m->data.window_event.boot_id; s=m->data.window_event.tx_sequence; break;
    case 0x092u: b=m->data.window_result.boot_id; s=m->data.window_result.tx_sequence; break;
    case 0x330u: b=m->data.window_fault.boot_id; s=m->data.window_fault.tx_sequence; break;
    case 0x160u: b=m->data.vss_event.boot_id; s=m->data.vss_event.tx_sequence; break;
    case 0x0C0u: b=m->data.vss_warning_state.boot_id; s=m->data.vss_warning_state.tx_sequence; break;
    case 0x340u: b=m->data.vss_status.boot_id; s=m->data.vss_status.tx_sequence; break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    *boot=b; *sequence=s; return NET_WIRE_OK;
}
uint16_t net_upper_can_max_age(const NetUpperCanMessage *m)
{
    uint16_t age = 0u;
    if (m == NULL) return UINT16_MAX;
    switch (m->id) {
    case 0x080u:
        if (m->data.domain_permission.source_age_ms > age) age = m->data.domain_permission.source_age_ms;
        return age;
    case 0x100u:
        if (m->data.bcm_door_command.command_age_ms > age) age = m->data.bcm_door_command.command_age_ms;
        return age;
    case 0x110u:
        if (m->data.bcm_climate_command.command_age_ms > age) age = m->data.bcm_climate_command.command_age_ms;
        return age;
    case 0x120u:
        if (m->data.bcm_light_command.command_age_ms > age) age = m->data.bcm_light_command.command_age_ms;
        return age;
    case 0x090u:
        if (m->data.window_command.command_age_ms > age) age = m->data.window_command.command_age_ms;
        return age;
    case 0x200u:
        if (m->data.bcm_door_state.source_age_ms > age) age = m->data.bcm_door_state.source_age_ms;
        return age;
    case 0x210u:
        if (m->data.bcm_climate_state.source_age_ms > age) age = m->data.bcm_climate_state.source_age_ms;
        return age;
    case 0x220u:
        if (m->data.bcm_light_state.source_age_ms > age) age = m->data.bcm_light_state.source_age_ms;
        return age;
    case 0x140u:
        if (m->data.bcm_result.event_age_ms > age) age = m->data.bcm_result.event_age_ms;
        return age;
    case 0x141u:
        if (m->data.bcm_event.event_age_ms > age) age = m->data.bcm_event.event_age_ms;
        return age;
    case 0x230u:
        if (m->data.bcm_status.source_age_ms > age) age = m->data.bcm_status.source_age_ms;
        return age;
    case 0x300u:
        if (m->data.cis_environment.temperature_age_ms > age) age = m->data.cis_environment.temperature_age_ms;
        if (m->data.cis_environment.humidity_age_ms > age) age = m->data.cis_environment.humidity_age_ms;
        if (m->data.cis_environment.illuminance_age_ms > age) age = m->data.cis_environment.illuminance_age_ms;
        return age;
    case 0x310u:
        if (m->data.cis_occupant.vision_age_ms > age) age = m->data.cis_occupant.vision_age_ms;
        return age;
    case 0x0A0u:
        if (m->data.cis_rear.source_age_ms > age) age = m->data.cis_rear.source_age_ms;
        return age;
    case 0x320u:
        if (m->data.cis_status.source_age_ms > age) age = m->data.cis_status.source_age_ms;
        return age;
    case 0x0B0u:
        if (m->data.window_state.source_age_ms > age) age = m->data.window_state.source_age_ms;
        return age;
    case 0x091u:
        if (m->data.window_event.event_age_ms > age) age = m->data.window_event.event_age_ms;
        return age;
    case 0x092u:
        return age;
    case 0x330u:
        if (m->data.window_fault.source_age_ms > age) age = m->data.window_fault.source_age_ms;
        return age;
    case 0x160u:
        if (m->data.vss_event.original_age_ms > age) age = m->data.vss_event.original_age_ms;
        return age;
    case 0x0C0u:
        if (m->data.vss_warning_state.antipinch_original_age_ms > age) age = m->data.vss_warning_state.antipinch_original_age_ms;
        if (m->data.vss_warning_state.occupant_hazard_original_age_ms > age) age = m->data.vss_warning_state.occupant_hazard_original_age_ms;
        if (m->data.vss_warning_state.rear_obstacle_original_age_ms > age) age = m->data.vss_warning_state.rear_obstacle_original_age_ms;
        if (m->data.vss_warning_state.rear_activation_age_ms > age) age = m->data.vss_warning_state.rear_activation_age_ms;
        return age;
    case 0x340u:
        /* Freshness = current service evaluation only. last_fault_age_ms is a
         * history age that legitimately saturates at 65535 ~65 s after a fault
         * (nodes/VSS.md status-flow step 4, B16-VSS-04); using it here made every
         * later VSS_STATUS TX/RX EXPIRED. Both ages are still advanced
         * independently by net_upper_can_add_age(). */
        if (m->data.vss_status.source_age_ms > age) age = m->data.vss_status.source_age_ms;
        return age;
    default: return UINT16_MAX;
    }
}
