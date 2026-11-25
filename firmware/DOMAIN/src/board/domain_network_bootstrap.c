#include "domain_network_bootstrap.h"

#include <stddef.h>

#include "network/app/net_if_entry.h"

static DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Observe(
    DomainNetworkBootstrap_t *context)
{
    DomainNetworkBringup_Status_t bringupStatus;
    DomainNetworkBringup_State_t bringupState;

    bringupStatus = DomainNetworkBringup_ObserveNetworkStatus(context->bringup);
    bringupState = DomainNetworkBringup_GetState(context->bringup);

    if (bringupState == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        context->status = DOMAIN_NETWORK_BOOTSTRAP_FAULT;
    }
    else if (bringupState == DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED)
    {
        /*
         * Fail closed. The current public Network API exposes Boot ID but no
         * authoritative port epoch, and no Production evidence deadline has
         * been approved. RequestNormal must not be called here.
         */
        context->status = DOMAIN_NETWORK_BOOTSTRAP_PREPARED_NORMAL_BLOCKED;
    }
    else if (bringupStatus == DOMAIN_NETWORK_BRINGUP_PENDING)
    {
        context->status = DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING;
    }
    else
    {
        (void)DomainNetworkBringup_ReportFault(
            context->bringup,
            DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
        context->status = DOMAIN_NETWORK_BOOTSTRAP_FAULT;
    }

    return context->status;
}

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Init(
    DomainNetworkBootstrap_t *context,
    DomainNetworkBringup_t *bringup)
{
    if ((context == NULL) || (bringup == NULL))
    {
        return DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED;
    }
    if (context->initialized)
    {
        return DOMAIN_NETWORK_BOOTSTRAP_ALREADY;
    }

    context->initialized = true;
    context->netInitCalled = false;
    context->bringup = bringup;
    context->status = DOMAIN_NETWORK_BOOTSTRAP_READY;
    return context->status;
}

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Start(
    DomainNetworkBootstrap_t *context,
    bool hardwareGateOpen)
{
    DomainNetworkBringup_Status_t bringupStatus;

    if ((context == NULL) || !context->initialized ||
        (context->bringup == NULL))
    {
        return DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED;
    }
    if (context->netInitCalled)
    {
        return DOMAIN_NETWORK_BOOTSTRAP_ALREADY;
    }
    if (!hardwareGateOpen)
    {
        context->status = DOMAIN_NETWORK_BOOTSTRAP_DEFERRED_HARDWARE_GATE;
        return context->status;
    }

    bringupStatus = DomainNetworkBringup_BeginNetworkInitialization(
        context->bringup);
    if (bringupStatus != DOMAIN_NETWORK_BRINGUP_OK)
    {
        if (bringupStatus == DOMAIN_NETWORK_BRINGUP_NOT_APPROVED)
        {
            context->status = DOMAIN_NETWORK_BOOTSTRAP_NOT_APPROVED;
            return context->status;
        }
        if (bringupStatus != DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED)
        {
            (void)DomainNetworkBringup_ReportFault(
                context->bringup,
                DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
        }
        context->status = DOMAIN_NETWORK_BOOTSTRAP_FAULT;
        return context->status;
    }

    /* Set before the one-shot call so no later path can submit it again. */
    context->netInitCalled = true;
    Net_Init();
    return DomainNetworkBootstrap_Observe(context);
}

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_Poll(
    DomainNetworkBootstrap_t *context)
{
    if ((context == NULL) || !context->initialized ||
        (context->bringup == NULL))
    {
        return DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED;
    }
    if (!context->netInitCalled)
    {
        return context->status;
    }
    if (context->status == DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING)
    {
        return DomainNetworkBootstrap_Observe(context);
    }
    return DOMAIN_NETWORK_BOOTSTRAP_ALREADY;
}

DomainNetworkBootstrap_Status_t
DomainNetworkBootstrap_ReportInitializationTimeout(
    DomainNetworkBootstrap_t *context)
{
    if ((context == NULL) || !context->initialized ||
        (context->bringup == NULL))
    {
        return DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED;
    }
    if (context->status == DOMAIN_NETWORK_BOOTSTRAP_FAULT)
    {
        return DOMAIN_NETWORK_BOOTSTRAP_ALREADY;
    }
    if (context->status != DOMAIN_NETWORK_BOOTSTRAP_INITIALIZING)
    {
        return DOMAIN_NETWORK_BOOTSTRAP_INVALID_STATE;
    }

    (void)DomainNetworkBringup_ReportFault(
        context->bringup,
        DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
    context->status = DOMAIN_NETWORK_BOOTSTRAP_FAULT;
    return context->status;
}

DomainNetworkBootstrap_Status_t DomainNetworkBootstrap_GetStatus(
    const DomainNetworkBootstrap_t *context)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_NETWORK_BOOTSTRAP_NOT_INITIALIZED;
    }
    return context->status;
}

bool DomainNetworkBootstrap_WasNetInitCalled(
    const DomainNetworkBootstrap_t *context)
{
    return (context != NULL) && context->initialized && context->netInitCalled;
}
