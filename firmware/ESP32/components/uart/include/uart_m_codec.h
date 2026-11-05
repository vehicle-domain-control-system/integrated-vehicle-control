#ifndef UART_M_CODEC_H
#define UART_M_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All UART-M v2 payload layouts remain WIRE_PROPOSED in 20_UART_M.md. */
#define UART_M_AVAILABILITY_ITEM_COUNT 9U

typedef enum
{
    UART_M_CODEC_OK = 0,
    UART_M_CODEC_INVALID_ARGUMENT,
    UART_M_CODEC_UNSUPPORTED_TYPE,
    UART_M_CODEC_INVALID_DIRECTION,
    UART_M_CODEC_INVALID_LENGTH,
    UART_M_CODEC_OUTPUT_TOO_SMALL,
    UART_M_CODEC_INVALID_ENUM,
    UART_M_CODEC_INVALID_RESERVED,
    UART_M_CODEC_INVALID_RANGE,
    UART_M_CODEC_INVALID_CONTEXT
} uart_m_codec_status_t;

typedef enum
{
    UART_M_DIRECTION_ESP32_TX = 0,
    UART_M_DIRECTION_ESP32_RX
} uart_m_direction_t;

typedef enum
{
    UART_M_TYPE_CONTEXT = 0x01,
    UART_M_TYPE_DOMAIN_ALIVE = 0x02,
    UART_M_TYPE_REQUEST = 0x10,
    UART_M_TYPE_QUERY = 0x11,
    UART_M_TYPE_WARNING_ACK = 0x12,
    UART_M_TYPE_RESULT = 0x20,
    UART_M_TYPE_WARNING = 0x21,
    UART_M_TYPE_AVAILABILITY = 0x22,
    UART_M_TYPE_DIGITAL_STATUS = 0x23,
    UART_M_TYPE_DIGITAL_RESULT = 0x24,
    UART_M_TYPE_USER_SETTINGS = 0x25,
    UART_M_TYPE_BCM_DOOR_STATE = 0x30,
    UART_M_TYPE_BCM_CLIMATE_STATE = 0x31,
    UART_M_TYPE_BCM_LIGHT_STATE = 0x32,
    UART_M_TYPE_BCM_STATUS = 0x33,
    UART_M_TYPE_CIS_ENVIRONMENT = 0x34,
    UART_M_TYPE_CIS_OCCUPANT = 0x35,
    UART_M_TYPE_CIS_REAR = 0x36,
    UART_M_TYPE_CIS_STATUS = 0x37,
    UART_M_TYPE_WINDOW_STATE = 0x38,
    UART_M_TYPE_WINDOW_FAULT = 0x39,
    UART_M_TYPE_VSS_STATUS = 0x3A,
    UART_M_TYPE_QUERY_END = 0x40
} uart_m_message_type_t;

typedef struct
{
    uart_m_message_type_t type;
    uart_m_direction_t direction;
    uint16_t payload_length;
    bool schema_only;
} uart_m_message_descriptor_t;

