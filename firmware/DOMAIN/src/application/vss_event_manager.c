#include "vss_event_manager.h"

#include <limits.h>
#include <string.h>

static uint32_t VssEventManager_SaturatingAdd(
    uint32_t left,
    uint32_t right)
{
    return (UINT32_MAX - left < right) ? UINT32_MAX : left + right;
}

static bool VssEventManager_IsExpired(
    const VssEventManager_Slot_t *slot,
    uint32_t nowMs)
{
    return VssEventManager_SaturatingAdd(
               slot->originalAgeMs, (uint32_t)(nowMs - slot->createdAtMs)) >=
           VSS_EVENT_MANAGER_USE_LIMIT_MS;
}

static void VssEventManager_DiscardSlots(VssEventManager_t *manager)
{
    (void)memset(manager->slots, 0, sizeof(manager->slots));
}

static VssEventManager_Status_t VssEventManager_RefreshGeneration(
    VssEventManager_t *manager)
{
    SourceGeneration_t generation = 0U;

    if ((manager->generationProvider == NULL) ||
        !manager->generationProvider(manager->generationProviderContext,
                                     &generation) ||
        (generation == 0U))
    {
        VssEventManager_DiscardSlots(manager);
        manager->identityGeneration = 0U;
        manager->generationKnown = false;
        return VSS_EVENT_MANAGER_GENERATION_UNAVAILABLE;
    }
    if (manager->generationKnown &&
        (manager->identityGeneration != generation))
    {
        VssEventManager_DiscardSlots(manager);
    }
    manager->identityGeneration = generation;
    manager->generationKnown = true;
    return VSS_EVENT_MANAGER_OK;
}

static VssEventManager_Slot_t *VssEventManager_Allocate(
    VssEventManager_t *manager,
    uint32_t nowMs)
{
    size_t index;

    for (index = 0U; index < VSS_EVENT_MANAGER_CAPACITY; ++index)
    {
        if (manager->slots[index].inUse &&
            VssEventManager_IsExpired(&manager->slots[index], nowMs))
        {
            (void)memset(&manager->slots[index], 0,
                         sizeof(manager->slots[index]));
        }
        if (!manager->slots[index].inUse)
        {
            return &manager->slots[index];
        }
    }
    return NULL;
}

static OccurrenceId_t VssEventManager_AllocateOccurrence(
    VssEventManager_t *manager)
{
    OccurrenceId_t occurrence = manager->nextOccurrenceId;

    manager->nextOccurrenceId += 1U;
    if (manager->nextOccurrenceId == 0U)
    {
        manager->nextOccurrenceId = 1U;
    }
    return occurrence;
}

static bool VssEventManager_IsDuplicate(
    VssEventManager_t *manager,
    uint32_t sourceOccurrence,
    DomainCommandId_t sourceCommand,
    VssOneShotEvent_t event,
    uint32_t nowMs)
{
    size_t index;

    for (index = 0U; index < VSS_EVENT_MANAGER_CAPACITY; ++index)
    {
        VssEventManager_Slot_t *slot = &manager->slots[index];
        if (slot->inUse && VssEventManager_IsExpired(slot, nowMs))
        {
            slot->inUse = false;
            continue;
        }
        if (slot->inUse &&
            (slot->sourceOccurrenceId == sourceOccurrence) &&
            (slot->sourceCommandId == sourceCommand) &&
            (slot->event.event == event))
        {
            return true;
        }
    }
    return false;
}

