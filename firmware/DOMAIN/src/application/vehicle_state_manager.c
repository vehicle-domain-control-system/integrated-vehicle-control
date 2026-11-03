#include "vehicle_state_manager.h"

#include <limits.h>
#include <string.h>

#define VEHICLE_STATE_SEQUENCE_HALF_RANGE (UINT32_C(0x80000000))

static bool VehicleStateManager_IsQualityValid(VehicleStateQuality_t quality)
{
    return (quality == VEHICLE_STATE_QUALITY_OK) ||
           (quality == VEHICLE_STATE_QUALITY_STALE) ||
           (quality == VEHICLE_STATE_QUALITY_INVALID) ||
           (quality == VEHICLE_STATE_QUALITY_NO_DATA);
}

static uint32_t VehicleStateManager_SaturatingAddU32(uint32_t left, uint32_t right)
{
    if (right > (UINT32_MAX - left))
    {
        return UINT32_MAX;
    }

    return left + right;
}

static bool VehicleStateManager_IsInitialized(const VehicleStateManager_t *manager)
{
    return (manager != NULL) && manager->initialized &&
           (manager->slots != NULL) && (manager->slotCount > 0U);
}

static VehicleStateManager_Status_t VehicleStateManager_CheckSequence(
    const VehicleStateManager_Slot_t *slot,
    const VehicleStateManager_Update_t *update)
{
    uint32_t delta;

    if ((!slot->hasObservation) ||
        (slot->sourceGeneration != update->sourceGeneration))
    {
        return VEHICLE_STATE_MANAGER_OK;
    }

    delta = update->updateSequence - slot->updateSequence;
    if (delta == 0U)
    {
        return VEHICLE_STATE_MANAGER_DUPLICATE;
    }

    /* RFC-1982 style serial comparison. Exactly half-range is ambiguous. */
    if (delta >= VEHICLE_STATE_SEQUENCE_HALF_RANGE)
    {
        return VEHICLE_STATE_MANAGER_OUT_OF_ORDER;
    }

    return VEHICLE_STATE_MANAGER_OK;
}

VehicleStateManager_Status_t VehicleStateManager_Init(
    VehicleStateManager_t *manager,
    VehicleStateManager_Slot_t *slots,
    const VehicleStateManager_SlotConfig_t *slotConfigs,
    size_t slotCount)
{
    size_t index;

    if ((manager == NULL) || (slots == NULL) ||
        (slotConfigs == NULL) || (slotCount == 0U))
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    manager->slots = NULL;
    manager->slotCount = 0U;
    manager->initialized = false;

    for (index = 0U; index < slotCount; ++index)
    {
        if ((slotConfigs[index].valueStorage == NULL) ||
            (slotConfigs[index].valueCapacity == 0U))
        {
            return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
        }
    }

    for (index = 0U; index < slotCount; ++index)
    {
        VehicleStateManager_Slot_t *slot = &slots[index];

        (void)memset(slotConfigs[index].valueStorage,
                     0,
                     slotConfigs[index].valueCapacity);
        slot->valueStorage = (uint8_t *)slotConfigs[index].valueStorage;
        slot->valueCapacity = slotConfigs[index].valueCapacity;
        slot->valueSize = 0U;
        slot->quality = VEHICLE_STATE_QUALITY_NO_DATA;
        slot->sourceAgeMs = 0U;
        slot->updateSequence = 0U;
        slot->sourceGeneration = 0U;
        slot->receivedAtMs = 0U;
        slot->hasObservation = false;
        slot->hasValue = false;
        slot->sourceGenerationChanged = false;
    }

    manager->slots = slots;
    manager->slotCount = slotCount;
    manager->initialized = true;

    return VEHICLE_STATE_MANAGER_OK;
}

VehicleStateManager_Status_t VehicleStateManager_Update(
    VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    const VehicleStateManager_Update_t *update)
{
    VehicleStateManager_Slot_t *slot;
    VehicleStateManager_Status_t sequenceStatus;
    bool generationChanged;

    if (!VehicleStateManager_IsInitialized(manager))
    {
        return (manager == NULL) ? VEHICLE_STATE_MANAGER_INVALID_ARGUMENT
                                 : VEHICLE_STATE_MANAGER_NOT_INITIALIZED;
    }

    if (update == NULL)
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    if (slotId >= manager->slotCount)
    {
        return VEHICLE_STATE_MANAGER_INVALID_SLOT;
    }

    if (!VehicleStateManager_IsQualityValid(update->quality))
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    if (update->quality == VEHICLE_STATE_QUALITY_NO_DATA)
    {
        if ((update->value != NULL) || (update->valueSize != 0U))
        {
            return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
        }
    }
    else if ((update->value == NULL) || (update->valueSize == 0U))
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    slot = &manager->slots[slotId];
    if (update->valueSize > slot->valueCapacity)
    {
        return VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED;
    }

    sequenceStatus = VehicleStateManager_CheckSequence(slot, update);
    if (sequenceStatus != VEHICLE_STATE_MANAGER_OK)
    {
        return sequenceStatus;
    }

    generationChanged = slot->hasObservation &&
                        (slot->sourceGeneration != update->sourceGeneration);

    if (update->valueSize > 0U)
    {
        (void)memcpy(slot->valueStorage, update->value, update->valueSize);
        slot->hasValue = true;
    }
    else
    {
        slot->hasValue = false;
    }

    slot->valueSize = update->valueSize;
    slot->quality = update->quality;
    slot->sourceAgeMs = update->sourceAgeMs;
    slot->updateSequence = update->updateSequence;
    slot->sourceGeneration = update->sourceGeneration;
    slot->receivedAtMs = update->receivedAtMs;
    slot->hasObservation = true;
    slot->sourceGenerationChanged = generationChanged;

    return VEHICLE_STATE_MANAGER_OK;
}