typedef enum { UART_M_FLAG_NO = 0, UART_M_FLAG_YES } uart_m_flag_t;
typedef enum { UART_M_TRI_NO = 0, UART_M_TRI_YES, UART_M_TRI_UNKNOWN } uart_m_tri_state_t;
typedef enum {
    UART_M_QUALITY_OK = 0,
    UART_M_QUALITY_STALE,
    UART_M_QUALITY_INVALID,
    UART_M_QUALITY_NO_DATA
} uart_m_value_quality_t;
typedef enum { UART_M_VALIDITY_INVALID = 0, UART_M_VALIDITY_VALID } uart_m_validity_t;
typedef enum {
    UART_M_QUALITY_REASON_NONE = 0,
    UART_M_QUALITY_REASON_NOT_READY,
    UART_M_QUALITY_REASON_OUT_OF_RANGE,
    UART_M_QUALITY_REASON_SENSOR_FAULT,
    UART_M_QUALITY_REASON_VISION_FAULT,
    UART_M_QUALITY_REASON_STALE,
    UART_M_QUALITY_REASON_NO_DATA,
    UART_M_QUALITY_REASON_COMMUNICATION_FAULT,
    UART_M_QUALITY_REASON_RECOVERING,
    UART_M_QUALITY_REASON_DATA_INVALID
} uart_m_quality_reason_t;
typedef enum {
    UART_M_RESULT_ACCEPTED = 0,
    UART_M_RESULT_IN_PROGRESS,
    UART_M_RESULT_DONE,
    UART_M_RESULT_REJECTED,
    UART_M_RESULT_CANCELLED,
    UART_M_RESULT_FAILED,
    UART_M_RESULT_NOT_PRESENT
} uart_m_vehicle_result_t;
typedef enum { UART_M_UNCONFIRMED = 0, UART_M_CONFIRMED } uart_m_confirmation_t;
typedef enum {
    UART_M_AVAILABILITY_AVAILABLE = 0,
    UART_M_AVAILABILITY_LIMITED,
    UART_M_AVAILABILITY_UNAVAILABLE
} uart_m_availability_t;
typedef enum {
    UART_M_ORIGIN_NONE = 0,
    UART_M_ORIGIN_MOBILE_MANUAL,
    UART_M_ORIGIN_PROXIMITY_AUTO,
    UART_M_ORIGIN_LOCAL,
    UART_M_ORIGIN_TEST,
    UART_M_ORIGIN_VEHICLE_POLICY
} uart_m_origin_t;
typedef enum {
    UART_M_WARNING_REAR = 0,
    UART_M_WARNING_OCCUPANT,
    UART_M_WARNING_ANTIPINCH,
    UART_M_WARNING_BCM_FAULT,
    UART_M_WARNING_CIS_FAULT,
    UART_M_WARNING_WINDOW_FAULT,
    UART_M_WARNING_VSS_FAULT,
    UART_M_WARNING_OPEN_DOOR_AFTER_LEAVE
} uart_m_warning_type_t;
typedef enum {
    UART_M_SEVERITY_INFO = 0,
    UART_M_SEVERITY_CAUTION,
    UART_M_SEVERITY_EMERGENCY
} uart_m_severity_t;
typedef enum { UART_M_WARNING_CLEAR = 0, UART_M_WARNING_ACTIVE } uart_m_warning_state_t;
typedef enum {
    UART_M_FUNCTION_DOOR = 0,
    UART_M_FUNCTION_CLIMATE,
    UART_M_FUNCTION_INTERIOR_LIGHT,
    UART_M_FUNCTION_DIGITAL_KEY,
    UART_M_FUNCTION_OCCUPANT,
    UART_M_FUNCTION_ENVIRONMENT,
    UART_M_FUNCTION_REAR_WARNING,
    UART_M_FUNCTION_WINDOW,
    UART_M_FUNCTION_VSS
} uart_m_function_id_t;
typedef enum {
    UART_M_FAULT_CATEGORY_NONE = 0,
    UART_M_FAULT_CATEGORY_SENSOR,
    UART_M_FAULT_CATEGORY_FUNCTION,
    UART_M_FAULT_CATEGORY_COMM
} uart_m_fault_category_t;
typedef enum {
    UART_M_CONTROLLER_INIT = 0,
    UART_M_CONTROLLER_READY,
    UART_M_CONTROLLER_DEGRADED,
    UART_M_CONTROLLER_FAULT
} uart_m_controller_state_t;
typedef enum {
    UART_M_SENSOR_NOT_READY = 0,
    UART_M_SENSOR_READY,
    UART_M_SENSOR_ACTIVE,
    UART_M_SENSOR_FAULT,
    UART_M_SENSOR_RECOVERING
} uart_m_sensor_state_t;
typedef enum {
    UART_M_FAN_OFF = 0,
    UART_M_FAN_LOW,
    UART_M_FAN_MEDIUM,
    UART_M_FAN_HIGH,
    UART_M_FAN_UNKNOWN
} uart_m_fan_level_t;
typedef enum {
    UART_M_THERMAL_IDLE = 0,
    UART_M_THERMAL_COOL,
    UART_M_THERMAL_HEAT
} uart_m_thermal_direction_t;
typedef enum {
    UART_M_HEAT_REMOVAL_NORMAL = 0,
    UART_M_HEAT_REMOVAL_OVERHEAT_CUTOFF,
    UART_M_HEAT_REMOVAL_UNAVAILABLE
} uart_m_heat_removal_t;
typedef enum {
    UART_M_LIGHT_NORMAL = 0,
    UART_M_LIGHT_GOODBYE,
    UART_M_LIGHT_WARNING,
    UART_M_LIGHT_FAULT
} uart_m_light_type_t;
typedef enum {
    UART_M_LIGHT_APPLIED = 0,
    UART_M_LIGHT_FAILED,
    UART_M_LIGHT_APPLY_UNKNOWN
} uart_m_light_apply_t;
typedef enum {
    UART_M_DOOR_LOCKED = 0,
    UART_M_DOOR_UNLOCKED,
    UART_M_DOOR_LOCK_UNKNOWN
} uart_m_door_lock_state_t;
typedef enum {
    UART_M_DOOR_CLOSED = 0,
    UART_M_DOOR_OPEN,
    UART_M_DOOR_OPEN_UNKNOWN
} uart_m_door_open_state_t;
typedef enum {
    UART_M_DOOR_NORMAL = 0,
    UART_M_DOOR_INCONSISTENT,
    UART_M_DOOR_UNTRUSTED
} uart_m_door_composite_t;
typedef enum {
    UART_M_PROXIMITY_VALID_DISTANCE = 0,
    UART_M_PROXIMITY_NO_OBJECT,
    UART_M_PROXIMITY_UNAVAILABLE,
    UART_M_PROXIMITY_FAULT,
    UART_M_PROXIMITY_RECOVERING,
    UART_M_PROXIMITY_INACTIVE
} uart_m_proximity_status_t;
typedef enum {
    UART_M_VSS_STARTUP = 0,
    UART_M_VSS_READY,
    UART_M_VSS_PLAYING,
    UART_M_VSS_FAULT
} uart_m_vss_state_t;
typedef enum {
    UART_M_VSS_FULL = 0,
    UART_M_VSS_DEGRADED,
    UART_M_VSS_UNAVAILABLE
} uart_m_vss_availability_t;
typedef enum {
    UART_M_WINDOW_STOPPED = 0,
    UART_M_WINDOW_OPENING,
    UART_M_WINDOW_CLOSING,
    UART_M_WINDOW_ANTIPINCH_REVERSING,
    UART_M_WINDOW_MOTION_UNKNOWN
} uart_m_window_motion_t;
typedef enum {
    UART_M_ANTIPINCH_IDLE = 0,
    UART_M_ANTIPINCH_IN_PROGRESS,
    UART_M_ANTIPINCH_DONE,
    UART_M_ANTIPINCH_INTERRUPTED
} uart_m_antipinch_state_t;
typedef enum {
    UART_M_WINDOW_FAULT_NONE = 0,
    UART_M_WINDOW_FAULT_ACTIVE,
    UART_M_WINDOW_FAULT_RECOVERING
} uart_m_window_fault_state_t;
typedef enum {
    UART_M_QUERY_STATE = 0,
    UART_M_QUERY_RESULT,
    UART_M_QUERY_WARNING,
    UART_M_QUERY_ALL
} uart_m_query_scope_t;
typedef enum {
    UART_M_QUERY_DONE = 0,
    UART_M_QUERY_NOT_FOUND,
    UART_M_QUERY_PARTIAL_UNCONFIRMED,
    UART_M_QUERY_CONTEXT_REJECTED
} uart_m_query_status_t;
typedef enum {
    UART_M_COMMAND_TARGET_DOMAIN_SETTING = 0,
    UART_M_COMMAND_TARGET_BCM,
    UART_M_COMMAND_TARGET_WINDOW
} uart_m_command_target_t;

