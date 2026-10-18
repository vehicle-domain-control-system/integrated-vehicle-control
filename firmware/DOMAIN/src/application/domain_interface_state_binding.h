#ifndef DOMAIN_INTERFACE_STATE_BINDING_H
#define DOMAIN_INTERFACE_STATE_BINDING_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "vehicle_state_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*DomainIfStateBinding_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

typedef struct
{
    VehicleStateManager_SlotId_t bcmDoorState;
    VehicleStateManager_SlotId_t bcmStatus;
    VehicleStateManager_SlotId_t cisRearState;
    VehicleStateManager_SlotId_t cisStatus;
} DomainIfStateBinding_SlotMap_t;

/*
 * Configure once after DomainIf_Init() and VehicleStateManager_Init(), before
 * Network ingress starts. Calls and controlled reconfiguration are serialized
 * by the single Domain owner. ISR use and concurrent rebinding are unsupported.
 * The manager, its slot storage, time provider and provider context must remain
 * valid for the complete binding lifetime.
 */
DomainIf_Status_t DomainIfStateBinding_Configure(
    VehicleStateManager_t *stateManager,
    const DomainIfStateBinding_SlotMap_t *slotMap,
    DomainIfStateBinding_TimeProvider_t timeProvider,
    void *timeProviderContext);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_STATE_BINDING_H */
