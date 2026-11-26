#include "network/net_wire_uart_m_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#define PROFILE NET_WIRE_CANDIDATE_ONLY

typedef union {
    NetWireMQueryRaw query;
    NetWireMWarningAckRaw warning_ack;
    NetWireMDomainAliveRaw domain_alive;
    NetWireMWarningRaw warning;
    NetWireMAvailabilityRaw availability;
    NetWireMDigitalStatusRaw digital_status;
    NetWireMDigitalResultRaw digital_result;
    NetWireMUserSettingsRaw user_settings;
    NetWireMQueryEndRaw query_end;
} Dto;

/* Independent Python whole-message struct.pack + binascii CRC frames.
 * Never obtained from the candidate encoder. Every payload field is compared
 * independently on decode; frame literal includes length, sequence and CRC. */
static const uint8_t golden_query[] = {
    0xa5U, 0x5aU, 0x02U, 0x11U, 0x20U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x80U, 0x70U, 0x60U, 0x50U,
    0x01U, 0x00U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
    0x88U, 0x99U, 0xefU, 0xcdU, 0xabU, 0x90U, 0x00U, 0x00U,
    0x9dU, 0x09U,
};
static const uint8_t golden_warning_ack[] = {
    0xa5U, 0x5aU, 0x02U, 0x12U, 0x15U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x08U, 0xefU, 0xcdU, 0xabU,
    0x90U, 0x40U, 0x30U, 0x20U, 0x10U, 0x85U, 0x0bU,
};
static const uint8_t golden_domain_alive[] = {
    0xa5U, 0x5aU, 0x02U, 0x02U, 0x0cU, 0x00U, 0xa5U, 0x5aU,
    0x40U, 0x30U, 0x20U, 0x10U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0xffU, 0xffU, 0x02U, 0x01U, 0x65U, 0x0fU,
};
static const uint8_t golden_warning[] = {
    0xa5U, 0x5aU, 0x02U, 0x21U, 0x28U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x08U, 0xefU, 0xcdU, 0xabU,
    0x90U, 0x02U, 0x01U, 0x00U, 0x01U, 0x05U, 0x87U, 0x65U,
    0x43U, 0x21U, 0x98U, 0xbaU, 0xdcU, 0xfeU, 0x34U, 0x12U,
    0xb3U, 0xcdU,
};
static const uint8_t golden_availability[] = {
    0xa5U, 0x5aU, 0x02U, 0x22U, 0x3eU, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x01U, 0x00U, 0x00U, 0x00U,
    0x02U, 0x01U, 0x11U, 0x00U, 0x03U, 0x02U, 0x0eU, 0x00U,
    0x04U, 0x00U, 0x00U, 0x00U, 0x05U, 0x01U, 0x11U, 0x00U,
    0x06U, 0x02U, 0x0eU, 0x00U, 0x07U, 0x00U, 0x00U, 0x00U,
    0x08U, 0x01U, 0x11U, 0x00U, 0x09U, 0x02U, 0x0eU, 0x00U,
    0xefU, 0xcdU, 0xabU, 0x90U, 0x34U, 0x12U, 0xe9U, 0xcdU,
};
static const uint8_t golden_digital_status[] = {
    0xa5U, 0x5aU, 0x02U, 0x23U, 0x1eU, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x00U, 0x01U, 0x11U, 0x00U,
    0xefU, 0xcdU, 0xabU, 0x90U, 0x34U, 0x12U, 0x88U, 0xa4U,
};
static const uint8_t golden_digital_result[] = {
    0xa5U, 0x5aU, 0x02U, 0x24U, 0x23U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0xefU, 0xcdU, 0xabU, 0x90U,
    0x87U, 0x65U, 0x43U, 0x21U, 0x02U, 0x02U, 0x09U, 0x00U,
    0x01U, 0x34U, 0x12U, 0xc6U, 0x9fU,
};
static const uint8_t golden_user_settings[] = {
    0xa5U, 0x5aU, 0x02U, 0x25U, 0x24U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x2eU, 0xfbU, 0x00U, 0x03U,
    0x01U, 0x64U, 0xa5U, 0x5aU, 0xffU, 0x00U, 0xefU, 0xcdU,
    0xabU, 0x90U, 0x34U, 0x12U, 0x41U, 0x54U,
};
static const uint8_t golden_query_end[] = {
    0xa5U, 0x5aU, 0x02U, 0x40U, 0x18U, 0x00U, 0xa5U, 0x5aU,
    0xa5U, 0x5aU, 0x34U, 0x12U, 0xefU, 0xcdU, 0xabU, 0x89U,
    0x67U, 0x45U, 0x23U, 0x01U, 0x40U, 0x30U, 0x20U, 0x10U,
    0x80U, 0x70U, 0x60U, 0x50U, 0x03U, 0x02U, 0x08U, 0x00U,
    0xf7U, 0x85U,
};
static const struct { const uint8_t *bytes; size_t length; } vectors[] = {
    {golden_query, sizeof(golden_query)},
    {golden_warning_ack, sizeof(golden_warning_ack)},
    {golden_domain_alive, sizeof(golden_domain_alive)},
    {golden_warning, sizeof(golden_warning)},
    {golden_availability, sizeof(golden_availability)},
    {golden_digital_status, sizeof(golden_digital_status)},
    {golden_digital_result, sizeof(golden_digital_result)},
    {golden_user_settings, sizeof(golden_user_settings)},
    {golden_query_end, sizeof(golden_query_end)},
};
static const char *const names[] = {"query", "warning_ack", "domain_alive", "warning", "availability", "digital_status", "digital_result", "user_settings", "query_end"};

