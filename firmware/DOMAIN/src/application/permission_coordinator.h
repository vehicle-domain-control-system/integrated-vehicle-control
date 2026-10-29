#ifndef PERMISSION_COORDINATOR_H
#define PERMISSION_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "permission_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PERMISSION_COORDINATOR_EVALUATION_PERIOD_MS 200U

/*
 * Production scheduling/submit boundary for CIS power permission.
 * The injected evaluator owns the Domain policy. A NULL evaluator means
 * trusted input is unavailable and is recorded/submitted as NO_DATA.
 * Calls are serialized by the same non-ISR owner as DomainRuntime_Poll().
 */
typedef enum
{
    PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED = 0,
    PERMISSION_COORDINATOR_EVALUATION_FAILED,
    PERMISSION_COORDINATOR_EVALUATION_NO_DATA
} PermissionCoordinator_EvaluationResult_t;

typedef PermissionCoordinator_EvaluationResult_t
(*PermissionCoordinator_Evaluator_t)(
    void *context,
    uint32_t nowMs,
    PermissionManager_Evaluation_t *outEvaluation);

typedef enum
{
    PERMISSION_COORDINATOR_OK = 0,
    PERMISSION_COORDINATOR_INVALID_ARGUMENT,
    PERMISSION_COORDINATOR_NOT_INITIALIZED,
    PERMISSION_COORDINATOR_MANAGER_ERROR,
    PERMISSION_COORDINATOR_EVALUATOR_ERROR,
    PERMISSION_COORDINATOR_ADAPTER_ERROR,
    PERMISSION_COORDINATOR_TX_FAILED
} PermissionCoordinator_Status_t;

typedef struct
{
    bool initialized;
    bool lastPollWasDue;
    PermissionCoordinator_EvaluationResult_t lastEvaluationResult;
    DomainIf_Status_t lastTxStatus;
    uint32_t evaluationCount;
    uint32_t submissionAttemptCount;
    uint32_t submissionSuccessCount;
} PermissionCoordinator_Snapshot_t;

typedef struct
{
    PermissionManager_t *manager;
    PermissionCoordinator_Evaluator_t evaluator;
    void *evaluatorContext;
    PermissionCoordinator_Snapshot_t snapshot;
} PermissionCoordinator_t;

PermissionCoordinator_Status_t PermissionCoordinator_Init(
    PermissionCoordinator_t *coordinator,
    PermissionManager_t *manager,
    PermissionCoordinator_Evaluator_t evaluator,
    void *evaluatorContext);

/* Performs at most one evaluation and one local TxPort submission. */
PermissionCoordinator_Status_t PermissionCoordinator_Poll(
    PermissionCoordinator_t *coordinator,
    uint32_t nowMs);

/* Read-only value copy. Failure leaves outSnapshot unchanged. */
PermissionCoordinator_Status_t PermissionCoordinator_GetSnapshot(
    const PermissionCoordinator_t *coordinator,
    PermissionCoordinator_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSION_COORDINATOR_H */
