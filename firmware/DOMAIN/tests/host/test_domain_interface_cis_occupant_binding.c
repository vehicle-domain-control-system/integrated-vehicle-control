#include "domain_interface_cis_occupant_binding.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t nowMs;
    bool available;
} TestClock_t;

static CisOccupantManager_t g_manager;
static DomainCisOccupantPriv_t g_private;
static TestClock_t g_clock;
static int g_failures;
static int g_tests;
static uint32_t g_observerCalls;

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

static bool GetNow(void *context, uint32_t *outNowMs)
{
    TestClock_t *clock = (TestClock_t *)context;

    if ((clock == NULL) || (outNowMs == NULL) || !clock->available)
    {
        return false;
    }
    *outNowMs = clock->nowMs;
    return true;
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

static CisOccupantState_t NormalState(void)
{
    CisOccupantState_t state;

    state.occupantPresent = true;
    state.occupantCount = 2U;
    state.presenceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 10U, 20U);
    state.countQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 15U, 20U);
    state.sourceGeneration = 9U;
    state.visionFunctionStatus = CIS_FUNCTION_STATUS_ACTIVE;
    return state;
}

static CisOccupantManager_Policy_t Policy(void)
{
    CisOccupantManager_Policy_t policy;

    policy.presenceTtlMs = 100U;
    policy.countTtlMs = 200U;
    return policy;
}

static DomainIf_Status_t ObserveCisStatus(
    const DomainIf_CisStatusReport_t *status)
{
    (void)status;
    ++g_observerCalls;
    return DOMAIN_IF_OK;
}

