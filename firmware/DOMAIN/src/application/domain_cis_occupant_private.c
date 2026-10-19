#include "domain_cis_occupant_private.h"

#include <string.h>

static bool DomainCisOccupantPriv_IsSameTransition(
    const DomainCisOccupantPriv_Transition_t *left,
    const DomainCisOccupantPriv_Transition_t *right)
{
    return (left->admittedSourceGeneration ==
            right->admittedSourceGeneration) &&
           (left->transitionEpoch == right->transitionEpoch);
}

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_Init(
    DomainCisOccupantPriv_t *context,
    CisOccupantManager_t *occupantManager)
{
    if ((context == NULL) || (occupantManager == NULL))
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT;
    }

    (void)memset(context, 0, sizeof(*context));
    context->occupantManager = occupantManager;
    context->state = DOMAIN_CIS_OCCUPANT_PRIV_STATE_READY;
    context->initialized = true;
    return DOMAIN_CIS_OCCUPANT_PRIV_OK;
}

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_ApplyBootTransition(
    DomainCisOccupantPriv_t *context,
    const DomainCisOccupantPriv_Transition_t *transition,
    DomainCisOccupantPriv_TransitionReceipt_t *outReceipt)
{
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    CisOccupantManager_Status_t managerStatus;
    DomainCisOccupantPriv_TransitionEpoch_t expectedEpoch;

    if ((context == NULL) || (transition == NULL) || (outReceipt == NULL))
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_NOT_INITIALIZED;
    }
    if ((transition->admittedSourceGeneration == 0U) ||
        (transition->transitionEpoch == UINT64_C(0)))
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT;
    }
    if (context->transitionInProgress)
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_BUSY;
    }

    if (context->hasTransitionIdentity)
    {
        if (transition->transitionEpoch ==
            context->currentTransition.transitionEpoch)
        {
            if (!DomainCisOccupantPriv_IsSameTransition(
                    transition, &context->currentTransition))
            {
                return DOMAIN_CIS_OCCUPANT_PRIV_ID_CONFLICT;
            }
            if (context->hasAppliedTransition)
            {
                *outReceipt = context->currentReceipt;
                return DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED;
            }
            return DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED;
        }

        if (!context->hasAppliedTransition)
        {
            return DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED;
        }
        if (context->currentTransition.transitionEpoch == UINT64_MAX)
        {
            return DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR;
        }

        expectedEpoch = context->currentTransition.transitionEpoch +
                        UINT64_C(1);
        if (transition->transitionEpoch <
            context->currentTransition.transitionEpoch)
        {
            return DOMAIN_CIS_OCCUPANT_PRIV_STALE;
        }
        if (transition->transitionEpoch != expectedEpoch)
        {
            return DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR;
        }

        context->currentTransition = *transition;
        context->hasAppliedTransition = false;
    }
    else
    {
        if (transition->transitionEpoch != UINT64_C(1))
        {
            return DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR;
        }
        context->currentTransition = *transition;
        context->hasTransitionIdentity = true;
    }

    context->transitionInProgress = true;
    context->newDataAdmissionAllowed = false;
    context->state = DOMAIN_CIS_OCCUPANT_PRIV_STATE_TRANSITION_IN_PROGRESS;

    managerStatus = CisOccupantManager_Invalidate(
        context->occupantManager,
        transition->admittedSourceGeneration);
    if (managerStatus != CIS_OCCUPANT_MANAGER_OK)
    {
        context->transitionInProgress = false;
        context->state = DOMAIN_CIS_OCCUPANT_PRIV_STATE_FAULT_BLOCKED;
        return DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED;
    }

    context->domainBarrierRevision++;
    receipt.transitionEpoch = transition->transitionEpoch;
    receipt.admittedSourceGeneration = transition->admittedSourceGeneration;
    receipt.domainBarrierRevision = context->domainBarrierRevision;
    receipt.occupantInvalidated = true;

    context->currentReceipt = receipt;
    context->hasAppliedTransition = true;
    context->transitionInProgress = false;
    context->newDataAdmissionAllowed = true;
    context->state = DOMAIN_CIS_OCCUPANT_PRIV_STATE_WAITING_NEW_DATA;
    *outReceipt = receipt;
    return DOMAIN_CIS_OCCUPANT_PRIV_APPLIED;
}

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_GetSnapshot(
    const DomainCisOccupantPriv_t *context,
    DomainCisOccupantPriv_Snapshot_t *outSnapshot)
{
    DomainCisOccupantPriv_Snapshot_t snapshot;

    if ((context == NULL) || (outSnapshot == NULL))
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_CIS_OCCUPANT_PRIV_NOT_INITIALIZED;
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
    return DOMAIN_CIS_OCCUPANT_PRIV_OK;
}