static Dto fixture(size_t kind)
{
    Dto d;
    memset(&d, 0, sizeof(d));
    switch (kind) {
    case 0U:
        d.query.device_context_id = 0x12345aa5U;
        d.query.session_id = UINT64_C(0x123456789abcdef);
        d.query.query_id = 0x50607080U;
        d.query.query_scope = 0x1U;
        d.query.reserved = 0x0U;
        d.query.request_session = UINT64_C(0x9988776655443322);
        d.query.request_id = 0x90abcdefU;
        d.query.cursor = 0x0U;
        break;
    case 1U:
        d.warning_ack.device_context_id = 0x12345aa5U;
        d.warning_ack.session_id = UINT64_C(0x123456789abcdef);
        d.warning_ack.warning_type = 0x8U;
        d.warning_ack.occurrence_id = 0x90abcdefU;
        d.warning_ack.warning_domain_boot = 0x10203040U;
        break;
    case 2U:
        d.domain_alive.domain_boot_id = 0x10203040U;
        d.domain_alive.update_sequence = 0x90abcdefU;
        d.domain_alive.source_age_ms = 0xffffU;
        d.domain_alive.domain_state = 0x2U;
        d.domain_alive.value_quality = 0x1U;
        break;
    case 3U:
        d.warning.device_context_id = 0x12345aa5U;
        d.warning.session_id = UINT64_C(0x123456789abcdef);
        d.warning.domain_boot_id = 0x10203040U;
        d.warning.query_id = 0x50607080U;
        d.warning.warning_type = 0x8U;
        d.warning.occurrence_id = 0x90abcdefU;
        d.warning.severity = 0x2U;
        d.warning.warning_state = 0x1U;
        d.warning.read_state = 0x0U;
        d.warning.value_quality = 0x1U;
        d.warning.quality_reason = 0x5U;
        d.warning.source_boot = 0x21436587U;
        d.warning.source_sequence = 0xfedcba98U;
        d.warning.source_age_ms = 0x1234U;
        break;
    case 4U:
        d.availability.device_context_id = 0x12345aa5U;
        d.availability.session_id = UINT64_C(0x123456789abcdef);
        d.availability.domain_boot_id = 0x10203040U;
        d.availability.query_id = 0x50607080U;
        d.availability.items[0].function_id = 0x1U;
        d.availability.items[0].availability = 0x0U;
        d.availability.items[0].reason = 0x0U;
        d.availability.items[1].function_id = 0x2U;
        d.availability.items[1].availability = 0x1U;
        d.availability.items[1].reason = 0x11U;
        d.availability.items[2].function_id = 0x3U;
        d.availability.items[2].availability = 0x2U;
        d.availability.items[2].reason = 0xeU;
        d.availability.items[3].function_id = 0x4U;
        d.availability.items[3].availability = 0x0U;
        d.availability.items[3].reason = 0x0U;
        d.availability.items[4].function_id = 0x5U;
        d.availability.items[4].availability = 0x1U;
        d.availability.items[4].reason = 0x11U;
        d.availability.items[5].function_id = 0x6U;
        d.availability.items[5].availability = 0x2U;
        d.availability.items[5].reason = 0xeU;
        d.availability.items[6].function_id = 0x7U;
        d.availability.items[6].availability = 0x0U;
        d.availability.items[6].reason = 0x0U;
        d.availability.items[7].function_id = 0x8U;
        d.availability.items[7].availability = 0x1U;
        d.availability.items[7].reason = 0x11U;
        d.availability.items[8].function_id = 0x9U;
        d.availability.items[8].availability = 0x2U;
        d.availability.items[8].reason = 0xeU;
        d.availability.update_sequence = 0x90abcdefU;
        d.availability.source_age_ms = 0x1234U;
        break;
    case 5U:
        d.digital_status.device_context_id = 0x12345aa5U;
        d.digital_status.session_id = UINT64_C(0x123456789abcdef);
        d.digital_status.domain_boot_id = 0x10203040U;
        d.digital_status.query_id = 0x50607080U;
        d.digital_status.digital_key_setting_state = 0x0U;
        d.digital_status.digital_key_availability = 0x1U;
        d.digital_status.reason = 0x11U;
        d.digital_status.update_sequence = 0x90abcdefU;
        d.digital_status.source_age_ms = 0x1234U;
        break;
    case 6U:
        d.digital_result.device_context_id = 0x12345aa5U;
        d.digital_result.session_id = UINT64_C(0x123456789abcdef);
        d.digital_result.domain_boot_id = 0x10203040U;
        d.digital_result.query_id = 0x50607080U;
        d.digital_result.auto_occurrence_id = 0x90abcdefU;
        d.digital_result.related_door_command_id = 0x21436587U;
        d.digital_result.origin = 0x2U;
        d.digital_result.result = 0x2U;
        d.digital_result.result_reason = 0x9U;
        d.digital_result.result_confirmation = 0x1U;
        d.digital_result.result_age_ms = 0x1234U;
        break;
    case 7U:
        d.user_settings.device_context_id = 0x12345aa5U;
        d.user_settings.session_id = UINT64_C(0x123456789abcdef);
        d.user_settings.domain_boot_id = 0x10203040U;
        d.user_settings.query_id = 0x50607080U;
        d.user_settings.target_temperature_centi_c = -1234;
        d.user_settings.climate_auto = 0x0U;
        d.user_settings.user_fan_level = 0x3U;
        d.user_settings.interior_light_enabled = 0x1U;
        d.user_settings.normal_light_level_percent = 0x64U;
        d.user_settings.normal_light_rgb[0] = 0xa5U;
        d.user_settings.normal_light_rgb[1] = 0x5aU;
        d.user_settings.normal_light_rgb[2] = 0xffU;
        d.user_settings.value_quality = 0x0U;
        d.user_settings.update_sequence = 0x90abcdefU;
        d.user_settings.source_age_ms = 0x1234U;
        break;
    case 8U:
        d.query_end.device_context_id = 0x12345aa5U;
        d.query_end.session_id = UINT64_C(0x123456789abcdef);
        d.query_end.domain_boot_id = 0x10203040U;
        d.query_end.query_id = 0x50607080U;
        d.query_end.query_scope = 0x3U;
        d.query_end.query_status = 0x2U;
        d.query_end.item_count = 0x8U;
        break;
    default: break;
    }
    return d;
}

