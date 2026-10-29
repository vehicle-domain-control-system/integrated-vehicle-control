#include "permission_manager.h"

#include <limits.h>
#include <string.h>

static bool PermissionManager_IsPermissionValid(
    PermissionManager_Permission_t permission)
{
    switch (permission)
    {
        case PERMISSION_MANAGER_PERMISSION_DENY:
        case PERMISSION_MANAGER_PERMISSION_ALLOW:
        case PERMISSION_MANAGER_PERMISSION_UNKNOWN:
            return true;
        default:
            return false;
    }
}

static bool PermissionManager_IsQualityValid(DataQuality_t quality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
        case DATA_QUALITY_STALE:
        case DATA_QUALITY_INVALID:
        case DATA_QUALITY_NO_DATA:
            return true;
        default:
            return false;
    }
}

static uint32_t PermissionManager_SaturatingAdd(
    uint32_t left,
    uint32_t right)
{
    if (right > (UINT32_MAX - left))
    {
        return UINT32_MAX;
    }
    return left + right;
}

PermissionManager_Status_t PermissionManager_Init(
    PermissionManager_t *manager,
    const PermissionManager_Policy_t *policy)
{
    if ((manager == NULL) || (policy == NULL))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }

    (void)memset(manager, 0, sizeof(*manager));
    manager->policy = *policy;
    manager->stored.permission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    manager->stored.sourceQuality = DATA_QUALITY_NO_DATA;
    manager->lastAttemptOutcome = PERMISSION_MANAGER_ATTEMPT_NONE;
    manager->initialized = true;
    return PERMISSION_MANAGER_OK;
}

PermissionManager_Status_t PermissionManager_IsEvaluationDue(
    const PermissionManager_t *manager,
    uint32_t nowMs,
    bool *outDue)
{
    if ((manager == NULL) || (outDue == NULL))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return PERMISSION_MANAGER_NOT_INITIALIZED;
    }

    if (manager->lastAttemptOutcome == PERMISSION_MANAGER_ATTEMPT_NONE)
    {
        *outDue = true;
    }
    else
    {
        *outDue = ((uint32_t)(nowMs - manager->lastAttemptAtMs) >=
                   manager->policy.reevaluationPeriodMs);
    }
    return PERMISSION_MANAGER_OK;
}

PermissionManager_Status_t PermissionManager_RecordEvaluation(
    PermissionManager_t *manager,
    const PermissionManager_Evaluation_t *evaluation,
    uint32_t evaluatedAtMs)
{
    PermissionManager_StoredSnapshot_t next;

    if ((manager == NULL) || (evaluation == NULL))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return PERMISSION_MANAGER_NOT_INITIALIZED;
    }
    if (!PermissionManager_IsPermissionValid(evaluation->permission) ||
        !PermissionManager_IsQualityValid(evaluation->sourceQuality))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }

    next = manager->stored;
    next.hasEvaluation = true;
    next.permission = evaluation->permission;
    next.sourceQuality = evaluation->sourceQuality;
    next.sourceAgeMs = evaluation->sourceAgeMs;
    next.evaluatedAtMs = evaluatedAtMs;
    next.updateSequence = manager->stored.updateSequence + 1U;

    manager->stored = next;
    manager->lastAttemptOutcome = PERMISSION_MANAGER_ATTEMPT_SUCCEEDED;
    manager->lastAttemptAtMs = evaluatedAtMs;
    manager->stateRevision += 1U;
    return PERMISSION_MANAGER_OK;
}

PermissionManager_Status_t PermissionManager_RecordUnsuccessfulAttempt(
    PermissionManager_t *manager,
    PermissionManager_AttemptOutcome_t outcome,
    uint32_t attemptedAtMs)
{
    if (manager == NULL)
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return PERMISSION_MANAGER_NOT_INITIALIZED;
    }
    if ((outcome != PERMISSION_MANAGER_ATTEMPT_FAILED) &&
        (outcome != PERMISSION_MANAGER_ATTEMPT_NO_DATA))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }

    manager->lastAttemptOutcome = outcome;
    manager->lastAttemptAtMs = attemptedAtMs;
    manager->stateRevision += 1U;
    return PERMISSION_MANAGER_OK;
}

