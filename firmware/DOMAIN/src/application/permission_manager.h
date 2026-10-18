#ifndef PERMISSION_MANAGER_H
#define PERMISSION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Vehicle_Types_v0.2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Domain-owned permission evaluation cache.
 *
 * The Core stores an already evaluated input. It does not infer permission
 * from vehicle state, call DomainIf_TxCisPowerPermission(), or create Network
 * BOOT_ID/TX sequence values. All calls are serialized by one owner context,
 * or protected by the caller around the complete API call.
 */

typedef enum
{
    PERMISSION_MANAGER_PERMISSION_DENY = 0,
    PERMISSION_MANAGER_PERMISSION_ALLOW = 1,
    PERMISSION_MANAGER_PERMISSION_UNKNOWN = 255
} PermissionManager_Permission_t;

typedef enum
{
    PERMISSION_MANAGER_ATTEMPT_NONE = 0,
    PERMISSION_MANAGER_ATTEMPT_SUCCEEDED,
    PERMISSION_MANAGER_ATTEMPT_FAILED,
    PERMISSION_MANAGER_ATTEMPT_NO_DATA
} PermissionManager_AttemptOutcome_t;

typedef enum
{
    PERMISSION_MANAGER_OK = 0,
    PERMISSION_MANAGER_INVALID_ARGUMENT,
    PERMISSION_MANAGER_NOT_INITIALIZED
} PermissionManager_Status_t;

typedef struct
{
    /* Target cadence for requesting a new evaluation, not a validity TTL. */
    uint32_t reevaluationPeriodMs;
    /* Maximum permitted effective source age; expiry occurs at >= this value. */
    uint32_t sourceValidityMs;
} PermissionManager_Policy_t;

typedef struct
{
    PermissionManager_Permission_t permission;
    DataQuality_t sourceQuality;
    /* Source age at the instant this evaluation completed. */
    uint32_t sourceAgeMs;
} PermissionManager_Evaluation_t;

typedef struct
{
    bool hasEvaluation;
    PermissionManager_Permission_t permission;
    DataQuality_t sourceQuality;
    uint32_t sourceAgeMs;
    uint32_t evaluatedAtMs;
    uint32_t updateSequence;
} PermissionManager_StoredSnapshot_t;

typedef struct
{
    bool hasEvaluation;
    PermissionManager_Permission_t evaluatedPermission;
    PermissionManager_Permission_t effectivePermission;
    DataQuality_t sourceQuality;
    DataQuality_t effectiveQuality;
    uint32_t sourceAgeAtEvaluationMs;
    uint32_t localElapsedMs;
    uint32_t effectiveAgeMs;
    uint32_t evaluatedAtMs;
    uint32_t updateSequence;
    /*
     * Local change token for Adapter notification/polling. It changes for
     * every recorded evaluation outcome, while updateSequence changes only
     * for successful evaluations. It is not a C2 or Network sequence.
     */
    uint32_t stateRevision;
    bool invalidatedByUnsuccessfulAttempt;
    bool expired;
    bool activeAllowed;
    PermissionManager_AttemptOutcome_t lastAttemptOutcome;
    uint32_t lastAttemptAtMs;
} PermissionManager_Snapshot_t;

typedef struct
{
    PermissionManager_Policy_t policy;
    PermissionManager_StoredSnapshot_t stored;
    PermissionManager_AttemptOutcome_t lastAttemptOutcome;
    uint32_t lastAttemptAtMs;
    uint32_t stateRevision;
    bool initialized;
} PermissionManager_t;

PermissionManager_Status_t PermissionManager_Init(
    PermissionManager_t *manager,
    const PermissionManager_Policy_t *policy);

PermissionManager_Status_t PermissionManager_IsEvaluationDue(
    const PermissionManager_t *manager,
    uint32_t nowMs,
    bool *outDue);

/* A successful external evaluation atomically replaces the stored snapshot. */
PermissionManager_Status_t PermissionManager_RecordEvaluation(
    PermissionManager_t *manager,
    const PermissionManager_Evaluation_t *evaluation,
    uint32_t evaluatedAtMs);

/*
 * Records FAILED or NO_DATA without changing the last successful evaluation
 * or its sequence. The retained record remains available for diagnostics,
 * but its effective permission becomes UNKNOWN immediately. stateRevision is
 * advanced so a future Adapter can observe the withdrawal independently of
 * the successful-evaluation updateSequence.
 */
PermissionManager_Status_t PermissionManager_RecordUnsuccessfulAttempt(
    PermissionManager_t *manager,
    PermissionManager_AttemptOutcome_t outcome,
    uint32_t attemptedAtMs);

/* Read-only value copy. Calling this never refreshes age or sequence. */
PermissionManager_Status_t PermissionManager_GetSnapshot(
    const PermissionManager_t *manager,
    uint32_t nowMs,
    PermissionManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSION_MANAGER_H */
