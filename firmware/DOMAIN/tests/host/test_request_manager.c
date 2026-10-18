#include "request_manager.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    RequestManager_Request_t request;
    RequestManager_AuthContext_t auth;
    RequestManager_Context_t context;
    RequestManager_Policy_t policy;
} TestFixture_t;

static unsigned int g_failedChecks;
static unsigned int g_executedTests;
static unsigned int g_commandExecutions;

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

static RequestManager_FunctionGate_t MakeOpenGate(void)
{
    RequestManager_FunctionGate_t gate;

    gate.supportKnown = true;
    gate.supported = true;
    gate.permissionKnown = true;
    gate.operationAllowed = true;
    return gate;
}

static void Fixture_Init(TestFixture_t *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));

    fixture->request.deviceContextId = UINT32_C(11);
    fixture->request.sessionId = UINT64_C(0x123456789);
    fixture->request.kind = REQUEST_MANAGER_KIND_DOOR;
    fixture->request.operation = REQUEST_MANAGER_OPERATION_DOOR_LOCK;
    fixture->request.quality = VEHICLE_STATE_QUALITY_OK;

    fixture->auth.registrationVerified = true;
    fixture->auth.registered = true;
    fixture->auth.connectionVerified = true;
    fixture->auth.connected = true;
    fixture->auth.sessionVerified = true;
    fixture->auth.currentDeviceContextId = fixture->request.deviceContextId;
    fixture->auth.currentSessionId = fixture->request.sessionId;

    fixture->context.door = MakeOpenGate();
    fixture->context.climate = MakeOpenGate();
    fixture->context.interiorLight = MakeOpenGate();
    fixture->context.digitalKeySetting = MakeOpenGate();
    fixture->context.doorLockState = REQUEST_MANAGER_DOOR_UNLOCKED;
    fixture->context.doorLockQuality = VEHICLE_STATE_QUALITY_OK;
    fixture->context.doorOpenState = REQUEST_MANAGER_DOOR_CLOSED;
    fixture->context.doorOpenQuality = VEHICLE_STATE_QUALITY_OK;
    fixture->context.currentFanLevel = REQUEST_MANAGER_FAN_LEVEL_OFF;
    fixture->context.currentFanQuality = VEHICLE_STATE_QUALITY_OK;
    fixture->context.cabinTemperatureQuality = VEHICLE_STATE_QUALITY_OK;
    fixture->context.cabinIlluminanceQuality = VEHICLE_STATE_QUALITY_OK;

    /* Test-supplied policy values; production limits remain an integration input. */
    fixture->policy.hasTargetTemperatureRange = true;
    fixture->policy.minimumTargetTemperatureCentiC = INT16_C(1600);
    fixture->policy.maximumTargetTemperatureCentiC = INT16_C(3000);
}

static RequestManager_Decision_t Evaluate(TestFixture_t *fixture)
{
    RequestManager_Decision_t decision;

    decision.verdict = REQUEST_MANAGER_VERDICT_REJECT;
    decision.reason = REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE;
    decision.action = REQUEST_MANAGER_ACTION_NONE;
    CHECK(RequestManager_Evaluate(&fixture->request, &fixture->auth,
                                  &fixture->context, &fixture->policy,
                                  &decision) == REQUEST_MANAGER_OK);
    return decision;
}

static void CheckAllowedAction(TestFixture_t *fixture, RequestManager_Action_t action)
{
    RequestManager_Decision_t decision = Evaluate(fixture);

    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_ALLOW);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_NONE);
    CHECK(decision.action == action);
}

static void Test_ValidAuthenticatedRequests(void)
{
    TestFixture_t fixture;

    Fixture_Init(&fixture);
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);

    fixture.request.operation = REQUEST_MANAGER_OPERATION_DOOR_UNLOCK;
    fixture.context.doorLockState = REQUEST_MANAGER_DOOR_LOCKED;
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);

    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation =
        REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE;
    fixture.request.argument.targetTemperatureCentiC = INT16_C(2200);
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);

    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_AUTO_ENABLED;
    fixture.request.argument.enabled = true;
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);

    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL;
    fixture.request.argument.fanLevel = REQUEST_MANAGER_FAN_LEVEL_HIGH;
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);

    fixture.request.kind = REQUEST_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_ENABLED;
    fixture.request.argument.enabled = true;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);

    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_LEVEL;
    fixture.request.argument.levelPercent = 80U;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);

    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_COLOR;
    fixture.request.argument.color.red = 1U;
    fixture.request.argument.color.green = 2U;
    fixture.request.argument.color.blue = 3U;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);

    fixture.request.kind = REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_DIGITAL_KEY_SET_ENABLED;
    fixture.request.argument.enabled = true;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY);
}

static void Test_UnregisteredAndSessionMismatch(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.auth.registered = false;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_DEVICE_NOT_REGISTERED);

    Fixture_Init(&fixture);
    fixture.auth.currentSessionId++;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_SESSION_MISMATCH);

    Fixture_Init(&fixture);
    fixture.auth.connected = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_NOT_CONNECTED);

    Fixture_Init(&fixture);
    fixture.auth.currentDeviceContextId++;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_DEVICE_MISMATCH);
}

