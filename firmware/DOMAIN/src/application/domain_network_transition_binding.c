#include "domain_network_transition_binding.h"

#include <string.h>

NetDomainC2TransitionStatus domain_transition_environment_status(
    DomainCisEnvironmentPriv_Status_t status)
{
    switch (status) {
    case DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED: return NET_DOMAIN_C2_TRANSITION_APPLIED;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED: return NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_ID_CONFLICT: return NET_DOMAIN_C2_TRANSITION_ID_CONFLICT;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_EPOCH_ORDER_UNAVAILABLE: return NET_DOMAIN_C2_TRANSITION_EPOCH_ORDER_UNAVAILABLE;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_BUSY: return NET_DOMAIN_C2_TRANSITION_BUSY;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT: return NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED: return NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_STALE: return NET_DOMAIN_C2_TRANSITION_STALE;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR: return NET_DOMAIN_C2_TRANSITION_ORDER_ERROR;
    case DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED:
    case DOMAIN_CIS_ENVIRONMENT_PRIV_OK:
    default: return NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED;
    }
}

NetDomainC2TransitionStatus domain_transition_occupant_status(
    DomainCisOccupantPriv_Status_t status)
{
    switch (status) {
    case DOMAIN_CIS_OCCUPANT_PRIV_APPLIED: return NET_DOMAIN_C2_TRANSITION_APPLIED;
    case DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED: return NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED;
    case DOMAIN_CIS_OCCUPANT_PRIV_ID_CONFLICT: return NET_DOMAIN_C2_TRANSITION_ID_CONFLICT;
    case DOMAIN_CIS_OCCUPANT_PRIV_BUSY: return NET_DOMAIN_C2_TRANSITION_BUSY;
    case DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT: return NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT;
    case DOMAIN_CIS_OCCUPANT_PRIV_NOT_INITIALIZED: return NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED;
    case DOMAIN_CIS_OCCUPANT_PRIV_STALE: return NET_DOMAIN_C2_TRANSITION_STALE;
    case DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR: return NET_DOMAIN_C2_TRANSITION_ORDER_ERROR;
    case DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED:
    case DOMAIN_CIS_OCCUPANT_PRIV_OK:
    default: return NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED;
    }
}

static NetDomainC2TransitionStatus apply_environment(
    void *context,
    uint32_t id,
    uint64_t epoch,
    NetDomainC2TransitionReceipt *out)
{
    DomainCisEnvironmentPriv_t *priv;
    DomainCisEnvironmentPriv_Transition_t transition;
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_Status_t status;

    if (out == NULL) return NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT;
    (void)memset(out, 0, sizeof(*out));
    if (context == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    priv = DomainRuntime_GetCisEnvironmentPrivate((DomainRuntime_t *)context);
    if (priv == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    (void)memset(&receipt, 0, sizeof(receipt));
    transition.admittedCisBootId = id;
    transition.transitionEpoch = epoch;
    status = DomainCisEnvironmentPriv_ApplyBootTransition(
        priv, &transition, &receipt);
    if ((status == DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED ||
         status == DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED) &&
        receipt.environmentInvalidated &&
        receipt.admittedCisBootId == id &&
        receipt.transitionEpoch == epoch &&
        receipt.domainBarrierRevision != 0u) {
        out->id = receipt.admittedCisBootId;
        out->epoch = receipt.transitionEpoch;
        out->revision = receipt.domainBarrierRevision;
    } else if (status == DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED ||
               status == DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED) {
        return NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED;
    }
    return domain_transition_environment_status(status);
}

static NetDomainC2TransitionStatus apply_occupant(
    void *context,
    uint32_t id,
    uint64_t epoch,
    NetDomainC2TransitionReceipt *out)
{
    DomainCisOccupantPriv_t *priv;
    DomainCisOccupantPriv_Transition_t transition;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    DomainCisOccupantPriv_Status_t status;

    if (out == NULL) return NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT;
    (void)memset(out, 0, sizeof(*out));
    if (context == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    priv = DomainRuntime_GetCisOccupantPrivate((DomainRuntime_t *)context);
    if (priv == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    (void)memset(&receipt, 0, sizeof(receipt));
    transition.admittedSourceGeneration = id;
    transition.transitionEpoch = epoch;
    status = DomainCisOccupantPriv_ApplyBootTransition(
        priv, &transition, &receipt);
    if ((status == DOMAIN_CIS_OCCUPANT_PRIV_APPLIED ||
         status == DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED) &&
        receipt.occupantInvalidated &&
        receipt.admittedSourceGeneration == id &&
        receipt.transitionEpoch == epoch &&
        receipt.domainBarrierRevision != 0u) {
        out->id = receipt.admittedSourceGeneration;
        out->epoch = receipt.transitionEpoch;
        out->revision = receipt.domainBarrierRevision;
    } else if (status == DOMAIN_CIS_OCCUPANT_PRIV_APPLIED ||
               status == DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED) {
        return NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED;
    }
    return domain_transition_occupant_status(status);
}

NetDomainC2TransitionBinding domain_network_transition_binding(
    DomainRuntime_t *runtime)
{
    NetDomainC2TransitionBinding binding;
    binding.context = runtime;
    binding.environment = apply_environment;
    binding.occupant = apply_occupant;
    return binding;
}
