#include "vehicle_state_manager.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_SLOT_COUNT (2U)

typedef struct
{
    uint32_t first;
    uint16_t second;
    uint8_t third;
} TestValue_t;

typedef struct
{
    VehicleStateManager_t manager;
    VehicleStateManager_Slot_t slots[TEST_SLOT_COUNT];
    TestValue_t values[TEST_SLOT_COUNT];
    VehicleStateManager_SlotConfig_t configs[TEST_SLOT_COUNT];
} TestFixture_t;

static unsigned int g_failedChecks;
static unsigned int g_executedTests;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            (void)fprintf(stderr, "%s:%d: CHECK failed: %s\n",                \
                          __FILE__, __LINE__, #condition);                       \
            ++g_failedChecks;                                                   \
        }                                                                       \
    } while (0)

#define RUN_TEST(testFunction)                                                  \
    do                                                                          \
    {                                                                           \
        unsigned int failuresBefore = g_failedChecks;                           \
        ++g_executedTests;                                                      \
        testFunction();                                                         \
        (void)printf("%s: %s\n", #testFunction,                               \
                     (failuresBefore == g_failedChecks) ? "PASS" : "FAIL");   \
    } while (0)

static void Fixture_Init(TestFixture_t *fixture)
{
    size_t index;

    (void)memset(fixture, 0xA5, sizeof(*fixture));
    for (index = 0U; index < TEST_SLOT_COUNT; ++index)
    {
        fixture->configs[index].valueStorage = &fixture->values[index];
        fixture->configs[index].valueCapacity = sizeof(fixture->values[index]);
    }

    CHECK(VehicleStateManager_Init(&fixture->manager,
                                   fixture->slots,
                                   fixture->configs,
                                   TEST_SLOT_COUNT) == VEHICLE_STATE_MANAGER_OK);
}

static VehicleStateManager_Update_t MakeUpdate(
    const TestValue_t *value,
    VehicleStateQuality_t quality,
    uint32_t sourceAgeMs,
    uint32_t sequence,
    uint32_t generation,
    uint32_t receivedAtMs)
{
    VehicleStateManager_Update_t update;

    update.value = value;
    update.valueSize = (value != NULL) ? sizeof(*value) : 0U;
    update.quality = quality;
    update.sourceAgeMs = sourceAgeMs;
    update.updateSequence = sequence;
    update.sourceGeneration = generation;
    update.receivedAtMs = receivedAtMs;
    return update;
}

static void Test_InitialNoData(void)
{
    TestFixture_t fixture;
    VehicleStateManager_Snapshot_t snapshot;
    TestValue_t output = { 1U, 2U, 3U };

    Fixture_Init(&fixture);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 100U, 50U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_NO_DATA);
    CHECK(!snapshot.hasObservation);
    CHECK(!snapshot.hasValue);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_NO_DATA);
    CHECK(output.first == 1U);
}

static void Test_NormalStoreAndGet(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { UINT32_C(0x12345678), UINT16_C(0x4321), UINT8_C(7) };
    TestValue_t output = { 0U, 0U, 0U };
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 10U, 5U, 3U, 100U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 120U, 100U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(memcmp(&input, &output, sizeof(input)) == 0);
    CHECK(snapshot.hasObservation);
    CHECK(snapshot.hasValue);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_OK);
    CHECK(snapshot.sourceAgeMs == 10U);
    CHECK(snapshot.localElapsedMs == 20U);
    CHECK(snapshot.effectiveAgeMs == 30U);
    CHECK(snapshot.updateSequence == 5U);
    CHECK(snapshot.sourceGeneration == 3U);
}