VehicleStateManager_Status_t VehicleStateManager_WithdrawSameSequence(
    VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    const VehicleStateManager_Update_t *update)
{
    VehicleStateManager_Slot_t *slot;
    uint32_t preservedAgeMs;

    if (!VehicleStateManager_IsInitialized(manager))
    {
        return (manager == NULL) ? VEHICLE_STATE_MANAGER_INVALID_ARGUMENT
                                 : VEHICLE_STATE_MANAGER_NOT_INITIALIZED;
    }
    if (update == NULL)
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }
    if (slotId >= manager->slotCount)
    {
        return VEHICLE_STATE_MANAGER_INVALID_SLOT;
    }
    if (!VehicleStateManager_IsQualityValid(update->quality) ||
        (update->quality == VEHICLE_STATE_QUALITY_OK))
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }
    if (update->quality == VEHICLE_STATE_QUALITY_NO_DATA)
    {
        if ((update->value != NULL) || (update->valueSize != 0U))
        {
            return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
        }
    }
    else if ((update->value == NULL) || (update->valueSize == 0U))
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    slot = &manager->slots[slotId];
    if (update->valueSize > slot->valueCapacity)
    {
        return VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED;
    }
    if (!slot->hasObservation ||
        (slot->sourceGeneration != update->sourceGeneration) ||
        (slot->updateSequence != update->updateSequence) ||
        (slot->quality != VEHICLE_STATE_QUALITY_OK))
    {
        return VEHICLE_STATE_MANAGER_CONFLICT;
    }

    preservedAgeMs = VehicleStateManager_SaturatingAddU32(
        slot->sourceAgeMs, update->receivedAtMs - slot->receivedAtMs);
    if (update->sourceAgeMs > preservedAgeMs)
    {
        preservedAgeMs = update->sourceAgeMs;
    }

    if (update->valueSize > 0U)
    {
        (void)memcpy(slot->valueStorage, update->value, update->valueSize);
        slot->hasValue = true;
    }
    else
    {
        slot->hasValue = false;
    }
    slot->valueSize = update->valueSize;
    slot->quality = update->quality;
    slot->sourceAgeMs = preservedAgeMs;
    slot->receivedAtMs = update->receivedAtMs;
    slot->sourceGenerationChanged = false;
    return VEHICLE_STATE_MANAGER_OK;
}

VehicleStateManager_Status_t VehicleStateManager_GetSnapshot(
    const VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    uint32_t nowMs,
    uint32_t ttlMs,
    void *outValue,
    size_t outValueCapacity,
    VehicleStateManager_Snapshot_t *outSnapshot)
{
    const VehicleStateManager_Slot_t *slot;
    VehicleStateManager_Snapshot_t snapshot;

    if (!VehicleStateManager_IsInitialized(manager))
    {
        return (manager == NULL) ? VEHICLE_STATE_MANAGER_INVALID_ARGUMENT
                                 : VEHICLE_STATE_MANAGER_NOT_INITIALIZED;
    }

    if (outSnapshot == NULL)
    {
        return VEHICLE_STATE_MANAGER_INVALID_ARGUMENT;
    }

    if (slotId >= manager->slotCount)
    {
        return VEHICLE_STATE_MANAGER_INVALID_SLOT;
    }

    slot = &manager->slots[slotId];
    snapshot.hasObservation = slot->hasObservation;
    snapshot.hasValue = slot->hasValue;
    snapshot.sourceGenerationChanged = slot->sourceGenerationChanged;
    snapshot.quality = slot->quality;
    snapshot.sourceAgeMs = slot->sourceAgeMs;
    snapshot.localElapsedMs = 0U;
    snapshot.effectiveAgeMs = slot->sourceAgeMs;
    snapshot.updateSequence = slot->updateSequence;
    snapshot.sourceGeneration = slot->sourceGeneration;
    snapshot.receivedAtMs = slot->receivedAtMs;
    snapshot.valueSize = slot->valueSize;

    if (!slot->hasObservation)
    {
        *outSnapshot = snapshot;
        return VEHICLE_STATE_MANAGER_NO_DATA;
    }

    snapshot.localElapsedMs = nowMs - slot->receivedAtMs;
    snapshot.effectiveAgeMs = VehicleStateManager_SaturatingAddU32(
        slot->sourceAgeMs,
        snapshot.localElapsedMs);

    if ((snapshot.quality == VEHICLE_STATE_QUALITY_OK) &&
        (snapshot.effectiveAgeMs >= ttlMs))
    {
        snapshot.quality = VEHICLE_STATE_QUALITY_STALE;
    }

    if (slot->hasValue)
    {
        if ((outValue == NULL) || (outValueCapacity < slot->valueSize))
        {
            return (outValue == NULL) ? VEHICLE_STATE_MANAGER_INVALID_ARGUMENT
                                      : VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED;
        }

        (void)memcpy(outValue, slot->valueStorage, slot->valueSize);
    }

    *outSnapshot = snapshot;
    return VEHICLE_STATE_MANAGER_OK;
}
