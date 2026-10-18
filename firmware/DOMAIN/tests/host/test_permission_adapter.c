#include "permission_adapter.h"

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

static int GetSnapshot(
    PermissionManager_t *manager,
    uint32_t nowMs,
    PermissionManager_Snapshot_t *snapshot)
{
    return (PermissionManager_GetSnapshot(manager, nowMs, snapshot) ==
            PERMISSION_MANAGER_OK) ? 0 : 1;
}

static int TestAllowAndDeny(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation;
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    evaluation = Evaluation(PERMISSION_MANAGER_PERMISSION_ALLOW,
                            DATA_QUALITY_OK, 10U);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(0, GetSnapshot(&manager, 120U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_OK, output.quality);
    ASSERT_EQ(DATA_QUALITY_OK, output.meta.quality);
    ASSERT_EQ(30U, output.meta.ageMs);
    ASSERT_EQ(1U, output.meta.updateSequence);

    evaluation = Evaluation(PERMISSION_MANAGER_PERMISSION_DENY,
                            DATA_QUALITY_OK, 4U);
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 130U));
    ASSERT_EQ(0, GetSnapshot(&manager, 131U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_OK, output.quality);
    ASSERT_EQ(5U, output.meta.ageMs);
    ASSERT_EQ(2U, output.meta.updateSequence);
    return 0;
}

static int TestInitialNoDataAndReinitialize(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(0, GetSnapshot(&manager, 0U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, output.quality);
    ASSERT_EQ(0U, output.meta.updateSequence);

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 1U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(0, GetSnapshot(&manager, 2U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, output.meta.quality);
    ASSERT_EQ(0U, output.meta.updateSequence);
    return 0;
}

static int TestInvalidAndStaleUnknown(void)
{
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.effectivePermission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    snapshot.effectiveQuality = DATA_QUALITY_INVALID;
    snapshot.effectiveAgeMs = 22U;
    snapshot.updateSequence = 7U;
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_INVALID, output.quality);
    ASSERT_EQ(22U, output.meta.ageMs);
    ASSERT_EQ(7U, output.meta.updateSequence);

    snapshot.effectiveQuality = DATA_QUALITY_STALE;
    snapshot.effectiveAgeMs = UINT32_MAX;
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_EQ(DATA_QUALITY_STALE, output.quality);
    ASSERT_EQ(UINT32_MAX, output.meta.ageMs);
    return 0;
}

static int TestUnknownOkRejectedAndOutputUnchanged(void)
{
    PermissionManager_Snapshot_t snapshot;
    PermissionManager_Snapshot_t before;
    DomainIf_CisPowerPermission_t output;
    DomainIf_CisPowerPermission_t expected;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.effectivePermission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    snapshot.effectiveQuality = DATA_QUALITY_OK;
    snapshot.updateSequence = 9U;
    before = snapshot;
    (void)memset(&output, 0xA5, sizeof(output));
    expected = output;

    ASSERT_EQ(PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_EQ(0, memcmp(&output, &expected, sizeof(output)));
    ASSERT_EQ(0, memcmp(&snapshot, &before, sizeof(snapshot)));
    return 0;
}

static int TestTtlBoundary(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 100U);
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 1000U));
    ASSERT_EQ(0, GetSnapshot(&manager, 1399U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(output.vehiclePowerAllowed);
    ASSERT_EQ(499U, output.meta.ageMs);

    ASSERT_EQ(0, GetSnapshot(&manager, 1400U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_STALE, output.quality);
    ASSERT_EQ(500U, output.meta.ageMs);
    ASSERT_EQ(1U, output.meta.updateSequence);
    return 0;
}

static int TestFailureWithdrawalSequenceAndRevision(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 5U);
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 100U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_FAILED, 110U));
    ASSERT_EQ(0, GetSnapshot(&manager, 110U, &snapshot));
    ASSERT_EQ(1U, snapshot.updateSequence);
    ASSERT_EQ(2U, snapshot.stateRevision);
    ASSERT_TRUE(snapshot.invalidatedByUnsuccessfulAttempt);
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_INVALID, output.quality);
    ASSERT_EQ(1U, output.meta.updateSequence);
    ASSERT_EQ(15U, output.meta.ageMs);
    return 0;
}

