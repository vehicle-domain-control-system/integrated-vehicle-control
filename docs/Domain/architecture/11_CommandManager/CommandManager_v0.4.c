/**
 * @file CommandManager.c
 */

#include "CommandManager.h"

#include <stddef.h>
#include <string.h>

typedef struct
{
    bool initialized;

    DomainCommandId_t nextCommandId;
    uint32_t nextDecisionSequence;

    CommandRecord_t records[COMMAND_MANAGER_MAX_RECORDS];
} CommandManager_Context_t;

static CommandManager_Context_t g_commandManager;

static bool CommandManager_IsFinalResult(
    RequestResult_t result)
{
    return ((result == REQUEST_RESULT_DONE) ||
            (result == REQUEST_RESULT_REJECTED) ||
            (result == REQUEST_RESULT_CANCELLED) ||
            (result == REQUEST_RESULT_FAILED));
}

static CommandRecord_t *CommandManager_FindMutable(
    DomainCommandId_t commandId)
{
    uint32_t i;

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        if (g_commandManager.records[i].inUse &&
            (g_commandManager.records[i].commandId == commandId))
        {
            return &g_commandManager.records[i];
        }
    }

    return NULL;
}

static const CommandRecord_t *CommandManager_FindConst(
    DomainCommandId_t commandId)
{
    uint32_t i;

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        if (g_commandManager.records[i].inUse &&
            (g_commandManager.records[i].commandId == commandId))
        {
            return &g_commandManager.records[i];
        }
    }

    return NULL;
}

static bool CommandManager_IdInUse(
    DomainCommandId_t commandId)
{
    return (CommandManager_FindConst(commandId) != NULL);
}

static CommandManager_Status_t CommandManager_AllocateCommandId(
    DomainCommandId_t *outCommandId)
{
    uint32_t attempts = 0U;

    if (outCommandId == NULL)
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    while (attempts <= COMMAND_MANAGER_MAX_RECORDS)
    {
        DomainCommandId_t candidate =
            g_commandManager.nextCommandId;

        ++g_commandManager.nextCommandId;

        /* 0 is reserved internally as "not assigned". */
        if (g_commandManager.nextCommandId == 0U)
        {
            g_commandManager.nextCommandId = 1U;
        }

        if (candidate == 0U)
        {
            ++attempts;
            continue;
        }

        if (!CommandManager_IdInUse(candidate))
        {
            *outCommandId = candidate;
            return COMMAND_MANAGER_STATUS_OK;
        }

        ++attempts;
    }

    return COMMAND_MANAGER_STATUS_ID_EXHAUSTED;
}

static uint32_t CommandManager_AllocateDecisionSequence(void)
{
    uint32_t value = g_commandManager.nextDecisionSequence;

    ++g_commandManager.nextDecisionSequence;

    if (g_commandManager.nextDecisionSequence == 0U)
    {
        g_commandManager.nextDecisionSequence = 1U;
    }

    if (value == 0U)
    {
        value = g_commandManager.nextDecisionSequence;
        ++g_commandManager.nextDecisionSequence;

        if (g_commandManager.nextDecisionSequence == 0U)
        {
            g_commandManager.nextDecisionSequence = 1U;
        }
    }

    return value;
}

static CommandRecord_t *CommandManager_FindFree(void)
{
    uint32_t i;

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        if (!g_commandManager.records[i].inUse)
        {
            return &g_commandManager.records[i];
        }
    }

    return NULL;
}

