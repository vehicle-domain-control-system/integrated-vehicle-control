#ifndef NETWORK_TESTS_DOMAIN_TRANSITION_BINDING_H
#define NETWORK_TESTS_DOMAIN_TRANSITION_BINDING_H
/* TEST ONLY. CIS transition binding (network/net_domain_c2.h
 * NetDomainC2TransitionBinding) over the actual DOMAIN runtime: it plays the
 * DOMAIN-side integration code that calls the DOMAIN private BOOT transitions
 * of the environment and of the Occupant. It needs DOMAIN application headers,
 * which the Network product code and the board files never include. */
#include "network/net_domain_c2.h"
#include "domain_runtime.h"

/* context = the DOMAIN runtime. Its private context missing (Occupant disabled,
 * runtime not ready) -> UNAVAILABLE without a DOMAIN call. */
NetDomainC2TransitionBinding domain_transition_binding(DomainRuntime_t *runtime);
NetDomainC2TransitionStatus domain_transition_environment_status(DomainCisEnvironmentPriv_Status_t status);
NetDomainC2TransitionStatus domain_transition_occupant_status(DomainCisOccupantPriv_Status_t status);
#endif