static void Test_MissingVerifiedContext(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.auth.registrationVerified = false;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_UNAVAILABLE);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_REGISTRATION_UNAVAILABLE);

    Fixture_Init(&fixture);
    fixture.auth.connectionVerified = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_CONNECTION_UNAVAILABLE);

    Fixture_Init(&fixture);
    fixture.auth.sessionVerified = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_SESSION_UNAVAILABLE);
}

static void Test_UnknownKindAndOperation(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.request.kind = (RequestManager_RequestKind_t)99;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_UNKNOWN_REQUEST_KIND);

    Fixture_Init(&fixture);
    fixture.request.operation = (RequestManager_Operation_t)99;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION);

    Fixture_Init(&fixture);
    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION);
}

static void Test_RangeAndPolicyValidation(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_LEVEL;
    fixture.request.argument.levelPercent = 101U;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE);

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL;
    fixture.request.argument.fanLevel = REQUEST_MANAGER_FAN_LEVEL_UNKNOWN;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE);

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation =
        REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE;
    fixture.request.argument.targetTemperatureCentiC = INT16_C(1599);
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE);

    fixture.request.argument.targetTemperatureCentiC = INT16_C(2000);
    fixture.policy.hasTargetTemperatureRange = false;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_UNAVAILABLE);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_POLICY_UNAVAILABLE);

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation =
        REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE;
    fixture.policy.minimumTargetTemperatureCentiC = INT16_C(3000);
    fixture.policy.maximumTargetTemperatureCentiC = INT16_C(1600);
    CHECK(RequestManager_Evaluate(&fixture.request, &fixture.auth,
                                  &fixture.context, &fixture.policy,
                                  &decision) == REQUEST_MANAGER_INVALID_ARGUMENT);
}

static void Test_TemperatureAndLightBoundaries(void)
{
    TestFixture_t fixture;

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation =
        REQUEST_MANAGER_OPERATION_CLIMATE_SET_TARGET_TEMPERATURE;
    fixture.request.argument.targetTemperatureCentiC = INT16_C(1600);
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);
    fixture.request.argument.targetTemperatureCentiC = INT16_C(3000);
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_CLIMATE_INTENT);

    fixture.request.kind = REQUEST_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_LEVEL;
    fixture.request.argument.levelPercent = 100U;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);
}

static void Test_RequiredStateQualities(void)
{
    static const VehicleStateQuality_t qualities[] = {
        VEHICLE_STATE_QUALITY_NO_DATA,
        VEHICLE_STATE_QUALITY_STALE,
        VEHICLE_STATE_QUALITY_INVALID
    };
    static const RequestManager_Reason_t reasons[] = {
        REQUEST_MANAGER_REASON_STATE_NO_DATA,
        REQUEST_MANAGER_REASON_STATE_STALE,
        REQUEST_MANAGER_REASON_STATE_INVALID
    };
    TestFixture_t fixture;
    RequestManager_Decision_t decision;
    size_t index;

    for (index = 0U; index < (sizeof(qualities) / sizeof(qualities[0])); ++index)
    {
        Fixture_Init(&fixture);
        fixture.context.doorLockQuality = qualities[index];
        decision = Evaluate(&fixture);
        CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_UNAVAILABLE);
        CHECK(decision.reason == reasons[index]);
    }
}

static void Test_RequestQuality(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.request.quality = VEHICLE_STATE_QUALITY_NO_DATA;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_REQUEST_NO_DATA);

    fixture.request.quality = VEHICLE_STATE_QUALITY_STALE;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_REQUEST_STALE);

    fixture.request.quality = VEHICLE_STATE_QUALITY_INVALID;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_REQUEST_INVALID);
}

static void Test_DoorLockOpenAndUnknown(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.context.doorOpenState = REQUEST_MANAGER_DOOR_OPEN;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_REJECT);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_DOOR_OPEN);

    Fixture_Init(&fixture);
    fixture.context.doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_UNAVAILABLE);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN);

    Fixture_Init(&fixture);
    fixture.context.doorOpenQuality = VEHICLE_STATE_QUALITY_STALE;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_STATE_STALE);
}

static void Test_UnrelatedSensorQualityDoesNotBlock(void)
{
    TestFixture_t fixture;

    Fixture_Init(&fixture);
    fixture.context.cabinTemperatureQuality = VEHICLE_STATE_QUALITY_INVALID;
    fixture.context.cabinIlluminanceQuality = VEHICLE_STATE_QUALITY_NO_DATA;
    fixture.policy.minimumTargetTemperatureCentiC = INT16_C(3000);
    fixture.policy.maximumTargetTemperatureCentiC = INT16_C(1600);
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);

    fixture.request.kind = REQUEST_MANAGER_KIND_INTERIOR_LIGHT;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_INTERIOR_LIGHT_SET_COLOR;
    fixture.context.doorLockQuality = VEHICLE_STATE_QUALITY_STALE;
    CheckAllowedAction(&fixture,
                       REQUEST_MANAGER_ACTION_FORWARD_INTERIOR_LIGHT_INTENT);
}

