/**
 * @file CommandManager.h
 * @brief Domain이 실행 ECU에 내린 Command의 ID/순서/출처/Lifecycle 추적
 *
 * ============================================================================
 * 책임
 * ============================================================================
 *
 * RequestManager:
 *   MOBILE Request의 Session + Request ID / 중복 / History를 관리
 *
 * CommandManager:
 *   Domain이 최종 결정하여 실행 ECU에 보낸 Command를 관리
 *
 * 예:
 *
 *   MOBILE Request #100
 *       -> Domain Door Decision
 *       -> BCM Command #25
 *
 *   AutoVentilation Job #3
 *       -> WINDOW Command #26
 *
 * 즉 Command는 MOBILE Request 없이도 만들어질 수 있다.
 *
 * ============================================================================
 * 핵심 원칙
 * ============================================================================
 *
 * - Command ID는 Domain 내부에서 유일하게 발급
 * - 중앙 최종 결정 순서(decisionSequence)를 별도 추적
 * - Command Origin과 원 Request/Automatic Job을 연결
 * - 진행 중 Command는 Capacity 압박으로 덮어쓰지 않음
 * - UNKNOWN은 실제 ECU Result가 아니므로 Confirmation과 분리
 * - Wire Sequence 폭/rolling counter는 Network 설계 책임
 *
 * v0.4:
 * - MOBILE Request와 연결되지 않은 자동 정책 Command를 새 중앙 결정으로
 *   대체할 때 CANCELLED 처리하는 CancelByPolicy API 추가
 *
 * v0.3:
 * - 전송 큐 진입 전에 확정적으로 취소/실패한 CREATED Command를
 *   Final 처리하는 AbortBeforeDispatch API 추가
 *
 * v0.2:
 * - MOBILE Request provenance에 DeviceContextId 추가
 *   (Result -> RequestManager 역연결에 필요)
 */

#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface.h"
#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMMAND_MANAGER_MAX_RECORDS
#define COMMAND_MANAGER_MAX_RECORDS (24U)
#endif

typedef uint32_t DomainJobId_t;

typedef enum
{
    COMMAND_MANAGER_STATUS_OK = 0,
    COMMAND_MANAGER_STATUS_INVALID_ARGUMENT,
    COMMAND_MANAGER_STATUS_NOT_INITIALIZED,
    COMMAND_MANAGER_STATUS_NOT_FOUND,
    COMMAND_MANAGER_STATUS_CAPACITY_FULL,
    COMMAND_MANAGER_STATUS_INVALID_TRANSITION,
    COMMAND_MANAGER_STATUS_ID_EXHAUSTED
} CommandManager_Status_t;

typedef enum
{
    COMMAND_TARGET_BCM = 0,
    COMMAND_TARGET_WINDOW
} CommandTarget_t;

typedef enum
{
    COMMAND_TYPE_BCM_DOOR = 0,
    COMMAND_TYPE_BCM_CLIMATE,
    COMMAND_TYPE_BCM_INTERIOR_LIGHT,
    COMMAND_TYPE_WINDOW_MOVE,
    COMMAND_TYPE_WINDOW_STOP
} CommandType_t;

typedef enum
{
    COMMAND_ORIGIN_MOBILE_REQUEST = 0,
    COMMAND_ORIGIN_DIGITAL_KEY,
    COMMAND_ORIGIN_CLIMATE_POLICY,
    COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY,
    COMMAND_ORIGIN_AUTO_VENTILATION,
    COMMAND_ORIGIN_SYSTEM
} CommandOrigin_t;

typedef enum
{
    COMMAND_LIFECYCLE_CREATED = 0,
    COMMAND_LIFECYCLE_DISPATCHED,
    COMMAND_LIFECYCLE_ACCEPTED,
    COMMAND_LIFECYCLE_IN_PROGRESS,
    COMMAND_LIFECYCLE_FINAL
} CommandLifecycle_t;

typedef struct
{
    CommandTarget_t target;
    DomainFunctionId_t functionId;
    CommandType_t type;
    CommandOrigin_t origin;

    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext;

    bool hasJobContext;
    DomainJobId_t jobId;

    /**
     * 최종 결정의 원본 정보 유효성/경과 근거.
     * 실제 Command payload의 validity/age와 연계한다.
     */
    SignalMeta_t sourceMeta;
} CommandCreateRequest_t;

typedef struct
{
    bool inUse;

    DomainCommandId_t commandId;
    uint32_t decisionSequence;

    CommandTarget_t target;
    DomainFunctionId_t functionId;
    CommandType_t type;
    CommandOrigin_t origin;

    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext;

    bool hasJobContext;
    DomainJobId_t jobId;

    SignalMeta_t sourceMeta;

    CommandLifecycle_t lifecycle;

    bool hasExecutionResult;
    RequestResult_t executionResult;
    ResultReason_t resultReason;
    ResultConfirmation_t confirmation;

    uint32_t createdAtMs;
    uint32_t updatedAtMs;
    uint32_t finalizedAtMs;
} CommandRecord_t;

