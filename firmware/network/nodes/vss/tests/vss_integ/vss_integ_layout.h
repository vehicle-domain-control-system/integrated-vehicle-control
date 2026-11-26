#ifndef VSS_INTEG_LAYOUT_H
#define VSS_INTEG_LAYOUT_H

/* Application types and codes that cross the VSS <-> Network link boundary
 * (two header model: VSS compiles with firmware/vss src/net_if/vss_net_api.h,
 * the Network with network/app/net_if_*.h; they meet only at link time).
 *
 * One list, expanded by each half of the harness with its own headers
 * (case vss_integ.vi0_api_layout): the struct sizes, every field's offset and
 * size, and the code values. No type is declared here; the macros name types
 * and constants that both headers must declare with the same spelling.
 *   SIZE(T)      sizeof(T)
 *   FIELD(T, f)  offsetof(T, f), then sizeof of the field
 *   VALUE(v)     the value of a code or constant */
#define VSS_INTEG_API_LAYOUT(SIZE, FIELD, VALUE) \
    SIZE(NetCanSource) \
    FIELD(NetCanSource, boot_id) \
    FIELD(NetCanSource, tx_sequence) \
    SIZE(NetCanRxMeta) \
    FIELD(NetCanRxMeta, source) \
    FIELD(NetCanRxMeta, received_at_ms) \
    SIZE(NetVssEvent) \
    FIELD(NetVssEvent, event_kind) \
    FIELD(NetVssEvent, occurrence_id) \
    FIELD(NetVssEvent, original_age_ms) \
    FIELD(NetVssEvent, use_limit_ms) \
    FIELD(NetVssEvent, value_quality) \
    FIELD(NetVssEvent, input_availability) \
    FIELD(NetVssEvent, related_command_id) \
    SIZE(NetVssWarningState) \
    FIELD(NetVssWarningState, antipinch_source_boot) \
    FIELD(NetVssWarningState, antipinch_source_sequence) \
    FIELD(NetVssWarningState, antipinch_original_age_ms) \
    FIELD(NetVssWarningState, antipinch_state) \
    FIELD(NetVssWarningState, antipinch_quality) \
    FIELD(NetVssWarningState, antipinch_reason) \
    FIELD(NetVssWarningState, antipinch_availability) \
    FIELD(NetVssWarningState, occupant_hazard_source_boot) \
    FIELD(NetVssWarningState, occupant_hazard_source_sequence) \
    FIELD(NetVssWarningState, occupant_hazard_original_age_ms) \
    FIELD(NetVssWarningState, occupant_hazard_state) \
    FIELD(NetVssWarningState, occupant_hazard_quality) \
    FIELD(NetVssWarningState, occupant_hazard_reason) \
    FIELD(NetVssWarningState, occupant_hazard_availability) \
    FIELD(NetVssWarningState, rear_obstacle_source_boot) \
    FIELD(NetVssWarningState, rear_obstacle_source_sequence) \
    FIELD(NetVssWarningState, rear_obstacle_original_age_ms) \
    FIELD(NetVssWarningState, rear_obstacle_state) \
    FIELD(NetVssWarningState, rear_obstacle_quality) \
    FIELD(NetVssWarningState, rear_obstacle_reason) \
    FIELD(NetVssWarningState, rear_obstacle_availability) \
    FIELD(NetVssWarningState, rear_detection_activation) \
    FIELD(NetVssWarningState, activation_meta_format) \
    FIELD(NetVssWarningState, rear_activation_quality) \
    FIELD(NetVssWarningState, rear_activation_reason) \
    FIELD(NetVssWarningState, rear_activation_sequence) \
    FIELD(NetVssWarningState, rear_activation_age_ms) \
    SIZE(NetVssStatus) \
    FIELD(NetVssStatus, vss_state) \
    FIELD(NetVssStatus, vss_availability) \
    FIELD(NetVssStatus, vss_accepting_events) \
    FIELD(NetVssStatus, vss_fault_active) \
    FIELD(NetVssStatus, fault_mask) \
    FIELD(NetVssStatus, vss_last_fault) \
    FIELD(NetVssStatus, last_fault_occurrence) \
    FIELD(NetVssStatus, last_fault_age_ms) \
    FIELD(NetVssStatus, last_fault_quality) \
    FIELD(NetVssStatus, update_sequence) \
    FIELD(NetVssStatus, source_age_ms) \
    VALUE(NET_SUBMIT_OK) \
    VALUE(NET_SUBMIT_INVALID_ARGUMENT) \
    VALUE(NET_SUBMIT_NOT_READY) \
    VALUE(NET_SUBMIT_NO_CAPACITY) \
    VALUE(NET_QUALITY_OK) \
    VALUE(NET_QUALITY_STALE) \
    VALUE(NET_QUALITY_INVALID) \
    VALUE(NET_QUALITY_NO_DATA) \
    VALUE(NET_QUALITY_REASON_NONE) \
    VALUE(NET_QUALITY_REASON_NOT_READY) \
    VALUE(NET_QUALITY_REASON_OUT_OF_RANGE) \
    VALUE(NET_QUALITY_REASON_SENSOR_FAULT) \
    VALUE(NET_QUALITY_REASON_VISION_FAULT) \
    VALUE(NET_QUALITY_REASON_STALE) \
    VALUE(NET_QUALITY_REASON_NO_DATA) \
    VALUE(NET_QUALITY_REASON_COMMUNICATION_FAULT) \
    VALUE(NET_QUALITY_REASON_RECOVERING) \
    VALUE(NET_QUALITY_REASON_DATA_INVALID) \
    VALUE(NET_AVAILABILITY_AVAILABLE) \
    VALUE(NET_AVAILABILITY_LIMITED) \
    VALUE(NET_AVAILABILITY_UNAVAILABLE) \
    VALUE(NET_FLAG_NO) \
    VALUE(NET_FLAG_YES) \
    VALUE(NET_VSS_STATE_STARTUP) \
    VALUE(NET_VSS_STATE_READY) \
    VALUE(NET_VSS_STATE_PLAYING) \
    VALUE(NET_VSS_STATE_FAULT) \
    VALUE(NET_VSS_AVAILABILITY_FULL) \
    VALUE(NET_VSS_AVAILABILITY_DEGRADED) \
    VALUE(NET_VSS_AVAILABILITY_UNAVAILABLE) \
    VALUE(NET_VSS_EVENT_WELCOME) \
    VALUE(NET_VSS_EVENT_GOODBYE) \
    VALUE(NET_VSS_EVENT_LOCK_COMPLETE) \
    VALUE(NET_VSS_EVENT_UNLOCK_COMPLETE) \
    VALUE(NET_VSS_EVENT_LOCK_ERROR) \
    VALUE(NET_REAR_WARNING_CLEAR) \
    VALUE(NET_REAR_WARNING_CAUTION) \
    VALUE(NET_REAR_WARNING_EMERGENCY) \
    VALUE(NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE) \
    VALUE(NET_VSS_FAULT_PLAYBACK_START_FAILURE) \
    VALUE(NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE) \
    VALUE(NET_VSS_FAULT_PLAYBACK_STATE_FAILURE) \
    VALUE(NET_VSS_FAULT_INITIALIZATION_FAILURE) \
    VALUE(NET_VSS_FAULT_CODE_NONE) \
    VALUE(NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE) \
    VALUE(NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE) \
    VALUE(NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE) \
    VALUE(NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE) \
    VALUE(NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE) \
    VALUE(NET_AGE_SATURATED_MS)

/* Upper bound of the entries (a field counts twice). */
#define VSS_INTEG_LAYOUT_MAX 256u

#endif
