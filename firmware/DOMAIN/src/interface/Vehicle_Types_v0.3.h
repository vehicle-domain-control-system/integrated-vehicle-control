/**
 * @file Vehicle_Types_v0.3.h
 * @brief S32K344 Domain Controller 공통 논리 데이터 타입 정의 (v0.3 후보)
 *
 * v0.3 후보 변경:
 * - ClimateState_t와 InteriorLightState_t 끝에 sourceGeneration 추가
 * - sourceGeneration은 상태 의미를 생성한 원 producer의 generation이다.
 * - 0은 원 생산 세대를 확인할 수 없음을 뜻한다. 0을 임의의 유효 세대로
 *   치환하거나, generation이 0이라는 이유만으로 품질을 변경하지 않는다.
 * - 실제 유효 관측(aggregate 또는 authoritative per-field quality가 OK)은
 *   nonzero generation을 요구한다. INVALID/NO_DATA에서는 0을 허용한다.
 * - STALE과 generation 0의 조합은 OPEN / TEAM_DECISION_REQUIRED이다.
 * - BCM BOOT_ID와의 검증/매핑은 BINDING_OPEN / TEAM_DECISION_REQUIRED이다.
 *   Transport BOOT_ID를 자동 복사하거나 Domain이 generation을 합성하지 않는다.
 * - 이 후보는 C 논리 계약만 변경하며 CAN/UART wire format을 변경하지 않는다.
 * - Network 담당자 승인 전 Production include로 전환하지 않는다.
 *
 * v0.2 변경:
 * - CIS 값을 Occupant / Cabin / Rear 그룹으로 분리
 * - CIS 각 값에 독립적인 Validity / QualityReason / Freshness 근거 부여
 * - ESP32 Registration/Connection과 Proximity의 Freshness를 분리
 * - VSS 서비스 상태에도 SignalMeta를 포함
 *
 * 이 파일은 차량 의미(Canonical Logical Types)를 정의한다.
 * CAN ID, Byte/Bit, UART Frame, CRC/E2E 배치는 포함하지 않는다.
 * C2: explicit enum/field codec only; never memcpy this C layout to wire.
 * Registration/Connection meta is one shared tuple; App appMeta and Proximity
 * have independent evidence. Producer source TTL remains OPEN (not RX 300ms).
 */

#ifndef VEHICLE_TYPES_V0_3_H
#define VEHICLE_TYPES_V0_3_H

#if defined(VEHICLE_TYPES_H)
#error "Vehicle_Types_v0.2.h and Vehicle_Types_v0.3.h must not be mixed"
#endif

/* Keep the primary include guard versioned.  The legacy guard is also claimed
 * so that a later accidental include of v0.2 cannot redefine the public types.
 */
#define VEHICLE_TYPES_H

#define VEHICLE_TYPES_VERSION_MAJOR (0U)
#define VEHICLE_TYPES_VERSION_MINOR (3U)
#define VEHICLE_TYPES_VERSION_PATCH (0U)
#define VEHICLE_TYPES_VERSION        (0x00000300UL)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 0. Common IDs
 * ============================================================================
 */

typedef uint32_t RequestId_t;
typedef uint64_t SessionId_t;
typedef uint32_t OccurrenceId_t;
typedef uint32_t UpdateSequence_t;

/* Shared semantic IDs/lineage. Transport identity is Network-private. */
typedef uint32_t DeviceContextId_t;

typedef uint32_t DomainCommandId_t;

/*
 * Original semantic state producer generation.
 *
 * 0: producer generation is unknown/unconfirmed. It is not a valid generation
 *    synthesized by Domain or Network and must never be promoted to nonzero.
 * nonzero: a producer generation explicitly preserved by the agreed binding.
 *
 * A state claiming a current valid observation (aggregate quality or any
 * authoritative per-field quality is DATA_QUALITY_OK) requires nonzero.
 * DATA_QUALITY_INVALID and DATA_QUALITY_NO_DATA may carry 0; generation does
 * not make those qualities usable. DATA_QUALITY_STALE with 0 remains OPEN.
 */