static void Configure(void)
{
    CisOccupantManager_Policy_t policy = Policy();
    DomainCisOccupantPriv_Transition_t transition;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;

    CHECK(CisOccupantManager_Init(&g_manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&g_private, &g_manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    g_clock.nowMs = 1000U;
    g_clock.available = true;
    CHECK(DomainIfCisOccupantBinding_Configure(
              &g_manager, &g_private, GetNow, &g_clock) == DOMAIN_IF_OK);
    transition.admittedSourceGeneration = 9U;
    transition.transitionEpoch = 1U;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &g_private, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
}

static void Test_PreInitializationAndNull(void)
{
    CisOccupantManager_Policy_t policy = Policy();
    CisOccupantState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_PreInitializationAndNull");
    CHECK(DomainIf_RxCisOccupant(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_QueryGetCisOccupant(0U, NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(CisOccupantManager_Init(&g_manager, &policy) ==
          CIS_OCCUPANT_MANAGER_OK);
    CHECK(DomainCisOccupantPriv_Init(&g_private, &g_manager) ==
          DOMAIN_CIS_OCCUPANT_PRIV_OK);
    CHECK(DomainIfCisOccupantBinding_Configure(
              &g_manager, &g_private, GetNow, &g_clock) ==
          DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_NOT_INITIALIZED);
    EndTest(before);
}

static void Test_ConfigureAndInitialNoData(void)
{
    CisOccupantState_t input = NormalState();
    CisOccupantState_t output;
    CisOccupantState_t saved;
    int before = g_failures;

    BeginTest("Test_ConfigureAndInitialNoData");
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    (void)memset(&output, 0xA5, sizeof(output));
    saved = output;
    CHECK(DomainIf_RxCisOccupant(&input) == DOMAIN_IF_NOT_AVAILABLE);
    CHECK(DomainIf_QueryGetCisOccupant(1000U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(DomainIfCisOccupantBinding_Configure(
              NULL, &g_private, GetNow, &g_clock) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIfCisOccupantBinding_Configure(
              &g_manager, NULL, GetNow, &g_clock) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIfCisOccupantBinding_Configure(
              &g_manager, &g_private, NULL, &g_clock) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    Configure();
    CHECK(DomainIf_QueryGetCisOccupant(1000U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.visionFunctionStatus == CIS_FUNCTION_STATUS_NOT_READY);
    EndTest(before);
}

static void Test_NormalStoreQueryAndNoInventedObserver(void)
{
    DomainIf_RxObserver_t observer;
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    int before = g_failures;

    BeginTest("Test_NormalStoreQueryAndNoInventedObserver");
    (void)memset(&observer, 0, sizeof(observer));
    observer.onCisStatus = ObserveCisStatus;
    g_observerCalls = 0U;
    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    g_clock.nowMs = 2000U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(g_observerCalls == 0U);
    CHECK(DomainIf_QueryGetCisOccupant(2000U, &output) == DOMAIN_IF_OK);
    CHECK(memcmp(&output, &state, sizeof(state)) == 0);
    EndTest(before);
}

static void Test_IndependentQualityAndTtl(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    int before = g_failures;

    BeginTest("Test_IndependentQualityAndTtl");
    Configure();
    state.presenceQuality.meta.ageMs = 0U;
    state.countQuality.meta.ageMs = 0U;
    g_clock.nowMs = 3000U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3099U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3100U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_STALE);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(DomainIf_QueryGetCisOccupant(3200U, &output) == DOMAIN_IF_OK);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_STALE);
    EndTest(before);
}

static void Test_DuplicateOutOfOrderAndRollover(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    int before = g_failures;

    BeginTest("Test_DuplicateOutOfOrderAndRollover");
    Configure();
    state.presenceQuality.meta.updateSequence = UINT32_MAX;
    state.countQuality.meta.updateSequence = UINT32_MAX;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    state.occupantCount = 3U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    CHECK(DomainIf_QueryGetCisOccupant(g_clock.nowMs, &output) == DOMAIN_IF_OK);
    CHECK(output.occupantCount == 2U);
    state.presenceQuality.meta.updateSequence = UINT32_MAX - 1U;
    state.countQuality.meta.updateSequence = UINT32_MAX - 1U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    state.presenceQuality.meta.updateSequence = 0U;
    state.countQuality.meta.updateSequence = 0U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(g_clock.nowMs, &output) == DOMAIN_IF_OK);
    CHECK(output.occupantCount == 3U);
    EndTest(before);
}

static void Test_SameSequenceWithdrawalAndRecovery(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    int before = g_failures;

    BeginTest("Test_SameSequenceWithdrawalAndRecovery");
    Configure();
    state.presenceQuality.meta.ageMs = 0U;
    state.countQuality.meta.ageMs = 0U;
    g_clock.nowMs = 2000U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);

    g_clock.nowMs = 2005U;
    state.presenceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_DATA_INVALID,
        DATA_QUALITY_INVALID, 5U, 20U);
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(2005U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_INVALID);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_OK);

    state.countQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 10U, 20U);
    g_clock.nowMs = 2010U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(2010U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_INVALID);
    CHECK(output.countQuality.meta.quality == DATA_QUALITY_NO_DATA);

    state.presenceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 15U, 20U);
    g_clock.nowMs = 2015U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    CHECK(DomainIf_QueryGetCisOccupant(2015U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_INVALID);
    EndTest(before);
}

static void Test_TransitionFailureIsFailClosed(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    CisOccupantState_t savedOutput;
    DomainCisOccupantPriv_Transition_t transition;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    int before = g_failures;

    BeginTest("Test_TransitionFailureIsFailClosed");
    Configure();
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    g_manager.initialized = false;
    transition.admittedSourceGeneration = 10U;
    transition.transitionEpoch = 2U;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &g_private, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_INVALIDATION_FAILED_BLOCKED);
    (void)memset(&output, 0xA5, sizeof(output));
    savedOutput = output;
    CHECK(DomainIf_QueryGetCisOccupant(g_clock.nowMs, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &savedOutput, sizeof(output)) == 0);
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    EndTest(before);
}

static void Test_GenerationTransitionBlocksDelayedOld(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    DomainCisOccupantPriv_Transition_t transition;
    DomainCisOccupantPriv_TransitionReceipt_t receipt;
    int before = g_failures;

    BeginTest("Test_GenerationTransitionBlocksDelayedOld");
    Configure();
    state.presenceQuality.meta.updateSequence = 500U;
    state.countQuality.meta.updateSequence = 500U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    state.sourceGeneration = 1U;
    state.presenceQuality.meta.updateSequence = 0U;
    state.countQuality.meta.updateSequence = 0U;
    state.occupantCount = 1U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    transition.admittedSourceGeneration = 1U;
    transition.transitionEpoch = 2U;
    CHECK(DomainCisOccupantPriv_ApplyBootTransition(
              &g_private, &transition, &receipt) ==
          DOMAIN_CIS_OCCUPANT_PRIV_APPLIED);
    CHECK(DomainIf_QueryGetCisOccupant(g_clock.nowMs, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    state.sourceGeneration = 9U;
    state.presenceQuality.meta.updateSequence = 501U;
    state.countQuality.meta.updateSequence = 501U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_REJECTED);
    CHECK(DomainIf_QueryGetCisOccupant(g_clock.nowMs, &output) == DOMAIN_IF_OK);
    CHECK(output.sourceGeneration == 1U);
    CHECK(output.occupantCount == 1U);
    EndTest(before);
}

static void Test_InvalidInputAndClockFailureAreAtomic(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantManager_t saved;
    DataQuality_t badQuality = (DataQuality_t)99;
    int before = g_failures;

    BeginTest("Test_InvalidInputAndClockFailureAreAtomic");
    Configure();
    saved = g_manager;
    state.occupantCount = 6U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(memcmp(&g_manager, &saved, sizeof(g_manager)) == 0);
    state = NormalState();
    (void)memcpy(&state.presenceQuality.meta.quality,
                 &badQuality, sizeof(badQuality));
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(memcmp(&g_manager, &saved, sizeof(g_manager)) == 0);
    state = NormalState();
    g_clock.available = false;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&g_manager, &saved, sizeof(g_manager)) == 0);
    g_clock.available = true;
    EndTest(before);
}

