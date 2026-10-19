#include "cis_occupant_manager.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_tests;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void BeginTest(const char *name)
{
    ++g_tests;
    (void)printf("%s: ", name);
}

static void EndTest(int before)
{
    (void)printf((g_failures == before) ? "PASS\n" : "FAIL\n");
}

static ValueQuality_t Quality(
    ValueValidity_t validity,
    QualityReason_t reason,
    DataQuality_t quality,
    uint32_t ageMs,
    uint32_t sequence)
{
    ValueQuality_t value;

    value.validity = validity;
    value.reason = reason;
    value.meta.quality = quality;
    value.meta.ageMs = ageMs;
    value.meta.updateSequence = sequence;
    return value;
}

static CisOccupantManager_Policy_t Policy(void)
{
    CisOccupantManager_Policy_t policy;

    policy.presenceTtlMs = 100U;
    policy.countTtlMs = 200U;
    return policy;
}

static CisOccupantState_t NormalState(void)
{
    CisOccupantState_t state;

    state.occupantPresent = true;
    state.occupantCount = 2U;
    state.presenceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 10U, 7U);
    state.countQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 20U, 7U);
    state.sourceGeneration = 3U;
    state.visionFunctionStatus = CIS_FUNCTION_STATUS_ACTIVE;
    return state;
}

