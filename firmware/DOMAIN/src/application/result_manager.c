#include "result_manager.h"

#include <string.h>

static bool ResultManager_IsInitialized(const ResultManager_t *manager)
{
    return (manager != NULL) && manager->initialized &&
           (manager->slots != NULL) && (manager->capacity > 0U);
}

static bool ResultManager_KeyEquals(
    const ResultManager_RequestKey_t *left,
    const ResultManager_RequestKey_t *right)
{
    return (left->deviceContextId == right->deviceContextId) &&
           (left->context.sessionId == right->context.sessionId) &&
           (left->context.requestId == right->context.requestId);
}

static ResultManager_RequestKey_t ResultManager_KeyFromRequest(
    const DomainIf_MobileRequest_t *request)
{
    ResultManager_RequestKey_t key;

    key.deviceContextId = request->deviceContextId;
    key.context = request->context;
    return key;
}

static ResultManager_Slot_t *ResultManager_FindByRequest(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            ResultManager_KeyEquals(&manager->slots[index].snapshot.request,
                                    request))
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static const ResultManager_Slot_t *ResultManager_FindByRequestConst(
    const ResultManager_t *manager,
    const ResultManager_RequestKey_t *request)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            ResultManager_KeyEquals(&manager->slots[index].snapshot.request,
                                    request))
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static ResultManager_Slot_t *ResultManager_FindByCommand(
    ResultManager_t *manager,
    DomainCommandId_t commandId)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            manager->slots[index].snapshot.hasCommand &&
            (manager->slots[index].snapshot.commandId == commandId))
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static ResultManager_Slot_t *ResultManager_FindFree(ResultManager_t *manager)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (!manager->slots[index].inUse)
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static bool ResultManager_IsSequenceNewer(uint32_t left, uint32_t right)
{
    uint32_t delta = left - right;

    return (delta != 0U) && (delta < UINT32_C(0x80000000));
}

static size_t ResultManager_CountForDevice(
    const ResultManager_t *manager,
    DeviceContextId_t deviceContextId)
{
    size_t count = 0U;
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            (manager->slots[index].snapshot.request.deviceContextId ==
             deviceContextId))
        {
            ++count;
        }
    }
    return count;
}

static ResultManager_Slot_t *ResultManager_FindOldestForDevice(
    ResultManager_t *manager,
    DeviceContextId_t deviceContextId)
{
    ResultManager_Slot_t *oldest = NULL;
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        ResultManager_Slot_t *candidate = &manager->slots[index];

        if (!candidate->inUse ||
            (candidate->snapshot.request.deviceContextId != deviceContextId))
        {
            continue;
        }
        if ((oldest == NULL) ||
            ResultManager_IsSequenceNewer(oldest->historySequence,
                                          candidate->historySequence))
        {
            oldest = candidate;
        }
    }
    return oldest;
}

static const ResultManager_Slot_t *ResultManager_FindNewestForDevice(
    const ResultManager_t *manager,
    DeviceContextId_t deviceContextId)
{
    const ResultManager_Slot_t *newest = NULL;
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        const ResultManager_Slot_t *candidate = &manager->slots[index];

        if (!candidate->inUse ||
            (candidate->snapshot.request.deviceContextId != deviceContextId))
        {
            continue;
        }
        if ((newest == NULL) ||
            ResultManager_IsSequenceNewer(candidate->historySequence,
                                          newest->historySequence))
        {
            newest = candidate;
        }
    }
    return newest;
}

static bool ResultManager_IsQualityValid(DataQuality_t quality)
{
    return (quality == DATA_QUALITY_OK) ||
           (quality == DATA_QUALITY_STALE) ||
           (quality == DATA_QUALITY_INVALID) ||
           (quality == DATA_QUALITY_NO_DATA);
}

static bool ResultManager_IsRequestTypeValid(DomainIf_MobileRequestType_t type)
{
    switch (type)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_FAN_LEVEL:
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            return true;
        default:
            return false;
    }
}

static bool ResultManager_IsResultValid(RequestResult_t result)
{
    switch (result)
    {
        case REQUEST_RESULT_ACCEPTED:
        case REQUEST_RESULT_IN_PROGRESS:
        case REQUEST_RESULT_DONE:
        case REQUEST_RESULT_REJECTED:
        case REQUEST_RESULT_CANCELLED:
        case REQUEST_RESULT_FAILED:
            return true;
        default:
            return false;
    }
}