typedef struct { uint8_t r; uint8_t g; uint8_t b; } uart_m_rgb_t;
typedef struct { uint32_t device_context_id; uint64_t session_id; } uart_m_app_context_t;
typedef struct {
    uart_m_app_context_t recipient;
    uint32_t domain_boot_id;
    uint32_t query_id;
} uart_m_down_context_t;
typedef struct { uint32_t boot_id; uint32_t tx_sequence; } uart_m_can_source_t;

typedef struct {
    uart_m_app_context_t context;
    uart_m_tri_state_t registration;
    uart_m_tri_state_t connection;
    uart_m_tri_state_t app_active;
    uart_m_value_quality_t app_quality;
    uint32_t app_update_sequence;
    uint16_t app_age_ms;
    uart_m_tri_state_t proximity;
    uart_m_validity_t proximity_validity;
    uart_m_quality_reason_t proximity_reason;
    uint32_t proximity_update_sequence;
    uint16_t proximity_age_ms;
    uart_m_value_quality_t registration_connection_quality;
    uint32_t registration_connection_update_sequence;
    uint16_t registration_connection_age_ms;
} uart_m_context_t;

typedef enum {
    UART_M_REQUEST_DOOR = 0,
    UART_M_REQUEST_CLIMATE,
    UART_M_REQUEST_INTERIOR_LIGHT,
    UART_M_REQUEST_DIGITAL_KEY
} uart_m_request_kind_t;
typedef enum { UART_M_DOOR_OPERATION_LOCK = 0, UART_M_DOOR_OPERATION_UNLOCK } uart_m_door_operation_t;
typedef enum {
    UART_M_CLIMATE_TARGET_TEMPERATURE = 0,
    UART_M_CLIMATE_AUTO,
    UART_M_CLIMATE_FAN
} uart_m_climate_operation_t;
typedef enum {
    UART_M_LIGHT_ENABLE = 0,
    UART_M_LIGHT_LEVEL,
    UART_M_LIGHT_RGB
} uart_m_light_operation_t;
typedef struct {
    uart_m_app_context_t context;
    uint32_t request_id;
    uart_m_request_kind_t kind;
    union {
        uart_m_door_operation_t door;
        struct {
            uart_m_climate_operation_t operation;
            int16_t target_temperature_centi_c;
            uart_m_flag_t enabled;
            uart_m_fan_level_t fan_level;
        } climate;
        struct {
            uart_m_light_operation_t operation;
            uart_m_flag_t enabled;
            uint8_t level_percent;
            uart_m_rgb_t rgb;
        } light;
        uart_m_flag_t digital_key_enabled;
    } argument;
} uart_m_request_t;

