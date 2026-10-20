/**
 * @file Domain_Interface_v0.6_C2.h
 * @brief S32K344 Domain Controller 논리 입출력 Interface (v0.6 후보)
 *
 * v0.6 후보 변경:
 * - Vehicle_Types_v0.3.h와의 compile-time 버전 호환성 검사 추가
 * - Climate/InteriorLight sourceGeneration을 C2 typed state로 전달
 * - 기존 72개 공개 API와 26개 TxPort callback 원형은 유지
 * - sourceGeneration 0은 원 생산 세대 미확인을 뜻한다. 실제 유효 관측은
 *   nonzero를 요구하며 INVALID/NO_DATA에는 0을 허용한다. STALE+0은 OPEN이다.
 * - sourceGeneration과 BCM BOOT_ID의 매핑은 BINDING_OPEN이며 이 헤더는
 *   CAN/UART wire format 또는 자동 변환을 정의하지 않는다.
 * - Network 담당자 승인 전 Production include로 전환하지 않는다.
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

/* C2 Domain boundary: Task-only, non-blocking. RX const objects are borrowed
 * until return. TX OK requires caller-independent stable Network-owned bounded
 * value/deep copy, or synchronous Network-owned encode with AGE/CRC refreshed
 * before send. Never retain a caller pointer. Tables are copied by value.
 * Core commit precedes optional observer; observer failure cannot replay commit.
 * Explicit enum/field codec only; never cast local enums or memcpy structs to wire.
 * WINDOW [구현 보류 — 설계 유지]: declarations only, no runtime activation.
 */

#ifndef DOMAIN_INTERFACE_V0_6_C2_H
#define DOMAIN_INTERFACE_V0_6_C2_H

#if defined(DOMAIN_INTERFACE_H)
#error "Domain_Interface_v0.5_C2_target.h and Domain_Interface_v0.6_C2.h must not be mixed"
#endif

/* Keep the primary include guard versioned.  Claiming the legacy guard prevents
 * a later accidental include of v0.5 from redefining this candidate contract.
 */
#define DOMAIN_INTERFACE_H

#define DOMAIN_INTERFACE_VERSION_MAJOR (0U)
#define DOMAIN_INTERFACE_VERSION_MINOR (6U)
#define DOMAIN_INTERFACE_VERSION_PATCH (0U)
#define DOMAIN_INTERFACE_VERSION        (0x00000600UL)

#define DOMAIN_INTERFACE_REQUIRED_VEHICLE_TYPES_VERSION (0x00000300UL)

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "Vehicle_Types_v0.3.h"

#if !defined(VEHICLE_TYPES_VERSION)
#error "Domain Interface v0.6 requires a versioned Vehicle Types header"
#elif VEHICLE_TYPES_VERSION != DOMAIN_INTERFACE_REQUIRED_VEHICLE_TYPES_VERSION
#error "Domain Interface v0.6 requires Vehicle Types v0.3"
#endif

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
    DOMAIN_IF_REJECTED,
    DOMAIN_IF_UNSUPPORTED
} DomainIf_Status_t;




typedef enum
{
    DOMAIN_FUNCTION_DOOR = 0,
    DOMAIN_FUNCTION_CLIMATE,
    DOMAIN_FUNCTION_INTERIOR_LIGHT,
    DOMAIN_FUNCTION_DIGITAL_KEY,
    DOMAIN_FUNCTION_WINDOW,
    DOMAIN_FUNCTION_REAR_WARNING,
    DOMAIN_FUNCTION_OCCUPANT_WARNING,
    DOMAIN_FUNCTION_VSS,
    DOMAIN_FUNCTION_ENVIRONMENT,
    DOMAIN_FUNCTION_COUNT
} DomainFunctionId_t;

