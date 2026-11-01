/* UART-M frames of the ESP32 stand-in (tools/esp32_bench) with the
 * repository codec, for its host cross-check: every UART-M message, both
 * directions. Built from field values (encode) and decoded from copied bytes
 * (decode). As in the rest of vss_bench every verdict is the codec verdict.
 * Field tables follow the codec structures (net_wire_uart_dto.h,
 * net_wire_uart_m_core.h, net_wire_uart_m_projection.h) member by member. */
#include "vss_bench.h"

#include "network/net_wire.h"
#include "network/net_wire_uart.h"
#include "network/net_wire_uart_dto.h"
#include "network/net_wire_uart_m_core.h"
#include "network/net_wire_uart_m_projection.h"

#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROFILE NET_WIRE_CANDIDATE_ONLY
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Appends to the text buffer (the same rule as vss_bench.c). */
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

static const char *status_name(NetWireStatus s)
{
    switch (s) {
    case NET_WIRE_OK: return "OK";
    case NET_WIRE_MALFORMED: return "MALFORMED";
    case NET_WIRE_CONTRACT_HOLD: return "CONTRACT_HOLD";
    case NET_WIRE_UNSUPPORTED: return "UNSUPPORTED";
    case NET_WIRE_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    default: return "OTHER";
    }
}

/* One member of a codec structure: 1, 2, 4 or 8 bytes, unsigned or (the
 * temperatures) two's complement. Every member is settable. */
typedef struct {
    const char *name;
    size_t offset;
    uint8_t width;
    bool sign;
} Field;

#define MF(type, m) { #m, offsetof(type, m), (uint8_t)sizeof(((type *)0)->m), false }
#define MS(type, m) { #m, offsetof(type, m), (uint8_t)sizeof(((type *)0)->m), true }

static uint64_t field_get(const void *raw, const Field *f)
{
    const uint8_t *p = (const uint8_t *)raw + f->offset;
    switch (f->width) {
    case 1u: return *p;
    case 2u: { uint16_t v; memcpy(&v, p, 2u); return v; }
    case 4u: { uint32_t v; memcpy(&v, p, 4u); return v; }
    default: { uint64_t v; memcpy(&v, p, 8u); return v; }
    }
}

static void field_put(void *raw, const Field *f, uint64_t value)
{
    uint8_t *p = (uint8_t *)raw + f->offset;
    switch (f->width) {
    case 1u: *p = (uint8_t)value; break;
    case 2u: { uint16_t v = (uint16_t)value; memcpy(p, &v, 2u); break; }
    case 4u: { uint32_t v = (uint32_t)value; memcpy(p, &v, 4u); break; }
    default: memcpy(p, &value, 8u); break;
    }
}

/* field=value: decimal or 0x hex; a signed member also takes -N. */
static bool parse_value(const Field *f, const char *s, uint64_t *out)
{
    char *end = NULL;
    uint64_t max = f->width == 8u ? UINT64_MAX : (UINT64_C(1) << (8u * f->width)) - 1u;
    if (s == NULL || *s == '\0' || *s == ' ' || *s == '+') return false;
    if (*s == '-') {
        long long v;
        if (!f->sign) return false;
        errno = 0;
        v = strtoll(s, &end, 0);
        if (end == s || *end != '\0' || errno == ERANGE || v < -32768) return false;
        *out = (uint64_t)(uint16_t)(int16_t)v;
        return true;
    }
    errno = 0;
    *out = strtoull(s, &end, 0);
    return end != s && *end == '\0' && errno != ERANGE && *out <= max && (!f->sign || *out <= 32767u);
}

static void say_fields(VssBenchText *t, const void *raw, const Field *fields, size_t count)
{
    size_t i;
    say(t, "fields:");
    for (i = 0u; i < count; ++i) {
        uint64_t v = field_get(raw, &fields[i]);
        if (fields[i].sign) say(t, " %s=%d", fields[i].name, (int)(int16_t)(uint16_t)v);
        else say(t, " %s=%llu", fields[i].name, (unsigned long long)v);
    }
    say(t, "\n");
}

typedef struct {
    uint8_t type;
    const Field *fields;
    size_t field_count;
    NetWireStatus (*encode)(uint16_t seq, const void *r, NetWireUartFrame *f);
    NetWireStatus (*decode)(const NetWireUartFrame *f, void *r, uint16_t *seq);
} UartMMsg;

typedef union {
    NetWireMContext context;
    NetWireMDomainAliveRaw domain_alive;
    NetWireMRequest request;
    NetWireMQueryRaw query;
    NetWireMWarningAckRaw warning_ack;
    NetWireMResult result;
    NetWireMWarningRaw warning;
    NetWireMAvailabilityRaw availability;
    NetWireMDigitalStatusRaw digital_status;
    NetWireMDigitalResultRaw digital_result;
    NetWireMUserSettingsRaw user_settings;
    NetWireMBcmDoorStateRaw bcm_door_state;
    NetWireMBcmClimateStateRaw bcm_climate_state;
    NetWireMBcmLightStateRaw bcm_light_state;
    NetWireMBcmStatusRaw bcm_status;
    NetWireMCisEnvironmentRaw cis_environment;
    NetWireMCisOccupantRaw cis_occupant;
    NetWireMCisRearRaw cis_rear;
    NetWireMCisStatusRaw cis_status;
    NetWireMWindowStateRaw window_state;
    NetWireMWindowFaultRaw window_fault;
    NetWireMVssStatusRaw vss_status;
    NetWireMQueryEndRaw query_end;
} UartMRaw;

