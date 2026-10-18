#include "domain_interface_state_binding.h"

#include "domain_interface_core_internal.h"

typedef struct
{
    VehicleStateManager_t *stateManager;
    DomainIfStateBinding_SlotMap_t slots;
    DomainIfStateBinding_TimeProvider_t timeProvider;
    void *timeProviderContext;
    bool configured;
} DomainIfStateBinding_Context_t;

static DomainIfStateBinding_Context_t g_stateBinding;

static bool DomainIfStateBinding_AreSlotsDistinct(
    const DomainIfStateBinding_SlotMap_t *slots)
{
    return (slots->bcmDoorState != slots->bcmStatus) &&
           (slots->bcmDoorState != slots->cisRearState) &&
           (slots->bcmDoorState != slots->cisStatus) &&
           (slots->bcmStatus != slots->cisRearState) &&
           (slots->bcmStatus != slots->cisStatus) &&
           (slots->cisRearState != slots->cisStatus);
}

static bool DomainIfStateBinding_IsSlotUsable(
    const VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    size_t requiredCapacity)
{
    return manager->initialized && (manager->slots != NULL) &&
           (manager->slotCount > 0U) && (slotId < manager->slotCount) &&
           (manager->slots[slotId].valueStorage != NULL) &&
           (manager->slots[slotId].valueCapacity >= requiredCapacity);
}

static DomainIf_Status_t DomainIfStateBinding_MapQuality(
    DataQuality_t quality,
    VehicleStateQuality_t *outQuality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
            *outQuality = VEHICLE_STATE_QUALITY_OK;
            return DOMAIN_IF_OK;
        case DATA_QUALITY_STALE:
            *outQuality = VEHICLE_STATE_QUALITY_STALE;
            return DOMAIN_IF_OK;
        case DATA_QUALITY_INVALID:
            *outQuality = VEHICLE_STATE_QUALITY_INVALID;
            return DOMAIN_IF_OK;
        case DATA_QUALITY_NO_DATA:
            *outQuality = VEHICLE_STATE_QUALITY_NO_DATA;
            return DOMAIN_IF_OK;
        default:
            return DOMAIN_IF_INVALID_ARGUMENT;
    }
}

