#include "permission_coordinator.h"

#include <string.h>

#include "permission_adapter.h"

static bool PermissionCoordinator_IsEvaluationSupported(
    const PermissionManager_Evaluation_t *evaluation)
{
    if (evaluation == NULL)
    {
        return false;
    }
    switch (evaluation->permission)
    {
        case PERMISSION_MANAGER_PERMISSION_ALLOW:
        case PERMISSION_MANAGER_PERMISSION_DENY:
            return evaluation->sourceQuality == DATA_QUALITY_OK;
        case PERMISSION_MANAGER_PERMISSION_UNKNOWN:
            return (evaluation->sourceQuality == DATA_QUALITY_STALE) ||
                   (evaluation->sourceQuality == DATA_QUALITY_INVALID) ||
                   (evaluation->sourceQuality == DATA_QUALITY_NO_DATA);
        default:
            return false;
    }
}

static PermissionCoordinator_Status_t PermissionCoordinator_RecordAttempt(
    PermissionCoordinator_t *coordinator,
    PermissionCoordinator_EvaluationResult_t result,
    const PermissionManager_Evaluation_t *evaluation,
    uint32_t nowMs)
{
    PermissionManager_Status_t status;

    switch (result)
    {
        case PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED:
            if (!PermissionCoordinator_IsEvaluationSupported(evaluation))
            {
                status = PermissionManager_RecordUnsuccessfulAttempt(
                    coordinator->manager,
                    PERMISSION_MANAGER_ATTEMPT_FAILED,
                    nowMs);
                coordinator->snapshot.lastEvaluationResult =
                    PERMISSION_COORDINATOR_EVALUATION_FAILED;
                return (status == PERMISSION_MANAGER_OK)
                           ? PERMISSION_COORDINATOR_EVALUATOR_ERROR
                           : PERMISSION_COORDINATOR_MANAGER_ERROR;
            }
            status = PermissionManager_RecordEvaluation(
                coordinator->manager, evaluation, nowMs);
            break;
        case PERMISSION_COORDINATOR_EVALUATION_FAILED:
            status = PermissionManager_RecordUnsuccessfulAttempt(
                coordinator->manager,
                PERMISSION_MANAGER_ATTEMPT_FAILED,
                nowMs);
            break;
        case PERMISSION_COORDINATOR_EVALUATION_NO_DATA:
            status = PermissionManager_RecordUnsuccessfulAttempt(
                coordinator->manager,
                PERMISSION_MANAGER_ATTEMPT_NO_DATA,
                nowMs);
            break;
        default:
            status = PermissionManager_RecordUnsuccessfulAttempt(
                coordinator->manager,
                PERMISSION_MANAGER_ATTEMPT_FAILED,
                nowMs);
            coordinator->snapshot.lastEvaluationResult =
                PERMISSION_COORDINATOR_EVALUATION_FAILED;
            return (status == PERMISSION_MANAGER_OK)
                       ? PERMISSION_COORDINATOR_EVALUATOR_ERROR
                       : PERMISSION_COORDINATOR_MANAGER_ERROR;
    }

    return (status == PERMISSION_MANAGER_OK)
               ? PERMISSION_COORDINATOR_OK
               : PERMISSION_COORDINATOR_MANAGER_ERROR;
}

static PermissionCoordinator_Status_t PermissionCoordinator_Submit(
    PermissionCoordinator_t *coordinator,
    uint32_t nowMs)
{
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t permission;
    DomainIf_Status_t status;

    if (PermissionManager_GetSnapshot(
            coordinator->manager, nowMs, &snapshot) != PERMISSION_MANAGER_OK)
    {
        return PERMISSION_COORDINATOR_MANAGER_ERROR;
    }
    if (PermissionAdapter_ConvertCisPowerPermission(
            &snapshot, &permission) != PERMISSION_ADAPTER_OK)
    {
        return PERMISSION_COORDINATOR_ADAPTER_ERROR;
    }

    coordinator->snapshot.submissionAttemptCount += 1U;
    status = DomainIf_TxCisPowerPermission(&permission);
    coordinator->snapshot.lastTxStatus = status;
    if (status != DOMAIN_IF_OK)
    {
        return PERMISSION_COORDINATOR_TX_FAILED;
    }
    coordinator->snapshot.submissionSuccessCount += 1U;
    return PERMISSION_COORDINATOR_OK;
}

