#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "uart_m_codec.h"
#include "uart_m_frame.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

#define APP_BYTES \
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
#define DOWN_BYTES \
    APP_BYTES, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14
#define CAN_BYTES \
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C

static const uint8_t golden_context[] = {
    APP_BYTES, 0xFF, 0x01, 0x00, 0x01, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0xFF, 0x01, 0x08, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x02,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25};
static const uint8_t golden_domain_alive[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x03, 0x02};
static const uint8_t golden_request[] = {
    APP_BYTES, 0x0D, 0x0E, 0x0F, 0x10, 0x02, 0x01, 0xD4, 0xFE, 0x00, 0x00, 0x00, 0x00};
static const uint8_t golden_query[] = {
    APP_BYTES, 0x0D, 0x0E, 0x0F, 0x10, 0x03, 0x00,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x00, 0x00};
static const uint8_t golden_warning_ack[] = {
    APP_BYTES, 0x08, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
static const uint8_t golden_result[] = {
    DOWN_BYTES, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x20, 0x02, 0x0D, 0x00, 0x01, 0x01,
    0x21, 0x22, 0x23, 0x24, 0x02, 0x25, 0x26};
static const uint8_t golden_warning[] = {
    DOWN_BYTES, 0x08, 0x15, 0x16, 0x17, 0x18, 0x02, 0x01, 0x01, 0x01, 0x08,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};
static const uint8_t golden_availability[] = {
    DOWN_BYTES,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x01, 0x01, 0x00, 0x03, 0x02, 0x02, 0x00,
    0x04, 0x00, 0x03, 0x00, 0x05, 0x01, 0x04, 0x00, 0x06, 0x02, 0x05, 0x00,
    0x07, 0x00, 0x06, 0x00, 0x08, 0x01, 0x07, 0x00, 0x09, 0x02, 0x08, 0x00,
    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E};
static const uint8_t golden_digital_status[] = {
    DOWN_BYTES, 0x01, 0x02, 0x19, 0x00, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22};
static const uint8_t golden_digital_result[] = {
    DOWN_BYTES, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x02, 0x05, 0x09, 0x00, 0x01, 0x22, 0x23};
static const uint8_t golden_user_settings[] = {
    DOWN_BYTES, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0x64, 0x11, 0x22, 0x33, 0x03,
    0x1F, 0x20, 0x21, 0x22, 0x23, 0x24};
static const uint8_t golden_query_end[] = {DOWN_BYTES, 0x03, 0x02, 0x15, 0x16};
static const uint8_t golden_bcm_door[] = {
    DOWN_BYTES, CAN_BYTES, 0xFF, 0x01, 0x02, 0x00, 0x01, 0x02,
    0x23, 0x24, 0x25, 0x26, 0x27, 0x28};
static const uint8_t golden_bcm_climate[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0xFF, 0x02, 0x64, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x00,
    0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B};
static const uint8_t golden_bcm_light[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0x64, 0x11, 0x22, 0x33, 0xFF, 0x01,
    0x00, 0x01, 0x02, 0x03, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C};
static const uint8_t golden_bcm_status[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0xA5, 0x00, 0x03, 0x08, 0x00, 0x07, 0x01,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
static const uint8_t golden_cis_environment[] = {
    DOWN_BYTES, CAN_BYTES,
    0x60, 0xF0, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x01, 0x00,
    0x10, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x01, 0x08,
    0x40, 0x0D, 0x03, 0x00, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x01, 0x09};
static const uint8_t golden_cis_occupant[] = {
    DOWN_BYTES, CAN_BYTES, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0xFF, 0x00, 0x06, 0x05, 0x01, 0x00, 0x04};
static const uint8_t golden_cis_rear[] = {
    DOWN_BYTES, CAN_BYTES, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
    0xFF, 0xFF, 0x00, 0x06, 0x01};
static const uint8_t golden_cis_status[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0x04, 0x03, 0x02, 0x01, 0x00, 0x02, 0x01,
    0xFF, 0x01, 0x09, 0x00, 0x01, 0x08, 0x00,
    0x2B, 0x2C, 0x2D, 0x2E, 0x3F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
static const uint8_t golden_window_state[] = {
    DOWN_BYTES, CAN_BYTES, 0xFF, 0x03, 0xFF, 0x03, 0xFF, 0x01, 0x03, 0xFF,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
static const uint8_t golden_window_fault[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0xFF, 0x00, 0x02,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
static const uint8_t golden_vss_status[] = {
    DOWN_BYTES, CAN_BYTES, 0x03, 0x02, 0x01, 0x01, 0x1F, 0x05, 0x00,
    0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x03,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F};

typedef struct
{
    uart_m_message_type_t type;
    uart_m_direction_t direction;
    const uint8_t *bytes;
    size_t length;
} golden_fixture_t;

static const golden_fixture_t fixtures[] = {
    {UART_M_TYPE_CONTEXT, UART_M_DIRECTION_ESP32_TX, golden_context, sizeof(golden_context)},
    {UART_M_TYPE_DOMAIN_ALIVE, UART_M_DIRECTION_ESP32_RX, golden_domain_alive, sizeof(golden_domain_alive)},
    {UART_M_TYPE_REQUEST, UART_M_DIRECTION_ESP32_TX, golden_request, sizeof(golden_request)},
    {UART_M_TYPE_QUERY, UART_M_DIRECTION_ESP32_TX, golden_query, sizeof(golden_query)},
    {UART_M_TYPE_WARNING_ACK, UART_M_DIRECTION_ESP32_TX, golden_warning_ack, sizeof(golden_warning_ack)},
    {UART_M_TYPE_RESULT, UART_M_DIRECTION_ESP32_RX, golden_result, sizeof(golden_result)},
    {UART_M_TYPE_WARNING, UART_M_DIRECTION_ESP32_RX, golden_warning, sizeof(golden_warning)},
    {UART_M_TYPE_AVAILABILITY, UART_M_DIRECTION_ESP32_RX, golden_availability, sizeof(golden_availability)},
    {UART_M_TYPE_DIGITAL_STATUS, UART_M_DIRECTION_ESP32_RX, golden_digital_status, sizeof(golden_digital_status)},
    {UART_M_TYPE_DIGITAL_RESULT, UART_M_DIRECTION_ESP32_RX, golden_digital_result, sizeof(golden_digital_result)},
    {UART_M_TYPE_USER_SETTINGS, UART_M_DIRECTION_ESP32_RX, golden_user_settings, sizeof(golden_user_settings)},
    {UART_M_TYPE_BCM_DOOR_STATE, UART_M_DIRECTION_ESP32_RX, golden_bcm_door, sizeof(golden_bcm_door)},
    {UART_M_TYPE_BCM_CLIMATE_STATE, UART_M_DIRECTION_ESP32_RX, golden_bcm_climate, sizeof(golden_bcm_climate)},
    {UART_M_TYPE_BCM_LIGHT_STATE, UART_M_DIRECTION_ESP32_RX, golden_bcm_light, sizeof(golden_bcm_light)},
    {UART_M_TYPE_BCM_STATUS, UART_M_DIRECTION_ESP32_RX, golden_bcm_status, sizeof(golden_bcm_status)},
    {UART_M_TYPE_CIS_ENVIRONMENT, UART_M_DIRECTION_ESP32_RX, golden_cis_environment, sizeof(golden_cis_environment)},
    {UART_M_TYPE_CIS_OCCUPANT, UART_M_DIRECTION_ESP32_RX, golden_cis_occupant, sizeof(golden_cis_occupant)},
    {UART_M_TYPE_CIS_REAR, UART_M_DIRECTION_ESP32_RX, golden_cis_rear, sizeof(golden_cis_rear)},
    {UART_M_TYPE_CIS_STATUS, UART_M_DIRECTION_ESP32_RX, golden_cis_status, sizeof(golden_cis_status)},
    {UART_M_TYPE_WINDOW_STATE, UART_M_DIRECTION_ESP32_RX, golden_window_state, sizeof(golden_window_state)},
    {UART_M_TYPE_WINDOW_FAULT, UART_M_DIRECTION_ESP32_RX, golden_window_fault, sizeof(golden_window_fault)},
    {UART_M_TYPE_VSS_STATUS, UART_M_DIRECTION_ESP32_RX, golden_vss_status, sizeof(golden_vss_status)},
    {UART_M_TYPE_QUERY_END, UART_M_DIRECTION_ESP32_RX, golden_query_end, sizeof(golden_query_end)},
};

_Static_assert(ARRAY_SIZE(fixtures) == 23U, "UART-M v2 requires exactly 23 message fixtures");

static void set_app_context(uart_m_app_context_t *context)
{
    context->device_context_id = UINT32_C(0x04030201);
    context->session_id = UINT64_C(0x0C0B0A0908070605);
}

static void set_down_context(uart_m_down_context_t *context)
{
    set_app_context(&context->recipient);
    context->domain_boot_id = UINT32_C(0x100F0E0D);
    context->query_id = UINT32_C(0x14131211);
}

static void set_can_source(uart_m_can_source_t *source)
{
    source->boot_id = UINT32_C(0x18171615);
    source->tx_sequence = UINT32_C(0x1C1B1A19);
}

static void make_expected(uart_m_message_type_t type, uart_m_message_t *message)
{
    memset(message, 0, sizeof(*message));
    message->type = type;

    switch (type) {
    case UART_M_TYPE_CONTEXT: {
        uart_m_context_t *m = &message->body.context;
        set_app_context(&m->context);
        m->registration = UART_M_TRI_UNKNOWN;
        m->connection = UART_M_TRI_YES;
        m->app_active = UART_M_TRI_NO;
        m->app_quality = UART_M_QUALITY_STALE;
        m->app_update_sequence = UINT32_C(0x14131211);
        m->app_age_ms = UINT16_C(0x1615);
        m->proximity = UART_M_TRI_UNKNOWN;
        m->proximity_validity = UART_M_VALIDITY_VALID;
        m->proximity_reason = UART_M_QUALITY_REASON_RECOVERING;
        m->proximity_update_sequence = UINT32_C(0x1C1B1A19);
        m->proximity_age_ms = UINT16_C(0x1E1D);
        m->registration_connection_quality = UART_M_QUALITY_INVALID;
        m->registration_connection_update_sequence = UINT32_C(0x23222120);
        m->registration_connection_age_ms = UINT16_C(0x2524);
        break;
    }
    case UART_M_TYPE_DOMAIN_ALIVE: {
        uart_m_domain_alive_t *m = &message->body.domain_alive;
        m->domain_boot_id = UINT32_C(0x04030201);
        m->update_sequence = UINT32_C(0x08070605);
        m->source_age_ms = UINT16_C(0x0A09);
        m->domain_state = UART_M_CONTROLLER_FAULT;
        m->quality = UART_M_QUALITY_INVALID;
        break;
    }
    case UART_M_TYPE_REQUEST: {
        uart_m_request_t *m = &message->body.request;
        set_app_context(&m->context);
        m->request_id = UINT32_C(0x100F0E0D);
        m->kind = UART_M_REQUEST_CLIMATE;
        m->argument.climate.operation = UART_M_CLIMATE_TARGET_TEMPERATURE;
        m->argument.climate.target_temperature_centi_c = -300;
        break;
    }
    case UART_M_TYPE_QUERY: {
        uart_m_query_t *m = &message->body.query;
        set_app_context(&m->context);
        m->query_id = UINT32_C(0x100F0E0D);
        m->scope = UART_M_QUERY_ALL;
        m->request_session = UINT64_C(0x1817161514131211);
        m->request_id = UINT32_C(0x1C1B1A19);
        break;
    }
    case UART_M_TYPE_WARNING_ACK: {
        uart_m_warning_ack_t *m = &message->body.warning_ack;
        set_app_context(&m->context);
        m->warning_type = UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE;
        m->occurrence_id = UINT32_C(0x100F0E0D);
        m->warning_domain_boot = UINT32_C(0x14131211);
        break;
    }
    case UART_M_TYPE_RESULT: {
        uart_m_result_t *m = &message->body.result;
        set_down_context(&m->context);
        m->request_session = UINT64_C(0x1C1B1A1918171615);
        m->request_id = UINT32_C(0x201F1E1D);
        m->result = UART_M_RESULT_DONE;
        m->result_reason = 13U;
        m->confirmation = UART_M_CONFIRMED;
        m->command_target = UART_M_COMMAND_TARGET_BCM;
        m->command_id = UINT32_C(0x24232221);
        m->origin = UART_M_ORIGIN_PROXIMITY_AUTO;
        m->result_age_ms = UINT16_C(0x2625);
        break;
    }
    case UART_M_TYPE_WARNING: {
        uart_m_warning_t *m = &message->body.warning;
        set_down_context(&m->context);
        m->warning_type = UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE;
        m->occurrence_id = UINT32_C(0x18171615);
        m->severity = UART_M_SEVERITY_EMERGENCY;
        m->state = UART_M_WARNING_ACTIVE;
        m->read = UART_M_FLAG_YES;
        m->quality = UART_M_QUALITY_STALE;
        m->quality_reason = UART_M_QUALITY_REASON_RECOVERING;
        m->source_boot = UINT32_C(0x21201F1E);
        m->source_sequence = UINT32_C(0x25242322);
        m->source_age_ms = UINT16_C(0x2726);
        break;
    }
    case UART_M_TYPE_AVAILABILITY: {
        uart_m_availability_message_t *m = &message->body.availability;
        set_down_context(&m->context);
        for (size_t i = 0U; i < UART_M_AVAILABILITY_ITEM_COUNT; ++i) {
            m->items[i].function_id = (uart_m_function_id_t)i;
            m->items[i].availability = (uart_m_availability_t)(i % 3U);
            m->items[i].reason = (uint16_t)i;
        }
        m->update_sequence = UINT32_C(0x3C3B3A39);
        m->source_age_ms = UINT16_C(0x3E3D);
        break;
    }
    case UART_M_TYPE_DIGITAL_STATUS: {
        uart_m_digital_status_t *m = &message->body.digital_status;
        set_down_context(&m->context);
        m->setting = UART_M_FLAG_YES;
        m->availability = UART_M_AVAILABILITY_UNAVAILABLE;
        m->reason = 25U;
        m->update_sequence = UINT32_C(0x201F1E1D);
        m->source_age_ms = UINT16_C(0x2221);
        break;
    }
    case UART_M_TYPE_DIGITAL_RESULT: {
        uart_m_digital_result_t *m = &message->body.digital_result;
        set_down_context(&m->context);
        m->auto_occurrence_id = UINT32_C(0x18171615);
        m->related_door_command_id = UINT32_C(0x1C1B1A19);
        m->origin = UART_M_ORIGIN_PROXIMITY_AUTO;
        m->result = UART_M_RESULT_FAILED;
        m->result_reason = 9U;
        m->confirmation = UART_M_CONFIRMED;
        m->result_age_ms = UINT16_C(0x2322);
        break;
    }
    case UART_M_TYPE_USER_SETTINGS: {
        uart_m_user_settings_t *m = &message->body.user_settings;
        set_down_context(&m->context);
        m->target_temperature_centi_c = INT16_MIN;
        m->climate_auto = UART_M_TRI_UNKNOWN;
        m->fan_level = UART_M_FAN_UNKNOWN;
        m->light_enabled = UART_M_TRI_UNKNOWN;
        m->normal_light_level = 100U;
        m->normal_light_rgb = (uart_m_rgb_t){0x11U, 0x22U, 0x33U};
        m->quality = UART_M_QUALITY_NO_DATA;
        m->update_sequence = UINT32_C(0x2221201F);
        m->source_age_ms = UINT16_C(0x2423);
        break;
    }
    case UART_M_TYPE_QUERY_END: {
        uart_m_query_end_t *m = &message->body.query_end;
        set_down_context(&m->context);
        m->scope = UART_M_QUERY_ALL;
        m->status = UART_M_QUERY_PARTIAL_UNCONFIRMED;
        m->item_count = UINT16_C(0x1615);
        break;
    }
    case UART_M_TYPE_BCM_DOOR_STATE: {
        uart_m_bcm_door_state_t *m = &message->body.bcm_door_state;
        set_down_context(&m->context); set_can_source(&m->source);
        m->lock_state = UART_M_DOOR_LOCK_UNKNOWN;
        m->open_state = UART_M_DOOR_OPEN;
        m->composite_state = UART_M_DOOR_UNTRUSTED;
        m->lock_quality = UART_M_QUALITY_OK;
        m->open_quality = UART_M_QUALITY_STALE;
        m->composite_quality = UART_M_QUALITY_INVALID;
        m->update_sequence = UINT32_C(0x26252423);
        m->source_age_ms = UINT16_C(0x2827);
        break;
    }
    case UART_M_TYPE_BCM_CLIMATE_STATE: {
        uart_m_bcm_climate_state_t *m = &message->body.bcm_climate_state;
        set_down_context(&m->context); set_can_source(&m->source);
        m->fan_command = UART_M_FAN_HIGH;
        m->fan_measured = UART_M_FAN_UNKNOWN;
        m->thermal_direction = UART_M_THERMAL_HEAT;
        m->thermal_output_level = 100U;
        m->heat_removal = UART_M_HEAT_REMOVAL_UNAVAILABLE;
        m->fan_command_quality = UART_M_QUALITY_OK;
        m->fan_measured_quality = UART_M_QUALITY_STALE;
        m->thermal_direction_quality = UART_M_QUALITY_INVALID;
        m->thermal_output_quality = UART_M_QUALITY_NO_DATA;
        m->heat_removal_quality = UART_M_QUALITY_OK;
        m->update_sequence = UINT32_C(0x29282726);
        m->source_age_ms = UINT16_C(0x2B2A);
        break;
    }
    case UART_M_TYPE_BCM_LIGHT_STATE: {
        uart_m_bcm_light_state_t *m = &message->body.bcm_light_state;
        set_down_context(&m->context); set_can_source(&m->source);
        m->light_type = UART_M_LIGHT_FAULT;
        m->level = 100U;
        m->rgb = (uart_m_rgb_t){0x11U, 0x22U, 0x33U};
        m->apply_result = UART_M_LIGHT_APPLY_UNKNOWN;
        m->physical_feedback = UART_M_FLAG_YES;
        m->type_quality = UART_M_QUALITY_OK;
        m->level_quality = UART_M_QUALITY_STALE;
        m->color_quality = UART_M_QUALITY_INVALID;
        m->apply_quality = UART_M_QUALITY_NO_DATA;
        m->update_sequence = UINT32_C(0x2A292827);
        m->source_age_ms = UINT16_C(0x2C2B);
        break;
    }
    case UART_M_TYPE_BCM_STATUS: {
        uart_m_bcm_status_t *m = &message->body.bcm_status;
        set_down_context(&m->context); set_can_source(&m->source);
        m->ecu_state = UART_M_CONTROLLER_FAULT;
        m->fault_mask = UINT16_C(0x00A5);
        m->active_fault_category = UART_M_FAULT_CATEGORY_COMM;
        m->active_fault_code = 8U;
        m->affected_function_mask = 7U;
        m->recovering = UART_M_FLAG_YES;
        m->update_sequence = UINT32_C(0x27262524);
        m->source_age_ms = UINT16_C(0x2928);
        break;
    }
    case UART_M_TYPE_CIS_ENVIRONMENT: {
        uart_m_cis_environment_t *m = &message->body.cis_environment;
        set_down_context(&m->context); set_can_source(&m->source);
        m->temperature.value = -4000;
        m->temperature.sequence = UINT32_C(0x21201F1E);
        m->temperature.age_ms = UINT16_C(0x2322);
        m->temperature.validity = UART_M_VALIDITY_VALID;
        m->temperature.reason = UART_M_QUALITY_REASON_NONE;
        m->humidity.value = 10000U;
        m->humidity.sequence = UINT32_C(0x2B2A2928);
        m->humidity.age_ms = UINT16_C(0x2D2C);
        m->humidity.validity = UART_M_VALIDITY_VALID;
        m->humidity.reason = UART_M_QUALITY_REASON_RECOVERING;
        m->illuminance.value = UINT32_C(200000);
        m->illuminance.sequence = UINT32_C(0x37363534);
        m->illuminance.age_ms = UINT16_C(0x3938);
        m->illuminance.validity = UART_M_VALIDITY_VALID;
        m->illuminance.reason = UART_M_QUALITY_REASON_DATA_INVALID;
        break;
    }
    case UART_M_TYPE_CIS_OCCUPANT: {
        uart_m_cis_occupant_t *m = &message->body.cis_occupant;
        set_down_context(&m->context); set_can_source(&m->source);
        m->pi_boot_id = UINT32_C(0x201F1E1D);
        m->vision_sequence = UINT32_C(0x24232221);
        m->vision_age_ms = UINT16_C(0x2625);
        m->presence = UART_M_TRI_UNKNOWN;
        m->presence_validity = UART_M_VALIDITY_INVALID;
        m->presence_reason = UART_M_QUALITY_REASON_NO_DATA;
        m->occupant_count = 5U;
        m->count_validity = UART_M_VALIDITY_VALID;
        m->count_reason = UART_M_QUALITY_REASON_NONE;
        m->vision_function_status = UART_M_SENSOR_RECOVERING;
        break;
    }
    case UART_M_TYPE_CIS_REAR: {
        uart_m_cis_rear_t *m = &message->body.cis_rear;
        set_down_context(&m->context); set_can_source(&m->source);
        m->update_sequence = UINT32_C(0x201F1E1D);
        m->source_age_ms = UINT16_C(0x2221);
        m->rear_distance = UINT16_MAX;
        m->validity = UART_M_VALIDITY_INVALID;
        m->quality_reason = UART_M_QUALITY_REASON_NO_DATA;
        m->proximity_status = UART_M_PROXIMITY_NO_OBJECT;
        break;
    }
    case UART_M_TYPE_CIS_STATUS: {
        uart_m_cis_status_t *m = &message->body.cis_status;
        set_down_context(&m->context); set_can_source(&m->source);
        m->cis_state = UART_M_CONTROLLER_FAULT;
        m->vision_status = UART_M_SENSOR_RECOVERING;
        m->temperature_status = UART_M_SENSOR_FAULT;
        m->humidity_status = UART_M_SENSOR_ACTIVE;
        m->illuminance_status = UART_M_SENSOR_READY;
        m->rear_status = UART_M_SENSOR_NOT_READY;
        m->pi_uart_status = 2U;
        m->can_interface_status = 1U;
        m->fault_mask = UINT16_C(0x01FF);
        m->active_fault = 9U;
        m->recovering = UART_M_FLAG_YES;
        m->last_fault = 8U;
        m->last_fault_occurrence = UINT32_C(0x2E2D2C2B);
        m->affected_function_mask = UINT8_C(0x3F);
        m->update_sequence = UINT32_C(0x33323130);
        m->source_age_ms = UINT16_C(0x3534);
        break;
    }
    case UART_M_TYPE_WINDOW_STATE: {
        uart_m_window_state_t *m = &message->body.window_state;
        set_down_context(&m->context); set_can_source(&m->source);
        m->motion = UART_M_WINDOW_MOTION_UNKNOWN;
        m->ecu_state = UART_M_CONTROLLER_FAULT;
        m->position = UINT8_MAX;
        m->quality = UART_M_QUALITY_NO_DATA;
        m->fully_open = UART_M_TRI_UNKNOWN;
        m->fully_closed = UART_M_TRI_YES;
        m->antipinch = UART_M_ANTIPINCH_INTERRUPTED;
        m->reverse = UART_M_TRI_UNKNOWN;
        m->update_sequence = UINT32_C(0x27262524);
        m->source_age_ms = UINT16_C(0x2928);
        break;
    }
    case UART_M_TYPE_WINDOW_FAULT: {
        uart_m_window_fault_t *m = &message->body.window_fault;
        set_down_context(&m->context); set_can_source(&m->source);
        m->category = UART_M_FAULT_CATEGORY_COMM;
        m->code = UINT8_MAX;
        m->status = UART_M_WINDOW_FAULT_RECOVERING;
        m->occurrence_id = UINT32_C(0x23222120);
        m->update_sequence = UINT32_C(0x27262524);
        m->source_age_ms = UINT16_C(0x2928);
        break;
    }
    case UART_M_TYPE_VSS_STATUS: {
        uart_m_vss_status_t *m = &message->body.vss_status;
        set_down_context(&m->context); set_can_source(&m->source);
        m->state = UART_M_VSS_FAULT;
        m->availability = UART_M_VSS_UNAVAILABLE;
        m->accepting_events = UART_M_FLAG_YES;
        m->fault_active = UART_M_FLAG_YES;
        m->fault_mask = UINT8_C(0x1F);
        m->last_fault = 5U;
        m->last_fault_occurrence = UINT32_C(0x26252423);
        m->last_fault_age_ms = UINT16_C(0x2827);
        m->last_fault_quality = UART_M_QUALITY_NO_DATA;
        m->update_sequence = UINT32_C(0x2D2C2B2A);
        m->source_age_ms = UINT16_C(0x2F2E);
        break;
    }
    default:
        break;
    }
}

static bool test_descriptor_table(void)
{
    for (size_t i = 0U; i < ARRAY_SIZE(fixtures); ++i) {
        const uart_m_message_descriptor_t *descriptor =
            uart_m_codec_descriptor((uint8_t)fixtures[i].type);
        CHECK(descriptor != NULL);
        CHECK(descriptor->type == fixtures[i].type);
        CHECK(descriptor->direction == fixtures[i].direction);
        CHECK(descriptor->payload_length == fixtures[i].length);
        CHECK(descriptor->schema_only ==
              ((fixtures[i].type == UART_M_TYPE_WINDOW_STATE) ||
               (fixtures[i].type == UART_M_TYPE_WINDOW_FAULT)));
        CHECK(uart_m_codec_validate_direction((uint8_t)fixtures[i].type,
                                               fixtures[i].direction) == UART_M_CODEC_OK);
        CHECK(uart_m_codec_validate_direction(
                  (uint8_t)fixtures[i].type,
                  (fixtures[i].direction == UART_M_DIRECTION_ESP32_TX) ?
                      UART_M_DIRECTION_ESP32_RX : UART_M_DIRECTION_ESP32_TX) ==
              UART_M_CODEC_INVALID_DIRECTION);
    }
    CHECK(uart_m_codec_descriptor(UINT8_C(0x7F)) == NULL);
    CHECK(uart_m_codec_validate_direction(UINT8_C(0x7F), UART_M_DIRECTION_ESP32_RX) ==
          UART_M_CODEC_UNSUPPORTED_TYPE);
    return true;
}

static bool test_all_golden_vectors(void)
{
    for (size_t i = 0U; i < ARRAY_SIZE(fixtures); ++i) {
        uart_m_message_t expected;
        uart_m_message_t decoded;
        uint8_t encoded[128];
        size_t encoded_length = 0U;

        make_expected(fixtures[i].type, &expected);
        CHECK(uart_m_codec_encode(&expected, encoded, sizeof(encoded), &encoded_length) ==
              UART_M_CODEC_OK);
        CHECK(encoded_length == fixtures[i].length);
        CHECK(memcmp(encoded, fixtures[i].bytes, encoded_length) == 0);

        CHECK(uart_m_codec_decode((uint8_t)fixtures[i].type, fixtures[i].bytes,
                                  fixtures[i].length, &decoded) == UART_M_CODEC_OK);
        CHECK(memcmp(&decoded, &expected, sizeof(expected)) == 0);
    }
    return true;
}

static void make_request_base(uart_m_message_t *message)
{
    memset(message, 0, sizeof(*message));
    message->type = UART_M_TYPE_REQUEST;
    set_app_context(&message->body.request.context);
    message->body.request.request_id = 1U;
}

static bool encode_request_args(const uart_m_message_t *message, const uint8_t expected[8])
{
    uint8_t payload[24];
    size_t length = 0U;
    if (uart_m_codec_encode(message, payload, sizeof(payload), &length) != UART_M_CODEC_OK) {
        return false;
    }
    return (length == sizeof(payload)) && (memcmp(&payload[16], expected, 8U) == 0);
}

static bool test_request_variants(void)
{
    uart_m_message_t message;
    static const uint8_t door_unlock[8] = {1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U};
    static const uint8_t climate_temperature[8] = {2U, 1U, 0xD2U, 0x04U, 0U, 0U, 0U, 0U};
    static const uint8_t climate_auto[8] = {2U, 2U, 1U, 0U, 0U, 0U, 0U, 0U};
    static const uint8_t climate_fan[8] = {2U, 3U, 3U, 0U, 0U, 0U, 0U, 0U};
    static const uint8_t light_enable[8] = {3U, 1U, 1U, 0U, 0U, 0U, 0U, 0U};
    static const uint8_t light_level[8] = {3U, 2U, 100U, 0U, 0U, 0U, 0U, 0U};
    static const uint8_t light_rgb[8] = {3U, 3U, 0xA1U, 0xB2U, 0xC3U, 0U, 0U, 0U};
    static const uint8_t digital_key[8] = {4U, 1U, 1U, 0U, 0U, 0U, 0U, 0U};

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_DOOR;
    message.body.request.argument.door = UART_M_DOOR_OPERATION_UNLOCK;
    CHECK(encode_request_args(&message, door_unlock));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_CLIMATE;
    message.body.request.argument.climate.operation = UART_M_CLIMATE_TARGET_TEMPERATURE;
    message.body.request.argument.climate.target_temperature_centi_c = 1234;
    CHECK(encode_request_args(&message, climate_temperature));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_CLIMATE;
    message.body.request.argument.climate.operation = UART_M_CLIMATE_AUTO;
    message.body.request.argument.climate.enabled = UART_M_FLAG_YES;
    CHECK(encode_request_args(&message, climate_auto));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_CLIMATE;
    message.body.request.argument.climate.operation = UART_M_CLIMATE_FAN;
    message.body.request.argument.climate.fan_level = UART_M_FAN_HIGH;
    CHECK(encode_request_args(&message, climate_fan));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_INTERIOR_LIGHT;
    message.body.request.argument.light.operation = UART_M_LIGHT_ENABLE;
    message.body.request.argument.light.enabled = UART_M_FLAG_YES;
    CHECK(encode_request_args(&message, light_enable));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_INTERIOR_LIGHT;
    message.body.request.argument.light.operation = UART_M_LIGHT_LEVEL;
    message.body.request.argument.light.level_percent = 100U;
    CHECK(encode_request_args(&message, light_level));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_INTERIOR_LIGHT;
    message.body.request.argument.light.operation = UART_M_LIGHT_RGB;
    message.body.request.argument.light.rgb = (uart_m_rgb_t){0xA1U, 0xB2U, 0xC3U};
    CHECK(encode_request_args(&message, light_rgb));

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_DIGITAL_KEY;
    message.body.request.argument.digital_key_enabled = UART_M_FLAG_YES;
    CHECK(encode_request_args(&message, digital_key));
    return true;
}

static bool test_invalid_length_reserved_and_enum(void)
{
    uart_m_message_t message;
    uint8_t payload[128];

    CHECK(uart_m_codec_decode(UART_M_TYPE_QUERY, golden_query, sizeof(golden_query) - 1U,
                              &message) == UART_M_CODEC_INVALID_LENGTH);
    CHECK(uart_m_codec_decode(UART_M_TYPE_QUERY, golden_query, sizeof(golden_query) + 1U,
                              &message) == UART_M_CODEC_INVALID_LENGTH);
    CHECK(uart_m_codec_decode(UINT8_C(0x7F), golden_query, sizeof(golden_query), &message) ==
          UART_M_CODEC_UNSUPPORTED_TYPE);

    memcpy(payload, golden_query, sizeof(golden_query));
    payload[17] = 1U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_QUERY, payload, sizeof(golden_query), &message) ==
          UART_M_CODEC_INVALID_RESERVED);
    memcpy(payload, golden_query, sizeof(golden_query));
    payload[30] = 1U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_QUERY, payload, sizeof(golden_query), &message) ==
          UART_M_CODEC_INVALID_RESERVED);
    memcpy(payload, golden_request, sizeof(golden_request));
    payload[23] = 1U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_REQUEST, payload, sizeof(golden_request), &message) ==
          UART_M_CODEC_INVALID_RESERVED);

    memcpy(payload, golden_warning, sizeof(golden_warning));
    payload[20] = 0U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_WARNING, payload, sizeof(golden_warning), &message) ==
          UART_M_CODEC_INVALID_ENUM);
    memcpy(payload, golden_context, sizeof(golden_context));
    payload[12] = 2U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_CONTEXT, payload, sizeof(golden_context), &message) ==
          UART_M_CODEC_INVALID_ENUM);
    memcpy(payload, golden_availability, sizeof(golden_availability));
    payload[24] = 9U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_AVAILABILITY, payload, sizeof(golden_availability),
                              &message) == UART_M_CODEC_INVALID_RESERVED);
    memcpy(payload, golden_result, sizeof(golden_result));
    payload[33] = 26U;
    payload[34] = 0U;
    CHECK(uart_m_codec_decode(UART_M_TYPE_RESULT, payload, sizeof(golden_result), &message) ==
          UART_M_CODEC_INVALID_ENUM);
    return true;
}