typedef struct
{
    DomainCommandId_t commandId;

    /**
     * 중앙이 최종 실행 목표를 결정한 논리 순서.
     * Domain 내부 provenance로만 유지하며 Network 필수 field 또는 wire 값으로 전송하지 않는다.
     */
    uint32_t decisionSequence;

    bool hasRequestContext;
    RequestContext_t requestContext;
    DeviceContextId_t requestDeviceContextId;
    CommandOrigin_t origin;
    uint32_t commandAgeMs; /* command birth age, not sensor sourceMeta.ageMs */

    SignalMeta_t meta; /* existing Domain decision/source provenance */
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
    DOMAIN_WARNING_BCM_FAULT,
    DOMAIN_WARNING_CIS_FAULT,
    DOMAIN_WARNING_WINDOW_FAULT,
    DOMAIN_WARNING_VSS_FAULT,
    DOMAIN_WARNING_COUNT
} DomainWarningType_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    DomainWarningType_t type;
    OccurrenceId_t occurrenceId;
} DomainIf_WarningAck_t;

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
    bool isRead;
    bool simulated; /* Domain-local; no silent drop on export, see O-C05 */
    DataQuality_t quality;
    QualityReason_t qualityReason;
    SourceEvidence_t source;
} DomainIf_WarningInfo_t;

typedef struct
{
    DomainFunctionId_t functionId;
    FunctionAvailability_t availability;
    ResultReason_t reason;
} DomainIf_FunctionAvailability_t;

typedef struct
{
    DomainIf_FunctionAvailability_t items[DOMAIN_FUNCTION_COUNT];
    SignalMeta_t meta;
} DomainIf_FunctionAvailabilitySet_t;

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
    OccurrenceId_t occurrenceId; /* automatic unlock execution identity */
    uint32_t resultAgeMs;

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

/* C2 application boundary DTOs: no wire layout or additional state owner. */
typedef enum
{
    BCM_COMMAND_KIND_DOOR = 0,
    BCM_COMMAND_KIND_CLIMATE,
    BCM_COMMAND_KIND_INTERIOR_LIGHT
} DomainIf_BcmCommandKind_t;

/* Local completion evidence; Domain decides the vehicle event/audio meaning. */
typedef enum
{
    BCM_DOOR_EVIDENCE_LOCK_COMPLETED = 0,
    BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED,
    BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET,
    BCM_DOOR_EVIDENCE_REQUEST_REJECTED,
    BCM_DOOR_EVIDENCE_REQUEST_FAILED
} DomainIf_BcmDoorEvidenceType_t;

typedef struct
{
    DomainIf_BcmDoorEvidenceType_t type;
    OccurrenceId_t occurrenceId;
    DoorLockTarget_t target;
    DataQuality_t quality;
    uint32_t ageMs;
} DomainIf_BcmDoorResultEvidence_t;

/* Evidence travels atomically with its result, but is not a protection event.
 * No synthetic result age/sequence is inferred from EVENT_AGE/TX_SEQUENCE.
 */
typedef struct
{
    DomainCommandId_t commandId;
    DomainIf_BcmCommandKind_t kind;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
    bool hasDoorEvidence;
    DomainIf_BcmDoorResultEvidence_t doorEvidence;
} DomainIf_BcmCommandResult_t;

typedef enum
{
    BCM_PROTECTION_OVERHEAT_DETECTED = 0,
    BCM_PROTECTION_FAN_MISMATCH_DETECTED,
    BCM_PROTECTION_RECOVERY_CONFIRMED
} DomainIf_BcmProtectionEventType_t;

typedef struct
{
    DomainIf_BcmProtectionEventType_t type;
    OccurrenceId_t occurrenceId;
    SourceGeneration_t sourceGeneration; /* identity/derived fault-warning lineage */
    DataQuality_t quality;
    uint32_t ageMs;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
    uint8_t affectedFunctionMask; /* semantic bits: door / climate / light */
} DomainIf_BcmProtectionEvent_t;

