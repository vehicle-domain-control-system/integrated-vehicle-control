#include "permission_unknown_evaluator.h"

#include <stddef.h>

PermissionCoordinator_EvaluationResult_t PermissionUnknownEvaluator_Evaluate(
    void *context,
    uint32_t nowMs,
    PermissionManager_Evaluation_t *outEvaluation)
{
    (void)context;
    (void)nowMs;

    if (outEvaluation == NULL)
    {
        return PERMISSION_COORDINATOR_EVALUATION_FAILED;
    }

    outEvaluation->permission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    outEvaluation->sourceQuality = DATA_QUALITY_NO_DATA;
    outEvaluation->sourceAgeMs = 0U;
    return PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED;
}
