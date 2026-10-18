#include "command_manager.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_CAPACITY (4U)

typedef struct
{
    CommandManager_t manager;
    CommandManager_Slot_t slots[TEST_CAPACITY];
    RequestManager_Request_t request;
    RequestManager_Decision_t decision;
    CommandManager_CommandSpec_t spec;
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

static void Fixture_InitWithFirstId(
    TestFixture_t *fixture,
    CommandManager_CommandId_t firstCommandId)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    CHECK(CommandManager_Init(&fixture->manager, fixture->slots,
                              TEST_CAPACITY, firstCommandId) ==
          COMMAND_MANAGER_OK);

    fixture->request.deviceContextId = UINT32_C(17);
    fixture->request.sessionId = UINT64_C(0x1020304050607080);
    fixture->request.kind = REQUEST_MANAGER_KIND_DOOR;
    fixture->request.operation = REQUEST_MANAGER_OPERATION_DOOR_LOCK;
    fixture->request.quality = VEHICLE_STATE_QUALITY_OK;

    fixture->decision.verdict = REQUEST_MANAGER_VERDICT_ALLOW;
    fixture->decision.reason = REQUEST_MANAGER_REASON_NONE;
    fixture->decision.action = REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT;

    fixture->spec.kind = COMMAND_MANAGER_KIND_DOOR;
    fixture->spec.target.door.target = COMMAND_MANAGER_DOOR_TARGET_LOCK;
}

static void Fixture_Init(TestFixture_t *fixture)
{
    Fixture_InitWithFirstId(fixture, 1U);
}

static CommandManager_Status_t Create(
    TestFixture_t *fixture,
    CommandManager_RequestId_t requestId,
    uint32_t createdAtMs,
    CommandManager_Intent_t *outIntent)
{
    return CommandManager_CreateIntent(&fixture->manager,
                                       &fixture->request,
                                       requestId,
                                       &fixture->decision,
                                       &fixture->spec,
                                       createdAtMs,
                                       outIntent);
}

static void Test_DoorIntentCreation(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    CHECK(Create(&fixture, UINT32_C(9001), 123U, &intent) == COMMAND_MANAGER_OK);
    CHECK(intent.commandId == 1U);
    CHECK(intent.commandId != 0U);
    CHECK(intent.decisionSequence == 1U);
    CHECK(intent.targetEcu == COMMAND_MANAGER_ECU_BCM);
    CHECK(intent.kind == COMMAND_MANAGER_KIND_DOOR);
    CHECK(intent.target.door.target == COMMAND_MANAGER_DOOR_TARGET_LOCK);
    CHECK(intent.createdAtMs == 123U);
    CHECK(intent.request.deviceContextId == fixture.request.deviceContextId);
    CHECK(intent.request.sessionId == fixture.request.sessionId);
    CHECK(intent.request.requestId == UINT32_C(9001));
    CHECK(intent.submissionState == COMMAND_MANAGER_SUBMISSION_NOT_SUBMITTED);
    CHECK(!intent.submissionAttempted);
}