typedef struct
{
    DomainCommandId_t commandId;
    uint32_t decisionSequence;
    DomainIf_CommandContext_t interfaceContext;
} CommandCreateResult_t;


/* ============================================================================
 * Init / Process
 * ============================================================================
 */

CommandManager_Status_t CommandManager_Init(void);

bool CommandManager_IsInitialized(void);

/**
 * @brief 모든 Command record를 지우는 명시적 초기화 API.
 *
 * Runtime reconnect 용도가 아니다.
 */
CommandManager_Status_t CommandManager_ClearAll(void);


/* ============================================================================
 * Create / Lifecycle
 * ============================================================================
 */

/**
 * @brief 새 Domain Command를 발급한다.
 *
 * CommandId와 decisionSequence를 생성하고 CREATED 상태로 기록한다.
 */
CommandManager_Status_t CommandManager_Create(
    const CommandCreateRequest_t *request,
    uint32_t nowMs,
    CommandCreateResult_t *outResult);

/**
 * @brief Adapter/Interface로 실제 송신 큐잉이 완료된 시점.
 */
CommandManager_Status_t CommandManager_MarkDispatched(
    DomainCommandId_t commandId,
    uint32_t nowMs);

/**
 * @brief 아직 DISPATCHED 되지 않은 CREATED Command를 취소/실패 처리.
 *
 * 사용 예:
 * - Logical Tx Port 부재
 * - Adapter enqueue 실패
 * - 송신 전 기능 조건 상실
 *
 * CANCELLED 또는 FAILED만 허용한다.
 * 실행 ECU 결과가 아니라 Domain 내부의 "미전송 종료" 기록이다.
 */
CommandManager_Status_t CommandManager_AbortBeforeDispatch(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs);

/**
 * @brief 자동 정책 Command를 더 최신 중앙 결정으로 대체.
 *
 * 제한:
 * - requestContext가 있는 MOBILE 요청 연계 Command에는 사용하지 않는다.
 * - CREATED/DISPATCHED/ACCEPTED/IN_PROGRESS만 CANCELLED로 종료한다.
 * - 실행 ECU에 물리적으로 CANCEL을 보냈다는 의미가 아니다.
 *   새 decisionSequence의 Command가 최종 목표를 대체한다.
 */
CommandManager_Status_t CommandManager_CancelByPolicy(
    DomainCommandId_t commandId,
    ResultReason_t reason,
    uint32_t nowMs);

/**
 * @brief 실행 ECU가 Command를 수용했음을 확인.
 */
CommandManager_Status_t CommandManager_MarkAccepted(
    DomainCommandId_t commandId,
    uint32_t nowMs);

/**
 * @brief 실행 진행 상태 확인.
 */
CommandManager_Status_t CommandManager_MarkInProgress(
    DomainCommandId_t commandId,
    uint32_t nowMs);

/**
 * @brief DONE / REJECTED / CANCELLED / FAILED 최종 결과 기록.
 */
CommandManager_Status_t CommandManager_Finalize(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs);

/**
 * @brief 실행 결과를 현재 확정할 수 없게 되었음을 표시.
 *
 * FAILED를 생성하지 않는다.
 */
CommandManager_Status_t CommandManager_MarkUnconfirmed(
    DomainCommandId_t commandId,
    uint32_t nowMs);


/* ============================================================================
 * Query
 * ============================================================================
 */

CommandManager_Status_t CommandManager_GetRecord(
    DomainCommandId_t commandId,
    CommandRecord_t *outRecord);

CommandManager_Status_t CommandManager_GetRequestContext(
    DomainCommandId_t commandId,
    RequestContext_t *outContext,
    bool *outHasContext);

/**
 * @brief 원 MOBILE Request의 Device + Request Context를 함께 조회.
 */
CommandManager_Status_t CommandManager_GetRequestProvenance(
    DomainCommandId_t commandId,
    DeviceContextId_t *outDeviceContextId,
    RequestContext_t *outContext,
    bool *outHasContext);

CommandManager_Status_t CommandManager_GetJobContext(
    DomainCommandId_t commandId,
    DomainJobId_t *outJobId,
    bool *outHasContext);

/**
 * @brief 특정 Target/Function에서 가장 최근 중앙 결정 순서를 반환.
 */
CommandManager_Status_t CommandManager_GetLatestDecisionSequence(
    CommandTarget_t target,
    DomainFunctionId_t functionId,
    uint32_t *outSequence);

uint32_t CommandManager_GetRecordCount(void);

uint32_t CommandManager_GetActiveCount(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_MANAGER_H */