static int check_fields(size_t kind, const Dto *d)
{
    switch (kind) {
    case 0U:
        CHECK(d->query.device_context_id == 0x12345aa5U);
        CHECK(d->query.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->query.query_id == 0x50607080U);
        CHECK(d->query.query_scope == 0x1U);
        CHECK(d->query.reserved == 0x0U);
        CHECK(d->query.request_session == UINT64_C(0x9988776655443322));
        CHECK(d->query.request_id == 0x90abcdefU);
        CHECK(d->query.cursor == 0x0U);
        break;
    case 1U:
        CHECK(d->warning_ack.device_context_id == 0x12345aa5U);
        CHECK(d->warning_ack.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->warning_ack.warning_type == 0x8U);
        CHECK(d->warning_ack.occurrence_id == 0x90abcdefU);
        CHECK(d->warning_ack.warning_domain_boot == 0x10203040U);
        break;
    case 2U:
        CHECK(d->domain_alive.domain_boot_id == 0x10203040U);
        CHECK(d->domain_alive.update_sequence == 0x90abcdefU);
        CHECK(d->domain_alive.source_age_ms == 0xffffU);
        CHECK(d->domain_alive.domain_state == 0x2U);
        CHECK(d->domain_alive.value_quality == 0x1U);
        break;
    case 3U:
        CHECK(d->warning.device_context_id == 0x12345aa5U);
        CHECK(d->warning.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->warning.domain_boot_id == 0x10203040U);
        CHECK(d->warning.query_id == 0x50607080U);
        CHECK(d->warning.warning_type == 0x8U);
        CHECK(d->warning.occurrence_id == 0x90abcdefU);
        CHECK(d->warning.severity == 0x2U);
        CHECK(d->warning.warning_state == 0x1U);
        CHECK(d->warning.read_state == 0x0U);
        CHECK(d->warning.value_quality == 0x1U);
        CHECK(d->warning.quality_reason == 0x5U);
        CHECK(d->warning.source_boot == 0x21436587U);
        CHECK(d->warning.source_sequence == 0xfedcba98U);
        CHECK(d->warning.source_age_ms == 0x1234U);
        break;
    case 4U:
        CHECK(d->availability.device_context_id == 0x12345aa5U);
        CHECK(d->availability.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->availability.domain_boot_id == 0x10203040U);
        CHECK(d->availability.query_id == 0x50607080U);
        CHECK(d->availability.items[0].function_id == 0x1U);
        CHECK(d->availability.items[0].availability == 0x0U);
        CHECK(d->availability.items[0].reason == 0x0U);
        CHECK(d->availability.items[1].function_id == 0x2U);
        CHECK(d->availability.items[1].availability == 0x1U);
        CHECK(d->availability.items[1].reason == 0x11U);
        CHECK(d->availability.items[2].function_id == 0x3U);
        CHECK(d->availability.items[2].availability == 0x2U);
        CHECK(d->availability.items[2].reason == 0xeU);
        CHECK(d->availability.items[3].function_id == 0x4U);
        CHECK(d->availability.items[3].availability == 0x0U);
        CHECK(d->availability.items[3].reason == 0x0U);
        CHECK(d->availability.items[4].function_id == 0x5U);
        CHECK(d->availability.items[4].availability == 0x1U);
        CHECK(d->availability.items[4].reason == 0x11U);
        CHECK(d->availability.items[5].function_id == 0x6U);
        CHECK(d->availability.items[5].availability == 0x2U);
        CHECK(d->availability.items[5].reason == 0xeU);
        CHECK(d->availability.items[6].function_id == 0x7U);
        CHECK(d->availability.items[6].availability == 0x0U);
        CHECK(d->availability.items[6].reason == 0x0U);
        CHECK(d->availability.items[7].function_id == 0x8U);
        CHECK(d->availability.items[7].availability == 0x1U);
        CHECK(d->availability.items[7].reason == 0x11U);
        CHECK(d->availability.items[8].function_id == 0x9U);
        CHECK(d->availability.items[8].availability == 0x2U);
        CHECK(d->availability.items[8].reason == 0xeU);
        CHECK(d->availability.update_sequence == 0x90abcdefU);
        CHECK(d->availability.source_age_ms == 0x1234U);
        break;
    case 5U:
        CHECK(d->digital_status.device_context_id == 0x12345aa5U);
        CHECK(d->digital_status.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->digital_status.domain_boot_id == 0x10203040U);
        CHECK(d->digital_status.query_id == 0x50607080U);
        CHECK(d->digital_status.digital_key_setting_state == 0x0U);
        CHECK(d->digital_status.digital_key_availability == 0x1U);
        CHECK(d->digital_status.reason == 0x11U);
        CHECK(d->digital_status.update_sequence == 0x90abcdefU);
        CHECK(d->digital_status.source_age_ms == 0x1234U);
        break;
    case 6U:
        CHECK(d->digital_result.device_context_id == 0x12345aa5U);
        CHECK(d->digital_result.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->digital_result.domain_boot_id == 0x10203040U);
        CHECK(d->digital_result.query_id == 0x50607080U);
        CHECK(d->digital_result.auto_occurrence_id == 0x90abcdefU);
        CHECK(d->digital_result.related_door_command_id == 0x21436587U);
        CHECK(d->digital_result.origin == 0x2U);
        CHECK(d->digital_result.result == 0x2U);
        CHECK(d->digital_result.result_reason == 0x9U);
        CHECK(d->digital_result.result_confirmation == 0x1U);
        CHECK(d->digital_result.result_age_ms == 0x1234U);
        break;
    case 7U:
        CHECK(d->user_settings.device_context_id == 0x12345aa5U);
        CHECK(d->user_settings.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->user_settings.domain_boot_id == 0x10203040U);
        CHECK(d->user_settings.query_id == 0x50607080U);
        CHECK(d->user_settings.target_temperature_centi_c == -1234);
        CHECK(d->user_settings.climate_auto == 0x0U);
        CHECK(d->user_settings.user_fan_level == 0x3U);
        CHECK(d->user_settings.interior_light_enabled == 0x1U);
        CHECK(d->user_settings.normal_light_level_percent == 0x64U);
        CHECK(d->user_settings.normal_light_rgb[0] == 0xa5U);
        CHECK(d->user_settings.normal_light_rgb[1] == 0x5aU);
        CHECK(d->user_settings.normal_light_rgb[2] == 0xffU);
        CHECK(d->user_settings.value_quality == 0x0U);
        CHECK(d->user_settings.update_sequence == 0x90abcdefU);
        CHECK(d->user_settings.source_age_ms == 0x1234U);
        break;
    case 8U:
        CHECK(d->query_end.device_context_id == 0x12345aa5U);
        CHECK(d->query_end.session_id == UINT64_C(0x123456789abcdef));
        CHECK(d->query_end.domain_boot_id == 0x10203040U);
        CHECK(d->query_end.query_id == 0x50607080U);
        CHECK(d->query_end.query_scope == 0x3U);
        CHECK(d->query_end.query_status == 0x2U);
        CHECK(d->query_end.item_count == 0x8U);
        break;
    default: return 1;
    }
    return 0;
}