static CommandRecord_t *CommandManager_FindOldestFinal(void)
{
    uint32_t i;
    CommandRecord_t *candidate = NULL;

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        CommandRecord_t *record =
            &g_commandManager.records[i];

        if ((!record->inUse) ||
            (record->lifecycle != COMMAND_LIFECYCLE_FINAL))
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

static CommandRecord_t *CommandManager_AllocateRecord(void)
{
    CommandRecord_t *record =
        CommandManager_FindFree();

    if (record != NULL)
    {
        return record;
    }

    /*
     * 진행 중 Command는 덮어쓰지 않는다.
     * Final record만 가장 오래된 것부터 재사용한다.
     */
    return CommandManager_FindOldestFinal();
}

static bool CommandManager_CanMoveToDispatched(
    const CommandRecord_t *record)
{
    return ((record->lifecycle == COMMAND_LIFECYCLE_CREATED) ||
            (record->lifecycle == COMMAND_LIFECYCLE_DISPATCHED));
}

static bool CommandManager_CanMoveToAccepted(
    const CommandRecord_t *record)
{
    return ((record->lifecycle == COMMAND_LIFECYCLE_DISPATCHED) ||
            (record->lifecycle == COMMAND_LIFECYCLE_ACCEPTED));
}

static bool CommandManager_CanMoveToInProgress(
    const CommandRecord_t *record)
{
    return ((record->lifecycle == COMMAND_LIFECYCLE_DISPATCHED) ||
            (record->lifecycle == COMMAND_LIFECYCLE_ACCEPTED) ||
            (record->lifecycle == COMMAND_LIFECYCLE_IN_PROGRESS));
}

CommandManager_Status_t CommandManager_Init(void)
{
    (void)memset(
        &g_commandManager,
        0,
        sizeof(g_commandManager));

    g_commandManager.nextCommandId = 1U;
    g_commandManager.nextDecisionSequence = 1U;
    g_commandManager.initialized = true;

    return COMMAND_MANAGER_STATUS_OK;
}

bool CommandManager_IsInitialized(void)
{
    return g_commandManager.initialized;
}

CommandManager_Status_t CommandManager_ClearAll(void)
{
    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    (void)memset(
        g_commandManager.records,
        0,
        sizeof(g_commandManager.records));

    g_commandManager.nextCommandId = 1U;
    g_commandManager.nextDecisionSequence = 1U;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_Create(
    const CommandCreateRequest_t *request,
    uint32_t nowMs,
    CommandCreateResult_t *outResult)
{
    CommandRecord_t *record;
    DomainCommandId_t commandId;
    CommandManager_Status_t status;

    if ((request == NULL) || (outResult == NULL))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    if (request->hasRequestContext &&
        request->hasJobContext)
    {
        /*
         * 현재 Architecture에서는 Command의 primary origin context를
         * Request 또는 Job 중 하나로 둔다.
         * 둘 다 필요해지는 실제 요구가 나오면 provenance 구조를 확장한다.
         */
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    record = CommandManager_AllocateRecord();

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_CAPACITY_FULL;
    }

    status = CommandManager_AllocateCommandId(
        &commandId);

    if (status != COMMAND_MANAGER_STATUS_OK)
    {
        return status;
    }

    (void)memset(record, 0, sizeof(*record));

    record->inUse = true;
    record->commandId = commandId;
    record->decisionSequence =
        CommandManager_AllocateDecisionSequence();

    record->target = request->target;
    record->functionId = request->functionId;
    record->type = request->type;
    record->origin = request->origin;

    record->hasRequestContext =
        request->hasRequestContext;
    if (request->hasRequestContext)
    {
        record->requestDeviceContextId =
            request->requestDeviceContextId;
        record->requestContext =
            request->requestContext;
    }

    record->hasJobContext =
        request->hasJobContext;
    if (request->hasJobContext)
    {
        record->jobId = request->jobId;
    }

    record->sourceMeta = request->sourceMeta;

    record->lifecycle = COMMAND_LIFECYCLE_CREATED;
    record->confirmation = RESULT_CONFIRMATION_UNCONFIRMED;

    record->createdAtMs = nowMs;
    record->updatedAtMs = nowMs;

    (void)memset(outResult, 0, sizeof(*outResult));

    outResult->commandId = record->commandId;
    outResult->decisionSequence =
        record->decisionSequence;

    outResult->interfaceContext.commandId =
        record->commandId;
    outResult->interfaceContext.decisionSequence =
        record->decisionSequence;
    outResult->interfaceContext.hasRequestContext =
        record->hasRequestContext;

    if (record->hasRequestContext)
    {
        outResult->interfaceContext.requestContext =
            record->requestContext;
    }

    outResult->interfaceContext.meta =
        record->sourceMeta;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_MarkDispatched(
    DomainCommandId_t commandId,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (!CommandManager_CanMoveToDispatched(record))
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_DISPATCHED;
    record->updatedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_AbortBeforeDispatch(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if ((finalResult != REQUEST_RESULT_CANCELLED) &&
        (finalResult != REQUEST_RESULT_FAILED))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (record->lifecycle != COMMAND_LIFECYCLE_CREATED)
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_FINAL;
    record->hasExecutionResult = true;
    record->executionResult = finalResult;
    record->resultReason = reason;
    record->confirmation = RESULT_CONFIRMATION_CONFIRMED;
    record->updatedAtMs = nowMs;
    record->finalizedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_CancelByPolicy(
    DomainCommandId_t commandId,
    ResultReason_t reason,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (record->hasRequestContext)
    {
        /*
         * MOBILE request lifecycle을 Feature policy가 임의 취소하지 않는다.
         */
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    if (record->lifecycle == COMMAND_LIFECYCLE_FINAL)
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_FINAL;
    record->hasExecutionResult = true;
    record->executionResult = REQUEST_RESULT_CANCELLED;
    record->resultReason = reason;
    record->confirmation = RESULT_CONFIRMATION_CONFIRMED;
    record->updatedAtMs = nowMs;
    record->finalizedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_MarkAccepted(
    DomainCommandId_t commandId,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (!CommandManager_CanMoveToAccepted(record))
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_ACCEPTED;
    record->hasExecutionResult = true;
    record->executionResult = REQUEST_RESULT_ACCEPTED;
    record->resultReason = RESULT_REASON_NONE;
    record->confirmation = RESULT_CONFIRMATION_CONFIRMED;
    record->updatedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_MarkInProgress(
    DomainCommandId_t commandId,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (!CommandManager_CanMoveToInProgress(record))
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_IN_PROGRESS;
    record->hasExecutionResult = true;
    record->executionResult = REQUEST_RESULT_IN_PROGRESS;
    record->resultReason = RESULT_REASON_NONE;
    record->confirmation = RESULT_CONFIRMATION_CONFIRMED;
    record->updatedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_Finalize(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!CommandManager_IsFinalResult(finalResult))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (record->lifecycle == COMMAND_LIFECYCLE_FINAL)
    {
        if (record->hasExecutionResult &&
            (record->executionResult == finalResult) &&
            (record->resultReason == reason))
        {
            record->confirmation =
                RESULT_CONFIRMATION_CONFIRMED;
            record->updatedAtMs = nowMs;
            return COMMAND_MANAGER_STATUS_OK;
        }

        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    /*
     * ECU는 빠르게 REJECTED/FAILED/DONE을 반환할 수 있으므로
     * DISPATCHED에서 ACCEPTED/IN_PROGRESS를 생략한 Final도 허용한다.
     *
     * CREATED 상태에서 Final은 아직 실제 전송되지 않은 Command이므로
     * 허용하지 않는다.
     */
    if (record->lifecycle == COMMAND_LIFECYCLE_CREATED)
    {
        return COMMAND_MANAGER_STATUS_INVALID_TRANSITION;
    }

    record->lifecycle = COMMAND_LIFECYCLE_FINAL;
    record->hasExecutionResult = true;
    record->executionResult = finalResult;
    record->resultReason = reason;
    record->confirmation = RESULT_CONFIRMATION_CONFIRMED;
    record->updatedAtMs = nowMs;
    record->finalizedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_MarkUnconfirmed(
    DomainCommandId_t commandId,
    uint32_t nowMs)
{
    CommandRecord_t *record;

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindMutable(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    if (record->hasExecutionResult)
    {
        record->confirmation =
            RESULT_CONFIRMATION_UNCONFIRMED;
    }

    record->updatedAtMs = nowMs;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_GetRecord(
    DomainCommandId_t commandId,
    CommandRecord_t *outRecord)
{
    const CommandRecord_t *record;

    if (outRecord == NULL)
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindConst(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    *outRecord = *record;
    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_GetRequestContext(
    DomainCommandId_t commandId,
    RequestContext_t *outContext,
    bool *outHasContext)
{
    const CommandRecord_t *record;

    if ((outContext == NULL) ||
        (outHasContext == NULL))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindConst(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    *outHasContext = record->hasRequestContext;

    if (record->hasRequestContext)
    {
        *outContext = record->requestContext;
    }
    else
    {
        (void)memset(outContext, 0, sizeof(*outContext));
    }

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_GetRequestProvenance(
    DomainCommandId_t commandId,
    DeviceContextId_t *outDeviceContextId,
    RequestContext_t *outContext,
    bool *outHasContext)
{
    const CommandRecord_t *record;

    if ((outDeviceContextId == NULL) ||
        (outContext == NULL) ||
        (outHasContext == NULL))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindConst(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    *outHasContext = record->hasRequestContext;

    if (record->hasRequestContext)
    {
        *outDeviceContextId =
            record->requestDeviceContextId;
        *outContext = record->requestContext;
    }
    else
    {
        *outDeviceContextId = 0U;
        (void)memset(outContext, 0, sizeof(*outContext));
    }

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_GetJobContext(
    DomainCommandId_t commandId,
    DomainJobId_t *outJobId,
    bool *outHasContext)
{
    const CommandRecord_t *record;

    if ((outJobId == NULL) ||
        (outHasContext == NULL))
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    record = CommandManager_FindConst(commandId);

    if (record == NULL)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    *outHasContext = record->hasJobContext;
    *outJobId = record->hasJobContext ?
        record->jobId : 0U;

    return COMMAND_MANAGER_STATUS_OK;
}

CommandManager_Status_t CommandManager_GetLatestDecisionSequence(
    CommandTarget_t target,
    DomainFunctionId_t functionId,
    uint32_t *outSequence)
{
    uint32_t i;
    bool found = false;
    uint32_t latest = 0U;

    if (outSequence == NULL)
    {
        return COMMAND_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_commandManager.initialized)
    {
        return COMMAND_MANAGER_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        const CommandRecord_t *record =
            &g_commandManager.records[i];

        if ((!record->inUse) ||
            (record->target != target) ||
            (record->functionId != functionId))
        {
            continue;
        }

        if ((!found) ||
            ((int32_t)(record->decisionSequence - latest) > 0))
        {
            latest = record->decisionSequence;
            found = true;
        }
    }

    if (!found)
    {
        return COMMAND_MANAGER_STATUS_NOT_FOUND;
    }

    *outSequence = latest;
    return COMMAND_MANAGER_STATUS_OK;
}

uint32_t CommandManager_GetRecordCount(void)
{
    uint32_t i;
    uint32_t count = 0U;

    if (!g_commandManager.initialized)
    {
        return 0U;
    }

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        if (g_commandManager.records[i].inUse)
        {
            ++count;
        }
    }

    return count;
}

uint32_t CommandManager_GetActiveCount(void)
{
    uint32_t i;
    uint32_t count = 0U;

    if (!g_commandManager.initialized)
    {
        return 0U;
    }

    for (i = 0U; i < COMMAND_MANAGER_MAX_RECORDS; ++i)
    {
        if (g_commandManager.records[i].inUse &&
            (g_commandManager.records[i].lifecycle !=
             COMMAND_LIFECYCLE_FINAL))
        {
            ++count;
        }
    }

    return count;
}