static VssEventManager_Status_t VssEventManager_Record(
    VssEventManager_t *manager,
    VssOneShotEvent_t event,
    uint32_t sourceOccurrence,
    DomainCommandId_t sourceCommand,
    uint32_t originalAgeMs,
    bool hasRelatedCommand,
    uint32_t nowMs)
{
    VssEventManager_Slot_t *slot;
    VssEventManager_Status_t status;

    status = VssEventManager_RefreshGeneration(manager);
    if (status != VSS_EVENT_MANAGER_OK)
    {
        return status;
    }

    if (VssEventManager_IsDuplicate(manager, sourceOccurrence,
                                    sourceCommand, event, nowMs))
    {
        return VSS_EVENT_MANAGER_DUPLICATE;
    }
    if (originalAgeMs >= VSS_EVENT_MANAGER_USE_LIMIT_MS)
    {
        return VSS_EVENT_MANAGER_IGNORED;
    }
    slot = VssEventManager_Allocate(manager, nowMs);
    if (slot == NULL)
    {
        return VSS_EVENT_MANAGER_NO_CAPACITY;
    }
    (void)memset(slot, 0, sizeof(*slot));
    slot->inUse = true;
    slot->createdAtMs = nowMs;
    slot->originalAgeMs = originalAgeMs;
    slot->identityGeneration = manager->identityGeneration;
    slot->sourceOccurrenceId = sourceOccurrence;
    slot->sourceCommandId = sourceCommand;
    slot->event.event = event;
    slot->event.occurrenceId = VssEventManager_AllocateOccurrence(manager);
    slot->event.meta.quality = DATA_QUALITY_OK;
    slot->event.meta.ageMs = originalAgeMs;
    slot->event.meta.updateSequence = 0U;
    slot->event.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    slot->event.hasRelatedCommand = hasRelatedCommand;
    slot->event.relatedCommandId = hasRelatedCommand ? sourceCommand : 0U;
    return VSS_EVENT_MANAGER_OK;
}

VssEventManager_Status_t VssEventManager_Init(
    VssEventManager_t *manager,
    VssEventManager_Tx_t tx)
{
    if ((manager == NULL) || (tx == NULL))
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    (void)memset(manager, 0, sizeof(*manager));
    manager->nextOccurrenceId = 1U;
    manager->tx = tx;
    manager->initialized = true;
    return VSS_EVENT_MANAGER_OK;
}

VssEventManager_Status_t VssEventManager_BindGenerationProvider(
    VssEventManager_t *manager,
    VssEventManager_GenerationProvider_t provider,
    void *providerContext)
{
    if ((manager == NULL) || (provider == NULL))
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_EVENT_MANAGER_NOT_INITIALIZED;
    }
    VssEventManager_DiscardSlots(manager);
    manager->generationProvider = provider;
    manager->generationProviderContext = providerContext;
    manager->identityGeneration = 0U;
    manager->generationKnown = false;
    return VSS_EVENT_MANAGER_OK;
}

VssEventManager_Status_t VssEventManager_RecordBcmDoorResult(
    VssEventManager_t *manager,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs)
{
    VssOneShotEvent_t event;

    if ((manager == NULL) || (result == NULL))
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_EVENT_MANAGER_NOT_INITIALIZED;
    }
    if ((result->kind != BCM_COMMAND_KIND_DOOR) ||
        !result->hasDoorEvidence ||
        (result->confirmation != RESULT_CONFIRMATION_CONFIRMED) ||
        (result->doorEvidence.quality != DATA_QUALITY_OK))
    {
        return VSS_EVENT_MANAGER_IGNORED;
    }

    if ((result->result == REQUEST_RESULT_DONE) &&
        (result->reason == RESULT_REASON_NONE) &&
        (result->doorEvidence.type == BCM_DOOR_EVIDENCE_LOCK_COMPLETED) &&
        (result->doorEvidence.target == DOOR_TARGET_LOCK))
    {
        event = VSS_EVENT_DOOR_LOCK_COMPLETE;
    }
    else if ((result->result == REQUEST_RESULT_DONE) &&
             (result->reason == RESULT_REASON_NONE) &&
             (result->doorEvidence.type ==
              BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED) &&
             (result->doorEvidence.target == DOOR_TARGET_UNLOCK))
    {
        event = VSS_EVENT_DOOR_UNLOCK_COMPLETE;
    }
    else if ((result->result == REQUEST_RESULT_FAILED) &&
             (result->doorEvidence.type ==
              BCM_DOOR_EVIDENCE_REQUEST_FAILED) &&
             (result->doorEvidence.target == DOOR_TARGET_LOCK) &&
             ((result->reason == RESULT_REASON_NO_FEEDBACK) ||
              (result->reason == RESULT_REASON_DRIVE_LIMIT_EXCEEDED)))
    {
        event = VSS_EVENT_DOOR_LOCK_ERROR;
    }
    else
    {
        return VSS_EVENT_MANAGER_IGNORED;
    }

    return VssEventManager_Record(
        manager, event, result->doorEvidence.occurrenceId,
        result->commandId, result->doorEvidence.ageMs, true, nowMs);
}