typedef uint32_t SourceGeneration_t;

typedef struct
{
    SourceGeneration_t generation;
    UpdateSequence_t sequence;
    uint32_t ageMs;
} SourceEvidence_t;

typedef enum
{
    COMMAND_ORIGIN_MOBILE_REQUEST = 0,
    COMMAND_ORIGIN_DIGITAL_KEY,
    COMMAND_ORIGIN_CLIMATE_POLICY,
    COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY,
    COMMAND_ORIGIN_AUTO_VENTILATION,
    COMMAND_ORIGIN_SYSTEM
} CommandOrigin_t;

/* ============================================================================
 * 1. Common Quality / Availability
 * ============================================================================
 */

typedef enum
{
    DATA_QUALITY_OK = 0,
    DATA_QUALITY_STALE,
    DATA_QUALITY_INVALID,
    DATA_QUALITY_NO_DATA
} DataQuality_t;

typedef enum
{
    VALUE_VALIDITY_INVALID = 0,
    VALUE_VALIDITY_VALID
} ValueValidity_t;

typedef enum
{
    QUALITY_REASON_NONE = 0,
    QUALITY_REASON_NOT_READY,
    QUALITY_REASON_OUT_OF_RANGE,
    QUALITY_REASON_SENSOR_FAULT,
    QUALITY_REASON_VISION_FAULT,
    QUALITY_REASON_STALE,
    QUALITY_REASON_NO_DATA,
    QUALITY_REASON_COMMUNICATION_FAULT,
    QUALITY_REASON_RECOVERING,
    QUALITY_REASON_DATA_INVALID
} QualityReason_t;

typedef enum
{
    FUNCTION_AVAILABILITY_AVAILABLE = 0,
    FUNCTION_AVAILABILITY_DEGRADED,
    FUNCTION_AVAILABILITY_UNAVAILABLE,
    FUNCTION_AVAILABILITY_UNSUPPORTED
} FunctionAvailability_t;

/**
 * @brief 값 또는 논리 정보의 전달 품질/최신성 근거.
 *
 * ageMs는 Domain 수신 이후 시간 그 자체라기보다
 * 원본 측정/판정의 경과 시간 의미를 보존하기 위한 논리 필드다.
 * 실제 wire encoding은 네트워크 설계에서 결정한다.
 */
typedef struct
{
    DataQuality_t quality;
    uint32_t ageMs;
    UpdateSequence_t updateSequence;
} SignalMeta_t;

/**
 * @brief CIS 등 개별 값에 사용하는 품질 묶음.
 *
 * 하나의 ECU에서 나온 여러 값이 서로 다른 품질을 가질 수 있으므로
 * ECU 전체에 하나의 validity를 두지 않고 값 단위로 사용한다.
 */
