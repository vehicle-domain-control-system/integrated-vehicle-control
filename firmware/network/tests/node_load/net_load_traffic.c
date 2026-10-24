/* Documented periodic traffic for the node load cases (net_load_traffic.h).
 * Contract: docs/network/interface/03_COMMUNICATION/10_CAN_FD.md (common
 * header: BOOT_ID of the sending Network, TX_SEQUENCE per message from 1,
 * +1 per frame; message list and periods). The content of every frame is a
 * valid observation of its sender, encoded with the repository codec. */
#include "net_load_traffic.h"
#include "network/app/net_if_bcm.h"
#include "network/app/net_if_cis.h"
#include "network/app/net_if_vss.h"
#include <stdio.h>
#include <string.h>

const NetLoadMessage net_load_messages[NET_LOAD_MESSAGE_COUNT] = {
    { 0x080u, 50u, NET_LOAD_DOMAIN },   /* DOMAIN_PERMISSION */
    { 0x0C0u, 50u, NET_LOAD_DOMAIN },   /* VSS_WARNING_STATE */
    { 0x0A0u, 50u, NET_LOAD_CIS },      /* CIS_REAR */
    { 0x300u, 200u, NET_LOAD_CIS },     /* CIS_ENVIRONMENT */
    { 0x310u, 100u, NET_LOAD_CIS },     /* CIS_OCCUPANT */
    { 0x320u, 250u, NET_LOAD_CIS },     /* CIS_STATUS */
    { 0x200u, 100u, NET_LOAD_BCM },     /* BCM_DOOR_STATE */
    { 0x210u, 200u, NET_LOAD_BCM },     /* BCM_CLIMATE_STATE */
    { 0x220u, 200u, NET_LOAD_BCM },     /* BCM_LIGHT_STATE */
    { 0x230u, 250u, NET_LOAD_BCM },     /* BCM_STATUS */
    { 0x340u, 250u, NET_LOAD_VSS }      /* VSS_STATUS */
};

uint32_t net_load_boot(NetLoadSender sender)
{
    switch (sender) {
    case NET_LOAD_DOMAIN: return 0x444F4D31u;
    case NET_LOAD_CIS: return 0x43495331u;
    case NET_LOAD_BCM: return 0x42434D31u;
    default: return 0x56535331u;
    }
}

static bool permission(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireDomainPermissionRaw p;
    memset(&p, 0, sizeof(p));
    p.boot_id = boot;
    p.tx_sequence = n + 1u;
    p.vehicle_power_permission = NET_TRISTATE_YES;
    p.power_permission_quality = NET_QUALITY_OK;
    p.power_operation_state = 1u;
    p.operation_allowed = NET_FLAG_YES;
    p.operation_quality = NET_QUALITY_OK;
    p.update_sequence = n / 4u + 1u;                    /* new evaluation every 200 ms */
    p.source_age_ms = (uint16_t)(10u + 50u * (n % 4u)); /* repetitions: AGE grows with the wait */
    return net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, out) == NET_WIRE_OK;
}

static bool warning_state(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireVssWarningStateRaw w;
    memset(&w, 0, sizeof(w));
    w.boot_id = boot;
    w.tx_sequence = n + 1u;
    w.antipinch_source_boot = 0x57494E31u;
    w.antipinch_source_sequence = n + 1u;
    w.antipinch_original_age_ms = 20u;
    w.antipinch_state = NET_FLAG_NO;
    w.antipinch_quality = NET_QUALITY_OK;
    w.antipinch_reason = NET_QUALITY_REASON_NONE;
    w.antipinch_availability = NET_AVAILABILITY_AVAILABLE;
    w.occupant_hazard_source_boot = net_load_boot(NET_LOAD_CIS);
    w.occupant_hazard_source_sequence = n + 1u;
    w.occupant_hazard_original_age_ms = 30u;
    w.occupant_hazard_state = NET_FLAG_NO;
    w.occupant_hazard_quality = NET_QUALITY_OK;
    w.occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    w.occupant_hazard_availability = NET_AVAILABILITY_AVAILABLE;
    w.rear_obstacle_source_boot = net_load_boot(NET_LOAD_CIS);
    w.rear_obstacle_source_sequence = n + 1u;
    w.rear_obstacle_original_age_ms = 15u;
    w.rear_obstacle_state = NET_REAR_WARNING_CAUTION;
    w.rear_obstacle_quality = NET_QUALITY_OK;
    w.rear_obstacle_reason = NET_QUALITY_REASON_NONE;
    w.rear_obstacle_availability = NET_AVAILABILITY_AVAILABLE;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_quality = NET_QUALITY_OK;
    w.rear_activation_reason = NET_QUALITY_REASON_NONE;
    w.rear_activation_sequence = n + 1u;
    w.rear_activation_age_ms = 10u;
    return net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, out) == NET_WIRE_OK;
}