/* Lossless status bundle replaces the old separate ECU-state/fault ingress. */
typedef struct
{
    CommonEcuState_t ecuState;
    uint16_t faultMask;
    FaultInfo_t activeFault; /* faultCode==0: explicitly NO active fault.
        category/state/quality are then ignored; no redundant hasActiveFault.
        If code!=0, category must be valid. Wire NONE=0 is not SENSOR=0. */
    uint8_t affectedFunctionMask;
    bool recovering;
    SourceGeneration_t sourceGeneration; /* for the derived BCM fault warning */
    SignalMeta_t meta;
} DomainIf_BcmStatusReport_t;

/* Common evidence only for derived warning outputs, not every message. */
typedef struct
{
    DataQuality_t quality;
    QualityReason_t reason;
    FunctionAvailability_t availability;
    SourceEvidence_t source;
} DomainIf_WarningInputEvidence_t;

/* Publication envelope: reuses the Application-owned result object. */
typedef enum
{
    MOBILE_RESULT_TARGET_DOMAIN_SETTING = 0,
    MOBILE_RESULT_TARGET_BCM,
    MOBILE_RESULT_TARGET_WINDOW /* [구현 보류 — 설계 유지] */
} DomainIf_MobileResultTarget_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestResultInfo_t resultInfo; /* original Session + Request ID */
    bool hasVehicleResult;         /* false: resultInfo.result is not inspected */
    DomainIf_MobileResultTarget_t target;
    DomainCommandId_t commandId;   /* zero for a setting-only result */
    CommandOrigin_t origin;
    uint32_t resultAgeMs;          /* at API call; Network adds its own waiting */
} DomainIf_MobileRequestResult_t;

/* Typed projection of existing SettingsManager getters, not another store. */
typedef struct
{
    bool hasTargetTemperature;
    int16_t targetTemperatureCentiC;
    bool hasClimateAutoSetting;
    bool climateAutoEnabled;
    bool hasUserFanLevel;
    FanLevel_t userFanLevel;
    bool hasInteriorLightEnableSetting;
    bool interiorLightEnabled;
    bool hasNormalLightLevel;
    uint8_t normalLightLevelPercent;
    bool hasNormalLightColor;
    RgbColor_t normalLightColor;
    SignalMeta_t meta;
} DomainIf_UserSettingsState_t;

/* Node-local link identity. Unsupported links return local UNSUPPORTED. */
typedef enum
{
    NETWORK_LINK_CAN = 0,
    NETWORK_LINK_UART_M,
    NETWORK_LINK_UART_C
} NetworkLinkId_t;

typedef enum
{
    NETWORK_TRANSPORT_UNKNOWN = 0,
    NETWORK_TRANSPORT_UNAVAILABLE,
    NETWORK_TRANSPORT_AVAILABLE,
    NETWORK_TRANSPORT_RECOVERING
} NetworkTransportState_t;

/* Saturating u32 operational counters, reset only at local Network init/reset.
 * Not per-Request outcomes or a new diagnostics owner/store.
 */
typedef struct
{
    uint32_t rxCrcRejectCount;
    uint32_t rxFormatRejectCount;
    uint32_t rxOverflowCount;
    uint32_t txFinalFailureCount;
    uint32_t busOffCount;
} NetworkOperationalCounters_t;

typedef struct
{
    NetworkLinkId_t link;
    NetworkTransportState_t transportState;
    bool hasValidRx;
    uint32_t lastValidRxAgeMs; /* meaningful only when hasValidRx */
    NetworkOperationalCounters_t counters;
} NetworkHealthSnapshot_t;

/* Local semantic correlation only. No wire code, TxTicket or success event. */
typedef enum
{
    NETWORK_LOCAL_ITEM_COMMAND = 0,
    NETWORK_LOCAL_ITEM_REQUEST_RESULT,
    NETWORK_LOCAL_ITEM_OCCURRENCE /* one-shot Event or occurrence-keyed Result */
} NetworkLocalItemKind_t;

typedef enum
{
    NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL = 0,
    NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND,
    NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND
} NetworkLocalFailureReason_t;