static void Test_QueryFailureLeavesOutputUnchanged(void)
{
    CisOccupantState_t output;
    CisOccupantState_t saved;
    int before = g_failures;

    BeginTest("Test_QueryFailureLeavesOutputUnchanged");
    (void)memset(&output, 0xA5, sizeof(output));
    saved = output;
    g_manager.initialized = false;
    CHECK(DomainIf_QueryGetCisOccupant(0U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    Configure();
    EndTest(before);
}

static void Test_TimeRolloverAndAgeSaturation(void)
{
    CisOccupantState_t state = NormalState();
    CisOccupantState_t output;
    int before = g_failures;

    BeginTest("Test_TimeRolloverAndAgeSaturation");
    Configure();
    state.presenceQuality.meta.ageMs = UINT32_MAX - 2U;
    state.countQuality.meta.ageMs = 1U;
    g_clock.nowMs = UINT32_MAX - 4U;
    CHECK(DomainIf_RxCisOccupant(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisOccupant(5U, &output) == DOMAIN_IF_OK);
    CHECK(output.presenceQuality.meta.ageMs == UINT32_MAX);
    CHECK(output.countQuality.meta.ageMs == 11U);
    EndTest(before);
}

int main(void)
{
    Test_PreInitializationAndNull();
    Test_ConfigureAndInitialNoData();
    Test_NormalStoreQueryAndNoInventedObserver();
    Test_IndependentQualityAndTtl();
    Test_DuplicateOutOfOrderAndRollover();
    Test_SameSequenceWithdrawalAndRecovery();
    Test_GenerationTransitionBlocksDelayedOld();
    Test_TransitionFailureIsFailClosed();
    Test_InvalidInputAndClockFailureAreAtomic();
    Test_QueryFailureLeavesOutputUnchanged();
    Test_TimeRolloverAndAgeSaturation();

    (void)printf(
        "domain_interface_cis_occupant_binding: %d tests, %d failures\n",
        g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