typedef struct
{
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ValueQuality_t;

/* ============================================================================
 * 2. Request / Result
 * ============================================================================
 */

typedef enum
{
    REQUEST_RESULT_ACCEPTED = 0,
    REQUEST_RESULT_IN_PROGRESS,
    REQUEST_RESULT_DONE,
    REQUEST_RESULT_REJECTED,
    REQUEST_RESULT_CANCELLED,
    REQUEST_RESULT_FAILED
} RequestResult_t;

typedef enum
{
    RESULT_CONFIRMATION_UNCONFIRMED = 0,
    RESULT_CONFIRMATION_CONFIRMED
} ResultConfirmation_t;

typedef enum
{
    RESULT_REASON_NONE = 0,
    RESULT_REASON_LOCAL_OVERRIDE,
    RESULT_REASON_STOP_REQUESTED,
    RESULT_REASON_SUPERSEDED,
    RESULT_REASON_ANTIPINCH,
    RESULT_REASON_NOT_ALLOWED,
    RESULT_REASON_INVALID_COMMAND,
    RESULT_REASON_STATE_UNTRUSTED,
    RESULT_REASON_NO_FEEDBACK,
    RESULT_REASON_DRIVE_LIMIT_EXCEEDED,
    RESULT_REASON_MOTOR_FAULT,
    RESULT_REASON_SENSOR_FAULT,
    RESULT_REASON_TIMEOUT,
    RESULT_REASON_OTHER, /* internal-only unless an explicit wire mapping exists */
    RESULT_REASON_NOT_REGISTERED,
    RESULT_REASON_LINK_LOST,
    RESULT_REASON_SESSION_INVALID,
    RESULT_REASON_DOOR_OPEN,
    RESULT_REASON_ALREADY_AT_TARGET,
    RESULT_REASON_FAULT,
    RESULT_REASON_STALE,
    RESULT_REASON_NO_DATA,
    RESULT_REASON_NOT_READY,
    RESULT_REASON_OUT_OF_RANGE,
    RESULT_REASON_SOURCE_FAILURE,
    RESULT_REASON_UNKNOWN_REQUEST,
    RESULT_REASON_EXPIRED,
    RESULT_REASON_ID_CONFLICT,
    RESULT_REASON_POWER_NOT_ALLOWED,
    RESULT_REASON_OVERHEAT,
    RESULT_REASON_FAN_MISMATCH
} ResultReason_t;

typedef struct
{
    SessionId_t sessionId;
    RequestId_t requestId;
} RequestContext_t;

typedef struct
{
    RequestContext_t context;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
} RequestResultInfo_t;

/* ============================================================================
 * 3. Fault
 * ============================================================================
 */

typedef enum
{
    FAULT_CATEGORY_SENSOR = 0,
    FAULT_CATEGORY_COMM,
    FAULT_CATEGORY_FUNCTION
} FaultCategory_t;

typedef enum
{
    FAULT_STATE_INACTIVE = 0,
    FAULT_STATE_ACTIVE,
    FAULT_STATE_RECOVERING
} FaultState_t;

typedef struct
{
    FaultCategory_t category;
    FaultState_t state;
    uint16_t faultCode;
    DataQuality_t quality;
} FaultInfo_t;

/* ============================================================================
 * 4. Common ECU state
 * ============================================================================
 */

typedef enum
{
    ECU_STATE_INIT = 0,
    ECU_STATE_READY,
    ECU_STATE_DEGRADED,
    ECU_STATE_FAULT
} CommonEcuState_t;

/* ============================================================================
 * 5. BCM - Door
 * ============================================================================
 */

typedef enum
{
    DOOR_LOCK_STATE_UNKNOWN = 0,
    DOOR_LOCK_STATE_LOCKED,
    DOOR_LOCK_STATE_UNLOCKED
} DoorLockState_t;

typedef enum
{
    DOOR_OPEN_STATE_UNKNOWN = 0,
    DOOR_OPEN_STATE_CLOSED,
    DOOR_OPEN_STATE_OPEN
} DoorOpenState_t;

typedef enum
{
    DOOR_COMPOSITE_STATE_NORMAL = 0,
    DOOR_COMPOSITE_STATE_INCONSISTENT,
    DOOR_COMPOSITE_STATE_UNTRUSTED
} DoorCompositeState_t;

typedef enum
{
    DOOR_TARGET_LOCK = 0,
    DOOR_TARGET_UNLOCK
} DoorLockTarget_t;

typedef struct
{
    DoorLockState_t lockState;
    DoorOpenState_t openState;
    DoorCompositeState_t compositeState;
    DataQuality_t lockQuality;
    DataQuality_t openQuality;
    DataQuality_t compositeQuality;
    SourceGeneration_t sourceGeneration; /* DOOR_LEFT_OPEN warning source */
    SignalMeta_t meta; /* shared age/sequence; quality is conservative summary */
} DoorState_t;

/* ============================================================================
 * 6. BCM - Climate
 * ============================================================================
 */

typedef enum
{
    FAN_LEVEL_OFF = 0,
    FAN_LEVEL_LOW,
    FAN_LEVEL_MEDIUM,
    FAN_LEVEL_HIGH,
    FAN_LEVEL_UNKNOWN
} FanLevel_t;

typedef enum
{
    THERMAL_DIRECTION_IDLE = 0,
    THERMAL_DIRECTION_COOL,
    THERMAL_DIRECTION_HEAT
} ThermalDirection_t;

typedef enum
{
    HEAT_REMOVAL_STATE_NORMAL = 0,
    HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT,
    HEAT_REMOVAL_STATE_UNMEASURABLE
} HeatRemovalState_t;

typedef struct
{
    FanLevel_t commandedFanLevel;
    FanLevel_t measuredFanLevel;
    ThermalDirection_t thermalDirection;
    uint8_t thermalOutputLevelPercent;
    HeatRemovalState_t heatRemovalState;
    DataQuality_t commandedFanQuality;
    DataQuality_t measuredFanQuality;
    DataQuality_t thermalDirectionQuality;
    DataQuality_t thermalOutputQuality;
    DataQuality_t heatRemovalQuality;
    SignalMeta_t meta; /* per-field quality is authoritative */
    /* Original semantic state producer generation. 0 is permitted when every
     * authoritative quality is INVALID/NO_DATA. If meta.quality or any
     * per-field quality is OK, this value shall be nonzero. STALE+0 is OPEN.
     * BCM BOOT_ID mapping is BINDING_OPEN; do not synthesize or derive it.
     */
    SourceGeneration_t sourceGeneration;
} ClimateState_t;

/* ============================================================================
 * 7. BCM - Interior Light
 * ============================================================================
 */

typedef enum
{
    INTERIOR_LIGHT_TYPE_NORMAL = 0,
    INTERIOR_LIGHT_TYPE_GOODBYE,
    INTERIOR_LIGHT_TYPE_WARNING,
    INTERIOR_LIGHT_TYPE_FAULT
} InteriorLightType_t;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RgbColor_t;

typedef enum
{
    INTERIOR_LIGHT_APPLY_UNKNOWN = 0,
    INTERIOR_LIGHT_APPLY_APPLIED,
    INTERIOR_LIGHT_APPLY_FAILED
} InteriorLightApplyState_t;

typedef struct
{
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
    InteriorLightApplyState_t applyState; /* UNKNOWN is not false/FAILED */
    bool physicalFeedbackSupported;
    DataQuality_t typeQuality;
    DataQuality_t levelQuality;
    DataQuality_t colorQuality;
    DataQuality_t applyQuality;
    SignalMeta_t meta;
    /* Original semantic state producer generation. 0 is permitted when every
     * authoritative quality is INVALID/NO_DATA. If meta.quality or any
     * per-field quality is OK, this value shall be nonzero. STALE+0 is OPEN.
     * BCM BOOT_ID mapping is BINDING_OPEN; do not synthesize or derive it.
     */
    SourceGeneration_t sourceGeneration;
} InteriorLightState_t;

/* ============================================================================
 * 8. ESP32 / Digital Key
 * ============================================================================
 */

typedef enum
{
    DEVICE_REGISTRATION_UNKNOWN = 0,
    DEVICE_REGISTRATION_NOT_REGISTERED,
    DEVICE_REGISTRATION_REGISTERED
} DeviceRegistrationState_t;

typedef enum
{
    BT_CONNECTION_UNKNOWN = 0,
    BT_CONNECTION_DISCONNECTED,
    BT_CONNECTION_CONNECTED
} BtConnectionState_t;

typedef enum
{
    APP_ACTIVE_UNKNOWN = 0,
    APP_ACTIVE_INACTIVE,
    APP_ACTIVE_ACTIVE
} AppActiveState_t;

typedef enum
{
    PROXIMITY_UNKNOWN = 0,
    PROXIMITY_FAR,
    PROXIMITY_NEAR
} ProximityState_t;

typedef enum
{
    DIGITAL_KEY_SETTING_OFF = 0,
    DIGITAL_KEY_SETTING_ON
} DigitalKeySetting_t;

typedef enum
{
    UNLOCK_ORIGIN_USER_REQUEST = 0,
    UNLOCK_ORIGIN_PROXIMITY_AUTO
} UnlockOrigin_t;

/**
 * @brief ESP32가 제공하는 등록/현재 연결/APP Session 문맥.
 */
typedef struct
{
    DeviceRegistrationState_t registration;
    BtConnectionState_t btConnection;
    AppActiveState_t appActive;
    SessionId_t sessionId;
    SignalMeta_t meta;    /* ONE coherent Registration/Connection quality/sequence/age */
    SignalMeta_t appMeta; /* App activity sequence/age/quality, independently */
} DigitalKeyConnectionState_t;

/**
 * @brief ESP32가 RSSI 등을 바탕으로 생성한 근접 상태.
 *
 * ESP32는 NEAR/FAR/UNKNOWN까지만 제공하고,
 * 자동 Unlock 여부는 Domain이 결정한다.
 */
typedef struct
{
    ProximityState_t state;
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ProximityInput_t;

typedef struct
{
    DigitalKeyConnectionState_t connection;
    ProximityInput_t proximity;
} DigitalKeyInput_t;

typedef struct
{
    DigitalKeySetting_t setting;
    FunctionAvailability_t availability;
    UnlockOrigin_t lastUnlockOrigin; /* Domain internal observation */
    ResultReason_t reason;
    SignalMeta_t meta;
} DigitalKeyState_t;

/* ============================================================================
 * 9. CIS
 * ============================================================================
 */

typedef enum
{
    REAR_MEASUREMENT_VALID_DISTANCE = 0,
    REAR_MEASUREMENT_NO_OBJECT,
    REAR_MEASUREMENT_UNAVAILABLE,
    REAR_MEASUREMENT_FAULT,
    REAR_MEASUREMENT_RECOVERING,
    REAR_MEASUREMENT_INACTIVE
} RearMeasurementState_t;

typedef enum
{
    CIS_STATE_STARTUP = 0,
    CIS_STATE_READY,
    CIS_STATE_ACTIVE,
    CIS_STATE_FAULT
} CisState_t;

/**
 * @brief 탑승자 존재/인원수는 같은 판정 회차의 일관된 결과로 묶는다.
 */
typedef enum
{
    CIS_FUNCTION_STATUS_NOT_READY = 0,
    CIS_FUNCTION_STATUS_READY,
    CIS_FUNCTION_STATUS_ACTIVE,
    CIS_FUNCTION_STATUS_FAULT,
    CIS_FUNCTION_STATUS_RECOVERING
} CisFunctionStatus_t;

typedef struct
{
    bool occupantPresent; /* only usable when presenceQuality says usable */
    uint8_t occupantCount;
    ValueQuality_t presenceQuality;
    ValueQuality_t countQuality;
    SourceGeneration_t sourceGeneration; /* PI_BOOT_ID: Presence/Count only */
    CisFunctionStatus_t visionFunctionStatus; /* CIS-owned, same occupant update;
        not Pi observation identity; never replaced by latest separate CIS_STATUS */
} CisOccupantState_t;

/**
 * @brief 실내 환경값은 값별로 독립적인 품질을 갖는다.
 */
typedef struct
{
    int16_t cabinTemperature; /* 0.01 deg C */
    ValueQuality_t temperatureQuality;

    uint16_t cabinHumidity; /* 0.01 percent RH */
    ValueQuality_t humidityQuality;

    uint32_t cabinIlluminance; /* 1 lx; no truncation above 65535 */
    ValueQuality_t illuminanceQuality;
} CisCabinEnvironmentState_t;

/**
 * @brief 후방 거리와 측정 상태.
 *
 * NO_OBJECT와 UNAVAILABLE은 다르다.
 * 거리 위험(CLEAR/CAUTION/EMERGENCY)은 Domain이 판단한다.
 */
typedef struct
{
    uint16_t rearDistanceMm; /* exact 0.1 cm input; no integer-cm truncation */
    RearMeasurementState_t measurementState;
    ValueQuality_t distanceQuality;
    SourceGeneration_t sourceGeneration; /* admitted original CIS lineage */
} CisRearState_t;

/**
 * @brief 전체 Snapshot 편의를 위한 CIS Aggregate.
 *
 * 각 하위 그룹의 품질/최신성은 독립적으로 유지한다.
 */
typedef struct
{
    CisOccupantState_t occupant;
    CisCabinEnvironmentState_t cabin;
    CisRearState_t rear;
} CisEnvironmentState_t;

/* ============================================================================
 * 10. WINDOW
 * ============================================================================
 */

typedef enum
{
    WINDOW_COMMAND_OPEN = 0,
    WINDOW_COMMAND_CLOSE,
    WINDOW_COMMAND_STOP,
    WINDOW_COMMAND_VENT,
    WINDOW_COMMAND_MOVE_TO_POSITION
} WindowCommand_t;

typedef enum
{
    WINDOW_MOTION_UNKNOWN = 0,
    WINDOW_MOTION_STOPPED,
    WINDOW_MOTION_OPENING,
    WINDOW_MOTION_CLOSING,
    WINDOW_MOTION_ANTIPINCH_REVERSING
} WindowMotionState_t;

typedef enum
{
    ANTIPINCH_STATE_CLEAR = 0,
    ANTIPINCH_STATE_ACTIVE
} AntiPinchState_t;

typedef struct
{
    uint8_t channel;
    WindowMotionState_t motionState;
    CommonEcuState_t ecuState;
    uint8_t positionPercent; /* 0%=Fully Open, 100%=Fully Closed */
    bool fullyOpen;
    bool fullyClosed;
    AntiPinchState_t antiPinchState;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration; /* anti-pinch lineage; implementation deferred */
} WindowState_t;

typedef struct
{
    RequestContext_t context;
    uint8_t channel;
    WindowCommand_t command;
    uint8_t targetPositionPercent;
    DataQuality_t validity;
} WindowCommandRequest_t;

/* ============================================================================
 * 11. VSS
 * ============================================================================
 */

typedef enum
{
    VSS_STATE_STARTUP = 0,
    VSS_STATE_READY,
    VSS_STATE_PLAYING,
    VSS_STATE_FAULT
} VssState_t;

typedef enum
{
    VSS_AVAILABILITY_FULL = 0,
    VSS_AVAILABILITY_DEGRADED,
    VSS_AVAILABILITY_UNAVAILABLE
} VssAvailability_t;

typedef enum
{
    REAR_OBSTACLE_STATE_CLEAR = 0,
    REAR_OBSTACLE_STATE_CAUTION,
    REAR_OBSTACLE_STATE_EMERGENCY
} RearObstacleState_t;

typedef enum
{
    REAR_DETECTION_DISABLED = 0,
    REAR_DETECTION_ACTIVE
} RearDetectionActivation_t;

typedef enum
{
    VSS_EVENT_VEHICLE_WELCOME = 0,
    VSS_EVENT_VEHICLE_GOODBYE,
    VSS_EVENT_DOOR_LOCK_COMPLETE,
    VSS_EVENT_DOOR_UNLOCK_COMPLETE,
    VSS_EVENT_DOOR_LOCK_ERROR
} VssOneShotEvent_t;

typedef struct
{
    VssState_t state;
    VssAvailability_t availability;
    bool acceptingEvents;
    bool faultActive;
    SignalMeta_t meta;
} VssServiceState_t;

/* ============================================================================
 * 12. Domain aggregate state
 * ============================================================================
 */

typedef struct
{
    DoorState_t door;
    ClimateState_t climate;
    InteriorLightState_t interiorLight;
    CisEnvironmentState_t environment;
    WindowState_t window;
    VssServiceState_t vss;
    DigitalKeyState_t digitalKey;
} VehicleState_t;

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_TYPES_V0_3_H */