static bool test_context_and_range_validation(void)
{
    uart_m_message_t message;
    uint8_t payload[128];
    size_t length = 0U;

    make_expected(UART_M_TYPE_QUERY, &message);
    message.body.query.context.device_context_id = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);

    make_expected(UART_M_TYPE_RESULT, &message);
    message.body.result.context.domain_boot_id = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);
    message.body.result.context.domain_boot_id = UINT32_C(0x0F0E0D0C);
    message.body.result.request_id = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_RANGE);

    make_expected(UART_M_TYPE_DOMAIN_ALIVE, &message);
    message.body.domain_alive.domain_boot_id = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);

    make_expected(UART_M_TYPE_WARNING_ACK, &message);
    message.body.warning_ack.warning_domain_boot = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);

    make_expected(UART_M_TYPE_WARNING, &message);
    message.body.warning.source_boot = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);

    make_expected(UART_M_TYPE_CIS_OCCUPANT, &message);
    message.body.cis_occupant.pi_boot_id = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_CONTEXT);

    make_request_base(&message);
    message.body.request.kind = UART_M_REQUEST_CLIMATE;
    message.body.request.argument.climate.operation = UART_M_CLIMATE_TARGET_TEMPERATURE;
    message.body.request.argument.climate.target_temperature_centi_c = INT16_MIN;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_RANGE);
    message.body.request.argument.climate.target_temperature_centi_c = INT16_MAX;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) == UART_M_CODEC_OK);
    message.body.request.argument.climate.target_temperature_centi_c = -32767;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) == UART_M_CODEC_OK);

    make_expected(UART_M_TYPE_CIS_ENVIRONMENT, &message);
    message.body.cis_environment.temperature.value = 12500;
    message.body.cis_environment.humidity.value = 0U;
    message.body.cis_environment.illuminance.value = 0U;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) == UART_M_CODEC_OK);
    message.body.cis_environment.temperature.value = 12501;
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_RANGE);

    make_expected(UART_M_TYPE_BCM_STATUS, &message);
    message.body.bcm_status.fault_mask = UINT16_C(0x0100);
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_RANGE);
    make_expected(UART_M_TYPE_VSS_STATUS, &message);
    message.body.vss_status.fault_mask = UINT8_C(0x20);
    CHECK(uart_m_codec_encode(&message, payload, sizeof(payload), &length) ==
          UART_M_CODEC_INVALID_RANGE);

    make_expected(UART_M_TYPE_QUERY, &message);
    CHECK(uart_m_codec_encode(&message, payload, sizeof(golden_query) - 1U, &length) ==
          UART_M_CODEC_OUTPUT_TOO_SMALL);
    return true;
}

