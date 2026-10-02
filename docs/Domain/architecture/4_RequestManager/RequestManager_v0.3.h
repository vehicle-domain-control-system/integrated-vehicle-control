/**
 * @file RequestManager.h
 * @brief MOBILE Request 식별/중복 방지/Lifecycle/History 관리
 *
 * v0.3 변경:
 * - 실행 노드의 최초 응답이 IN_PROGRESS일 수 있으므로
 *   RECEIVED -> IN_PROGRESS 직접 전이를 허용
 *
 * v0.2 변경:
 * - Session 무효화 시 Request 기록 삭제 금지
 * - 실행 가능 Session Context와 과거 Result History 분리
 * - 최근 5건 표시 History 조회 API 추가
 * - GatewaySessionManager가 Session 유효성을 먼저 검증한다는 책임 경계 명시
 */

#ifndef REQUEST_MANAGER_H
#define REQUEST_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef REQUEST_MANAGER_MAX_RECORDS
#define REQUEST_MANAGER_MAX_RECORDS (16U)
#endif

#ifndef REQUEST_MANAGER_DISPLAY_HISTORY_LIMIT
#define REQUEST_MANAGER_DISPLAY_HISTORY_LIMIT (5U)
#endif

typedef enum
{
    RM_STATUS_OK = 0,
    RM_STATUS_INVALID_ARGUMENT,
    RM_STATUS_NOT_INITIALIZED,
    RM_STATUS_NOT_FOUND,
    RM_STATUS_CAPACITY_FULL,
    RM_STATUS_ID_CONFLICT,
    RM_STATUS_INVALID_TRANSITION,
    RM_STATUS_STALE_REQUEST
} RequestManager_Status_t;

typedef enum
{
    RM_REGISTER_NEW = 0,
    RM_REGISTER_DUPLICATE_SAME,
    RM_REGISTER_ID_CONFLICT,
    RM_REGISTER_STALE,
    RM_REGISTER_NO_CAPACITY
} RequestManager_RegisterResult_t;

typedef enum
{
    RM_LIFECYCLE_RECEIVED = 0,
    RM_LIFECYCLE_ACCEPTED,
    RM_LIFECYCLE_IN_PROGRESS,
    RM_LIFECYCLE_FINAL
} RequestManager_Lifecycle_t;

typedef struct
{
    uint32_t maxRequestAgeMs;

    /*
     * 내부 중복방지/조회 근거의 보존 시간.
     * MOBILE 표시 이력 5건 제한과는 별도다.
     * 0이면 시간 기반 삭제를 하지 않는다.
     */
    uint32_t retentionMs;
} RequestManager_Config_t;

typedef struct
{
    bool inUse;

    /*
     * false가 되어도 기록 자체는 유지한다.
     * 이전 Session에서 온 패킷을 다시 실행하지 않기 위한 표시다.
     */
    bool executionContextActive;

    DeviceContextId_t deviceContextId;
    DomainIf_MobileRequest_t request;

    RequestManager_Lifecycle_t lifecycle;

    bool hasVehicleResult;
    RequestResultInfo_t result;

    uint32_t receivedAtMs;
    uint32_t updatedAtMs;
    uint32_t finalizedAtMs;
} RequestManager_Record_t;

typedef struct
{
    RequestManager_RegisterResult_t registerResult;
    bool hasExistingRecord;
    RequestManager_Record_t existingRecord;
} RequestManager_RegisterInfo_t;

RequestManager_Status_t RequestManager_Init(
    const RequestManager_Config_t *config);

bool RequestManager_IsInitialized(void);

RequestManager_Status_t RequestManager_Process(uint32_t nowMs);

/**
 * @brief 재시작/세션 재수립 시 현재 실행 Context만 비활성화한다.
 *
 * History를 삭제하지 않는다.
 * 새 Session의 실제 유효성 판단은 GatewaySessionManager 책임이다.
 */
RequestManager_Status_t RequestManager_DeactivateAllExecutionContexts(
    uint32_t nowMs);

/**
 * @brief 특정 Device/Session의 실행 Context를 비활성화한다.
 *
 * 과거 Result를 보존한다.
 */
RequestManager_Status_t RequestManager_DeactivateSession(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t nowMs);

/**
 * @brief 테스트/명시적 초기화 용도.
 *
 * Runtime reconnect 처리에 사용하지 않는다.
 */
RequestManager_Status_t RequestManager_ClearAllRecords(void);

RequestManager_Status_t RequestManager_Register(
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_RegisterInfo_t *outInfo);

RequestManager_Status_t RequestManager_MarkAccepted(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs);

RequestManager_Status_t RequestManager_MarkInProgress(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs);

RequestManager_Status_t RequestManager_Finalize(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs);

RequestManager_Status_t RequestManager_MarkUnconfirmed(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs);

RequestManager_Status_t RequestManager_ApplyLateFinalResult(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs);

RequestManager_Status_t RequestManager_GetRecord(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestManager_Record_t *outRecord);

RequestManager_Status_t RequestManager_GetResult(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResultInfo_t *outResult,
    bool *outHasVehicleResult);

/**
 * @brief MOBILE 표시용 최근 Final Result를 최신순으로 최대 5건 반환.
 *
 * 이 5건 제한은 내부 중복 실행 방지 Record를 삭제하는 기준이 아니다.
 */
RequestManager_Status_t RequestManager_GetRecentFinalHistory(
    RequestManager_Record_t *outRecords,
    uint32_t capacity,
    uint32_t *outCount);

uint32_t RequestManager_GetRecordCount(void);

#ifdef __cplusplus
}
#endif

#endif /* REQUEST_MANAGER_H */
