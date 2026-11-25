#ifndef DOMAIN_NETWORK_TRANSITION_BINDING_H
#define DOMAIN_NETWORK_TRANSITION_BINDING_H

/*
 * Domain-owned adapter over the Network public transition contract.
 * Register the returned binding before Net_Init(). Network invokes it only
 * from the agreed serialized non-ISR owner after old callbacks are drained.
 */
#include "network/net_domain_c2.h"
#include "domain_runtime.h"

NetDomainC2TransitionBinding domain_network_transition_binding(
    DomainRuntime_t *runtime);
NetDomainC2TransitionStatus domain_transition_environment_status(
    DomainCisEnvironmentPriv_Status_t status);
NetDomainC2TransitionStatus domain_transition_occupant_status(
    DomainCisOccupantPriv_Status_t status);

#endif