PermissionCoordinator_Status_t PermissionCoordinator_Init(
    PermissionCoordinator_t *coordinator,
    PermissionManager_t *manager,
    PermissionCoordinator_Evaluator_t evaluator,
    void *evaluatorContext)
{
    PermissionManager_Snapshot_t snapshot;

    if ((coordinator == NULL) || (manager == NULL))
    {
        return PERMISSION_COORDINATOR_INVALID_ARGUMENT;
    }
    if (PermissionManager_GetSnapshot(manager, 0U, &snapshot) !=
        PERMISSION_MANAGER_OK)
    {
        return PERMISSION_COORDINATOR_MANAGER_ERROR;
    }
    if ((manager->policy.reevaluationPeriodMs !=
         PERMISSION_COORDINATOR_EVALUATION_PERIOD_MS) ||
        (manager->policy.sourceValidityMs == 0U))
    {
        return PERMISSION_COORDINATOR_INVALID_ARGUMENT;
    }

    (void)memset(coordinator, 0, sizeof(*coordinator));
    coordinator->manager = manager;
    coordinator->evaluator = evaluator;
    coordinator->evaluatorContext = evaluatorContext;
    coordinator->snapshot.initialized = true;
    coordinator->snapshot.lastEvaluationResult =
        PERMISSION_COORDINATOR_EVALUATION_NO_DATA;
    coordinator->snapshot.lastTxStatus = DOMAIN_IF_NOT_AVAILABLE;
    return PERMISSION_COORDINATOR_OK;
}

PermissionCoordinator_Status_t PermissionCoordinator_Poll(
    PermissionCoordinator_t *coordinator,
    uint32_t nowMs)
{
    PermissionManager_Evaluation_t evaluation;
    PermissionCoordinator_EvaluationResult_t result;
    PermissionCoordinator_Status_t recordStatus;
    PermissionCoordinator_Status_t submitStatus;
    bool due;

    if (coordinator == NULL)
    {
        return PERMISSION_COORDINATOR_INVALID_ARGUMENT;
    }
    if (!coordinator->snapshot.initialized ||
        (coordinator->manager == NULL))
    {
        return PERMISSION_COORDINATOR_NOT_INITIALIZED;
    }
    if (PermissionManager_IsEvaluationDue(
            coordinator->manager, nowMs, &due) != PERMISSION_MANAGER_OK)
    {
        return PERMISSION_COORDINATOR_MANAGER_ERROR;
    }

    coordinator->snapshot.lastPollWasDue = due;
    if (!due)
    {
        return PERMISSION_COORDINATOR_OK;
    }

    (void)memset(&evaluation, 0, sizeof(evaluation));
    evaluation.permission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    evaluation.sourceQuality = DATA_QUALITY_NO_DATA;
    result = (coordinator->evaluator != NULL)
                 ? coordinator->evaluator(
                       coordinator->evaluatorContext, nowMs, &evaluation)
                 : PERMISSION_COORDINATOR_EVALUATION_NO_DATA;

    coordinator->snapshot.evaluationCount += 1U;
    coordinator->snapshot.lastEvaluationResult = result;
    recordStatus = PermissionCoordinator_RecordAttempt(
        coordinator, result, &evaluation, nowMs);

    /*
     * Do not submit a retained permission when the Manager could not record
     * the current outcome. In particular, a failed withdrawal must never
     * leave an earlier ALLOW eligible for local Network admission.
     */
    if (recordStatus == PERMISSION_COORDINATOR_MANAGER_ERROR)
    {
        return recordStatus;
    }

    /* A failed or invalid evaluation still submits its fail-closed result. */
    submitStatus = PermissionCoordinator_Submit(coordinator, nowMs);
    if (submitStatus != PERMISSION_COORDINATOR_OK)
    {
        return submitStatus;
    }
    return recordStatus;
}

PermissionCoordinator_Status_t PermissionCoordinator_GetSnapshot(
    const PermissionCoordinator_t *coordinator,
    PermissionCoordinator_Snapshot_t *outSnapshot)
{
    if ((coordinator == NULL) || (outSnapshot == NULL))
    {
        return PERMISSION_COORDINATOR_INVALID_ARGUMENT;
    }
    if (!coordinator->snapshot.initialized)
    {
        return PERMISSION_COORDINATOR_NOT_INITIALIZED;
    }
    *outSnapshot = coordinator->snapshot;
    return PERMISSION_COORDINATOR_OK;
}
