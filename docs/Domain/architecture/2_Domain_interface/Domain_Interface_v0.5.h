/**
 * @file Domain_Interface.h
 * @brief S32K344 Domain Controller 논리 입출력 Interface
 *
 * v0.5 변경:
 * - Digital Key 결과에 hasVehicleResult / actualUnlockTransition 추가
 *   (UNKNOWN/미확인과 실제 근접 해제 성공을 구분)
 *
 * v0.4 변경:
 * - Rx Observer 등록 API 추가: 아직 Feature/Diagnostic 소비자가 없는 Event를
 *   Domain Task/상위 모듈로 전달 가능
 * - Tx Port 등록 API 추가: CAN/UART Adapter 미연결 상태에서 전송 성공을
 *   임의 생성하지 않음
 * - Domain_Interface.c Core Routing 구현 기준 추가
 *
 * v0.3 변경:
 * - 모든 실행 Command Context에 중앙 최종 결정 순서(decisionSequence) 추가
 * - WINDOW Command/Result를 RequestContext 중심에서 CommandId 중심으로 수정
 * - AutoVentilation 등 MOBILE Request가 없는 Domain Command도 동일하게 추적 가능
 *
 * v0.2 변경:
 * - BCM 상태 입력을 Door / Climate / InteriorLight로 분리
 * - CIS 입력을 Occupant / Cabin / Rear / Status로 분리
 * - ESP32 입력을 Connection / Proximity로 분리
 * - 각 논리 정보가 독립적인 Freshness를 유지할 수 있도록 API를 세분화
 *
 * CAN/UART Raw Encoding은 이 Header의 책임이 아니다.
 */

#ifndef DOMAIN_INTERFACE_H
#define DOMAIN_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DOMAIN_IF_OK = 0,
    DOMAIN_IF_INVALID_ARGUMENT,
    DOMAIN_IF_NOT_INITIALIZED,
    DOMAIN_IF_NOT_AVAILABLE,
    DOMAIN_IF_QUEUE_FULL,
    DOMAIN_IF_REJECTED
} DomainIf_Status_t;

typedef uint32_t DomainCommandId_t;
typedef uint32_t DeviceContextId_t;

typedef enum
{
    DOMAIN_FUNCTION_DOOR = 0,
    DOMAIN_FUNCTION_CLIMATE,
    DOMAIN_FUNCTION_INTERIOR_LIGHT,
    DOMAIN_FUNCTION_DIGITAL_KEY,
    DOMAIN_FUNCTION_WINDOW,
    DOMAIN_FUNCTION_REAR_WARNING,
    DOMAIN_FUNCTION_OCCUPANT_WARNING,
    DOMAIN_FUNCTION_VSS
} DomainFunctionId_t;

typedef struct
{
    DomainCommandId_t commandId;

    /**
     * 중앙이 최종 실행 목표를 결정한 논리 순서.
     * 실제 wire width/rolling-counter mapping은 Network 설계에서 결정한다.
     */
    uint32_t decisionSequence;

    bool hasRequestContext;
    RequestContext_t requestContext;

    SignalMeta_t meta;
} DomainIf_CommandContext_t;

/* ============================================================================
 * MOBILE / ESP32 -> Domain
 * ============================================================================
 */

typedef enum
{
    MOBILE_REQUEST_DOOR_LOCK = 0,
    MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE,
    MOBILE_REQUEST_CLIMATE_AUTO_ENABLE,
    MOBILE_REQUEST_FAN_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE,
    MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL,
    MOBILE_REQUEST_INTERIOR_LIGHT_COLOR,
    MOBILE_REQUEST_DIGITAL_KEY_SETTING
} DomainIf_MobileRequestType_t;

typedef union
{
    DoorLockTarget_t doorTarget;
    int16_t targetTemperature;
    bool enable;
    FanLevel_t fanLevel;
    uint8_t levelPercent;
    RgbColor_t color;
    DigitalKeySetting_t digitalKeySetting;
} DomainIf_MobileRequestPayload_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestContext_t context;
    DomainIf_MobileRequestType_t type;
    DomainIf_MobileRequestPayload_t payload;
    SignalMeta_t meta;
} DomainIf_MobileRequest_t;