static bool cis_rear(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireCisRearRaw r;
    memset(&r, 0, sizeof(r));
    r.boot_id = boot;
    r.tx_sequence = n + 1u;
    r.update_sequence = n + 1u;
    r.source_age_ms = 5u;
    r.rear_distance_deci_cm = 1234u;
    r.validity = NET_VALIDITY_VALID;
    r.quality_reason = NET_QUALITY_REASON_NONE;
    r.proximity_status = NET_PROXIMITY_VALID_DISTANCE;
    return net_wire_can_encode_rear(NET_WIRE_CANDIDATE_ONLY, &r, out) == NET_WIRE_OK;
}

static bool cis_environment(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireCisEnvironmentRaw v;
    memset(&v, 0, sizeof(v));
    v.boot_id = boot;
    v.tx_sequence = n + 1u;
    v.temperature_centi_c = 2350;
    v.temperature_sequence = n + 1u;
    v.temperature_age_ms = 40u;
    v.temperature_validity = NET_VALIDITY_VALID;
    v.temperature_reason = NET_QUALITY_REASON_NONE;
    v.humidity_centi_percent = 4500u;
    v.humidity_sequence = n + 1u;
    v.humidity_age_ms = 50u;
    v.humidity_validity = NET_VALIDITY_VALID;
    v.humidity_reason = NET_QUALITY_REASON_NONE;
    v.illuminance_lux = 300u;
    v.illuminance_sequence = n + 1u;
    v.illuminance_age_ms = 60u;
    v.illuminance_validity = NET_VALIDITY_VALID;
    v.illuminance_reason = NET_QUALITY_REASON_NONE;
    return net_wire_can_encode_cis_environment(NET_WIRE_CANDIDATE_ONLY, &v, out) == NET_WIRE_OK;
}

static bool cis_occupant(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireCisOccupantRaw o;
    memset(&o, 0, sizeof(o));
    o.boot_id = boot;
    o.tx_sequence = n + 1u;
    o.pi_boot_id = 0x50490001u;
    o.vision_sequence = n + 1u;
    o.vision_age_ms = 25u;
    o.occupant_presence = NET_TRISTATE_YES;
    o.presence_validity = NET_VALIDITY_VALID;
    o.presence_reason = NET_QUALITY_REASON_NONE;
    o.occupant_count = 2u;
    o.count_validity = NET_VALIDITY_VALID;
    o.count_reason = NET_QUALITY_REASON_NONE;
    o.vision_function_status = NET_SENSOR_FUNCTION_ACTIVE;
    return net_wire_can_encode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &o, out) == NET_WIRE_OK;
}

