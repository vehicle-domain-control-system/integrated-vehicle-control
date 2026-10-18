#include "domain_cis_environment_private.h"

#include <string.h>

static bool DomainCisEnvironmentPriv_IsSameTransition(
    const DomainCisEnvironmentPriv_Transition_t *left,
    const DomainCisEnvironmentPriv_Transition_t *right)
{
    return (left->admittedCisBootId == right->admittedCisBootId) &&
           (left->transitionEpoch == right->transitionEpoch);
}

DomainCisEnvironmentPriv_Status_t DomainCisEnvironmentPriv_Init(
    DomainCisEnvironmentPriv_t *context,
    CisEnvironmentManager_t *environmentManager)
{
    if ((context == NULL) || (environmentManager == NULL))
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT;
    }

    (void)memset(context, 0, sizeof(*context));
    context->environmentManager = environmentManager;
    context->state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_READY;
    context->initialized = true;
    return DOMAIN_CIS_ENVIRONMENT_PRIV_OK;
}

DomainCisEnvironmentPriv_Status_t
DomainCisEnvironmentPriv_ApplyBootTransition(
    DomainCisEnvironmentPriv_t *context,
    const DomainCisEnvironmentPriv_Transition_t *transition,
    DomainCisEnvironmentPriv_TransitionReceipt_t *outReceipt)
{
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisEnvironmentManager_Status_t managerStatus;
    DomainCisEnvironmentPriv_TransitionEpoch_t expectedEpoch;

    if ((context == NULL) || (transition == NULL) || (outReceipt == NULL))
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED;
    }
    if (transition->admittedCisBootId == 0U)
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT;
    }
    if (transition->transitionEpoch == UINT64_C(0))
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT;
    }
    if (context->transitionInProgress)
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_BUSY;
    }

    if (context->hasTransitionIdentity)
    {
        if (transition->transitionEpoch ==
            context->currentTransition.transitionEpoch)
        {
            if (!DomainCisEnvironmentPriv_IsSameTransition(
                    transition, &context->currentTransition))
            {
                return DOMAIN_CIS_ENVIRONMENT_PRIV_ID_CONFLICT;
            }
            if (context->hasAppliedTransition)
            {
                *outReceipt = context->currentReceipt;
                return DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED;
            }

            /* A failed or incomplete transition is never retried into success. */
            return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED;
        }

        if (!context->hasAppliedTransition)
        {
            return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED;
        }
        if (context->currentTransition.transitionEpoch == UINT64_MAX)
        {
            return DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR;
        }

        expectedEpoch = context->currentTransition.transitionEpoch + UINT64_C(1);
        if (transition->transitionEpoch < context->currentTransition.transitionEpoch)
        {
            return DOMAIN_CIS_ENVIRONMENT_PRIV_STALE;
        }
        if (transition->transitionEpoch != expectedEpoch)
        {
            return DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR;
        }

        context->currentTransition = *transition;
        context->hasAppliedTransition = false;
    }
    else
    {
        if (transition->transitionEpoch != UINT64_C(1))
        {
            return DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR;
        }
        context->currentTransition = *transition;
        context->hasTransitionIdentity = true;
    }

    context->transitionInProgress = true;
    context->newDataAdmissionAllowed = false;
    context->state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_TRANSITION_IN_PROGRESS;

    managerStatus = CisEnvironmentManager_Invalidate(
        context->environmentManager);
    if (managerStatus != CIS_ENVIRONMENT_MANAGER_OK)
    {
        context->transitionInProgress = false;
        context->state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_FAULT_BLOCKED;
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED;
    }

    context->domainBarrierRevision++;
    receipt.transitionEpoch = transition->transitionEpoch;
    receipt.admittedCisBootId = transition->admittedCisBootId;
    receipt.domainBarrierRevision = context->domainBarrierRevision;
    receipt.environmentInvalidated = true;

    context->currentReceipt = receipt;
    context->hasAppliedTransition = true;
    context->transitionInProgress = false;
    context->newDataAdmissionAllowed = true;
    context->state = DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_WAITING_NEW_DATA;
    *outReceipt = receipt;
    return DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED;
}

DomainCisEnvironmentPriv_Status_t DomainCisEnvironmentPriv_GetSnapshot(
    const DomainCisEnvironmentPriv_t *context,
    DomainCisEnvironmentPriv_Snapshot_t *outSnapshot)
{
    DomainCisEnvironmentPriv_Snapshot_t snapshot;

    if ((context == NULL) || (outSnapshot == NULL))
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED;
    }

    snapshot.state = context->state;
    snapshot.currentTransition = context->currentTransition;
    snapshot.currentReceipt = context->currentReceipt;
    snapshot.domainBarrierRevision = context->domainBarrierRevision;
    snapshot.hasTransitionIdentity = context->hasTransitionIdentity;
    snapshot.hasAppliedTransition = context->hasAppliedTransition;
    snapshot.transitionInProgress = context->transitionInProgress;
    snapshot.newDataAdmissionAllowed = context->newDataAdmissionAllowed;
    *outSnapshot = snapshot;
    return DOMAIN_CIS_ENVIRONMENT_PRIV_OK;
}