static void Test_FunctionGateReasons(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.context.door.supportKnown = false;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_UNAVAILABLE);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_FUNCTION_SUPPORT_UNAVAILABLE);

    Fixture_Init(&fixture);
    fixture.context.door.supported = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_FUNCTION_UNSUPPORTED);

    Fixture_Init(&fixture);
    fixture.context.door.permissionKnown = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_PERMISSION_UNAVAILABLE);

    Fixture_Init(&fixture);
    fixture.context.door.operationAllowed = false;
    decision = Evaluate(&fixture);
    CHECK(decision.reason == REQUEST_MANAGER_REASON_OPERATION_NOT_ALLOWED);
}

static void Test_AlreadyAtTargetNeedsNoCommand(void)
{
    TestFixture_t fixture;

    Fixture_Init(&fixture);
    fixture.context.doorLockState = REQUEST_MANAGER_DOOR_LOCKED;
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED);

    fixture.request.kind = REQUEST_MANAGER_KIND_CLIMATE;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_CLIMATE_SET_FAN_LEVEL;
    fixture.request.argument.fanLevel = REQUEST_MANAGER_FAN_LEVEL_OFF;
    CheckAllowedAction(&fixture, REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED);
}

static void Test_EvaluationDoesNotExecuteCommand(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;
    unsigned int before;

    Fixture_Init(&fixture);
    before = g_commandExecutions;
    decision = Evaluate(&fixture);
    CHECK(decision.action == REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);
    CHECK(g_commandExecutions == before);
}

static void Test_DigitalKeySettingNeverAuthorizesUnlock(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    fixture.request.kind = REQUEST_MANAGER_KIND_DIGITAL_KEY_SETTING;
    fixture.request.operation = REQUEST_MANAGER_OPERATION_DIGITAL_KEY_SET_ENABLED;
    fixture.request.argument.enabled = true;
    decision = Evaluate(&fixture);
    CHECK(decision.verdict == REQUEST_MANAGER_VERDICT_ALLOW);
    CHECK(decision.action ==
          REQUEST_MANAGER_ACTION_UPDATE_DIGITAL_KEY_SETTING_ONLY);
    CHECK(decision.action != REQUEST_MANAGER_ACTION_FORWARD_DOOR_INTENT);
}

static void Test_NullArgumentsAndInvalidEnum(void)
{
    TestFixture_t fixture;
    RequestManager_Decision_t decision;

    Fixture_Init(&fixture);
    CHECK(RequestManager_Evaluate(NULL, &fixture.auth, &fixture.context,
                                  &fixture.policy, &decision) ==
          REQUEST_MANAGER_INVALID_ARGUMENT);
    CHECK(RequestManager_Evaluate(&fixture.request, NULL, &fixture.context,
                                  &fixture.policy, &decision) ==
          REQUEST_MANAGER_INVALID_ARGUMENT);
    CHECK(RequestManager_Evaluate(&fixture.request, &fixture.auth, NULL,
                                  &fixture.policy, &decision) ==
          REQUEST_MANAGER_INVALID_ARGUMENT);
    CHECK(RequestManager_Evaluate(&fixture.request, &fixture.auth,
                                  &fixture.context, NULL, &decision) ==
          REQUEST_MANAGER_INVALID_ARGUMENT);
    CHECK(RequestManager_Evaluate(&fixture.request, &fixture.auth,
                                  &fixture.context, &fixture.policy, NULL) ==
          REQUEST_MANAGER_INVALID_ARGUMENT);

    fixture.request.quality = (VehicleStateQuality_t)99;
    CHECK(RequestManager_Evaluate(&fixture.request, &fixture.auth,
                                  &fixture.context, &fixture.policy,
                                  &decision) == REQUEST_MANAGER_INVALID_ARGUMENT);
}

int main(void)
{
    RUN_TEST(Test_ValidAuthenticatedRequests);
    RUN_TEST(Test_UnregisteredAndSessionMismatch);
    RUN_TEST(Test_MissingVerifiedContext);
    RUN_TEST(Test_UnknownKindAndOperation);
    RUN_TEST(Test_RangeAndPolicyValidation);
    RUN_TEST(Test_TemperatureAndLightBoundaries);
    RUN_TEST(Test_RequiredStateQualities);
    RUN_TEST(Test_RequestQuality);
    RUN_TEST(Test_DoorLockOpenAndUnknown);
    RUN_TEST(Test_UnrelatedSensorQualityDoesNotBlock);
    RUN_TEST(Test_FunctionGateReasons);
    RUN_TEST(Test_AlreadyAtTargetNeedsNoCommand);
    RUN_TEST(Test_EvaluationDoesNotExecuteCommand);
    RUN_TEST(Test_DigitalKeySettingNeverAuthorizesUnlock);
    RUN_TEST(Test_NullArgumentsAndInvalidEnum);

    (void)printf("Executed %u tests, %u failed checks.\n",
                 g_executedTests, g_failedChecks);
    return (g_failedChecks == 0U) ? 0 : 1;
}