static NetWireStatus encode(size_t kind, NetWireProfile profile, uint16_t sequence, const Dto *d, NetWireUartFrame *out)
{
    switch (kind) {
    case 0U: return net_wire_m_query_encode(profile, sequence, d ? &d->query : NULL, out);
    case 1U: return net_wire_m_warning_ack_encode(profile, sequence, d ? &d->warning_ack : NULL, out);
    case 2U: return net_wire_m_domain_alive_encode(profile, sequence, d ? &d->domain_alive : NULL, out);
    case 3U: return net_wire_m_warning_encode(profile, sequence, d ? &d->warning : NULL, out);
    case 4U: return net_wire_m_availability_encode(profile, sequence, d ? &d->availability : NULL, out);
    case 5U: return net_wire_m_digital_status_encode(profile, sequence, d ? &d->digital_status : NULL, out);
    case 6U: return net_wire_m_digital_result_encode(profile, sequence, d ? &d->digital_result : NULL, out);
    case 7U: return net_wire_m_user_settings_encode(profile, sequence, d ? &d->user_settings : NULL, out);
    case 8U: return net_wire_m_query_end_encode(profile, sequence, d ? &d->query_end : NULL, out);
    default: return NET_WIRE_INVALID_ARGUMENT;
    }
}

static NetWireStatus decode(size_t kind, NetWireProfile profile, const NetWireUartFrame *frame, Dto *out, uint16_t *sequence)
{
    switch (kind) {
    case 0U: return net_wire_m_query_decode(profile, frame, out ? &out->query : NULL, sequence);
    case 1U: return net_wire_m_warning_ack_decode(profile, frame, out ? &out->warning_ack : NULL, sequence);
    case 2U: return net_wire_m_domain_alive_decode(profile, frame, out ? &out->domain_alive : NULL, sequence);
    case 3U: return net_wire_m_warning_decode(profile, frame, out ? &out->warning : NULL, sequence);
    case 4U: return net_wire_m_availability_decode(profile, frame, out ? &out->availability : NULL, sequence);
    case 5U: return net_wire_m_digital_status_decode(profile, frame, out ? &out->digital_status : NULL, sequence);
    case 6U: return net_wire_m_digital_result_decode(profile, frame, out ? &out->digital_result : NULL, sequence);
    case 7U: return net_wire_m_user_settings_decode(profile, frame, out ? &out->user_settings : NULL, sequence);
    case 8U: return net_wire_m_query_end_decode(profile, frame, out ? &out->query_end : NULL, sequence);
    default: return NET_WIRE_INVALID_ARGUMENT;
    }
}

