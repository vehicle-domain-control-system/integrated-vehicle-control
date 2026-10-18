#ifndef DOMAIN_INTERFACE_QUERY_BINDING_H
#define DOMAIN_INTERFACE_QUERY_BINDING_H

#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "domain_interface_state_binding.h"
#include "result_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t bcmDoorTtlMs;
    uint32_t bcmStatusTtlMs;
    uint32_t cisRearTtlMs;
    uint32_t cisStatusTtlMs;
} DomainIfQueryBinding_StatePolicy_t;

/*
 * Bind the already initialized state owner and caller-supplied freshness
 * policy. Calls are serialized with the corresponding Rx binding; concurrent
 * reconfiguration or ISR use is unsupported. The manager and its storage must
 * remain valid for the binding lifetime.
 */
DomainIf_Status_t DomainIfQueryBinding_ConfigureState(
    VehicleStateManager_t *stateManager,
    const DomainIfStateBinding_SlotMap_t *slotMap,
    const DomainIfQueryBinding_StatePolicy_t *policy);

/*
 * Bind the already initialized request/result history owner. Query getters
 * only use ResultManager's public read API and never expose its slot storage.
 */
DomainIf_Status_t DomainIfQueryBinding_ConfigureResults(
    ResultManager_t *resultManager);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_QUERY_BINDING_H */