static const Field m_context_fields[] = {
    MF(NetWireMContext, device_context_id),
    MF(NetWireMContext, session_id),
    MF(NetWireMContext, device_registration_state),
    MF(NetWireMContext, bt_connection_state),
    MF(NetWireMContext, app_active_state),
    MF(NetWireMContext, app_quality),
    MF(NetWireMContext, app_update_sequence),
    MF(NetWireMContext, app_age_ms),
    MF(NetWireMContext, proximity_state),
    MF(NetWireMContext, proximity_validity),
    MF(NetWireMContext, proximity_reason),
    MF(NetWireMContext, proximity_update_sequence),
    MF(NetWireMContext, proximity_age_ms),
    MF(NetWireMContext, registration_connection_quality),
    MF(NetWireMContext, registration_connection_update_sequence),
    MF(NetWireMContext, registration_connection_age_ms),
};
static const Field m_domain_alive_fields[] = {
    MF(NetWireMDomainAliveRaw, domain_boot_id),
    MF(NetWireMDomainAliveRaw, update_sequence),
    MF(NetWireMDomainAliveRaw, source_age_ms),
    MF(NetWireMDomainAliveRaw, domain_state),
    MF(NetWireMDomainAliveRaw, value_quality),
};
static const Field m_request_fields[] = {
    MF(NetWireMRequest, device_context_id),
    MF(NetWireMRequest, session_id),
    MF(NetWireMRequest, request_id),
    MF(NetWireMRequest, kind),
    MF(NetWireMRequest, operation),
    MF(NetWireMRequest, args[0]),
    MF(NetWireMRequest, args[1]),
    MF(NetWireMRequest, args[2]),
    MF(NetWireMRequest, args[3]),
    MF(NetWireMRequest, args[4]),
    MF(NetWireMRequest, args[5]),
};
static const Field m_query_fields[] = {
    MF(NetWireMQueryRaw, device_context_id),
    MF(NetWireMQueryRaw, session_id),
    MF(NetWireMQueryRaw, query_id),
    MF(NetWireMQueryRaw, query_scope),
    MF(NetWireMQueryRaw, reserved),
    MF(NetWireMQueryRaw, request_session),
    MF(NetWireMQueryRaw, request_id),
    MF(NetWireMQueryRaw, cursor),
};
static const Field m_warning_ack_fields[] = {
    MF(NetWireMWarningAckRaw, device_context_id),
    MF(NetWireMWarningAckRaw, session_id),
    MF(NetWireMWarningAckRaw, warning_type),
    MF(NetWireMWarningAckRaw, occurrence_id),
    MF(NetWireMWarningAckRaw, warning_domain_boot),
};
static const Field m_result_fields[] = {
    MF(NetWireMResult, device_context_id),
    MF(NetWireMResult, session_id),
    MF(NetWireMResult, domain_boot_id),
    MF(NetWireMResult, query_id),
    MF(NetWireMResult, request_session),
    MF(NetWireMResult, request_id),
    MF(NetWireMResult, result),
    MF(NetWireMResult, result_reason),
    MF(NetWireMResult, result_confirmation),
    MF(NetWireMResult, command_target),
    MF(NetWireMResult, command_id),
    MF(NetWireMResult, origin),
    MF(NetWireMResult, result_age_ms),
};
static const Field m_warning_fields[] = {
    MF(NetWireMWarningRaw, device_context_id),
    MF(NetWireMWarningRaw, session_id),
    MF(NetWireMWarningRaw, domain_boot_id),
    MF(NetWireMWarningRaw, query_id),
    MF(NetWireMWarningRaw, warning_type),
    MF(NetWireMWarningRaw, occurrence_id),
    MF(NetWireMWarningRaw, severity),
    MF(NetWireMWarningRaw, warning_state),
    MF(NetWireMWarningRaw, read_state),
    MF(NetWireMWarningRaw, value_quality),
    MF(NetWireMWarningRaw, quality_reason),
    MF(NetWireMWarningRaw, source_boot),
    MF(NetWireMWarningRaw, source_sequence),
    MF(NetWireMWarningRaw, source_age_ms),
};
static const Field m_availability_fields[] = {
    MF(NetWireMAvailabilityRaw, device_context_id),
    MF(NetWireMAvailabilityRaw, session_id),
    MF(NetWireMAvailabilityRaw, domain_boot_id),
    MF(NetWireMAvailabilityRaw, query_id),
    MF(NetWireMAvailabilityRaw, items[0].function_id),
    MF(NetWireMAvailabilityRaw, items[0].availability),
    MF(NetWireMAvailabilityRaw, items[0].reason),
    MF(NetWireMAvailabilityRaw, items[1].function_id),
    MF(NetWireMAvailabilityRaw, items[1].availability),
    MF(NetWireMAvailabilityRaw, items[1].reason),
    MF(NetWireMAvailabilityRaw, items[2].function_id),
    MF(NetWireMAvailabilityRaw, items[2].availability),
    MF(NetWireMAvailabilityRaw, items[2].reason),
    MF(NetWireMAvailabilityRaw, items[3].function_id),
    MF(NetWireMAvailabilityRaw, items[3].availability),
    MF(NetWireMAvailabilityRaw, items[3].reason),
    MF(NetWireMAvailabilityRaw, items[4].function_id),
    MF(NetWireMAvailabilityRaw, items[4].availability),
    MF(NetWireMAvailabilityRaw, items[4].reason),
    MF(NetWireMAvailabilityRaw, items[5].function_id),
    MF(NetWireMAvailabilityRaw, items[5].availability),
    MF(NetWireMAvailabilityRaw, items[5].reason),
    MF(NetWireMAvailabilityRaw, items[6].function_id),
    MF(NetWireMAvailabilityRaw, items[6].availability),
    MF(NetWireMAvailabilityRaw, items[6].reason),
    MF(NetWireMAvailabilityRaw, items[7].function_id),
    MF(NetWireMAvailabilityRaw, items[7].availability),
    MF(NetWireMAvailabilityRaw, items[7].reason),
    MF(NetWireMAvailabilityRaw, items[8].function_id),
    MF(NetWireMAvailabilityRaw, items[8].availability),
    MF(NetWireMAvailabilityRaw, items[8].reason),
    MF(NetWireMAvailabilityRaw, update_sequence),
    MF(NetWireMAvailabilityRaw, source_age_ms),
};
static const Field m_digital_status_fields[] = {
    MF(NetWireMDigitalStatusRaw, device_context_id),
    MF(NetWireMDigitalStatusRaw, session_id),
    MF(NetWireMDigitalStatusRaw, domain_boot_id),
    MF(NetWireMDigitalStatusRaw, query_id),
    MF(NetWireMDigitalStatusRaw, digital_key_setting_state),
    MF(NetWireMDigitalStatusRaw, digital_key_availability),
    MF(NetWireMDigitalStatusRaw, reason),
    MF(NetWireMDigitalStatusRaw, update_sequence),
    MF(NetWireMDigitalStatusRaw, source_age_ms),
};
static const Field m_digital_result_fields[] = {
    MF(NetWireMDigitalResultRaw, device_context_id),
    MF(NetWireMDigitalResultRaw, session_id),
    MF(NetWireMDigitalResultRaw, domain_boot_id),
    MF(NetWireMDigitalResultRaw, query_id),
    MF(NetWireMDigitalResultRaw, auto_occurrence_id),
    MF(NetWireMDigitalResultRaw, related_door_command_id),
    MF(NetWireMDigitalResultRaw, origin),
    MF(NetWireMDigitalResultRaw, result),
    MF(NetWireMDigitalResultRaw, result_reason),
    MF(NetWireMDigitalResultRaw, result_confirmation),
    MF(NetWireMDigitalResultRaw, result_age_ms),
};
static const Field m_user_settings_fields[] = {
    MF(NetWireMUserSettingsRaw, device_context_id),
    MF(NetWireMUserSettingsRaw, session_id),
    MF(NetWireMUserSettingsRaw, domain_boot_id),
    MF(NetWireMUserSettingsRaw, query_id),
    MS(NetWireMUserSettingsRaw, target_temperature_centi_c),
    MF(NetWireMUserSettingsRaw, climate_auto),
    MF(NetWireMUserSettingsRaw, user_fan_level),
    MF(NetWireMUserSettingsRaw, interior_light_enabled),
    MF(NetWireMUserSettingsRaw, normal_light_level_percent),
    MF(NetWireMUserSettingsRaw, normal_light_rgb[0]),
    MF(NetWireMUserSettingsRaw, normal_light_rgb[1]),
    MF(NetWireMUserSettingsRaw, normal_light_rgb[2]),
    MF(NetWireMUserSettingsRaw, value_quality),
    MF(NetWireMUserSettingsRaw, update_sequence),
    MF(NetWireMUserSettingsRaw, source_age_ms),
};
static const Field m_bcm_door_state_fields[] = {
    MF(NetWireMBcmDoorStateRaw, device_context_id),
    MF(NetWireMBcmDoorStateRaw, session_id),
    MF(NetWireMBcmDoorStateRaw, domain_boot_id),
    MF(NetWireMBcmDoorStateRaw, query_id),
    MF(NetWireMBcmDoorStateRaw, original_can_boot_id),
    MF(NetWireMBcmDoorStateRaw, original_can_tx_sequence),
    MF(NetWireMBcmDoorStateRaw, door_lock_state),
    MF(NetWireMBcmDoorStateRaw, door_open_state),
    MF(NetWireMBcmDoorStateRaw, door_composite_state),
    MF(NetWireMBcmDoorStateRaw, lock_quality),
    MF(NetWireMBcmDoorStateRaw, open_quality),
    MF(NetWireMBcmDoorStateRaw, composite_quality),
    MF(NetWireMBcmDoorStateRaw, update_sequence),
    MF(NetWireMBcmDoorStateRaw, source_age_ms),
};
static const Field m_bcm_climate_state_fields[] = {
    MF(NetWireMBcmClimateStateRaw, device_context_id),
    MF(NetWireMBcmClimateStateRaw, session_id),
    MF(NetWireMBcmClimateStateRaw, domain_boot_id),
    MF(NetWireMBcmClimateStateRaw, query_id),
    MF(NetWireMBcmClimateStateRaw, original_can_boot_id),
    MF(NetWireMBcmClimateStateRaw, original_can_tx_sequence),
    MF(NetWireMBcmClimateStateRaw, fan_command_level),
    MF(NetWireMBcmClimateStateRaw, fan_measured_level),
    MF(NetWireMBcmClimateStateRaw, thermal_direction_state),
    MF(NetWireMBcmClimateStateRaw, thermal_output_percent),
    MF(NetWireMBcmClimateStateRaw, heat_removal_state),
    MF(NetWireMBcmClimateStateRaw, fan_command_quality),
    MF(NetWireMBcmClimateStateRaw, fan_measured_quality),
    MF(NetWireMBcmClimateStateRaw, thermal_direction_quality),
    MF(NetWireMBcmClimateStateRaw, thermal_output_quality),
    MF(NetWireMBcmClimateStateRaw, heat_removal_quality),
    MF(NetWireMBcmClimateStateRaw, update_sequence),
    MF(NetWireMBcmClimateStateRaw, source_age_ms),
};
static const Field m_bcm_light_state_fields[] = {
    MF(NetWireMBcmLightStateRaw, device_context_id),
    MF(NetWireMBcmLightStateRaw, session_id),
    MF(NetWireMBcmLightStateRaw, domain_boot_id),
    MF(NetWireMBcmLightStateRaw, query_id),
    MF(NetWireMBcmLightStateRaw, original_can_boot_id),
    MF(NetWireMBcmLightStateRaw, original_can_tx_sequence),
    MF(NetWireMBcmLightStateRaw, interior_light_type_state),
    MF(NetWireMBcmLightStateRaw, light_level_percent),
    MF(NetWireMBcmLightStateRaw, color_r),
    MF(NetWireMBcmLightStateRaw, color_g),
    MF(NetWireMBcmLightStateRaw, color_b),
    MF(NetWireMBcmLightStateRaw, interior_light_apply_result),
    MF(NetWireMBcmLightStateRaw, light_physical_feedback_capability),
    MF(NetWireMBcmLightStateRaw, light_type_quality),
    MF(NetWireMBcmLightStateRaw, light_level_quality),
    MF(NetWireMBcmLightStateRaw, light_color_quality),
    MF(NetWireMBcmLightStateRaw, light_apply_quality),
    MF(NetWireMBcmLightStateRaw, update_sequence),
    MF(NetWireMBcmLightStateRaw, source_age_ms),
};
static const Field m_bcm_status_fields[] = {
    MF(NetWireMBcmStatusRaw, device_context_id),
    MF(NetWireMBcmStatusRaw, session_id),
    MF(NetWireMBcmStatusRaw, domain_boot_id),
    MF(NetWireMBcmStatusRaw, query_id),
    MF(NetWireMBcmStatusRaw, original_can_boot_id),
    MF(NetWireMBcmStatusRaw, original_can_tx_sequence),
    MF(NetWireMBcmStatusRaw, bcm_ecu_state),
    MF(NetWireMBcmStatusRaw, fault_mask),
    MF(NetWireMBcmStatusRaw, active_fault_category),
    MF(NetWireMBcmStatusRaw, active_fault_code),
    MF(NetWireMBcmStatusRaw, affected_function),
    MF(NetWireMBcmStatusRaw, recovering),
    MF(NetWireMBcmStatusRaw, update_sequence),
    MF(NetWireMBcmStatusRaw, source_age_ms),
};
static const Field m_cis_environment_fields[] = {
    MF(NetWireMCisEnvironmentRaw, device_context_id),
    MF(NetWireMCisEnvironmentRaw, session_id),
    MF(NetWireMCisEnvironmentRaw, domain_boot_id),
    MF(NetWireMCisEnvironmentRaw, query_id),
    MF(NetWireMCisEnvironmentRaw, original_can_boot_id),
    MF(NetWireMCisEnvironmentRaw, original_can_tx_sequence),
    MS(NetWireMCisEnvironmentRaw, temperature_centi_c),
    MF(NetWireMCisEnvironmentRaw, temperature_sequence),
    MF(NetWireMCisEnvironmentRaw, temperature_age_ms),
    MF(NetWireMCisEnvironmentRaw, temperature_validity),
    MF(NetWireMCisEnvironmentRaw, temperature_reason),
    MF(NetWireMCisEnvironmentRaw, humidity_centi_percent),
    MF(NetWireMCisEnvironmentRaw, humidity_sequence),
    MF(NetWireMCisEnvironmentRaw, humidity_age_ms),
    MF(NetWireMCisEnvironmentRaw, humidity_validity),
    MF(NetWireMCisEnvironmentRaw, humidity_reason),
    MF(NetWireMCisEnvironmentRaw, illuminance_lux),
    MF(NetWireMCisEnvironmentRaw, illuminance_sequence),
    MF(NetWireMCisEnvironmentRaw, illuminance_age_ms),
    MF(NetWireMCisEnvironmentRaw, illuminance_validity),
    MF(NetWireMCisEnvironmentRaw, illuminance_reason),
};
static const Field m_cis_occupant_fields[] = {
    MF(NetWireMCisOccupantRaw, device_context_id),
    MF(NetWireMCisOccupantRaw, session_id),
    MF(NetWireMCisOccupantRaw, domain_boot_id),
    MF(NetWireMCisOccupantRaw, query_id),
    MF(NetWireMCisOccupantRaw, original_can_boot_id),
    MF(NetWireMCisOccupantRaw, original_can_tx_sequence),
    MF(NetWireMCisOccupantRaw, pi_boot_id),
    MF(NetWireMCisOccupantRaw, vision_sequence),
    MF(NetWireMCisOccupantRaw, vision_age_ms),
    MF(NetWireMCisOccupantRaw, occupant_presence),
    MF(NetWireMCisOccupantRaw, presence_validity),
    MF(NetWireMCisOccupantRaw, presence_reason),
    MF(NetWireMCisOccupantRaw, occupant_count),
    MF(NetWireMCisOccupantRaw, count_validity),
    MF(NetWireMCisOccupantRaw, count_reason),
    MF(NetWireMCisOccupantRaw, vision_function_status),
};
static const Field m_cis_rear_fields[] = {
    MF(NetWireMCisRearRaw, device_context_id),
    MF(NetWireMCisRearRaw, session_id),
    MF(NetWireMCisRearRaw, domain_boot_id),
    MF(NetWireMCisRearRaw, query_id),
    MF(NetWireMCisRearRaw, original_can_boot_id),
    MF(NetWireMCisRearRaw, original_can_tx_sequence),
    MF(NetWireMCisRearRaw, update_sequence),
    MF(NetWireMCisRearRaw, source_age_ms),
    MF(NetWireMCisRearRaw, rear_distance_deci_cm),
    MF(NetWireMCisRearRaw, validity),
    MF(NetWireMCisRearRaw, quality_reason),
    MF(NetWireMCisRearRaw, proximity_status),
};
static const Field m_cis_status_fields[] = {
    MF(NetWireMCisStatusRaw, device_context_id),
    MF(NetWireMCisStatusRaw, session_id),
    MF(NetWireMCisStatusRaw, domain_boot_id),
    MF(NetWireMCisStatusRaw, query_id),
    MF(NetWireMCisStatusRaw, original_can_boot_id),
    MF(NetWireMCisStatusRaw, original_can_tx_sequence),
    MF(NetWireMCisStatusRaw, cis_state),
    MF(NetWireMCisStatusRaw, vision_status),
    MF(NetWireMCisStatusRaw, temperature_status),
    MF(NetWireMCisStatusRaw, humidity_status),
    MF(NetWireMCisStatusRaw, illuminance_status),
    MF(NetWireMCisStatusRaw, rear_status),
    MF(NetWireMCisStatusRaw, pi_uart_status),
    MF(NetWireMCisStatusRaw, can_interface_status),
    MF(NetWireMCisStatusRaw, fault_mask),
    MF(NetWireMCisStatusRaw, active_fault),
    MF(NetWireMCisStatusRaw, recovering),
    MF(NetWireMCisStatusRaw, last_fault),
    MF(NetWireMCisStatusRaw, last_fault_occurrence),
    MF(NetWireMCisStatusRaw, affected_function),
    MF(NetWireMCisStatusRaw, update_sequence),
    MF(NetWireMCisStatusRaw, source_age_ms),
};
static const Field m_window_state_fields[] = {
    MF(NetWireMWindowStateRaw, device_context_id),
    MF(NetWireMWindowStateRaw, session_id),
    MF(NetWireMWindowStateRaw, domain_boot_id),
    MF(NetWireMWindowStateRaw, query_id),
    MF(NetWireMWindowStateRaw, original_can_boot_id),
    MF(NetWireMWindowStateRaw, original_can_tx_sequence),
    MF(NetWireMWindowStateRaw, motion_state),
    MF(NetWireMWindowStateRaw, window_ecu_state),
    MF(NetWireMWindowStateRaw, window_position),
    MF(NetWireMWindowStateRaw, window_value_quality),
    MF(NetWireMWindowStateRaw, fully_open_state),
    MF(NetWireMWindowStateRaw, fully_closed_state),
    MF(NetWireMWindowStateRaw, antipinch_protection_status),
    MF(NetWireMWindowStateRaw, reverse_state),
    MF(NetWireMWindowStateRaw, update_sequence),
    MF(NetWireMWindowStateRaw, source_age_ms),
};
static const Field m_window_fault_fields[] = {
    MF(NetWireMWindowFaultRaw, device_context_id),
    MF(NetWireMWindowFaultRaw, session_id),
    MF(NetWireMWindowFaultRaw, domain_boot_id),
    MF(NetWireMWindowFaultRaw, query_id),
    MF(NetWireMWindowFaultRaw, original_can_boot_id),
    MF(NetWireMWindowFaultRaw, original_can_tx_sequence),
    MF(NetWireMWindowFaultRaw, fault_category),
    MF(NetWireMWindowFaultRaw, fault_code),
    MF(NetWireMWindowFaultRaw, fault_status),
    MF(NetWireMWindowFaultRaw, occurrence_id),
    MF(NetWireMWindowFaultRaw, update_sequence),
    MF(NetWireMWindowFaultRaw, source_age_ms),
};
static const Field m_vss_status_fields[] = {
    MF(NetWireMVssStatusRaw, device_context_id),
    MF(NetWireMVssStatusRaw, session_id),
    MF(NetWireMVssStatusRaw, domain_boot_id),
    MF(NetWireMVssStatusRaw, query_id),
    MF(NetWireMVssStatusRaw, original_can_boot_id),
    MF(NetWireMVssStatusRaw, original_can_tx_sequence),
    MF(NetWireMVssStatusRaw, vss_state),
    MF(NetWireMVssStatusRaw, vss_availability),
    MF(NetWireMVssStatusRaw, vss_accepting_events),
    MF(NetWireMVssStatusRaw, vss_fault_active),
    MF(NetWireMVssStatusRaw, fault_mask),
    MF(NetWireMVssStatusRaw, vss_last_fault),
    MF(NetWireMVssStatusRaw, last_fault_occurrence),
    MF(NetWireMVssStatusRaw, last_fault_age_ms),
    MF(NetWireMVssStatusRaw, last_fault_quality),
    MF(NetWireMVssStatusRaw, update_sequence),
    MF(NetWireMVssStatusRaw, source_age_ms),
};
static const Field m_query_end_fields[] = {
    MF(NetWireMQueryEndRaw, device_context_id),
    MF(NetWireMQueryEndRaw, session_id),
    MF(NetWireMQueryEndRaw, domain_boot_id),
    MF(NetWireMQueryEndRaw, query_id),
    MF(NetWireMQueryEndRaw, query_scope),
    MF(NetWireMQueryEndRaw, query_status),
    MF(NetWireMQueryEndRaw, item_count),
};

