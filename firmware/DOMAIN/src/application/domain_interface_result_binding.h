#ifndef DOMAIN_INTERFACE_RESULT_BINDING_H
#define DOMAIN_INTERFACE_RESULT_BINDING_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "result_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The provider must return a rollover-capable monotonic uint32_t millisecond
 * value. false means that no trustworthy timestamp is currently available.
 */
typedef bool (*DomainIfResultBinding_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

/*
 * Bind once after ResultManager_Init() and before Network ingress starts.
 * Calls and any controlled reconfiguration must be serialized by the single
 * Domain owner; ISR use and concurrent rebinding are not supported.
 * The ResultManager instance and provider context must outlive the binding.
 */
DomainIf_Status_t DomainIfResultBinding_Configure(
    ResultManager_t *resultManager,
    DomainIfResultBinding_TimeProvider_t timeProvider,
    void *timeProviderContext);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_RESULT_BINDING_H */