static void Test_InvalidArgumentsAndSlot(void)
{
    TestFixture_t fixture;
    VehicleStateManager_t notInitialized = { 0 };
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;
    const TestValue_t input = { 1U, 2U, 3U };

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 0U, 1U, 1U, 0U);

    CHECK(VehicleStateManager_Init(NULL, fixture.slots, fixture.configs,
                                   TEST_SLOT_COUNT) ==
          VEHICLE_STATE_MANAGER_INVALID_ARGUMENT);
    CHECK(VehicleStateManager_Update(NULL, 0U, &update) ==
          VEHICLE_STATE_MANAGER_INVALID_ARGUMENT);
    CHECK(VehicleStateManager_Update(&notInitialized, 0U, &update) ==
          VEHICLE_STATE_MANAGER_NOT_INITIALIZED);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, NULL) ==
          VEHICLE_STATE_MANAGER_INVALID_ARGUMENT);
    CHECK(VehicleStateManager_Update(&fixture.manager, TEST_SLOT_COUNT, &update) ==
          VEHICLE_STATE_MANAGER_INVALID_SLOT);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, TEST_SLOT_COUNT,
                                          0U, 1U, NULL, 0U, &snapshot) ==
          VEHICLE_STATE_MANAGER_INVALID_SLOT);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 0U, 1U,
                                          NULL, 0U, NULL) ==
          VEHICLE_STATE_MANAGER_INVALID_ARGUMENT);
}

static void Test_QualityPreservation(void)
{
    static const VehicleStateQuality_t qualities[] = {
        VEHICLE_STATE_QUALITY_OK,
        VEHICLE_STATE_QUALITY_STALE,
        VEHICLE_STATE_QUALITY_INVALID
    };
    TestFixture_t fixture;
    const TestValue_t input = { 11U, 22U, 33U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;
    size_t index;

    Fixture_Init(&fixture);
    for (index = 0U; index < (sizeof(qualities) / sizeof(qualities[0])); ++index)
    {
        update = MakeUpdate(&input, qualities[index], 5U,
                            (uint32_t)(index + 1U), 1U, 10U);
        CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
              VEHICLE_STATE_MANAGER_OK);
        CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 11U,
                                              100U, &output, sizeof(output),
                                              &snapshot) ==
              VEHICLE_STATE_MANAGER_OK);
        CHECK(snapshot.quality == qualities[index]);
    }

    update = MakeUpdate(NULL, VEHICLE_STATE_QUALITY_NO_DATA, 9U, 4U, 1U, 20U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 21U, 100U,
                                          NULL, 0U, &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.hasObservation);
    CHECK(!snapshot.hasValue);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_NO_DATA);
}

static void Test_AgeAndTtlBoundary(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { 1U, 2U, 3U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 20U, 1U, 1U, 100U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);

    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 129U, 50U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.effectiveAgeMs == 49U);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_OK);

    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 130U, 50U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.effectiveAgeMs == 50U);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_STALE);
}

static void Test_TimeRollover(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { 1U, 2U, 3U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 3U, 1U, 1U,
                        UINT32_MAX - 4U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 5U, 20U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.localElapsedMs == 10U);
    CHECK(snapshot.effectiveAgeMs == 13U);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_OK);
}

static void Test_AgeSaturation(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { 1U, 2U, 3U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, UINT32_MAX - 2U,
                        1U, 1U, 10U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 20U,
                                          UINT32_MAX, &output, sizeof(output),
                                          &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.effectiveAgeMs == UINT32_MAX);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_STALE);
}

