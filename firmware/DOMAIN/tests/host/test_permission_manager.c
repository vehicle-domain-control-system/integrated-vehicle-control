#include "permission_manager.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                \
    do                                                                        \
    {                                                                         \
        if (!(condition))                                                     \
        {                                                                     \
            printf("%s:%d assertion failed: %s\n", __FILE__, __LINE__,       \
                   #condition);                                               \
            return 1;                                                         \
        }                                                                     \
    } while (0)

#define ASSERT_EQ(expected, actual) ASSERT_TRUE((expected) == (actual))

static PermissionManager_Policy_t DefaultPolicy(void)
{
    PermissionManager_Policy_t policy;

    policy.reevaluationPeriodMs = 200U;
    policy.sourceValidityMs = 500U;
    return policy;
}

static PermissionManager_Evaluation_t Evaluation(
    PermissionManager_Permission_t permission,
    DataQuality_t quality,
    uint32_t sourceAgeMs)
{
    PermissionManager_Evaluation_t evaluation;

    evaluation.permission = permission;
    evaluation.sourceQuality = quality;
    evaluation.sourceAgeMs = sourceAgeMs;
    return evaluation;
}

static int TestInitialUnknownAndDue(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Snapshot_t snapshot;
    bool due = false;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 100U, &snapshot));
    ASSERT_TRUE(!snapshot.hasEvaluation);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, snapshot.effectiveQuality);
    ASSERT_TRUE(!snapshot.activeAllowed);
    ASSERT_EQ(PERMISSION_MANAGER_ATTEMPT_NONE,
              snapshot.lastAttemptOutcome);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 100U, &due));
    ASSERT_TRUE(due);
    return 0;
}

static int TestFirstEvaluation(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 20U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 1000U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1010U, &snapshot));
    ASSERT_TRUE(snapshot.hasEvaluation);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.evaluatedPermission);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.effectivePermission);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(10U, snapshot.localElapsedMs);
    ASSERT_EQ(30U, snapshot.effectiveAgeMs);
    ASSERT_TRUE(snapshot.activeAllowed);
    return 0;
}

static int TestPermissionValueDistinction(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation;
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    evaluation = Evaluation(PERMISSION_MANAGER_PERMISSION_DENY,
                            DATA_QUALITY_OK, 0U);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 1U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_DENY,
              snapshot.effectivePermission);
    ASSERT_TRUE(!snapshot.activeAllowed);

    evaluation = Evaluation(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
                            DATA_QUALITY_OK, 0U);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 2U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 2U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestReevaluationPeriodBoundary(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_DENY, DATA_QUALITY_OK, 0U);
    bool due = true;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 1000U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 1199U, &due));
    ASSERT_TRUE(!due);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 1200U, &due));
    ASSERT_TRUE(due);
    return 0;
}

static int TestValidityBoundaryAndFailClosed(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 100U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 1000U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1399U, &snapshot));
    ASSERT_EQ(499U, snapshot.effectiveAgeMs);
    ASSERT_TRUE(!snapshot.expired);
    ASSERT_TRUE(snapshot.activeAllowed);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1400U, &snapshot));
    ASSERT_EQ(500U, snapshot.effectiveAgeMs);
    ASSERT_TRUE(snapshot.expired);
    ASSERT_EQ(DATA_QUALITY_STALE, snapshot.effectiveQuality);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestSameValueReevaluationIncrementsSequence(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_DENY, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 1U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 2U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 2U, &snapshot));
    ASSERT_EQ(2U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.evaluatedAtMs);
    return 0;
}

static int TestReadAndRetransmissionDoNotRefresh(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t first;
    PermissionManager_Snapshot_t second;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 200U, &first));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 600U, &second));
    ASSERT_EQ(first.updateSequence, second.updateSequence);
    ASSERT_EQ(first.evaluatedAtMs, second.evaluatedAtMs);
    ASSERT_TRUE(first.activeAllowed);
    ASSERT_TRUE(!second.activeAllowed);
    ASSERT_TRUE(second.expired);
    return 0;
}

