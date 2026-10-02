/**
 * @file ResultManager.c
 */

#include "ResultManager.h"

#include <stddef.h>
#include <string.h>

static bool g_resultManagerInitialized = false;

static bool ResultManager_IsFinal(
    RequestResult_t result)
{
    return ((result == REQUEST_RESULT_DONE) ||
            (result == REQUEST_RESULT_REJECTED) ||
            (result == REQUEST_RESULT_CANCELLED) ||
            (result == REQUEST_RESULT_FAILED));
}

static void ResultManager_ClearOutput(
    ResultManager_Output_t *outResult)
{
    (void)memset(outResult, 0, sizeof(*outResult));
    outResult->confirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;
    outResult->route = RESULT_ROUTE_NONE;
    outResult->completionEvidence =
        RESULT_EVIDENCE_NONE;
}

static ResultManager_Status_t ResultManager_LoadCommandRoute(
    const CommandRecord_t *command,
    ResultManager_Output_t *outResult)
{
    if ((command == NULL) || (outResult == NULL))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    outResult->commandId = command->commandId;
    outResult->origin = command->origin;
    outResult->commandType = command->type;
    outResult->functionId = command->functionId;

    outResult->hasRequestContext =
        command->hasRequestContext;

    if (command->hasRequestContext)
    {
        outResult->route = RESULT_ROUTE_REQUEST;
        outResult->requestDeviceContextId =
            command->requestDeviceContextId;
        outResult->requestContext =
            command->requestContext;
    }

    outResult->hasJobContext =
        command->hasJobContext;

    if (command->hasJobContext)
    {
        outResult->route = RESULT_ROUTE_JOB;
        outResult->jobId = command->jobId;
    }

    return RESULT_MANAGER_STATUS_OK;
}

static ResultManager_Status_t ResultManager_UpdateRequest(
    const CommandRecord_t *command,
    RequestResult_t result,
    ResultReason_t reason,
    uint32_t nowMs,
    bool *outAdvanced)
{
    RequestManager_Record_t requestRecord;
    RequestManager_Status_t requestStatus;

    if ((command == NULL) ||
        (outAdvanced == NULL))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    *outAdvanced = false;

    if (!command->hasRequestContext)
    {
        return RESULT_MANAGER_STATUS_OK;
    }

    requestStatus = RequestManager_GetRecord(
        command->requestDeviceContextId,
        &command->requestContext,
        &requestRecord);

    if (requestStatus != RM_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_REQUEST_LINK_ERROR;
    }

    /*
     * 늦은 중간 결과로 상태를 뒤로 돌리지 않는다.
     */
    if (result == REQUEST_RESULT_ACCEPTED)
    {
        if ((requestRecord.lifecycle ==
             RM_LIFECYCLE_IN_PROGRESS) ||
            (requestRecord.lifecycle ==
             RM_LIFECYCLE_FINAL))
        {
            return RESULT_MANAGER_STATUS_OK;
        }

        requestStatus = RequestManager_MarkAccepted(
            command->requestDeviceContextId,
            &command->requestContext,
            nowMs);
    }
    else if (result == REQUEST_RESULT_IN_PROGRESS)
    {
        if (requestRecord.lifecycle ==
            RM_LIFECYCLE_FINAL)
        {
            return RESULT_MANAGER_STATUS_OK;
        }

        requestStatus = RequestManager_MarkInProgress(
            command->requestDeviceContextId,
            &command->requestContext,
            nowMs);
    }
    else if (ResultManager_IsFinal(result))
    {
        requestStatus = RequestManager_Finalize(
            command->requestDeviceContextId,
            &command->requestContext,
            result,
            reason,
            nowMs);
    }
    else
    {
        return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
    }

    if (requestStatus == RM_STATUS_OK)
    {
        *outAdvanced = true;
        return RESULT_MANAGER_STATUS_OK;
    }

    if (requestStatus == RM_STATUS_INVALID_TRANSITION)
    {
        return RESULT_MANAGER_STATUS_INVALID_TRANSITION;
    }

    return RESULT_MANAGER_STATUS_REQUEST_LINK_ERROR;
}