typedef enum
{
    STATE_QUERY_VEHICLE_STATE = 0,
    STATE_QUERY_REQUEST_RESULT
} DomainIf_StateQueryType_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    DomainIf_StateQueryType_t queryType;
    RequestId_t requestId;
} DomainIf_StateQuery_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    OccurrenceId_t occurrenceId;
} DomainIf_WarningAck_t;

/* ============================================================================
 * Domain -> MOBILE / ESP32
 * ============================================================================
 */

typedef enum
{
    DOMAIN_WARNING_WINDOW_ANTIPINCH = 0,
    DOMAIN_WARNING_OCCUPANT_HAZARD,
    DOMAIN_WARNING_REAR_OBSTACLE,
    DOMAIN_WARNING_DOOR_LEFT_OPEN,
    DOMAIN_WARNING_DOOR_LOCK_ERROR
} DomainWarningType_t;

typedef enum
{
    DOMAIN_WARNING_SEVERITY_INFO = 0,
    DOMAIN_WARNING_SEVERITY_CAUTION,
    DOMAIN_WARNING_SEVERITY_EMERGENCY
} DomainWarningSeverity_t;

typedef enum
{
    DOMAIN_WARNING_STATE_CLEAR = 0,
    DOMAIN_WARNING_STATE_ACTIVE,
    DOMAIN_WARNING_STATE_UNCONFIRMED
} DomainWarningState_t;

typedef struct
{
    DomainWarningType_t type;
    DomainWarningSeverity_t severity;
    DomainWarningState_t state;
    OccurrenceId_t occurrenceId;
    bool simulated;
    SignalMeta_t meta;
} DomainIf_WarningInfo_t;

typedef struct
{
    DomainFunctionId_t functionId;
    FunctionAvailability_t availability;
    ResultReason_t reason;
} DomainIf_FunctionAvailability_t;

typedef struct
{
    /**
     * false이면 현재 확정 가능한 차량 실행 결과가 없음.
     * confirmation=UNCONFIRMED과 함께 MOBILE에서는 UNKNOWN 의미로 표시 가능.
     */
    bool hasVehicleResult;

    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;

    UnlockOrigin_t origin;
    DomainCommandId_t relatedDoorCommandId;

    /**
     * 실제 BCM 잠금 해제 전이가 확인된 경우만 true.
     * ALREADY_AT_TARGET + DONE은 false.
     */
    bool actualUnlockTransition;
} DomainIf_DigitalKeyResult_t;

/* ============================================================================
 * BCM
 * ============================================================================
 */

typedef enum
{
    BCM_EVENT_LOCK_COMPLETED = 0,
    BCM_EVENT_UNLOCK_COMPLETED,
    BCM_EVENT_ALREADY_AT_TARGET,
    BCM_EVENT_REQUEST_REJECTED,
    BCM_EVENT_REQUEST_FAILED,
    BCM_EVENT_OVERHEAT_DETECTED,
    BCM_EVENT_FAN_MISMATCH_DETECTED,
    BCM_EVENT_RECOVERY_CONFIRMED
} DomainIf_BcmEventType_t;

typedef struct
{
    DomainCommandId_t commandId;
    DomainIf_BcmEventType_t eventType;
    RequestResult_t result;
    ResultReason_t reason;
    SignalMeta_t meta;
} DomainIf_BcmEvent_t;

typedef struct
{
    DomainIf_CommandContext_t context;
    DoorLockTarget_t target;
} DomainIf_BcmDoorCommand_t;

typedef struct
{
    DomainIf_CommandContext_t context;
    FanLevel_t fanTarget;
    ThermalDirection_t thermalDirection;
    uint8_t thermalTargetLevelPercent;
} DomainIf_BcmClimateCommand_t;

typedef struct
{
    DomainIf_CommandContext_t context;
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
} DomainIf_BcmInteriorLightCommand_t;

/* ============================================================================
 * CIS
 * ============================================================================
 */