static bool cis_status(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireCisStatusRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.cis_state = NET_CIS_STATE_ACTIVE;
    s.vision_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.temperature_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.humidity_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.illuminance_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.rear_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.pi_uart_status = NET_INTERFACE_READY;
    s.can_interface_status = NET_INTERFACE_READY;
    s.fault_mask = 0u;
    s.active_fault = NET_CIS_FAULT_CODE_NONE;
    s.recovering = NET_FLAG_NO;
    s.last_fault = NET_CIS_FAULT_CODE_NONE;
    s.last_fault_occurrence = 0u;
    s.affected_function = 0u;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_cis_status(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

static bool bcm_door_state(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireBcmDoorStateRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.door_lock_state = NET_DOOR_LOCK_LOCKED;
    s.door_open_state = NET_DOOR_OPEN_CLOSED;
    s.door_composite_state = NET_DOOR_COMPOSITE_NORMAL;
    s.lock_quality = NET_QUALITY_OK;
    s.open_quality = NET_QUALITY_OK;
    s.composite_quality = NET_QUALITY_OK;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_bcm_door_state(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

static bool bcm_climate_state(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireBcmClimateStateRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.fan_command_level = NET_FAN_HIGH;
    s.fan_measured_level = NET_FAN_HIGH;
    s.thermal_direction_state = NET_THERMAL_HEAT;
    s.thermal_output_percent = 40u;
    s.heat_removal_state = NET_HEAT_REMOVAL_UNAVAILABLE;
    s.fan_command_quality = NET_QUALITY_OK;
    s.fan_measured_quality = NET_QUALITY_OK;
    s.thermal_direction_quality = NET_QUALITY_OK;
    s.thermal_output_quality = NET_QUALITY_OK;
    s.heat_removal_quality = NET_QUALITY_OK;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_bcm_climate_state(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

static bool bcm_light_state(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireBcmLightStateRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.interior_light_type_state = NET_LIGHT_WARNING;
    s.light_level_percent = 55u;
    s.red = 1u; s.green = 2u; s.blue = 3u;
    s.interior_light_apply_result = NET_LIGHT_APPLY_APPLIED;
    s.light_physical_feedback_capability = NET_FLAG_NO;
    s.light_type_quality = NET_QUALITY_OK;
    s.light_level_quality = NET_QUALITY_OK;
    s.light_color_quality = NET_QUALITY_OK;
    s.light_apply_quality = NET_QUALITY_OK;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_bcm_light_state(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

static bool bcm_status(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireBcmStatusRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.bcm_ecu_state = NET_CONTROLLER_READY;
    s.fault_mask = 0u;
    s.active_fault_category = NET_FAULT_CATEGORY_NONE;
    s.active_fault_code = NET_BCM_FAULT_CODE_NONE;
    s.affected_function = 0u;
    s.recovering = NET_FLAG_NO;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_bcm_status(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

static bool vss_status(uint32_t boot, uint32_t n, NetWireCanEnvelope *out)
{
    NetWireVssStatusRaw s;
    memset(&s, 0, sizeof(s));
    s.boot_id = boot;
    s.tx_sequence = n + 1u;
    s.vss_state = NET_VSS_STATE_READY;
    s.vss_availability = NET_VSS_AVAILABILITY_FULL;
    s.vss_accepting_events = NET_FLAG_YES;
    s.vss_fault_active = NET_FLAG_NO;
    s.fault_mask = 0u;
    s.vss_last_fault = NET_VSS_FAULT_CODE_NONE;
    s.last_fault_occurrence = 0u;
    s.last_fault_age_ms = 0u;
    s.last_fault_quality = NET_QUALITY_NO_DATA;
    s.update_sequence = n + 1u;
    s.source_age_ms = 5u;
    return net_wire_can_encode_vss_status(NET_WIRE_CANDIDATE_ONLY, &s, out) == NET_WIRE_OK;
}

bool net_load_frame(size_t k, uint32_t n, NetWireCanEnvelope *out)
{
    uint32_t boot;
    if (k >= NET_LOAD_MESSAGE_COUNT || out == NULL) return false;
    boot = net_load_boot(net_load_messages[k].sender);
    switch (net_load_messages[k].id) {
    case 0x080u: return permission(boot, n, out);
    case 0x0C0u: return warning_state(boot, n, out);
    case 0x0A0u: return cis_rear(boot, n, out);
    case 0x300u: return cis_environment(boot, n, out);
    case 0x310u: return cis_occupant(boot, n, out);
    case 0x320u: return cis_status(boot, n, out);
    case 0x200u: return bcm_door_state(boot, n, out);
    case 0x210u: return bcm_climate_state(boot, n, out);
    case 0x220u: return bcm_light_state(boot, n, out);
    case 0x230u: return bcm_status(boot, n, out);
    case 0x340u: return vss_status(boot, n, out);
    default: return false;
    }
}

void net_load_tx_init(NetLoadTx *t, uint16_t id, uint32_t period_ms, uint32_t slack_ms)
{
    memset(t, 0, sizeof(*t));
    t->id = id;
    t->period_ms = period_ms;
    t->limit_ms = (uint64_t)period_ms + slack_ms;
}

bool net_load_tx_seen(NetLoadTx *t, size_t count, const NetWireCanEnvelope *e, uint64_t at_ms)
{
    size_t i;
    uint32_t sequence;
    for (i = 0u; i < count; ++i) {
        NetLoadTx *m = &t[i];
        if (m->id != e->standard_id) continue;
        /* Common header B4..7: TX_SEQUENCE, u32 little endian. */
        sequence = (uint32_t)e->bytes[4] | ((uint32_t)e->bytes[5] << 8) |
                   ((uint32_t)e->bytes[6] << 16) | ((uint32_t)e->bytes[7] << 24);
        /* From 1 in the sender run, +1 per frame (10_CAN_FD.md common header). */
        if (e->length < 8u || sequence != (m->frames == 0u ? 1u : m->last_sequence + 1u)) ++m->sequence_gaps;
        m->last_sequence = sequence;
        if (m->frames == 0u) {
            m->first_ms = at_ms;
        } else {
            if (at_ms - m->last_ms > m->max_gap_ms) m->max_gap_ms = at_ms - m->last_ms;
            if (at_ms - m->last_ms > m->limit_ms) ++m->violations;
        }
        m->last_ms = at_ms;
        ++m->frames;
        return true;
    }
    return false;
}

uint32_t net_load_tx_violations(const NetLoadTx *t, size_t count, uint64_t end_ms)
{
    size_t i;
    uint32_t n = 0u;
    for (i = 0u; i < count; ++i) {
        if (t[i].frames == 0u) { ++n; continue; }
        n += t[i].violations;
        if (end_ms > t[i].last_ms && end_ms - t[i].last_ms > t[i].limit_ms) ++n;
    }
    return n;
}

uint32_t net_load_tx_frames(const NetLoadTx *t, size_t count)
{
    size_t i;
    uint32_t n = 0u;
    for (i = 0u; i < count; ++i) n += t[i].frames;
    return n;
}

uint32_t net_load_tx_gaps(const NetLoadTx *t, size_t count)
{
    size_t i;
    uint32_t n = 0u;
    for (i = 0u; i < count; ++i) n += t[i].sequence_gaps;
    return n;
}

uint32_t net_load_report(const char *label, const NetLoadCount *c, size_t count)
{
    size_t i;
    uint32_t sum = 0u;
    bool any = false;
    printf("%s:", label);
    for (i = 0u; i < count; ++i) {
        if (c[i].value == 0u) continue;
        printf(" %s=%lu", c[i].name, (unsigned long)c[i].value);
        sum = c[i].value > UINT32_MAX - sum ? UINT32_MAX : sum + c[i].value;
        any = true;
    }
    printf("%s\n", any ? "" : " none");
    return sum;
}
