#ifndef RESULT_MANAGER_H
#define RESULT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "command_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-owned request/result history.
 *
 * All calls must be serialized by one owner context, or protected by the
 * caller. This module never transmits a message and never calls DomainIf_*.
 */

typedef enum
{
    RESULT_MANAGER_OK = 0,
    RESULT_MANAGER_INVALID_ARGUMENT,
    RESULT_MANAGER_NOT_INITIALIZED,
    RESULT_MANAGER_CAPACITY_EXCEEDED,
    RESULT_MANAGER_NOT_FOUND,
    RESULT_MANAGER_DUPLICATE,
    RESULT_MANAGER_ID_CONFLICT,
    RESULT_MANAGER_COMMAND_CONFLICT,
    RESULT_MANAGER_KIND_MISMATCH,
    RESULT_MANAGER_INVALID_TRANSITION,
    RESULT_MANAGER_UNCONFIRMED_RECORDED,
    RESULT_MANAGER_EVIDENCE_REJECTED,
    RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED,
    RESULT_MANAGER_FINAL_CONFLICT
} ResultManager_Status_t;

typedef enum
{
    RESULT_MANAGER_TIMEOUT_NONE = 0,
    RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE,
    RESULT_MANAGER_TIMEOUT_FINAL_RESULT
} ResultManager_TimeoutState_t;

typedef enum
{
    RESULT_MANAGER_DISPATCH_UNKNOWN = 0,
    RESULT_MANAGER_NOT_DISPATCHED,
    RESULT_MANAGER_DISPATCHED
} ResultManager_DispatchState_t;

/* Domain-owned retention depth per DeviceContextId_t. */
#define RESULT_MANAGER_HISTORY_PER_DEVICE_MAX 8U

typedef struct
{
    bool firstResponseTimeoutEnabled;
    uint32_t firstResponseTimeoutMs;
    bool finalResultTimeoutEnabled;
    uint32_t finalResultTimeoutMs;
    bool doorEvidenceTtlEnabled;
    uint32_t doorEvidenceTtlMs;
} ResultManager_Policy_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    RequestContext_t context;
} ResultManager_RequestKey_t;

typedef struct
{
    ResultManager_RequestKey_t request;
    DomainIf_MobileRequestType_t requestType;
    DomainIf_MobileResultTarget_t target;
    CommandOrigin_t origin;
    uint32_t registeredAtMs;

    bool hasResult;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
    uint32_t resultUpdatedAtMs;
    bool terminal;

    bool firstResponseObserved;
    uint32_t firstResponseAtMs;
    ResultManager_TimeoutState_t timeoutState;
    bool outcomeUnconfirmed;
    bool identityConflictObserved;

    bool hasCommand;
    DomainCommandId_t commandId;
    DomainIf_BcmCommandKind_t commandKind;
    bool hasExpectedDoorTarget;
    DoorLockTarget_t expectedDoorTarget;
    ResultManager_DispatchState_t dispatchState;

    bool hasUnconfirmedBcmResult;
    DomainIf_BcmCommandResult_t unconfirmedBcmResult;
    bool hasAppliedBcmResult;
    DomainIf_BcmCommandResult_t appliedBcmResult;

    bool hasNetworkLocalFailure;
    NetworkLocalFailure_t networkLocalFailure;
    uint32_t networkLocalFailureReceivedAtMs;
} ResultManager_Snapshot_t;

typedef struct
{
    bool inUse;
    uint32_t historySequence;
    DomainIf_MobileRequest_t originalRequest;
    ResultManager_Snapshot_t snapshot;
} ResultManager_Slot_t;

typedef struct
{
    ResultManager_Slot_t *slots;
    size_t capacity;
    size_t count;
    ResultManager_Policy_t policy;
    bool initialized;
} ResultManager_t;

ResultManager_Status_t ResultManager_Init(
    ResultManager_t *manager,
    ResultManager_Slot_t *slots,
    size_t capacity,
    const ResultManager_Policy_t *policy);

/* Registers SENT/request identity without inventing a RequestResult_t value. */
ResultManager_Status_t ResultManager_RegisterRequest(
    ResultManager_t *manager,
    const DomainIf_MobileRequest_t *request,
    uint32_t registeredAtMs);

/* Domain-owned admission/rejection or evidence-backed no-command completion. */
ResultManager_Status_t ResultManager_RecordDomainResult(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    RequestResult_t result,
    ResultReason_t reason,
    bool completionEvidenceVerified,
    uint32_t nowMs);

/* Binds an already allocated CommandManager intent; no ID is generated here. */
ResultManager_Status_t ResultManager_BindCommand(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    const CommandManager_Intent_t *intent);

/* Explicit Network/adapter evidence only; local Tx acceptance is not DISPATCHED. */
ResultManager_Status_t ResultManager_RecordDispatchState(
    ResultManager_t *manager,
    DomainCommandId_t commandId,
    ResultManager_DispatchState_t dispatchState);

ResultManager_Status_t ResultManager_ApplyBcmResult(
    ResultManager_t *manager,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs);

/* Records diagnostic/unconfirmed evidence and never creates vehicle FAILED. */
ResultManager_Status_t ResultManager_RecordNetworkLocalFailure(
    ResultManager_t *manager,
    const NetworkLocalFailure_t *failure,
    uint32_t receivedAtMs);

/* Evaluates caller-supplied policy using rollover-safe uint32_t subtraction. */
ResultManager_Status_t ResultManager_EvaluateTimeout(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    uint32_t nowMs,
    ResultManager_TimeoutState_t *outNewTimeout);

ResultManager_Status_t ResultManager_GetSnapshot(
    const ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    ResultManager_Snapshot_t *outSnapshot);

/* Builds a C2 value only; it does not call DomainIf_TxMobileRequestResult(). */
ResultManager_Status_t ResultManager_GetMobileResult(
    const ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResult);

/*
 * Copies one Device's logical request records newest-registration-first.
 * A status update changes the existing record and does not change its order.
 * capacity == 0 is a count-only query and outResults may be NULL. The count is
 * at most RESULT_MANAGER_HISTORY_PER_DEVICE_MAX. On failure, caller outputs
 * are unchanged.
 */
ResultManager_Status_t ResultManager_GetRecentMobileResults(
    const ResultManager_t *manager,
    DeviceContextId_t requestingDevice,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResults,
    size_t capacity,
    size_t *outCount);

#ifdef __cplusplus
}
#endif

#endif /* RESULT_MANAGER_H */