static ResultManager_Status_t ResultManager_ApplyTrustedResult(
    DomainCommandId_t commandId,
    RequestResult_t result,
    ResultReason_t reason,
    ResultCompletionEvidence_t evidence,
    uint32_t nowMs,
    ResultManager_Output_t *outResult)
{
    CommandRecord_t before;
    CommandRecord_t after;
    CommandManager_Status_t commandStatus;
    ResultManager_Status_t requestLinkStatus;
    bool requestAdvanced = false;
    bool commandAdvanced = false;

    commandStatus = CommandManager_GetRecord(
        commandId,
        &before);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_COMMAND_NOT_FOUND;
    }

    /*
     * Command state monotonicity:
     * - late ACCEPTED after IN_PROGRESS/FINAL -> ignore
     * - late IN_PROGRESS after FINAL -> ignore
     * - same final repeat -> idempotent
     */
    if (result == REQUEST_RESULT_ACCEPTED)
    {
        if ((before.lifecycle ==
             COMMAND_LIFECYCLE_IN_PROGRESS) ||
            (before.lifecycle ==
             COMMAND_LIFECYCLE_FINAL))
        {
            commandStatus = COMMAND_MANAGER_STATUS_OK;
        }
        else
        {
            commandStatus = CommandManager_MarkAccepted(
                commandId,
                nowMs);
            commandAdvanced =
                (commandStatus == COMMAND_MANAGER_STATUS_OK);
        }
    }
    else if (result == REQUEST_RESULT_IN_PROGRESS)
    {
        if (before.lifecycle ==
            COMMAND_LIFECYCLE_FINAL)
        {
            commandStatus = COMMAND_MANAGER_STATUS_OK;
        }
        else
        {
            commandStatus = CommandManager_MarkInProgress(
                commandId,
                nowMs);
            commandAdvanced =
                (commandStatus == COMMAND_MANAGER_STATUS_OK);
        }
    }
    else if (ResultManager_IsFinal(result))
    {
        commandStatus = CommandManager_Finalize(
            commandId,
            result,
            reason,
            nowMs);

        if (commandStatus == COMMAND_MANAGER_STATUS_OK)
        {
            /*
             * 동일 Final repeat도 OK를 반환할 수 있으므로
             * 실제 advancement는 before 상태로 판정.
             */
            commandAdvanced =
                (before.lifecycle != COMMAND_LIFECYCLE_FINAL);
        }
    }
    else
    {
        return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
    }

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_INVALID_TRANSITION;
    }

    commandStatus = CommandManager_GetRecord(
        commandId,
        &after);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_COMMAND_NOT_FOUND;
    }

    requestLinkStatus = ResultManager_UpdateRequest(
        &after,
        result,
        reason,
        nowMs,
        &requestAdvanced);

    if (requestLinkStatus != RESULT_MANAGER_STATUS_OK)
    {
        return requestLinkStatus;
    }

    ResultManager_ClearOutput(outResult);
    (void)ResultManager_LoadCommandRoute(
        &after,
        outResult);

    outResult->result = result;
    outResult->reason = reason;
    outResult->confirmation =
        RESULT_CONFIRMATION_CONFIRMED;
    outResult->completionEvidence = evidence;
    outResult->stateAdvanced =
        (commandAdvanced || requestAdvanced);

    return RESULT_MANAGER_STATUS_OK;
}

static bool ResultManager_IsTrustedMeta(
    const SignalMeta_t *meta)
{
    return ((meta != NULL) &&
            (meta->quality == DATA_QUALITY_OK));
}

static ResultManager_Status_t ResultManager_CheckBcmConsistency(
    const DomainIf_BcmEvent_t *event,
    ResultCompletionEvidence_t *outEvidence)
{
    if ((event == NULL) || (outEvidence == NULL))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    *outEvidence = RESULT_EVIDENCE_NONE;

    switch (event->eventType)
    {
        case BCM_EVENT_LOCK_COMPLETED:
        case BCM_EVENT_UNLOCK_COMPLETED:
            if (event->result != REQUEST_RESULT_DONE)
            {
                return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
            }

            *outEvidence =
                RESULT_EVIDENCE_TARGET_TRANSITION_CONFIRMED;
            return RESULT_MANAGER_STATUS_OK;

        case BCM_EVENT_ALREADY_AT_TARGET:
            if (event->result != REQUEST_RESULT_DONE)
            {
                return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
            }

            *outEvidence =
                RESULT_EVIDENCE_ALREADY_AT_TARGET;
            return RESULT_MANAGER_STATUS_OK;

        case BCM_EVENT_REQUEST_REJECTED:
            if (event->result != REQUEST_RESULT_REJECTED)
            {
                return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
            }

            *outEvidence =
                RESULT_EVIDENCE_EXECUTION_RESULT;
            return RESULT_MANAGER_STATUS_OK;

        case BCM_EVENT_REQUEST_FAILED:
            if (event->result != REQUEST_RESULT_FAILED)
            {
                return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
            }

            *outEvidence =
                RESULT_EVIDENCE_EXECUTION_RESULT;
            return RESULT_MANAGER_STATUS_OK;

        case BCM_EVENT_OVERHEAT_DETECTED:
        case BCM_EVENT_FAN_MISMATCH_DETECTED:
        case BCM_EVENT_RECOVERY_CONFIRMED:
        default:
            /*
             * Diagnostic/Protection event.
             * Command result로 추정하지 않는다.
             */
            return RESULT_MANAGER_STATUS_INCONSISTENT_REPORT;
    }
}