static bool ResultManager_IsReasonValid(ResultReason_t reason)
{
    switch (reason)
    {
        case RESULT_REASON_NONE:
        case RESULT_REASON_LOCAL_OVERRIDE:
        case RESULT_REASON_STOP_REQUESTED:
        case RESULT_REASON_SUPERSEDED:
        case RESULT_REASON_ANTIPINCH:
        case RESULT_REASON_NOT_ALLOWED:
        case RESULT_REASON_INVALID_COMMAND:
        case RESULT_REASON_STATE_UNTRUSTED:
        case RESULT_REASON_NO_FEEDBACK:
        case RESULT_REASON_DRIVE_LIMIT_EXCEEDED:
        case RESULT_REASON_MOTOR_FAULT:
        case RESULT_REASON_SENSOR_FAULT:
        case RESULT_REASON_TIMEOUT:
        case RESULT_REASON_OTHER:
        case RESULT_REASON_NOT_REGISTERED:
        case RESULT_REASON_LINK_LOST:
        case RESULT_REASON_SESSION_INVALID:
        case RESULT_REASON_DOOR_OPEN:
        case RESULT_REASON_ALREADY_AT_TARGET:
        case RESULT_REASON_FAULT:
        case RESULT_REASON_STALE:
        case RESULT_REASON_NO_DATA:
        case RESULT_REASON_NOT_READY:
        case RESULT_REASON_OUT_OF_RANGE:
        case RESULT_REASON_SOURCE_FAILURE:
        case RESULT_REASON_UNKNOWN_REQUEST:
        case RESULT_REASON_EXPIRED:
        case RESULT_REASON_ID_CONFLICT:
        case RESULT_REASON_POWER_NOT_ALLOWED:
        case RESULT_REASON_OVERHEAT:
        case RESULT_REASON_FAN_MISMATCH:
            return true;
        default:
            return false;
    }
}

static bool ResultManager_IsConfirmationValid(ResultConfirmation_t confirmation)
{
    return (confirmation == RESULT_CONFIRMATION_UNCONFIRMED) ||
           (confirmation == RESULT_CONFIRMATION_CONFIRMED);
}

static bool ResultManager_IsBcmKindValid(DomainIf_BcmCommandKind_t kind)
{
    return (kind == BCM_COMMAND_KIND_DOOR) ||
           (kind == BCM_COMMAND_KIND_CLIMATE) ||
           (kind == BCM_COMMAND_KIND_INTERIOR_LIGHT);
}

static bool ResultManager_IsTerminal(RequestResult_t result)
{
    return (result == REQUEST_RESULT_DONE) ||
           (result == REQUEST_RESULT_REJECTED) ||
           (result == REQUEST_RESULT_CANCELLED) ||
           (result == REQUEST_RESULT_FAILED);
}

static bool ResultManager_IsIntermediate(RequestResult_t result)
{
    return (result == REQUEST_RESULT_ACCEPTED) ||
           (result == REQUEST_RESULT_IN_PROGRESS);
}

static bool ResultManager_IsPayloadValid(const DomainIf_MobileRequest_t *request)
{
    switch (request->type)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
            return (request->payload.doorTarget == DOOR_TARGET_LOCK) ||
                   (request->payload.doorTarget == DOOR_TARGET_UNLOCK);
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            return true;
        case MOBILE_REQUEST_FAN_LEVEL:
            switch (request->payload.fanLevel)
            {
                case FAN_LEVEL_OFF:
                case FAN_LEVEL_LOW:
                case FAN_LEVEL_MEDIUM:
                case FAN_LEVEL_HIGH:
                    return true;
                case FAN_LEVEL_UNKNOWN:
                default:
                    return false;
            }
        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
            return request->payload.levelPercent <= 100U;
        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            return (request->payload.digitalKeySetting == DIGITAL_KEY_SETTING_OFF) ||
                   (request->payload.digitalKeySetting == DIGITAL_KEY_SETTING_ON);
        default:
            return false;
    }
}

static bool ResultManager_RequestContentEquals(
    const DomainIf_MobileRequest_t *left,
    const DomainIf_MobileRequest_t *right)
{
    if (left->type != right->type)
    {
        return false;
    }

    switch (left->type)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
            return left->payload.doorTarget == right->payload.doorTarget;
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
            return left->payload.targetTemperature ==
                   right->payload.targetTemperature;
        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
            return left->payload.enable == right->payload.enable;
        case MOBILE_REQUEST_FAN_LEVEL:
            return left->payload.fanLevel == right->payload.fanLevel;
        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
            return left->payload.levelPercent == right->payload.levelPercent;
        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            return (left->payload.color.red == right->payload.color.red) &&
                   (left->payload.color.green == right->payload.color.green) &&
                   (left->payload.color.blue == right->payload.color.blue);
        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            return left->payload.digitalKeySetting ==
                   right->payload.digitalKeySetting;
        default:
            return false;
    }
}

