#ifndef DOMAIN_CIS_ENVIRONMENT_PRIVATE_H
#define DOMAIN_CIS_ENVIRONMENT_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

#include "cis_environment_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-private BOOT barrier. This is not a public DomainIf_* API.
 * Network owns and validates both identifiers. During one synchronized
 * Domain/Network runtime, transitionEpoch starts at 1 and increases by one
 * after each completed transition. Domain validates that order but never
 * creates an epoch or treats it as proof of a CIS BOOT.
 *
 * All calls, environment Rx stores, and environment Query reads must be
 * serialized by one external Domain owner. This module provides no ISR,
 * locking, queue, or general multi-thread safety.
 */

typedef uint64_t DomainCisEnvironmentPriv_TransitionEpoch_t;

typedef struct
{
    uint32_t admittedCisBootId;
    DomainCisEnvironmentPriv_TransitionEpoch_t transitionEpoch;
} DomainCisEnvironmentPriv_Transition_t;

typedef enum
{
    DOMAIN_CIS_ENVIRONMENT_PRIV_OK = 0,
    DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED,
    DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED,
    DOMAIN_CIS_ENVIRONMENT_PRIV_ID_CONFLICT,
    /* Retained for private ABI compatibility; the sequential contract does
     * not return this legacy status. */
    DOMAIN_CIS_ENVIRONMENT_PRIV_EPOCH_ORDER_UNAVAILABLE,
    DOMAIN_CIS_ENVIRONMENT_PRIV_BUSY,
    DOMAIN_CIS_ENVIRONMENT_PRIV_INVALID_ARGUMENT,
    DOMAIN_CIS_ENVIRONMENT_PRIV_NOT_INITIALIZED,
    DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED,
    /* Added after the original private ABI; existing numeric values remain. */
    DOMAIN_CIS_ENVIRONMENT_PRIV_STALE,
    DOMAIN_CIS_ENVIRONMENT_PRIV_ORDER_ERROR
} DomainCisEnvironmentPriv_Status_t;

typedef enum
{
    DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_READY = 0,
    DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_TRANSITION_IN_PROGRESS,
    DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_WAITING_NEW_DATA,
    DOMAIN_CIS_ENVIRONMENT_PRIV_STATE_FAULT_BLOCKED
} DomainCisEnvironmentPriv_State_t;

typedef struct
{
    DomainCisEnvironmentPriv_TransitionEpoch_t transitionEpoch;
    uint32_t admittedCisBootId;
    uint32_t domainBarrierRevision;
    bool environmentInvalidated;
} DomainCisEnvironmentPriv_TransitionReceipt_t;

typedef struct
{
    CisEnvironmentManager_t *environmentManager;
    DomainCisEnvironmentPriv_Transition_t currentTransition;
    DomainCisEnvironmentPriv_TransitionReceipt_t currentReceipt;
    uint32_t domainBarrierRevision;
    DomainCisEnvironmentPriv_State_t state;
    bool initialized;
    bool hasTransitionIdentity;
    bool hasAppliedTransition;
    bool transitionInProgress;
    /* Domain-side readiness only; it never reopens the Network gate. */
    bool newDataAdmissionAllowed;
} DomainCisEnvironmentPriv_t;

typedef struct
{
    DomainCisEnvironmentPriv_State_t state;
    DomainCisEnvironmentPriv_Transition_t currentTransition;
    DomainCisEnvironmentPriv_TransitionReceipt_t currentReceipt;
    uint32_t domainBarrierRevision;
    bool hasTransitionIdentity;
    bool hasAppliedTransition;
    bool transitionInProgress;
    bool newDataAdmissionAllowed;
} DomainCisEnvironmentPriv_Snapshot_t;

/*
 * First initialization and explicit reinitialization require Network to have
 * stopped delivery and drained all queued/in-flight callbacks. Reinitializing
 * resets the volatile epoch history, so the next accepted epoch is 1.
 */
DomainCisEnvironmentPriv_Status_t DomainCisEnvironmentPriv_Init(
    DomainCisEnvironmentPriv_t *context,
    CisEnvironmentManager_t *environmentManager);

/*
 * Completes manager invalidation before returning APPLIED. The first accepted
 * epoch is 1; each later accepted epoch is exactly the completed epoch plus
 * one. Exact retries are idempotent only after successful completion. Past
 * epochs return STALE, skipped future epochs return ORDER_ERROR, and
 * UINT64_MAX never wraps.
 *
 * Before the first call and before an explicit reinitialization, Network must
 * already have synchronized initial state, closed delivery, and drained old
 * callbacks. The Core cannot verify those external conditions.
 *
 * Failure leaves outReceipt unchanged. Invalidation failure latches
 * FAULT_BLOCKED and never marks new-data admission as allowed. It cannot be
 * retried into success implicitly; recovery requires an explicitly agreed
 * reinitialization after Network has again closed and drained delivery.
 *
 * Calls are synchronous and non-blocking except for the bounded manager call.
 * Invoke only from the single serialized non-ISR Domain owner. This function
 * neither waits for a Network queue nor reopens the Network delivery gate.
 */
DomainCisEnvironmentPriv_Status_t
DomainCisEnvironmentPriv_ApplyBootTransition(
    DomainCisEnvironmentPriv_t *context,
    const DomainCisEnvironmentPriv_Transition_t *transition,
    DomainCisEnvironmentPriv_TransitionReceipt_t *outReceipt);

/* Read-only value copy. */
DomainCisEnvironmentPriv_Status_t DomainCisEnvironmentPriv_GetSnapshot(
    const DomainCisEnvironmentPriv_t *context,
    DomainCisEnvironmentPriv_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CIS_ENVIRONMENT_PRIVATE_H */