static bool test_frame_codec_integration(void)
{
    uart_m_message_t expected;
    uart_m_message_t decoded_message;
    uart_m_frame_t decoded_frame;
    uint8_t payload[128];
    uint8_t frame[UART_M_MAX_FRAME_SIZE];
    size_t payload_length = 0U;
    size_t frame_length = 0U;

    make_expected(UART_M_TYPE_QUERY, &expected);
    CHECK(uart_m_codec_encode(&expected, payload, sizeof(payload), &payload_length) ==
          UART_M_CODEC_OK);
    CHECK(uart_m_frame_encode((uint8_t)expected.type, UINT16_C(0xFFFE), payload,
                              payload_length, frame, sizeof(frame), &frame_length) ==
          UART_M_FRAME_STATUS_OK);
    CHECK(frame_length == payload_length + UART_M_MIN_FRAME_SIZE);
    CHECK(uart_m_frame_decode(frame, frame_length, &decoded_frame) == UART_M_FRAME_STATUS_OK);
    CHECK(decoded_frame.message_type == (uint8_t)UART_M_TYPE_QUERY);
    CHECK(decoded_frame.link_sequence == UINT16_C(0xFFFE));
    CHECK(uart_m_codec_decode(decoded_frame.message_type, decoded_frame.payload,
                              decoded_frame.payload_length, &decoded_message) == UART_M_CODEC_OK);
    CHECK(memcmp(&decoded_message, &expected, sizeof(expected)) == 0);
    return true;
}

typedef bool (*test_function_t)(void);
typedef struct { const char *name; test_function_t function; } test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"descriptor_table", test_descriptor_table},
        {"all_23_golden_vectors", test_all_golden_vectors},
        {"request_variants", test_request_variants},
        {"invalid_length_reserved_enum", test_invalid_length_reserved_and_enum},
        {"context_and_range_validation", test_context_and_range_validation},
        {"p1_frame_integration", test_frame_codec_integration},
    };

    for (size_t i = 0U; i < ARRAY_SIZE(tests); ++i) {
        if (!tests[i].function()) {
            printf("[FAIL] %s\n", tests[i].name);
            return 1;
        }
        printf("[PASS] %s\n", tests[i].name);
    }
    printf("All %zu UART-M codec test groups passed.\n", ARRAY_SIZE(tests));
    return 0;
}