static DomainIf_MobileResultTarget_t ResultManager_TargetForRequest(
    DomainIf_MobileRequestType_t type)
{
    return (type == MOBILE_REQUEST_DIGITAL_KEY_SETTING)
               ? MOBILE_RESULT_TARGET_DOMAIN_SETTING
               : MOBILE_RESULT_TARGET_BCM;
}

static bool ResultManager_MapCommandKind(
    CommandManager_CommandKind_t source,
    DomainIf_BcmCommandKind_t *outKind)
{
    switch (source)
    {
        case COMMAND_MANAGER_KIND_DOOR:
            *outKind = BCM_COMMAND_KIND_DOOR;
            return true;
        case COMMAND_MANAGER_KIND_CLIMATE:
            *outKind = BCM_COMMAND_KIND_CLIMATE;
            return true;
        case COMMAND_MANAGER_KIND_INTERIOR_LIGHT:
            *outKind = BCM_COMMAND_KIND_INTERIOR_LIGHT;
            return true;
        default:
            return false;
    }
}

static bool ResultManager_RequestAcceptsCommandKind(
    DomainIf_MobileRequestType_t requestType,
    DomainIf_BcmCommandKind_t commandKind)
{
    switch (requestType)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
            return commandKind == BCM_COMMAND_KIND_DOOR;
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_FAN_LEVEL:
            return commandKind == BCM_COMMAND_KIND_CLIMATE;
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            return commandKind == BCM_COMMAND_KIND_INTERIOR_LIGHT;
        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
        default:
            return false;
    }
}

static bool ResultManager_MapDoorTarget(
    CommandManager_DoorTarget_t source,
    DoorLockTarget_t *outTarget)
{
    switch (source)
    {
        case COMMAND_MANAGER_DOOR_TARGET_LOCK:
            *outTarget = DOOR_TARGET_LOCK;
            return true;
        case COMMAND_MANAGER_DOOR_TARGET_UNLOCK:
            *outTarget = DOOR_TARGET_UNLOCK;
            return true;
        default:
            return false;
    }
}

static bool ResultManager_ResultReasonCombinationValid(
    RequestResult_t result,
    ResultReason_t reason)
{
    if ((result == REQUEST_RESULT_ACCEPTED) ||
        (result == REQUEST_RESULT_IN_PROGRESS))
    {
        return reason == RESULT_REASON_NONE;
    }
    if (result == REQUEST_RESULT_DONE)
    {
        return (reason == RESULT_REASON_NONE) ||
               (reason == RESULT_REASON_ALREADY_AT_TARGET);
    }
    return reason != RESULT_REASON_NONE;
}

static bool ResultManager_DoorEvidenceEnumValid(
    const DomainIf_BcmDoorResultEvidence_t *evidence)
{
    bool typeValid;

    switch (evidence->type)
    {
        case BCM_DOOR_EVIDENCE_LOCK_COMPLETED:
        case BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED:
        case BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET:
        case BCM_DOOR_EVIDENCE_REQUEST_REJECTED:
        case BCM_DOOR_EVIDENCE_REQUEST_FAILED:
            typeValid = true;
            break;
        default:
            typeValid = false;
            break;
    }
    return typeValid &&
           ((evidence->target == DOOR_TARGET_LOCK) ||
            (evidence->target == DOOR_TARGET_UNLOCK)) &&
           ResultManager_IsQualityValid(evidence->quality);
}

static bool ResultManager_DoorEvidenceMatches(
    const ResultManager_t *manager,
    const ResultManager_Snapshot_t *snapshot,
    const DomainIf_BcmCommandResult_t *result)
{
    const DomainIf_BcmDoorResultEvidence_t *evidence;

    if (!result->hasDoorEvidence)
    {
        return result->result != REQUEST_RESULT_DONE;
    }
    if (!snapshot->hasExpectedDoorTarget)
    {
        return false;
    }

    evidence = &result->doorEvidence;
    if (!ResultManager_DoorEvidenceEnumValid(evidence) ||
        (evidence->target != snapshot->expectedDoorTarget) ||
        (evidence->quality != DATA_QUALITY_OK))
    {
        return false;
    }
    if (manager->policy.doorEvidenceTtlEnabled &&
        (evidence->ageMs >= manager->policy.doorEvidenceTtlMs))
    {
        return false;
    }

    switch (result->result)
    {
        case REQUEST_RESULT_DONE:
            if (evidence->type == BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET)
            {
                return true;
            }
            return ((evidence->target == DOOR_TARGET_LOCK) &&
                    (evidence->type == BCM_DOOR_EVIDENCE_LOCK_COMPLETED)) ||
                   ((evidence->target == DOOR_TARGET_UNLOCK) &&
                    (evidence->type == BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED));
        case REQUEST_RESULT_REJECTED:
            return evidence->type == BCM_DOOR_EVIDENCE_REQUEST_REJECTED;
        case REQUEST_RESULT_FAILED:
            return evidence->type == BCM_DOOR_EVIDENCE_REQUEST_FAILED;
        default:
            return false;
    }
}

