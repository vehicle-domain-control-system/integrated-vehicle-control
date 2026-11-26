#include "network/net_upper_uart.h"
#include "../../l04_frame_codec/net_wire_internal.h"
#include <string.h>

static bool uart_link(NetWireLink link)
{
    return link == NET_WIRE_UART_M || link == NET_WIRE_UART_C;
}
static bool recognized(const NetUpperUartMessage *message)
{
    return message != NULL && uart_link(message->link) &&
        net_wire_registry_find(message->link, message->type) != NULL;
}
NetWireStatus net_upper_uart_encode(NetWireProfile profile,
    const NetUpperUartMessage *message, NetWireUartFrame *out)
{
    if (message == NULL || out == NULL ||
        net_wire_ranges_overlap(message, sizeof(*message), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    if (!recognized(message)) return NET_WIRE_UNSUPPORTED;
    switch (message->type) {
    case 0x01u: return net_wire_m_context_encode(profile, message->sequence,
        &message->data.m_context, out);
    case 0x10u: return net_wire_m_request_encode(profile, message->sequence,
        &message->data.m_request, out);
    case 0x11u: return net_wire_m_query_encode(profile, message->sequence,
        &message->data.m_query, out);
    case 0x12u: return net_wire_m_warning_ack_encode(profile, message->sequence,
        &message->data.m_warning_ack, out);
    case 0x02u: return net_wire_m_domain_alive_encode(profile, message->sequence,
        &message->data.m_domain_alive, out);
    case 0x20u: return net_wire_m_result_encode(profile, message->sequence,
        &message->data.m_result, out);
    case 0x21u: return net_wire_m_warning_encode(profile, message->sequence,
        &message->data.m_warning, out);
    case 0x22u: return net_wire_m_availability_encode(profile, message->sequence,
        &message->data.m_availability, out);
    case 0x23u: return net_wire_m_digital_status_encode(profile, message->sequence,
        &message->data.m_digital_status, out);
    case 0x24u: return net_wire_m_digital_result_encode(profile, message->sequence,
        &message->data.m_digital_result, out);
    case 0x25u: return net_wire_m_user_settings_encode(profile, message->sequence,
        &message->data.m_user_settings, out);
    case 0x40u: return net_wire_m_query_end_encode(profile, message->sequence,
        &message->data.m_query_end, out);
    case 0x30u: return net_wire_m_bcm_door_state_encode(profile, message->sequence,
        &message->data.m_bcm_door_state, out);
    case 0x31u: return net_wire_m_bcm_climate_state_encode(profile, message->sequence,
        &message->data.m_bcm_climate_state, out);
    case 0x32u: return net_wire_m_bcm_light_state_encode(profile, message->sequence,
        &message->data.m_bcm_light_state, out);
    case 0x33u: return net_wire_m_bcm_status_encode(profile, message->sequence,
        &message->data.m_bcm_status, out);
    case 0x34u: return net_wire_m_cis_environment_encode(profile, message->sequence,
        &message->data.m_cis_environment, out);
    case 0x35u: return net_wire_m_cis_occupant_encode(profile, message->sequence,
        &message->data.m_cis_occupant, out);
    case 0x36u: return net_wire_m_cis_rear_encode(profile, message->sequence,
        &message->data.m_cis_rear, out);
    case 0x37u: return net_wire_m_cis_status_encode(profile, message->sequence,
        &message->data.m_cis_status, out);
    case 0x3Au: return net_wire_m_vss_status_encode(profile, message->sequence,
        &message->data.m_vss_status, out);
    case 0x38u: return net_wire_m_window_state_encode(profile, message->sequence,
        &message->data.m_window_state, out);
    case 0x39u: return net_wire_m_window_fault_encode(profile, message->sequence,
        &message->data.m_window_fault, out);
    case 0x60u: return net_wire_p_vision_result_encode(profile, message->sequence,
        &message->data.p_vision_result, out);
    case 0x61u: return net_wire_p_vision_status_encode(profile, message->sequence,
        &message->data.p_vision_status, out);
    case 0x62u: return net_wire_p_permission_encode(profile, message->sequence,
        &message->data.p_permission, out);
    default: return NET_WIRE_UNSUPPORTED;
    }
}
NetWireStatus net_upper_uart_decode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, const NetWireUartFrame *frame,
    NetUpperUartMessage *out)
{
    NetUpperUartMessage work = {0};
    NetWireUartPayload payload;
    NetWireStatus status;
    if (frame == NULL || out == NULL ||
        net_wire_ranges_overlap(frame, sizeof(*frame), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = net_wire_uart_decode(profile, link, direction, frame, &payload);
    if (status != NET_WIRE_OK) return status;
    work.link = link; work.type = payload.type;
    switch (work.type) {
    case 0x01u: status = net_wire_m_context_decode(profile, frame,
        &work.data.m_context, &work.sequence); break;
    case 0x10u: status = net_wire_m_request_decode(profile, frame,
        &work.data.m_request, &work.sequence); break;
    case 0x11u: status = net_wire_m_query_decode(profile, frame,
        &work.data.m_query, &work.sequence); break;
    case 0x12u: status = net_wire_m_warning_ack_decode(profile, frame,
        &work.data.m_warning_ack, &work.sequence); break;
    case 0x02u: status = net_wire_m_domain_alive_decode(profile, frame,
        &work.data.m_domain_alive, &work.sequence); break;
    case 0x20u: status = net_wire_m_result_decode(profile, frame,
        &work.data.m_result, &work.sequence); break;
    case 0x21u: status = net_wire_m_warning_decode(profile, frame,
        &work.data.m_warning, &work.sequence); break;
    case 0x22u: status = net_wire_m_availability_decode(profile, frame,
        &work.data.m_availability, &work.sequence); break;
    case 0x23u: status = net_wire_m_digital_status_decode(profile, frame,
        &work.data.m_digital_status, &work.sequence); break;
    case 0x24u: status = net_wire_m_digital_result_decode(profile, frame,
        &work.data.m_digital_result, &work.sequence); break;
    case 0x25u: status = net_wire_m_user_settings_decode(profile, frame,
        &work.data.m_user_settings, &work.sequence); break;
    case 0x40u: status = net_wire_m_query_end_decode(profile, frame,
        &work.data.m_query_end, &work.sequence); break;
    case 0x30u: status = net_wire_m_bcm_door_state_decode(profile, frame,
        &work.data.m_bcm_door_state, &work.sequence); break;
    case 0x31u: status = net_wire_m_bcm_climate_state_decode(profile, frame,
        &work.data.m_bcm_climate_state, &work.sequence); break;
    case 0x32u: status = net_wire_m_bcm_light_state_decode(profile, frame,
        &work.data.m_bcm_light_state, &work.sequence); break;
    case 0x33u: status = net_wire_m_bcm_status_decode(profile, frame,
        &work.data.m_bcm_status, &work.sequence); break;
    case 0x34u: status = net_wire_m_cis_environment_decode(profile, frame,
        &work.data.m_cis_environment, &work.sequence); break;
    case 0x35u: status = net_wire_m_cis_occupant_decode(profile, frame,
        &work.data.m_cis_occupant, &work.sequence); break;
    case 0x36u: status = net_wire_m_cis_rear_decode(profile, frame,
        &work.data.m_cis_rear, &work.sequence); break;
    case 0x37u: status = net_wire_m_cis_status_decode(profile, frame,
        &work.data.m_cis_status, &work.sequence); break;
    case 0x3Au: status = net_wire_m_vss_status_decode(profile, frame,
        &work.data.m_vss_status, &work.sequence); break;
    case 0x38u: status = net_wire_m_window_state_decode(profile, frame,
        &work.data.m_window_state, &work.sequence); break;
    case 0x39u: status = net_wire_m_window_fault_decode(profile, frame,
        &work.data.m_window_fault, &work.sequence); break;
    case 0x60u: status = net_wire_p_vision_result_decode(profile, frame,
        &work.data.p_vision_result, &work.sequence); break;
    case 0x61u: status = net_wire_p_vision_status_decode(profile, frame,
        &work.data.p_vision_status, &work.sequence); break;
    case 0x62u: status = net_wire_p_permission_decode(profile, frame,
        &work.data.p_permission, &work.sequence); break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    if (status == NET_WIRE_OK) *out = work;
    return status;
}
static uint16_t add16(uint16_t age, uint64_t elapsed)
{
    return elapsed >= (uint64_t)(UINT16_MAX - age) ? UINT16_MAX :
        (uint16_t)((uint64_t)age + elapsed);
}
NetWireStatus net_upper_uart_add_age(NetUpperUartMessage *message,
    uint64_t elapsed_ms)
{
    if (message == NULL) return NET_WIRE_INVALID_ARGUMENT;
    if (!recognized(message)) return NET_WIRE_UNSUPPORTED;
    switch (message->type) {
    case 0x01u:
        message->data.m_context.app_age_ms = add16(message->data.m_context.app_age_ms, elapsed_ms);
        message->data.m_context.proximity_age_ms = add16(message->data.m_context.proximity_age_ms, elapsed_ms);
        message->data.m_context.registration_connection_age_ms = add16(message->data.m_context.registration_connection_age_ms, elapsed_ms);
        break;
    case 0x10u:
        break;
    case 0x11u:
        break;
    case 0x12u:
        break;
    case 0x02u:
        message->data.m_domain_alive.source_age_ms = add16(message->data.m_domain_alive.source_age_ms, elapsed_ms);
        break;
    case 0x20u:
        message->data.m_result.result_age_ms = add16(message->data.m_result.result_age_ms, elapsed_ms);
        break;
    case 0x21u:
        message->data.m_warning.source_age_ms = add16(message->data.m_warning.source_age_ms, elapsed_ms);
        break;
    case 0x22u:
        message->data.m_availability.source_age_ms = add16(message->data.m_availability.source_age_ms, elapsed_ms);
        break;
    case 0x23u:
        message->data.m_digital_status.source_age_ms = add16(message->data.m_digital_status.source_age_ms, elapsed_ms);
        break;
    case 0x24u:
        message->data.m_digital_result.result_age_ms = add16(message->data.m_digital_result.result_age_ms, elapsed_ms);
        break;
    case 0x25u:
        message->data.m_user_settings.source_age_ms = add16(message->data.m_user_settings.source_age_ms, elapsed_ms);
        break;
    case 0x40u:
        break;
    case 0x30u:
        message->data.m_bcm_door_state.source_age_ms = add16(message->data.m_bcm_door_state.source_age_ms, elapsed_ms);
        break;
    case 0x31u:
        message->data.m_bcm_climate_state.source_age_ms = add16(message->data.m_bcm_climate_state.source_age_ms, elapsed_ms);
        break;
    case 0x32u:
        message->data.m_bcm_light_state.source_age_ms = add16(message->data.m_bcm_light_state.source_age_ms, elapsed_ms);
        break;
    case 0x33u:
        message->data.m_bcm_status.source_age_ms = add16(message->data.m_bcm_status.source_age_ms, elapsed_ms);
        break;
    case 0x34u:
        message->data.m_cis_environment.temperature_age_ms = add16(message->data.m_cis_environment.temperature_age_ms, elapsed_ms);
        message->data.m_cis_environment.humidity_age_ms = add16(message->data.m_cis_environment.humidity_age_ms, elapsed_ms);
        message->data.m_cis_environment.illuminance_age_ms = add16(message->data.m_cis_environment.illuminance_age_ms, elapsed_ms);
        break;
    case 0x35u:
        message->data.m_cis_occupant.vision_age_ms = add16(message->data.m_cis_occupant.vision_age_ms, elapsed_ms);
        break;
    case 0x36u:
        message->data.m_cis_rear.source_age_ms = add16(message->data.m_cis_rear.source_age_ms, elapsed_ms);
        break;
    case 0x37u:
        message->data.m_cis_status.source_age_ms = add16(message->data.m_cis_status.source_age_ms, elapsed_ms);
        break;
    case 0x3Au:
        /* quality3 is a confirmed absence tuple, not an ageing observation. */
        if (message->data.m_vss_status.last_fault_quality == 0u)
            message->data.m_vss_status.last_fault_age_ms = add16(message->data.m_vss_status.last_fault_age_ms, elapsed_ms);
        message->data.m_vss_status.source_age_ms = add16(message->data.m_vss_status.source_age_ms, elapsed_ms);
        break;
    case 0x38u:
        message->data.m_window_state.source_age_ms = add16(message->data.m_window_state.source_age_ms, elapsed_ms);
        break;
    case 0x39u:
        message->data.m_window_fault.source_age_ms = add16(message->data.m_window_fault.source_age_ms, elapsed_ms);
        break;
    case 0x60u:
        message->data.p_vision_result.vision_age_ms = add16(message->data.p_vision_result.vision_age_ms, elapsed_ms);
        break;
    case 0x61u:
        message->data.p_vision_status.status_age_ms = add16(message->data.p_vision_status.status_age_ms, elapsed_ms);
        break;
    case 0x62u:
        message->data.p_permission.source_age_ms = add16(message->data.p_permission.source_age_ms, elapsed_ms);
        break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    return NET_WIRE_OK;
}
uint16_t net_upper_uart_max_age(const NetUpperUartMessage *message)
{
    uint16_t maximum=0u;
    if (!recognized(message)) return UINT16_MAX;
    switch(message->type) {
    case 0x01u:
        if(message->data.m_context.app_age_ms>maximum) maximum=message->data.m_context.app_age_ms;
        if(message->data.m_context.proximity_age_ms>maximum) maximum=message->data.m_context.proximity_age_ms;
        if(message->data.m_context.registration_connection_age_ms>maximum) maximum=message->data.m_context.registration_connection_age_ms;
        break;
    case 0x10u:
        break;
    case 0x11u:
        break;
    case 0x12u:
        break;
    case 0x02u:
        if(message->data.m_domain_alive.source_age_ms>maximum) maximum=message->data.m_domain_alive.source_age_ms;
        break;
    case 0x20u:
        if(message->data.m_result.result_age_ms>maximum) maximum=message->data.m_result.result_age_ms;
        break;
    case 0x21u:
        if(message->data.m_warning.source_age_ms>maximum) maximum=message->data.m_warning.source_age_ms;
        break;
    case 0x22u:
        if(message->data.m_availability.source_age_ms>maximum) maximum=message->data.m_availability.source_age_ms;
        break;
    case 0x23u:
        if(message->data.m_digital_status.source_age_ms>maximum) maximum=message->data.m_digital_status.source_age_ms;
        break;
    case 0x24u:
        if(message->data.m_digital_result.result_age_ms>maximum) maximum=message->data.m_digital_result.result_age_ms;
        break;
    case 0x25u:
        if(message->data.m_user_settings.source_age_ms>maximum) maximum=message->data.m_user_settings.source_age_ms;
        break;
    case 0x40u:
        break;
    case 0x30u:
        if(message->data.m_bcm_door_state.source_age_ms>maximum) maximum=message->data.m_bcm_door_state.source_age_ms;
        break;
    case 0x31u:
        if(message->data.m_bcm_climate_state.source_age_ms>maximum) maximum=message->data.m_bcm_climate_state.source_age_ms;
        break;
    case 0x32u:
        if(message->data.m_bcm_light_state.source_age_ms>maximum) maximum=message->data.m_bcm_light_state.source_age_ms;
        break;
    case 0x33u:
        if(message->data.m_bcm_status.source_age_ms>maximum) maximum=message->data.m_bcm_status.source_age_ms;
        break;
    case 0x34u:
        if(message->data.m_cis_environment.temperature_age_ms>maximum) maximum=message->data.m_cis_environment.temperature_age_ms;
        if(message->data.m_cis_environment.humidity_age_ms>maximum) maximum=message->data.m_cis_environment.humidity_age_ms;
        if(message->data.m_cis_environment.illuminance_age_ms>maximum) maximum=message->data.m_cis_environment.illuminance_age_ms;
        break;
    case 0x35u:
        if(message->data.m_cis_occupant.vision_age_ms>maximum) maximum=message->data.m_cis_occupant.vision_age_ms;
        break;
    case 0x36u:
        if(message->data.m_cis_rear.source_age_ms>maximum) maximum=message->data.m_cis_rear.source_age_ms;
        break;
    case 0x37u:
        if(message->data.m_cis_status.source_age_ms>maximum) maximum=message->data.m_cis_status.source_age_ms;
        break;
    case 0x3Au:
        if(message->data.m_vss_status.last_fault_age_ms>maximum) maximum=message->data.m_vss_status.last_fault_age_ms;
        if(message->data.m_vss_status.source_age_ms>maximum) maximum=message->data.m_vss_status.source_age_ms;
        break;
    case 0x38u:
        if(message->data.m_window_state.source_age_ms>maximum) maximum=message->data.m_window_state.source_age_ms;
        break;
    case 0x39u:
        if(message->data.m_window_fault.source_age_ms>maximum) maximum=message->data.m_window_fault.source_age_ms;
        break;
    case 0x60u:
        if(message->data.p_vision_result.vision_age_ms>maximum) maximum=message->data.p_vision_result.vision_age_ms;
        break;
    case 0x61u:
        if(message->data.p_vision_status.status_age_ms>maximum) maximum=message->data.p_vision_status.status_age_ms;
        break;
    case 0x62u:
        if(message->data.p_permission.source_age_ms>maximum) maximum=message->data.p_permission.source_age_ms;
        break;
    default: return UINT16_MAX;
    }
    return maximum;
}

NetWireStatus net_upper_uart_set_query_context(NetUpperUartMessage *message,
    uint32_t device, uint64_t session, uint32_t domain_boot, uint32_t query_id)
{
    if (message == NULL) return NET_WIRE_INVALID_ARGUMENT;
    if (message->link != NET_WIRE_UART_M || device == 0u || session == 0u ||
        domain_boot == 0u) return NET_WIRE_INVALID_ARGUMENT;
    switch (message->type) {
    case 0x20u:
        message->data.m_result.device_context_id = device;
        message->data.m_result.session_id = session;
        message->data.m_result.domain_boot_id = domain_boot;
        message->data.m_result.query_id = query_id;
        break;
    case 0x21u:
        message->data.m_warning.device_context_id = device;
        message->data.m_warning.session_id = session;
        message->data.m_warning.domain_boot_id = domain_boot;
        message->data.m_warning.query_id = query_id;
        break;
    case 0x22u:
        message->data.m_availability.device_context_id = device;
        message->data.m_availability.session_id = session;
        message->data.m_availability.domain_boot_id = domain_boot;
        message->data.m_availability.query_id = query_id;
        break;
    case 0x23u:
        message->data.m_digital_status.device_context_id = device;
        message->data.m_digital_status.session_id = session;
        message->data.m_digital_status.domain_boot_id = domain_boot;
        message->data.m_digital_status.query_id = query_id;
        break;
    case 0x24u:
        message->data.m_digital_result.device_context_id = device;
        message->data.m_digital_result.session_id = session;
        message->data.m_digital_result.domain_boot_id = domain_boot;
        message->data.m_digital_result.query_id = query_id;
        break;
    case 0x25u:
        message->data.m_user_settings.device_context_id = device;
        message->data.m_user_settings.session_id = session;
        message->data.m_user_settings.domain_boot_id = domain_boot;
        message->data.m_user_settings.query_id = query_id;
        break;
    case 0x30u:
        message->data.m_bcm_door_state.device_context_id = device;
        message->data.m_bcm_door_state.session_id = session;
        message->data.m_bcm_door_state.domain_boot_id = domain_boot;
        message->data.m_bcm_door_state.query_id = query_id;
        break;
    case 0x31u:
        message->data.m_bcm_climate_state.device_context_id = device;
        message->data.m_bcm_climate_state.session_id = session;
        message->data.m_bcm_climate_state.domain_boot_id = domain_boot;
        message->data.m_bcm_climate_state.query_id = query_id;
        break;
    case 0x32u:
        message->data.m_bcm_light_state.device_context_id = device;
        message->data.m_bcm_light_state.session_id = session;
        message->data.m_bcm_light_state.domain_boot_id = domain_boot;
        message->data.m_bcm_light_state.query_id = query_id;
        break;
    case 0x33u:
        message->data.m_bcm_status.device_context_id = device;
        message->data.m_bcm_status.session_id = session;
        message->data.m_bcm_status.domain_boot_id = domain_boot;
        message->data.m_bcm_status.query_id = query_id;
        break;
    case 0x34u:
        message->data.m_cis_environment.device_context_id = device;
        message->data.m_cis_environment.session_id = session;
        message->data.m_cis_environment.domain_boot_id = domain_boot;
        message->data.m_cis_environment.query_id = query_id;
        break;
    case 0x35u:
        message->data.m_cis_occupant.device_context_id = device;
        message->data.m_cis_occupant.session_id = session;
        message->data.m_cis_occupant.domain_boot_id = domain_boot;
        message->data.m_cis_occupant.query_id = query_id;
        break;
    case 0x36u:
        message->data.m_cis_rear.device_context_id = device;
        message->data.m_cis_rear.session_id = session;
        message->data.m_cis_rear.domain_boot_id = domain_boot;
        message->data.m_cis_rear.query_id = query_id;
        break;
    case 0x37u:
        message->data.m_cis_status.device_context_id = device;
        message->data.m_cis_status.session_id = session;
        message->data.m_cis_status.domain_boot_id = domain_boot;
        message->data.m_cis_status.query_id = query_id;
        break;
    case 0x3Au:
        message->data.m_vss_status.device_context_id = device;
        message->data.m_vss_status.session_id = session;
        message->data.m_vss_status.domain_boot_id = domain_boot;
        message->data.m_vss_status.query_id = query_id;
        break;
    case 0x38u:
        message->data.m_window_state.device_context_id = device;
        message->data.m_window_state.session_id = session;
        message->data.m_window_state.domain_boot_id = domain_boot;
        message->data.m_window_state.query_id = query_id;
        break;
    case 0x39u:
        message->data.m_window_fault.device_context_id = device;
        message->data.m_window_fault.session_id = session;
        message->data.m_window_fault.domain_boot_id = domain_boot;
        message->data.m_window_fault.query_id = query_id;
        break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    return NET_WIRE_OK;
}

static void write_le(uint8_t *p, uint64_t value, size_t count)
{
    size_t i;
    for (i = 0u; i < count; ++i) p[i] = (uint8_t)(value >> (8u * i));
}
NetWireStatus net_upper_uart_project_can(NetWireProfile profile,
    const NetUpperCanMessage *can, uint32_t device, uint64_t session,
    uint32_t domain_boot, uint32_t query_id, uint16_t link_sequence,
    NetUpperUartMessage *out)
{
    NetWireCanEnvelope original;
    NetWireUartFrame frame;
    NetWireStatus status;
    const NetWireRegistryEntry *entry;
    uint8_t type, payload[NET_WIRE_UART_PAYLOAD_MAX] = {0};
    if (can == NULL || out == NULL || device == 0u || session == 0u || domain_boot == 0u ||
        net_wire_ranges_overlap(can, sizeof(*can), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    switch (can->id) {
    case 0x200u: type = 0x30u; break;
    case 0x210u: type = 0x31u; break;
    case 0x220u: type = 0x32u; break;
    case 0x230u: type = 0x33u; break;
    case 0x300u: type = 0x34u; break;
    case 0x310u: type = 0x35u; break;
    case 0x0a0u: type = 0x36u; break;
    case 0x320u: type = 0x37u; break;
    case 0x340u: type = 0x3au; break;
    case 0x0b0u: type = 0x38u; break;
    case 0x330u: type = 0x39u; break;
    default: return NET_WIRE_UNSUPPORTED;
    }
    entry = net_wire_registry_find(NET_WIRE_CAN, can->id);
    if (entry == NULL || entry->payload_length + 20u > sizeof(payload))
        return NET_WIRE_UNSUPPORTED;
    status = net_upper_can_encode(profile, can, &original);
    if (status != NET_WIRE_OK) return status;
    write_le(payload, device, 4u); write_le(payload + 4u, session, 8u);
    write_le(payload + 12u, domain_boot, 4u); write_le(payload + 16u, query_id, 4u);
    /* All eleven frozen schemas are DOWN_CONTEXT20 + original CAN used bytes.
     * No CAN padding, fabricated generation, re-aged observation or unit loss. */
    memcpy(payload + 20u, original.bytes, entry->payload_length);
    status = net_wire_uart_encode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK,
        type, link_sequence, payload, entry->payload_length + 20u, &frame);
    if (status != NET_WIRE_OK) return status;
    return net_upper_uart_decode(profile, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, &frame, out);
}

static void increment(uint32_t *value)
{
    if (*value != UINT32_MAX) ++*value;
}
NetWireStatus net_upper_uart_receiver_init(NetUpperUartReceiver *receiver,
    NetWireProfile profile, NetWireLink link, NetWireDirection direction,
    const NetWireStreamPolicy *policy, uint64_t port_epoch)
{
    NetWireStatus status;
    if (receiver == NULL || policy == NULL || port_epoch == 0u ||
        net_wire_ranges_overlap(receiver, sizeof(*receiver), policy, sizeof(*policy)))
        return NET_WIRE_INVALID_ARGUMENT;
    status = nw_uart_stream_config_check(profile, link, direction, policy);
    if (status != NET_WIRE_OK) return status;
    /* Pure preflight is complete, policy is disjoint, and no callback occurs. */
    memset(receiver,0,sizeof(*receiver));
    (void)net_wire_uart_stream_init(&receiver->stream,profile,link,direction,policy);
    receiver->port_epoch = port_epoch; receiver->initialized = true;
    return NET_WIRE_OK;
}
/* Ring position of the i-th oldest history entry (head, i < FRAME_MAX). */
static size_t history_at(const NetUpperUartReceiver *receiver, size_t i)
{
    size_t p = receiver->history_head + i;
    return p >= NET_WIRE_UART_FRAME_MAX ? p - NET_WIRE_UART_FRAME_MAX : p;
}
/* Constant work per byte: a full ring overwrites its oldest entry. */
static void append_history(NetUpperUartReceiver *receiver, uint8_t byte, uint64_t at)
{
    size_t p;
    if (receiver->history_count == NET_WIRE_UART_FRAME_MAX) {
        p = receiver->history_head;
        receiver->history_head = history_at(receiver, 1u);
    } else {
        p = history_at(receiver, receiver->history_count++);
    }
    receiver->history_bytes[p] = byte;
    receiver->history_at_ms[p] = at;
}
static NetWireStatus deliver(NetUpperUartReceiver *receiver,
    const NetWireUartFrame *frame, NetUpperUartDelivery *out)
{
    NetUpperUartMessage message;
    NetWireStatus status;
    size_t start, i;
    status = net_upper_uart_decode(receiver->stream.profile, receiver->stream.link,
        receiver->stream.direction, frame, &message);
    if (status != NET_WIRE_OK) {
        increment(&receiver->typed_rejected);
        return status;
    }
    /* The stream only discards prefixes. Its post-return count is exactly
     * the captured suffix after this frame, including a complete later frame.
     * This positional provenance remains exact even for identical bytes with
     * equal last timestamps but different per-byte capture vectors. */
    if (frame->length > receiver->history_count ||
        receiver->stream.count > receiver->history_count - frame->length)
        return NET_WIRE_TIME_HOLD;
    start = receiver->history_count - receiver->stream.count - frame->length;
    if (receiver->history_at_ms[history_at(receiver, start + frame->length - 1u)] != frame->captured_at_ms)
        return NET_WIRE_TIME_HOLD;
    for (i = 0u; i < frame->length; ++i)
        if (receiver->history_bytes[history_at(receiver, start + i)] != frame->bytes[i])
            return NET_WIRE_TIME_HOLD;
    /* Small typed staging only; OUT begins changing after all failures. */
    net_wire_zero(out, sizeof(*out));
    net_wire_copy(&out->message, &message, sizeof(message));
    for (i = 0u; i < frame->length; ++i)
        out->byte_at_ms[i] = receiver->history_at_ms[history_at(receiver, start + i)];
    net_wire_copy(&out->frame, frame, sizeof(*frame)); out->port_epoch = receiver->port_epoch;
    increment(&receiver->delivered_frames);
    return NET_WIRE_OK;
}
/* The stream's buffered bytes must be the newest history entries, byte and
 * time (positional provenance for deliver). Older entries were checked the
 * same way while they were still buffered and are never changed afterwards,
 * so only that suffix is compared: the work per byte stays bounded by the
 * frame being assembled, not by the history length. */
static NetWireStatus history_check(const NetUpperUartReceiver *r)
{
    size_t i,p;
    if(r->history_count>NET_WIRE_UART_FRAME_MAX || r->history_head>=NET_WIRE_UART_FRAME_MAX ||
       r->stream.count>NET_WIRE_UART_FRAME_MAX)
        return NET_WIRE_INVALID_ARGUMENT;
    if(r->stream.count>r->history_count) return NET_WIRE_TIME_HOLD;
    p=history_at(r,r->history_count-r->stream.count);
    for(i=0u;i<r->stream.count;++i) {
        if(r->stream.bytes[i]!=r->history_bytes[p] ||
           r->stream.captured_at_ms[i]!=r->history_at_ms[p]) return NET_WIRE_TIME_HOLD;
        if(++p==NET_WIRE_UART_FRAME_MAX) p=0u;
    }
    return NET_WIRE_OK;
}
static NetWireStatus receive(NetUpperUartReceiver *receiver,
    const uint8_t *byte, const uint64_t *at, uint64_t now_ms,
    bool timestamp_known, uint64_t port_epoch, bool *consumed,
    NetUpperUartDelivery *out)
{
    NetWireStreamInput input;
    NetWireUartFrame frame;
    NetWireStatus status;
    size_t used = 0u;
    if (receiver == NULL || consumed == NULL || out == NULL ||
        net_wire_ranges_overlap(receiver, sizeof(*receiver), out, sizeof(*out)) ||
        net_wire_ranges_overlap(receiver, sizeof(*receiver), consumed, sizeof(*consumed)) ||
        net_wire_ranges_overlap(consumed, sizeof(*consumed), out, sizeof(*out)))
        return NET_WIRE_INVALID_ARGUMENT;
    if (!receiver->initialized) return NET_WIRE_NOT_BOUND_STATUS;
    if (port_epoch == 0u || port_epoch != receiver->port_epoch) return NET_WIRE_CONTRACT_HOLD;
    if (!timestamp_known || (at != NULL && now_ms < *at)) return NET_WIRE_TIME_HOLD;
    status=history_check(receiver); if(status!=NET_WIRE_OK) return status;
    /* Every member is set: no structure clear per received byte. */
    input.bytes = byte; input.captured_at_ms = at;
    input.length = byte != NULL ? 1u : 0u;
    input.now_ms = now_ms; input.timestamps_unambiguous = true;
    input.timer_check_permitted = byte == NULL;
    status = nw_uart_stream_next_owned(&receiver->stream, &input, &used, &frame);
    if (status != NET_WIRE_OK && status != NET_WIRE_NEED_MORE) return status;
    if (used != 0u) append_history(receiver, *byte, *at);
    if (status == NET_WIRE_OK) status = deliver(receiver, &frame, out);
    *consumed = used != 0u;
    return status;
}
NetWireStatus net_upper_uart_receiver_feed(NetUpperUartReceiver *receiver,
    uint8_t byte, uint64_t captured_at_ms, uint64_t now_ms,
    bool timestamp_known, uint64_t port_epoch, bool *consumed,
    NetUpperUartDelivery *out)
{
    return receive(receiver, &byte, &captured_at_ms, now_ms, timestamp_known,
        port_epoch, consumed, out);
}
NetWireStatus net_upper_uart_receiver_poll(NetUpperUartReceiver *receiver,
    uint64_t now_ms, uint64_t port_epoch, NetUpperUartDelivery *out)
{
    bool consumed = false;
    return receive(receiver, NULL, NULL, now_ms, true, port_epoch, &consumed, out);
}
NetWireStatus net_upper_uart_receiver_signal(NetUpperUartReceiver *receiver,
    uint64_t port_epoch, bool byte_loss, bool epoch_reset)
{
    NetWireStatus status;
    if (receiver == NULL || port_epoch == 0u) return NET_WIRE_INVALID_ARGUMENT;
    if (!receiver->initialized) return NET_WIRE_NOT_BOUND_STATUS;
    if (epoch_reset) {
        NetWireStreamPolicy policy=receiver->stream.policy;
        uint32_t discarded,bad_headers,bad_crc,timeouts,losses;
        if (port_epoch <= receiver->port_epoch) return NET_WIRE_CONTRACT_HOLD;
        status = nw_uart_stream_config_check(receiver->stream.profile,
            receiver->stream.link, receiver->stream.direction, &policy);
        if (status != NET_WIRE_OK) return status;
        discarded=receiver->stream.discarded_bytes; bad_headers=receiver->stream.bad_headers;
        bad_crc=receiver->stream.bad_crc; timeouts=receiver->stream.assembly_timeouts;
        losses=receiver->stream.byte_losses;
        /* Scalar arguments are evaluated before the destination is cleared.
         * Local policy is small and disjoint; pure validation already passed. */
        (void)net_wire_uart_stream_init(&receiver->stream,receiver->stream.profile,
            receiver->stream.link,receiver->stream.direction,&policy);
        receiver->stream.discarded_bytes=discarded; receiver->stream.bad_headers=bad_headers;
        receiver->stream.bad_crc=bad_crc; receiver->stream.assembly_timeouts=timeouts;
        receiver->stream.byte_losses=losses; receiver->port_epoch=port_epoch;
    } else {
        if(port_epoch!=receiver->port_epoch) return NET_WIRE_CONTRACT_HOLD;
        if(!receiver->stream.configured) return NET_WIRE_NOT_BOUND_STATUS;
    }
    (void)net_wire_uart_stream_reset(&receiver->stream, byte_loss);
    receiver->history_count = 0u; receiver->history_head = 0u;
    return NET_WIRE_OK;
}