static int TestUnusableQualityNeverAllows(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    const DataQuality_t qualities[] = {
        DATA_QUALITY_STALE,
        DATA_QUALITY_INVALID,
        DATA_QUALITY_NO_DATA
    };
    size_t index;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    for (index = 0U; index < (sizeof(qualities) / sizeof(qualities[0]));
         ++index)
    {
        PermissionManager_Evaluation_t evaluation = Evaluation(
            PERMISSION_MANAGER_PERMISSION_ALLOW, qualities[index], 0U);
        PermissionManager_Snapshot_t snapshot;

        ASSERT_EQ(PERMISSION_MANAGER_OK,
                  PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                     (uint32_t)index));
        ASSERT_EQ(PERMISSION_MANAGER_OK,
                  PermissionManager_GetSnapshot(&manager, (uint32_t)index,
                                                &snapshot));
        ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
                  snapshot.evaluatedPermission);
        ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
                  snapshot.effectivePermission);
        ASSERT_EQ(qualities[index], snapshot.sourceQuality);
        ASSERT_TRUE(!snapshot.activeAllowed);
    }
    return 0;
}

static int TestAlreadyExpiredInputNeverAllows(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 500U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 100U, &snapshot));
    ASSERT_TRUE(snapshot.expired);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestFailureAndNoDataInvalidateImmediately(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    bool due = false;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED, 200U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 300U, &snapshot));
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.stateRevision);
    ASSERT_EQ(PERMISSION_MANAGER_ATTEMPT_FAILED,
              snapshot.lastAttemptOutcome);
    ASSERT_TRUE(snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.evaluatedPermission);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_EQ(DATA_QUALITY_OK, snapshot.sourceQuality);
    ASSERT_EQ(DATA_QUALITY_INVALID, snapshot.effectiveQuality);
    ASSERT_EQ(100U, snapshot.evaluatedAtMs);
    ASSERT_EQ(0U, snapshot.sourceAgeAtEvaluationMs);
    ASSERT_TRUE(!snapshot.activeAllowed);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 301U, &snapshot));
    ASSERT_TRUE(!snapshot.activeAllowed);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.stateRevision);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 399U, &due));
    ASSERT_TRUE(!due);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 400U, &due));
    ASSERT_TRUE(due);

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_NO_DATA, 450U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 600U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_ATTEMPT_NO_DATA,
              snapshot.lastAttemptOutcome);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(3U, snapshot.stateRevision);
    ASSERT_TRUE(snapshot.expired);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, snapshot.effectiveQuality);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestNoDataImmediatelyInvalidatesAllow(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 10U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_NO_DATA, 150U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 150U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.evaluatedPermission);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, snapshot.effectiveQuality);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.stateRevision);
    ASSERT_EQ(100U, snapshot.evaluatedAtMs);
    ASSERT_EQ(10U, snapshot.sourceAgeAtEvaluationMs);
    ASSERT_TRUE(snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestFailureBeforeFirstEvaluationIsDistinguished(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED, 50U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 50U, &snapshot));
    ASSERT_TRUE(!snapshot.hasEvaluation);
    ASSERT_EQ(PERMISSION_MANAGER_ATTEMPT_FAILED,
              snapshot.lastAttemptOutcome);
    ASSERT_TRUE(snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, snapshot.sourceQuality);
    ASSERT_EQ(DATA_QUALITY_INVALID, snapshot.effectiveQuality);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_EQ(0U, snapshot.updateSequence);
    ASSERT_EQ(1U, snapshot.stateRevision);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestDueWithoutAttemptDoesNotInvalidate(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    bool due = false;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 300U, &due));
    ASSERT_TRUE(due);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 300U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.effectivePermission);
    ASSERT_TRUE(snapshot.activeAllowed);
    ASSERT_TRUE(!snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(1U, snapshot.stateRevision);
    return 0;
}

static int TestSuccessfulEvaluationRecoversAfterFailure(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED, 200U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 250U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 250U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              snapshot.effectivePermission);
    ASSERT_EQ(DATA_QUALITY_OK, snapshot.effectiveQuality);
    ASSERT_EQ(PERMISSION_MANAGER_ATTEMPT_SUCCEEDED,
              snapshot.lastAttemptOutcome);
    ASSERT_TRUE(!snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_TRUE(snapshot.activeAllowed);
    ASSERT_EQ(2U, snapshot.updateSequence);
    ASSERT_EQ(3U, snapshot.stateRevision);
    return 0;
}

static int TestDuplicateFailureAndRollover(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 UINT32_MAX - 20U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED,
                  UINT32_MAX - 10U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED, 5U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 10U, &snapshot));
    ASSERT_EQ(31U, snapshot.localElapsedMs);
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(3U, snapshot.stateRevision);
    ASSERT_EQ(5U, snapshot.lastAttemptAtMs);
    ASSERT_EQ(DATA_QUALITY_INVALID, snapshot.effectiveQuality);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              snapshot.effectivePermission);
    ASSERT_TRUE(!snapshot.activeAllowed);
    return 0;
}