typedef struct
{
    NetworkLinkId_t link;
    NetworkLocalItemKind_t itemKind;
    NetworkLocalFailureReason_t reason;
    SourceGeneration_t identityGeneration; /* captured at admission:
        COMMAND: command producer/current Domain generation;
        REQUEST_RESULT: result publication producer generation;
        OCCURRENCE: occurrence producer generation.
        Not target ECU BOOT_ID, RequestContext Session or current App Session.
        Never relabel an old item with a new admitted generation. */
    uint32_t elapsedSinceAdmissionMs; /* local evidence, not wire/source age */
    bool hasCommandId;
    DomainCommandId_t commandId;
    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext; /* original Session, never current relabel */
    bool hasOccurrenceId;
    OccurrenceId_t occurrenceId;
} NetworkLocalFailure_t;

/* WINDOW [구현 보류 — 설계 유지]. Small fault projection only; no live binding.
 * faultCode==0 ignores category/state; occurrence/age are retained separately.
 */
typedef struct
{
    FaultInfo_t fault;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration;
} DomainIf_WindowFaultState_t;





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
    CIS_INTERFACE_AVAILABLE = 0, /* semantic READY; explicitly mapped */
    CIS_INTERFACE_UNAVAILABLE = 2,
    CIS_INTERFACE_RECOVERING = 3 /* no public DEGRADED; never raw-cast */
} DomainIf_CisInterfaceStatus_t;

typedef struct
{
    CisState_t ecuState;
    CisFunctionStatus_t occupantFunctionStatus;
    CisFunctionStatus_t temperatureFunctionStatus;
    CisFunctionStatus_t humidityFunctionStatus;
    CisFunctionStatus_t illuminanceFunctionStatus;
    CisFunctionStatus_t rearFunctionStatus;
    DomainIf_CisInterfaceStatus_t piUartStatus;
    DomainIf_CisInterfaceStatus_t canInterfaceStatus;
    uint16_t faultMask;
    uint16_t activeFaultCode;
    bool recovering;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint8_t affectedFunctionMask;
    SourceGeneration_t sourceGeneration; /* derived CIS fault-warning lineage */
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
    SourceGeneration_t sourceGeneration; /* [구현 보류 — 설계 유지] */
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
    SignalMeta_t meta; /* quality/original age; updateSequence is ignored by
        VSS_EVENT codec and occurrence dedup/replay, but semantic zero is not forced */
    FunctionAvailability_t availability;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
} DomainIf_VssOneShotEvent_t;

typedef enum
{
    OCCUPANT_HAZARD_CLEAR = 0,
    OCCUPANT_HAZARD_ACTIVE,
    OCCUPANT_HAZARD_UNCONFIRMED
} OccupantHazardState_t;

typedef struct
{
    AntiPinchState_t windowAntiPinchState; /* [구현 보류 — 설계 유지] */
    DomainIf_WarningInputEvidence_t antiPinchEvidence;
    OccupantHazardState_t occupantHazardState;
    DomainIf_WarningInputEvidence_t occupantEvidence;
    RearObstacleState_t rearObstacleState;
    DomainIf_WarningInputEvidence_t rearEvidence;
    RearDetectionActivation_t rearDetectionActivation;
    SignalMeta_t rearActivationMeta; /* independent Domain policy evaluation */
    QualityReason_t rearActivationReason;
    /* No format marker or separate activation source boot here. */
} DomainIf_VssWarningState_t;