/* Byte algebra reference differs from production bitwise CRC. */
static uint16_t reference_crc(const uint8_t *p, size_t count)
{
    uint32_t crc = 0xffffU;
    size_t i;
    for (i = 0U; i < count; ++i) {
        uint32_t x = (crc >> 8U) ^ p[i];
        x ^= x >> 4U;
        crc = ((crc << 8U) ^ (x << 12U) ^ (x << 5U) ^ x) & 0xffffU;
    }
    return (uint16_t)crc;
}
static void fix_crc(NetWireUartFrame *f)
{
    uint16_t crc = reference_crc(f->bytes + 2, f->length - 4U);
    f->bytes[f->length - 2U] = (uint8_t)crc;
    f->bytes[f->length - 1U] = (uint8_t)(crc >> 8U);
}
static NetWireUartFrame fixture_frame(size_t kind)
{
    NetWireUartFrame f = {0};
    f.length = vectors[kind].length;
    f.captured_at_ms = UINT64_C(9876543210);
    memcpy(f.bytes, vectors[kind].bytes, f.length);
    return f;
}
static int decode_failure(size_t kind, NetWireProfile profile,
    const NetWireUartFrame *frame, NetWireStatus expected)
{
    Dto out, before;
    uint16_t sequence = 0x8badU;
    memset(&out, 0x6d, sizeof(out)); before = out;
    CHECK(decode(kind, profile, frame, &out, &sequence) == expected);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    CHECK(sequence == 0x8badU);
    return 0;
}
static int encode_failure(size_t kind, NetWireProfile profile,
    const Dto *input, NetWireStatus expected)
{
    NetWireUartFrame out, before;
    memset(&out, 0x7c, sizeof(out)); before = out;
    CHECK(encode(kind, profile, 0x5aa5U, input, &out) == expected);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    return 0;
}

/* Field changes are applied independently to caller DTO members and the
 * frozen frame. No candidate encoder is used to create decoder error inputs. */
typedef struct {
    size_t kind, wire_offset, dto_offset, width;
    uint64_t value;
    NetWireStatus expected;
} Mutation;
#define MUT(k, dto, field, off, w, val, result) \
    { k, off, offsetof(Dto, dto.field), w, val, result }
