#ifndef DOMAIN_INTERFACE_CIS_OCCUPANT_BINDING_H
#define DOMAIN_INTERFACE_CIS_OCCUPANT_BINDING_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "cis_occupant_manager.h"
#include "domain_cis_occupant_private.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*DomainIfCisOccupantBinding_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

/* Internal lifecycle hook. Network ingress must already be closed. */
void DomainIfCisOccupantBinding_Reset(void);

/*
 * Binds an already initialized manager and private admission context. The
 * caller retains ownership and serializes Transition/Rx/Query calls in one
 * non-ISR context for the binding lifetime. Initial Rx remains blocked until
 * a private transition has synchronously admitted a producer generation.
 */
DomainIf_Status_t DomainIfCisOccupantBinding_Configure(
    CisOccupantManager_t *occupantManager,
    DomainCisOccupantPriv_t *privateContext,
    DomainIfCisOccupantBinding_TimeProvider_t timeProvider,
    void *timeProviderContext);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_CIS_OCCUPANT_BINDING_H */
