#ifndef VEHICLE_STATE_MANAGER_H
#define VEHICLE_STATE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-internal state cache engine.
 *
 * This header deliberately does not reproduce any DomainIf or vehicle type.
 * The integration layer supplies one statically allocated value buffer per
 * slot, sized for the real C2 type when that package becomes available.
 *
 * Concurrency contract: all APIs are called from one serialized owner context,
 * or the caller provides synchronization around the complete API call.
 */

typedef enum
{
    VEHICLE_STATE_QUALITY_OK = 0,
    VEHICLE_STATE_QUALITY_STALE,
    VEHICLE_STATE_QUALITY_INVALID,
    VEHICLE_STATE_QUALITY_NO_DATA
} VehicleStateQuality_t;

typedef enum
{
    VEHICLE_STATE_MANAGER_OK = 0,
    VEHICLE_STATE_MANAGER_INVALID_ARGUMENT,
    VEHICLE_STATE_MANAGER_NOT_INITIALIZED,
    VEHICLE_STATE_MANAGER_INVALID_SLOT,
    VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED,
    VEHICLE_STATE_MANAGER_DUPLICATE,
    VEHICLE_STATE_MANAGER_OUT_OF_ORDER,
    VEHICLE_STATE_MANAGER_NO_DATA
} VehicleStateManager_Status_t;

typedef size_t VehicleStateManager_SlotId_t;

typedef struct
{
    void *valueStorage;
    size_t valueCapacity;
} VehicleStateManager_SlotConfig_t;

typedef struct
{
    const void *value;
    size_t valueSize;
    VehicleStateQuality_t quality;
    uint32_t sourceAgeMs;
    uint32_t updateSequence;
    uint32_t sourceGeneration;
    uint32_t receivedAtMs;
} VehicleStateManager_Update_t;

typedef struct
{
    bool hasObservation;
    bool hasValue;
    bool sourceGenerationChanged;
    VehicleStateQuality_t quality;
    uint32_t sourceAgeMs;
    uint32_t localElapsedMs;
    uint32_t effectiveAgeMs;
    uint32_t updateSequence;
    uint32_t sourceGeneration;
    uint32_t receivedAtMs;
    size_t valueSize;
} VehicleStateManager_Snapshot_t;

typedef struct
{
    uint8_t *valueStorage;
    size_t valueCapacity;
    size_t valueSize;
    VehicleStateQuality_t quality;
    uint32_t sourceAgeMs;
    uint32_t updateSequence;
    uint32_t sourceGeneration;
    uint32_t receivedAtMs;
    bool hasObservation;
    bool hasValue;
    bool sourceGenerationChanged;
} VehicleStateManager_Slot_t;

typedef struct
{
    VehicleStateManager_Slot_t *slots;
    size_t slotCount;
    bool initialized;
} VehicleStateManager_t;

VehicleStateManager_Status_t VehicleStateManager_Init(
    VehicleStateManager_t *manager,
    VehicleStateManager_Slot_t *slots,
    const VehicleStateManager_SlotConfig_t *slotConfigs,
    size_t slotCount);

VehicleStateManager_Status_t VehicleStateManager_Update(
    VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    const VehicleStateManager_Update_t *update);

VehicleStateManager_Status_t VehicleStateManager_GetSnapshot(
    const VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    uint32_t nowMs,
    uint32_t ttlMs,
    void *outValue,
    size_t outValueCapacity,
    VehicleStateManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_STATE_MANAGER_H */