static const Mutation mutations[] = {
    MUT(0U, query, query_id, 12U, 4U, 0U, NET_WIRE_MALFORMED),
    MUT(0U, query, query_scope, 16U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(0U, query, reserved, 17U, 1U, 1U, NET_WIRE_MALFORMED),
    MUT(0U, query, cursor, 30U, 2U, 1U, NET_WIRE_MALFORMED),
    MUT(0U, query, query_scope, 16U, 1U, 0U, NET_WIRE_MALFORMED),
    MUT(0U, query, query_scope, 16U, 1U, 2U, NET_WIRE_CONTRACT_HOLD),
    MUT(0U, query, request_id, 26U, 4U, 0U, NET_WIRE_CONTRACT_HOLD),
    MUT(1U, warning_ack, warning_type, 12U, 1U, 0U, NET_WIRE_MALFORMED),
    MUT(1U, warning_ack, warning_type, 12U, 1U, 9U, NET_WIRE_MALFORMED),
    MUT(1U, warning_ack, warning_domain_boot, 17U, 4U, 0U, NET_WIRE_CONTRACT_HOLD),
    MUT(1U, warning_ack, device_context_id, 0U, 4U, 0U, NET_WIRE_MALFORMED),
    MUT(2U, domain_alive, domain_state, 10U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(2U, domain_alive, value_quality, 11U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(2U, domain_alive, domain_boot_id, 0U, 4U, 0U, NET_WIRE_MALFORMED),
    MUT(3U, warning, warning_type, 20U, 1U, 0U, NET_WIRE_MALFORMED),
    MUT(3U, warning, severity, 25U, 1U, 3U, NET_WIRE_MALFORMED),
    MUT(3U, warning, warning_state, 26U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(3U, warning, read_state, 27U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(3U, warning, value_quality, 28U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(3U, warning, quality_reason, 29U, 1U, 10U, NET_WIRE_MALFORMED),
    MUT(3U, warning, source_boot, 30U, 4U, 0U, NET_WIRE_CONTRACT_HOLD),
    MUT(3U, warning, value_quality, 28U, 1U, 0U, NET_WIRE_CONTRACT_HOLD),
    MUT(4U, availability, items[0].function_id, 20U, 1U, 0U, NET_WIRE_MALFORMED),
    MUT(4U, availability, items[1].function_id, 24U, 1U, 1U, NET_WIRE_MALFORMED),
    MUT(4U, availability, items[8].function_id, 52U, 1U, 8U, NET_WIRE_MALFORMED),
    MUT(4U, availability, items[4].availability, 37U, 1U, 3U, NET_WIRE_MALFORMED),
    MUT(4U, availability, items[8].reason, 54U, 2U, 26U, NET_WIRE_MALFORMED),
    MUT(4U, availability, items[8].reason, 54U, 2U, 65535U, NET_WIRE_MALFORMED),
    MUT(5U, digital_status, digital_key_setting_state, 20U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(5U, digital_status, digital_key_availability, 21U, 1U, 3U, NET_WIRE_MALFORMED),
    MUT(5U, digital_status, reason, 22U, 2U, 26U, NET_WIRE_MALFORMED),
    MUT(6U, digital_result, origin, 28U, 1U, 1U, NET_WIRE_MALFORMED),
    MUT(6U, digital_result, result, 29U, 1U, 6U, NET_WIRE_MALFORMED),
    MUT(6U, digital_result, result_reason, 30U, 2U, 26U, NET_WIRE_MALFORMED),
    MUT(6U, digital_result, result_confirmation, 32U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(6U, digital_result, result, 29U, 1U, 255U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, user_fan_level, 23U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, normal_light_level_percent, 25U, 1U, 101U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, climate_auto, 22U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, interior_light_enabled, 24U, 1U, 2U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, value_quality, 29U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(7U, user_settings, climate_auto, 22U, 1U, 255U, NET_WIRE_CONTRACT_HOLD),
    MUT(7U, user_settings, user_fan_level, 23U, 1U, 255U, NET_WIRE_CONTRACT_HOLD),
    MUT(8U, query_end, query_id, 16U, 4U, 0U, NET_WIRE_MALFORMED),
    MUT(8U, query_end, query_scope, 20U, 1U, 4U, NET_WIRE_MALFORMED),
    MUT(8U, query_end, query_status, 21U, 1U, 4U, NET_WIRE_MALFORMED),
};
#undef MUT
static void native_write(void *p, size_t width, uint64_t value)
{
    if (width == 1U) { uint8_t v = (uint8_t)value; memcpy(p, &v, sizeof(v)); }
    else if (width == 2U) { uint16_t v = (uint16_t)value; memcpy(p, &v, sizeof(v)); }
    else if (width == 4U) { uint32_t v = (uint32_t)value; memcpy(p, &v, sizeof(v)); }
    else { uint64_t v = value; memcpy(p, &v, sizeof(v)); }
}
static int field_errors(size_t kind)
{
    size_t i;
    for (i = 0U; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        const Mutation *m = &mutations[i];
        Dto d;
        NetWireUartFrame f;
        size_t j;
        if (m->kind != kind) continue;
        d = fixture(kind); f = fixture_frame(kind);
        native_write((uint8_t *)&d + m->dto_offset, m->width, m->value);
        for (j = 0U; j < m->width; ++j)
            f.bytes[8U + m->wire_offset + j] = (uint8_t)(m->value >> (j * 8U));
        fix_crc(&f);
        CHECK(encode_failure(kind, PROFILE, &d, m->expected) == 0);
        CHECK(decode_failure(kind, PROFILE, &f, m->expected) == 0);
    }
    return 0;
}

static int message_case(size_t kind)
{
    Dto d = fixture(kind), out;
    NetWireUartFrame f = fixture_frame(kind), encoded, changed;
    NetWireUartPayload payload;
    NetWireDirection wrong = kind < 2U ? NET_WIRE_DOWNLINK : NET_WIRE_UPLINK;
    uint16_t sequence = 0U;
    size_t i;
    CHECK(reference_crc((const uint8_t *)"123456789", 9U) == 0x29b1U);
    CHECK(encode(kind, PROFILE, 0x5aa5U, &d, &encoded) == NET_WIRE_OK);
    CHECK(encoded.length == f.length && memcmp(encoded.bytes, f.bytes, f.length) == 0);
    memset(&out, 0xc7, sizeof(out));
    CHECK(decode(kind, PROFILE, &f, &out, &sequence) == NET_WIRE_OK);
    CHECK(sequence == 0x5aa5U && check_fields(kind, &out) == 0);
    CHECK(encode_failure(kind, NET_WIRE_NOT_BOUND, &d, NET_WIRE_NOT_BOUND_STATUS) == 0);
    CHECK(decode_failure(kind, NET_WIRE_NOT_BOUND, &f, NET_WIRE_NOT_BOUND_STATUS) == 0);
    CHECK(encode_failure(kind, (NetWireProfile)99, &d, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(decode_failure(kind, (NetWireProfile)99, &f, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(encode_failure(kind, PROFILE, NULL, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(decode_failure(kind, PROFILE, NULL, NET_WIRE_INVALID_ARGUMENT) == 0);
    CHECK(encode(kind, PROFILE, 1U, &d, NULL) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(decode(kind, PROFILE, &f, NULL, &sequence) == NET_WIRE_INVALID_ARGUMENT);
    CHECK(decode(kind, PROFILE, &f, &out, NULL) == NET_WIRE_INVALID_ARGUMENT);
    memset(&payload, 0x6d, sizeof(payload));
    { NetWireUartPayload before = payload;
      CHECK(net_wire_uart_decode(PROFILE, NET_WIRE_UART_M, wrong, &f, &payload) != NET_WIRE_OK);
      CHECK(memcmp(&payload, &before, sizeof(payload)) == 0); }
    /* Every truncated frame, oversized length and extra trailing byte. */
    for (i = 0U; i < f.length; ++i) {
        changed = f; changed.length = i;
        CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    }
    changed = f; changed.length = NET_WIRE_UART_FRAME_MAX + 1U;
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    changed = f; ++changed.length;
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    /* Header validation with independently correct CRC for the changed body. */
    changed = f; changed.bytes[2] = 1U; fix_crc(&changed);
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    changed = f; changed.bytes[3] = 0xffU; fix_crc(&changed);
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_UNSUPPORTED) == 0);
    changed = f; changed.bytes[3] = kind < 2U ? 2U : 0x11U; fix_crc(&changed);
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    changed = f; changed.bytes[4] ^= 1U; fix_crc(&changed);
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    changed = f; changed.bytes[5] = 1U; fix_crc(&changed);
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    changed = f; changed.bytes[0] ^= 1U;
    CHECK(decode_failure(kind, PROFILE, &changed, NET_WIRE_MALFORMED) == 0);
    /* Every protected byte and both CRC bytes detect one-bit corruption. */
    for (i = 2U; i < f.length; ++i) {
        changed = f; changed.bytes[i] ^= 1U;
        CHECK(decode_failure(kind, PROFILE, &changed,
            i == 3U && net_wire_registry_find(NET_WIRE_UART_M, changed.bytes[3]) == NULL ?
            NET_WIRE_UNSUPPORTED : NET_WIRE_MALFORMED) == 0);
    }
    CHECK(field_errors(kind) == 0);
    return 0;
}

static int alias_case(void)
{
    union { Dto dto; NetWireUartFrame frame; uint64_t align; uint8_t bytes[512]; } storage;
    uint8_t before[sizeof(storage)];
    size_t k;
    for (k = 0U; k < 9U; ++k) {
        Dto d = fixture(k), out;
        NetWireUartFrame f = fixture_frame(k);
        uint16_t sequence = 0x8badU;
        memset(&storage, 0x7c, sizeof(storage));
        memcpy(before, &storage, sizeof(storage));
        CHECK(encode(k, PROFILE, 1U, &storage.dto, &storage.frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, &storage, sizeof(storage)) == 0);
        CHECK(encode(k, PROFILE, 1U, (const Dto *)(const void *)(storage.bytes + 8), &storage.frame) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, &storage, sizeof(storage)) == 0);
        CHECK(encode(k, PROFILE, 1U, &storage.dto, (NetWireUartFrame *)(void *)(storage.bytes + 8)) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, &storage, sizeof(storage)) == 0);
        CHECK(decode(k, PROFILE, &storage.frame, &storage.dto, &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(decode(k, PROFILE, &storage.frame, (Dto *)(void *)(storage.bytes + 8), &sequence) == NET_WIRE_INVALID_ARGUMENT);
        CHECK(memcmp(before, &storage, sizeof(storage)) == 0 && sequence == 0x8badU);
        memset(&out, 0x6d, sizeof(out));
        { Dto saved = out; NetWireUartFrame saved_frame = f;
          CHECK(decode(k, PROFILE, &f, &out, (uint16_t *)(void *)f.bytes) == NET_WIRE_INVALID_ARGUMENT);
          CHECK(memcmp(&out, &saved, sizeof(out)) == 0 && memcmp(&f, &saved_frame, sizeof(f)) == 0);
          CHECK(decode(k, PROFILE, &f, &out, (uint16_t *)(void *)&out) == NET_WIRE_INVALID_ARGUMENT);
          CHECK(decode(k, PROFILE, &f, &out, (uint16_t *)(void *)((uint8_t *)&out + 2)) == NET_WIRE_INVALID_ARGUMENT);
          CHECK(memcmp(&out, &saved, sizeof(out)) == 0); }
        /* Successful encode does not modify the caller's DTO. */
        { Dto saved = d; CHECK(encode(k, PROFILE, 0U, &d, &f) == NET_WIRE_OK);
          CHECK(memcmp(&d, &saved, sizeof(d)) == 0); }
    }
    return 0;
}

/* A changed valid boundary is checked against an independently edited frozen
 * frame, on encode and decode. No round-trip supplies the expected bytes. */
static int boundary(size_t kind, const Dto *d, const NetWireUartFrame *expected, uint16_t seq)
{
    NetWireUartFrame actual;
    Dto decoded;
    uint16_t got = 0U;
    CHECK(encode(kind, PROFILE, seq, d, &actual) == NET_WIRE_OK);
    CHECK(actual.length == expected->length && memcmp(actual.bytes, expected->bytes, actual.length) == 0);
    CHECK(decode(kind, PROFILE, expected, &decoded, &got) == NET_WIRE_OK);
    CHECK(got == seq);
    CHECK(encode(kind, PROFILE, got, &decoded, &actual) == NET_WIRE_OK);
    CHECK(memcmp(actual.bytes, expected->bytes, actual.length) == 0);
    return 0;
}
static int semantic_case(void)
{
    Dto d;
    NetWireUartFrame f;
    unsigned code;
    size_t k;
    /* Link sequence 0/FFFF neither allocates nor refreshes payload identity/AGE. */
    for (k = 0U; k < 9U; ++k) {
        d = fixture(k); f = fixture_frame(k);
        f.bytes[6] = 0U; f.bytes[7] = 0U; fix_crc(&f);
        CHECK(boundary(k, &d, &f, 0U) == 0);
        f.bytes[6] = 0xffU; f.bytes[7] = 0xffU; fix_crc(&f);
        CHECK(boundary(k, &d, &f, UINT16_MAX) == 0);
    }
    /* Zero/current Session is not independently forbidden by this raw codec. */
    d = fixture(0U); f = fixture_frame(0U);
    d.query.session_id = 0U; memset(f.bytes + 12, 0, 8U);
    d.query.request_session = 0U; d.query.request_id = 0U;
    memset(f.bytes + 26, 0, 12U);
    for (code = 0U; code <= 3U; ++code) {
        d.query.query_scope = (uint8_t)code; f.bytes[24] = (uint8_t)code; fix_crc(&f);
        CHECK(boundary(0U, &d, &f, 0x5aa5U) == 0);
    }
    /* All historical result codes remain distinct from absence255 and zero. */
    for (code = 0U; code <= 5U; ++code) {
        d = fixture(6U); f = fixture_frame(6U);
        d.digital_result.result = (uint8_t)code; f.bytes[37] = (uint8_t)code; fix_crc(&f);
        CHECK(boundary(6U, &d, &f, 0x5aa5U) == 0);
    }
    d = fixture(6U); f = fixture_frame(6U);
    d.digital_result.result = 255U; d.digital_result.result_confirmation = 0U;
    f.bytes[37] = 255U; f.bytes[40] = 0U; fix_crc(&f);
    CHECK(boundary(6U, &d, &f, 0x5aa5U) == 0);
    /* QueryEnd does not count transmissions or certify query completion. */
    for (code = 0U; code < 4U; ++code) {
        d = fixture(8U); f = fixture_frame(8U);
        d.query_end.query_status = (uint8_t)code; d.query_end.item_count = 0U;
        f.bytes[29] = (uint8_t)code; f.bytes[30] = 0U; f.bytes[31] = 0U; fix_crc(&f);
        CHECK(boundary(8U, &d, &f, 0x5aa5U) == 0);
        d.query_end.item_count = UINT16_MAX; f.bytes[30] = 255U; f.bytes[31] = 255U; fix_crc(&f);
        CHECK(boundary(8U, &d, &f, 0x5aa5U) == 0);
    }
    /* All 9 availability IDs and defined reason0..25 keep their raw order. */
    for (code = 0U; code <= 25U; ++code) {
        d = fixture(4U); f = fixture_frame(4U);
        for (k = 0U; k < 9U; ++k) {
            d.availability.items[k].reason = (uint16_t)code;
            f.bytes[30U + k * 4U] = (uint8_t)code; f.bytes[31U + k * 4U] = 0U;
        }
        fix_crc(&f); CHECK(boundary(4U, &d, &f, 0x5aa5U) == 0);
    }
    return 0;
}
static int settings_case(void)
{
    static const int16_t temperatures[] = {INT16_MIN, -32767, -1234, -1, 0, 1, INT16_MAX};
    size_t i;
    for (i = 0U; i < sizeof(temperatures) / sizeof(temperatures[0]); ++i) {
        Dto d = fixture(7U), decoded;
        NetWireUartFrame f = fixture_frame(7U);
        uint16_t bits = (uint16_t)temperatures[i], seq = 0U;
        d.user_settings.target_temperature_centi_c = temperatures[i];
        d.user_settings.value_quality = 3U;
        f.bytes[28] = (uint8_t)bits; f.bytes[29] = (uint8_t)(bits >> 8U); f.bytes[37] = 3U;
        fix_crc(&f); CHECK(boundary(7U, &d, &f, 0x5aa5U) == 0);
        CHECK(decode(7U, PROFILE, &f, &decoded, &seq) == NET_WIRE_OK);
        CHECK(decoded.user_settings.target_temperature_centi_c == temperatures[i]);
        if (temperatures[i] == INT16_MIN) {
            d.user_settings.value_quality = 0U; f.bytes[37] = 0U; fix_crc(&f);
            CHECK(encode_failure(7U, PROFILE, &d, NET_WIRE_CONTRACT_HOLD) == 0);
            CHECK(decode_failure(7U, PROFILE, &f, NET_WIRE_CONTRACT_HOLD) == 0);
        }
    }
    for (i = 0U; i < 2U; ++i) {
        Dto d = fixture(7U), decoded;
        NetWireUartFrame f = fixture_frame(7U);
        uint16_t seq;
        uint8_t value = i == 0U ? 0U : 255U;
        d.user_settings.climate_auto = value; d.user_settings.user_fan_level = value;
        d.user_settings.interior_light_enabled = value; d.user_settings.value_quality = 3U;
        d.user_settings.normal_light_level_percent = i == 0U ? 0U : 100U;
        f.bytes[30] = value; f.bytes[31] = value; f.bytes[32] = value;
        f.bytes[33] = d.user_settings.normal_light_level_percent; f.bytes[37] = 3U;
        fix_crc(&f); CHECK(boundary(7U, &d, &f, 0x5aa5U) == 0);
        CHECK(decode(7U, PROFILE, &f, &decoded, &seq) == NET_WIRE_OK);
        CHECK(decoded.user_settings.climate_auto == value && decoded.user_settings.user_fan_level == value);
        CHECK(decoded.user_settings.interior_light_enabled == value);
        CHECK(decoded.user_settings.normal_light_rgb[0] == 0xa5U &&
              decoded.user_settings.normal_light_rgb[1] == 0x5aU &&
              decoded.user_settings.normal_light_rgb[2] == 0xffU);
    }
    return 0;
}
int main(int argc, char **argv)
{
    size_t i;
    if (argc != 2) return 2;
    for (i = 0U; i < 9U; ++i) if (strcmp(argv[1], names[i]) == 0) return message_case(i);
    if (strcmp(argv[1], "alias") == 0) return alias_case();
    if (strcmp(argv[1], "semantics") == 0) return semantic_case();
    if (strcmp(argv[1], "settings") == 0) return settings_case();
    return 2;
}