static void InitializeAndAdmit(
    CisOccupantManager_t *manager,
    const CisOccupantManager_Policy_t *policy,
    SourceGeneration_t generation)
{
    CHECK(CisOccupantManager_Init(manager, policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_Invalidate(manager, generation) ==
          CIS_OCCUPANT_MANAGER_OK);
}

static void Test_InitialNoDataAdmissionAndReinit(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_InitialNoDataAdmissionAndReinit");
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 10U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(!snapshot.hasObservation);
    CHECK(snapshot.invalidated);
    CHECK(snapshot.effective.presenceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(CisOccupantManager_Store(&manager, &state, 10U) ==
          CIS_OCCUPANT_MANAGER_NOT_ADMITTED);
    CHECK(CisOccupantManager_Invalidate(&manager, state.sourceGeneration) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_Store(&manager, &state, 10U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 20U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(!snapshot.hasObservation);
    CHECK(snapshot.invalidated);
    CHECK(snapshot.effective.presenceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_ZeroTtlIsRejectedWithoutChangingManager(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Policy_t policy = Policy();
    int before = g_failures;

    BeginTest("Test_ZeroTtlIsRejectedWithoutChangingManager");
    InitializeAndAdmit(&manager, &policy, 3U);
    saved = manager;
    policy.presenceTtlMs = 0U;
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    policy = Policy();
    policy.countTtlMs = 0U;
    CHECK(CisOccupantManager_Init(&manager, &policy) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_StorePreservesIndependentQuality(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_StorePreservesIndependentQuality");
    state.presenceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_STALE,
        DATA_QUALITY_STALE, 11U, 9U);
    state.countQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_SENSOR_FAULT,
        DATA_QUALITY_INVALID, 22U, 9U);
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 30U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 30U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(memcmp(&snapshot.original, &state, sizeof(state)) == 0);
    CHECK(snapshot.effective.presenceQuality.meta.quality ==
          DATA_QUALITY_STALE);
    CHECK(snapshot.effective.countQuality.meta.quality ==
          DATA_QUALITY_INVALID);
    EndTest(before);
}

static void Test_DuplicateConflictAndOutOfOrderAreAtomic(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_DuplicateConflictAndOutOfOrderAreAtomic");
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    state.presenceQuality.meta.updateSequence = 10U;
    state.countQuality.meta.updateSequence = 10U;
    CHECK(CisOccupantManager_Store(&manager, &state, 100U) ==
          CIS_OCCUPANT_MANAGER_OK);
    saved = manager;
    CHECK(CisOccupantManager_Store(&manager, &state, 101U) ==
          CIS_OCCUPANT_MANAGER_DUPLICATE);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    state.occupantCount = 3U;
    CHECK(CisOccupantManager_Store(&manager, &state, 101U) ==
          CIS_OCCUPANT_MANAGER_CONFLICT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    state.occupantCount = 2U;
    state.visionFunctionStatus = CIS_FUNCTION_STATUS_READY;
    CHECK(CisOccupantManager_Store(&manager, &state, 101U) ==
          CIS_OCCUPANT_MANAGER_CONFLICT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    state.visionFunctionStatus = CIS_FUNCTION_STATUS_ACTIVE;
    state.presenceQuality.meta.updateSequence = 9U;
    state.countQuality.meta.updateSequence = 9U;
    CHECK(CisOccupantManager_Store(&manager, &state, 102U) ==
          CIS_OCCUPANT_MANAGER_OUT_OF_ORDER);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_SameSequenceIndependentQualityWithdrawal(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    ValueQuality_t originalCount;
    int before = g_failures;

    BeginTest("Test_SameSequenceIndependentQualityWithdrawal");
    state.presenceQuality.meta.ageMs = 0U;
    state.countQuality.meta.ageMs = 0U;
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 100U) ==
          CIS_OCCUPANT_MANAGER_OK);
    originalCount = state.countQuality;

    state.presenceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_DATA_INVALID,
        DATA_QUALITY_INVALID, 5U, 7U);
    state.countQuality.meta.ageMs = 999U;
    CHECK(CisOccupantManager_Store(&manager, &state, 105U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 105U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.original.presenceQuality.meta.quality ==
          DATA_QUALITY_INVALID);
    CHECK(memcmp(&snapshot.original.countQuality, &originalCount,
                 sizeof(originalCount)) == 0);

    state.presenceQuality = snapshot.original.presenceQuality;
    state.countQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 10U, 7U);
    CHECK(CisOccupantManager_Store(&manager, &state, 110U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 110U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.original.presenceQuality.meta.quality ==
          DATA_QUALITY_INVALID);
    CHECK(snapshot.original.countQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.presenceQuality.meta.ageMs == 10U);
    CHECK(snapshot.effective.countQuality.meta.ageMs == 10U);
    EndTest(before);
}

static void Test_SameSequenceRecoveryAndAgeRejuvenationAreRejected(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_SameSequenceRecoveryAndAgeRejuvenationAreRejected");
    state.presenceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 20U, 7U);
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 100U) ==
          CIS_OCCUPANT_MANAGER_OK);
    saved = manager;
    state.presenceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 25U, 7U);
    CHECK(CisOccupantManager_Store(&manager, &state, 105U) ==
          CIS_OCCUPANT_MANAGER_CONFLICT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);

    state.presenceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_SENSOR_FAULT,
        DATA_QUALITY_INVALID, 1U, 7U);
    CHECK(CisOccupantManager_Store(&manager, &state, 110U) ==
          CIS_OCCUPANT_MANAGER_CONFLICT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_GenerationAdmissionBlocksDelayedOldCallback(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_GenerationAdmissionBlocksDelayedOldCallback");
    state.sourceGeneration = 100U;
    state.presenceQuality.meta.updateSequence = 500U;
    state.countQuality.meta.updateSequence = 500U;
    InitializeAndAdmit(&manager, &policy, 100U);
    CHECK(CisOccupantManager_Store(&manager, &state, 1U) ==
          CIS_OCCUPANT_MANAGER_OK);

    state.sourceGeneration = 2U;
    state.presenceQuality.meta.updateSequence = 0U;
    state.countQuality.meta.updateSequence = 0U;
    CHECK(CisOccupantManager_Store(&manager, &state, 2U) ==
          CIS_OCCUPANT_MANAGER_NOT_ADMITTED);
    CHECK(CisOccupantManager_Invalidate(&manager, 2U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_Store(&manager, &state, 2U) ==
          CIS_OCCUPANT_MANAGER_OK);

    state.sourceGeneration = 100U;
    state.presenceQuality.meta.updateSequence = 501U;
    state.countQuality.meta.updateSequence = 501U;
    CHECK(CisOccupantManager_Store(&manager, &state, 3U) ==
          CIS_OCCUPANT_MANAGER_NOT_ADMITTED);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 3U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.original.sourceGeneration == 2U);
    EndTest(before);
}

static void Test_InvalidateIsAtomicAndPreservesDiagnosticOriginal(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_InvalidateIsAtomicAndPreservesDiagnosticOriginal");
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 10U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_Invalidate(&manager, 4U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 11U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.invalidated);
    CHECK(memcmp(&snapshot.original, &state, sizeof(state)) == 0);
    CHECK(snapshot.effective.presenceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.countQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    saved = manager;
    CHECK(CisOccupantManager_Invalidate(&manager, 0U) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_SequenceRolloverAndTtlBoundaries(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_SequenceRolloverAndTtlBoundaries");
    state.presenceQuality.meta.updateSequence = UINT32_MAX;
    state.countQuality.meta.updateSequence = UINT32_MAX;
    state.presenceQuality.meta.ageMs = 0U;
    state.countQuality.meta.ageMs = 0U;
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 1000U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 1099U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.effective.presenceQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 1100U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.effective.presenceQuality.meta.quality ==
          DATA_QUALITY_STALE);
    state.presenceQuality.meta.updateSequence = 0U;
    state.countQuality.meta.updateSequence = 0U;
    state.occupantCount = 1U;
    CHECK(CisOccupantManager_Store(&manager, &state, 1100U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 1100U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.original.occupantCount == 1U);
    EndTest(before);
}

static void Test_TimeRolloverAndAgeSaturation(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_TimeRolloverAndAgeSaturation");
    state.presenceQuality.meta.ageMs = UINT32_MAX - 2U;
    state.countQuality.meta.ageMs = 1U;
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(
              &manager, &state, UINT32_MAX - 4U) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(CisOccupantManager_GetSnapshot(&manager, 5U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(snapshot.presenceTiming.localElapsedMs == 10U);
    CHECK(snapshot.presenceTiming.effectiveAgeMs == UINT32_MAX);
    CHECK(snapshot.countTiming.effectiveAgeMs == 11U);
    EndTest(before);
}

static void Test_InvalidInputAndGetterFailureAreAtomic(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Snapshot_t output;
    CisOccupantManager_Snapshot_t savedOutput;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    DataQuality_t badQuality = (DataQuality_t)99;
    int before = g_failures;

    BeginTest("Test_InvalidInputAndGetterFailureAreAtomic");
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    saved = manager;
    state.countQuality.meta.updateSequence = 8U;
    CHECK(CisOccupantManager_Store(&manager, &state, 1U) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    state = NormalState();
    state.occupantCount = 6U;
    CHECK(CisOccupantManager_Store(&manager, &state, 1U) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    state = NormalState();
    (void)memcpy(&state.countQuality.meta.quality,
                 &badQuality, sizeof(badQuality));
    CHECK(CisOccupantManager_Store(&manager, &state, 1U) ==
          CIS_OCCUPANT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    (void)memset(&manager, 0, sizeof(manager));
    (void)memset(&output, 0xA5, sizeof(output));
    savedOutput = output;
    CHECK(CisOccupantManager_GetSnapshot(&manager, 0U, &output) ==
          CIS_OCCUPANT_MANAGER_NOT_INITIALIZED);
    CHECK(memcmp(&output, &savedOutput, sizeof(output)) == 0);
    EndTest(before);
}

static void Test_GetterIsReadOnly(void)
{
    CisOccupantManager_t manager;
    CisOccupantManager_t saved;
    CisOccupantManager_Snapshot_t snapshot;
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_GetterIsReadOnly");
    InitializeAndAdmit(&manager, &policy, state.sourceGeneration);
    CHECK(CisOccupantManager_Store(&manager, &state, 100U) ==
          CIS_OCCUPANT_MANAGER_OK);
    saved = manager;
    CHECK(CisOccupantManager_GetSnapshot(&manager, 150U, &snapshot) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

int main(void)
{
    Test_InitialNoDataAdmissionAndReinit();
    Test_ZeroTtlIsRejectedWithoutChangingManager();
    Test_StorePreservesIndependentQuality();
    Test_DuplicateConflictAndOutOfOrderAreAtomic();
    Test_SameSequenceIndependentQualityWithdrawal();
    Test_SameSequenceRecoveryAndAgeRejuvenationAreRejected();
    Test_GenerationAdmissionBlocksDelayedOldCallback();
    Test_InvalidateIsAtomicAndPreservesDiagnosticOriginal();
    Test_SequenceRolloverAndTtlBoundaries();
    Test_TimeRolloverAndAgeSaturation();
    Test_InvalidInputAndGetterFailureAreAtomic();
    Test_GetterIsReadOnly();

    (void)printf("cis_occupant_manager: %d tests, %d failures\n",
                 g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
