#include "domain_interface_cis_environment_binding.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_tests;

static CisEnvironmentManager_t g_manager;
static DomainCisEnvironmentPriv_t g_privateContext;

typedef struct
{
    uint32_t nowMs;
    bool available;
} TestClock_t;

static TestClock_t g_clock;

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

static void EndTest(int failuresBefore)
{
    (void)printf((g_failures == failuresBefore) ? "PASS\n" : "FAIL\n");
}

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
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

static CisCabinEnvironmentState_t NormalState(void)
{
    CisCabinEnvironmentState_t state;

    state.cabinTemperature = 2345;
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 0U, 101U);
    state.cabinHumidity = 4567U;
    state.humidityQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 0U, 202U);
    state.cabinIlluminance = 123456U;
    state.illuminanceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 0U, 303U);
    return state;
}

static void FillPattern(void *object, size_t size, unsigned char pattern)
{
    (void)memset(object, (int)pattern, size);
}

static void InitializeBinding(void)
{
    g_clock.nowMs = 0U;
    g_clock.available = true;
    CHECK(DomainIfCisEnvironmentBinding_Initialize(
              &g_manager, &g_privateContext,
              TestClock_GetNow, &g_clock) == DOMAIN_IF_OK);
}

static DomainCisEnvironmentPriv_Status_t ApplyTransition(
    uint64_t epoch,
    uint32_t bootId,
    DomainCisEnvironmentPriv_TransitionReceipt_t *outReceipt)
{
    DomainCisEnvironmentPriv_Transition_t transition;

    transition.transitionEpoch = epoch;
    transition.admittedCisBootId = bootId;
    return DomainCisEnvironmentPriv_ApplyBootTransition(
        &g_privateContext, &transition, outReceipt);
}

