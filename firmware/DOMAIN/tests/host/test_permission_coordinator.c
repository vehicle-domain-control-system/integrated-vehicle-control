#include "permission_coordinator.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

typedef struct
{
    PermissionCoordinator_EvaluationResult_t result;
    PermissionManager_Evaluation_t evaluation;
    uint32_t calls;
} FakeEvaluator_t;

static DomainIf_Status_t g_txResult;
static uint32_t g_txCalls;
static DomainIf_CisPowerPermission_t g_lastTx;

static PermissionCoordinator_EvaluationResult_t Evaluate(
    void *context,
    uint32_t nowMs,
    PermissionManager_Evaluation_t *outEvaluation)
{
    FakeEvaluator_t *fake = (FakeEvaluator_t *)context;
    (void)nowMs;
    fake->calls += 1U;
    *outEvaluation = fake->evaluation;
    return fake->result;
}

static DomainIf_Status_t Transmit(
    const DomainIf_CisPowerPermission_t *permission)
{
    g_txCalls += 1U;
    g_lastTx = *permission;
    return g_txResult;
}

static PermissionManager_Policy_t Policy(void)
{
    PermissionManager_Policy_t policy = {200U, 500U};
    return policy;
}

static FakeEvaluator_t AllowEvaluator(void)
{
    FakeEvaluator_t fake;
    (void)memset(&fake, 0, sizeof(fake));
    fake.result = PERMISSION_COORDINATOR_EVALUATION_SUCCEEDED;
    fake.evaluation.permission = PERMISSION_MANAGER_PERMISSION_ALLOW;
    fake.evaluation.sourceQuality = DATA_QUALITY_OK;
    return fake;
}

static int Setup(
    PermissionManager_t *manager,
    PermissionCoordinator_t *coordinator,
    FakeEvaluator_t *evaluator,
    bool registerPort)
{
    DomainIf_TxPort_t port;
    PermissionManager_Policy_t policy = Policy();

    CHECK(PermissionManager_Init(manager, &policy) == PERMISSION_MANAGER_OK);
    CHECK(PermissionCoordinator_Init(
              coordinator,
              manager,
              (evaluator != NULL) ? Evaluate : NULL,
              evaluator) == PERMISSION_COORDINATOR_OK);
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_OK);
    g_txResult = DOMAIN_IF_OK;
    g_txCalls = 0U;
    (void)memset(&g_lastTx, 0, sizeof(g_lastTx));
    if (registerPort)
    {
        (void)memset(&port, 0, sizeof(port));
        port.txCisPowerPermission = Transmit;
        CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    }
    return 0;
}

