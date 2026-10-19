#include "domain_interface_cis_occupant_binding.h"

#include "domain_interface_core_internal.h"

#include <string.h>

typedef struct
{
    CisOccupantManager_t *occupantManager;
    DomainCisOccupantPriv_t *privateContext;
    DomainIfCisOccupantBinding_TimeProvider_t timeProvider;
    void *timeProviderContext;
    bool configured;
    bool disabled;
} DomainIfCisOccupantBinding_Context_t;

static DomainIfCisOccupantBinding_Context_t g_cisOccupantBinding;

void DomainIfCisOccupantBinding_Reset(void)
{
    (void)memset(&g_cisOccupantBinding, 0, sizeof(g_cisOccupantBinding));
}

DomainIf_Status_t DomainIfCisOccupantBinding_Disable(void)
{
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    DomainIfCisOccupantBinding_Reset();
    g_cisOccupantBinding.disabled = true;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIfCisOccupantBinding_MapManagerStatus(
    CisOccupantManager_Status_t status)
{
    switch (status)
    {
        case CIS_OCCUPANT_MANAGER_OK:
            return DOMAIN_IF_OK;
        case CIS_OCCUPANT_MANAGER_DUPLICATE:
            return DOMAIN_IF_OK;
        case CIS_OCCUPANT_MANAGER_OUT_OF_ORDER:
        case CIS_OCCUPANT_MANAGER_CONFLICT:
        case CIS_OCCUPANT_MANAGER_NOT_ADMITTED:
            return DOMAIN_IF_REJECTED;
        case CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case CIS_OCCUPANT_MANAGER_NOT_INITIALIZED:
        default:
            return DOMAIN_IF_NOT_AVAILABLE;
    }
}

DomainIf_Status_t DomainIfCisOccupantBinding_Configure(
    CisOccupantManager_t *occupantManager,
    DomainCisOccupantPriv_t *privateContext,
    DomainIfCisOccupantBinding_TimeProvider_t timeProvider,
    void *timeProviderContext)
{
    CisOccupantManager_Snapshot_t probe;
    DomainCisOccupantPriv_Snapshot_t privateProbe;

    if ((occupantManager == NULL) || (privateContext == NULL) ||
        (timeProvider == NULL))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (CisOccupantManager_GetSnapshot(occupantManager, 0U, &probe) !=
        CIS_OCCUPANT_MANAGER_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (DomainCisOccupantPriv_GetSnapshot(
            privateContext, &privateProbe) != DOMAIN_CIS_OCCUPANT_PRIV_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    g_cisOccupantBinding.occupantManager = occupantManager;
    g_cisOccupantBinding.privateContext = privateContext;
    g_cisOccupantBinding.timeProvider = timeProvider;
    g_cisOccupantBinding.timeProviderContext = timeProviderContext;
    g_cisOccupantBinding.configured = true;
    g_cisOccupantBinding.disabled = false;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxCisOccupant(const CisOccupantState_t *state)
{
    DomainCisOccupantPriv_Snapshot_t privateSnapshot;
    CisOccupantManager_Status_t managerStatus;
    uint32_t nowMs;

    if (state == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIfCore_IsCisOccupantValid(state))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (g_cisOccupantBinding.disabled)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_cisOccupantBinding.configured ||
        (g_cisOccupantBinding.occupantManager == NULL) ||
        (g_cisOccupantBinding.privateContext == NULL) ||
        (g_cisOccupantBinding.timeProvider == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    if (DomainCisOccupantPriv_GetSnapshot(
            g_cisOccupantBinding.privateContext, &privateSnapshot) !=
        DOMAIN_CIS_OCCUPANT_PRIV_OK)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!privateSnapshot.hasAppliedTransition ||
        !privateSnapshot.newDataAdmissionAllowed ||
        privateSnapshot.transitionInProgress ||
        (privateSnapshot.state !=
         DOMAIN_CIS_OCCUPANT_PRIV_STATE_WAITING_NEW_DATA) ||
        (state->sourceGeneration !=
         privateSnapshot.currentTransition.admittedSourceGeneration))
    {
        return DOMAIN_IF_REJECTED;
    }
    if (!g_cisOccupantBinding.timeProvider(
            g_cisOccupantBinding.timeProviderContext, &nowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    /*
     * The typed v0.5 observer table has no occupant callback. Successful
     * synchronous Manager commit is therefore the terminal Core action.
     */
    managerStatus = CisOccupantManager_Store(
        g_cisOccupantBinding.occupantManager, state, nowMs);
    return DomainIfCisOccupantBinding_MapManagerStatus(managerStatus);
}

DomainIf_Status_t DomainIf_QueryGetCisOccupant(
    uint32_t nowMs,
    CisOccupantState_t *outState)
{
    DomainCisOccupantPriv_Snapshot_t privateSnapshot;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Status_t managerStatus;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (g_cisOccupantBinding.disabled)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_cisOccupantBinding.configured ||
        (g_cisOccupantBinding.occupantManager == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if ((g_cisOccupantBinding.privateContext == NULL) ||
        (DomainCisOccupantPriv_GetSnapshot(
             g_cisOccupantBinding.privateContext, &privateSnapshot) !=
         DOMAIN_CIS_OCCUPANT_PRIV_OK))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (privateSnapshot.transitionInProgress ||
        (privateSnapshot.state ==
         DOMAIN_CIS_OCCUPANT_PRIV_STATE_FAULT_BLOCKED))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    managerStatus = CisOccupantManager_GetSnapshot(
        g_cisOccupantBinding.occupantManager, nowMs, &snapshot);
    status = DomainIfCisOccupantBinding_MapManagerStatus(managerStatus);
    if (status == DOMAIN_IF_OK)
    {
        /* Initial typed NO_DATA is a successful passive snapshot. */
        *outState = snapshot.effective;
    }
    return status;
}