static bool ResultManager_BcmResultEquals(
    const DomainIf_BcmCommandResult_t *left,
    const DomainIf_BcmCommandResult_t *right)
{
    if ((left->commandId != right->commandId) ||
        (left->kind != right->kind) ||
        (left->result != right->result) ||
        (left->reason != right->reason) ||
        (left->confirmation != right->confirmation) ||
        (left->hasDoorEvidence != right->hasDoorEvidence))
    {
        return false;
    }
    if (!left->hasDoorEvidence)
    {
        return true;
    }
    return (left->doorEvidence.type == right->doorEvidence.type) &&
           (left->doorEvidence.occurrenceId == right->doorEvidence.occurrenceId) &&
           (left->doorEvidence.target == right->doorEvidence.target) &&
           (left->doorEvidence.quality == right->doorEvidence.quality) &&
           (left->doorEvidence.ageMs == right->doorEvidence.ageMs);
}

static bool ResultManager_NetworkFailureValid(
    const NetworkLocalFailure_t *failure)
{
    bool linkValid;
    bool itemKindValid;
    bool reasonValid;

    switch (failure->link)
    {
        case NETWORK_LINK_CAN:
        case NETWORK_LINK_UART_M:
        case NETWORK_LINK_UART_C:
            linkValid = true;
            break;
        default:
            linkValid = false;
            break;
    }
    switch (failure->itemKind)
    {
        case NETWORK_LOCAL_ITEM_COMMAND:
        case NETWORK_LOCAL_ITEM_REQUEST_RESULT:
        case NETWORK_LOCAL_ITEM_OCCURRENCE:
            itemKindValid = true;
            break;
        default:
            itemKindValid = false;
            break;
    }
    switch (failure->reason)
    {
        case NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL:
        case NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND:
        case NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND:
            reasonValid = true;
            break;
        default:
            reasonValid = false;
            break;
    }
    if (!linkValid || !itemKindValid || !reasonValid)
    {
        return false;
    }
    if ((failure->itemKind == NETWORK_LOCAL_ITEM_COMMAND) &&
        (!failure->hasCommandId || (failure->commandId == 0U)))
    {
        return false;
    }
    if ((failure->itemKind == NETWORK_LOCAL_ITEM_REQUEST_RESULT) &&
        !failure->hasRequestContext)
    {
        return false;
    }
    return true;
}

static bool ResultManager_NetworkFailureEquals(
    const NetworkLocalFailure_t *left,
    const NetworkLocalFailure_t *right)
{
    if ((left->link != right->link) ||
        (left->itemKind != right->itemKind) ||
        (left->reason != right->reason) ||
        (left->identityGeneration != right->identityGeneration) ||
        (left->elapsedSinceAdmissionMs != right->elapsedSinceAdmissionMs) ||
        (left->hasCommandId != right->hasCommandId) ||
        (left->hasRequestContext != right->hasRequestContext) ||
        (left->hasOccurrenceId != right->hasOccurrenceId))
    {
        return false;
    }
    if (left->hasCommandId && (left->commandId != right->commandId))
    {
        return false;
    }
    if (left->hasRequestContext &&
        ((left->requestDeviceContextId != right->requestDeviceContextId) ||
         (left->requestContext.sessionId != right->requestContext.sessionId) ||
         (left->requestContext.requestId != right->requestContext.requestId)))
    {
        return false;
    }
    if (left->hasOccurrenceId &&
        (left->occurrenceId != right->occurrenceId))
    {
        return false;
    }
    return true;
}

static void ResultManager_ApplyKnownResult(
    ResultManager_Snapshot_t *snapshot,
    RequestResult_t result,
    ResultReason_t reason,
    ResultConfirmation_t confirmation,
    uint32_t nowMs)
{
    snapshot->hasResult = true;
    snapshot->result = result;
    snapshot->reason = reason;
    snapshot->confirmation = confirmation;
    snapshot->resultUpdatedAtMs = nowMs;
    snapshot->terminal = ResultManager_IsTerminal(result);
    snapshot->outcomeUnconfirmed = false;
    snapshot->timeoutState = RESULT_MANAGER_TIMEOUT_NONE;

    if (!snapshot->firstResponseObserved)
    {
        snapshot->firstResponseObserved = true;
        snapshot->firstResponseAtMs = nowMs;
    }
}

