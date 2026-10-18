#include "command_manager.h"

#include <limits.h>
#include <string.h>

static bool CommandManager_IsInitialized(const CommandManager_t *manager)
{
    return (manager != NULL) && manager->initialized &&
           (manager->slots != NULL) && (manager->capacity > 0U);
}

static CommandManager_CommandId_t CommandManager_IncrementNonZero(
    CommandManager_CommandId_t value)
{
    ++value;
    if (value == 0U)
    {
        value = 1U;
    }
    return value;
}

static uint32_t CommandManager_NextDecisionSequence(uint32_t value)
{
    ++value;
    if (value == 0U)
    {
        value = 1U;
    }
    return value;
}

static CommandManager_Slot_t *CommandManager_FindFreeSlot(
    CommandManager_t *manager)
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

static CommandManager_Slot_t *CommandManager_FindById(
    CommandManager_t *manager,
    CommandManager_CommandId_t commandId)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            (manager->slots[index].intent.commandId == commandId))
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static const CommandManager_Slot_t *CommandManager_FindByIdConst(
    const CommandManager_t *manager,
    CommandManager_CommandId_t commandId)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        if (manager->slots[index].inUse &&
            (manager->slots[index].intent.commandId == commandId))
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static bool CommandManager_HasActiveRequest(
    const CommandManager_t *manager,
    const CommandManager_RequestLink_t *request)
{
    size_t index;

    for (index = 0U; index < manager->capacity; ++index)
    {
        const CommandManager_Slot_t *slot = &manager->slots[index];
        if (slot->inUse &&
            (slot->intent.request.deviceContextId == request->deviceContextId) &&
            (slot->intent.request.sessionId == request->sessionId) &&
            (slot->intent.request.requestId == request->requestId))
        {
            return true;
        }
    }
    return false;
}

static CommandManager_Status_t CommandManager_AllocateId(
    CommandManager_t *manager,
    CommandManager_CommandId_t *outCommandId)
{
    CommandManager_CommandId_t candidate = manager->nextCommandId;
    size_t checked = 0U;

    if (candidate == 0U)
    {
        candidate = 1U;
    }

    while (checked <= manager->capacity)
    {
        if (CommandManager_FindById(manager, candidate) == NULL)
        {
            *outCommandId = candidate;
            manager->nextCommandId = CommandManager_IncrementNonZero(candidate);
            return COMMAND_MANAGER_OK;
        }
        candidate = CommandManager_IncrementNonZero(candidate);
        ++checked;
    }

    return COMMAND_MANAGER_COMMAND_ID_EXHAUSTED;
}

static bool CommandManager_IsFanTargetValid(RequestManager_FanLevel_t target)
{
    switch (target)
    {
        case REQUEST_MANAGER_FAN_LEVEL_OFF:
        case REQUEST_MANAGER_FAN_LEVEL_LOW:
        case REQUEST_MANAGER_FAN_LEVEL_MEDIUM:
        case REQUEST_MANAGER_FAN_LEVEL_HIGH:
            return true;

        case REQUEST_MANAGER_FAN_LEVEL_UNKNOWN:
        default:
            return false;
    }
}

static bool CommandManager_IsThermalDirectionValid(
    CommandManager_ThermalDirection_t direction)
{
    switch (direction)
    {
        case COMMAND_MANAGER_THERMAL_IDLE:
        case COMMAND_MANAGER_THERMAL_COOL:
        case COMMAND_MANAGER_THERMAL_HEAT:
            return true;

        default:
            return false;
    }
}

static bool CommandManager_IsInteriorLightTypeValid(
    CommandManager_InteriorLightType_t type)
{
    switch (type)
    {
        case COMMAND_MANAGER_LIGHT_NORMAL:
        case COMMAND_MANAGER_LIGHT_GOODBYE:
        case COMMAND_MANAGER_LIGHT_WARNING:
        case COMMAND_MANAGER_LIGHT_FAULT:
            return true;

        default:
            return false;
    }
}

static bool CommandManager_IsSpecValid(const CommandManager_CommandSpec_t *spec)
{
    switch (spec->kind)
    {
        case COMMAND_MANAGER_KIND_DOOR:
            return (spec->target.door.target == COMMAND_MANAGER_DOOR_TARGET_LOCK) ||
                   (spec->target.door.target == COMMAND_MANAGER_DOOR_TARGET_UNLOCK);

        case COMMAND_MANAGER_KIND_CLIMATE:
            return CommandManager_IsFanTargetValid(
                       spec->target.climate.fanTarget) &&
                   CommandManager_IsThermalDirectionValid(
                       spec->target.climate.thermalDirection) &&
                   (spec->target.climate.thermalTargetLevelPercent <= 100U);

        case COMMAND_MANAGER_KIND_INTERIOR_LIGHT:
            return CommandManager_IsInteriorLightTypeValid(
                       spec->target.interiorLight.type) &&
                   (spec->target.interiorLight.levelPercent <= 100U);

        default:
            return false;
    }
}