static int TestFirstPollAndBoundary(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionCoordinator_Snapshot_t snapshot;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 100U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 1U && g_txCalls == 1U);
    CHECK(g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.meta.updateSequence == 1U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 299U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 1U && g_txCalls == 1U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 300U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 2U && g_txCalls == 2U);
    CHECK(g_lastTx.meta.updateSequence == 2U);
    CHECK(PermissionCoordinator_GetSnapshot(&coordinator, &snapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(snapshot.evaluationCount == 2U);
    CHECK(snapshot.submissionSuccessCount == 2U);
    return 0;
}

static int TestDelayedPollDoesNotCatchUp(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(PermissionCoordinator_Poll(&coordinator, 5000U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 2U);
    CHECK(g_txCalls == 2U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 5001U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 2U);
    return 0;
}

static int TestRollover(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, UINT32_MAX - 50U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(PermissionCoordinator_Poll(&coordinator, 148U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 1U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 149U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(evaluator.calls == 2U);
    return 0;
}

static int TestMissingInputAndRepeatedFailure(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionCoordinator_Snapshot_t coordinatorSnapshot;

    CHECK(Setup(&manager, &coordinator, NULL, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 10U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_lastTx.meta.updateSequence == 0U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 210U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 2U);
    CHECK(g_lastTx.meta.updateSequence == 0U);
    CHECK(PermissionCoordinator_GetSnapshot(
              &coordinator, &coordinatorSnapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(coordinatorSnapshot.evaluationCount == 2U);
    return 0;
}

static int TestAllowWithdrawalAndSequence(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionManager_Snapshot_t snapshot;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_OK);
    evaluator.result = PERMISSION_COORDINATOR_EVALUATION_FAILED;
    CHECK(PermissionCoordinator_Poll(&coordinator, 200U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_INVALID);
    CHECK(g_lastTx.meta.updateSequence == 1U);
    CHECK(PermissionManager_GetSnapshot(&manager, 200U, &snapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(snapshot.stateRevision == 2U);
    CHECK(snapshot.updateSequence == 1U);
    CHECK(PermissionCoordinator_Poll(&coordinator, 400U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 3U);
    CHECK(g_lastTx.meta.updateSequence == 1U);

    evaluator.result = PERMISSION_COORDINATOR_EVALUATION_NO_DATA;
    CHECK(PermissionCoordinator_Poll(&coordinator, 600U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(g_txCalls == 4U);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_NO_DATA);
    CHECK(g_lastTx.meta.updateSequence == 1U);
    CHECK(PermissionManager_GetSnapshot(&manager, 600U, &snapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(snapshot.stateRevision == 4U);
    CHECK(snapshot.updateSequence == 1U);
    return 0;
}

static int TestSuccessfulQualityDegradationGetsNewSequence(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_OK);
    evaluator.evaluation.permission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    evaluator.evaluation.sourceQuality = DATA_QUALITY_INVALID;
    CHECK(PermissionCoordinator_Poll(&coordinator, 200U) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_INVALID);
    CHECK(g_lastTx.meta.updateSequence == 2U);
    return 0;
}

static int TestTxFailuresAreNotSuccess(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionCoordinator_Snapshot_t snapshot;
    FakeEvaluator_t evaluator = AllowEvaluator();

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    g_txResult = DOMAIN_IF_REJECTED;
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_TX_FAILED);
    CHECK(PermissionCoordinator_GetSnapshot(&coordinator, &snapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(snapshot.lastTxStatus == DOMAIN_IF_REJECTED);
    CHECK(snapshot.submissionAttemptCount == 1U);
    CHECK(snapshot.submissionSuccessCount == 0U);

    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    g_txResult = DOMAIN_IF_QUEUE_FULL;
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_TX_FAILED);
    CHECK(PermissionCoordinator_GetSnapshot(&coordinator, &snapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(snapshot.lastTxStatus == DOMAIN_IF_QUEUE_FULL);
    CHECK(snapshot.submissionAttemptCount == 1U);
    CHECK(snapshot.submissionSuccessCount == 0U);

    CHECK(Setup(&manager, &coordinator, &evaluator, false) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_TX_FAILED);
    CHECK(PermissionCoordinator_GetSnapshot(&coordinator, &snapshot) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(snapshot.lastTxStatus == DOMAIN_IF_NOT_AVAILABLE);
    return 0;
}

static int TestInvalidAllowFailsClosed(void)
{
    PermissionManager_t manager;
    PermissionCoordinator_t coordinator;
    PermissionManager_Snapshot_t snapshot;
    FakeEvaluator_t evaluator = AllowEvaluator();

    evaluator.evaluation.sourceQuality = DATA_QUALITY_INVALID;
    CHECK(Setup(&manager, &coordinator, &evaluator, true) == 0);
    CHECK(PermissionCoordinator_Poll(&coordinator, 0U) ==
          PERMISSION_COORDINATOR_EVALUATOR_ERROR);
    CHECK(!g_lastTx.vehiclePowerAllowed);
    CHECK(g_lastTx.quality == DATA_QUALITY_INVALID);
    CHECK(PermissionManager_GetSnapshot(&manager, 0U, &snapshot) ==
          PERMISSION_MANAGER_OK);
    CHECK(snapshot.updateSequence == 0U);
    CHECK(snapshot.stateRevision == 1U);
    return 0;
}

static int TestNullInitAndReinitialize(void)
{
    PermissionManager_t manager;
    PermissionManager_t uninitialized;
    PermissionCoordinator_t coordinator;
    PermissionCoordinator_t blank;
    PermissionCoordinator_Snapshot_t output;
    PermissionCoordinator_Snapshot_t saved;
    PermissionManager_Policy_t policy = Policy();
    FakeEvaluator_t evaluator = AllowEvaluator();

    (void)memset(&uninitialized, 0, sizeof(uninitialized));
    (void)memset(&blank, 0, sizeof(blank));
    CHECK(PermissionCoordinator_Init(NULL, &manager, NULL, NULL) ==
          PERMISSION_COORDINATOR_INVALID_ARGUMENT);
    CHECK(PermissionCoordinator_Init(&coordinator, NULL, NULL, NULL) ==
          PERMISSION_COORDINATOR_INVALID_ARGUMENT);
    CHECK(PermissionCoordinator_Init(
              &coordinator, &uninitialized, NULL, NULL) ==
          PERMISSION_COORDINATOR_MANAGER_ERROR);
    CHECK(PermissionManager_Init(&manager, &policy) == PERMISSION_MANAGER_OK);
    manager.policy.reevaluationPeriodMs = 199U;
    CHECK(PermissionCoordinator_Init(
              &coordinator, &manager, NULL, NULL) ==
          PERMISSION_COORDINATOR_INVALID_ARGUMENT);
    manager.policy.reevaluationPeriodMs = 200U;
    manager.policy.sourceValidityMs = 0U;
    CHECK(PermissionCoordinator_Init(
              &coordinator, &manager, NULL, NULL) ==
          PERMISSION_COORDINATOR_INVALID_ARGUMENT);
    CHECK(PermissionCoordinator_Poll(&blank, 0U) ==
          PERMISSION_COORDINATOR_NOT_INITIALIZED);
    (void)memset(&output, 0xA5, sizeof(output));
    saved = output;
    CHECK(PermissionCoordinator_GetSnapshot(&blank, &output) ==
          PERMISSION_COORDINATOR_NOT_INITIALIZED);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);

    CHECK(PermissionManager_Init(&manager, &policy) == PERMISSION_MANAGER_OK);
    CHECK(PermissionCoordinator_Init(
              &coordinator, &manager, Evaluate, &evaluator) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(PermissionCoordinator_Init(
              &coordinator, &manager, Evaluate, &evaluator) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(PermissionCoordinator_GetSnapshot(&coordinator, &output) ==
          PERMISSION_COORDINATOR_OK);
    CHECK(output.evaluationCount == 0U);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += TestFirstPollAndBoundary();
    failures += TestDelayedPollDoesNotCatchUp();
    failures += TestRollover();
    failures += TestMissingInputAndRepeatedFailure();
    failures += TestAllowWithdrawalAndSequence();
    failures += TestSuccessfulQualityDegradationGetsNewSequence();
    failures += TestTxFailuresAreNotSuccess();
    failures += TestInvalidAllowFailsClosed();
    failures += TestNullInitAndReinitialize();
    if (failures == 0)
    {
        printf("permission_coordinator: 9 tests passed\n");
    }
    return failures;
}