ResultManager_Status_t ResultManager_Init(
    ResultManager_t *manager,
    ResultManager_Slot_t *slots,
    size_t capacity,
    const ResultManager_Policy_t *policy)
{
    size_t index;

    if ((manager == NULL) || (slots == NULL) || (capacity == 0U) ||
        (policy == NULL))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    for (index = 0U; index < capacity; ++index)
    {
        (void)memset(&slots[index], 0, sizeof(slots[index]));
    }
    manager->slots = slots;
    manager->capacity = capacity;
    manager->count = 0U;
    manager->policy = *policy;
    manager->initialized = true;
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_RegisterRequest(
    ResultManager_t *manager,
    const DomainIf_MobileRequest_t *request,
    uint32_t registeredAtMs)
{
    ResultManager_RequestKey_t key;
    ResultManager_Slot_t *slot;
    const ResultManager_Slot_t *newest;
    uint32_t historySequence;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || !ResultManager_IsRequestTypeValid(request->type) ||
        !ResultManager_IsPayloadValid(request) ||
        !ResultManager_IsQualityValid(request->meta.quality))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    key = ResultManager_KeyFromRequest(request);
    slot = ResultManager_FindByRequest(manager, &key);
    if (slot != NULL)
    {
        if (ResultManager_RequestContentEquals(&slot->originalRequest, request))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        slot->snapshot.identityConflictObserved = true;
        return RESULT_MANAGER_ID_CONFLICT;
    }

    newest = ResultManager_FindNewestForDevice(
        manager, request->deviceContextId);
    historySequence = (newest == NULL)
                          ? 0U
                          : newest->historySequence + 1U;
    if (ResultManager_CountForDevice(manager, request->deviceContextId) >=
        RESULT_MANAGER_HISTORY_PER_DEVICE_MAX)
    {
        slot = ResultManager_FindOldestForDevice(
            manager, request->deviceContextId);
    }
    else
    {
        slot = ResultManager_FindFree(manager);
    }
    if (slot == NULL)
    {
        return RESULT_MANAGER_CAPACITY_EXCEEDED;
    }

    if (!slot->inUse)
    {
        ++manager->count;
    }
    (void)memset(slot, 0, sizeof(*slot));
    slot->inUse = true;
    slot->historySequence = historySequence;
    slot->originalRequest = *request;
    slot->snapshot.request = key;
    slot->snapshot.requestType = request->type;
    slot->snapshot.target = ResultManager_TargetForRequest(request->type);
    slot->snapshot.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    slot->snapshot.registeredAtMs = registeredAtMs;
    slot->snapshot.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    slot->snapshot.dispatchState = RESULT_MANAGER_DISPATCH_UNKNOWN;
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_RecordDomainResult(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    RequestResult_t result,
    ResultReason_t reason,
    bool completionEvidenceVerified,
    uint32_t nowMs)
{
    ResultManager_Slot_t *slot;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || !ResultManager_IsResultValid(result) ||
        !ResultManager_IsReasonValid(reason) ||
        !ResultManager_ResultReasonCombinationValid(result, reason) ||
        ((result != REQUEST_RESULT_ACCEPTED) &&
         (result != REQUEST_RESULT_REJECTED) &&
         (result != REQUEST_RESULT_DONE)))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    slot = ResultManager_FindByRequest(manager, request);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    if ((result == REQUEST_RESULT_DONE) &&
        (!completionEvidenceVerified || slot->snapshot.hasCommand))
    {
        return RESULT_MANAGER_EVIDENCE_REJECTED;
    }
    if (slot->snapshot.terminal)
    {
        if (slot->snapshot.hasResult && (slot->snapshot.result == result) &&
            (slot->snapshot.reason == reason))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        return RESULT_MANAGER_FINAL_CONFLICT;
    }
    if ((slot->snapshot.timeoutState != RESULT_MANAGER_TIMEOUT_NONE) &&
        ResultManager_IsIntermediate(result))
    {
        return RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED;
    }
    if (slot->snapshot.hasResult &&
        (slot->snapshot.result == REQUEST_RESULT_IN_PROGRESS) &&
        (result == REQUEST_RESULT_REJECTED))
    {
        return RESULT_MANAGER_INVALID_TRANSITION;
    }

    ResultManager_ApplyKnownResult(&slot->snapshot, result, reason,
                                   RESULT_CONFIRMATION_CONFIRMED, nowMs);
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_BindCommand(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    const CommandManager_Intent_t *intent)
{
    ResultManager_Slot_t *slot;
    DomainIf_BcmCommandKind_t kind;
    DoorLockTarget_t doorTarget;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || (intent == NULL) || (intent->commandId == 0U) ||
        !ResultManager_MapCommandKind(intent->kind, &kind))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }
    if ((intent->request.deviceContextId != request->deviceContextId) ||
        (intent->request.sessionId != request->context.sessionId) ||
        (intent->request.requestId != request->context.requestId))
    {
        return RESULT_MANAGER_ID_CONFLICT;
    }

    slot = ResultManager_FindByRequest(manager, request);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    if (!slot->snapshot.hasResult ||
        (slot->snapshot.result != REQUEST_RESULT_ACCEPTED) ||
        slot->snapshot.terminal)
    {
        return RESULT_MANAGER_INVALID_TRANSITION;
    }
    if (!ResultManager_RequestAcceptsCommandKind(slot->snapshot.requestType,
                                                  kind))
    {
        return RESULT_MANAGER_KIND_MISMATCH;
    }
    if (ResultManager_FindByCommand(manager, intent->commandId) != NULL)
    {
        if (slot->snapshot.hasCommand &&
            (slot->snapshot.commandId == intent->commandId) &&
            (slot->snapshot.commandKind == kind))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        return RESULT_MANAGER_COMMAND_CONFLICT;
    }
    if (slot->snapshot.hasCommand)
    {
        return RESULT_MANAGER_COMMAND_CONFLICT;
    }

    slot->snapshot.hasCommand = true;
    slot->snapshot.commandId = intent->commandId;
    slot->snapshot.commandKind = kind;
    slot->snapshot.dispatchState = RESULT_MANAGER_DISPATCH_UNKNOWN;
    if (kind == BCM_COMMAND_KIND_DOOR)
    {
        if (!ResultManager_MapDoorTarget(intent->target.door.target, &doorTarget))
        {
            slot->snapshot.hasCommand = false;
            slot->snapshot.commandId = 0U;
            return RESULT_MANAGER_INVALID_ARGUMENT;
        }
        slot->snapshot.hasExpectedDoorTarget = true;
        slot->snapshot.expectedDoorTarget = doorTarget;
    }
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_RecordDispatchState(
    ResultManager_t *manager,
    DomainCommandId_t commandId,
    ResultManager_DispatchState_t dispatchState)
{
    ResultManager_Slot_t *slot;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((commandId == 0U) ||
        ((dispatchState != RESULT_MANAGER_NOT_DISPATCHED) &&
         (dispatchState != RESULT_MANAGER_DISPATCHED)))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    slot = ResultManager_FindByCommand(manager, commandId);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    if (slot->snapshot.dispatchState == dispatchState)
    {
        return RESULT_MANAGER_DUPLICATE;
    }
    if (slot->snapshot.dispatchState == RESULT_MANAGER_DISPATCHED)
    {
        return RESULT_MANAGER_INVALID_TRANSITION;
    }
    slot->snapshot.dispatchState = dispatchState;
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_ApplyBcmResult(
    ResultManager_t *manager,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs)
{
    ResultManager_Slot_t *slot;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((result == NULL) || (result->commandId == 0U) ||
        !ResultManager_IsBcmKindValid(result->kind) ||
        !ResultManager_IsResultValid(result->result) ||
        !ResultManager_IsReasonValid(result->reason) ||
        !ResultManager_IsConfirmationValid(result->confirmation) ||
        !ResultManager_ResultReasonCombinationValid(result->result,
                                                     result->reason))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    slot = ResultManager_FindByCommand(manager, result->commandId);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    if (slot->snapshot.commandKind != result->kind)
    {
        return RESULT_MANAGER_KIND_MISMATCH;
    }
    if ((result->kind != BCM_COMMAND_KIND_DOOR) && result->hasDoorEvidence)
    {
        return RESULT_MANAGER_EVIDENCE_REJECTED;
    }
    if ((result->kind == BCM_COMMAND_KIND_DOOR) &&
        !ResultManager_DoorEvidenceMatches(manager, &slot->snapshot, result))
    {
        return RESULT_MANAGER_EVIDENCE_REJECTED;
    }
    if ((result->confirmation == RESULT_CONFIRMATION_UNCONFIRMED) &&
        ResultManager_IsTerminal(result->result))
    {
        if (slot->snapshot.hasUnconfirmedBcmResult &&
            ResultManager_BcmResultEquals(&slot->snapshot.unconfirmedBcmResult,
                                          result))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        slot->snapshot.hasUnconfirmedBcmResult = true;
        slot->snapshot.unconfirmedBcmResult = *result;
        slot->snapshot.outcomeUnconfirmed = true;
        return RESULT_MANAGER_UNCONFIRMED_RECORDED;
    }
    if (slot->snapshot.terminal)
    {
        if (ResultManager_IsIntermediate(result->result))
        {
            return RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED;
        }
        if (slot->snapshot.hasAppliedBcmResult &&
            ResultManager_BcmResultEquals(&slot->snapshot.appliedBcmResult,
                                          result))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        return RESULT_MANAGER_FINAL_CONFLICT;
    }
    if ((slot->snapshot.timeoutState != RESULT_MANAGER_TIMEOUT_NONE) &&
        ResultManager_IsIntermediate(result->result))
    {
        return RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED;
    }
    if (slot->snapshot.hasResult)
    {
        if ((slot->snapshot.result == result->result) &&
            (slot->snapshot.reason == result->reason))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        if ((slot->snapshot.result == REQUEST_RESULT_IN_PROGRESS) &&
            (result->result == REQUEST_RESULT_ACCEPTED))
        {
            return RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED;
        }
        if ((slot->snapshot.result == REQUEST_RESULT_IN_PROGRESS) &&
            (result->result == REQUEST_RESULT_REJECTED))
        {
            return RESULT_MANAGER_INVALID_TRANSITION;
        }
    }

    ResultManager_ApplyKnownResult(&slot->snapshot, result->result,
                                   result->reason, result->confirmation, nowMs);
    slot->snapshot.hasUnconfirmedBcmResult = false;
    slot->snapshot.hasAppliedBcmResult = true;
    slot->snapshot.appliedBcmResult = *result;
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_RecordNetworkLocalFailure(
    ResultManager_t *manager,
    const NetworkLocalFailure_t *failure,
    uint32_t receivedAtMs)
{
    ResultManager_Slot_t *slot = NULL;
    ResultManager_RequestKey_t key;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((failure == NULL) || !ResultManager_NetworkFailureValid(failure))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    if (failure->hasCommandId)
    {
        slot = ResultManager_FindByCommand(manager, failure->commandId);
    }
    if ((slot == NULL) && failure->hasRequestContext)
    {
        key.deviceContextId = failure->requestDeviceContextId;
        key.context = failure->requestContext;
        slot = ResultManager_FindByRequest(manager, &key);
    }
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    if (failure->hasCommandId &&
        (!slot->snapshot.hasCommand ||
         (slot->snapshot.commandId != failure->commandId)))
    {
        return RESULT_MANAGER_COMMAND_CONFLICT;
    }
    if (failure->hasRequestContext &&
        ((slot->snapshot.request.deviceContextId !=
          failure->requestDeviceContextId) ||
         (slot->snapshot.request.context.sessionId !=
          failure->requestContext.sessionId) ||
         (slot->snapshot.request.context.requestId !=
          failure->requestContext.requestId)))
    {
        return RESULT_MANAGER_ID_CONFLICT;
    }
    if (slot->snapshot.hasNetworkLocalFailure)
    {
        if (ResultManager_NetworkFailureEquals(
                &slot->snapshot.networkLocalFailure, failure))
        {
            return RESULT_MANAGER_DUPLICATE;
        }
        return RESULT_MANAGER_CAPACITY_EXCEEDED;
    }

    slot->snapshot.hasNetworkLocalFailure = true;
    slot->snapshot.networkLocalFailure = *failure;
    slot->snapshot.networkLocalFailureReceivedAtMs = receivedAtMs;
    if (!slot->snapshot.terminal)
    {
        slot->snapshot.outcomeUnconfirmed = true;
    }
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_EvaluateTimeout(
    ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    uint32_t nowMs,
    ResultManager_TimeoutState_t *outNewTimeout)
{
    ResultManager_Slot_t *slot;
    ResultManager_TimeoutState_t timeout = RESULT_MANAGER_TIMEOUT_NONE;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || (outNewTimeout == NULL))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }
    slot = ResultManager_FindByRequest(manager, request);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }

    if (!slot->snapshot.terminal &&
        (slot->snapshot.timeoutState == RESULT_MANAGER_TIMEOUT_NONE))
    {
        if (!slot->snapshot.firstResponseObserved &&
            manager->policy.firstResponseTimeoutEnabled &&
            ((uint32_t)(nowMs - slot->snapshot.registeredAtMs) >=
             manager->policy.firstResponseTimeoutMs))
        {
            timeout = RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE;
        }
        else if (slot->snapshot.firstResponseObserved &&
                 manager->policy.finalResultTimeoutEnabled &&
                 ((uint32_t)(nowMs - slot->snapshot.firstResponseAtMs) >=
                  manager->policy.finalResultTimeoutMs))
        {
            timeout = RESULT_MANAGER_TIMEOUT_FINAL_RESULT;
        }
    }

    if (timeout != RESULT_MANAGER_TIMEOUT_NONE)
    {
        slot->snapshot.timeoutState = timeout;
        slot->snapshot.outcomeUnconfirmed = true;
        slot->snapshot.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    }
    *outNewTimeout = timeout;
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_GetSnapshot(
    const ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    ResultManager_Snapshot_t *outSnapshot)
{
    const ResultManager_Slot_t *slot;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || (outSnapshot == NULL))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }
    slot = ResultManager_FindByRequestConst(manager, request);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }
    *outSnapshot = slot->snapshot;
    return RESULT_MANAGER_OK;
}

