#ifndef PERMISSION_UNKNOWN_EVALUATOR_H
#define PERMISSION_UNKNOWN_EVALUATOR_H

#include <stdint.h>

#include "permission_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fail-closed evaluator for a system that has no approved vehicle-power
 * decision source. Each successful call is a new observation of
 * UNKNOWN/NO_DATA; scheduling and transmission remain Coordinator-owned.
 *
 * This evaluator is not selected by the zero-initialized Runtime config.
 * Production activation remains an integration decision.
 */
PermissionCoordinator_EvaluationResult_t PermissionUnknownEvaluator_Evaluate(
    void *context,
    uint32_t nowMs,
    PermissionManager_Evaluation_t *outEvaluation);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSION_UNKNOWN_EVALUATOR_H */
