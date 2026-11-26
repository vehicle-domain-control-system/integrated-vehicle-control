#ifndef VSS_EVENT_MANAGER_H
#define VSS_EVENT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VSS_EVENT_MANAGER_CAPACITY 4U
#define VSS_EVENT_MANAGER_USE_LIMIT_MS 500U

typedef enum
{
    VSS_EVENT_MANAGER_OK = 0,
    VSS_EVENT_MANAGER_INVALID_ARGUMENT,
    VSS_EVENT_MANAGER_NOT_INITIALIZED,
    VSS_EVENT_MANAGER_DUPLICATE,
    VSS_EVENT_MANAGER_IGNORED,
    VSS_EVENT_MANAGER_NO_CAPACITY,
    VSS_EVENT_MANAGER_NOT_FOUND,
    VSS_EVENT_MANAGER_GENERATION_UNAVAILABLE,
    VSS_EVENT_MANAGER_GENERATION_MISMATCH
} VssEventManager_Status_t;

typedef enum
{
    VSS_EVENT_MANAGER_VEHICLE_USE_STARTED = 0,
    VSS_EVENT_MANAGER_VEHICLE_USE_ENDED
} VssEventManager_VehicleUseTransition_t;

typedef DomainIf_Status_t (*VssEventManager_Tx_t)(
    const DomainIf_VssOneShotEvent_t *event);

typedef bool (*VssEventManager_GenerationProvider_t)(
    void *context,
    SourceGeneration_t *outGeneration);

typedef struct
{
    bool inUse;
    bool submitted;
    uint32_t createdAtMs;
    uint32_t originalAgeMs;
    SourceGeneration_t identityGeneration;
    uint32_t sourceOccurrenceId;
    DomainCommandId_t sourceCommandId;
    DomainIf_VssOneShotEvent_t event;
} VssEventManager_Slot_t;

typedef struct
{
    VssEventManager_Slot_t slots[VSS_EVENT_MANAGER_CAPACITY];
    OccurrenceId_t nextOccurrenceId;
    VssEventManager_Tx_t tx;
    VssEventManager_GenerationProvider_t generationProvider;
    void *generationProviderContext;
    SourceGeneration_t identityGeneration;
    bool generationKnown;
    bool initialized;
} VssEventManager_t;

VssEventManager_Status_t VssEventManager_Init(
    VssEventManager_t *manager,
    VssEventManager_Tx_t tx);

/*
 * Binds the private producer-generation source used to correlate a Network
 * LocalFailure with the exact event run that created it. A missing/zero
 * generation keeps event production fail-closed. A changed generation drops
 * pending slots rather than relabelling them into the new Network run.
 */
VssEventManager_Status_t VssEventManager_BindGenerationProvider(
    VssEventManager_t *manager,
    VssEventManager_GenerationProvider_t provider,
    void *providerContext);

VssEventManager_Status_t VssEventManager_RecordBcmDoorResult(
    VssEventManager_t *manager,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs);

/* Host/policy entry only. Runtime has no confirmed vehicle-use transition
 * provider yet and therefore does not call this API. */
VssEventManager_Status_t VssEventManager_RecordVehicleUseTransition(
    VssEventManager_t *manager,
    VssEventManager_VehicleUseTransition_t transition,
    uint32_t nowMs);

VssEventManager_Status_t VssEventManager_RecordNetworkLocalFailure(
    VssEventManager_t *manager,
    const NetworkLocalFailure_t *failure,
    uint32_t nowMs);

VssEventManager_Status_t VssEventManager_Poll(
    VssEventManager_t *manager,
    uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* VSS_EVENT_MANAGER_H */