typedef struct
{
    VssServiceState_t service;
    uint8_t faultMask;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint32_t lastFaultAgeMs;
    DataQuality_t faultQuality;
    SourceGeneration_t sourceGeneration; /* derived VSS fault-warning lineage */
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

    DomainIf_Status_t (*onWarningAck)(
        const DomainIf_WarningAck_t *ack);

    DomainIf_Status_t (*onBcmStatus)(
        const DomainIf_BcmStatusReport_t *status);

    DomainIf_Status_t (*onBcmCommandResult)(
        const DomainIf_BcmCommandResult_t *result);

    DomainIf_Status_t (*onBcmProtectionEvent)(
        const DomainIf_BcmProtectionEvent_t *event);

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
        const DomainIf_MobileRequestResult_t *result);

    DomainIf_Status_t (*txMobileBcmDoorState)(const DoorState_t *state);
    DomainIf_Status_t (*txMobileBcmClimateState)(const ClimateState_t *state);
    DomainIf_Status_t (*txMobileBcmInteriorLightState)(const InteriorLightState_t *state);
    DomainIf_Status_t (*txMobileBcmStatus)(const DomainIf_BcmStatusReport_t *state);
    DomainIf_Status_t (*txMobileCisEnvironment)(const CisCabinEnvironmentState_t *state);
    DomainIf_Status_t (*txMobileCisOccupant)(const CisOccupantState_t *state);
    DomainIf_Status_t (*txMobileCisRear)(const CisRearState_t *state);
    DomainIf_Status_t (*txMobileCisStatus)(const DomainIf_CisStatusReport_t *state);
    DomainIf_Status_t (*txMobileVssStatus)(const DomainIf_VssStateReport_t *state);
    DomainIf_Status_t (*txMobileWindowState)(const WindowState_t *state);
    DomainIf_Status_t (*txMobileWindowFault)(const DomainIf_WindowFaultState_t *state);

    DomainIf_Status_t (*txMobileWarning)(
        const DomainIf_WarningInfo_t *warning);

    DomainIf_Status_t (*txMobileFunctionAvailability)(
        const DomainIf_FunctionAvailabilitySet_t *availability);

    DomainIf_Status_t (*txMobileDigitalKeyState)(
        const DigitalKeyState_t *state);

    DomainIf_Status_t (*txMobileDigitalKeyResult)(
        const DomainIf_DigitalKeyResult_t *result);

    DomainIf_Status_t (*txMobileUserSettings)(
        const DomainIf_UserSettingsState_t *settings);
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



DomainIf_Status_t DomainIf_RxWarningAck(
    const DomainIf_WarningAck_t *ack);

/* BCM */


DomainIf_Status_t DomainIf_RxBcmDoorState(
    const DoorState_t *state);

DomainIf_Status_t DomainIf_RxBcmClimateState(
    const ClimateState_t *state);

DomainIf_Status_t DomainIf_RxBcmInteriorLightState(
    const InteriorLightState_t *state);





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
    const DomainIf_MobileRequestResult_t *result);



DomainIf_Status_t DomainIf_TxMobileWarning(
    const DomainIf_WarningInfo_t *warning);

DomainIf_Status_t DomainIf_TxMobileFunctionAvailability(
    const DomainIf_FunctionAvailabilitySet_t *availability);

DomainIf_Status_t DomainIf_TxMobileDigitalKeyState(
    const DigitalKeyState_t *state);

DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult(
    const DomainIf_DigitalKeyResult_t *result);

/* C2: additional small projections and split BCM ingress. */
DomainIf_Status_t DomainIf_RxBcmStatus(
    const DomainIf_BcmStatusReport_t *status);

DomainIf_Status_t DomainIf_RxBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result);

DomainIf_Status_t DomainIf_RxBcmProtectionEvent(
    const DomainIf_BcmProtectionEvent_t *event);

DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);

DomainIf_Status_t DomainIf_TxMobileBcmClimateState(const ClimateState_t *state);

DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(const InteriorLightState_t *state);

DomainIf_Status_t DomainIf_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state);

DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);

DomainIf_Status_t DomainIf_TxMobileCisOccupant(const CisOccupantState_t *state);

DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state);

DomainIf_Status_t DomainIf_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state);

DomainIf_Status_t DomainIf_TxMobileVssStatus(const DomainIf_VssStateReport_t *state);

DomainIf_Status_t DomainIf_TxMobileWindowState(const WindowState_t *state);