typedef struct {
    uart_m_app_context_t context;
    uint32_t query_id;
    uart_m_query_scope_t scope;
    uint64_t request_session;
    uint32_t request_id;
} uart_m_query_t;
typedef struct {
    uart_m_app_context_t context;
    uart_m_warning_type_t warning_type;
    uint32_t occurrence_id;
    uint32_t warning_domain_boot;
} uart_m_warning_ack_t;
typedef struct {
    uint32_t domain_boot_id;
    uint32_t update_sequence;
    uint16_t source_age_ms;
    uart_m_controller_state_t domain_state;
    uart_m_value_quality_t quality;
} uart_m_domain_alive_t;
typedef struct {
    uart_m_down_context_t context;
    uint64_t request_session;
    uint32_t request_id;
    uart_m_vehicle_result_t result;
    uint16_t result_reason;
    uart_m_confirmation_t confirmation;
    uart_m_command_target_t command_target;
    uint32_t command_id;
    uart_m_origin_t origin;
    uint16_t result_age_ms;
} uart_m_result_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_warning_type_t warning_type;
    uint32_t occurrence_id;
    uart_m_severity_t severity;
    uart_m_warning_state_t state;
    uart_m_flag_t read;
    uart_m_value_quality_t quality;
    uart_m_quality_reason_t quality_reason;
    uint32_t source_boot;
    uint32_t source_sequence;
    uint16_t source_age_ms;
} uart_m_warning_t;
typedef struct {
    uart_m_function_id_t function_id;
    uart_m_availability_t availability;
    uint16_t reason;
} uart_m_availability_item_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_availability_item_t items[UART_M_AVAILABILITY_ITEM_COUNT];
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_availability_message_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_flag_t setting;
    uart_m_availability_t availability;
    uint16_t reason;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_digital_status_t;
