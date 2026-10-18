#include "domain_interface_result_binding.h"

#include "domain_interface_core_internal.h"

typedef struct
{
    ResultManager_t *resultManager;
    DomainIfResultBinding_TimeProvider_t timeProvider;
    void *timeProviderContext;
    bool configured;
} DomainIfResultBinding_Context_t;

static DomainIfResultBinding_Context_t g_resultBinding;

static DomainIf_Status_t DomainIfResultBinding_MapManagerStatus(
    ResultManager_Status_t status,
    bool *outCommitted)
{
    *outCommitted = false;

    switch (status)
    {
        case RESULT_MANAGER_OK:
        case RESULT_MANAGER_UNCONFIRMED_RECORDED:
            *outCommitted = true;
            return DOMAIN_IF_OK;
        case RESULT_MANAGER_DUPLICATE:
        case RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED:
            return DOMAIN_IF_OK;
        case RESULT_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case RESULT_MANAGER_NOT_INITIALIZED:
            return DOMAIN_IF_NOT_AVAILABLE;
        case RESULT_MANAGER_CAPACITY_EXCEEDED:
            return DOMAIN_IF_QUEUE_FULL;
        case RESULT_MANAGER_NOT_FOUND:
        case RESULT_MANAGER_ID_CONFLICT:
        case RESULT_MANAGER_COMMAND_CONFLICT:
        case RESULT_MANAGER_KIND_MISMATCH:
        case RESULT_MANAGER_INVALID_TRANSITION:
        case RESULT_MANAGER_EVIDENCE_REJECTED:
        case RESULT_MANAGER_FINAL_CONFLICT:
            return DOMAIN_IF_REJECTED;
        default:
            return DOMAIN_IF_REJECTED;
    }
}

static DomainIf_Status_t DomainIfResultBinding_GetNow(uint32_t *outNowMs)
{
    if (!g_resultBinding.configured ||
        (g_resultBinding.resultManager == NULL) ||
        (g_resultBinding.timeProvider == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_resultBinding.timeProvider(g_resultBinding.timeProviderContext,
                                      outNowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIfResultBinding_Configure(
    ResultManager_t *resultManager,
    DomainIfResultBinding_TimeProvider_t timeProvider,
    void *timeProviderContext)
{
    if ((resultManager == NULL) || (timeProvider == NULL))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    g_resultBinding.resultManager = resultManager;
    g_resultBinding.timeProvider = timeProvider;
    g_resultBinding.timeProviderContext = timeProviderContext;
    g_resultBinding.configured = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_RxBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result)
{
    ResultManager_Status_t managerStatus;
    DomainIf_Status_t status;
    uint32_t nowMs;
    bool committed;

    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfResultBinding_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    managerStatus = ResultManager_ApplyBcmResult(
        g_resultBinding.resultManager, result, nowMs);
    status = DomainIfResultBinding_MapManagerStatus(managerStatus, &committed);
    if ((status != DOMAIN_IF_OK) || !committed)
    {
        return status;
    }

    /* Core state is committed before the optional observer is notified. */
    return DomainIfCore_NotifyBcmCommandResult(result);
}

DomainIf_Status_t DomainIf_RxNetworkLocalFailure(
    const NetworkLocalFailure_t *failure)
{
    ResultManager_Status_t managerStatus;
    DomainIf_Status_t status;
    uint32_t nowMs;
    bool committed;

    if (failure == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (failure->itemKind == NETWORK_LOCAL_ITEM_OCCURRENCE)
    {
        /* No occurrence-keyed Application owner is implemented yet. */
        return DOMAIN_IF_UNSUPPORTED;
    }
    if ((failure->itemKind != NETWORK_LOCAL_ITEM_COMMAND) &&
        (failure->itemKind != NETWORK_LOCAL_ITEM_REQUEST_RESULT))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (failure->link == NETWORK_LINK_UART_C)
    {
        /* Pi UART-C is not a Domain-local Network link. */
        return DOMAIN_IF_UNSUPPORTED;
    }

    status = DomainIfResultBinding_GetNow(&nowMs);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }

    managerStatus = ResultManager_RecordNetworkLocalFailure(
        g_resultBinding.resultManager, failure, nowMs);
    status = DomainIfResultBinding_MapManagerStatus(managerStatus, &committed);
    (void)committed; /* No NetworkLocalFailure observer exists in C2 v0.5. */
    return status;
}
