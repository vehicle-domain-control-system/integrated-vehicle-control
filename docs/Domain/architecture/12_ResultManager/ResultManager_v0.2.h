/**
 * @file ResultManager.h
 * @brief 실행 ECU의 결과를 Command -> Request/Job으로 연결하는 Domain 결과 라우터
 *
 * ============================================================================
 * 책임
 * ============================================================================
 *
 * 1. ECU Result/Event의 CommandId를 이용해 원 Command를 찾는다.
 * 2. CommandManager의 Lifecycle/Result를 단조롭게 갱신한다.
 * 3. 원 Command가 MOBILE Request에서 왔다면 RequestManager까지 결과를 반영한다.
 * 4. 원 Command가 Automatic Job에서 왔다면 Job Context를 상위 Feature에 반환한다.
 * 5. 결과를 확인할 수 없을 때 FAILED를 추정하지 않는다.
 *
 * 하지 않는 것:
 * - Digital Key FAR->NEAR 정책
 * - AutoVentilation Job 완료 조건
 * - Warning 생성
 * - Fault/Availability 집계
 *
 * ============================================================================
 * 중요한 예외
 * ============================================================================
 *
 * BCM ALREADY_AT_TARGET + DONE은 일반 요청의 DONE일 수 있지만,
 * Digital Key 근접 자동 해제의 "실제 Unlock 성공" 증거로 사용하면 안 된다.
 *
 * ResultManager는 이를 RESULT_EVIDENCE_ALREADY_AT_TARGET로 보존하고,
 * DigitalKeyManager가 실제 성공 판정 시 사용하도록 한다.
 */

#ifndef RESULT_MANAGER_H
#define RESULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "CommandManager.h"
#include "Domain_Interface.h"
#include "RequestManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    RESULT_MANAGER_STATUS_OK = 0,
    RESULT_MANAGER_STATUS_INVALID_ARGUMENT,
    RESULT_MANAGER_STATUS_NOT_INITIALIZED,
    RESULT_MANAGER_STATUS_COMMAND_NOT_FOUND,
    RESULT_MANAGER_STATUS_UNTRUSTED_REPORT,
    RESULT_MANAGER_STATUS_INCONSISTENT_REPORT,
    RESULT_MANAGER_STATUS_INVALID_TRANSITION,
    RESULT_MANAGER_STATUS_REQUEST_LINK_ERROR
} ResultManager_Status_t;

typedef enum
{
    RESULT_ROUTE_NONE = 0,
    RESULT_ROUTE_REQUEST,
    RESULT_ROUTE_JOB
} ResultRoute_t;

typedef enum
{
    RESULT_EVIDENCE_NONE = 0,
    RESULT_EVIDENCE_TARGET_TRANSITION_CONFIRMED,
    RESULT_EVIDENCE_ALREADY_AT_TARGET,
    RESULT_EVIDENCE_EXECUTION_RESULT,
    RESULT_EVIDENCE_OBSERVED_STATE
} ResultCompletionEvidence_t;

typedef struct
{
    DomainCommandId_t commandId;

    CommandOrigin_t origin;
    CommandType_t commandType;
    DomainFunctionId_t functionId;

    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;

    ResultCompletionEvidence_t completionEvidence;

    ResultRoute_t route;

    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext;

    bool hasJobContext;
    DomainJobId_t jobId;

    /**
     * true이면 이 입력 결과 때문에 Command/Request 상태가 실제로 전진했다.
     * late ACCEPTED처럼 이미 더 진행된 상태는 false로 정상 무시할 수 있다.
     */
    bool stateAdvanced;
} ResultManager_Output_t;


/* ============================================================================
 * Init
 * ============================================================================
 */

ResultManager_Status_t ResultManager_Init(void);

bool ResultManager_IsInitialized(void);


/* ============================================================================
 * ECU result input
 * ============================================================================
 */

/**
 * @brief BCM Event 중 Command Result 성격의 이벤트를 처리한다.
 *
 * Command result로 처리:
 * - LOCK_COMPLETED
 * - UNLOCK_COMPLETED
 * - ALREADY_AT_TARGET
 * - REQUEST_REJECTED
 * - REQUEST_FAILED
 *
 * 진단/보호 상태 이벤트:
 * - OVERHEAT_DETECTED
 * - FAN_MISMATCH_DETECTED
 * - RECOVERY_CONFIRMED
 *
 * 위 진단 이벤트는 ResultManager가 Command 결과로 변환하지 않는다.
 */
ResultManager_Status_t ResultManager_ProcessBcmEvent(
    const DomainIf_BcmEvent_t *event,
    uint32_t nowMs,
    ResultManager_Output_t *outResult);

/**
 * @brief WINDOW Command Result 처리.
 */
ResultManager_Status_t ResultManager_ProcessWindowResult(
    const DomainIf_WindowCommandResult_t *result,
    uint32_t nowMs,
    ResultManager_Output_t *outResult);


/* ============================================================================
 * Result confidence loss
 * ============================================================================
 */

/**
 * @brief 해당 Command의 최종 결과를 더 이상 확정할 수 없게 된 경우.
 *
 * FAILED를 생성하지 않는다.
 * 이미 알려진 중간 결과는 UNCONFIRMED로 표시한다.
 */
ResultManager_Status_t ResultManager_MarkCommandUnconfirmed(
    DomainCommandId_t commandId,
    uint32_t nowMs,
    ResultManager_Output_t *outResult);

/**
 * @brief Trusted observed state confirms a Command final result.
 *
 * Used when SysRS completion is defined by observed applied/measured state
 * rather than a dedicated command-result event.
 */
ResultManager_Status_t ResultManager_ConfirmObservedState(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs,
    ResultManager_Output_t *outResult);

#ifdef __cplusplus
}
#endif

#endif /* RESULT_MANAGER_H */