static NetWireStatus m_context_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_context_encode(PROFILE, s, &((const UartMRaw *)r)->context, f); }
static NetWireStatus m_context_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_context_decode(PROFILE, f, &((UartMRaw *)r)->context, s); }
static NetWireStatus m_domain_alive_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_domain_alive_encode(PROFILE, s, &((const UartMRaw *)r)->domain_alive, f); }
static NetWireStatus m_domain_alive_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_domain_alive_decode(PROFILE, f, &((UartMRaw *)r)->domain_alive, s); }
static NetWireStatus m_request_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_request_encode(PROFILE, s, &((const UartMRaw *)r)->request, f); }
static NetWireStatus m_request_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_request_decode(PROFILE, f, &((UartMRaw *)r)->request, s); }
static NetWireStatus m_query_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_query_encode(PROFILE, s, &((const UartMRaw *)r)->query, f); }
static NetWireStatus m_query_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_query_decode(PROFILE, f, &((UartMRaw *)r)->query, s); }
static NetWireStatus m_warning_ack_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_warning_ack_encode(PROFILE, s, &((const UartMRaw *)r)->warning_ack, f); }
static NetWireStatus m_warning_ack_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_warning_ack_decode(PROFILE, f, &((UartMRaw *)r)->warning_ack, s); }
static NetWireStatus m_result_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_result_encode(PROFILE, s, &((const UartMRaw *)r)->result, f); }
static NetWireStatus m_result_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_result_decode(PROFILE, f, &((UartMRaw *)r)->result, s); }
static NetWireStatus m_warning_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_warning_encode(PROFILE, s, &((const UartMRaw *)r)->warning, f); }
static NetWireStatus m_warning_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_warning_decode(PROFILE, f, &((UartMRaw *)r)->warning, s); }
static NetWireStatus m_availability_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_availability_encode(PROFILE, s, &((const UartMRaw *)r)->availability, f); }
static NetWireStatus m_availability_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_availability_decode(PROFILE, f, &((UartMRaw *)r)->availability, s); }
static NetWireStatus m_digital_status_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_digital_status_encode(PROFILE, s, &((const UartMRaw *)r)->digital_status, f); }
static NetWireStatus m_digital_status_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_digital_status_decode(PROFILE, f, &((UartMRaw *)r)->digital_status, s); }
static NetWireStatus m_digital_result_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_digital_result_encode(PROFILE, s, &((const UartMRaw *)r)->digital_result, f); }
static NetWireStatus m_digital_result_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_digital_result_decode(PROFILE, f, &((UartMRaw *)r)->digital_result, s); }
static NetWireStatus m_user_settings_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_user_settings_encode(PROFILE, s, &((const UartMRaw *)r)->user_settings, f); }
static NetWireStatus m_user_settings_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_user_settings_decode(PROFILE, f, &((UartMRaw *)r)->user_settings, s); }
static NetWireStatus m_bcm_door_state_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_bcm_door_state_encode(PROFILE, s, &((const UartMRaw *)r)->bcm_door_state, f); }
static NetWireStatus m_bcm_door_state_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_bcm_door_state_decode(PROFILE, f, &((UartMRaw *)r)->bcm_door_state, s); }
static NetWireStatus m_bcm_climate_state_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_bcm_climate_state_encode(PROFILE, s, &((const UartMRaw *)r)->bcm_climate_state, f); }
static NetWireStatus m_bcm_climate_state_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_bcm_climate_state_decode(PROFILE, f, &((UartMRaw *)r)->bcm_climate_state, s); }
static NetWireStatus m_bcm_light_state_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_bcm_light_state_encode(PROFILE, s, &((const UartMRaw *)r)->bcm_light_state, f); }
static NetWireStatus m_bcm_light_state_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_bcm_light_state_decode(PROFILE, f, &((UartMRaw *)r)->bcm_light_state, s); }
static NetWireStatus m_bcm_status_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_bcm_status_encode(PROFILE, s, &((const UartMRaw *)r)->bcm_status, f); }
static NetWireStatus m_bcm_status_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_bcm_status_decode(PROFILE, f, &((UartMRaw *)r)->bcm_status, s); }
static NetWireStatus m_cis_environment_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_cis_environment_encode(PROFILE, s, &((const UartMRaw *)r)->cis_environment, f); }
static NetWireStatus m_cis_environment_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_cis_environment_decode(PROFILE, f, &((UartMRaw *)r)->cis_environment, s); }
static NetWireStatus m_cis_occupant_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_cis_occupant_encode(PROFILE, s, &((const UartMRaw *)r)->cis_occupant, f); }
static NetWireStatus m_cis_occupant_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_cis_occupant_decode(PROFILE, f, &((UartMRaw *)r)->cis_occupant, s); }
static NetWireStatus m_cis_rear_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_cis_rear_encode(PROFILE, s, &((const UartMRaw *)r)->cis_rear, f); }
static NetWireStatus m_cis_rear_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_cis_rear_decode(PROFILE, f, &((UartMRaw *)r)->cis_rear, s); }
static NetWireStatus m_cis_status_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_cis_status_encode(PROFILE, s, &((const UartMRaw *)r)->cis_status, f); }
static NetWireStatus m_cis_status_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_cis_status_decode(PROFILE, f, &((UartMRaw *)r)->cis_status, s); }
static NetWireStatus m_window_state_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_window_state_encode(PROFILE, s, &((const UartMRaw *)r)->window_state, f); }
static NetWireStatus m_window_state_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_window_state_decode(PROFILE, f, &((UartMRaw *)r)->window_state, s); }
static NetWireStatus m_window_fault_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_window_fault_encode(PROFILE, s, &((const UartMRaw *)r)->window_fault, f); }
static NetWireStatus m_window_fault_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_window_fault_decode(PROFILE, f, &((UartMRaw *)r)->window_fault, s); }
static NetWireStatus m_vss_status_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_vss_status_encode(PROFILE, s, &((const UartMRaw *)r)->vss_status, f); }
static NetWireStatus m_vss_status_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_vss_status_decode(PROFILE, f, &((UartMRaw *)r)->vss_status, s); }
static NetWireStatus m_query_end_encode(uint16_t s, const void *r, NetWireUartFrame *f)
{ return net_wire_m_query_end_encode(PROFILE, s, &((const UartMRaw *)r)->query_end, f); }
static NetWireStatus m_query_end_decode(const NetWireUartFrame *f, void *r, uint16_t *s)
{ return net_wire_m_query_end_decode(PROFILE, f, &((UartMRaw *)r)->query_end, s); }