static int TestTimeRolloverAndAgeSaturation(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK,
        UINT32_MAX - 5U);
    PermissionManager_Snapshot_t snapshot;
    bool due = false;

    policy.sourceValidityMs = UINT32_MAX;
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 UINT32_MAX - 50U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 49U, &snapshot));
    ASSERT_EQ(100U, snapshot.localElapsedMs);
    ASSERT_EQ(UINT32_MAX, snapshot.effectiveAgeMs);
    ASSERT_TRUE(snapshot.expired);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_IsEvaluationDue(&manager, 149U, &due));
    ASSERT_TRUE(due);
    return 0;
}

static int TestSequenceRollover(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_DENY, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    manager.stored.updateSequence = UINT32_MAX;
    manager.stateRevision = UINT32_MAX;
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 5U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 5U, &snapshot));
    ASSERT_EQ(0U, snapshot.updateSequence);
    ASSERT_EQ(0U, snapshot.stateRevision);
    ASSERT_TRUE(snapshot.hasEvaluation);
    return 0;
}

static int TestInvalidArgumentsAndEnums(void)
{
    PermissionManager_t manager;
    PermissionManager_t uninitialized;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    bool due;

    (void)memset(&uninitialized, 0, sizeof(uninitialized));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_Init(NULL, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_Init(&manager, NULL));
    ASSERT_EQ(PERMISSION_MANAGER_NOT_INITIALIZED,
              PermissionManager_GetSnapshot(&uninitialized, 0U, &snapshot));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    evaluation.permission = (PermissionManager_Permission_t)2;
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 0U));
    evaluation.permission = PERMISSION_MANAGER_PERMISSION_ALLOW;
    evaluation.sourceQuality = (DataQuality_t)99;
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 0U));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_SUCCEEDED, 0U));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_GetSnapshot(&manager, 0U, NULL));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_IsEvaluationDue(&manager, 0U, NULL));
    ASSERT_EQ(PERMISSION_MANAGER_INVALID_ARGUMENT,
              PermissionManager_IsEvaluationDue(NULL, 0U, &due));
    return 0;
}

static int TestSnapshotCopyAndReinitialization(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    PermissionManager_Snapshot_t afterMutation;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation, 1U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1U, &snapshot));
    snapshot.effectivePermission = PERMISSION_MANAGER_PERMISSION_DENY;
    snapshot.updateSequence = 77U;
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1U, &afterMutation));
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_ALLOW,
              afterMutation.effectivePermission);
    ASSERT_EQ(1U, afterMutation.updateSequence);

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_GetSnapshot(&manager, 1U, &afterMutation));
    ASSERT_TRUE(!afterMutation.hasEvaluation);
    ASSERT_EQ(PERMISSION_MANAGER_PERMISSION_UNKNOWN,
              afterMutation.effectivePermission);
    ASSERT_EQ(0U, afterMutation.updateSequence);
    ASSERT_EQ(0U, afterMutation.stateRevision);
    ASSERT_TRUE(!afterMutation.invalidatedByUnsuccessfulAttempt);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestInitialUnknownAndDue();
    failures += TestFirstEvaluation();
    failures += TestPermissionValueDistinction();
    failures += TestReevaluationPeriodBoundary();
    failures += TestValidityBoundaryAndFailClosed();
    failures += TestSameValueReevaluationIncrementsSequence();
    failures += TestReadAndRetransmissionDoNotRefresh();
    failures += TestUnusableQualityNeverAllows();
    failures += TestAlreadyExpiredInputNeverAllows();
    failures += TestFailureAndNoDataInvalidateImmediately();
    failures += TestNoDataImmediatelyInvalidatesAllow();
    failures += TestFailureBeforeFirstEvaluationIsDistinguished();
    failures += TestDueWithoutAttemptDoesNotInvalidate();
    failures += TestSuccessfulEvaluationRecoversAfterFailure();
    failures += TestDuplicateFailureAndRollover();
    failures += TestTimeRolloverAndAgeSaturation();
    failures += TestSequenceRollover();
    failures += TestInvalidArgumentsAndEnums();
    failures += TestSnapshotCopyAndReinitialization();

    if (failures == 0)
    {
        printf("permission_manager: 19 tests passed\n");
    }
    return failures;
}
