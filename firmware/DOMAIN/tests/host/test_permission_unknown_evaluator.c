#include "permission_unknown_evaluator.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

static uint32_t g_txCalls;
static DomainIf_CisPowerPermission_t g_lastTx;

static DomainIf_Status_t Transmit(
    const DomainIf_CisPowerPermission_t *permission)
{
    g_txCalls += 1U;
    g_lastTx = *permission;
    return DOMAIN_IF_OK;
}

static int Setup(
    PermissionManager_t *manager,
    PermissionCoordinator_t *coordinator)
{
    PermissionManager_Policy_t policy = {200U, 500U};
    DomainIf_TxPort_t port;

    CHECK(PermissionManager_Init(manager, &policy) == PERMISSION_MANAGER_OK);
    CHECK(PermissionCoordinator_Init(
              coordinator,
              manager,
              PermissionUnknownEvaluator_Evaluate,
              NULL) == PERMISSION_COORDINATOR_OK);
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    (void)memset(&port, 0, sizeof(port));
    port.txCisPowerPermission = Transmit;
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    g_txCalls = 0U;
    (void)memset(&g_lastTx, 0, sizeof(g_lastTx));
    return 0;
}

static int TestDirectEvaluation(void)
{
    PermissionManager_Evaluation_t evaluation;

    (void)memset(&evaluation, 0xA5, sizeof(evaluation));
    CHECK(PermissionUnknownEvaluator_Evaluate(NULL, 123U, &evaluation) ==
          PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED);
    CHECK(evaluation.permission == PERMISSION_MANAGER_PERMISSION_UNKNOWN);
    CHECK(evaluation.sourceQuality == DATA_QUALITY_NO_DATA);
    CHECK(evaluation.sourceAgeMs == 0U);
    CHECK(PermissionUnknownEvaluator_Evaluate(NULL, 123U, NULL) ==
          PERMISSION_COORDINATOR_EVALUATION_FAILED);
    return 0;
}

static int TestCoordinatorIssuesNewSuccessfulEvaluation(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionManager_Snapshot_t snapshot;
    PermissionCoordinator_Snapshot_t coordinatorSnapshot;

    CHECK(Setup(&manager, &coordinator) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 100U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 1U);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_lastTx.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_lastTx.meta.ageMs == 0U);
    CHECK(g_lastTx.meta.updateSequence == 1U);

    CHECK(PermissionCoordinator_Poll(&coordinator, 299U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 1U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 300U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 2U);
    CHECK(g_lastTx.meta.updateSequence == 2U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 500U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 3U);
    CHECK(g_lastTx.meta.updateSequence == 3U);

    CHECK(PermissionManager_GetSnapshot(&manager, 500U, &snapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(snapshot.evaluatedPermission ==
          PERMISSION_MANAGER_PERMISSION_UNKNOWN);
    CHECK(snapshot.effectivePermission ==
          PERMISSION_MANAGER_PERMISSION_UNKNOWN);
    CHECK(snapshot.sourceQuality == DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effectiveQuality == DATA_QUALITY_NO_DATA);
    CHECK(snapshot.updateSequence == 3U);
    CHECK(snapshot.stateRevision == 3U);
    CHECK(PermissionCoordinator_GetSnapshot(
              &coordinator, &coordinatorSnapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(coordinatorSnapshot.evaluationCount == 3U);
    CHECK(coordinatorSnapshot.submissionSuccessCount == 3U);
    return 0;
}

static int TestDelayedPollDoesNotCatchUp(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;

    CHECK(Setup(&manager, &coordinator) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(PermissionCoordinator_Poll(&coordinator, 2000U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 2U);
    CHECK(g_lastTx.meta.updateSequence == 2U);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestDirectEvaluation();
    failures += TestCoordinatorIssuesNewSuccessfulEvaluation();
    failures += TestDelayedPollDoesNotCatchUp();
    if (failures == 0)
    {
        printf("permission_unknown_evaluator: 3 tests passed\n");
    }
    return failures;
}
