#include "domain_interface_cis_environment_binding.h"

typedef struct
{
    CisEnvironmentManager_t *environmentManager;
    DomainCisEnvironmentPriv_t *privateContext;
    DomainIfCisEnvironmentBinding_TimeProvider_t timeProvider;
    void *timeProviderContext;
    bool configured;
} DomainIfCisEnvironmentBinding_Context_t;

static DomainIfCisEnvironmentBinding_Context_t g_cisEnvironmentBinding;

static DomainIf_Status_t DomainIfCisEnvironmentBinding_MapManagerStatus(
    CisEnvironmentManager_Status_t status)
{
    switch (status)
    {
        case CIS_ENVIRONMENT_MANAGER_OK:
            return DOMAIN_IF_OK;
        case CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED:
        default:
            return DOMAIN_IF_NOT_AVAILABLE;
    }
}

static DomainIf_Status_t DomainIfCisEnvironmentBinding_GetNow(
    uint32_t *outNowMs)
{
    if (!g_cisEnvironmentBinding.configured ||
        (g_cisEnvironmentBinding.environmentManager == NULL) ||
        (g_cisEnvironmentBinding.privateContext == NULL) ||
        (g_cisEnvironmentBinding.timeProvider == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_cisEnvironmentBinding.timeProvider(
            g_cisEnvironmentBinding.timeProviderContext, outNowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    return DOMAIN_IF_OK;
}

static bool DomainIfCisEnvironmentBinding_IsStoreAdmitted(
    const DomainCisEnvironmentPriv_Snapshot_t *snapshot)
{
    return snapshot->hasAppliedTransition &&
           snapshot->newDataAdmissionAllowed &&
           !snapshot->transitionInProgress &&
           (snapshot->state ==
            DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_WAITING_NEW_DATA);
}

DomainIf_Status_t DomainIfCisEnvironmentBinding_Initialize(
    CisEnvironmentManager_t *environmentManager,
    DomainCisEnvironmentPriv_t *privateContext,
    DomainIfCisEnvironmentBinding_TimeProvider_t timeProvider,
    void *timeProviderContext)
{
    CisEnvironmentManager_Status_t managerStatus;
    DomainCisEnvironmentPriv_Status_t privateStatus;

    if ((environmentManager == NULL) || (privateContext == NULL) ||
        (timeProvider == NULL))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    managerStatus = CisEnvironmentManager_Init(environmentManager);
    if (managerStatus != CIS_ENVIRONMENT_MANAGER_OK)
    {
        return DomainIfCisEnvironmentBinding_MapManagerStatus(managerStatus);
    }
    privateStatus = DomainCisEnvironmentPriv_Init(
        privateContext, environmentManager);
    if (privateStatus != DOMAIN_CIS_ENVIRONMENT_PRIV_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    g_cisEnvironmentBinding.environmentManager = environmentManager;
    g_cisEnvironmentBinding.privateContext = privateContext;
    g_cisEnvironmentBinding.timeProvider = timeProvider;
    g_cisEnvironmentBinding.timeProviderContext = timeProviderContext;
    g_cisEnvironmentBinding.configured = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxCisCabinEnvironment(
    const CisCabinEnvironmentState_t *state)
{
    DomainCisEnvironmentPriv_Snapshot_t privateSnapshot;
    DomainCisEnvironmentPriv_Status_t privateStatus;
    CisEnvironmentManager_Status_t managerStatus;
    DomainIf_Status_t status;
    uint32_t nowMs;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfCisEnvironmentBinding_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    privateStatus = DomainCisEnvironmentPriv_GetSnapshot(
        g_cisEnvironmentBinding.privateContext, &privateSnapshot);
    if (privateStatus != DOMAIN_CIS_ENVIRONMENT_PRIV_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!DomainIfCisEnvironmentBinding_IsStoreAdmitted(&privateSnapshot))
    {
        return DOMAIN_IF_REJECTED;
    }

    /*
     * Network must already have validated BOOT identity, drained the prior
     * callback set and admitted this aggregate under the current gate. The C2
     * v0.5 payload contains neither BOOT ID nor transition Epoch, so this
     * binding deliberately does not infer either from NO_DATA or sequences.
     */
    managerStatus = CisEnvironmentManager_Store(
        g_cisEnvironmentBinding.environmentManager, state, nowMs);
    return DomainIfCisEnvironmentBinding_MapManagerStatus(managerStatus);
}

DomainIf_Status_t DomainIf_QueryGetCisEnvironment(
    uint32_t nowMs,
    CisCabinEnvironmentState_t *outState)
{
    DomainCisEnvironmentPriv_Snapshot_t privateSnapshot;
    DomainCisEnvironmentPriv_Status_t privateStatus;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisEnvironmentManager_Status_t managerStatus;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_cisEnvironmentBinding.configured ||
        (g_cisEnvironmentBinding.environmentManager == NULL) ||
        (g_cisEnvironmentBinding.privateContext == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    privateStatus = DomainCisEnvironmentPriv_GetSnapshot(
        g_cisEnvironmentBinding.privateContext, &privateSnapshot);
    if (privateStatus != DOMAIN_CIS_ENVIRONMENT_PRIV_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (privateSnapshot.transitionInProgress ||
        (privateSnapshot.state ==
         DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_FAULT_BLOCKED))
    {
        /* Never expose a pre-transition OK value after barrier failure. */
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    managerStatus = CisEnvironmentManager_GetSnapshot(
        g_cisEnvironmentBinding.environmentManager, nowMs, &snapshot);
    status = DomainIfCisEnvironmentBinding_MapManagerStatus(managerStatus);
    if (status == DOMAIN_IF_OK)
    {
        /* A typed NO_DATA snapshot is a successful passive query result. */
        *outState = snapshot.effective;
    }
    return status;
}
