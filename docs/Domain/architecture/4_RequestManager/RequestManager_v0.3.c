/**
 * @file RequestManager.c
 * @brief RequestManager v0.2 implementation
 */

#include "RequestManager.h"

#include <string.h>

typedef struct
{
    bool initialized;
    RequestManager_Config_t config;
    RequestManager_Record_t records[REQUEST_MANAGER_MAX_RECORDS];
} RequestManager_Context_t;

static RequestManager_Context_t g_rm;

static uint32_t Rm_ElapsedMs(uint32_t nowMs, uint32_t thenMs)
{
    return (uint32_t)(nowMs - thenMs);
}

static bool Rm_IsFinalResult(RequestResult_t result)
{
    return ((result == REQUEST_RESULT_DONE) ||
            (result == REQUEST_RESULT_REJECTED) ||
            (result == REQUEST_RESULT_CANCELLED) ||
            (result == REQUEST_RESULT_FAILED));
}

static bool Rm_IsSameKey(
    const RequestManager_Record_t *record,
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context)
{
    if ((record == NULL) || (context == NULL) || (!record->inUse))
    {
        return false;
    }

    return ((record->deviceContextId == deviceContextId) &&
            (record->request.context.sessionId == context->sessionId) &&
            (record->request.context.requestId == context->requestId));
}

static RequestManager_Record_t *Rm_FindMutable(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context)
{
    uint32_t i;

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        if (Rm_IsSameKey(&g_rm.records[i], deviceContextId, context))
        {
            return &g_rm.records[i];
        }
    }

    return NULL;
}

static const RequestManager_Record_t *Rm_FindConst(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context)
{
    uint32_t i;

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        if (Rm_IsSameKey(&g_rm.records[i], deviceContextId, context))
        {
            return &g_rm.records[i];
        }
    }

    return NULL;
}

static bool Rm_RequestPayloadEqual(
    const DomainIf_MobileRequest_t *a,
    const DomainIf_MobileRequest_t *b)
{
    if ((a == NULL) || (b == NULL) || (a->type != b->type))
    {
        return false;
    }

    switch (a->type)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
            return (a->payload.doorTarget == b->payload.doorTarget);

        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
            return (a->payload.targetTemperature ==
                    b->payload.targetTemperature);

        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
            return (a->payload.enable == b->payload.enable);

        case MOBILE_REQUEST_FAN_LEVEL:
            return (a->payload.fanLevel == b->payload.fanLevel);

        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
            return (a->payload.levelPercent ==
                    b->payload.levelPercent);

        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            return ((a->payload.color.red == b->payload.color.red) &&
                    (a->payload.color.green == b->payload.color.green) &&
                    (a->payload.color.blue == b->payload.color.blue));

        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            return (a->payload.digitalKeySetting ==
                    b->payload.digitalKeySetting);

        default:
            return false;
    }
}

static bool Rm_IsSameLogicalRequest(
    const RequestManager_Record_t *record,
    const DomainIf_MobileRequest_t *request)
{
    if ((record == NULL) || (request == NULL))
    {
        return false;
    }

    return Rm_RequestPayloadEqual(&record->request, request);
}

static RequestManager_Record_t *Rm_FindFreeRecord(void)
{
    uint32_t i;

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        if (!g_rm.records[i].inUse)
        {
            return &g_rm.records[i];
        }
    }

    return NULL;
}

static RequestManager_Record_t *Rm_FindOldestFinalRecord(void)
{
    uint32_t i;
    RequestManager_Record_t *candidate = NULL;

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        RequestManager_Record_t *record = &g_rm.records[i];

        if ((!record->inUse) ||
            (record->lifecycle != RM_LIFECYCLE_FINAL))
        {
            continue;
        }

        if ((candidate == NULL) ||
            ((int32_t)(record->finalizedAtMs -
                       candidate->finalizedAtMs) < 0))
        {
            candidate = record;
        }
    }

    return candidate;
}