static CommandManager_Status_t CommandManager_ValidateDecisionAndSpec(
    const RequestManager_Request_t *request,
    const RequestManager_Decision_t *decision,
    const CommandManager_CommandSpec_t *spec)
{
    if (decision->verdict != REQUEST_MANAGER_VERDICT_ALLOW)
    {
        return COMMAND_MANAGER_DECISION_NOT_ALLOWED;
    }

    if (decision->action == REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED)
    {
        return COMMAND_MANAGER_NO_COMMAND_REQUIRED;
    }

    switch (decision->action)
    {
        case REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT:
            if ((request->kind != REQUEST_MANAGER_KIND_DOOR) ||
                (spec->kind != COMMAND_MANAGER_KIND_DOOR))
            {
                return COMMAND_MANAGER_INVALID_ARGUMENT;
            }
            if (((request->operation == REQUEST_MANAGER_OPERATION_DOOR_LOCK) &&
                 (spec->target.door.target != COMMAND_MANAGER_DOOR_TARGET_LOCK)) ||
                ((request->operation == REQUEST_MANAGER_OPERATION_DOOR_UNLOCK) &&
                 (spec->target.door.target != COMMAND_MANAGER_DOOR_TARGET_UNLOCK)))
            {
                return COMMAND_MANAGER_INVALID_ARGUMENT;
            }
            break;

        case REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT:
            if ((request->kind != REQUEST_MANAGER_KIND_CLIMATE) ||
                (spec->kind != COMMAND_MANAGER_KIND_CLIMATE))
            {
                return COMMAND_MANAGER_INVALID_ARGUMENT;
            }
            break;

        case REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT:
            if ((request->kind != REQUEST_MANAGER_KIND_INTERIOR_LIGHT) ||
                (spec->kind != COMMAND_MANAGER_KIND_INTERIOR_LIGHT))
            {
                return COMMAND_MANAGER_INVALID_ARGUMENT;
            }
            break;

        case REQUEST_MANAGER_ACTION_NONE:
        case REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY:
        default:
            return COMMAND_MANAGER_UNSUPPORTED_ACTION;
    }

    return CommandManager_IsSpecValid(spec) ? COMMAND_MANAGER_OK
                                             : COMMAND_MANAGER_INVALID_ARGUMENT;
}

static bool CommandManager_IsSubmitOutcomeValid(CommandManager_SubmitOutcome_t outcome)
{
    return (outcome == COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED) ||
           (outcome == COMMAND_MANAGER_SUBMIT_FAILED_NOT_READY) ||
           (outcome == COMMAND_MANAGER_SUBMIT_FAILED_NO_CAPACITY) ||
           (outcome == COMMAND_MANAGER_SUBMIT_FAILED_REJECTED) ||
           (outcome == COMMAND_MANAGER_SUBMIT_FAILED_OTHER);
}

static CommandManager_SubmissionState_t CommandManager_MapSubmissionState(
    CommandManager_SubmitOutcome_t outcome)
{
    switch (outcome)
    {
        case COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED:
            return COMMAND_MANAGER_SUBMISSION_LOCALLY_ACCEPTED;
        case COMMAND_MANAGER_SUBMIT_FAILED_NOT_READY:
            return COMMAND_MANAGER_SUBMISSION_FAILED_NOT_READY;
        case COMMAND_MANAGER_SUBMIT_FAILED_NO_CAPACITY:
            return COMMAND_MANAGER_SUBMISSION_FAILED_NO_CAPACITY;
        case COMMAND_MANAGER_SUBMIT_FAILED_REJECTED:
            return COMMAND_MANAGER_SUBMISSION_FAILED_REJECTED;
        case COMMAND_MANAGER_SUBMIT_FAILED_OTHER:
        default:
            return COMMAND_MANAGER_SUBMISSION_FAILED_OTHER;
    }
}

CommandManager_Status_t CommandManager_Init(
    CommandManager_t *manager,
    CommandManager_Slot_t *slots,
    size_t capacity,
    CommandManager_CommandId_t firstCommandId)
{
    size_t index;

    if ((manager == NULL) || (slots == NULL) || (capacity == 0U))
    {
        return COMMAND_MANAGER_INVALID_ARGUMENT;
    }

    for (index = 0U; index < capacity; ++index)
    {
        (void)memset(&slots[index], 0, sizeof(slots[index]));
    }

    manager->slots = slots;
    manager->capacity = capacity;
    manager->activeCount = 0U;
    manager->nextCommandId = (firstCommandId == 0U) ? 1U : firstCommandId;
    manager->nextDecisionSequence = 1U;
    manager->initialized = true;
    return COMMAND_MANAGER_OK;
}