typedef struct {
    uart_m_down_context_t context;
    uint32_t auto_occurrence_id;
    uint32_t related_door_command_id;
    uart_m_origin_t origin;
    uart_m_vehicle_result_t result;
    uint16_t result_reason;
    uart_m_confirmation_t confirmation;
    uint16_t result_age_ms;
} uart_m_digital_result_t;
typedef struct {
    uart_m_down_context_t context;
    int16_t target_temperature_centi_c;
    uart_m_tri_state_t climate_auto;
    uart_m_fan_level_t fan_level;
    uart_m_tri_state_t light_enabled;
    uint8_t normal_light_level;
    uart_m_rgb_t normal_light_rgb;
    uart_m_value_quality_t quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_user_settings_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_query_scope_t scope;
    uart_m_query_status_t status;
    uint16_t item_count;
} uart_m_query_end_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_door_lock_state_t lock_state;
    uart_m_door_open_state_t open_state;
    uart_m_door_composite_t composite_state;
    uart_m_value_quality_t lock_quality;
    uart_m_value_quality_t open_quality;
    uart_m_value_quality_t composite_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_bcm_door_state_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_fan_level_t fan_command;
    uart_m_fan_level_t fan_measured;
    uart_m_thermal_direction_t thermal_direction;
    uint8_t thermal_output_level;
    uart_m_heat_removal_t heat_removal;
    uart_m_value_quality_t fan_command_quality;
    uart_m_value_quality_t fan_measured_quality;
    uart_m_value_quality_t thermal_direction_quality;
    uart_m_value_quality_t thermal_output_quality;
    uart_m_value_quality_t heat_removal_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_bcm_climate_state_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_light_type_t light_type;
    uint8_t level;
    uart_m_rgb_t rgb;
    uart_m_light_apply_t apply_result;
    uart_m_flag_t physical_feedback;
    uart_m_value_quality_t type_quality;
    uart_m_value_quality_t level_quality;
    uart_m_value_quality_t color_quality;
    uart_m_value_quality_t apply_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_bcm_light_state_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_controller_state_t ecu_state;
    uint16_t fault_mask;
    uart_m_fault_category_t active_fault_category;
    uint16_t active_fault_code;
    uint8_t affected_function_mask;
    uart_m_flag_t recovering;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_bcm_status_t;