static RequestManager_Record_t *Rm_AllocateRecord(void)
{
    RequestManager_Record_t *record = Rm_FindFreeRecord();

    if (record != NULL)
    {
        return record;
    }

    /*
     * 진행 중 요청은 덮어쓰지 않는다.
     * Final Record만 가장 오래된 순서로 재사용 가능하다.
     */
    return Rm_FindOldestFinalRecord();
}

static void Rm_SetVehicleResult(
    RequestManager_Record_t *record,
    RequestResult_t result,
    ResultReason_t reason,
    ResultConfirmation_t confirmation)
{
    record->result.context = record->request.context;
    record->result.result = result;
    record->result.reason = reason;
    record->result.confirmation = confirmation;
    record->hasVehicleResult = true;
}

static bool Rm_CanMoveToAccepted(
    const RequestManager_Record_t *record)
{
    return (record->executionContextActive &&
            ((record->lifecycle == RM_LIFECYCLE_RECEIVED) ||
             ((record->lifecycle == RM_LIFECYCLE_ACCEPTED) &&
              record->hasVehicleResult &&
              (record->result.result == REQUEST_RESULT_ACCEPTED))));
}

static bool Rm_CanMoveToInProgress(
    const RequestManager_Record_t *record)
{
    return (record->executionContextActive &&
            ((record->lifecycle == RM_LIFECYCLE_RECEIVED) ||
             (record->lifecycle == RM_LIFECYCLE_ACCEPTED) ||
             ((record->lifecycle == RM_LIFECYCLE_IN_PROGRESS) &&
              record->hasVehicleResult &&
              (record->result.result == REQUEST_RESULT_IN_PROGRESS))));
}

static void Rm_DeactivateRecord(
    RequestManager_Record_t *record,
    uint32_t nowMs)
{
    if ((record == NULL) || (!record->inUse))
    {
        return;
    }

    record->executionContextActive = false;
    record->updatedAtMs = nowMs;

    if ((record->lifecycle != RM_LIFECYCLE_FINAL) &&
        record->hasVehicleResult)
    {
        record->result.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    }
}

RequestManager_Status_t RequestManager_Init(
    const RequestManager_Config_t *config)
{
    (void)memset(&g_rm, 0, sizeof(g_rm));

    if (config != NULL)
    {
        g_rm.config = *config;
    }

    g_rm.initialized = true;
    return RM_STATUS_OK;
}

bool RequestManager_IsInitialized(void)
{
    return g_rm.initialized;
}

