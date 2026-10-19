#ifndef DOMAIN_CIS_OCCUPANT_PRIVATE_H
#define DOMAIN_CIS_OCCUPANT_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

#include "cis_occupant_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-private Occupant BOOT barrier. This is not part of the 72
 * public DomainIf_* APIs. Network owns transport BOOT validation, callback
 * drain, admission gating, and the monotonic transition epoch. The admitted
 * source generation is the Pi producer generation carried by
 * CisOccupantState_t; it is not the CIS CAN BOOT_ID.
 *
 * Calls, Occupant Rx, and Query must be serialized by one non-ISR owner. This
 * Pure C Core does not implement a queue, lock, ISR, retry, or recovery.
 */

typedef uint64_t DomainCisOccupantPriv_TransitionEpoch_t;

typedef struct
{
    SourceGeneration_t admittedSourceGeneration;
    DomainCisOccupantPriv_TransitionEpoch_t transitionEpoch;
} DomainCisOccupantPriv_Transition_t;

typedef enum
{
    DOMAIN_CIS_OCCUPANT_PRIV_OK = 0,
    DOMAIN_CIS_OCCUPANT_PRIV_APPLIED,
    DOMAIN_CIS_OCCUPANT_PRIV_DUPLICATE_APPLIED,
    DOMAIN_CIS_OCCUPANT_PRIV_ID_CONFLICT,
    DOMAIN_CIS_OCCUPANT_PRIV_BUSY,
    DOMAIN_CIS_OCCUPANT_PRIV_INVALID_ARGUMENT,
    DOMAIN_CIS_OCCUPANT_PRIV_NOT_INITIALIZED,
    DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED,
    DOMAIN_CIS_OCCUPANT_PRIV_STALE,
    DOMAIN_CIS_OCCUPANT_PRIV_ORDER_ERROR
} DomainCisOccupantPriv_Status_t;

typedef enum
{
    DOMAIN_CIS_OCCUPANT_PRIV_STATE_READY = 0,
    DOMAIN_CIS_OCCUPANT_PRIV_STATE_TRANSITION_IN_PROGRESS,
    DOMAIN_CIS_OCCUPANT_PRIV_STATE_WAITING_NEW_DATA,
    DOMAIN_CIS_OCCUPANT_PRIV_STATE_FAULT_BLOCKED
} DomainCisOccupantPriv_State_t;

typedef struct
{
    DomainCisOccupantPriv_TransitionEpoch_t transitionEpoch;
    SourceGeneration_t admittedSourceGeneration;
    uint32_t domainBarrierRevision;
    bool occupantInvalidated;
} DomainCisOccupantPriv_TransitionReceipt_t;

typedef struct
{
    CisOccupantManager_t *occupantManager;
    DomainCisOccupantPriv_Transition_t currentTransition;
    DomainCisOccupantPriv_TransitionReceipt_t currentReceipt;
    uint32_t domainBarrierRevision;
    DomainCisOccupantPriv_State_t state;
    bool initialized;
    bool hasTransitionIdentity;
    bool hasAppliedTransition;
    bool transitionInProgress;
    bool newDataAdmissionAllowed;
} DomainCisOccupantPriv_t;

typedef struct
{
    DomainCisOccupantPriv_State_t state;
    DomainCisOccupantPriv_Transition_t currentTransition;
    DomainCisOccupantPriv_TransitionReceipt_t currentReceipt;
    uint32_t domainBarrierRevision;
    bool hasTransitionIdentity;
    bool hasAppliedTransition;
    bool transitionInProgress;
    bool newDataAdmissionAllowed;
} DomainCisOccupantPriv_Snapshot_t;

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_Init(
    DomainCisOccupantPriv_t *context,
    CisOccupantManager_t *occupantManager);

/*
 * The first accepted epoch is 1 and each completed transition is followed by
 * exactly N+1. APPLIED is returned only after synchronous Manager
 * invalidation. Exact retries are idempotent only after completion. Failure
 * latches FAULT_BLOCKED and never opens admission.
 *
 * Before calling, Network must close ingress, discard queued prior-BOOT
 * records, and drain in-flight callbacks. Domain cannot verify those facts.
 */
DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_ApplyBootTransition(
    DomainCisOccupantPriv_t *context,
    const DomainCisOccupantPriv_Transition_t *transition,
    DomainCisOccupantPriv_TransitionReceipt_t *outReceipt);

DomainCisOccupantPriv_Status_t DomainCisOccupantPriv_GetSnapshot(
    const DomainCisOccupantPriv_t *context,
    DomainCisOccupantPriv_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_CIS_OCCUPANT_PRIVATE_H */