PermissionManager_Status_t PermissionManager_GetSnapshot(
    const PermissionManager_t *manager,
    uint32_t nowMs,
    PermissionManager_Snapshot_t *outSnapshot)
{
    PermissionManager_Snapshot_t snapshot;

    if ((manager == NULL) || (outSnapshot == NULL))
    {
        return PERMISSION_MANAGER_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return PERMISSION_MANAGER_NOT_INITIALIZED;
    }

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.hasEvaluation = manager->stored.hasEvaluation;
    snapshot.evaluatedPermission = manager->stored.permission;
    snapshot.effectivePermission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    snapshot.sourceQuality = manager->stored.sourceQuality;
    snapshot.effectiveQuality = manager->stored.sourceQuality;
    snapshot.sourceAgeAtEvaluationMs = manager->stored.sourceAgeMs;
    snapshot.evaluatedAtMs = manager->stored.evaluatedAtMs;
    snapshot.updateSequence = manager->stored.updateSequence;
    snapshot.lastAttemptOutcome = manager->lastAttemptOutcome;
    snapshot.lastAttemptAtMs = manager->lastAttemptAtMs;
    snapshot.stateRevision = manager->stateRevision;
    snapshot.invalidatedByUnsuccessfulAttempt =
        ((manager->lastAttemptOutcome == PERMISSION_MANAGER_ATTEMPT_FAILED) ||
         (manager->lastAttemptOutcome == PERMISSION_MANAGER_ATTEMPT_NO_DATA));

    if (!manager->stored.hasEvaluation)
    {
        snapshot.evaluatedPermission =
            PERMISSION_MANAGER_PERMISSION_UNKNOWN;
        snapshot.sourceQuality = DATA_QUALITY_NO_DATA;
        snapshot.effectiveQuality = DATA_QUALITY_NO_DATA;
        if (manager->lastAttemptOutcome == PERMISSION_MANAGER_ATTEMPT_FAILED)
        {
            snapshot.effectiveQuality = DATA_QUALITY_INVALID;
        }
    }
    else
    {
        snapshot.localElapsedMs =
            (uint32_t)(nowMs - manager->stored.evaluatedAtMs);
        snapshot.effectiveAgeMs = PermissionManager_SaturatingAdd(
            manager->stored.sourceAgeMs, snapshot.localElapsedMs);
        snapshot.expired =
            (snapshot.effectiveAgeMs >= manager->policy.sourceValidityMs);

        /*
         * The latest explicit evaluation failure is the withdrawal reason,
         * even when the retained diagnostic record has also expired. TTL
         * expiry must not hide FAILED/NO_DATA from the Network submission.
         */
        if (manager->lastAttemptOutcome ==
            PERMISSION_MANAGER_ATTEMPT_FAILED)
        {
            snapshot.effectiveQuality = DATA_QUALITY_INVALID;
        }
        else if (manager->lastAttemptOutcome ==
                 PERMISSION_MANAGER_ATTEMPT_NO_DATA)
        {
            snapshot.effectiveQuality = DATA_QUALITY_NO_DATA;
        }
        else if (snapshot.expired &&
                 (snapshot.effectiveQuality == DATA_QUALITY_OK))
        {
            snapshot.effectiveQuality = DATA_QUALITY_STALE;
        }
        else if (!snapshot.expired &&
                 (snapshot.effectiveQuality == DATA_QUALITY_OK))
        {
            snapshot.effectivePermission = manager->stored.permission;
        }
    }

    snapshot.activeAllowed =
        (snapshot.effectivePermission ==
         PERMISSION_MANAGER_PERMISSION_ALLOW);
    *outSnapshot = snapshot;
    return PERMISSION_MANAGER_OK;
}