DomainIf_Status_t DomainIf_TxMobileWindowFault(const DomainIf_WindowFaultState_t *state);

DomainIf_Status_t DomainIf_TxMobileUserSettings(
    const DomainIf_UserSettingsState_t *settings);

/* Domain passive operation. Apply prior M_CONTEXT first, then call this BEFORE
 * any owner getter in the same serialized owner context. SESSION_READY=0 does
 * not reject a valid first Query. Reuses GatewaySessionManager and the existing
 * DomainIf_ValidatePassiveSession behavior; does not create/activate a Session.
 * Liveness/freshness invalidation is required to be reflected in current context by
 * the existing owner/supervision. This operation defines/evaluates no producer
 * source TTL and does not reinterpret the M_CONTEXT receive timeout of 300ms.
 * Query snapshot getters keep nowMs solely for snapshot/projection age.
 */
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext(
    DeviceContextId_t deviceContextId, SessionId_t sessionId);

/* Caller-owned out objects. Failure leaves out undefined; never synthesize OK.
 * The same semantic types serve unsolicited TX and Query snapshots.
 * Network translation units include this boundary, never Core manager headers.
 * Query ID/Scope/End remain Network-private; no public transaction type.
 */
DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);
DomainIf_Status_t DomainIf_QueryGetBcmClimateState(uint32_t nowMs, ClimateState_t *outState);
DomainIf_Status_t DomainIf_QueryGetBcmInteriorLightState(uint32_t nowMs, InteriorLightState_t *outState);
DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);
DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);
DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t nowMs, CisOccupantState_t *outState);
DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t nowMs, CisRearState_t *outState);
DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);
DomainIf_Status_t DomainIf_QueryGetVssStatus(uint32_t nowMs, DomainIf_VssStateReport_t *outState);
DomainIf_Status_t DomainIf_QueryGetWindowState(uint32_t nowMs, WindowState_t *outState);
DomainIf_Status_t DomainIf_QueryGetWindowFault(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);
DomainIf_Status_t DomainIf_QueryGetUserSettings(
    uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);
DomainIf_Status_t DomainIf_QueryGetFunctionAvailability(
    uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);
DomainIf_Status_t DomainIf_QueryGetDigitalKeyState(
    uint32_t nowMs, DigitalKeyState_t *outState);
DomainIf_Status_t DomainIf_QueryGetCurrentWarning(
    DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);
DomainIf_Status_t DomainIf_QueryGetRequestResult(
    DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest,
    uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);
DomainIf_Status_t DomainIf_QueryGetRecentRequestResults(
    DeviceContextId_t requestingDevice, uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);
DomainIf_Status_t DomainIf_QueryGetLastDigitalKeyResult(
    DeviceContextId_t requestingDevice, uint32_t nowMs,
    DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);

/* Mandatory Health: link-local snapshot, not peer ECU/function availability.
 * Down transport can still return OK with transportState=UNAVAILABLE.
 * Domain consumer: CommunicationMonitor -> DiagnosticManager; other Nodes use
 * their existing supervision owner and equivalent node-local boundary.
 */
DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot(
    NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);

/* Required semantic ingress on a Domain Node supporting correlated failure.
 * Bind this ingress to an existing owner/supervision sink; an optional observer
 * is never its sole sink. Required binding must be ready before local success
 * admission for an item requiring this failure contract. Without binding, do
 * not claim correlated async failure support or erase identity into a counter.
 * Required sink binding is O-C10 (implementation blocker). O-C08 separately
 * keeps confidence/diagnostic/unconfirmed lifecycle policy OPEN.
 * Only after local TX success AND item disposition is required by its owner.
 * Not for sync reject, CRC/format reject, optional observer error or ordinary
 * periodic loss. Never AbortBeforeDispatch on an already DISPATCHED Command;
 * never synthesize Vehicle FAILED / REQUEST_RESULT_FAILED.
 */
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_V0_6_C2_H */
