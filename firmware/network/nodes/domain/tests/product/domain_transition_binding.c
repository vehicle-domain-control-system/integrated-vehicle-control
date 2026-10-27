/* TEST ONLY: see domain_transition_binding.h. */
#include "domain_transition_binding.h"
#include <string.h>

/* One to one. OK is not a transition result; an unexpected value is reported
 * as a completion that is not known (the adapter latches it). */
NetDomainC2TransitionStatus domain_transition_environment_status(DomainCisEnvironmentPriv_Status_t status)
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

NetDomainC2TransitionStatus domain_transition_occupant_status(DomainCisOccupantPriv_Status_t status)
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

/* The receipt is copied only from a completion that reports the invalidation;
 * otherwise it stays zero (revision 0), which no confirmed transition has. */
static NetDomainC2TransitionStatus apply_environment(void *context, uint32_t id, uint64_t epoch,
    NetDomainC2TransitionReceipt *out)
{
    DomainCisEnvironmentPriv_t *priv = DomainRuntime_GetCisEnvironmentPrivate((DomainRuntime_t *)context);
    DomainCisEnvironmentPriv_Transition_t t;
    DomainCisEnvironmentPriv_TransitionReceipt_t r;
    DomainCisEnvironmentPriv_Status_t st;
    if (priv == NULL || out == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    memset(&r, 0, sizeof(r));
    t.admittedCisBootId = id;
    t.transitionEpoch = epoch;
    st = DomainCisEnvironmentPriv_ApplyBootTransition(priv, &t, &r);
    memset(out, 0, sizeof(*out));
    if ((st == DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED || st == DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED) &&
        r.environmentInvalidated) {
        out->id = r.admittedCisBootId;
        out->epoch = r.transitionEpoch;
        out->revision = r.domainBarrierRevision;
    }
    return domain_transition_environment_status(st);
}

static NetDomainC2TransitionStatus apply_occupant(void *context, uint32_t id, uint64_t epoch,
    NetDomainC2TransitionReceipt *out)
{
    DomainCisOccupantPriv_t *priv = DomainRuntime_GetCisOccupantPrivate((DomainRuntime_t *)context);
    DomainCisOccupantPriv_Transition_t t;
    DomainCisOccupantPriv_TransitionReceipt_t r;
    DomainCisOccupantPriv_Status_t st;
    if (priv == NULL || out == NULL) return NET_DOMAIN_C2_TRANSITION_UNAVAILABLE;
    memset(&r, 0, sizeof(r));
    t.admittedSourceGeneration = id;
    t.transitionEpoch = epoch;
    st = DomainCisOccupantPriv_ApplyBootTransition(priv, &t, &r);
    memset(out, 0, sizeof(*out));
    if ((st == DOMAIN_CIS_OCCUPANT_PRIV_APPLIED || st == DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED) &&
        r.occupantInvalidated) {
        out->id = r.admittedSourceGeneration;
        out->epoch = r.transitionEpoch;
        out->revision = r.domainBarrierRevision;
    }
    return domain_transition_occupant_status(st);
}

NetDomainC2TransitionBinding domain_transition_binding(DomainRuntime_t *runtime)
{
    NetDomainC2TransitionBinding b;
    b.context = runtime;
    b.environment = apply_environment;
    b.occupant = apply_occupant;
    return b;
}
