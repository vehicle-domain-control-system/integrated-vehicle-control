#ifndef NET_DOMAIN_ENUM_INTERNAL_H
#define NET_DOMAIN_ENUM_INTERNAL_H
#include "network/net_domain_c2.h"
/* Explicit named mappings from pinned interface/04_COMMON/30_ENUM_WIRE_MAPPING.md. */
static inline bool nd_to_DataQuality_t(DataQuality_t in, uint8_t *out) { switch(in) {
case DATA_QUALITY_OK: *out=0u; return true;
case DATA_QUALITY_STALE: *out=1u; return true;
case DATA_QUALITY_INVALID: *out=2u; return true;
case DATA_QUALITY_NO_DATA: *out=3u; return true;
default: return false; } }
static inline bool nd_from_DataQuality_t(uint8_t in, DataQuality_t *out) { switch(in) {
case 0u: *out=DATA_QUALITY_OK; return true;
case 1u: *out=DATA_QUALITY_STALE; return true;
case 2u: *out=DATA_QUALITY_INVALID; return true;
case 3u: *out=DATA_QUALITY_NO_DATA; return true;
default: return false; } }
static inline bool nd_to_ValueValidity_t(ValueValidity_t in, uint8_t *out) { switch(in) {
case VALUE_VALIDITY_INVALID: *out=0u; return true;
case VALUE_VALIDITY_VALID: *out=1u; return true;
default: return false; } }
static inline bool nd_from_ValueValidity_t(uint8_t in, ValueValidity_t *out) { switch(in) {
case 0u: *out=VALUE_VALIDITY_INVALID; return true;
case 1u: *out=VALUE_VALIDITY_VALID; return true;
default: return false; } }
static inline bool nd_to_QualityReason_t(QualityReason_t in, uint8_t *out) { switch(in) {
case QUALITY_REASON_NONE: *out=0u; return true;
case QUALITY_REASON_NOT_READY: *out=1u; return true;
case QUALITY_REASON_OUT_OF_RANGE: *out=2u; return true;
case QUALITY_REASON_SENSOR_FAULT: *out=3u; return true;
case QUALITY_REASON_VISION_FAULT: *out=4u; return true;
case QUALITY_REASON_STALE: *out=5u; return true;
case QUALITY_REASON_NO_DATA: *out=6u; return true;
case QUALITY_REASON_COMMUNICATION_FAULT: *out=7u; return true;
case QUALITY_REASON_RECOVERING: *out=8u; return true;
case QUALITY_REASON_DATA_INVALID: *out=9u; return true;
default: return false; } }
static inline bool nd_from_QualityReason_t(uint8_t in, QualityReason_t *out) { switch(in) {
case 0u: *out=QUALITY_REASON_NONE; return true;
case 1u: *out=QUALITY_REASON_NOT_READY; return true;
case 2u: *out=QUALITY_REASON_OUT_OF_RANGE; return true;
case 3u: *out=QUALITY_REASON_SENSOR_FAULT; return true;
case 4u: *out=QUALITY_REASON_VISION_FAULT; return true;
case 5u: *out=QUALITY_REASON_STALE; return true;
case 6u: *out=QUALITY_REASON_NO_DATA; return true;
case 7u: *out=QUALITY_REASON_COMMUNICATION_FAULT; return true;
case 8u: *out=QUALITY_REASON_RECOVERING; return true;
case 9u: *out=QUALITY_REASON_DATA_INVALID; return true;
default: return false; } }
static inline bool nd_to_FunctionAvailability_t(FunctionAvailability_t in, uint8_t *out) { switch(in) {
case FUNCTION_AVAILABILITY_AVAILABLE: *out=0u; return true;
case FUNCTION_AVAILABILITY_DEGRADED: *out=1u; return true;
case FUNCTION_AVAILABILITY_UNAVAILABLE: *out=2u; return true;
default: return false; } }
static inline bool nd_from_FunctionAvailability_t(uint8_t in, FunctionAvailability_t *out) { switch(in) {
case 0u: *out=FUNCTION_AVAILABILITY_AVAILABLE; return true;
case 1u: *out=FUNCTION_AVAILABILITY_DEGRADED; return true;
case 2u: *out=FUNCTION_AVAILABILITY_UNAVAILABLE; return true;
default: return false; } }
static inline bool nd_to_RequestResult_t(RequestResult_t in, uint8_t *out) { switch(in) {
case REQUEST_RESULT_ACCEPTED: *out=0u; return true;
case REQUEST_RESULT_IN_PROGRESS: *out=1u; return true;
case REQUEST_RESULT_DONE: *out=2u; return true;
case REQUEST_RESULT_REJECTED: *out=3u; return true;
case REQUEST_RESULT_CANCELLED: *out=4u; return true;
case REQUEST_RESULT_FAILED: *out=5u; return true;
default: return false; } }
static inline bool nd_from_RequestResult_t(uint8_t in, RequestResult_t *out) { switch(in) {
case 0u: *out=REQUEST_RESULT_ACCEPTED; return true;
case 1u: *out=REQUEST_RESULT_IN_PROGRESS; return true;
case 2u: *out=REQUEST_RESULT_DONE; return true;
case 3u: *out=REQUEST_RESULT_REJECTED; return true;
case 4u: *out=REQUEST_RESULT_CANCELLED; return true;
case 5u: *out=REQUEST_RESULT_FAILED; return true;
default: return false; } }
static inline bool nd_to_ResultConfirmation_t(ResultConfirmation_t in, uint8_t *out) { switch(in) {
case RESULT_CONFIRMATION_UNCONFIRMED: *out=0u; return true;
case RESULT_CONFIRMATION_CONFIRMED: *out=1u; return true;
default: return false; } }
static inline bool nd_from_ResultConfirmation_t(uint8_t in, ResultConfirmation_t *out) { switch(in) {
case 0u: *out=RESULT_CONFIRMATION_UNCONFIRMED; return true;
case 1u: *out=RESULT_CONFIRMATION_CONFIRMED; return true;
default: return false; } }
static inline bool nd_to_ResultReason_t(ResultReason_t in, uint8_t *out) { switch(in) {
case RESULT_REASON_NONE: *out=0u; return true;
case RESULT_REASON_LOCAL_OVERRIDE: *out=10u; return true;
case RESULT_REASON_STOP_REQUESTED: *out=11u; return true;
case RESULT_REASON_SUPERSEDED: *out=12u; return true;
case RESULT_REASON_ANTIPINCH: *out=13u; return true;
case RESULT_REASON_INVALID_COMMAND: *out=6u; return true;
case RESULT_REASON_STATE_UNTRUSTED: *out=5u; return true;
case RESULT_REASON_NO_FEEDBACK: *out=7u; return true;
case RESULT_REASON_DRIVE_LIMIT_EXCEEDED: *out=8u; return true;
case RESULT_REASON_NOT_REGISTERED: *out=1u; return true;
case RESULT_REASON_LINK_LOST: *out=2u; return true;
case RESULT_REASON_SESSION_INVALID: *out=3u; return true;
case RESULT_REASON_DOOR_OPEN: *out=4u; return true;
case RESULT_REASON_ALREADY_AT_TARGET: *out=9u; return true;
case RESULT_REASON_FAULT: *out=14u; return true;
case RESULT_REASON_STALE: *out=15u; return true;
case RESULT_REASON_NO_DATA: *out=16u; return true;
case RESULT_REASON_NOT_READY: *out=17u; return true;
case RESULT_REASON_OUT_OF_RANGE: *out=18u; return true;
case RESULT_REASON_SOURCE_FAILURE: *out=19u; return true;
case RESULT_REASON_UNKNOWN_REQUEST: *out=20u; return true;
case RESULT_REASON_EXPIRED: *out=21u; return true;
case RESULT_REASON_ID_CONFLICT: *out=22u; return true;
case RESULT_REASON_POWER_NOT_ALLOWED: *out=23u; return true;
case RESULT_REASON_OVERHEAT: *out=24u; return true;
case RESULT_REASON_FAN_MISMATCH: *out=25u; return true;
default: return false; } }
static inline bool nd_from_ResultReason_t(uint8_t in, ResultReason_t *out) { switch(in) {
case 0u: *out=RESULT_REASON_NONE; return true;
case 10u: *out=RESULT_REASON_LOCAL_OVERRIDE; return true;
case 11u: *out=RESULT_REASON_STOP_REQUESTED; return true;
case 12u: *out=RESULT_REASON_SUPERSEDED; return true;
case 13u: *out=RESULT_REASON_ANTIPINCH; return true;
case 6u: *out=RESULT_REASON_INVALID_COMMAND; return true;
case 5u: *out=RESULT_REASON_STATE_UNTRUSTED; return true;
case 7u: *out=RESULT_REASON_NO_FEEDBACK; return true;
case 8u: *out=RESULT_REASON_DRIVE_LIMIT_EXCEEDED; return true;
case 1u: *out=RESULT_REASON_NOT_REGISTERED; return true;
case 2u: *out=RESULT_REASON_LINK_LOST; return true;
case 3u: *out=RESULT_REASON_SESSION_INVALID; return true;
case 4u: *out=RESULT_REASON_DOOR_OPEN; return true;
case 9u: *out=RESULT_REASON_ALREADY_AT_TARGET; return true;
case 14u: *out=RESULT_REASON_FAULT; return true;
case 15u: *out=RESULT_REASON_STALE; return true;
case 16u: *out=RESULT_REASON_NO_DATA; return true;
case 17u: *out=RESULT_REASON_NOT_READY; return true;
case 18u: *out=RESULT_REASON_OUT_OF_RANGE; return true;
case 19u: *out=RESULT_REASON_SOURCE_FAILURE; return true;
case 20u: *out=RESULT_REASON_UNKNOWN_REQUEST; return true;
case 21u: *out=RESULT_REASON_EXPIRED; return true;
case 22u: *out=RESULT_REASON_ID_CONFLICT; return true;
case 23u: *out=RESULT_REASON_POWER_NOT_ALLOWED; return true;
case 24u: *out=RESULT_REASON_OVERHEAT; return true;
case 25u: *out=RESULT_REASON_FAN_MISMATCH; return true;
default: return false; } }
static inline bool nd_to_FaultCategory_t(FaultCategory_t in, uint8_t *out) { switch(in) {
case FAULT_CATEGORY_SENSOR: *out=1u; return true;
case FAULT_CATEGORY_COMM: *out=3u; return true;
case FAULT_CATEGORY_FUNCTION: *out=2u; return true;
default: return false; } }
static inline bool nd_from_FaultCategory_t(uint8_t in, FaultCategory_t *out) { switch(in) {
case 1u: *out=FAULT_CATEGORY_SENSOR; return true;
case 3u: *out=FAULT_CATEGORY_COMM; return true;
case 2u: *out=FAULT_CATEGORY_FUNCTION; return true;
default: return false; } }
static inline bool nd_to_FaultState_t(FaultState_t in, uint8_t *out) { switch(in) {
case FAULT_STATE_INACTIVE: *out=0u; return true;
case FAULT_STATE_ACTIVE: *out=1u; return true;
case FAULT_STATE_RECOVERING: *out=2u; return true;
default: return false; } }
static inline bool nd_from_FaultState_t(uint8_t in, FaultState_t *out) { switch(in) {
case 0u: *out=FAULT_STATE_INACTIVE; return true;
case 1u: *out=FAULT_STATE_ACTIVE; return true;
case 2u: *out=FAULT_STATE_RECOVERING; return true;
default: return false; } }
static inline bool nd_to_CommonEcuState_t(CommonEcuState_t in, uint8_t *out) { switch(in) {
case ECU_STATE_INIT: *out=0u; return true;
case ECU_STATE_READY: *out=1u; return true;
case ECU_STATE_DEGRADED: *out=2u; return true;
case ECU_STATE_FAULT: *out=3u; return true;
default: return false; } }
static inline bool nd_from_CommonEcuState_t(uint8_t in, CommonEcuState_t *out) { switch(in) {
case 0u: *out=ECU_STATE_INIT; return true;
case 1u: *out=ECU_STATE_READY; return true;
case 2u: *out=ECU_STATE_DEGRADED; return true;
case 3u: *out=ECU_STATE_FAULT; return true;
default: return false; } }
static inline bool nd_to_DoorLockState_t(DoorLockState_t in, uint8_t *out) { switch(in) {
case DOOR_LOCK_STATE_UNKNOWN: *out=255u; return true;
case DOOR_LOCK_STATE_LOCKED: *out=0u; return true;
case DOOR_LOCK_STATE_UNLOCKED: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DoorLockState_t(uint8_t in, DoorLockState_t *out) { switch(in) {
case 255u: *out=DOOR_LOCK_STATE_UNKNOWN; return true;
case 0u: *out=DOOR_LOCK_STATE_LOCKED; return true;
case 1u: *out=DOOR_LOCK_STATE_UNLOCKED; return true;
default: return false; } }
static inline bool nd_to_DoorOpenState_t(DoorOpenState_t in, uint8_t *out) { switch(in) {
case DOOR_OPEN_STATE_UNKNOWN: *out=255u; return true;
case DOOR_OPEN_STATE_CLOSED: *out=0u; return true;
case DOOR_OPEN_STATE_OPEN: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DoorOpenState_t(uint8_t in, DoorOpenState_t *out) { switch(in) {
case 255u: *out=DOOR_OPEN_STATE_UNKNOWN; return true;
case 0u: *out=DOOR_OPEN_STATE_CLOSED; return true;
case 1u: *out=DOOR_OPEN_STATE_OPEN; return true;
default: return false; } }
static inline bool nd_to_DoorCompositeState_t(DoorCompositeState_t in, uint8_t *out) { switch(in) {
case DOOR_COMPOSITE_STATE_NORMAL: *out=0u; return true;
case DOOR_COMPOSITE_STATE_INCONSISTENT: *out=1u; return true;
case DOOR_COMPOSITE_STATE_UNTRUSTED: *out=2u; return true;
default: return false; } }
static inline bool nd_from_DoorCompositeState_t(uint8_t in, DoorCompositeState_t *out) { switch(in) {
case 0u: *out=DOOR_COMPOSITE_STATE_NORMAL; return true;
case 1u: *out=DOOR_COMPOSITE_STATE_INCONSISTENT; return true;
case 2u: *out=DOOR_COMPOSITE_STATE_UNTRUSTED; return true;
default: return false; } }
static inline bool nd_to_DoorLockTarget_t(DoorLockTarget_t in, uint8_t *out) { switch(in) {
case DOOR_TARGET_LOCK: *out=0u; return true;
case DOOR_TARGET_UNLOCK: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DoorLockTarget_t(uint8_t in, DoorLockTarget_t *out) { switch(in) {
case 0u: *out=DOOR_TARGET_LOCK; return true;
case 1u: *out=DOOR_TARGET_UNLOCK; return true;
default: return false; } }
static inline bool nd_to_FanLevel_t(FanLevel_t in, uint8_t *out) { switch(in) {
case FAN_LEVEL_OFF: *out=0u; return true;
case FAN_LEVEL_LOW: *out=1u; return true;
case FAN_LEVEL_MEDIUM: *out=2u; return true;
case FAN_LEVEL_HIGH: *out=3u; return true;
case FAN_LEVEL_UNKNOWN: *out=255u; return true;
default: return false; } }
static inline bool nd_from_FanLevel_t(uint8_t in, FanLevel_t *out) { switch(in) {
case 0u: *out=FAN_LEVEL_OFF; return true;
case 1u: *out=FAN_LEVEL_LOW; return true;
case 2u: *out=FAN_LEVEL_MEDIUM; return true;
case 3u: *out=FAN_LEVEL_HIGH; return true;
case 255u: *out=FAN_LEVEL_UNKNOWN; return true;
default: return false; } }
static inline bool nd_to_ThermalDirection_t(ThermalDirection_t in, uint8_t *out) { switch(in) {
case THERMAL_DIRECTION_IDLE: *out=0u; return true;
case THERMAL_DIRECTION_COOL: *out=1u; return true;
case THERMAL_DIRECTION_HEAT: *out=2u; return true;
default: return false; } }
static inline bool nd_from_ThermalDirection_t(uint8_t in, ThermalDirection_t *out) { switch(in) {
case 0u: *out=THERMAL_DIRECTION_IDLE; return true;
case 1u: *out=THERMAL_DIRECTION_COOL; return true;
case 2u: *out=THERMAL_DIRECTION_HEAT; return true;
default: return false; } }
static inline bool nd_to_HeatRemovalState_t(HeatRemovalState_t in, uint8_t *out) { switch(in) {
case HEAT_REMOVAL_STATE_NORMAL: *out=0u; return true;
case HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT: *out=1u; return true;
case HEAT_REMOVAL_STATE_UNMEASURABLE: *out=255u; return true;
default: return false; } }
static inline bool nd_from_HeatRemovalState_t(uint8_t in, HeatRemovalState_t *out) { switch(in) {
case 0u: *out=HEAT_REMOVAL_STATE_NORMAL; return true;
case 1u: *out=HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT; return true;
case 255u: *out=HEAT_REMOVAL_STATE_UNMEASURABLE; return true;
default: return false; } }
static inline bool nd_to_InteriorLightType_t(InteriorLightType_t in, uint8_t *out) { switch(in) {
case INTERIOR_LIGHT_TYPE_NORMAL: *out=0u; return true;
case INTERIOR_LIGHT_TYPE_GOODBYE: *out=1u; return true;
case INTERIOR_LIGHT_TYPE_WARNING: *out=2u; return true;
case INTERIOR_LIGHT_TYPE_FAULT: *out=3u; return true;
default: return false; } }
static inline bool nd_from_InteriorLightType_t(uint8_t in, InteriorLightType_t *out) { switch(in) {
case 0u: *out=INTERIOR_LIGHT_TYPE_NORMAL; return true;
case 1u: *out=INTERIOR_LIGHT_TYPE_GOODBYE; return true;
case 2u: *out=INTERIOR_LIGHT_TYPE_WARNING; return true;
case 3u: *out=INTERIOR_LIGHT_TYPE_FAULT; return true;
default: return false; } }
static inline bool nd_to_InteriorLightApplyState_t(InteriorLightApplyState_t in, uint8_t *out) { switch(in) {
case INTERIOR_LIGHT_APPLY_UNKNOWN: *out=255u; return true;
case INTERIOR_LIGHT_APPLY_APPLIED: *out=0u; return true;
case INTERIOR_LIGHT_APPLY_FAILED: *out=1u; return true;
default: return false; } }
static inline bool nd_from_InteriorLightApplyState_t(uint8_t in, InteriorLightApplyState_t *out) { switch(in) {
case 255u: *out=INTERIOR_LIGHT_APPLY_UNKNOWN; return true;
case 0u: *out=INTERIOR_LIGHT_APPLY_APPLIED; return true;
case 1u: *out=INTERIOR_LIGHT_APPLY_FAILED; return true;
default: return false; } }
static inline bool nd_to_DeviceRegistrationState_t(DeviceRegistrationState_t in, uint8_t *out) { switch(in) {
case DEVICE_REGISTRATION_UNKNOWN: *out=255u; return true;
case DEVICE_REGISTRATION_NOT_REGISTERED: *out=0u; return true;
case DEVICE_REGISTRATION_REGISTERED: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DeviceRegistrationState_t(uint8_t in, DeviceRegistrationState_t *out) { switch(in) {
case 255u: *out=DEVICE_REGISTRATION_UNKNOWN; return true;
case 0u: *out=DEVICE_REGISTRATION_NOT_REGISTERED; return true;
case 1u: *out=DEVICE_REGISTRATION_REGISTERED; return true;
default: return false; } }
static inline bool nd_to_BtConnectionState_t(BtConnectionState_t in, uint8_t *out) { switch(in) {
case BT_CONNECTION_UNKNOWN: *out=255u; return true;
case BT_CONNECTION_DISCONNECTED: *out=0u; return true;
case BT_CONNECTION_CONNECTED: *out=1u; return true;
default: return false; } }
static inline bool nd_from_BtConnectionState_t(uint8_t in, BtConnectionState_t *out) { switch(in) {
case 255u: *out=BT_CONNECTION_UNKNOWN; return true;
case 0u: *out=BT_CONNECTION_DISCONNECTED; return true;
case 1u: *out=BT_CONNECTION_CONNECTED; return true;
default: return false; } }
static inline bool nd_to_AppActiveState_t(AppActiveState_t in, uint8_t *out) { switch(in) {
case APP_ACTIVE_UNKNOWN: *out=255u; return true;
case APP_ACTIVE_INACTIVE: *out=0u; return true;
case APP_ACTIVE_ACTIVE: *out=1u; return true;
default: return false; } }
static inline bool nd_from_AppActiveState_t(uint8_t in, AppActiveState_t *out) { switch(in) {
case 255u: *out=APP_ACTIVE_UNKNOWN; return true;
case 0u: *out=APP_ACTIVE_INACTIVE; return true;
case 1u: *out=APP_ACTIVE_ACTIVE; return true;
default: return false; } }
static inline bool nd_to_ProximityState_t(ProximityState_t in, uint8_t *out) { switch(in) {
case PROXIMITY_UNKNOWN: *out=255u; return true;
case PROXIMITY_FAR: *out=0u; return true;
case PROXIMITY_NEAR: *out=1u; return true;
default: return false; } }
static inline bool nd_from_ProximityState_t(uint8_t in, ProximityState_t *out) { switch(in) {
case 255u: *out=PROXIMITY_UNKNOWN; return true;
case 0u: *out=PROXIMITY_FAR; return true;
case 1u: *out=PROXIMITY_NEAR; return true;
default: return false; } }
static inline bool nd_to_DigitalKeySetting_t(DigitalKeySetting_t in, uint8_t *out) { switch(in) {
case DIGITAL_KEY_SETTING_OFF: *out=0u; return true;
case DIGITAL_KEY_SETTING_ON: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DigitalKeySetting_t(uint8_t in, DigitalKeySetting_t *out) { switch(in) {
case 0u: *out=DIGITAL_KEY_SETTING_OFF; return true;
case 1u: *out=DIGITAL_KEY_SETTING_ON; return true;
default: return false; } }
static inline bool nd_to_UnlockOrigin_t(UnlockOrigin_t in, uint8_t *out) { switch(in) {
case UNLOCK_ORIGIN_USER_REQUEST: *out=1u; return true;
case UNLOCK_ORIGIN_PROXIMITY_AUTO: *out=2u; return true;
default: return false; } }
static inline bool nd_from_UnlockOrigin_t(uint8_t in, UnlockOrigin_t *out) { switch(in) {
case 1u: *out=UNLOCK_ORIGIN_USER_REQUEST; return true;
case 2u: *out=UNLOCK_ORIGIN_PROXIMITY_AUTO; return true;
default: return false; } }
static inline bool nd_to_RearMeasurementState_t(RearMeasurementState_t in, uint8_t *out) { switch(in) {
case REAR_MEASUREMENT_VALID_DISTANCE: *out=0u; return true;
case REAR_MEASUREMENT_NO_OBJECT: *out=1u; return true;
case REAR_MEASUREMENT_UNAVAILABLE: *out=2u; return true;
case REAR_MEASUREMENT_FAULT: *out=3u; return true;
case REAR_MEASUREMENT_RECOVERING: *out=4u; return true;
case REAR_MEASUREMENT_INACTIVE: *out=5u; return true;
default: return false; } }
static inline bool nd_from_RearMeasurementState_t(uint8_t in, RearMeasurementState_t *out) { switch(in) {
case 0u: *out=REAR_MEASUREMENT_VALID_DISTANCE; return true;
case 1u: *out=REAR_MEASUREMENT_NO_OBJECT; return true;
case 2u: *out=REAR_MEASUREMENT_UNAVAILABLE; return true;
case 3u: *out=REAR_MEASUREMENT_FAULT; return true;
case 4u: *out=REAR_MEASUREMENT_RECOVERING; return true;
case 5u: *out=REAR_MEASUREMENT_INACTIVE; return true;
default: return false; } }
static inline bool nd_to_CisState_t(CisState_t in, uint8_t *out) { switch(in) {
case CIS_STATE_STARTUP: *out=0u; return true;
case CIS_STATE_READY: *out=1u; return true;
case CIS_STATE_ACTIVE: *out=2u; return true;
case CIS_STATE_FAULT: *out=3u; return true;
default: return false; } }
static inline bool nd_from_CisState_t(uint8_t in, CisState_t *out) { switch(in) {
case 0u: *out=CIS_STATE_STARTUP; return true;
case 1u: *out=CIS_STATE_READY; return true;
case 2u: *out=CIS_STATE_ACTIVE; return true;
case 3u: *out=CIS_STATE_FAULT; return true;
default: return false; } }
static inline bool nd_to_CisFunctionStatus_t(CisFunctionStatus_t in, uint8_t *out) { switch(in) {
case CIS_FUNCTION_STATUS_NOT_READY: *out=0u; return true;
case CIS_FUNCTION_STATUS_READY: *out=1u; return true;
case CIS_FUNCTION_STATUS_ACTIVE: *out=2u; return true;
case CIS_FUNCTION_STATUS_FAULT: *out=3u; return true;
case CIS_FUNCTION_STATUS_RECOVERING: *out=4u; return true;
default: return false; } }
static inline bool nd_from_CisFunctionStatus_t(uint8_t in, CisFunctionStatus_t *out) { switch(in) {
case 0u: *out=CIS_FUNCTION_STATUS_NOT_READY; return true;
case 1u: *out=CIS_FUNCTION_STATUS_READY; return true;
case 2u: *out=CIS_FUNCTION_STATUS_ACTIVE; return true;
case 3u: *out=CIS_FUNCTION_STATUS_FAULT; return true;
case 4u: *out=CIS_FUNCTION_STATUS_RECOVERING; return true;
default: return false; } }
static inline bool nd_to_WindowCommand_t(WindowCommand_t in, uint8_t *out) { switch(in) {
case WINDOW_COMMAND_OPEN: *out=1u; return true;
case WINDOW_COMMAND_CLOSE: *out=2u; return true;
case WINDOW_COMMAND_STOP: *out=3u; return true;
case WINDOW_COMMAND_VENT: *out=4u; return true;
case WINDOW_COMMAND_MOVE_TO_POSITION: *out=5u; return true;
default: return false; } }
static inline bool nd_from_WindowCommand_t(uint8_t in, WindowCommand_t *out) { switch(in) {
case 1u: *out=WINDOW_COMMAND_OPEN; return true;
case 2u: *out=WINDOW_COMMAND_CLOSE; return true;
case 3u: *out=WINDOW_COMMAND_STOP; return true;
case 4u: *out=WINDOW_COMMAND_VENT; return true;
case 5u: *out=WINDOW_COMMAND_MOVE_TO_POSITION; return true;
default: return false; } }
static inline bool nd_to_WindowMotionState_t(WindowMotionState_t in, uint8_t *out) { switch(in) {
case WINDOW_MOTION_UNKNOWN: *out=255u; return true;
case WINDOW_MOTION_STOPPED: *out=0u; return true;
case WINDOW_MOTION_OPENING: *out=1u; return true;
case WINDOW_MOTION_CLOSING: *out=2u; return true;
case WINDOW_MOTION_ANTIPINCH_REVERSING: *out=3u; return true;
default: return false; } }
static inline bool nd_from_WindowMotionState_t(uint8_t in, WindowMotionState_t *out) { switch(in) {
case 255u: *out=WINDOW_MOTION_UNKNOWN; return true;
case 0u: *out=WINDOW_MOTION_STOPPED; return true;
case 1u: *out=WINDOW_MOTION_OPENING; return true;
case 2u: *out=WINDOW_MOTION_CLOSING; return true;
case 3u: *out=WINDOW_MOTION_ANTIPINCH_REVERSING; return true;
default: return false; } }
static inline bool nd_to_AntiPinchState_t(AntiPinchState_t in, uint8_t *out) { switch(in) {
case ANTIPINCH_STATE_CLEAR: *out=0u; return true;
case ANTIPINCH_STATE_ACTIVE: *out=1u; return true;
default: return false; } }
static inline bool nd_from_AntiPinchState_t(uint8_t in, AntiPinchState_t *out) { switch(in) {
case 0u: *out=ANTIPINCH_STATE_CLEAR; return true;
case 1u: *out=ANTIPINCH_STATE_ACTIVE; return true;
default: return false; } }
static inline bool nd_to_VssState_t(VssState_t in, uint8_t *out) { switch(in) {
case VSS_STATE_STARTUP: *out=0u; return true;
case VSS_STATE_READY: *out=1u; return true;
case VSS_STATE_PLAYING: *out=2u; return true;
case VSS_STATE_FAULT: *out=3u; return true;
default: return false; } }
static inline bool nd_from_VssState_t(uint8_t in, VssState_t *out) { switch(in) {
case 0u: *out=VSS_STATE_STARTUP; return true;
case 1u: *out=VSS_STATE_READY; return true;
case 2u: *out=VSS_STATE_PLAYING; return true;
case 3u: *out=VSS_STATE_FAULT; return true;
default: return false; } }
static inline bool nd_to_VssAvailability_t(VssAvailability_t in, uint8_t *out) { switch(in) {
case VSS_AVAILABILITY_FULL: *out=0u; return true;
case VSS_AVAILABILITY_DEGRADED: *out=1u; return true;
case VSS_AVAILABILITY_UNAVAILABLE: *out=2u; return true;
default: return false; } }
static inline bool nd_from_VssAvailability_t(uint8_t in, VssAvailability_t *out) { switch(in) {
case 0u: *out=VSS_AVAILABILITY_FULL; return true;
case 1u: *out=VSS_AVAILABILITY_DEGRADED; return true;
case 2u: *out=VSS_AVAILABILITY_UNAVAILABLE; return true;
default: return false; } }
static inline bool nd_to_RearObstacleState_t(RearObstacleState_t in, uint8_t *out) { switch(in) {
case REAR_OBSTACLE_STATE_CLEAR: *out=0u; return true;
case REAR_OBSTACLE_STATE_CAUTION: *out=1u; return true;
case REAR_OBSTACLE_STATE_EMERGENCY: *out=2u; return true;
default: return false; } }
static inline bool nd_from_RearObstacleState_t(uint8_t in, RearObstacleState_t *out) { switch(in) {
case 0u: *out=REAR_OBSTACLE_STATE_CLEAR; return true;
case 1u: *out=REAR_OBSTACLE_STATE_CAUTION; return true;
case 2u: *out=REAR_OBSTACLE_STATE_EMERGENCY; return true;
default: return false; } }
static inline bool nd_to_RearDetectionActivation_t(RearDetectionActivation_t in, uint8_t *out) { switch(in) {
case REAR_DETECTION_DISABLED: *out=0u; return true;
case REAR_DETECTION_ACTIVE: *out=1u; return true;
default: return false; } }
static inline bool nd_from_RearDetectionActivation_t(uint8_t in, RearDetectionActivation_t *out) { switch(in) {
case 0u: *out=REAR_DETECTION_DISABLED; return true;
case 1u: *out=REAR_DETECTION_ACTIVE; return true;
default: return false; } }
static inline bool nd_to_VssOneShotEvent_t(VssOneShotEvent_t in, uint8_t *out) { switch(in) {
case VSS_EVENT_VEHICLE_WELCOME: *out=1u; return true;
case VSS_EVENT_VEHICLE_GOODBYE: *out=2u; return true;
case VSS_EVENT_DOOR_LOCK_COMPLETE: *out=3u; return true;
case VSS_EVENT_DOOR_UNLOCK_COMPLETE: *out=4u; return true;
case VSS_EVENT_DOOR_LOCK_ERROR: *out=5u; return true;
default: return false; } }
static inline bool nd_from_VssOneShotEvent_t(uint8_t in, VssOneShotEvent_t *out) { switch(in) {
case 1u: *out=VSS_EVENT_VEHICLE_WELCOME; return true;
case 2u: *out=VSS_EVENT_VEHICLE_GOODBYE; return true;
case 3u: *out=VSS_EVENT_DOOR_LOCK_COMPLETE; return true;
case 4u: *out=VSS_EVENT_DOOR_UNLOCK_COMPLETE; return true;
case 5u: *out=VSS_EVENT_DOOR_LOCK_ERROR; return true;
default: return false; } }
static inline bool nd_to_DomainFunctionId_t(DomainFunctionId_t in, uint8_t *out) { switch(in) {
case DOMAIN_FUNCTION_DOOR: *out=1u; return true;
case DOMAIN_FUNCTION_CLIMATE: *out=2u; return true;
case DOMAIN_FUNCTION_INTERIOR_LIGHT: *out=3u; return true;
case DOMAIN_FUNCTION_DIGITAL_KEY: *out=4u; return true;
case DOMAIN_FUNCTION_WINDOW: *out=8u; return true;
case DOMAIN_FUNCTION_REAR_WARNING: *out=7u; return true;
case DOMAIN_FUNCTION_OCCUPANT_WARNING: *out=5u; return true;
case DOMAIN_FUNCTION_VSS: *out=9u; return true;
case DOMAIN_FUNCTION_ENVIRONMENT: *out=6u; return true;
default: return false; } }
static inline bool nd_from_DomainFunctionId_t(uint8_t in, DomainFunctionId_t *out) { switch(in) {
case 1u: *out=DOMAIN_FUNCTION_DOOR; return true;
case 2u: *out=DOMAIN_FUNCTION_CLIMATE; return true;
case 3u: *out=DOMAIN_FUNCTION_INTERIOR_LIGHT; return true;
case 4u: *out=DOMAIN_FUNCTION_DIGITAL_KEY; return true;
case 8u: *out=DOMAIN_FUNCTION_WINDOW; return true;
case 7u: *out=DOMAIN_FUNCTION_REAR_WARNING; return true;
case 5u: *out=DOMAIN_FUNCTION_OCCUPANT_WARNING; return true;
case 9u: *out=DOMAIN_FUNCTION_VSS; return true;
case 6u: *out=DOMAIN_FUNCTION_ENVIRONMENT; return true;
default: return false; } }
static inline bool nd_to_DomainWarningType_t(DomainWarningType_t in, uint8_t *out) { switch(in) {
case DOMAIN_WARNING_WINDOW_ANTIPINCH: *out=3u; return true;
case DOMAIN_WARNING_OCCUPANT_HAZARD: *out=2u; return true;
case DOMAIN_WARNING_REAR_OBSTACLE: *out=1u; return true;
case DOMAIN_WARNING_DOOR_LEFT_OPEN: *out=8u; return true;
case DOMAIN_WARNING_BCM_FAULT: *out=4u; return true;
case DOMAIN_WARNING_CIS_FAULT: *out=5u; return true;
case DOMAIN_WARNING_WINDOW_FAULT: *out=6u; return true;
case DOMAIN_WARNING_VSS_FAULT: *out=7u; return true;
default: return false; } }
static inline bool nd_from_DomainWarningType_t(uint8_t in, DomainWarningType_t *out) { switch(in) {
case 3u: *out=DOMAIN_WARNING_WINDOW_ANTIPINCH; return true;
case 2u: *out=DOMAIN_WARNING_OCCUPANT_HAZARD; return true;
case 1u: *out=DOMAIN_WARNING_REAR_OBSTACLE; return true;
case 8u: *out=DOMAIN_WARNING_DOOR_LEFT_OPEN; return true;
case 4u: *out=DOMAIN_WARNING_BCM_FAULT; return true;
case 5u: *out=DOMAIN_WARNING_CIS_FAULT; return true;
case 6u: *out=DOMAIN_WARNING_WINDOW_FAULT; return true;
case 7u: *out=DOMAIN_WARNING_VSS_FAULT; return true;
default: return false; } }
static inline bool nd_to_DomainWarningSeverity_t(DomainWarningSeverity_t in, uint8_t *out) { switch(in) {
case DOMAIN_WARNING_SEVERITY_INFO: *out=0u; return true;
case DOMAIN_WARNING_SEVERITY_CAUTION: *out=1u; return true;
case DOMAIN_WARNING_SEVERITY_EMERGENCY: *out=2u; return true;
default: return false; } }
static inline bool nd_from_DomainWarningSeverity_t(uint8_t in, DomainWarningSeverity_t *out) { switch(in) {
case 0u: *out=DOMAIN_WARNING_SEVERITY_INFO; return true;
case 1u: *out=DOMAIN_WARNING_SEVERITY_CAUTION; return true;
case 2u: *out=DOMAIN_WARNING_SEVERITY_EMERGENCY; return true;
default: return false; } }
static inline bool nd_to_DomainWarningState_t(DomainWarningState_t in, uint8_t *out) { switch(in) {
case DOMAIN_WARNING_STATE_CLEAR: *out=0u; return true;
case DOMAIN_WARNING_STATE_ACTIVE: *out=1u; return true;
default: return false; } }
static inline bool nd_from_DomainWarningState_t(uint8_t in, DomainWarningState_t *out) { switch(in) {
case 0u: *out=DOMAIN_WARNING_STATE_CLEAR; return true;
case 1u: *out=DOMAIN_WARNING_STATE_ACTIVE; return true;
default: return false; } }
static inline bool nd_to_DomainIf_BcmCommandKind_t(DomainIf_BcmCommandKind_t in, uint8_t *out) { switch(in) {
case BCM_COMMAND_KIND_DOOR: *out=1u; return true;
case BCM_COMMAND_KIND_CLIMATE: *out=2u; return true;
case BCM_COMMAND_KIND_INTERIOR_LIGHT: *out=3u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_BcmCommandKind_t(uint8_t in, DomainIf_BcmCommandKind_t *out) { switch(in) {
case 1u: *out=BCM_COMMAND_KIND_DOOR; return true;
case 2u: *out=BCM_COMMAND_KIND_CLIMATE; return true;
case 3u: *out=BCM_COMMAND_KIND_INTERIOR_LIGHT; return true;
default: return false; } }
static inline bool nd_to_DomainIf_BcmDoorEvidenceType_t(DomainIf_BcmDoorEvidenceType_t in, uint8_t *out) { switch(in) {
case BCM_DOOR_EVIDENCE_LOCK_COMPLETED: *out=1u; return true;
case BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED: *out=2u; return true;
case BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET: *out=3u; return true;
case BCM_DOOR_EVIDENCE_REQUEST_REJECTED: *out=4u; return true;
case BCM_DOOR_EVIDENCE_REQUEST_FAILED: *out=5u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_BcmDoorEvidenceType_t(uint8_t in, DomainIf_BcmDoorEvidenceType_t *out) { switch(in) {
case 1u: *out=BCM_DOOR_EVIDENCE_LOCK_COMPLETED; return true;
case 2u: *out=BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED; return true;
case 3u: *out=BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET; return true;
case 4u: *out=BCM_DOOR_EVIDENCE_REQUEST_REJECTED; return true;
case 5u: *out=BCM_DOOR_EVIDENCE_REQUEST_FAILED; return true;
default: return false; } }
static inline bool nd_to_DomainIf_BcmProtectionEventType_t(DomainIf_BcmProtectionEventType_t in, uint8_t *out) { switch(in) {
case BCM_PROTECTION_OVERHEAT_DETECTED: *out=1u; return true;
case BCM_PROTECTION_FAN_MISMATCH_DETECTED: *out=2u; return true;
case BCM_PROTECTION_RECOVERY_CONFIRMED: *out=3u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_BcmProtectionEventType_t(uint8_t in, DomainIf_BcmProtectionEventType_t *out) { switch(in) {
case 1u: *out=BCM_PROTECTION_OVERHEAT_DETECTED; return true;
case 2u: *out=BCM_PROTECTION_FAN_MISMATCH_DETECTED; return true;
case 3u: *out=BCM_PROTECTION_RECOVERY_CONFIRMED; return true;
default: return false; } }
static inline bool nd_to_DomainIf_MobileResultTarget_t(DomainIf_MobileResultTarget_t in, uint8_t *out) { switch(in) {
case MOBILE_RESULT_TARGET_DOMAIN_SETTING: *out=0u; return true;
case MOBILE_RESULT_TARGET_BCM: *out=1u; return true;
case MOBILE_RESULT_TARGET_WINDOW: *out=2u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_MobileResultTarget_t(uint8_t in, DomainIf_MobileResultTarget_t *out) { switch(in) {
case 0u: *out=MOBILE_RESULT_TARGET_DOMAIN_SETTING; return true;
case 1u: *out=MOBILE_RESULT_TARGET_BCM; return true;
case 2u: *out=MOBILE_RESULT_TARGET_WINDOW; return true;
default: return false; } }
static inline bool nd_to_DomainIf_CisInterfaceStatus_t(DomainIf_CisInterfaceStatus_t in, uint8_t *out) { switch(in) {
case CIS_INTERFACE_AVAILABLE: *out=0u; return true;
case CIS_INTERFACE_UNAVAILABLE: *out=1u; return true;
case CIS_INTERFACE_RECOVERING: *out=2u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_CisInterfaceStatus_t(uint8_t in, DomainIf_CisInterfaceStatus_t *out) { switch(in) {
case 0u: *out=CIS_INTERFACE_AVAILABLE; return true;
case 1u: *out=CIS_INTERFACE_UNAVAILABLE; return true;
case 2u: *out=CIS_INTERFACE_RECOVERING; return true;
default: return false; } }
static inline bool nd_to_DomainIf_AntiPinchProtectionState_t(DomainIf_AntiPinchProtectionState_t in, uint8_t *out) { switch(in) {
case ANTIPINCH_PROTECTION_IDLE: *out=0u; return true;
case ANTIPINCH_PROTECTION_REVERSING: *out=1u; return true;
case ANTIPINCH_PROTECTION_COMPLETED: *out=2u; return true;
case ANTIPINCH_PROTECTION_ABORTED: *out=3u; return true;
default: return false; } }
static inline bool nd_from_DomainIf_AntiPinchProtectionState_t(uint8_t in, DomainIf_AntiPinchProtectionState_t *out) { switch(in) {
case 0u: *out=ANTIPINCH_PROTECTION_IDLE; return true;
case 1u: *out=ANTIPINCH_PROTECTION_REVERSING; return true;
case 2u: *out=ANTIPINCH_PROTECTION_COMPLETED; return true;
case 3u: *out=ANTIPINCH_PROTECTION_ABORTED; return true;
default: return false; } }
static inline bool nd_to_OccupantHazardState_t(OccupantHazardState_t in, uint8_t *out) { switch(in) {
case OCCUPANT_HAZARD_CLEAR: *out=0u; return true;
case OCCUPANT_HAZARD_ACTIVE: *out=1u; return true;
default: return false; } }
static inline bool nd_from_OccupantHazardState_t(uint8_t in, OccupantHazardState_t *out) { switch(in) {
case 0u: *out=OCCUPANT_HAZARD_CLEAR; return true;
case 1u: *out=OCCUPANT_HAZARD_ACTIVE; return true;
default: return false; } }
#endif