static void Test_PreInitializationAndNull(void)
{
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_PreInitializationAndNull");
    CHECK(DomainIf_RxCisCabinEnvironment(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_QueryGetCisEnvironment(0U, NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_RxCisCabinEnvironment(&state) ==
          DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_QueryGetCisEnvironment(0U, &state) ==
          DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIfCisEnvironmentBinding_Initialize(
              &g_manager, &g_privateContext,
              TestClock_GetNow, &g_clock) == DOMAIN_IF_NOT_INITIALIZED);
    EndTest(before);
}

static void Test_UnconfiguredAndInitialization(void)
{
    CisCabinEnvironmentState_t output;
    CisCabinEnvironmentState_t saved;
    int before = g_failures;

    BeginTest("Test_UnconfiguredAndInitialization");
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    FillPattern(&output, sizeof(output), 0xA5U);
    saved = output;
    CHECK(DomainIf_QueryGetCisEnvironment(0U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &saved, sizeof(output)) == 0);
    CHECK(DomainIfCisEnvironmentBinding_Initialize(
              NULL, &g_privateContext,
              TestClock_GetNow, &g_clock) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIfCisEnvironmentBinding_Initialize(
              &g_manager, NULL,
              TestClock_GetNow, &g_clock) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIfCisEnvironmentBinding_Initialize(
              &g_manager, &g_privateContext,
              NULL, &g_clock) == DOMAIN_IF_INVALID_ARGUMENT);
    InitializeBinding();
    CHECK(g_privateContext.environmentManager == &g_manager);
    EndTest(before);
}

static void Test_InitialNoDataQuery(void)
{
    CisCabinEnvironmentState_t output;
    CisEnvironmentManager_t savedManager = g_manager;
    int before = g_failures;

    BeginTest("Test_InitialNoDataQuery");
    CHECK(DomainIf_QueryGetCisEnvironment(123U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.humidityQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.illuminanceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(memcmp(&g_manager, &savedManager, sizeof(g_manager)) == 0);
    EndTest(before);
}

static void Test_StoreRejectedBeforeBootAdmission(void)
{
    CisCabinEnvironmentState_t state = NormalState();
    CisEnvironmentManager_t savedManager = g_manager;
    int before = g_failures;

    BeginTest("Test_StoreRejectedBeforeBootAdmission");
    g_clock.nowMs = 100U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_REJECTED);
    CHECK(memcmp(&g_manager, &savedManager, sizeof(g_manager)) == 0);
    EndTest(before);
}

static void Test_FirstTransitionStoreAndQuery(void)
{
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_FirstTransitionStoreAndQuery");
    CHECK(ApplyTransition(UINT64_C(1), 11U, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(receipt.transitionEpoch == UINT64_C(1));
    g_clock.nowMs = 1000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisEnvironment(1000U, &output) == DOMAIN_IF_OK);
    CHECK(output.cabinTemperature == state.cabinTemperature);
    CHECK(output.cabinHumidity == state.cabinHumidity);
    CHECK(output.cabinIlluminance == state.cabinIlluminance);
    CHECK(output.temperatureQuality.meta.updateSequence == 101U);
    CHECK(output.humidityQuality.meta.updateSequence == 202U);
    CHECK(output.illuminanceQuality.meta.updateSequence == 303U);
    EndTest(before);
}

static void Test_TtlBoundariesAndIlluminanceOpen(void)
{
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_TtlBoundariesAndIlluminanceOpen");
    CHECK(DomainIf_QueryGetCisEnvironment(3499U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(output.humidityQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(output.illuminanceQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(output.temperatureQuality.meta.ageMs == 2499U);

    CHECK(DomainIf_QueryGetCisEnvironment(3500U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_STALE);
    CHECK(output.humidityQuality.meta.quality == DATA_QUALITY_STALE);
    CHECK(output.illuminanceQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(output.illuminanceQuality.meta.ageMs == 2500U);
    EndTest(before);
}

static void Test_IndependentQualitySequenceAndAge(void)
{
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_IndependentQualitySequenceAndAge");
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_SENSOR_FAULT,
        DATA_QUALITY_INVALID, 7U, 9U);
    state.humidityQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_STALE,
        DATA_QUALITY_STALE, 8U, 10U);
    state.illuminanceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 9U, 11U);
    g_clock.nowMs = 4000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisEnvironment(4005U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_INVALID);
    CHECK(output.temperatureQuality.meta.ageMs == 12U);
    CHECK(output.temperatureQuality.meta.updateSequence == 9U);
    CHECK(output.humidityQuality.meta.quality == DATA_QUALITY_STALE);
    CHECK(output.humidityQuality.meta.ageMs == 13U);
    CHECK(output.humidityQuality.meta.updateSequence == 10U);
    CHECK(output.illuminanceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.illuminanceQuality.meta.ageMs == 14U);
    CHECK(output.illuminanceQuality.meta.updateSequence == 11U);
    EndTest(before);
}

static void Test_OrdinaryNoDataIsStoredWithoutBootTransition(void)
{
    DomainCisEnvironmentPriv_Snapshot_t beforePrivate;
    DomainCisEnvironmentPriv_Snapshot_t afterPrivate;
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_OrdinaryNoDataIsStoredWithoutBootTransition");
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 15U, 12U);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(
              &g_privateContext, &beforePrivate) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    g_clock.nowMs = 5000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(DomainCisEnvironmentPriv_GetSnapshot(
              &g_privateContext, &afterPrivate) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_OK);
    CHECK(afterPrivate.currentTransition.transitionEpoch ==
          beforePrivate.currentTransition.transitionEpoch);
    CHECK(afterPrivate.domainBarrierRevision ==
          beforePrivate.domainBarrierRevision);
    CHECK(DomainIf_QueryGetCisEnvironment(5000U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_TransitionInvalidatesAndDuplicateDoesNotReinvalidate(void)
{
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    DomainCisEnvironmentPriv_TransitionReceipt_t duplicateReceipt;
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_TransitionInvalidatesAndDuplicateDoesNotReinvalidate");
    CHECK(ApplyTransition(UINT64_C(2), 22U, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    CHECK(DomainIf_QueryGetCisEnvironment(5100U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.humidityQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(output.illuminanceQuality.meta.quality == DATA_QUALITY_NO_DATA);

    g_clock.nowMs = 5200U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(ApplyTransition(UINT64_C(2), 22U, &duplicateReceipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_DUPLICATE_APPLIED);
    CHECK(memcmp(&receipt, &duplicateReceipt, sizeof(receipt)) == 0);
    CHECK(DomainIf_QueryGetCisEnvironment(5200U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_OK);
    EndTest(before);
}

static void Test_SequentialBootAndSequenceReset(void)
{
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_SequentialBootAndSequenceReset");
    CHECK(ApplyTransition(UINT64_C(3), 33U, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    state.temperatureQuality.meta.updateSequence = 0U;
    state.humidityQuality.meta.updateSequence = 0U;
    state.illuminanceQuality.meta.updateSequence = 0U;
    g_clock.nowMs = 6000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisEnvironment(6000U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.updateSequence == 0U);
    CHECK(output.humidityQuality.meta.updateSequence == 0U);
    CHECK(output.illuminanceQuality.meta.updateSequence == 0U);
    EndTest(before);
}

static void Test_TimeAndSequenceRollover(void)
{
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_TimeAndSequenceRollover");
    state.temperatureQuality.meta.ageMs = 3U;
    state.temperatureQuality.meta.updateSequence = UINT32_MAX;
    state.humidityQuality.meta.updateSequence = 0U;
    g_clock.nowMs = UINT32_MAX - 5U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);
    CHECK(DomainIf_QueryGetCisEnvironment(3U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.ageMs == 12U);
    CHECK(output.temperatureQuality.meta.updateSequence == UINT32_MAX);
    CHECK(output.humidityQuality.meta.updateSequence == 0U);
    EndTest(before);
}

static void Test_FailedRxIsAtomicAndOutputUnchanged(void)
{
    CisCabinEnvironmentState_t invalid = NormalState();
    CisCabinEnvironmentState_t output;
    CisCabinEnvironmentState_t savedOutput;
    CisEnvironmentManager_t savedManager = g_manager;
    int before = g_failures;

    BeginTest("Test_FailedRxIsAtomicAndOutputUnchanged");
    invalid.humidityQuality.meta.quality = (DataQuality_t)99;
    g_clock.nowMs = 7000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&invalid) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(memcmp(&g_manager, &savedManager, sizeof(g_manager)) == 0);

    g_clock.available = false;
    CHECK(DomainIf_RxCisCabinEnvironment(&invalid) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&g_manager, &savedManager, sizeof(g_manager)) == 0);
    g_clock.available = true;

    FillPattern(&output, sizeof(output), 0x5AU);
    savedOutput = output;
    g_manager.initialized = false;
    CHECK(DomainIf_QueryGetCisEnvironment(7000U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &savedOutput, sizeof(output)) == 0);
    g_manager = savedManager;
    EndTest(before);
}

static void Test_FaultBlockedIsFailClosed(void)
{
    DomainCisEnvironmentPriv_TransitionReceipt_t receipt;
    CisCabinEnvironmentState_t state = NormalState();
    CisCabinEnvironmentState_t output;
    CisCabinEnvironmentState_t savedOutput;
    int before = g_failures;

    BeginTest("Test_FaultBlockedIsFailClosed");
    InitializeBinding();
    CHECK(ApplyTransition(UINT64_C(1), 101U, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_APPLIED);
    g_clock.nowMs = 8000U;
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_OK);

    g_manager.initialized = false;
    CHECK(ApplyTransition(UINT64_C(2), 102U, &receipt) ==
          DOMAIN_CIS_ENVIRONMENT_PRIV_INVALIDATION_FAILED_BLOCKED);
    g_manager.initialized = true; /* fault injection recovery, gate stays shut */
    CHECK(DomainIf_RxCisCabinEnvironment(&state) == DOMAIN_IF_REJECTED);
    FillPattern(&output, sizeof(output), 0x3CU);
    savedOutput = output;
    CHECK(DomainIf_QueryGetCisEnvironment(8000U, &output) ==
          DOMAIN_IF_NOT_AVAILABLE);
    CHECK(memcmp(&output, &savedOutput, sizeof(output)) == 0);
    EndTest(before);
}

static void Test_ReinitializationReturnsToNoData(void)
{
    CisCabinEnvironmentState_t output;
    int before = g_failures;

    BeginTest("Test_ReinitializationReturnsToNoData");
    InitializeBinding();
    CHECK(DomainIf_QueryGetCisEnvironment(9000U, &output) == DOMAIN_IF_OK);
    CHECK(output.temperatureQuality.meta.quality == DATA_QUALITY_NO_DATA);
    CHECK(!g_privateContext.hasAppliedTransition);
    CHECK(!g_privateContext.newDataAdmissionAllowed);
    EndTest(before);
}

int main(void)
{
    Test_PreInitializationAndNull();
    Test_UnconfiguredAndInitialization();
    Test_InitialNoDataQuery();
    Test_StoreRejectedBeforeBootAdmission();
    Test_FirstTransitionStoreAndQuery();
    Test_TtlBoundariesAndIlluminanceOpen();
    Test_IndependentQualitySequenceAndAge();
    Test_OrdinaryNoDataIsStoredWithoutBootTransition();
    Test_TransitionInvalidatesAndDuplicateDoesNotReinvalidate();
    Test_SequentialBootAndSequenceReset();
    Test_TimeAndSequenceRollover();
    Test_FailedRxIsAtomicAndOutputUnchanged();
    Test_FaultBlockedIsFailClosed();
    Test_ReinitializationReturnsToNoData();

    (void)printf(
        "domain_interface_cis_environment_binding: %d tests, %d failed checks\n",
        g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