static void ResultManager_BuildMobileResult(
    const ResultManager_Slot_t *slot,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResult)
{
    (void)memset(outResult, 0, sizeof(*outResult));
    outResult->deviceContextId = slot->snapshot.request.deviceContextId;
    outResult->resultInfo.context = slot->snapshot.request.context;
    outResult->hasVehicleResult = slot->snapshot.hasResult &&
                                  !slot->snapshot.outcomeUnconfirmed;
    if (slot->snapshot.hasResult)
    {
        outResult->resultInfo.result = slot->snapshot.result;
        outResult->resultInfo.reason = slot->snapshot.reason;
        outResult->resultInfo.confirmation =
            slot->snapshot.outcomeUnconfirmed
                ? RESULT_CONFIRMATION_UNCONFIRMED
                : slot->snapshot.confirmation;
        outResult->resultAgeMs =
            (uint32_t)(nowMs - slot->snapshot.resultUpdatedAtMs);
    }
    else
    {
        /* resultInfo.result is intentionally not inspected when false. */
        outResult->resultInfo.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
        outResult->resultAgeMs =
            (uint32_t)(nowMs - slot->snapshot.registeredAtMs);
    }
    outResult->target = slot->snapshot.target;
    outResult->commandId = slot->snapshot.hasCommand
                               ? slot->snapshot.commandId
                               : 0U;
    outResult->origin = slot->snapshot.origin;
}