static DomainIf_Status_t DomainIfStateBinding_GetNow(uint32_t *outNowMs)
{
    if (!g_stateBinding.configured ||
        (g_stateBinding.stateManager == NULL) ||
        (g_stateBinding.timeProvider == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_stateBinding.timeProvider(g_stateBinding.timeProviderContext,
                                     outNowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIfStateBinding_MapManagerStatus(
    VehicleStateManager_Status_t status,
    bool *outCommitted)
{
    *outCommitted = false;

    switch (status)
    {
        case VEHICLE_STATE_MANAGER_OK:
            *outCommitted = true;
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_MANAGER_DUPLICATE:
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_MANAGER_OUT_OF_ORDER:
            return DOMAIN_IF_REJECTED;
        case VEHICLE_STATE_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case VEHICLE_STATE_MANAGER_NOT_INITIALIZED:
        case VEHICLE_STATE_MANAGER_INVALID_SLOT:
        case VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED:
            return DOMAIN_IF_NOT_AVAILABLE;
        case VEHICLE_STATE_MANAGER_NO_DATA:
        default:
            return DOMAIN_IF_REJECTED;
    }
}

static DomainIf_Status_t DomainIfStateBinding_Store(
    VehicleStateManager_SlotId_t slotId,
    const void *state,
    size_t stateSize,
    const SignalMeta_t *meta,
    SourceGeneration_t generation,
    bool *outCommitted)
{
    VehicleStateManager_Update_t update;
    VehicleStateManager_Status_t managerStatus;
    DomainIf_Status_t status;

    status = DomainIfStateBinding_MapQuality(meta->quality, &update.quality);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    status = DomainIfStateBinding_GetNow(&update.receivedAtMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    if (update.quality == VEHICLE_STATE_QUALITY_NO_DATA)
    {
        update.value = NULL;
        update.valueSize = 0U;
    }
    else
    {
        update.value = state;
        update.valueSize = stateSize;
    }
    update.sourceAgeMs = meta->ageMs;
    update.updateSequence = meta->updateSequence;
    update.sourceGeneration = generation;

    managerStatus = VehicleStateManager_Update(
        g_stateBinding.stateManager, slotId, &update);
    return DomainIfStateBinding_MapManagerStatus(managerStatus, outCommitted);
}

DomainIf_Status_t DomainIfStateBinding_Configure(
    VehicleStateManager_t *stateManager,
    const DomainIfStateBinding_SlotMap_t *slotMap,
    DomainIfStateBinding_TimeProvider_t timeProvider,
    void *timeProviderContext)
{
    if ((stateManager == NULL) || (slotMap == NULL) ||
        (timeProvider == NULL) ||
        !DomainIfStateBinding_AreSlotsDistinct(slotMap))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!DomainIfStateBinding_IsSlotUsable(
            stateManager, slotMap->bcmDoorState, sizeof(DoorState_t)) ||
        !DomainIfStateBinding_IsSlotUsable(
            stateManager, slotMap->bcmStatus,
            sizeof(DomainIf_BcmStatusReport_t)) ||
        !DomainIfStateBinding_IsSlotUsable(
            stateManager, slotMap->cisRearState, sizeof(CisRearState_t)) ||
        !DomainIfStateBinding_IsSlotUsable(
            stateManager, slotMap->cisStatus,
            sizeof(DomainIf_CisStatusReport_t)))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    g_stateBinding.stateManager = stateManager;
    g_stateBinding.slots = *slotMap;
    g_stateBinding.timeProvider = timeProvider;
    g_stateBinding.timeProviderContext = timeProviderContext;
    g_stateBinding.configured = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxBcmDoorState(const DoorState_t *state)
{
    bool committed;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIfCore_IsDoorStateValid(state))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    /* C2 v0.5 has no door-state observer; commit is the terminal action. */
    return DomainIfStateBinding_Store(
        g_stateBinding.slots.bcmDoorState, state, sizeof(*state),
        &state->meta, state->sourceGeneration, &committed);
}

DomainIf_Status_t DomainIf_RxBcmStatus(
    const DomainIf_BcmStatusReport_t *statusReport)
{
    DomainIf_Status_t status;
    bool committed;

    if (statusReport == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIfCore_IsBcmStatusValid(statusReport))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfStateBinding_Store(
        g_stateBinding.slots.bcmStatus, statusReport, sizeof(*statusReport),
        &statusReport->meta, statusReport->sourceGeneration, &committed);
    if ((status != DOMAIN_IF_OK) || !committed)
    {
        return status;
    }
    return DomainIfCore_NotifyBcmStatus(statusReport);
}

DomainIf_Status_t DomainIf_RxCisRear(const CisRearState_t *state)
{
    bool committed;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIfCore_IsCisRearValid(state))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    /* C2 v0.5 has no CIS-rear observer; commit is the terminal action. */
    return DomainIfStateBinding_Store(
        g_stateBinding.slots.cisRearState, state, sizeof(*state),
        &state->distanceQuality.meta, state->sourceGeneration, &committed);
}

DomainIf_Status_t DomainIf_RxCisStatus(
    const DomainIf_CisStatusReport_t *statusReport)
{
    DomainIf_Status_t status;
    bool committed;

    if (statusReport == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIfCore_IsCisStatusValid(statusReport))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfStateBinding_Store(
        g_stateBinding.slots.cisStatus, statusReport, sizeof(*statusReport),
        &statusReport->meta, statusReport->sourceGeneration, &committed);
    if ((status != DOMAIN_IF_OK) || !committed)
    {
        return status;
    }
    return DomainIfCore_NotifyCisStatus(statusReport);
}