typedef enum
{
    CIS_FUNCTION_STATUS_READY = 0,
    CIS_FUNCTION_STATUS_VALID,
    CIS_FUNCTION_STATUS_UNAVAILABLE,
    CIS_FUNCTION_STATUS_FAULT,
    CIS_FUNCTION_STATUS_RECOVERING
} DomainIf_CisFunctionStatus_t;

typedef enum
{
    CIS_INTERFACE_AVAILABLE = 0,
    CIS_INTERFACE_DEGRADED,
    CIS_INTERFACE_UNAVAILABLE
} DomainIf_CisInterfaceStatus_t;

typedef struct
{
    CisState_t ecuState;
    DomainIf_CisFunctionStatus_t occupantFunctionStatus;
    DomainIf_CisFunctionStatus_t environmentFunctionStatus;
    DomainIf_CisFunctionStatus_t rearFunctionStatus;
    DomainIf_CisInterfaceStatus_t interfaceStatus;
    SignalMeta_t meta;
} DomainIf_CisStatusReport_t;

typedef struct
{
    bool vehiclePowerAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_CisPowerPermission_t;

/* ============================================================================
 * WINDOW
 * ============================================================================
 */

typedef enum
{
    ANTIPINCH_PROTECTION_IDLE = 0,
    ANTIPINCH_PROTECTION_REVERSING,
    ANTIPINCH_PROTECTION_COMPLETED,
    ANTIPINCH_PROTECTION_ABORTED
} DomainIf_AntiPinchProtectionState_t;

typedef struct
{
    OccurrenceId_t occurrenceId;
    DomainIf_AntiPinchProtectionState_t protectionState;
    bool outputDisabled;
    bool movementStopped;
    ResultReason_t reason;
    SignalMeta_t meta;
} DomainIf_AntiPinchReport_t;

/**
 * @brief WINDOW 실행 결과.
 *
 * CommandId를 기준으로 결과를 연결한다.
 * AutoVentilation처럼 MOBILE Request가 없는 Command도 동일하게 추적 가능하다.
 */
typedef struct
{
    DomainCommandId_t commandId;
    RequestResult_t result;
    ResultReason_t reason;
    uint8_t channel;
    SignalMeta_t meta;
} DomainIf_WindowCommandResult_t;

/**
 * @brief Domain -> WINDOW 확정 명령.
 */
typedef struct
{
    DomainIf_CommandContext_t context;
    uint8_t channel;
    WindowCommand_t command;
    uint8_t targetPositionPercent;
} DomainIf_WindowCommand_t;

typedef struct
{
    bool operationAllowed;
    DataQuality_t quality;
    SignalMeta_t meta;
} DomainIf_WindowOperationPermission_t;

typedef struct
{
    bool domainLinkAvailable;
    SignalMeta_t meta;
} DomainIf_WindowDomainAlive_t;

/* ============================================================================
 * VSS
 * ============================================================================
 */

typedef struct
{
    VssOneShotEvent_t event;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta;
} DomainIf_VssOneShotEvent_t;

typedef enum
{
    OCCUPANT_HAZARD_CLEAR = 0,
    OCCUPANT_HAZARD_ACTIVE,
    OCCUPANT_HAZARD_UNCONFIRMED
} OccupantHazardState_t;

typedef struct
{
    AntiPinchState_t windowAntiPinchState;
    OccupantHazardState_t occupantHazardState;
    RearObstacleState_t rearObstacleState;
    RearDetectionActivation_t rearDetectionActivation;
    SignalMeta_t meta;
} DomainIf_VssWarningState_t;

typedef struct
{
    VssServiceState_t service;
    uint16_t lastFaultCode;
    DataQuality_t faultQuality;
} DomainIf_VssStateReport_t;


/* ============================================================================
 * Domain Interface Ports
 * ============================================================================
 *
 * Domain_Interface는 RTD/CAN/UART Driver를 직접 호출하지 않는다.
 *
 * Rx Observer:
 *   Core Manager가 직접 소비하지 않는 상위 Event/Request를 Domain Task 또는
 *   Feature/Service 계층에 전달한다.
 *
 * Tx Port:
 *   실제 Adapter/Queue가 등록하는 Logical Tx 함수 집합.
 *   Port가 등록되지 않으면 Tx 함수는 DOMAIN_IF_NOT_AVAILABLE을 반환한다.
 *
 * 모든 callback은 ISR이 아니라 Domain/Communication Task 문맥에서 호출하는
 * 것을 전제로 한다.
 */

typedef struct
{
    DomainIf_Status_t (*onMobileRequest)(
        const DomainIf_MobileRequest_t *request);

    DomainIf_Status_t (*onStateQuery)(
        const DomainIf_StateQuery_t *query);

    DomainIf_Status_t (*onWarningAck)(
        const DomainIf_WarningAck_t *ack);

    DomainIf_Status_t (*onBcmEcuState)(
        CommonEcuState_t state,
        const SignalMeta_t *meta);

    DomainIf_Status_t (*onBcmEvent)(
        const DomainIf_BcmEvent_t *event);

    DomainIf_Status_t (*onBcmFault)(
        const FaultInfo_t *fault);

    DomainIf_Status_t (*onCisStatus)(
        const DomainIf_CisStatusReport_t *status);

    DomainIf_Status_t (*onCisFault)(
        const FaultInfo_t *fault);

    DomainIf_Status_t (*onWindowCommandResult)(
        const DomainIf_WindowCommandResult_t *result);

    DomainIf_Status_t (*onWindowAntiPinch)(
        const DomainIf_AntiPinchReport_t *report);

    DomainIf_Status_t (*onWindowFault)(
        const FaultInfo_t *fault);

    DomainIf_Status_t (*onVssFault)(
        const FaultInfo_t *fault);
} DomainIf_RxObserver_t;

typedef struct
{
    DomainIf_Status_t (*txBcmDoorCommand)(
        const DomainIf_BcmDoorCommand_t *command);

    DomainIf_Status_t (*txBcmClimateCommand)(
        const DomainIf_BcmClimateCommand_t *command);

    DomainIf_Status_t (*txBcmInteriorLightCommand)(
        const DomainIf_BcmInteriorLightCommand_t *command);

    DomainIf_Status_t (*txCisPowerPermission)(
        const DomainIf_CisPowerPermission_t *permission);

    DomainIf_Status_t (*txWindowCommand)(
        const DomainIf_WindowCommand_t *command);

    DomainIf_Status_t (*txWindowOperationPermission)(
        const DomainIf_WindowOperationPermission_t *permission);

    DomainIf_Status_t (*txWindowDomainAlive)(
        const DomainIf_WindowDomainAlive_t *alive);

    DomainIf_Status_t (*txVssOneShotEvent)(
        const DomainIf_VssOneShotEvent_t *event);

    DomainIf_Status_t (*txVssWarningState)(
        const DomainIf_VssWarningState_t *state);

    DomainIf_Status_t (*txMobileRequestResult)(
        const RequestResultInfo_t *result);

    DomainIf_Status_t (*txMobileVehicleState)(
        const VehicleState_t *state);

    DomainIf_Status_t (*txMobileWarning)(
        const DomainIf_WarningInfo_t *warning);

    DomainIf_Status_t (*txMobileFunctionAvailability)(
        const DomainIf_FunctionAvailability_t *availability);

    DomainIf_Status_t (*txMobileDigitalKeyState)(
        const DigitalKeyState_t *state);

    DomainIf_Status_t (*txMobileDigitalKeyResult)(
        const DomainIf_DigitalKeyResult_t *result);
} DomainIf_TxPort_t;

/* ============================================================================
 * Init
 * ============================================================================
 */

DomainIf_Status_t DomainIf_Init(void);
bool DomainIf_IsInitialized(void);

DomainIf_Status_t DomainIf_SetRxObserver(
    const DomainIf_RxObserver_t *observer);

DomainIf_Status_t DomainIf_SetTxPort(
    const DomainIf_TxPort_t *port);

/* ============================================================================
 * Rx: External -> Domain
 * ============================================================================
 */

/* ESP32 */
DomainIf_Status_t DomainIf_RxDigitalKeyConnection(
    DeviceContextId_t deviceContextId,
    const DigitalKeyConnectionState_t *connection);

DomainIf_Status_t DomainIf_RxProximity(
    DeviceContextId_t deviceContextId,
    const ProximityInput_t *proximity);

DomainIf_Status_t DomainIf_RxMobileRequest(
    const DomainIf_MobileRequest_t *request);

DomainIf_Status_t DomainIf_RxStateQuery(
    const DomainIf_StateQuery_t *query);

DomainIf_Status_t DomainIf_RxWarningAck(
    const DomainIf_WarningAck_t *ack);

/* BCM */
DomainIf_Status_t DomainIf_RxBcmEcuState(
    CommonEcuState_t state,
    const SignalMeta_t *meta);

DomainIf_Status_t DomainIf_RxBcmDoorState(
    const DoorState_t *state);

DomainIf_Status_t DomainIf_RxBcmClimateState(
    const ClimateState_t *state);

DomainIf_Status_t DomainIf_RxBcmInteriorLightState(
    const InteriorLightState_t *state);

DomainIf_Status_t DomainIf_RxBcmEvent(
    const DomainIf_BcmEvent_t *event);

DomainIf_Status_t DomainIf_RxBcmFault(
    const FaultInfo_t *fault);

/* CIS */
DomainIf_Status_t DomainIf_RxCisStatus(
    const DomainIf_CisStatusReport_t *status);

DomainIf_Status_t DomainIf_RxCisOccupant(
    const CisOccupantState_t *state);

DomainIf_Status_t DomainIf_RxCisCabinEnvironment(
    const CisCabinEnvironmentState_t *state);

DomainIf_Status_t DomainIf_RxCisRear(
    const CisRearState_t *state);

DomainIf_Status_t DomainIf_RxCisFault(
    const FaultInfo_t *fault);

/* WINDOW */
DomainIf_Status_t DomainIf_RxWindowState(
    const WindowState_t *state);

DomainIf_Status_t DomainIf_RxWindowCommandResult(
    const DomainIf_WindowCommandResult_t *result);

DomainIf_Status_t DomainIf_RxWindowAntiPinch(
    const DomainIf_AntiPinchReport_t *report);

DomainIf_Status_t DomainIf_RxWindowFault(
    const FaultInfo_t *fault);

/* VSS */
DomainIf_Status_t DomainIf_RxVssState(
    const DomainIf_VssStateReport_t *state);

DomainIf_Status_t DomainIf_RxVssFault(
    const FaultInfo_t *fault);

/* ============================================================================
 * Tx: Domain -> External
 * ============================================================================
 */

DomainIf_Status_t DomainIf_TxBcmDoorCommand(
    const DomainIf_BcmDoorCommand_t *command);

DomainIf_Status_t DomainIf_TxBcmClimateCommand(
    const DomainIf_BcmClimateCommand_t *command);

DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand(
    const DomainIf_BcmInteriorLightCommand_t *command);

DomainIf_Status_t DomainIf_TxCisPowerPermission(
    const DomainIf_CisPowerPermission_t *permission);

DomainIf_Status_t DomainIf_TxWindowCommand(
    const DomainIf_WindowCommand_t *command);

DomainIf_Status_t DomainIf_TxWindowOperationPermission(
    const DomainIf_WindowOperationPermission_t *permission);

DomainIf_Status_t DomainIf_TxWindowDomainAlive(
    const DomainIf_WindowDomainAlive_t *alive);

DomainIf_Status_t DomainIf_TxVssOneShotEvent(
    const DomainIf_VssOneShotEvent_t *event);

DomainIf_Status_t DomainIf_TxVssWarningState(
    const DomainIf_VssWarningState_t *state);

DomainIf_Status_t DomainIf_TxMobileRequestResult(
    const RequestResultInfo_t *result);

DomainIf_Status_t DomainIf_TxMobileVehicleState(
    const VehicleState_t *state);

DomainIf_Status_t DomainIf_TxMobileWarning(
    const DomainIf_WarningInfo_t *warning);

DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(
    const DomainIf_FunctionAvailability_t *availability);

DomainIf_Status_t DomainIf_TxMobileDigitalKeyState(
    const DigitalKeyState_t *state);

DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult(
    const DomainIf_DigitalKeyResult_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_H */