ResultManager_Status_t ResultManager_GetMobileResult(
    const ResultManager_t *manager,
    const ResultManager_RequestKey_t *request,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResult)
{
    const ResultManager_Slot_t *slot;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || (outResult == NULL))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }
    slot = ResultManager_FindByRequestConst(manager, request);
    if (slot == NULL)
    {
        return RESULT_MANAGER_NOT_FOUND;
    }

    ResultManager_BuildMobileResult(slot, nowMs, outResult);
    return RESULT_MANAGER_OK;
}

ResultManager_Status_t ResultManager_GetRecentMobileResults(
    const ResultManager_t *manager,
    DeviceContextId_t requestingDevice,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResults,
    size_t capacity,
    size_t *outCount)
{
    const ResultManager_Slot_t *ordered[RESULT_MANAGER_HISTORY_PER_DEVICE_MAX];
    DomainIf_MobileRequestResult_t scratch[RESULT_MANAGER_HISTORY_PER_DEVICE_MAX];
    size_t available;
    size_t resultCount;
    size_t outputIndex;
    size_t slotIndex;

    if (!ResultManager_IsInitialized(manager))
    {
        return (manager == NULL) ? RESULT_MANAGER_INVALID_ARGUMENT
                                 : RESULT_MANAGER_NOT_INITIALIZED;
    }
    if ((requestingDevice == 0U) || (outCount == NULL) ||
        ((capacity > 0U) && (outResults == NULL)))
    {
        return RESULT_MANAGER_INVALID_ARGUMENT;
    }

    available = ResultManager_CountForDevice(manager, requestingDevice);
    if (available > RESULT_MANAGER_HISTORY_PER_DEVICE_MAX)
    {
        return RESULT_MANAGER_INVALID_TRANSITION;
    }
    if (capacity == 0U)
    {
        *outCount = available;
        return RESULT_MANAGER_OK;
    }

    resultCount = (available < capacity) ? available : capacity;
    for (outputIndex = 0U; outputIndex < resultCount; ++outputIndex)
    {
        const ResultManager_Slot_t *newest = NULL;

        for (slotIndex = 0U; slotIndex < manager->capacity; ++slotIndex)
        {
            const ResultManager_Slot_t *candidate =
                &manager->slots[slotIndex];
            bool alreadySelected = false;
            size_t selectedIndex;

            if (!candidate->inUse ||
                (candidate->snapshot.request.deviceContextId !=
                 requestingDevice))
            {
                continue;
            }
            for (selectedIndex = 0U; selectedIndex < outputIndex;
                 ++selectedIndex)
            {
                if (ordered[selectedIndex] == candidate)
                {
                    alreadySelected = true;
                    break;
                }
            }
            if (!alreadySelected &&
                ((newest == NULL) ||
                 ResultManager_IsSequenceNewer(candidate->historySequence,
                                               newest->historySequence)))
            {
                newest = candidate;
            }
        }
        if (newest == NULL)
        {
            return RESULT_MANAGER_INVALID_TRANSITION;
        }
        ordered[outputIndex] = newest;
        ResultManager_BuildMobileResult(newest, nowMs, &scratch[outputIndex]);
    }

    if (resultCount > 0U)
    {
        (void)memcpy(outResults, scratch, resultCount * sizeof(scratch[0]));
    }
    *outCount = resultCount;
    return RESULT_MANAGER_OK;
}