static void Test_ClimateAndInteriorLightIntents(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t climate;
    CommandManager_Intent_t light;

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL;
    fixture.decision.action = REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT;
    fixture.spec.kind = COMMAND_MANAGER_KIND_CLIMATE;
    fixture.spec.target.climate.fanTarget = REQUEST_MANAGER_FAN_LEVEL_HIGH;
    fixture.spec.target.climate.thermalDirection = COMMAND_MANAGER_THERMAL_COOL;
    fixture.spec.target.climate.thermalTargetLevelPercent = 75U;
    CHECK(Create(&fixture, 1U, 10U, &climate) == COMMAND_MANAGER_OK);
    CHECK(climate.kind == COMMAND_MANAGER_KIND_CLIMATE);
    CHECK(climate.target.climate.fanTarget == REQUEST_MANAGER_FAN_LEVEL_HIGH);
    CHECK(climate.target.climate.thermalDirection == COMMAND_MANAGER_THERMAL_COOL);
    CHECK(climate.target.climate.thermalTargetLevelPercent == 75U);

    fixture.request.kind = REQUEST_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_COLOR;
    fixture.decision.action = REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT;
    fixture.spec.kind = COMMAND_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.spec.target.interiorLight.type = COMMAND_MANAGER_LIGHT_NORMAL;
    fixture.spec.target.interiorLight.levelPercent = 60U;
    fixture.spec.target.interiorLight.color.red = 10U;
    fixture.spec.target.interiorLight.color.green = 20U;
    fixture.spec.target.interiorLight.color.blue = 30U;
    CHECK(Create(&fixture, 2U, 20U, &light) == COMMAND_MANAGER_OK);
    CHECK(light.kind == COMMAND_MANAGER_KIND_INTERIOR_LIGHT);
    CHECK(light.target.interiorLight.type == COMMAND_MANAGER_LIGHT_NORMAL);
    CHECK(light.target.interiorLight.levelPercent == 60U);
    CHECK(light.target.interiorLight.color.red == 10U);
    CHECK(light.target.interiorLight.color.green == 20U);
    CHECK(light.target.interiorLight.color.blue == 30U);
}

static void Test_NonAllowedDecisionBlocked(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    fixture.decision.verdict = REQUEST_MANAGER_VERDICT_REJECT;
    CHECK(Create(&fixture, 1U, 0U, &intent) ==
          COMMAND_MANAGER_DECISION_NOT_ALLOWED);
    CHECK(fixture.manager.activeCount == 0U);

    fixture.decision.verdict = REQUEST_MANAGER_VERDICT_UNAVAILABLE;
    CHECK(Create(&fixture, 2U, 0U, &intent) ==
          COMMAND_MANAGER_DECISION_NOT_ALLOWED);
    CHECK(fixture.manager.activeCount == 0U);
}

static void Test_NoCommandRequiredBlocked(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    fixture.decision.action = REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED;
    CHECK(Create(&fixture, 1U, 0U, &intent) ==
          COMMAND_MANAGER_NO_COMMAND_REQUIRED);
    CHECK(fixture.manager.activeCount == 0U);
    CHECK(fixture.manager.nextCommandId == 1U);
}

static void Test_CommandIdZeroReservedAndIncreasing(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t first;
    CommandManager_Intent_t second;

    Fixture_InitWithFirstId(&fixture, 0U);
    CHECK(Create(&fixture, 1U, 0U, &first) == COMMAND_MANAGER_OK);
    CHECK(first.commandId == 1U);
    fixture.request.sessionId++;
    CHECK(Create(&fixture, 2U, 0U, &second) == COMMAND_MANAGER_OK);
    CHECK(second.commandId == 2U);
    CHECK(second.commandId > first.commandId);
}

static void Test_CommandIdWrapAndDuplicateAvoidance(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t first;
    CommandManager_Intent_t second;
    CommandManager_Intent_t third;

    Fixture_InitWithFirstId(&fixture, UINT32_MAX);
    CHECK(Create(&fixture, 1U, 0U, &first) == COMMAND_MANAGER_OK);
    CHECK(first.commandId == UINT32_MAX);
    fixture.request.sessionId++;
    CHECK(Create(&fixture, 2U, 0U, &second) == COMMAND_MANAGER_OK);
    CHECK(second.commandId == 1U);

    /* Force the candidate onto an active ID; allocation must skip it. */
    fixture.manager.nextCommandId = second.commandId;
    fixture.request.sessionId++;
    CHECK(Create(&fixture, 3U, 0U, &third) == COMMAND_MANAGER_OK);
    CHECK(third.commandId == 2U);
    CHECK(third.commandId != first.commandId);
    CHECK(third.commandId != second.commandId);
}