typedef struct {
    int16_t value;
    uint32_t sequence;
    uint16_t age_ms;
    uart_m_validity_t validity;
    uart_m_quality_reason_t reason;
} uart_m_temperature_measurement_t;
typedef struct {
    uint16_t value;
    uint32_t sequence;
    uint16_t age_ms;
    uart_m_validity_t validity;
    uart_m_quality_reason_t reason;
} uart_m_humidity_measurement_t;
typedef struct {
    uint32_t value;
    uint32_t sequence;
    uint16_t age_ms;
    uart_m_validity_t validity;
    uart_m_quality_reason_t reason;
} uart_m_illuminance_measurement_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_temperature_measurement_t temperature;
    uart_m_humidity_measurement_t humidity;
    uart_m_illuminance_measurement_t illuminance;
} uart_m_cis_environment_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uint32_t pi_boot_id;
    uint32_t vision_sequence;
    uint16_t vision_age_ms;
    uart_m_tri_state_t presence;
    uart_m_validity_t presence_validity;
    uart_m_quality_reason_t presence_reason;
    uint8_t occupant_count;
    uart_m_validity_t count_validity;
    uart_m_quality_reason_t count_reason;
    uart_m_sensor_state_t vision_function_status;
} uart_m_cis_occupant_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uint32_t update_sequence;
    uint16_t source_age_ms;
    uint16_t rear_distance;
    uart_m_validity_t validity;
    uart_m_quality_reason_t quality_reason;
    uart_m_proximity_status_t proximity_status;
} uart_m_cis_rear_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_controller_state_t cis_state;
    uart_m_sensor_state_t vision_status;
    uart_m_sensor_state_t temperature_status;
    uart_m_sensor_state_t humidity_status;
    uart_m_sensor_state_t illuminance_status;
    uart_m_sensor_state_t rear_status;
    uint8_t pi_uart_status;
    uint8_t can_interface_status;
    uint16_t fault_mask;
    uint16_t active_fault;
    uart_m_flag_t recovering;
    uint16_t last_fault;
    uint32_t last_fault_occurrence;
    uint8_t affected_function_mask;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_cis_status_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_window_motion_t motion;
    uart_m_controller_state_t ecu_state;
    uint8_t position;
    uart_m_value_quality_t quality;
    uart_m_tri_state_t fully_open;
    uart_m_tri_state_t fully_closed;
    uart_m_antipinch_state_t antipinch;
    uart_m_tri_state_t reverse;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_window_state_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_fault_category_t category;
    uint16_t code;
    uart_m_window_fault_state_t status;
    uint32_t occurrence_id;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_window_fault_t;
typedef struct {
    uart_m_down_context_t context;
    uart_m_can_source_t source;
    uart_m_vss_state_t state;
    uart_m_vss_availability_t availability;
    uart_m_flag_t accepting_events;
    uart_m_flag_t fault_active;
    uint8_t fault_mask;
    uint16_t last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    uart_m_value_quality_t last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} uart_m_vss_status_t;

typedef struct
{
    uart_m_message_type_t type;
    union {
        uart_m_context_t context;
        uart_m_domain_alive_t domain_alive;
        uart_m_request_t request;
        uart_m_query_t query;
        uart_m_warning_ack_t warning_ack;
        uart_m_result_t result;
        uart_m_warning_t warning;
        uart_m_availability_message_t availability;
        uart_m_digital_status_t digital_status;
        uart_m_digital_result_t digital_result;
        uart_m_user_settings_t user_settings;
        uart_m_query_end_t query_end;
        uart_m_bcm_door_state_t bcm_door_state;
        uart_m_bcm_climate_state_t bcm_climate_state;
        uart_m_bcm_light_state_t bcm_light_state;
        uart_m_bcm_status_t bcm_status;
        uart_m_cis_environment_t cis_environment;
        uart_m_cis_occupant_t cis_occupant;
        uart_m_cis_rear_t cis_rear;
        uart_m_cis_status_t cis_status;
        uart_m_window_state_t window_state;
        uart_m_window_fault_t window_fault;
        uart_m_vss_status_t vss_status;
    } body;
} uart_m_message_t;

const uart_m_message_descriptor_t *uart_m_codec_descriptor(uint8_t message_type);
uart_m_codec_status_t uart_m_codec_validate_direction(
    uint8_t message_type,
    uart_m_direction_t direction);
uart_m_codec_status_t uart_m_codec_encode(
    const uart_m_message_t *message,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length);
uart_m_codec_status_t uart_m_codec_decode(
    uint8_t message_type,
    const uint8_t *payload,
    size_t payload_length,
    uart_m_message_t *message);

#ifdef __cplusplus
}
#endif

#endif
