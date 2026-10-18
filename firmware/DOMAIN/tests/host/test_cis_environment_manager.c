#include "cis_environment_manager.h"

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

    state.cabinTemperature = 2350;
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 10U, 101U);
    state.cabinHumidity = 4567U;
    state.humidityQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 20U, 202U);
    state.cabinIlluminance = 123456U;
    state.illuminanceQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_NONE,
        DATA_QUALITY_OK, 30U, 303U);
    return state;
}

static void BeginTest(const char *name)
{
    ++g_tests;
    (void)printf("%s: ", name);
}

static void EndTest(int failuresBefore)
{
    (void)printf((g_failures == failuresBefore) ? "PASS\n" : "FAIL\n");
}

static void Test_InitialNoData(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    int before = g_failures;

    BeginTest("Test_InitialNoData");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 4000U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(!snapshot.temperatureTiming.hasObservation);
    CHECK(!snapshot.humidityTiming.hasObservation);
    CHECK(!snapshot.illuminanceTiming.hasObservation);
    CHECK(snapshot.original.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.original.humidityQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.original.illuminanceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_AggregateStoreAndIndependentValues(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_AggregateStoreAndIndependentValues");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 1000U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 1000U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.cabinTemperature == 2350);
    CHECK(snapshot.original.cabinHumidity == 4567U);
    CHECK(snapshot.original.cabinIlluminance == 123456U);
    CHECK(snapshot.temperatureTiming.hasObservation);
    CHECK(snapshot.humidityTiming.hasObservation);
    CHECK(snapshot.illuminanceTiming.hasObservation);
    EndTest(before);
}

static void Test_ValueQualityAndSequencePreserved(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_ValueQualityAndSequencePreserved");
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_SENSOR_FAULT,
        DATA_QUALITY_INVALID, 77U, 0x12345678U);
    state.humidityQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_STALE,
        DATA_QUALITY_STALE, 88U, 0x87654321U);
    state.illuminanceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 99U, UINT32_MAX);
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 50U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 50U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.cabinTemperature == state.cabinTemperature);
    CHECK(snapshot.original.cabinHumidity == state.cabinHumidity);
    CHECK(snapshot.original.cabinIlluminance == state.cabinIlluminance);
    CHECK(snapshot.original.temperatureQuality.validity ==
          state.temperatureQuality.validity);
    CHECK(snapshot.original.temperatureQuality.reason ==
          state.temperatureQuality.reason);
    CHECK(snapshot.original.temperatureQuality.meta.quality ==
          state.temperatureQuality.meta.quality);
    CHECK(snapshot.original.temperatureQuality.meta.ageMs ==
          state.temperatureQuality.meta.ageMs);
    CHECK(snapshot.original.humidityQuality.validity ==
          state.humidityQuality.validity);
    CHECK(snapshot.original.humidityQuality.reason ==
          state.humidityQuality.reason);
    CHECK(snapshot.original.humidityQuality.meta.quality ==
          state.humidityQuality.meta.quality);
    CHECK(snapshot.original.humidityQuality.meta.ageMs ==
          state.humidityQuality.meta.ageMs);
    CHECK(snapshot.original.illuminanceQuality.validity ==
          state.illuminanceQuality.validity);
    CHECK(snapshot.original.illuminanceQuality.reason ==
          state.illuminanceQuality.reason);
    CHECK(snapshot.original.illuminanceQuality.meta.quality ==
          state.illuminanceQuality.meta.quality);
    CHECK(snapshot.original.illuminanceQuality.meta.ageMs ==
          state.illuminanceQuality.meta.ageMs);
    CHECK(snapshot.effective.temperatureQuality.meta.updateSequence ==
          0x12345678U);
    CHECK(snapshot.effective.humidityQuality.meta.updateSequence ==
          0x87654321U);
    CHECK(snapshot.effective.illuminanceQuality.meta.updateSequence ==
          UINT32_MAX);
    EndTest(before);
}