static void Test_RequestAndCommandIdsRemainIndependent(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    CHECK(Create(&fixture, UINT32_C(0xA5A5A5A5), 0U, &intent) ==
          COMMAND_MANAGER_OK);
    CHECK(intent.commandId == 1U);
    CHECK(intent.request.requestId == UINT32_C(0xA5A5A5A5));
    CHECK(intent.commandId != intent.request.requestId);
}

static void Test_DuplicateActiveRequestBlocked(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t first;
    CommandManager_Intent_t duplicate;

    Fixture_Init(&fixture);
    CHECK(Create(&fixture, 77U, 0U, &first) == COMMAND_MANAGER_OK);
    CHECK(Create(&fixture, 77U, 1U, &duplicate) ==
          COMMAND_MANAGER_DUPLICATE_REQUEST);
    CHECK(fixture.manager.activeCount == 1U);
    CHECK(fixture.manager.nextCommandId == 2U);
}

static void Test_FixedCapacityAndRelease(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intents[TEST_CAPACITY];
    CommandManager_Intent_t extra;
    size_t index;

    Fixture_Init(&fixture);
    for (index = 0U; index < TEST_CAPACITY; ++index)
    {
        fixture.request.sessionId = UINT64_C(100) + (uint64_t)index;
        CHECK(Create(&fixture, (uint32_t)index, (uint32_t)index,
                     &intents[index]) == COMMAND_MANAGER_OK);
    }
    CHECK(fixture.manager.activeCount == TEST_CAPACITY);
    fixture.request.sessionId++;
    CHECK(Create(&fixture, 99U, 99U, &extra) ==
          COMMAND_MANAGER_CAPACITY_EXCEEDED);

    CHECK(CommandManager_Release(&fixture.manager, intents[1].commandId) ==
          COMMAND_MANAGER_OK);
    CHECK(fixture.manager.activeCount == (TEST_CAPACITY - 1U));
    CHECK(Create(&fixture, 99U, 99U, &extra) == COMMAND_MANAGER_OK);
}

static void Test_SubmissionStateBeforeAndAfter(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;
    CommandManager_Intent_t snapshot;

    Fixture_Init(&fixture);
    CHECK(Create(&fixture, 1U, 100U, &intent) == COMMAND_MANAGER_OK);
    CHECK(CommandManager_GetIntent(&fixture.manager, intent.commandId,
                                   &snapshot) == COMMAND_MANAGER_OK);
    CHECK(snapshot.submissionState == COMMAND_MANAGER_SUBMISSION_NOT_SUBMITTED);
    CHECK(!snapshot.submissionAttempted);

    CHECK(CommandManager_RecordSubmission(
              &fixture.manager, intent.commandId,
              COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED, 120U) ==
          COMMAND_MANAGER_OK);
    CHECK(CommandManager_GetIntent(&fixture.manager, intent.commandId,
                                   &snapshot) == COMMAND_MANAGER_OK);
    CHECK(snapshot.submissionState ==
          COMMAND_MANAGER_SUBMISSION_LOCALLY_ACCEPTED);
    CHECK(snapshot.submissionAttempted);
    CHECK(snapshot.submissionAttemptedAtMs == 120U);
    CHECK(CommandManager_RecordSubmission(
              &fixture.manager, intent.commandId,
              COMMAND_MANAGER_SUBMIT_FAILED_OTHER, 130U) ==
          COMMAND_MANAGER_INVALID_TRANSITION);
}

static void Test_SubmissionFailureNeverBecomesDone(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;
    CommandManager_Intent_t snapshot;

    Fixture_Init(&fixture);
    CHECK(Create(&fixture, 1U, 100U, &intent) == COMMAND_MANAGER_OK);
    CHECK(CommandManager_RecordSubmission(
              &fixture.manager, intent.commandId,
              COMMAND_MANAGER_SUBMIT_FAILED_NOT_READY, 110U) ==
          COMMAND_MANAGER_OK);
    CHECK(CommandManager_GetIntent(&fixture.manager, intent.commandId,
                                   &snapshot) == COMMAND_MANAGER_OK);
    CHECK(snapshot.submissionState ==
          COMMAND_MANAGER_SUBMISSION_FAILED_NOT_READY);
    CHECK(snapshot.submissionState !=
          COMMAND_MANAGER_SUBMISSION_LOCALLY_ACCEPTED);
    CHECK(CommandManager_RecordSubmission(
              &fixture.manager, intent.commandId,
              COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED, 120U) ==
          COMMAND_MANAGER_INVALID_TRANSITION);
}