static const UartMMsg uartm_msgs[] = {
    { 0x01u, m_context_fields, COUNT(m_context_fields), m_context_encode, m_context_decode },
    { 0x02u, m_domain_alive_fields, COUNT(m_domain_alive_fields), m_domain_alive_encode, m_domain_alive_decode },
    { 0x10u, m_request_fields, COUNT(m_request_fields), m_request_encode, m_request_decode },
    { 0x11u, m_query_fields, COUNT(m_query_fields), m_query_encode, m_query_decode },
    { 0x12u, m_warning_ack_fields, COUNT(m_warning_ack_fields), m_warning_ack_encode, m_warning_ack_decode },
    { 0x20u, m_result_fields, COUNT(m_result_fields), m_result_encode, m_result_decode },
    { 0x21u, m_warning_fields, COUNT(m_warning_fields), m_warning_encode, m_warning_decode },
    { 0x22u, m_availability_fields, COUNT(m_availability_fields), m_availability_encode, m_availability_decode },
    { 0x23u, m_digital_status_fields, COUNT(m_digital_status_fields), m_digital_status_encode, m_digital_status_decode },
    { 0x24u, m_digital_result_fields, COUNT(m_digital_result_fields), m_digital_result_encode, m_digital_result_decode },
    { 0x25u, m_user_settings_fields, COUNT(m_user_settings_fields), m_user_settings_encode, m_user_settings_decode },
    { 0x30u, m_bcm_door_state_fields, COUNT(m_bcm_door_state_fields), m_bcm_door_state_encode, m_bcm_door_state_decode },
    { 0x31u, m_bcm_climate_state_fields, COUNT(m_bcm_climate_state_fields), m_bcm_climate_state_encode, m_bcm_climate_state_decode },
    { 0x32u, m_bcm_light_state_fields, COUNT(m_bcm_light_state_fields), m_bcm_light_state_encode, m_bcm_light_state_decode },
    { 0x33u, m_bcm_status_fields, COUNT(m_bcm_status_fields), m_bcm_status_encode, m_bcm_status_decode },
    { 0x34u, m_cis_environment_fields, COUNT(m_cis_environment_fields), m_cis_environment_encode, m_cis_environment_decode },
    { 0x35u, m_cis_occupant_fields, COUNT(m_cis_occupant_fields), m_cis_occupant_encode, m_cis_occupant_decode },
    { 0x36u, m_cis_rear_fields, COUNT(m_cis_rear_fields), m_cis_rear_encode, m_cis_rear_decode },
    { 0x37u, m_cis_status_fields, COUNT(m_cis_status_fields), m_cis_status_encode, m_cis_status_decode },
    { 0x38u, m_window_state_fields, COUNT(m_window_state_fields), m_window_state_encode, m_window_state_decode },
    { 0x39u, m_window_fault_fields, COUNT(m_window_fault_fields), m_window_fault_encode, m_window_fault_decode },
    { 0x3Au, m_vss_status_fields, COUNT(m_vss_status_fields), m_vss_status_encode, m_vss_status_decode },
    { 0x40u, m_query_end_fields, COUNT(m_query_end_fields), m_query_end_encode, m_query_end_decode },
};