RequestManager_Status_t RequestManager_Process(uint32_t nowMs)
{
    uint32_t i;

    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    if (g_rm.config.retentionMs == 0U)
    {
        return RM_STATUS_OK;
    }

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        RequestManager_Record_t *record = &g_rm.records[i];

        if (record->inUse &&
            (record->lifecycle == RM_LIFECYCLE_FINAL) &&
            (Rm_ElapsedMs(nowMs, record->finalizedAtMs) >=
             g_rm.config.retentionMs))
        {
            (void)memset(record, 0, sizeof(*record));
        }
    }

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_DeactivateAllExecutionContexts(
    uint32_t nowMs)
{
    uint32_t i;

    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        Rm_DeactivateRecord(&g_rm.records[i], nowMs);
    }

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_DeactivateSession(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t nowMs)
{
    uint32_t i;

    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        RequestManager_Record_t *record = &g_rm.records[i];

        if (record->inUse &&
            (record->deviceContextId == deviceContextId) &&
            (record->request.context.sessionId == sessionId))
        {
            Rm_DeactivateRecord(record, nowMs);
        }
    }

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_ClearAllRecords(void)
{
    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    (void)memset(g_rm.records, 0, sizeof(g_rm.records));
    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_Register(
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_RegisterInfo_t *outInfo)
{
    RequestManager_Record_t *existing;
    RequestManager_Record_t *record;

    if ((request == NULL) || (outInfo == NULL))
    {
        return RM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    (void)memset(outInfo, 0, sizeof(*outInfo));

    /*
     * IMPORTANT:
     * GatewaySessionManager가 현재 Device/Session의 인증/유효성을
     * 먼저 검증한 뒤 이 함수를 호출해야 한다.
     */

    existing = Rm_FindMutable(
        request->deviceContextId,
        &request->context);

    if (existing != NULL)
    {
        outInfo->hasExistingRecord = true;
        outInfo->existingRecord = *existing;

        if (Rm_IsSameLogicalRequest(existing, request))
        {
            /*
             * executionContextActive=false여도 새 실행하지 않는다.
             * 과거 세션 지연 패킷은 기존 History로만 취급된다.
             */
            outInfo->registerResult = RM_REGISTER_DUPLICATE_SAME;
            return RM_STATUS_OK;
        }

        outInfo->registerResult = RM_REGISTER_ID_CONFLICT;
        return RM_STATUS_ID_CONFLICT;
    }

    if ((g_rm.config.maxRequestAgeMs > 0U) &&
        (request->meta.ageMs >= g_rm.config.maxRequestAgeMs))
    {
        outInfo->registerResult = RM_REGISTER_STALE;
        return RM_STATUS_STALE_REQUEST;
    }

    if (request->meta.quality != DATA_QUALITY_OK)
    {
        outInfo->registerResult = RM_REGISTER_STALE;
        return RM_STATUS_STALE_REQUEST;
    }

    record = Rm_AllocateRecord();

    if (record == NULL)
    {
        outInfo->registerResult = RM_REGISTER_NO_CAPACITY;
        return RM_STATUS_CAPACITY_FULL;
    }

    (void)memset(record, 0, sizeof(*record));

    record->inUse = true;
    record->executionContextActive = true;
    record->deviceContextId = request->deviceContextId;
    record->request = *request;
    record->lifecycle = RM_LIFECYCLE_RECEIVED;
    record->receivedAtMs = nowMs;
    record->updatedAtMs = nowMs;

    outInfo->registerResult = RM_REGISTER_NEW;
    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_MarkAccepted(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs)
{
    RequestManager_Record_t *record;

    if (context == NULL) return RM_STATUS_INVALID_ARGUMENT;
    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindMutable(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;
    if (!Rm_CanMoveToAccepted(record)) return RM_STATUS_INVALID_TRANSITION;

    record->lifecycle = RM_LIFECYCLE_ACCEPTED;
    Rm_SetVehicleResult(
        record,
        REQUEST_RESULT_ACCEPTED,
        RESULT_REASON_NONE,
        RESULT_CONFIRMATION_CONFIRMED);
    record->updatedAtMs = nowMs;

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_MarkInProgress(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs)
{
    RequestManager_Record_t *record;

    if (context == NULL) return RM_STATUS_INVALID_ARGUMENT;
    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindMutable(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;
    if (!Rm_CanMoveToInProgress(record)) return RM_STATUS_INVALID_TRANSITION;

    record->lifecycle = RM_LIFECYCLE_IN_PROGRESS;
    Rm_SetVehicleResult(
        record,
        REQUEST_RESULT_IN_PROGRESS,
        RESULT_REASON_NONE,
        RESULT_CONFIRMATION_CONFIRMED);
    record->updatedAtMs = nowMs;

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_Finalize(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs)
{
    RequestManager_Record_t *record;

    if ((context == NULL) || (!Rm_IsFinalResult(finalResult)))
    {
        return RM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindMutable(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;

    if (record->lifecycle == RM_LIFECYCLE_FINAL)
    {
        if (record->hasVehicleResult &&
            (record->result.result == finalResult) &&
            (record->result.reason == reason))
        {
            record->result.confirmation = RESULT_CONFIRMATION_CONFIRMED;
            record->updatedAtMs = nowMs;
            return RM_STATUS_OK;
        }

        return RM_STATUS_INVALID_TRANSITION;
    }

    /*
     * 늦게 도착한 인증된 Final Result는 History를 고칠 수 있으므로
     * executionContextActive=false여도 Finalize 자체는 허용한다.
     */
    record->lifecycle = RM_LIFECYCLE_FINAL;
    Rm_SetVehicleResult(
        record,
        finalResult,
        reason,
        RESULT_CONFIRMATION_CONFIRMED);
    record->updatedAtMs = nowMs;
    record->finalizedAtMs = nowMs;

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_MarkUnconfirmed(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    uint32_t nowMs)
{
    RequestManager_Record_t *record;

    if (context == NULL) return RM_STATUS_INVALID_ARGUMENT;
    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindMutable(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;

    if (record->hasVehicleResult)
    {
        record->result.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    }

    record->updatedAtMs = nowMs;
    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_ApplyLateFinalResult(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs)
{
    return RequestManager_Finalize(
        deviceContextId,
        context,
        finalResult,
        reason,
        nowMs);
}

RequestManager_Status_t RequestManager_GetRecord(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestManager_Record_t *outRecord)
{
    const RequestManager_Record_t *record;

    if ((context == NULL) || (outRecord == NULL))
        return RM_STATUS_INVALID_ARGUMENT;
    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindConst(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;

    *outRecord = *record;
    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_GetResult(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *context,
    RequestResultInfo_t *outResult,
    bool *outHasVehicleResult)
{
    const RequestManager_Record_t *record;

    if ((context == NULL) ||
        (outResult == NULL) ||
        (outHasVehicleResult == NULL))
    {
        return RM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_rm.initialized) return RM_STATUS_NOT_INITIALIZED;

    record = Rm_FindConst(deviceContextId, context);
    if (record == NULL) return RM_STATUS_NOT_FOUND;

    *outHasVehicleResult = record->hasVehicleResult;

    if (record->hasVehicleResult)
    {
        *outResult = record->result;
    }
    else
    {
        (void)memset(outResult, 0, sizeof(*outResult));
        outResult->context = record->request.context;
        outResult->confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    }

    return RM_STATUS_OK;
}

RequestManager_Status_t RequestManager_GetRecentFinalHistory(
    RequestManager_Record_t *outRecords,
    uint32_t capacity,
    uint32_t *outCount)
{
    bool selected[REQUEST_MANAGER_MAX_RECORDS] = { false };
    uint32_t produced = 0U;
    uint32_t limit;
    uint32_t k;

    if ((outRecords == NULL) || (outCount == NULL) || (capacity == 0U))
    {
        return RM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_rm.initialized)
    {
        return RM_STATUS_NOT_INITIALIZED;
    }

    limit = capacity;
    if (limit > REQUEST_MANAGER_DISPLAY_HISTORY_LIMIT)
    {
        limit = REQUEST_MANAGER_DISPLAY_HISTORY_LIMIT;
    }

    for (k = 0U; k < limit; ++k)
    {
        int32_t bestIndex = -1;
        uint32_t i;

        for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
        {
            const RequestManager_Record_t *record = &g_rm.records[i];

            if (selected[i] ||
                (!record->inUse) ||
                (record->lifecycle != RM_LIFECYCLE_FINAL))
            {
                continue;
            }

            if ((bestIndex < 0) ||
                ((int32_t)(record->finalizedAtMs -
                 g_rm.records[(uint32_t)bestIndex].finalizedAtMs) > 0))
            {
                bestIndex = (int32_t)i;
            }
        }

        if (bestIndex < 0)
        {
            break;
        }

        selected[(uint32_t)bestIndex] = true;
        outRecords[produced] = g_rm.records[(uint32_t)bestIndex];
        ++produced;
    }

    *outCount = produced;
    return RM_STATUS_OK;
}

uint32_t RequestManager_GetRecordCount(void)
{
    uint32_t i;
    uint32_t count = 0U;

    if (!g_rm.initialized)
    {
        return 0U;
    }

    for (i = 0U; i < REQUEST_MANAGER_MAX_RECORDS; ++i)
    {
        if (g_rm.records[i].inUse)
        {
            ++count;
        }
    }

    return count;
}