static void Test_InvalidSpecAndDigitalKeyBlocked(void)
{
    TestFixture_t fixture;
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    fixture.spec.target.door.target = (CommandManager_DoorTarget_t)99;
    CHECK(Create(&fixture, 1U, 0U, &intent) ==
          COMMAND_MANAGER_INVALID_ARGUMENT);

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_DIGITAL_KEY_SET_ENABLED;
    fixture.decision.action = REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY;
    CHECK(Create(&fixture, 1U, 0U, &intent) ==
          COMMAND_MANAGER_UNSUPPORTED_ACTION);
    CHECK(fixture.manager.activeCount == 0U);
}

static void Test_NullAndLookupErrors(void)
{
    TestFixture_t fixture;
    CommandManager_t uninitialized = { 0 };
    CommandManager_Intent_t intent;

    Fixture_Init(&fixture);
    CHECK(CommandManager_Init(NULL, fixture.slots, TEST_CAPACITY, 1U) ==
          COMMAND_MANAGER_INVALID_ARGUMENT);
    CHECK(CommandManager_CreateIntent(NULL, &fixture.request, 1U,
                                      &fixture.decision, &fixture.spec, 0U,
                                      &intent) == COMMAND_MANAGER_INVALID_ARGUMENT);
    CHECK(CommandManager_CreateIntent(&uninitialized, &fixture.request, 1U,
                                      &fixture.decision, &fixture.spec, 0U,
                                      &intent) == COMMAND_MANAGER_NOT_INITIALIZED);
    CHECK(CommandManager_CreateIntent(&fixture.manager, NULL, 1U,
                                      &fixture.decision, &fixture.spec, 0U,
                                      &intent) == COMMAND_MANAGER_INVALID_ARGUMENT);
    CHECK(CommandManager_GetIntent(&fixture.manager, 0U, &intent) ==
          COMMAND_MANAGER_INVALID_ARGUMENT);
    CHECK(CommandManager_GetIntent(&fixture.manager, 999U, &intent) ==
          COMMAND_MANAGER_NOT_FOUND);
    CHECK(CommandManager_RecordSubmission(
              &fixture.manager, 999U, COMMAND_MANAGER_SUBMIT_FAILED_OTHER, 0U) ==
          COMMAND_MANAGER_NOT_FOUND);
}

int main(void)
{
    RUN_TEST(Test_DoorIntentCreation);
    RUN_TEST(Test_ClimateAndInteriorLightIntents);
    RUN_TEST(Test_NonAllowedDecisionBlocked);
    RUN_TEST(Test_NoCommandRequiredBlocked);
    RUN_TEST(Test_CommandIdZeroReservedAndIncreasing);
    RUN_TEST(Test_CommandIdWrapAndDuplicateAvoidance);
    RUN_TEST(Test_RequestAndCommandIdsRemainIndependent);
    RUN_TEST(Test_DuplicateActiveRequestBlocked);
    RUN_TEST(Test_FixedCapacityAndRelease);
    RUN_TEST(Test_SubmissionStateBeforeAndAfter);
    RUN_TEST(Test_SubmissionFailureNeverBecomesDone);
    RUN_TEST(Test_InvalidSpecAndDigitalKeyBlocked);
    RUN_TEST(Test_NullAndLookupErrors);

    (void)printf("Executed %u tests, %u failed checks.\n",
                 g_executedTests, g_failedChecks);
    return (g_failedChecks == 0U) ? 0 : 1;
}