static const UartMMsg *find_type(uint8_t type)
{
    size_t i;
    for (i = 0u; i < COUNT(uartm_msgs); ++i)
        if (uartm_msgs[i].type == type) return &uartm_msgs[i];
    return NULL;
}

/* NAME is the registry name (M_CONTEXT) or the type number (0x01). */
static const UartMMsg *find_name(const char *word)
{
    size_t i;
    char *end = NULL;
    unsigned long type = strtoul(word, &end, 0);
    if (end != word && *end == '\0' && type <= 0xFFu) return find_type((uint8_t)type);
    for (i = 0u; i < COUNT(uartm_msgs); ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_find(NET_WIRE_UART_M, uartm_msgs[i].type);
        if (r != NULL && strcmp(r->name, word) == 0) return &uartm_msgs[i];
    }
    return NULL;
}

static void usage(VssBenchText *t)
{
    say(t, "usage:\n"
           "  vss_bench uartm encode NAME|TYPE [--link N] [field=value ...]   (all fields start at 0)\n"
           "  vss_bench uartm decode <hex frame A5 5A ...>\n");
}

static int uartm_encode(VssBenchText *t, int argc, const char *const *argv)
{
    const UartMMsg *m;
    const NetWireRegistryEntry *reg;
    UartMRaw raw;
    NetWireUartFrame frame;
    NetWireStatus st;
    uint64_t link = 1u;
    int a = 1;
    size_t i;
    if (argc < 1 || (m = find_name(argv[0])) == NULL) {
        say(t, "error: unknown UART-M message '%s'\n", argc < 1 ? "" : argv[0]);
        usage(t);
        return VSS_BENCH_USAGE;
    }
    reg = net_wire_registry_find(NET_WIRE_UART_M, m->type);
    memset(&raw, 0, sizeof(raw));
    if (a < argc && strcmp(argv[a], "--link") == 0) {
        static const Field link_field = { "link", 0u, 2u, false };
        if (a + 1 >= argc || !parse_value(&link_field, argv[a + 1], &link)) {
            say(t, "error: --link needs 0..65535\n");
            return VSS_BENCH_USAGE;
        }
        a += 2;
    }
    for (; a < argc; ++a) {
        const char *eq = strchr(argv[a], '=');
        const Field *f = NULL;
        uint64_t v;
        if (eq == NULL || eq == argv[a]) {
            say(t, "error: expected field=value, got '%s'\n", argv[a]);
            return VSS_BENCH_USAGE;
        }
        for (i = 0u; i < m->field_count; ++i)
            if (strlen(m->fields[i].name) == (size_t)(eq - argv[a]) &&
                strncmp(m->fields[i].name, argv[a], (size_t)(eq - argv[a])) == 0) f = &m->fields[i];
        if (f == NULL) {
            say(t, "error: unknown field '%.*s' of %s\n", (int)(eq - argv[a]), argv[a], reg->name);
            return VSS_BENCH_USAGE;
        }
        if (!parse_value(f, eq + 1, &v)) {
            say(t, "error: value '%s' does not fit %s\n", eq + 1, f->name);
            return VSS_BENCH_USAGE;
        }
        field_put(&raw, f, v);
    }
    memset(&frame, 0, sizeof(frame));
    st = m->encode((uint16_t)link, &raw, &frame);
    if (st != NET_WIRE_OK) {   /* refused values: a usage error, like the generator */
        say(t, "codec: %s %s\n", status_name(st), reg->name);
        return VSS_BENCH_USAGE;
    }
    say(t, "codec: OK %s\n", reg->name);
    say_fields(t, &raw, m->fields, m->field_count);
    say(t, "frame:");
    for (i = 0u; i < frame.length; ++i) say(t, " %02X", frame.bytes[i]);
    say(t, "\n");
    return VSS_BENCH_OK;
}