static void Test_AggregateFailureIsAtomic(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_t beforeManager;
    CisCabinEnvironmentState_t state = NormalState();
    DataQuality_t invalidQuality;
    int before = g_failures;

    BeginTest("Test_AggregateFailureIsAtomic");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    beforeManager = manager;
    invalidQuality = (DataQuality_t)99;
    (void)memcpy(&state.humidityQuality.meta.quality,
                 &invalidQuality, sizeof(invalidQuality));
    CHECK(CisEnvironmentManager_Store(&manager, &state, 200U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &beforeManager, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_MixedExplicitQualityIsIndependent(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_MixedExplicitQualityIsIndependent");
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_SENSOR_FAULT,
        DATA_QUALITY_INVALID, 1U, 1U);
    state.humidityQuality = Quality(
        VALUE_VALIDITY_VALID, QUALITY_REASON_STALE,
        DATA_QUALITY_STALE, 2U, 2U);
    state.illuminanceQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 3U, 3U);
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 10U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 20U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_INVALID);
    CHECK(snapshot.effective.humidityQuality.meta.quality ==
          DATA_QUALITY_STALE);
    CHECK(snapshot.effective.illuminanceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_SourceAgeAndElapsed(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_SourceAgeAndElapsed");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 150U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.temperatureQuality.meta.ageMs == 10U);
    CHECK(snapshot.temperatureTiming.localElapsedMs == 50U);
    CHECK(snapshot.temperatureTiming.effectiveAgeMs == 60U);
    CHECK(snapshot.effective.temperatureQuality.meta.ageMs == 60U);
    EndTest(before);
}

static void Test_TemperatureTtlBoundary(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_TemperatureTtlBoundary");
    state.temperatureQuality.meta.ageMs = 0U;
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2599U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.temperatureQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2600U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_STALE);
    EndTest(before);
}

static void Test_HumidityTtlBoundary(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_HumidityTtlBoundary");
    state.humidityQuality.meta.ageMs = 0U;
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2599U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.humidityQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2600U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.humidityQuality.meta.quality ==
          DATA_QUALITY_STALE);
    EndTest(before);
}

static void Test_IlluminanceHasNoInventedTtl(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_IlluminanceHasNoInventedTtl");
    state.illuminanceQuality.meta.ageMs = 0U;
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 0U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 100000U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.effective.illuminanceQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(snapshot.effective.illuminanceQuality.meta.ageMs == 100000U);
    EndTest(before);
}

static void Test_NetworkAdmittedReverseSequenceIsPreserved(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_NetworkAdmittedReverseSequenceIsPreserved");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    state.temperatureQuality.meta.updateSequence = 900U;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 10U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    state.temperatureQuality.meta.updateSequence = 2U;
    state.cabinTemperature = 2400;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 20U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 20U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.cabinTemperature == 2400);
    CHECK(snapshot.original.temperatureQuality.meta.updateSequence == 2U);
    EndTest(before);
}

static void Test_NoDataDoesNotCreateBootState(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_NoDataDoesNotCreateBootState");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 10U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    state.temperatureQuality = Quality(
        VALUE_VALIDITY_INVALID, QUALITY_REASON_NO_DATA,
        DATA_QUALITY_NO_DATA, 300U, 0U);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 20U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 20U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.original.temperatureQuality.meta.updateSequence == 0U);
    CHECK(snapshot.temperatureTiming.hasObservation);
    EndTest(before);
}

static void Test_SequenceRolloverIsPreserved(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_SequenceRolloverIsPreserved");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    state.humidityQuality.meta.updateSequence = UINT32_MAX;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 1U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    state.humidityQuality.meta.updateSequence = 0U;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 2U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 2U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.original.humidityQuality.meta.updateSequence == 0U);
    EndTest(before);
}

