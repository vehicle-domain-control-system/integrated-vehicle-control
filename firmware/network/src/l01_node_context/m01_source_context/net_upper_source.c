/* M01 original-observation guards are separate from transport sequence admission.
 * Conservative comparison uses the same receiver's continuous capture clock.
 * A same-identity age below its prior local lower bound is HOLD; no rewrite,
 * silent age reset, wrap inference or fresh claim is made. */
#include "net_upper_private.h"
#include <string.h>
static bool younger(uint16_t old_age, uint16_t new_age, uint64_t elapsed)
{
    uint16_t floor=elapsed >= (uint64_t)(UINT16_MAX-old_age) ? UINT16_MAX : (uint16_t)(old_age+elapsed);
    return new_age < floor;
}
/* Quality of one lane of the same original observation reported again with a
 * newer TX_SEQUENCE. docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md
 * (repeat and current quality): the identity of an original observation does
 * not freeze its current quality; an actual VALID -> INVALID/STALE drop under
 * the same identity is accepted and delivered, but a higher TX_SEQUENCE alone
 * never changes the original value nor promotes a quality back to OK.
 * (VSS/10_DATA.md: every warning lane keeps its own quality and AGE.)
 * So: a restricted quality (STALE/INVALID/NO_DATA, any reason) may follow any
 * earlier quality; OK may only follow the same OK with the same reason. */