static int uartm_decode(VssBenchText *t, int argc, const char *const *argv)
{
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    size_t n = 0u;
    NetWireUartFrame frame;
    const UartMMsg *m;
    const NetWireRegistryEntry *reg;
    UartMRaw raw;
    uint16_t seq = 0u, stored, computed = 0u;
    NetWireStatus st;
    if (!vss_bench_parse_hex(argc, argv, bytes, sizeof(bytes), &n) || n < 10u ||
        bytes[0] != 0xA5u || bytes[1] != 0x5Au) {
        say(t, "error: expected one UART-M frame in hex (A5 5A ...)\n");
        return VSS_BENCH_USAGE;
    }
    say(t, "# frame: %u B version=%u type=0x%02X len=%u link_seq=%u\n", (unsigned)n, (unsigned)bytes[2],
        (unsigned)bytes[3], (unsigned)(bytes[4] | (bytes[5] << 8)), (unsigned)(bytes[6] | (bytes[7] << 8)));
    stored = (uint16_t)(bytes[n - 2u] | (bytes[n - 1u] << 8));
    if (net_wire_crc16(bytes + 2, n - 4u, &computed) != NET_WIRE_OK) return VSS_BENCH_USAGE;
    if (stored != computed) {
        say(t, "crc: BAD stored=0x%04X computed=0x%04X\n", (unsigned)stored, (unsigned)computed);
        return VSS_BENCH_INVALID;
    }
    say(t, "crc: OK 0x%04X\n", (unsigned)computed);
    m = find_type(bytes[3]);
    reg = net_wire_registry_find(NET_WIRE_UART_M, bytes[3]);
    if (m == NULL || reg == NULL) {
        say(t, "codec: INVALID (type 0x%02X is not a UART-M message)\n", (unsigned)bytes[3]);
        return VSS_BENCH_INVALID;
    }
    memset(&frame, 0, sizeof(frame));
    memcpy(frame.bytes, bytes, n);
    frame.length = n;
    memset(&raw, 0, sizeof(raw));
    st = m->decode(&frame, &raw, &seq);
    if (st != NET_WIRE_OK) {
        say(t, "codec: %s %s\n", status_name(st), reg->name);
        return VSS_BENCH_INVALID;
    }
    say(t, "codec: OK %s link=%u\n", reg->name, (unsigned)seq);
    say_fields(t, &raw, m->fields, m->field_count);
    return VSS_BENCH_OK;
}

int vss_bench_uartm_run(int argc, const char *const *argv, VssBenchText *out)
{
    if (argc >= 1 && strcmp(argv[0], "encode") == 0) return uartm_encode(out, argc - 1, argv + 1);
    if (argc >= 1 && strcmp(argv[0], "decode") == 0) return uartm_decode(out, argc - 1, argv + 1);
    usage(out);
    return VSS_BENCH_USAGE;
}