static void Test_DuplicateSequence(void)
{
    TestFixture_t fixture;
    const TestValue_t first = { 1U, 2U, 3U };
    const TestValue_t duplicate = { 9U, 9U, 9U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&first, VEHICLE_STATE_QUALITY_OK, 0U, 7U, 4U, 100U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    update = MakeUpdate(&duplicate, VEHICLE_STATE_QUALITY_OK, 0U, 7U, 4U, 200U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_DUPLICATE);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 210U, 1000U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(memcmp(&output, &first, sizeof(first)) == 0);
    CHECK(snapshot.receivedAtMs == 100U);
}

static void Test_OldSequenceRejectedAndWrapAccepted(void)
{
    TestFixture_t fixture;
    const TestValue_t first = { 1U, 2U, 3U };
    const TestValue_t old = { 4U, 5U, 6U };
    const TestValue_t wrapped = { 7U, 8U, 9U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&first, VEHICLE_STATE_QUALITY_OK, 0U, UINT32_MAX,
                        2U, 10U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);

    update = MakeUpdate(&old, VEHICLE_STATE_QUALITY_OK, 0U, UINT32_MAX - 1U,
                        2U, 11U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OUT_OF_ORDER);

    update = MakeUpdate(&wrapped, VEHICLE_STATE_QUALITY_OK, 0U, 0U, 2U, 12U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 12U, 100U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(memcmp(&output, &wrapped, sizeof(wrapped)) == 0);
    CHECK(snapshot.updateSequence == 0U);
}

static void Test_SourceGenerationChange(void)
{
    TestFixture_t fixture;
    const TestValue_t first = { 1U, 2U, 3U };
    const TestValue_t next = { 4U, 5U, 6U };
    TestValue_t output;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&first, VEHICLE_STATE_QUALITY_OK, 0U, 100U, 1U, 10U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);

    update = MakeUpdate(&next, VEHICLE_STATE_QUALITY_INVALID, 0U, 1U, 2U, 20U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 20U, 100U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.sourceGenerationChanged);
    CHECK(snapshot.sourceGeneration == 2U);
    CHECK(snapshot.updateSequence == 1U);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_INVALID);
    CHECK(memcmp(&output, &next, sizeof(next)) == 0);
}

static void Test_GetterDoesNotMutateState(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { 1U, 2U, 3U };
    TestValue_t output;
    TestValue_t storedBefore;
    VehicleStateManager_Slot_t slotBefore;
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 20U, 2U, 3U, 100U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    slotBefore = fixture.slots[0];
    storedBefore = fixture.values[0];

    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 200U, 50U,
                                          &output, sizeof(output), &snapshot) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(snapshot.quality == VEHICLE_STATE_QUALITY_STALE);
    CHECK(memcmp(&slotBefore, &fixture.slots[0], sizeof(slotBefore)) == 0);
    CHECK(memcmp(&storedBefore, &fixture.values[0], sizeof(storedBefore)) == 0);
}

static void Test_CapacityAndInvalidQuality(void)
{
    TestFixture_t fixture;
    const TestValue_t input = { 1U, 2U, 3U };
    VehicleStateManager_Update_t update;
    VehicleStateManager_Snapshot_t snapshot;
    uint8_t tooSmall[1];

    Fixture_Init(&fixture);
    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 0U, 1U, 1U, 0U);
    update.valueSize = sizeof(input) + 1U;
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED);

    update = MakeUpdate(&input, (VehicleStateQuality_t)99, 0U, 1U, 1U, 0U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_INVALID_ARGUMENT);

    update = MakeUpdate(&input, VEHICLE_STATE_QUALITY_OK, 0U, 1U, 1U, 0U);
    CHECK(VehicleStateManager_Update(&fixture.manager, 0U, &update) ==
          VEHICLE_STATE_MANAGER_OK);
    CHECK(VehicleStateManager_GetSnapshot(&fixture.manager, 0U, 0U, 10U,
                                          tooSmall, sizeof(tooSmall), &snapshot) ==
          VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED);
}

int main(void)
{
    RUN_TEST(Test_InitialNoData);
    RUN_TEST(Test_NormalStoreAndGet);
    RUN_TEST(Test_InvalidArgumentsAndSlot);
    RUN_TEST(Test_QualityPreservation);
    RUN_TEST(Test_AgeAndTtlBoundary);
    RUN_TEST(Test_TimeRollover);
    RUN_TEST(Test_AgeSaturation);
    RUN_TEST(Test_DuplicateSequence);
    RUN_TEST(Test_OldSequenceRejectedAndWrapAccepted);
    RUN_TEST(Test_SourceGenerationChange);
    RUN_TEST(Test_GetterDoesNotMutateState);
    RUN_TEST(Test_CapacityAndInvalidQuality);

    (void)printf("Executed %u tests, %u failed checks.\n",
                 g_executedTests, g_failedChecks);
    return (g_failedChecks == 0U) ? 0 : 1;
}
