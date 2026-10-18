#ifndef DOMAIN_INTERFACE_CIS_ENVIRONMENT_BINDING_H
#define DOMAIN_INTERFACE_CIS_ENVIRONMENT_BINDING_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "cis_environment_manager.h"
#include "domain_cis_environment_private.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*DomainIfCisEnvironmentBinding_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

/*
 * Initializes and binds the Domain-owned CIS environment Manager and private
 * BOOT-transition context. Call after DomainIf_Init() and before Network
 * ingress starts. Explicit reinitialization is permitted only after Network
 * has closed delivery and drained queued/in-flight callbacks.
 *
 * The caller owns both fixed-size contexts and keeps them alive for the
 * binding lifetime. Rx, Query and private transition calls must be serialized
 * by one non-ISR owner. This module provides no lock, queue or RTOS binding.
 */
DomainIf_Status_t DomainIfCisEnvironmentBinding_Initialize(
    CisEnvironmentManager_t *environmentManager,
    DomainCisEnvironmentPriv_t *privateContext,
    DomainIfCisEnvironmentBinding_TimeProvider_t timeProvider,
    void *timeProviderContext);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_INTERFACE_CIS_ENVIRONMENT_BINDING_H */