CommandManager_Status_t CommandManager_CreateIntent(
    CommandManager_t *manager,
    const RequestManager_Request_t *request,
    CommandManager_RequestId_t requestId,
    const RequestManager_Decision_t *decision,
    const CommandManager_CommandSpec_t *spec,
    uint32_t createdAtMs,
    CommandManager_Intent_t *outIntent)
{
    CommandManager_RequestLink_t requestLink;
    CommandManager_Slot_t *slot;
    CommandManager_CommandId_t commandId;
    CommandManager_Status_t status;

    if (!CommandManager_IsInitialized(manager))
    {
        return (manager == NULL) ? COMMAND_MANAGER_INVALID_ARGUMENT
                                 : COMMAND_MANAGER_NOT_INITIALIZED;
    }
    if ((request == NULL) || (decision == NULL) || (spec == NULL) ||
        (outIntent == NULL))
    {
        return COMMAND_MANAGER_INVALID_ARGUMENT;
    }

    status = CommandManager_ValidateDecisionAndSpec(request, decision, spec);
    if (status != COMMAND_MANAGER_OK)
    {
        return status;
    }

    requestLink.deviceContextId = request->deviceContextId;
    requestLink.sessionId = request->sessionId;
    requestLink.requestId = requestId;
    if (CommandManager_HasActiveRequest(manager, &requestLink))
    {
        return COMMAND_MANAGER_DUPLICATE_REQUEST;
    }

    slot = CommandManager_FindFreeSlot(manager);
    if (slot == NULL)
    {
        return COMMAND_MANAGER_CAPACITY_EXCEEDED;
    }

    status = CommandManager_AllocateId(manager, &commandId);
    if (status != COMMAND_MANAGER_OK)
    {
        return status;
    }

    slot->intent.commandId = commandId;
    slot->intent.decisionSequence = manager->nextDecisionSequence;
    slot->intent.targetEcu = COMMAND_MANAGER_ECU_BCM;
    slot->intent.kind = spec->kind;
    slot->intent.target = spec->target;
    slot->intent.createdAtMs = createdAtMs;
    slot->intent.request = requestLink;
    slot->intent.submissionState = COMMAND_MANAGER_SUBMISSION_NOT_SUBMITTED;
    slot->intent.submissionAttemptedAtMs = 0U;
    slot->intent.submissionAttempted = false;
    slot->inUse = true;

    manager->nextDecisionSequence =
        CommandManager_NextDecisionSequence(manager->nextDecisionSequence);
    ++manager->activeCount;
    *outIntent = slot->intent;
    return COMMAND_MANAGER_OK;
}

CommandManager_Status_t CommandManager_GetIntent(
    const CommandManager_t *manager,
    CommandManager_CommandId_t commandId,
    CommandManager_Intent_t *outIntent)
{
    const CommandManager_Slot_t *slot;

    if (!CommandManager_IsInitialized(manager))
    {
        return (manager == NULL) ? COMMAND_MANAGER_INVALID_ARGUMENT
                                 : COMMAND_MANAGER_NOT_INITIALIZED;
    }
    if ((commandId == 0U) || (outIntent == NULL))
    {
        return COMMAND_MANAGER_INVALID_ARGUMENT;
    }

    slot = CommandManager_FindByIdConst(manager, commandId);
    if (slot == NULL)
    {
        return COMMAND_MANAGER_NOT_FOUND;
    }
    *outIntent = slot->intent;
    return COMMAND_MANAGER_OK;
}

CommandManager_Status_t CommandManager_RecordSubmission(
    CommandManager_t *manager,
    CommandManager_CommandId_t commandId,
    CommandManager_SubmitOutcome_t outcome,
    uint32_t attemptedAtMs)
{
    CommandManager_Slot_t *slot;

    if (!CommandManager_IsInitialized(manager))
    {
        return (manager == NULL) ? COMMAND_MANAGER_INVALID_ARGUMENT
                                 : COMMAND_MANAGER_NOT_INITIALIZED;
    }
    if ((commandId == 0U) || !CommandManager_IsSubmitOutcomeValid(outcome))
    {
        return COMMAND_MANAGER_INVALID_ARGUMENT;
    }

    slot = CommandManager_FindById(manager, commandId);
    if (slot == NULL)
    {
        return COMMAND_MANAGER_NOT_FOUND;
    }
    if (slot->intent.submissionState != COMMAND_MANAGER_SUBMISSION_NOT_SUBMITTED)
    {
        return COMMAND_MANAGER_INVALID_TRANSITION;
    }

    slot->intent.submissionState = CommandManager_MapSubmissionState(outcome);
    slot->intent.submissionAttemptedAtMs = attemptedAtMs;
    slot->intent.submissionAttempted = true;
    return COMMAND_MANAGER_OK;
}

CommandManager_Status_t CommandManager_Release(
    CommandManager_t *manager,
    CommandManager_CommandId_t commandId)
{
    CommandManager_Slot_t *slot;

    if (!CommandManager_IsInitialized(manager))
    {
        return (manager == NULL) ? COMMAND_MANAGER_INVALID_ARGUMENT
                                 : COMMAND_MANAGER_NOT_INITIALIZED;
    }
    if (commandId == 0U)
    {
        return COMMAND_MANAGER_INVALID_ARGUMENT;
    }

    slot = CommandManager_FindById(manager, commandId);
    if (slot == NULL)
    {
        return COMMAND_MANAGER_NOT_FOUND;
    }

    (void)memset(slot, 0, sizeof(*slot));
    --manager->activeCount;
    return COMMAND_MANAGER_OK;
}