static void Test_TimeRolloverAndAgeSaturation(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_TimeRolloverAndAgeSaturation");
    state.temperatureQuality.meta.ageMs = UINT32_MAX - 2U;
    state.illuminanceQuality.meta.ageMs = 1U;
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, UINT32_MAX - 4U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 5U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.temperatureTiming.localElapsedMs == 10U);
    CHECK(snapshot.temperatureTiming.effectiveAgeMs == UINT32_MAX);
    CHECK(snapshot.illuminanceTiming.effectiveAgeMs == 11U);
    EndTest(before);
}

static void Test_InvalidEnumsAndHumidityRange(void)
{
    CisEnvironmentManager_t manager;
    CisCabinEnvironmentState_t state = NormalState();
    ValueValidity_t invalidValidity = (ValueValidity_t)7;
    int before = g_failures;

    BeginTest("Test_InvalidEnumsAndHumidityRange");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    (void)memcpy(&state.temperatureQuality.validity,
                 &invalidValidity, sizeof(invalidValidity));
    CHECK(CisEnvironmentManager_Store(&manager, &state, 0U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    state = NormalState();
    state.cabinHumidity = 10001U;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 0U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    EndTest(before);
}

static void Test_NullAndNotInitializedLeaveOutputUnchanged(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t output;
    CisEnvironmentManager_Snapshot_t sentinel;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_NullAndNotInitializedLeaveOutputUnchanged");
    (void)memset(&manager, 0, sizeof(manager));
    (void)memset(&output, 0xA5, sizeof(output));
    sentinel = output;
    CHECK(CisEnvironmentManager_Init(NULL) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(CisEnvironmentManager_Store(NULL, &state, 0U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(CisEnvironmentManager_Store(&manager, NULL, 0U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 0U, &output) ==
          CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED);
    CHECK(memcmp(&output, &sentinel, sizeof(output)) == 0);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 0U, NULL) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    EndTest(before);
}

static void Test_GetterIsReadOnly(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_t saved;
    CisEnvironmentManager_Snapshot_t first;
    CisEnvironmentManager_Snapshot_t second;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_GetterIsReadOnly");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    saved = manager;
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 200U, &first) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 300U, &second) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    CHECK(first.original.temperatureQuality.meta.ageMs ==
          second.original.temperatureQuality.meta.ageMs);
    CHECK(first.original.temperatureQuality.meta.updateSequence ==
          second.original.temperatureQuality.meta.updateSequence);
    EndTest(before);
}

static void Test_ReinitializationInvalidatesOldOk(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_ReinitializationInvalidatesOldOk");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 100U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(!snapshot.temperatureTiming.hasObservation);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.humidityQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.illuminanceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    EndTest(before);
}

static void Test_InvalidatePreservesOriginalAndBlocksEffective(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_InvalidatePreservesOriginalAndBlocksEffective");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, UINT32_MAX - 5U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Invalidate(&manager) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 4U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(snapshot.invalidated);
    CHECK(snapshot.original.cabinTemperature == state.cabinTemperature);
    CHECK(snapshot.original.cabinHumidity == state.cabinHumidity);
    CHECK(snapshot.original.cabinIlluminance == state.cabinIlluminance);
    CHECK(snapshot.original.temperatureQuality.meta.updateSequence == 101U);
    CHECK(snapshot.original.humidityQuality.meta.updateSequence == 202U);
    CHECK(snapshot.original.illuminanceQuality.meta.updateSequence == 303U);
    CHECK(snapshot.temperatureTiming.localElapsedMs == 10U);
    CHECK(snapshot.effective.cabinTemperature == 0);
    CHECK(snapshot.effective.cabinHumidity == 0U);
    CHECK(snapshot.effective.cabinIlluminance == 0U);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.humidityQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.illuminanceQuality.meta.quality ==
          DATA_QUALITY_NO_DATA);
    CHECK(snapshot.effective.temperatureQuality.meta.updateSequence == 0U);
    EndTest(before);
}

static void Test_InvalidateFailureAndRejectedStoreAreAtomic(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_t saved;
    CisCabinEnvironmentState_t invalid = NormalState();
    int before = g_failures;

    BeginTest("Test_InvalidateFailureAndRejectedStoreAreAtomic");
    (void)memset(&manager, 0, sizeof(manager));
    saved = manager;
    CHECK(CisEnvironmentManager_Invalidate(NULL) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(CisEnvironmentManager_Invalidate(&manager) ==
          CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);

    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Invalidate(&manager) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    saved = manager;
    invalid.humidityQuality.validity = (ValueValidity_t)99;
    CHECK(CisEnvironmentManager_Store(&manager, &invalid, 42U) ==
          CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    EndTest(before);
}

static void Test_SuccessfulAdmittedStoreClearsInvalidation(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_Snapshot_t snapshot;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_SuccessfulAdmittedStoreClearsInvalidation");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 100U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Invalidate(&manager) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    state.temperatureQuality.meta.updateSequence = 1U;
    state.humidityQuality.meta.updateSequence = 0U;
    state.illuminanceQuality.meta.updateSequence = UINT32_MAX;
    CHECK(CisEnvironmentManager_Store(&manager, &state, 200U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 200U, &snapshot) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(!snapshot.invalidated);
    CHECK(snapshot.effective.temperatureQuality.meta.quality ==
          DATA_QUALITY_OK);
    CHECK(snapshot.original.temperatureQuality.meta.updateSequence == 1U);
    CHECK(snapshot.original.humidityQuality.meta.updateSequence == 0U);
    CHECK(snapshot.original.illuminanceQuality.meta.updateSequence ==
          UINT32_MAX);
    EndTest(before);
}

static void Test_InvalidatedGetterIsReadOnly(void)
{
    CisEnvironmentManager_t manager;
    CisEnvironmentManager_t saved;
    CisEnvironmentManager_Snapshot_t first;
    CisEnvironmentManager_Snapshot_t second;
    CisCabinEnvironmentState_t state = NormalState();
    int before = g_failures;

    BeginTest("Test_InvalidatedGetterIsReadOnly");
    CHECK(CisEnvironmentManager_Init(&manager) == CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Store(&manager, &state, 10U) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_Invalidate(&manager) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    saved = manager;
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 20U, &first) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(CisEnvironmentManager_GetSnapshot(&manager, 30U, &second) ==
          CIS_ENVIRONMENT_MANAGER_OK);
    CHECK(memcmp(&manager, &saved, sizeof(manager)) == 0);
    CHECK(first.invalidated && second.invalidated);
    CHECK(first.original.temperatureQuality.meta.updateSequence ==
          second.original.temperatureQuality.meta.updateSequence);
    EndTest(before);
}

int main(void)
{
    Test_InitialNoData();
    Test_AggregateStoreAndIndependentValues();
    Test_ValueQualityAndSequencePreserved();
    Test_AggregateFailureIsAtomic();
    Test_MixedExplicitQualityIsIndependent();
    Test_SourceAgeAndElapsed();
    Test_TemperatureTtlBoundary();
    Test_HumidityTtlBoundary();
    Test_IlluminanceHasNoInventedTtl();
    Test_NetworkAdmittedReverseSequenceIsPreserved();
    Test_NoDataDoesNotCreateBootState();
    Test_SequenceRolloverIsPreserved();
    Test_TimeRolloverAndAgeSaturation();
    Test_InvalidEnumsAndHumidityRange();
    Test_NullAndNotInitializedLeaveOutputUnchanged();
    Test_GetterIsReadOnly();
    Test_ReinitializationInvalidatesOldOk();
    Test_InvalidatePreservesOriginalAndBlocksEffective();
    Test_InvalidateFailureAndRejectedStoreAreAtomic();
    Test_SuccessfulAdmittedStoreClearsInvalidation();
    Test_InvalidatedGetterIsReadOnly();

    (void)printf("cis_environment_manager: %d tests, %d failed checks\n",
                 g_tests, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