ResultManager_Status_t ResultManager_Init(void)
{
    if ((!CommandManager_IsInitialized()) ||
        (!RequestManager_IsInitialized()))
    {
        /*
         * ResultManager는 두 저장소 위의 router이므로
         * dependency 초기화 후 시작해야 한다.
         */
        return RESULT_MANAGER_STATUS_NOT_INITIALIZED;
    }

    g_resultManagerInitialized = true;
    return RESULT_MANAGER_STATUS_OK;
}

bool ResultManager_IsInitialized(void)
{
    return g_resultManagerInitialized;
}

ResultManager_Status_t ResultManager_ProcessBcmEvent(
    const DomainIf_BcmEvent_t *event,
    uint32_t nowMs,
    ResultManager_Output_t *outResult)
{
    ResultCompletionEvidence_t evidence;
    ResultManager_Status_t status;

    if ((event == NULL) || (outResult == NULL))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_resultManagerInitialized)
    {
        return RESULT_MANAGER_STATUS_NOT_INITIALIZED;
    }

    ResultManager_ClearOutput(outResult);

    if (!ResultManager_IsTrustedMeta(&event->meta))
    {
        (void)ResultManager_MarkCommandUnconfirmed(
            event->commandId,
            nowMs,
            outResult);

        return RESULT_MANAGER_STATUS_UNTRUSTED_REPORT;
    }

    status = ResultManager_CheckBcmConsistency(
        event,
        &evidence);

    if (status != RESULT_MANAGER_STATUS_OK)
    {
        return status;
    }

    return ResultManager_ApplyTrustedResult(
        event->commandId,
        event->result,
        event->reason,
        evidence,
        nowMs,
        outResult);
}

ResultManager_Status_t ResultManager_ProcessWindowResult(
    const DomainIf_WindowCommandResult_t *result,
    uint32_t nowMs,
    ResultManager_Output_t *outResult)
{
    if ((result == NULL) || (outResult == NULL))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_resultManagerInitialized)
    {
        return RESULT_MANAGER_STATUS_NOT_INITIALIZED;
    }

    ResultManager_ClearOutput(outResult);

    if (!ResultManager_IsTrustedMeta(&result->meta))
    {
        (void)ResultManager_MarkCommandUnconfirmed(
            result->commandId,
            nowMs,
            outResult);

        return RESULT_MANAGER_STATUS_UNTRUSTED_REPORT;
    }

    return ResultManager_ApplyTrustedResult(
        result->commandId,
        result->result,
        result->reason,
        RESULT_EVIDENCE_EXECUTION_RESULT,
        nowMs,
        outResult);
}


ResultManager_Status_t ResultManager_ConfirmObservedState(
    DomainCommandId_t commandId,
    RequestResult_t finalResult,
    ResultReason_t reason,
    uint32_t nowMs,
    ResultManager_Output_t *outResult)
{
    if (outResult == NULL)
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_resultManagerInitialized)
    {
        return RESULT_MANAGER_STATUS_NOT_INITIALIZED;
    }

    if (!ResultManager_IsFinal(finalResult))
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    return ResultManager_ApplyTrustedResult(
        commandId,
        finalResult,
        reason,
        RESULT_EVIDENCE_OBSERVED_STATE,
        nowMs,
        outResult);
}

ResultManager_Status_t ResultManager_MarkCommandUnconfirmed(
    DomainCommandId_t commandId,
    uint32_t nowMs,
    ResultManager_Output_t *outResult)
{
    CommandRecord_t command;
    CommandManager_Status_t commandStatus;

    if (outResult == NULL)
    {
        return RESULT_MANAGER_STATUS_INVALID_ARGUMENT;
    }

    if (!g_resultManagerInitialized)
    {
        return RESULT_MANAGER_STATUS_NOT_INITIALIZED;
    }

    ResultManager_ClearOutput(outResult);

    commandStatus = CommandManager_GetRecord(
        commandId,
        &command);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_COMMAND_NOT_FOUND;
    }

    commandStatus = CommandManager_MarkUnconfirmed(
        commandId,
        nowMs);

    if (commandStatus != COMMAND_MANAGER_STATUS_OK)
    {
        return RESULT_MANAGER_STATUS_INVALID_TRANSITION;
    }

    if (command.hasRequestContext)
    {
        RequestManager_Status_t requestStatus =
            RequestManager_MarkUnconfirmed(
                command.requestDeviceContextId,
                &command.requestContext,
                nowMs);

        if ((requestStatus != RM_STATUS_OK) &&
            (requestStatus != RM_STATUS_NOT_FOUND))
        {
            return RESULT_MANAGER_STATUS_REQUEST_LINK_ERROR;
        }
    }

    (void)ResultManager_LoadCommandRoute(
        &command,
        outResult);

    if (command.hasExecutionResult)
    {
        outResult->result = command.executionResult;
        outResult->reason = command.resultReason;
    }

    outResult->confirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;
    outResult->completionEvidence =
        RESULT_EVIDENCE_NONE;
    outResult->stateAdvanced = false;

    return RESULT_MANAGER_STATUS_OK;
}
