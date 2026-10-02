/**
 * @file Vehicle_Types.h
 * @brief S32K344 Domain Controller 공통 논리 데이터 타입 정의
 *
 * v0.2 변경:
 * - CIS 값을 Occupant / Cabin / Rear 그룹으로 분리
 * - CIS 각 값에 독립적인 Validity / QualityReason / Freshness 근거 부여
 * - ESP32 Registration/Connection과 Proximity의 Freshness를 분리
 * - VSS 서비스 상태에도 SignalMeta를 포함
 *
 * 이 파일은 차량 의미(Canonical Logical Types)를 정의한다.
 * CAN ID, Byte/Bit, UART Frame, CRC/E2E 배치는 포함하지 않는다.
 */

#ifndef VEHICLE_TYPES_H
#define VEHICLE_TYPES_H

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
typedef uint32_t SessionId_t;
typedef uint32_t OccurrenceId_t;
typedef uint32_t UpdateSequence_t;

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
    QUALITY_REASON_NO_DATA
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
    RESULT_REASON_OTHER
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
    SignalMeta_t meta;
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
    SignalMeta_t meta;
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

typedef struct
{
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
    bool commandApplied;
    bool physicalFeedbackSupported;
    SignalMeta_t meta;
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
    SignalMeta_t meta;
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
    UnlockOrigin_t lastUnlockOrigin;
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
    REAR_MEASUREMENT_RECOVERING
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
typedef struct
{
    bool occupantPresent;
    uint8_t occupantCount;
    ValueQuality_t quality;
} CisOccupantState_t;

/**
 * @brief 실내 환경값은 값별로 독립적인 품질을 갖는다.
 */
typedef struct
{
    int16_t cabinTemperature;
    ValueQuality_t temperatureQuality;

    uint16_t cabinHumidity;
    ValueQuality_t humidityQuality;

    uint16_t cabinIlluminance;
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
    uint16_t rearDistanceCm;
    RearMeasurementState_t measurementState;
    ValueQuality_t distanceQuality;
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

#endif /* VEHICLE_TYPES_H */