VssEventManager_Status_t VssEventManager_RecordVehicleUseTransition(
    VssEventManager_t *manager,
    VssEventManager_VehicleUseTransition_t transition,
    uint32_t nowMs)
{
    VssOneShotEvent_t event;

    if (manager == NULL)
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_EVENT_MANAGER_NOT_INITIALIZED;
    }
    if ((transition != VSS_EVENT_MANAGER_VEHICLE_USE_STARTED) &&
        (transition != VSS_EVENT_MANAGER_VEHICLE_USE_ENDED))
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    event = (transition == VSS_EVENT_MANAGER_VEHICLE_USE_STARTED)
                ? VSS_EVENT_VEHICLE_WELCOME
                : VSS_EVENT_VEHICLE_GOODBYE;
    return VssEventManager_Record(manager, event, 0U, 0U, 0U, false,
                                  nowMs);
}

VssEventManager_Status_t VssEventManager_RecordNetworkLocalFailure(
    VssEventManager_t *manager,
    const NetworkLocalFailure_t *failure,
    uint32_t nowMs)
{
    size_t index;
    VssEventManager_Status_t status;

    if ((manager == NULL) || (failure == NULL))
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_EVENT_MANAGER_NOT_INITIALIZED;
    }
    if ((failure->itemKind != NETWORK_LOCAL_ITEM_OCCURRENCE) ||
        !failure->hasOccurrenceId)
    {
        return VSS_EVENT_MANAGER_IGNORED;
    }
    status = VssEventManager_RefreshGeneration(manager);
    if (status != VSS_EVENT_MANAGER_OK)
    {
        return status;
    }
    if ((failure->identityGeneration == 0U) ||
        (failure->identityGeneration != manager->identityGeneration))
    {
        return VSS_EVENT_MANAGER_GENERATION_MISMATCH;
    }
    for (index = 0U; index < VSS_EVENT_MANAGER_CAPACITY; ++index)
    {
        VssEventManager_Slot_t *slot = &manager->slots[index];
        if (slot->inUse &&
            (slot->identityGeneration == failure->identityGeneration) &&
            (slot->event.occurrenceId == failure->occurrenceId))
        {
            if (VssEventManager_IsExpired(slot, nowMs))
            {
                slot->inUse = false;
                return VSS_EVENT_MANAGER_IGNORED;
            }
            slot->submitted = false;
            return VSS_EVENT_MANAGER_OK;
        }
    }
    return VSS_EVENT_MANAGER_NOT_FOUND;
}

VssEventManager_Status_t VssEventManager_Poll(
    VssEventManager_t *manager,
    uint32_t nowMs)
{
    size_t index;
    VssEventManager_Status_t status;

    if (manager == NULL)
    {
        return VSS_EVENT_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return VSS_EVENT_MANAGER_NOT_INITIALIZED;
    }
    status = VssEventManager_RefreshGeneration(manager);
    if (status != VSS_EVENT_MANAGER_OK)
    {
        return status;
    }
    for (index = 0U; index < VSS_EVENT_MANAGER_CAPACITY; ++index)
    {
        VssEventManager_Slot_t *slot = &manager->slots[index];
        if (!slot->inUse)
        {
            continue;
        }
        if (VssEventManager_IsExpired(slot, nowMs))
        {
            slot->inUse = false;
            continue;
        }
        if (!slot->submitted)
        {
            slot->event.meta.ageMs = VssEventManager_SaturatingAdd(
                slot->originalAgeMs,
                (uint32_t)(nowMs - slot->createdAtMs));
            if (manager->tx(&slot->event) == DOMAIN_IF_OK)
            {
                slot->submitted = true;
            }
        }
    }
    return VSS_EVENT_MANAGER_OK;
}