static bool quality_conflict(uint8_t old_quality, uint8_t old_reason, uint8_t new_quality, uint8_t new_reason)
{
    if(new_quality!=0u)return false;
    return old_quality!=0u || old_reason!=new_reason;
}
static bool permission_same(const NetWireDomainPermissionRaw *a, const NetWireDomainPermissionRaw *b)
{
    return a->boot_id==b->boot_id && a->update_sequence==b->update_sequence;
}
static bool warning_lane_same(const NetWireVssWarningStateRaw *a, const NetWireVssWarningStateRaw *b, unsigned lane)
{
    if(a->boot_id!=b->boot_id)return false;
    switch(lane) {
    case 0u:return a->antipinch_source_boot==b->antipinch_source_boot && a->antipinch_source_sequence==b->antipinch_source_sequence;
    case 1u:return a->occupant_hazard_source_boot==b->occupant_hazard_source_boot && a->occupant_hazard_source_sequence==b->occupant_hazard_source_sequence;
    case 2u:return a->rear_obstacle_source_boot==b->rear_obstacle_source_boot && a->rear_obstacle_source_sequence==b->rear_obstacle_source_sequence;
    default:return a->rear_activation_sequence==b->rear_activation_sequence && a->activation_meta_format==1u && b->activation_meta_format==1u;
    }
}
/* Same quality and reason on every lane: nothing new to deliver. */
static bool warning_quality_equal(const NetWireVssWarningStateRaw *a, const NetWireVssWarningStateRaw *b)
{
    return a->antipinch_quality==b->antipinch_quality && a->antipinch_reason==b->antipinch_reason &&
        a->occupant_hazard_quality==b->occupant_hazard_quality && a->occupant_hazard_reason==b->occupant_hazard_reason &&
        a->rear_obstacle_quality==b->rear_obstacle_quality && a->rear_obstacle_reason==b->rear_obstacle_reason &&
        a->rear_activation_quality==b->rear_activation_quality && a->rear_activation_reason==b->rear_activation_reason;
}
NetStatus net_upper_cis_publish_occupant(NetUpper *s,uint32_t cis_boot,uint32_t tx_sequence,
    const NetAgePolicy *policy,NetTxKey *out)
{
    size_t i;const NetUpperAdmission *a=NULL;const NetWirePVisionResult *v;
    NetUpperCanMessage m;NetSourceStamp source;
    if(s==NULL || !s->initialized || policy==NULL || out==NULL ||
       net_wire_ranges_overlap(s,sizeof(*s),policy,sizeof(*policy)) ||
       net_wire_ranges_overlap(s,sizeof(*s),out,sizeof(*out)) ||
       net_wire_ranges_overlap(policy,sizeof(*policy),out,sizeof(*out)))return NET_INVALID_ARGUMENT;
    if(s->in_call)return NET_BUSY;
    if(s->config.node!=NET_WIRE_NODE_CIS)return NET_REJECTED;
    for(i=0u;i<NET_UPPER_ROUTE_COUNT;++i) {
        const NetWireRegistryEntry *r=net_wire_registry_at(i);
        if(r->link==NET_WIRE_UART_C && r->id_type==0x60u){a=net_upper_admission_at(s,i);break;}
    }
    if(a==NULL || !a->seen || a->delivery.capture.port_epoch!=s->config.port_epoch[NET_WIRE_UART_C])return NET_HOLD;
    v=&a->uart.data.p_vision_result;memset(&m,0,sizeof(m));m.id=0x310u;
    m.data.cis_occupant.boot_id=cis_boot;m.data.cis_occupant.tx_sequence=tx_sequence;
    m.data.cis_occupant.pi_boot_id=v->pi_boot_id;m.data.cis_occupant.vision_sequence=v->vision_sequence;
    m.data.cis_occupant.vision_age_ms=v->vision_age_ms;
    m.data.cis_occupant.occupant_presence=v->occupant_presence;
    m.data.cis_occupant.presence_validity=v->presence_validity;m.data.cis_occupant.presence_reason=v->presence_reason;
    m.data.cis_occupant.occupant_count=v->occupant_count;m.data.cis_occupant.count_validity=v->count_validity;
    m.data.cis_occupant.count_reason=v->count_reason;m.data.cis_occupant.vision_function_status=v->vision_status;
    source.observation_id=a->delivery.receipt_id;source.observed_at_ms=a->delivery.capture.captured_at_ms;
    source.original_age_ms=v->vision_age_ms;
    return net_upper_submit_can(s,&m,&source,policy,out);
}
NetStatus net_upper_can_original_age(const NetUpperCanMessage *old, const NetUpperCanMessage *m, uint64_t elapsed)
{
    if(old->id!=m->id)return NET_HOLD;
    switch(m->id) {
    case 0x080u: {
        /* The original observation identity cannot carry different values; its
         * two qualities may only be restricted (quality_conflict). */
        const NetWireDomainPermissionRaw *a=&old->data.domain_permission,*b=&m->data.domain_permission;
        if (permission_same(a,b) && (
            a->vehicle_power_permission!=b->vehicle_power_permission ||
            a->power_operation_state!=b->power_operation_state ||
            a->operation_allowed!=b->operation_allowed ||
            quality_conflict(a->power_permission_quality,0u,b->power_permission_quality,0u) ||
            quality_conflict(a->operation_quality,0u,b->operation_quality,0u))) return NET_HOLD;
        if (permission_same(a,b) && younger(a->source_age_ms,b->source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    }
    case 0x100u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_door_command.boot_id==m->data.bcm_door_command.boot_id && old->data.bcm_door_command.target_boot_id==m->data.bcm_door_command.target_boot_id && old->data.bcm_door_command.command_id==m->data.bcm_door_command.command_id && (
            old->data.bcm_door_command.accept_limit_ms!=m->data.bcm_door_command.accept_limit_ms ||
            old->data.bcm_door_command.door_lock_target!=m->data.bcm_door_command.door_lock_target ||
            old->data.bcm_door_command.origin!=m->data.bcm_door_command.origin)) return NET_HOLD;
        if (old->data.bcm_door_command.boot_id==m->data.bcm_door_command.boot_id && old->data.bcm_door_command.target_boot_id==m->data.bcm_door_command.target_boot_id && old->data.bcm_door_command.command_id==m->data.bcm_door_command.command_id && younger(old->data.bcm_door_command.command_age_ms,m->data.bcm_door_command.command_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x110u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_climate_command.boot_id==m->data.bcm_climate_command.boot_id && old->data.bcm_climate_command.target_boot_id==m->data.bcm_climate_command.target_boot_id && old->data.bcm_climate_command.command_id==m->data.bcm_climate_command.command_id && (
            old->data.bcm_climate_command.accept_limit_ms!=m->data.bcm_climate_command.accept_limit_ms ||
            old->data.bcm_climate_command.fan_target_level!=m->data.bcm_climate_command.fan_target_level ||
            old->data.bcm_climate_command.thermal_direction!=m->data.bcm_climate_command.thermal_direction ||
            old->data.bcm_climate_command.thermal_target_percent!=m->data.bcm_climate_command.thermal_target_percent)) return NET_HOLD;
        if (old->data.bcm_climate_command.boot_id==m->data.bcm_climate_command.boot_id && old->data.bcm_climate_command.target_boot_id==m->data.bcm_climate_command.target_boot_id && old->data.bcm_climate_command.command_id==m->data.bcm_climate_command.command_id && younger(old->data.bcm_climate_command.command_age_ms,m->data.bcm_climate_command.command_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x120u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_light_command.boot_id==m->data.bcm_light_command.boot_id && old->data.bcm_light_command.target_boot_id==m->data.bcm_light_command.target_boot_id && old->data.bcm_light_command.command_id==m->data.bcm_light_command.command_id && (
            old->data.bcm_light_command.accept_limit_ms!=m->data.bcm_light_command.accept_limit_ms ||
            old->data.bcm_light_command.interior_light_type!=m->data.bcm_light_command.interior_light_type ||
            old->data.bcm_light_command.light_level_percent!=m->data.bcm_light_command.light_level_percent ||
            old->data.bcm_light_command.red!=m->data.bcm_light_command.red ||
            old->data.bcm_light_command.green!=m->data.bcm_light_command.green ||
            old->data.bcm_light_command.blue!=m->data.bcm_light_command.blue)) return NET_HOLD;
        if (old->data.bcm_light_command.boot_id==m->data.bcm_light_command.boot_id && old->data.bcm_light_command.target_boot_id==m->data.bcm_light_command.target_boot_id && old->data.bcm_light_command.command_id==m->data.bcm_light_command.command_id && younger(old->data.bcm_light_command.command_age_ms,m->data.bcm_light_command.command_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x090u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.window_command.boot_id==m->data.window_command.boot_id && old->data.window_command.target_boot_id==m->data.window_command.target_boot_id && old->data.window_command.command_id==m->data.window_command.command_id && (
            old->data.window_command.accept_limit_ms!=m->data.window_command.accept_limit_ms ||
            old->data.window_command.window_command!=m->data.window_command.window_command ||
            old->data.window_command.target_channel!=m->data.window_command.target_channel ||
            old->data.window_command.target_position!=m->data.window_command.target_position ||
            old->data.window_command.source_context!=m->data.window_command.source_context)) return NET_HOLD;
        if (old->data.window_command.boot_id==m->data.window_command.boot_id && old->data.window_command.target_boot_id==m->data.window_command.target_boot_id && old->data.window_command.command_id==m->data.window_command.command_id && younger(old->data.window_command.command_age_ms,m->data.window_command.command_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x200u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_door_state.boot_id==m->data.bcm_door_state.boot_id && old->data.bcm_door_state.update_sequence==m->data.bcm_door_state.update_sequence && (
            old->data.bcm_door_state.door_lock_state!=m->data.bcm_door_state.door_lock_state ||
            old->data.bcm_door_state.door_open_state!=m->data.bcm_door_state.door_open_state ||
            old->data.bcm_door_state.door_composite_state!=m->data.bcm_door_state.door_composite_state ||
            old->data.bcm_door_state.lock_quality!=m->data.bcm_door_state.lock_quality ||
            old->data.bcm_door_state.open_quality!=m->data.bcm_door_state.open_quality ||
            old->data.bcm_door_state.composite_quality!=m->data.bcm_door_state.composite_quality)) return NET_HOLD;
        if (old->data.bcm_door_state.boot_id==m->data.bcm_door_state.boot_id && old->data.bcm_door_state.update_sequence==m->data.bcm_door_state.update_sequence && younger(old->data.bcm_door_state.source_age_ms,m->data.bcm_door_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x210u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_climate_state.boot_id==m->data.bcm_climate_state.boot_id && old->data.bcm_climate_state.update_sequence==m->data.bcm_climate_state.update_sequence && (
            old->data.bcm_climate_state.fan_command_level!=m->data.bcm_climate_state.fan_command_level ||
            old->data.bcm_climate_state.fan_measured_level!=m->data.bcm_climate_state.fan_measured_level ||
            old->data.bcm_climate_state.thermal_direction_state!=m->data.bcm_climate_state.thermal_direction_state ||
            old->data.bcm_climate_state.thermal_output_percent!=m->data.bcm_climate_state.thermal_output_percent ||
            old->data.bcm_climate_state.heat_removal_state!=m->data.bcm_climate_state.heat_removal_state ||
            old->data.bcm_climate_state.fan_command_quality!=m->data.bcm_climate_state.fan_command_quality ||
            old->data.bcm_climate_state.fan_measured_quality!=m->data.bcm_climate_state.fan_measured_quality ||
            old->data.bcm_climate_state.thermal_direction_quality!=m->data.bcm_climate_state.thermal_direction_quality ||
            old->data.bcm_climate_state.thermal_output_quality!=m->data.bcm_climate_state.thermal_output_quality ||
            old->data.bcm_climate_state.heat_removal_quality!=m->data.bcm_climate_state.heat_removal_quality)) return NET_HOLD;
        if (old->data.bcm_climate_state.boot_id==m->data.bcm_climate_state.boot_id && old->data.bcm_climate_state.update_sequence==m->data.bcm_climate_state.update_sequence && younger(old->data.bcm_climate_state.source_age_ms,m->data.bcm_climate_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x220u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_light_state.boot_id==m->data.bcm_light_state.boot_id && old->data.bcm_light_state.update_sequence==m->data.bcm_light_state.update_sequence && (
            old->data.bcm_light_state.interior_light_type_state!=m->data.bcm_light_state.interior_light_type_state ||
            old->data.bcm_light_state.light_level_percent!=m->data.bcm_light_state.light_level_percent ||
            old->data.bcm_light_state.red!=m->data.bcm_light_state.red ||
            old->data.bcm_light_state.green!=m->data.bcm_light_state.green ||
            old->data.bcm_light_state.blue!=m->data.bcm_light_state.blue ||
            old->data.bcm_light_state.interior_light_apply_result!=m->data.bcm_light_state.interior_light_apply_result ||
            old->data.bcm_light_state.light_physical_feedback_capability!=m->data.bcm_light_state.light_physical_feedback_capability ||
            old->data.bcm_light_state.light_type_quality!=m->data.bcm_light_state.light_type_quality ||
            old->data.bcm_light_state.light_level_quality!=m->data.bcm_light_state.light_level_quality ||
            old->data.bcm_light_state.light_color_quality!=m->data.bcm_light_state.light_color_quality ||
            old->data.bcm_light_state.light_apply_quality!=m->data.bcm_light_state.light_apply_quality)) return NET_HOLD;
        if (old->data.bcm_light_state.boot_id==m->data.bcm_light_state.boot_id && old->data.bcm_light_state.update_sequence==m->data.bcm_light_state.update_sequence && younger(old->data.bcm_light_state.source_age_ms,m->data.bcm_light_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x140u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_result.boot_id==m->data.bcm_result.boot_id && old->data.bcm_result.occurrence_id==m->data.bcm_result.occurrence_id && old->data.bcm_result.occurrence_id!=0u && (
            old->data.bcm_result.door_event!=m->data.bcm_result.door_event ||
            old->data.bcm_result.door_target!=m->data.bcm_result.door_target ||
            old->data.bcm_result.event_quality!=m->data.bcm_result.event_quality)) return NET_HOLD;
        if (old->data.bcm_result.boot_id==m->data.bcm_result.boot_id && old->data.bcm_result.occurrence_id==m->data.bcm_result.occurrence_id && younger(old->data.bcm_result.event_age_ms,m->data.bcm_result.event_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x141u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_event.boot_id==m->data.bcm_event.boot_id && old->data.bcm_event.occurrence_id==m->data.bcm_event.occurrence_id && (
            old->data.bcm_event.event_kind!=m->data.bcm_event.event_kind ||
            old->data.bcm_event.event_quality!=m->data.bcm_event.event_quality ||
            old->data.bcm_event.related_domain_boot!=m->data.bcm_event.related_domain_boot ||
            old->data.bcm_event.related_command_id!=m->data.bcm_event.related_command_id ||
            old->data.bcm_event.affected_function!=m->data.bcm_event.affected_function)) return NET_HOLD;
        if (old->data.bcm_event.boot_id==m->data.bcm_event.boot_id && old->data.bcm_event.occurrence_id==m->data.bcm_event.occurrence_id && younger(old->data.bcm_event.event_age_ms,m->data.bcm_event.event_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x230u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.bcm_status.boot_id==m->data.bcm_status.boot_id && old->data.bcm_status.update_sequence==m->data.bcm_status.update_sequence && (
            old->data.bcm_status.bcm_ecu_state!=m->data.bcm_status.bcm_ecu_state ||
            old->data.bcm_status.fault_mask!=m->data.bcm_status.fault_mask ||
            old->data.bcm_status.active_fault_category!=m->data.bcm_status.active_fault_category ||
            old->data.bcm_status.active_fault_code!=m->data.bcm_status.active_fault_code ||
            old->data.bcm_status.affected_function!=m->data.bcm_status.affected_function ||
            old->data.bcm_status.recovering!=m->data.bcm_status.recovering)) return NET_HOLD;
        if (old->data.bcm_status.boot_id==m->data.bcm_status.boot_id && old->data.bcm_status.update_sequence==m->data.bcm_status.update_sequence && younger(old->data.bcm_status.source_age_ms,m->data.bcm_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x300u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.temperature_sequence==m->data.cis_environment.temperature_sequence && (
            old->data.cis_environment.temperature_centi_c!=m->data.cis_environment.temperature_centi_c ||
            old->data.cis_environment.temperature_validity!=m->data.cis_environment.temperature_validity ||
            old->data.cis_environment.temperature_reason!=m->data.cis_environment.temperature_reason)) return NET_HOLD;
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.temperature_sequence==m->data.cis_environment.temperature_sequence && younger(old->data.cis_environment.temperature_age_ms,m->data.cis_environment.temperature_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.humidity_sequence==m->data.cis_environment.humidity_sequence && (
            old->data.cis_environment.humidity_centi_percent!=m->data.cis_environment.humidity_centi_percent ||
            old->data.cis_environment.humidity_validity!=m->data.cis_environment.humidity_validity ||
            old->data.cis_environment.humidity_reason!=m->data.cis_environment.humidity_reason)) return NET_HOLD;
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.humidity_sequence==m->data.cis_environment.humidity_sequence && younger(old->data.cis_environment.humidity_age_ms,m->data.cis_environment.humidity_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.illuminance_sequence==m->data.cis_environment.illuminance_sequence && (
            old->data.cis_environment.illuminance_lux!=m->data.cis_environment.illuminance_lux ||
            old->data.cis_environment.illuminance_validity!=m->data.cis_environment.illuminance_validity ||
            old->data.cis_environment.illuminance_reason!=m->data.cis_environment.illuminance_reason)) return NET_HOLD;
        if (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.illuminance_sequence==m->data.cis_environment.illuminance_sequence && younger(old->data.cis_environment.illuminance_age_ms,m->data.cis_environment.illuminance_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x310u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_occupant.boot_id==m->data.cis_occupant.boot_id && old->data.cis_occupant.pi_boot_id==m->data.cis_occupant.pi_boot_id && old->data.cis_occupant.vision_sequence==m->data.cis_occupant.vision_sequence && (
            old->data.cis_occupant.occupant_presence!=m->data.cis_occupant.occupant_presence ||
            old->data.cis_occupant.presence_validity!=m->data.cis_occupant.presence_validity ||
            old->data.cis_occupant.presence_reason!=m->data.cis_occupant.presence_reason ||
            old->data.cis_occupant.occupant_count!=m->data.cis_occupant.occupant_count ||
            old->data.cis_occupant.count_validity!=m->data.cis_occupant.count_validity ||
            old->data.cis_occupant.count_reason!=m->data.cis_occupant.count_reason ||
            old->data.cis_occupant.vision_function_status!=m->data.cis_occupant.vision_function_status)) return NET_HOLD;
        if (old->data.cis_occupant.boot_id==m->data.cis_occupant.boot_id && old->data.cis_occupant.pi_boot_id==m->data.cis_occupant.pi_boot_id && old->data.cis_occupant.vision_sequence==m->data.cis_occupant.vision_sequence && younger(old->data.cis_occupant.vision_age_ms,m->data.cis_occupant.vision_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x0A0u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_rear.boot_id==m->data.cis_rear.boot_id && old->data.cis_rear.update_sequence==m->data.cis_rear.update_sequence && (
            old->data.cis_rear.rear_distance_deci_cm!=m->data.cis_rear.rear_distance_deci_cm ||
            old->data.cis_rear.validity!=m->data.cis_rear.validity ||
            old->data.cis_rear.quality_reason!=m->data.cis_rear.quality_reason ||
            old->data.cis_rear.proximity_status!=m->data.cis_rear.proximity_status)) return NET_HOLD;
        if (old->data.cis_rear.boot_id==m->data.cis_rear.boot_id && old->data.cis_rear.update_sequence==m->data.cis_rear.update_sequence && younger(old->data.cis_rear.source_age_ms,m->data.cis_rear.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x320u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.cis_status.boot_id==m->data.cis_status.boot_id && old->data.cis_status.update_sequence==m->data.cis_status.update_sequence && (
            old->data.cis_status.cis_state!=m->data.cis_status.cis_state ||
            old->data.cis_status.vision_status!=m->data.cis_status.vision_status ||
            old->data.cis_status.temperature_status!=m->data.cis_status.temperature_status ||
            old->data.cis_status.humidity_status!=m->data.cis_status.humidity_status ||
            old->data.cis_status.illuminance_status!=m->data.cis_status.illuminance_status ||
            old->data.cis_status.rear_status!=m->data.cis_status.rear_status ||
            old->data.cis_status.pi_uart_status!=m->data.cis_status.pi_uart_status ||
            old->data.cis_status.can_interface_status!=m->data.cis_status.can_interface_status ||
            old->data.cis_status.fault_mask!=m->data.cis_status.fault_mask ||
            old->data.cis_status.active_fault!=m->data.cis_status.active_fault ||
            old->data.cis_status.recovering!=m->data.cis_status.recovering ||
            old->data.cis_status.last_fault!=m->data.cis_status.last_fault ||
            old->data.cis_status.last_fault_occurrence!=m->data.cis_status.last_fault_occurrence ||
            old->data.cis_status.affected_function!=m->data.cis_status.affected_function)) return NET_HOLD;
        if (old->data.cis_status.boot_id==m->data.cis_status.boot_id && old->data.cis_status.update_sequence==m->data.cis_status.update_sequence && younger(old->data.cis_status.source_age_ms,m->data.cis_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x0B0u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.window_state.boot_id==m->data.window_state.boot_id && old->data.window_state.update_sequence==m->data.window_state.update_sequence && (
            old->data.window_state.motion_state!=m->data.window_state.motion_state ||
            old->data.window_state.window_ecu_state!=m->data.window_state.window_ecu_state ||
            old->data.window_state.window_position!=m->data.window_state.window_position ||
            old->data.window_state.window_value_quality!=m->data.window_state.window_value_quality ||
            old->data.window_state.fully_open_state!=m->data.window_state.fully_open_state ||
            old->data.window_state.fully_closed_state!=m->data.window_state.fully_closed_state ||
            old->data.window_state.antipinch_protection_status!=m->data.window_state.antipinch_protection_status ||
            old->data.window_state.reverse_state!=m->data.window_state.reverse_state)) return NET_HOLD;
        if (old->data.window_state.boot_id==m->data.window_state.boot_id && old->data.window_state.update_sequence==m->data.window_state.update_sequence && younger(old->data.window_state.source_age_ms,m->data.window_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x091u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.window_event.boot_id==m->data.window_event.boot_id && old->data.window_event.occurrence_id==m->data.window_event.occurrence_id && (
            old->data.window_event.event_kind!=m->data.window_event.event_kind ||
            old->data.window_event.target_channel!=m->data.window_event.target_channel ||
            old->data.window_event.related_domain_boot!=m->data.window_event.related_domain_boot ||
            old->data.window_event.related_request_sequence!=m->data.window_event.related_request_sequence ||
            old->data.window_event.origin!=m->data.window_event.origin ||
            old->data.window_event.event_quality!=m->data.window_event.event_quality)) return NET_HOLD;
        if (old->data.window_event.boot_id==m->data.window_event.boot_id && old->data.window_event.occurrence_id==m->data.window_event.occurrence_id && younger(old->data.window_event.event_age_ms,m->data.window_event.event_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x092u:
        return NET_OK;
    case 0x330u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.window_fault.boot_id==m->data.window_fault.boot_id && old->data.window_fault.update_sequence==m->data.window_fault.update_sequence && (
            old->data.window_fault.fault_category!=m->data.window_fault.fault_category ||
            old->data.window_fault.fault_code!=m->data.window_fault.fault_code ||
            old->data.window_fault.fault_status!=m->data.window_fault.fault_status ||
            old->data.window_fault.occurrence_id!=m->data.window_fault.occurrence_id)) return NET_HOLD;
        if (old->data.window_fault.boot_id==m->data.window_fault.boot_id && old->data.window_fault.update_sequence==m->data.window_fault.update_sequence && younger(old->data.window_fault.source_age_ms,m->data.window_fault.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x160u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.vss_event.boot_id==m->data.vss_event.boot_id && old->data.vss_event.occurrence_id==m->data.vss_event.occurrence_id && (
            old->data.vss_event.event_kind!=m->data.vss_event.event_kind ||
            old->data.vss_event.use_limit_ms!=m->data.vss_event.use_limit_ms ||
            old->data.vss_event.value_quality!=m->data.vss_event.value_quality ||
            old->data.vss_event.input_availability!=m->data.vss_event.input_availability ||
            old->data.vss_event.related_command_id!=m->data.vss_event.related_command_id)) return NET_HOLD;
        if (old->data.vss_event.boot_id==m->data.vss_event.boot_id && old->data.vss_event.occurrence_id==m->data.vss_event.occurrence_id && younger(old->data.vss_event.original_age_ms,m->data.vss_event.original_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x0C0u: {
        /* Each lane is judged on its own identity and fields only. Under the
         * same lane identity the value (state, availability) cannot change and
         * the quality may only be restricted (quality_conflict), so a lane that
         * lost its quality and keeps its last source sequence with a growing
         * AGE does not hold the frame that carries a new evaluation of another
         * lane. A lane that does change its value or promote its quality holds
         * the whole frame: one frame is committed or refused as one result. */
        const NetWireVssWarningStateRaw *a=&old->data.vss_warning_state,*b=&m->data.vss_warning_state;
        if (warning_lane_same(a,b,0u) && (
            a->antipinch_state!=b->antipinch_state ||
            a->antipinch_availability!=b->antipinch_availability ||
            quality_conflict(a->antipinch_quality,a->antipinch_reason,b->antipinch_quality,b->antipinch_reason))) return NET_HOLD;
        if (warning_lane_same(a,b,0u) && younger(a->antipinch_original_age_ms,b->antipinch_original_age_ms,elapsed)) return NET_HOLD;
        if (warning_lane_same(a,b,1u) && (
            a->occupant_hazard_state!=b->occupant_hazard_state ||
            a->occupant_hazard_availability!=b->occupant_hazard_availability ||
            quality_conflict(a->occupant_hazard_quality,a->occupant_hazard_reason,b->occupant_hazard_quality,b->occupant_hazard_reason))) return NET_HOLD;
        if (warning_lane_same(a,b,1u) && younger(a->occupant_hazard_original_age_ms,b->occupant_hazard_original_age_ms,elapsed)) return NET_HOLD;
        if (warning_lane_same(a,b,2u) && (
            a->rear_obstacle_state!=b->rear_obstacle_state ||
            a->rear_obstacle_availability!=b->rear_obstacle_availability ||
            quality_conflict(a->rear_obstacle_quality,a->rear_obstacle_reason,b->rear_obstacle_quality,b->rear_obstacle_reason))) return NET_HOLD;
        if (warning_lane_same(a,b,2u) && younger(a->rear_obstacle_original_age_ms,b->rear_obstacle_original_age_ms,elapsed)) return NET_HOLD;
        if (warning_lane_same(a,b,3u) && (
            a->rear_detection_activation!=b->rear_detection_activation ||
            quality_conflict(a->rear_activation_quality,a->rear_activation_reason,b->rear_activation_quality,b->rear_activation_reason))) return NET_HOLD;
        if (warning_lane_same(a,b,3u) && younger(a->rear_activation_age_ms,b->rear_activation_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    }
    case 0x340u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.last_fault_occurrence==m->data.vss_status.last_fault_occurrence && old->data.vss_status.last_fault_quality==0u && m->data.vss_status.last_fault_quality==0u && (
            old->data.vss_status.vss_last_fault!=m->data.vss_status.vss_last_fault)) return NET_HOLD;
        if (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.last_fault_occurrence==m->data.vss_status.last_fault_occurrence && old->data.vss_status.last_fault_quality==0u && m->data.vss_status.last_fault_quality==0u && younger(old->data.vss_status.last_fault_age_ms,m->data.vss_status.last_fault_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.update_sequence==m->data.vss_status.update_sequence && (
            old->data.vss_status.vss_state!=m->data.vss_status.vss_state ||
            old->data.vss_status.vss_availability!=m->data.vss_status.vss_availability ||
            old->data.vss_status.vss_accepting_events!=m->data.vss_status.vss_accepting_events ||
            old->data.vss_status.vss_fault_active!=m->data.vss_status.vss_fault_active ||
            old->data.vss_status.fault_mask!=m->data.vss_status.fault_mask)) return NET_HOLD;
        if (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.update_sequence==m->data.vss_status.update_sequence && younger(old->data.vss_status.source_age_ms,m->data.vss_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    default:return NET_HOLD;
    }
}
NetStatus net_upper_uart_original_age(const NetUpperUartMessage *old, const NetUpperUartMessage *m, uint64_t elapsed)
{
    if(old->link!=m->link || old->type!=m->type)return NET_HOLD;
    switch(m->type) {
    case 0x01u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.app_update_sequence==m->data.m_context.app_update_sequence && (
            old->data.m_context.app_active_state!=m->data.m_context.app_active_state ||
            old->data.m_context.app_quality!=m->data.m_context.app_quality)) return NET_HOLD;
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.app_update_sequence==m->data.m_context.app_update_sequence && younger(old->data.m_context.app_age_ms,m->data.m_context.app_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.proximity_update_sequence==m->data.m_context.proximity_update_sequence && (
            old->data.m_context.proximity_state!=m->data.m_context.proximity_state ||
            old->data.m_context.proximity_validity!=m->data.m_context.proximity_validity ||
            old->data.m_context.proximity_reason!=m->data.m_context.proximity_reason)) return NET_HOLD;
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.proximity_update_sequence==m->data.m_context.proximity_update_sequence && younger(old->data.m_context.proximity_age_ms,m->data.m_context.proximity_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.registration_connection_update_sequence==m->data.m_context.registration_connection_update_sequence && (
            old->data.m_context.device_registration_state!=m->data.m_context.device_registration_state ||
            old->data.m_context.bt_connection_state!=m->data.m_context.bt_connection_state ||
            old->data.m_context.registration_connection_quality!=m->data.m_context.registration_connection_quality)) return NET_HOLD;
        if (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.registration_connection_update_sequence==m->data.m_context.registration_connection_update_sequence && younger(old->data.m_context.registration_connection_age_ms,m->data.m_context.registration_connection_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x02u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_domain_alive.domain_boot_id==m->data.m_domain_alive.domain_boot_id && old->data.m_domain_alive.update_sequence==m->data.m_domain_alive.update_sequence && (
            old->data.m_domain_alive.domain_state!=m->data.m_domain_alive.domain_state ||
            old->data.m_domain_alive.value_quality!=m->data.m_domain_alive.value_quality)) return NET_HOLD;
        if (old->data.m_domain_alive.domain_boot_id==m->data.m_domain_alive.domain_boot_id && old->data.m_domain_alive.update_sequence==m->data.m_domain_alive.update_sequence && younger(old->data.m_domain_alive.source_age_ms,m->data.m_domain_alive.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x20u:
        if (old->data.m_result.domain_boot_id==m->data.m_result.domain_boot_id && old->data.m_result.request_session==m->data.m_result.request_session && old->data.m_result.request_id==m->data.m_result.request_id && old->data.m_result.command_id==m->data.m_result.command_id && younger(old->data.m_result.result_age_ms,m->data.m_result.result_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x21u:
        if (old->data.m_warning.domain_boot_id==m->data.m_warning.domain_boot_id && old->data.m_warning.source_boot==m->data.m_warning.source_boot && old->data.m_warning.source_sequence==m->data.m_warning.source_sequence && younger(old->data.m_warning.source_age_ms,m->data.m_warning.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x22u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_availability.domain_boot_id==m->data.m_availability.domain_boot_id && old->data.m_availability.update_sequence==m->data.m_availability.update_sequence && (
            old->data.m_availability.items[0].function_id!=m->data.m_availability.items[0].function_id ||
            old->data.m_availability.items[0].availability!=m->data.m_availability.items[0].availability ||
            old->data.m_availability.items[0].reason!=m->data.m_availability.items[0].reason ||
            old->data.m_availability.items[1].function_id!=m->data.m_availability.items[1].function_id ||
            old->data.m_availability.items[1].availability!=m->data.m_availability.items[1].availability ||
            old->data.m_availability.items[1].reason!=m->data.m_availability.items[1].reason ||
            old->data.m_availability.items[2].function_id!=m->data.m_availability.items[2].function_id ||
            old->data.m_availability.items[2].availability!=m->data.m_availability.items[2].availability ||
            old->data.m_availability.items[2].reason!=m->data.m_availability.items[2].reason ||
            old->data.m_availability.items[3].function_id!=m->data.m_availability.items[3].function_id ||
            old->data.m_availability.items[3].availability!=m->data.m_availability.items[3].availability ||
            old->data.m_availability.items[3].reason!=m->data.m_availability.items[3].reason ||
            old->data.m_availability.items[4].function_id!=m->data.m_availability.items[4].function_id ||
            old->data.m_availability.items[4].availability!=m->data.m_availability.items[4].availability ||
            old->data.m_availability.items[4].reason!=m->data.m_availability.items[4].reason ||
            old->data.m_availability.items[5].function_id!=m->data.m_availability.items[5].function_id ||
            old->data.m_availability.items[5].availability!=m->data.m_availability.items[5].availability ||
            old->data.m_availability.items[5].reason!=m->data.m_availability.items[5].reason ||
            old->data.m_availability.items[6].function_id!=m->data.m_availability.items[6].function_id ||
            old->data.m_availability.items[6].availability!=m->data.m_availability.items[6].availability ||
            old->data.m_availability.items[6].reason!=m->data.m_availability.items[6].reason ||
            old->data.m_availability.items[7].function_id!=m->data.m_availability.items[7].function_id ||
            old->data.m_availability.items[7].availability!=m->data.m_availability.items[7].availability ||
            old->data.m_availability.items[7].reason!=m->data.m_availability.items[7].reason ||
            old->data.m_availability.items[8].function_id!=m->data.m_availability.items[8].function_id ||
            old->data.m_availability.items[8].availability!=m->data.m_availability.items[8].availability ||
            old->data.m_availability.items[8].reason!=m->data.m_availability.items[8].reason)) return NET_HOLD;
        if (old->data.m_availability.domain_boot_id==m->data.m_availability.domain_boot_id && old->data.m_availability.update_sequence==m->data.m_availability.update_sequence && younger(old->data.m_availability.source_age_ms,m->data.m_availability.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x23u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_digital_status.domain_boot_id==m->data.m_digital_status.domain_boot_id && old->data.m_digital_status.update_sequence==m->data.m_digital_status.update_sequence && (
            old->data.m_digital_status.digital_key_setting_state!=m->data.m_digital_status.digital_key_setting_state ||
            old->data.m_digital_status.digital_key_availability!=m->data.m_digital_status.digital_key_availability ||
            old->data.m_digital_status.reason!=m->data.m_digital_status.reason)) return NET_HOLD;
        if (old->data.m_digital_status.domain_boot_id==m->data.m_digital_status.domain_boot_id && old->data.m_digital_status.update_sequence==m->data.m_digital_status.update_sequence && younger(old->data.m_digital_status.source_age_ms,m->data.m_digital_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x24u:
        if (old->data.m_digital_result.domain_boot_id==m->data.m_digital_result.domain_boot_id && old->data.m_digital_result.auto_occurrence_id==m->data.m_digital_result.auto_occurrence_id && old->data.m_digital_result.related_door_command_id==m->data.m_digital_result.related_door_command_id && younger(old->data.m_digital_result.result_age_ms,m->data.m_digital_result.result_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x25u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_user_settings.domain_boot_id==m->data.m_user_settings.domain_boot_id && old->data.m_user_settings.update_sequence==m->data.m_user_settings.update_sequence && (
            old->data.m_user_settings.target_temperature_centi_c!=m->data.m_user_settings.target_temperature_centi_c ||
            old->data.m_user_settings.climate_auto!=m->data.m_user_settings.climate_auto ||
            old->data.m_user_settings.user_fan_level!=m->data.m_user_settings.user_fan_level ||
            old->data.m_user_settings.interior_light_enabled!=m->data.m_user_settings.interior_light_enabled ||
            old->data.m_user_settings.normal_light_level_percent!=m->data.m_user_settings.normal_light_level_percent ||
            old->data.m_user_settings.normal_light_rgb[0]!=m->data.m_user_settings.normal_light_rgb[0] ||
            old->data.m_user_settings.normal_light_rgb[1]!=m->data.m_user_settings.normal_light_rgb[1] ||
            old->data.m_user_settings.normal_light_rgb[2]!=m->data.m_user_settings.normal_light_rgb[2] ||
            old->data.m_user_settings.value_quality!=m->data.m_user_settings.value_quality)) return NET_HOLD;
        if (old->data.m_user_settings.domain_boot_id==m->data.m_user_settings.domain_boot_id && old->data.m_user_settings.update_sequence==m->data.m_user_settings.update_sequence && younger(old->data.m_user_settings.source_age_ms,m->data.m_user_settings.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x60u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.p_vision_result.pi_boot_id==m->data.p_vision_result.pi_boot_id && old->data.p_vision_result.vision_sequence==m->data.p_vision_result.vision_sequence && (
            old->data.p_vision_result.occupant_presence!=m->data.p_vision_result.occupant_presence ||
            old->data.p_vision_result.presence_validity!=m->data.p_vision_result.presence_validity ||
            old->data.p_vision_result.presence_reason!=m->data.p_vision_result.presence_reason ||
            old->data.p_vision_result.occupant_count!=m->data.p_vision_result.occupant_count ||
            old->data.p_vision_result.count_validity!=m->data.p_vision_result.count_validity ||
            old->data.p_vision_result.count_reason!=m->data.p_vision_result.count_reason ||
            old->data.p_vision_result.vision_status!=m->data.p_vision_result.vision_status ||
            old->data.p_vision_result.vision_fault_code!=m->data.p_vision_result.vision_fault_code)) return NET_HOLD;
        if (old->data.p_vision_result.pi_boot_id==m->data.p_vision_result.pi_boot_id && old->data.p_vision_result.vision_sequence==m->data.p_vision_result.vision_sequence && younger(old->data.p_vision_result.vision_age_ms,m->data.p_vision_result.vision_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x61u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.p_vision_status.pi_boot_id==m->data.p_vision_status.pi_boot_id && old->data.p_vision_status.status_sequence==m->data.p_vision_status.status_sequence && (
            old->data.p_vision_status.vision_status!=m->data.p_vision_status.vision_status ||
            old->data.p_vision_status.active_fault!=m->data.p_vision_status.active_fault ||
            old->data.p_vision_status.last_fault!=m->data.p_vision_status.last_fault ||
            old->data.p_vision_status.affected_function!=m->data.p_vision_status.affected_function)) return NET_HOLD;
        if (old->data.p_vision_status.pi_boot_id==m->data.p_vision_status.pi_boot_id && old->data.p_vision_status.status_sequence==m->data.p_vision_status.status_sequence && younger(old->data.p_vision_status.status_age_ms,m->data.p_vision_status.status_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x62u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.p_permission.cis_boot_id==m->data.p_permission.cis_boot_id && old->data.p_permission.update_sequence==m->data.p_permission.update_sequence && (
            old->data.p_permission.vehicle_power_permission!=m->data.p_permission.vehicle_power_permission ||
            old->data.p_permission.value_quality!=m->data.p_permission.value_quality ||
            old->data.p_permission.quality_reason!=m->data.p_permission.quality_reason ||
            old->data.p_permission.use_limit_ms!=m->data.p_permission.use_limit_ms)) return NET_HOLD;
        if (old->data.p_permission.cis_boot_id==m->data.p_permission.cis_boot_id && old->data.p_permission.update_sequence==m->data.p_permission.update_sequence && younger(old->data.p_permission.source_age_ms,m->data.p_permission.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x30u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_bcm_door_state.original_can_boot_id==m->data.m_bcm_door_state.original_can_boot_id && old->data.m_bcm_door_state.update_sequence==m->data.m_bcm_door_state.update_sequence && (
            old->data.m_bcm_door_state.door_lock_state!=m->data.m_bcm_door_state.door_lock_state ||
            old->data.m_bcm_door_state.door_open_state!=m->data.m_bcm_door_state.door_open_state ||
            old->data.m_bcm_door_state.door_composite_state!=m->data.m_bcm_door_state.door_composite_state ||
            old->data.m_bcm_door_state.lock_quality!=m->data.m_bcm_door_state.lock_quality ||
            old->data.m_bcm_door_state.open_quality!=m->data.m_bcm_door_state.open_quality ||
            old->data.m_bcm_door_state.composite_quality!=m->data.m_bcm_door_state.composite_quality)) return NET_HOLD;
        if (old->data.m_bcm_door_state.original_can_boot_id==m->data.m_bcm_door_state.original_can_boot_id && old->data.m_bcm_door_state.update_sequence==m->data.m_bcm_door_state.update_sequence && younger(old->data.m_bcm_door_state.source_age_ms,m->data.m_bcm_door_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x31u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_bcm_climate_state.original_can_boot_id==m->data.m_bcm_climate_state.original_can_boot_id && old->data.m_bcm_climate_state.update_sequence==m->data.m_bcm_climate_state.update_sequence && (
            old->data.m_bcm_climate_state.fan_command_level!=m->data.m_bcm_climate_state.fan_command_level ||
            old->data.m_bcm_climate_state.fan_measured_level!=m->data.m_bcm_climate_state.fan_measured_level ||
            old->data.m_bcm_climate_state.thermal_direction_state!=m->data.m_bcm_climate_state.thermal_direction_state ||
            old->data.m_bcm_climate_state.thermal_output_percent!=m->data.m_bcm_climate_state.thermal_output_percent ||
            old->data.m_bcm_climate_state.heat_removal_state!=m->data.m_bcm_climate_state.heat_removal_state ||
            old->data.m_bcm_climate_state.fan_command_quality!=m->data.m_bcm_climate_state.fan_command_quality ||
            old->data.m_bcm_climate_state.fan_measured_quality!=m->data.m_bcm_climate_state.fan_measured_quality ||
            old->data.m_bcm_climate_state.thermal_direction_quality!=m->data.m_bcm_climate_state.thermal_direction_quality ||
            old->data.m_bcm_climate_state.thermal_output_quality!=m->data.m_bcm_climate_state.thermal_output_quality ||
            old->data.m_bcm_climate_state.heat_removal_quality!=m->data.m_bcm_climate_state.heat_removal_quality)) return NET_HOLD;
        if (old->data.m_bcm_climate_state.original_can_boot_id==m->data.m_bcm_climate_state.original_can_boot_id && old->data.m_bcm_climate_state.update_sequence==m->data.m_bcm_climate_state.update_sequence && younger(old->data.m_bcm_climate_state.source_age_ms,m->data.m_bcm_climate_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x32u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_bcm_light_state.original_can_boot_id==m->data.m_bcm_light_state.original_can_boot_id && old->data.m_bcm_light_state.update_sequence==m->data.m_bcm_light_state.update_sequence && (
            old->data.m_bcm_light_state.interior_light_type_state!=m->data.m_bcm_light_state.interior_light_type_state ||
            old->data.m_bcm_light_state.light_level_percent!=m->data.m_bcm_light_state.light_level_percent ||
            old->data.m_bcm_light_state.color_r!=m->data.m_bcm_light_state.color_r ||
            old->data.m_bcm_light_state.color_g!=m->data.m_bcm_light_state.color_g ||
            old->data.m_bcm_light_state.color_b!=m->data.m_bcm_light_state.color_b ||
            old->data.m_bcm_light_state.interior_light_apply_result!=m->data.m_bcm_light_state.interior_light_apply_result ||
            old->data.m_bcm_light_state.light_physical_feedback_capability!=m->data.m_bcm_light_state.light_physical_feedback_capability ||
            old->data.m_bcm_light_state.light_type_quality!=m->data.m_bcm_light_state.light_type_quality ||
            old->data.m_bcm_light_state.light_level_quality!=m->data.m_bcm_light_state.light_level_quality ||
            old->data.m_bcm_light_state.light_color_quality!=m->data.m_bcm_light_state.light_color_quality ||
            old->data.m_bcm_light_state.light_apply_quality!=m->data.m_bcm_light_state.light_apply_quality)) return NET_HOLD;
        if (old->data.m_bcm_light_state.original_can_boot_id==m->data.m_bcm_light_state.original_can_boot_id && old->data.m_bcm_light_state.update_sequence==m->data.m_bcm_light_state.update_sequence && younger(old->data.m_bcm_light_state.source_age_ms,m->data.m_bcm_light_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x33u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_bcm_status.original_can_boot_id==m->data.m_bcm_status.original_can_boot_id && old->data.m_bcm_status.update_sequence==m->data.m_bcm_status.update_sequence && (
            old->data.m_bcm_status.bcm_ecu_state!=m->data.m_bcm_status.bcm_ecu_state ||
            old->data.m_bcm_status.fault_mask!=m->data.m_bcm_status.fault_mask ||
            old->data.m_bcm_status.active_fault_category!=m->data.m_bcm_status.active_fault_category ||
            old->data.m_bcm_status.active_fault_code!=m->data.m_bcm_status.active_fault_code ||
            old->data.m_bcm_status.affected_function!=m->data.m_bcm_status.affected_function ||
            old->data.m_bcm_status.recovering!=m->data.m_bcm_status.recovering)) return NET_HOLD;
        if (old->data.m_bcm_status.original_can_boot_id==m->data.m_bcm_status.original_can_boot_id && old->data.m_bcm_status.update_sequence==m->data.m_bcm_status.update_sequence && younger(old->data.m_bcm_status.source_age_ms,m->data.m_bcm_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x34u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.temperature_sequence==m->data.m_cis_environment.temperature_sequence && (
            old->data.m_cis_environment.temperature_centi_c!=m->data.m_cis_environment.temperature_centi_c ||
            old->data.m_cis_environment.temperature_validity!=m->data.m_cis_environment.temperature_validity ||
            old->data.m_cis_environment.temperature_reason!=m->data.m_cis_environment.temperature_reason)) return NET_HOLD;
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.temperature_sequence==m->data.m_cis_environment.temperature_sequence && younger(old->data.m_cis_environment.temperature_age_ms,m->data.m_cis_environment.temperature_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.humidity_sequence==m->data.m_cis_environment.humidity_sequence && (
            old->data.m_cis_environment.humidity_centi_percent!=m->data.m_cis_environment.humidity_centi_percent ||
            old->data.m_cis_environment.humidity_validity!=m->data.m_cis_environment.humidity_validity ||
            old->data.m_cis_environment.humidity_reason!=m->data.m_cis_environment.humidity_reason)) return NET_HOLD;
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.humidity_sequence==m->data.m_cis_environment.humidity_sequence && younger(old->data.m_cis_environment.humidity_age_ms,m->data.m_cis_environment.humidity_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.illuminance_sequence==m->data.m_cis_environment.illuminance_sequence && (
            old->data.m_cis_environment.illuminance_lux!=m->data.m_cis_environment.illuminance_lux ||
            old->data.m_cis_environment.illuminance_validity!=m->data.m_cis_environment.illuminance_validity ||
            old->data.m_cis_environment.illuminance_reason!=m->data.m_cis_environment.illuminance_reason)) return NET_HOLD;
        if (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.illuminance_sequence==m->data.m_cis_environment.illuminance_sequence && younger(old->data.m_cis_environment.illuminance_age_ms,m->data.m_cis_environment.illuminance_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x35u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_occupant.original_can_boot_id==m->data.m_cis_occupant.original_can_boot_id && old->data.m_cis_occupant.pi_boot_id==m->data.m_cis_occupant.pi_boot_id && old->data.m_cis_occupant.vision_sequence==m->data.m_cis_occupant.vision_sequence && (
            old->data.m_cis_occupant.occupant_presence!=m->data.m_cis_occupant.occupant_presence ||
            old->data.m_cis_occupant.presence_validity!=m->data.m_cis_occupant.presence_validity ||
            old->data.m_cis_occupant.presence_reason!=m->data.m_cis_occupant.presence_reason ||
            old->data.m_cis_occupant.occupant_count!=m->data.m_cis_occupant.occupant_count ||
            old->data.m_cis_occupant.count_validity!=m->data.m_cis_occupant.count_validity ||
            old->data.m_cis_occupant.count_reason!=m->data.m_cis_occupant.count_reason ||
            old->data.m_cis_occupant.vision_function_status!=m->data.m_cis_occupant.vision_function_status)) return NET_HOLD;
        if (old->data.m_cis_occupant.original_can_boot_id==m->data.m_cis_occupant.original_can_boot_id && old->data.m_cis_occupant.pi_boot_id==m->data.m_cis_occupant.pi_boot_id && old->data.m_cis_occupant.vision_sequence==m->data.m_cis_occupant.vision_sequence && younger(old->data.m_cis_occupant.vision_age_ms,m->data.m_cis_occupant.vision_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x36u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_rear.original_can_boot_id==m->data.m_cis_rear.original_can_boot_id && old->data.m_cis_rear.update_sequence==m->data.m_cis_rear.update_sequence && (
            old->data.m_cis_rear.rear_distance_deci_cm!=m->data.m_cis_rear.rear_distance_deci_cm ||
            old->data.m_cis_rear.validity!=m->data.m_cis_rear.validity ||
            old->data.m_cis_rear.quality_reason!=m->data.m_cis_rear.quality_reason ||
            old->data.m_cis_rear.proximity_status!=m->data.m_cis_rear.proximity_status)) return NET_HOLD;
        if (old->data.m_cis_rear.original_can_boot_id==m->data.m_cis_rear.original_can_boot_id && old->data.m_cis_rear.update_sequence==m->data.m_cis_rear.update_sequence && younger(old->data.m_cis_rear.source_age_ms,m->data.m_cis_rear.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x37u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_cis_status.original_can_boot_id==m->data.m_cis_status.original_can_boot_id && old->data.m_cis_status.update_sequence==m->data.m_cis_status.update_sequence && (
            old->data.m_cis_status.cis_state!=m->data.m_cis_status.cis_state ||
            old->data.m_cis_status.vision_status!=m->data.m_cis_status.vision_status ||
            old->data.m_cis_status.temperature_status!=m->data.m_cis_status.temperature_status ||
            old->data.m_cis_status.humidity_status!=m->data.m_cis_status.humidity_status ||
            old->data.m_cis_status.illuminance_status!=m->data.m_cis_status.illuminance_status ||
            old->data.m_cis_status.rear_status!=m->data.m_cis_status.rear_status ||
            old->data.m_cis_status.pi_uart_status!=m->data.m_cis_status.pi_uart_status ||
            old->data.m_cis_status.can_interface_status!=m->data.m_cis_status.can_interface_status ||
            old->data.m_cis_status.fault_mask!=m->data.m_cis_status.fault_mask ||
            old->data.m_cis_status.active_fault!=m->data.m_cis_status.active_fault ||
            old->data.m_cis_status.recovering!=m->data.m_cis_status.recovering ||
            old->data.m_cis_status.last_fault!=m->data.m_cis_status.last_fault ||
            old->data.m_cis_status.last_fault_occurrence!=m->data.m_cis_status.last_fault_occurrence ||
            old->data.m_cis_status.affected_function!=m->data.m_cis_status.affected_function)) return NET_HOLD;
        if (old->data.m_cis_status.original_can_boot_id==m->data.m_cis_status.original_can_boot_id && old->data.m_cis_status.update_sequence==m->data.m_cis_status.update_sequence && younger(old->data.m_cis_status.source_age_ms,m->data.m_cis_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x38u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_window_state.original_can_boot_id==m->data.m_window_state.original_can_boot_id && old->data.m_window_state.update_sequence==m->data.m_window_state.update_sequence && (
            old->data.m_window_state.motion_state!=m->data.m_window_state.motion_state ||
            old->data.m_window_state.window_ecu_state!=m->data.m_window_state.window_ecu_state ||
            old->data.m_window_state.window_position!=m->data.m_window_state.window_position ||
            old->data.m_window_state.window_value_quality!=m->data.m_window_state.window_value_quality ||
            old->data.m_window_state.fully_open_state!=m->data.m_window_state.fully_open_state ||
            old->data.m_window_state.fully_closed_state!=m->data.m_window_state.fully_closed_state ||
            old->data.m_window_state.antipinch_protection_status!=m->data.m_window_state.antipinch_protection_status ||
            old->data.m_window_state.reverse_state!=m->data.m_window_state.reverse_state)) return NET_HOLD;
        if (old->data.m_window_state.original_can_boot_id==m->data.m_window_state.original_can_boot_id && old->data.m_window_state.update_sequence==m->data.m_window_state.update_sequence && younger(old->data.m_window_state.source_age_ms,m->data.m_window_state.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x39u:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_window_fault.original_can_boot_id==m->data.m_window_fault.original_can_boot_id && old->data.m_window_fault.update_sequence==m->data.m_window_fault.update_sequence && (
            old->data.m_window_fault.fault_category!=m->data.m_window_fault.fault_category ||
            old->data.m_window_fault.fault_code!=m->data.m_window_fault.fault_code ||
            old->data.m_window_fault.fault_status!=m->data.m_window_fault.fault_status ||
            old->data.m_window_fault.occurrence_id!=m->data.m_window_fault.occurrence_id)) return NET_HOLD;
        if (old->data.m_window_fault.original_can_boot_id==m->data.m_window_fault.original_can_boot_id && old->data.m_window_fault.update_sequence==m->data.m_window_fault.update_sequence && younger(old->data.m_window_fault.source_age_ms,m->data.m_window_fault.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    case 0x3au:
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.last_fault_occurrence==m->data.m_vss_status.last_fault_occurrence && old->data.m_vss_status.last_fault_quality==0u && m->data.m_vss_status.last_fault_quality==0u && (
            old->data.m_vss_status.vss_last_fault!=m->data.m_vss_status.vss_last_fault)) return NET_HOLD;
        if (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.last_fault_occurrence==m->data.m_vss_status.last_fault_occurrence && old->data.m_vss_status.last_fault_quality==0u && m->data.m_vss_status.last_fault_quality==0u && younger(old->data.m_vss_status.last_fault_age_ms,m->data.m_vss_status.last_fault_age_ms,elapsed)) return NET_HOLD;
        /* The original observation identity cannot carry different values or quality. */
        if (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.update_sequence==m->data.m_vss_status.update_sequence && (
            old->data.m_vss_status.vss_state!=m->data.m_vss_status.vss_state ||
            old->data.m_vss_status.vss_availability!=m->data.m_vss_status.vss_availability ||
            old->data.m_vss_status.vss_accepting_events!=m->data.m_vss_status.vss_accepting_events ||
            old->data.m_vss_status.vss_fault_active!=m->data.m_vss_status.vss_fault_active ||
            old->data.m_vss_status.fault_mask!=m->data.m_vss_status.fault_mask)) return NET_HOLD;
        if (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.update_sequence==m->data.m_vss_status.update_sequence && younger(old->data.m_vss_status.source_age_ms,m->data.m_vss_status.source_age_ms,elapsed)) return NET_HOLD;
        return NET_OK;
    default:return NET_OK; /* Request/Query/ACK/End carry no original AGE. */
    }
}

/* Transport refresh of the exact same source observation. Caller first validates
 * age/identity and preserves the newer raw/capture baseline without replaying Owner. */
bool net_upper_can_same_observation(const NetUpperCanMessage *old,const NetUpperCanMessage *m)
{
    if(old==NULL || m==NULL || old->id!=m->id)return false;
    switch(m->id) {
    case 0x080u:
        /* A restricted quality of the same evaluation is new to deliver. */
        return permission_same(&old->data.domain_permission,&m->data.domain_permission) &&
            old->data.domain_permission.power_permission_quality==m->data.domain_permission.power_permission_quality &&
            old->data.domain_permission.operation_quality==m->data.domain_permission.operation_quality;
    case 0x100u:
        return (old->data.bcm_door_command.boot_id==m->data.bcm_door_command.boot_id && old->data.bcm_door_command.target_boot_id==m->data.bcm_door_command.target_boot_id && old->data.bcm_door_command.command_id==m->data.bcm_door_command.command_id);
    case 0x110u:
        return (old->data.bcm_climate_command.boot_id==m->data.bcm_climate_command.boot_id && old->data.bcm_climate_command.target_boot_id==m->data.bcm_climate_command.target_boot_id && old->data.bcm_climate_command.command_id==m->data.bcm_climate_command.command_id);
    case 0x120u:
        return (old->data.bcm_light_command.boot_id==m->data.bcm_light_command.boot_id && old->data.bcm_light_command.target_boot_id==m->data.bcm_light_command.target_boot_id && old->data.bcm_light_command.command_id==m->data.bcm_light_command.command_id);
    case 0x090u:
        return (old->data.window_command.boot_id==m->data.window_command.boot_id && old->data.window_command.target_boot_id==m->data.window_command.target_boot_id && old->data.window_command.command_id==m->data.window_command.command_id);
    case 0x200u:
        return (old->data.bcm_door_state.boot_id==m->data.bcm_door_state.boot_id && old->data.bcm_door_state.update_sequence==m->data.bcm_door_state.update_sequence);
    case 0x210u:
        return (old->data.bcm_climate_state.boot_id==m->data.bcm_climate_state.boot_id && old->data.bcm_climate_state.update_sequence==m->data.bcm_climate_state.update_sequence);
    case 0x220u:
        return (old->data.bcm_light_state.boot_id==m->data.bcm_light_state.boot_id && old->data.bcm_light_state.update_sequence==m->data.bcm_light_state.update_sequence);
    case 0x141u:
        return (old->data.bcm_event.boot_id==m->data.bcm_event.boot_id && old->data.bcm_event.occurrence_id==m->data.bcm_event.occurrence_id);
    case 0x230u:
        return (old->data.bcm_status.boot_id==m->data.bcm_status.boot_id && old->data.bcm_status.update_sequence==m->data.bcm_status.update_sequence);
    case 0x300u:
        return (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.temperature_sequence==m->data.cis_environment.temperature_sequence) &&
            (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.humidity_sequence==m->data.cis_environment.humidity_sequence) &&
            (old->data.cis_environment.boot_id==m->data.cis_environment.boot_id && old->data.cis_environment.illuminance_sequence==m->data.cis_environment.illuminance_sequence);
    case 0x310u:
        return (old->data.cis_occupant.boot_id==m->data.cis_occupant.boot_id && old->data.cis_occupant.pi_boot_id==m->data.cis_occupant.pi_boot_id && old->data.cis_occupant.vision_sequence==m->data.cis_occupant.vision_sequence);
    case 0x0A0u:
        return (old->data.cis_rear.boot_id==m->data.cis_rear.boot_id && old->data.cis_rear.update_sequence==m->data.cis_rear.update_sequence);
    case 0x320u:
        return (old->data.cis_status.boot_id==m->data.cis_status.boot_id && old->data.cis_status.update_sequence==m->data.cis_status.update_sequence);
    case 0x0B0u:
        return (old->data.window_state.boot_id==m->data.window_state.boot_id && old->data.window_state.update_sequence==m->data.window_state.update_sequence);
    case 0x091u:
        return (old->data.window_event.boot_id==m->data.window_event.boot_id && old->data.window_event.occurrence_id==m->data.window_event.occurrence_id);
    case 0x330u:
        return (old->data.window_fault.boot_id==m->data.window_fault.boot_id && old->data.window_fault.update_sequence==m->data.window_fault.update_sequence);
    case 0x160u:
        return (old->data.vss_event.boot_id==m->data.vss_event.boot_id && old->data.vss_event.occurrence_id==m->data.vss_event.occurrence_id);
    case 0x0C0u: {
        /* Every lane the same identity and a restricted quality on none. */
        const NetWireVssWarningStateRaw *a=&old->data.vss_warning_state,*b=&m->data.vss_warning_state;
        return warning_lane_same(a,b,0u) && warning_lane_same(a,b,1u) && warning_lane_same(a,b,2u) &&
            warning_lane_same(a,b,3u) && warning_quality_equal(a,b);
    }
    case 0x340u:
        return (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.last_fault_occurrence==m->data.vss_status.last_fault_occurrence && old->data.vss_status.last_fault_quality==0u && m->data.vss_status.last_fault_quality==0u) &&
            (old->data.vss_status.boot_id==m->data.vss_status.boot_id && old->data.vss_status.update_sequence==m->data.vss_status.update_sequence);
    default:return false;
    }
}

/* Transport refresh of the exact same source observation. Caller first validates
 * age/identity and preserves the newer raw/capture baseline without replaying Owner. */
bool net_upper_uart_same_observation(const NetUpperUartMessage *old,const NetUpperUartMessage *m)
{
    if(old==NULL || m==NULL || old->type!=m->type)return false;
    if(old->link!=m->link)return false;
    switch(m->type) {
    case 0x01u:
        return (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.app_update_sequence==m->data.m_context.app_update_sequence) &&
            (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.proximity_update_sequence==m->data.m_context.proximity_update_sequence) &&
            (old->data.m_context.device_context_id==m->data.m_context.device_context_id && old->data.m_context.session_id==m->data.m_context.session_id && old->data.m_context.registration_connection_update_sequence==m->data.m_context.registration_connection_update_sequence);
    case 0x02u:
        return (old->data.m_domain_alive.domain_boot_id==m->data.m_domain_alive.domain_boot_id && old->data.m_domain_alive.update_sequence==m->data.m_domain_alive.update_sequence);
    case 0x22u:
        return (old->data.m_availability.domain_boot_id==m->data.m_availability.domain_boot_id && old->data.m_availability.update_sequence==m->data.m_availability.update_sequence) &&
            (old->data.m_availability.query_id==m->data.m_availability.query_id);
    case 0x23u:
        return (old->data.m_digital_status.domain_boot_id==m->data.m_digital_status.domain_boot_id && old->data.m_digital_status.update_sequence==m->data.m_digital_status.update_sequence) &&
            (old->data.m_digital_status.query_id==m->data.m_digital_status.query_id);
    case 0x25u:
        return (old->data.m_user_settings.domain_boot_id==m->data.m_user_settings.domain_boot_id && old->data.m_user_settings.update_sequence==m->data.m_user_settings.update_sequence) &&
            (old->data.m_user_settings.query_id==m->data.m_user_settings.query_id);
    case 0x60u: case 0x61u:
        /* Accepted newer-LINK Vision repeats feed the existing Owner receive
         * monitor; they are not a new source evaluation (UART-C VC-F04). */
        return false;
    case 0x62u:
        return (old->data.p_permission.cis_boot_id==m->data.p_permission.cis_boot_id && old->data.p_permission.update_sequence==m->data.p_permission.update_sequence);
    case 0x30u:
        return (old->data.m_bcm_door_state.original_can_boot_id==m->data.m_bcm_door_state.original_can_boot_id && old->data.m_bcm_door_state.update_sequence==m->data.m_bcm_door_state.update_sequence) &&
            (old->data.m_bcm_door_state.query_id==m->data.m_bcm_door_state.query_id);
    case 0x31u:
        return (old->data.m_bcm_climate_state.original_can_boot_id==m->data.m_bcm_climate_state.original_can_boot_id && old->data.m_bcm_climate_state.update_sequence==m->data.m_bcm_climate_state.update_sequence) &&
            (old->data.m_bcm_climate_state.query_id==m->data.m_bcm_climate_state.query_id);
    case 0x32u:
        return (old->data.m_bcm_light_state.original_can_boot_id==m->data.m_bcm_light_state.original_can_boot_id && old->data.m_bcm_light_state.update_sequence==m->data.m_bcm_light_state.update_sequence) &&
            (old->data.m_bcm_light_state.query_id==m->data.m_bcm_light_state.query_id);
    case 0x33u:
        return (old->data.m_bcm_status.original_can_boot_id==m->data.m_bcm_status.original_can_boot_id && old->data.m_bcm_status.update_sequence==m->data.m_bcm_status.update_sequence) &&
            (old->data.m_bcm_status.query_id==m->data.m_bcm_status.query_id);
    case 0x34u:
        return (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.temperature_sequence==m->data.m_cis_environment.temperature_sequence) &&
            (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.humidity_sequence==m->data.m_cis_environment.humidity_sequence) &&
            (old->data.m_cis_environment.original_can_boot_id==m->data.m_cis_environment.original_can_boot_id && old->data.m_cis_environment.illuminance_sequence==m->data.m_cis_environment.illuminance_sequence) &&
            (old->data.m_cis_environment.query_id==m->data.m_cis_environment.query_id);
    case 0x35u:
        return (old->data.m_cis_occupant.original_can_boot_id==m->data.m_cis_occupant.original_can_boot_id && old->data.m_cis_occupant.pi_boot_id==m->data.m_cis_occupant.pi_boot_id && old->data.m_cis_occupant.vision_sequence==m->data.m_cis_occupant.vision_sequence) &&
            (old->data.m_cis_occupant.query_id==m->data.m_cis_occupant.query_id);
    case 0x36u:
        return (old->data.m_cis_rear.original_can_boot_id==m->data.m_cis_rear.original_can_boot_id && old->data.m_cis_rear.update_sequence==m->data.m_cis_rear.update_sequence) &&
            (old->data.m_cis_rear.query_id==m->data.m_cis_rear.query_id);
    case 0x37u:
        return (old->data.m_cis_status.original_can_boot_id==m->data.m_cis_status.original_can_boot_id && old->data.m_cis_status.update_sequence==m->data.m_cis_status.update_sequence) &&
            (old->data.m_cis_status.query_id==m->data.m_cis_status.query_id);
    case 0x38u:
        return (old->data.m_window_state.original_can_boot_id==m->data.m_window_state.original_can_boot_id && old->data.m_window_state.update_sequence==m->data.m_window_state.update_sequence) &&
            (old->data.m_window_state.query_id==m->data.m_window_state.query_id);
    case 0x39u:
        return (old->data.m_window_fault.original_can_boot_id==m->data.m_window_fault.original_can_boot_id && old->data.m_window_fault.update_sequence==m->data.m_window_fault.update_sequence) &&
            (old->data.m_window_fault.query_id==m->data.m_window_fault.query_id);
    case 0x3au:
        return (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.last_fault_occurrence==m->data.m_vss_status.last_fault_occurrence && old->data.m_vss_status.last_fault_quality==0u && m->data.m_vss_status.last_fault_quality==0u) &&
            (old->data.m_vss_status.original_can_boot_id==m->data.m_vss_status.original_can_boot_id && old->data.m_vss_status.update_sequence==m->data.m_vss_status.update_sequence) &&
            (old->data.m_vss_status.query_id==m->data.m_vss_status.query_id);
    default:return false;
    }
}