static int TestNoDataWithdrawal(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_ALLOW, DATA_QUALITY_OK, 0U);
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 10U));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordUnsuccessfulAttempt(
                  &manager, PERMISSION_MANAGER_ATTEMPT_NO_DATA, 20U));
    ASSERT_EQ(0, GetSnapshot(&manager, 20U, &snapshot));
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_TRUE(!output.vehiclePowerAllowed);
    ASSERT_EQ(DATA_QUALITY_NO_DATA, output.quality);
    ASSERT_EQ(1U, output.meta.updateSequence);
    return 0;
}

static int TestAgeRolloverAndSnapshotUnchanged(void)
{
    PermissionManager_t manager;
    PermissionManager_Policy_t policy = DefaultPolicy();
    PermissionManager_Evaluation_t evaluation = Evaluation(
        PERMISSION_MANAGER_PERMISSION_DENY, DATA_QUALITY_OK, 1U);
    PermissionManager_Snapshot_t snapshot;
    PermissionManager_Snapshot_t before;
    DomainIf_CisPowerPermission_t output;

    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_Init(&manager, &policy));
    ASSERT_EQ(PERMISSION_MANAGER_OK,
              PermissionManager_RecordEvaluation(&manager, &evaluation,
                                                 UINT32_MAX - 5U));
    ASSERT_EQ(0, GetSnapshot(&manager, 4U, &snapshot));
    ASSERT_EQ(11U, snapshot.effectiveAgeMs);
    before = snapshot;
    ASSERT_EQ(PERMISSION_ADAPTER_OK,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_EQ(11U, output.meta.ageMs);
    ASSERT_EQ(0, memcmp(&snapshot, &before, sizeof(snapshot)));
    return 0;
}

static int TestNullAndInvalidCombinations(void)
{
    PermissionManager_Snapshot_t snapshot;
    DomainIf_CisPowerPermission_t output;
    DomainIf_CisPowerPermission_t expected;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    (void)memset(&output, 0x5A, sizeof(output));
    expected = output;
    ASSERT_EQ(PERMISSION_ADAPTER_INVALID_ARGUMENT,
              PermissionAdapter_ConvertCisPowerPermission(NULL, &output));
    ASSERT_EQ(PERMISSION_ADAPTER_INVALID_ARGUMENT,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot, NULL));
    ASSERT_EQ(0, memcmp(&output, &expected, sizeof(output)));

    snapshot.effectivePermission =
        (PermissionManager_Permission_t)2;
    snapshot.effectiveQuality = DATA_QUALITY_INVALID;
    ASSERT_EQ(PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_EQ(0, memcmp(&output, &expected, sizeof(output)));

    snapshot.effectivePermission = PERMISSION_MANAGER_PERMISSION_UNKNOWN;
    snapshot.effectiveQuality = (DataQuality_t)4;
    ASSERT_EQ(PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT,
              PermissionAdapter_ConvertCisPowerPermission(&snapshot,
                                                           &output));
    ASSERT_EQ(0, memcmp(&output, &expected, sizeof(output)));
    return 0;
}

typedef int (*TestFunction_t)(void);

int main(void)
{
    static const TestFunction_t tests[] = {
        TestAllowAndDeny,
        TestInitialNoDataAndReinitialize,
        TestInvalidAndStaleUnknown,
        TestUnknownOkRejectedAndOutputUnchanged,
        TestTtlBoundary,
        TestFailureWithdrawalSequenceAndRevision,
        TestNoDataWithdrawal,
        TestAgeRolloverAndSnapshotUnchanged,
        TestNullAndInvalidCombinations
    };
    size_t index;

    for (index = 0U; index < (sizeof(tests) / sizeof(tests[0])); ++index)
    {
        if (tests[index]() != 0)
        {
            return 1;
        }
    }

    printf("permission_adapter: %u tests passed\n",
           (unsigned int)(sizeof(tests) / sizeof(tests[0])));
    return 0;
}
