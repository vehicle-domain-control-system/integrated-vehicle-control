#include <stdio.h>
#include <string.h>

#include "domain_request_coordinator.h"

#define CHECK(x) do { if (!(x)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; \
} } while (0)

enum { TEST_CAPACITY = 8 };

typedef struct
{
    bool contextAvailable;
    bool transportAvailable;
    DomainIf_Status_t submitResult;
    DomainIf_Status_t publishResult;
    RequestManager_AuthContext_t auth;
    RequestManager_Context_t vehicle;
    RequestManager_Policy_t policy;
    unsigned submitCount;
    unsigned publishCount;
    DomainIf_BcmDoorCommand_t lastCommand;
    DomainIf_MobileRequestResult_t lastResult;
} FakePorts_t;

typedef struct
{
    DomainRequestCoordinator_t coordinator;
    CommandManager_Slot_t commands[TEST_CAPACITY];
    ResultManager_Slot_t results[TEST_CAPACITY];
    DomainRequestCoordinator_Track_t tracks[TEST_CAPACITY];
    FakePorts_t fake;
} Fixture_t;

static bool FakeGetContext(
    void *context,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_AuthContext_t *outAuth,
    RequestManager_Context_t *outVehicle,
    RequestManager_Policy_t *outPolicy)
{
    FakePorts_t *fake = (FakePorts_t *)context;
    (void)request;
    (void)nowMs;
    if (!fake->contextAvailable)
    {
        return false;
    }
    *outAuth = fake->auth;
    *outVehicle = fake->vehicle;
    *outPolicy = fake->policy;
    return true;
}

static bool FakeTransportAvailable(void *context)
{
    return ((FakePorts_t *)context)->transportAvailable;
}

static DomainIf_Status_t FakeSubmit(
    void *context,
    const DomainIf_BcmDoorCommand_t *command)
{
    FakePorts_t *fake = (FakePorts_t *)context;
    ++fake->submitCount;
    fake->lastCommand = *command;
    return fake->submitResult;
}

static DomainIf_Status_t FakePublish(
    void *context,
    const DomainIf_MobileRequestResult_t *result)
{
    FakePorts_t *fake = (FakePorts_t *)context;
    ++fake->publishCount;
    fake->lastResult = *result;
    return fake->publishResult;
}

static bool FakeNow(void *context, uint32_t *outNowMs)
{
    *outNowMs = *(const uint32_t *)context;
    return true;
}

static DomainIf_MobileRequest_t DoorRequest(
    RequestId_t requestId,
    DoorLockTarget_t target)
{
    DomainIf_MobileRequest_t request;
    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = 10U;
    request.context.sessionId = 20U;
    request.context.requestId = requestId;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = target;
    request.meta.quality = DATA_QUALITY_OK;
    request.meta.updateSequence = requestId;
    return request;
}

static DomainIf_MobileRequest_t AdmissionRequest(
    RequestId_t requestId,
    DoorLockTarget_t target)
{
    DomainIf_MobileRequest_t request = DoorRequest(requestId, target);
    request.meta.quality = DATA_QUALITY_NO_DATA;
    request.meta.ageMs = UINT32_MAX;
    request.meta.updateSequence = 0U;
    return request;
}

static DomainRequestCoordinator_ContextSnapshot_t AdmissionContext(
    uint32_t observedAtMs)
{
    DomainRequestCoordinator_ContextSnapshot_t context;
    (void)memset(&context, 0, sizeof(context));
    context.deviceContextId = 10U;
    context.sessionId = 20U;
    context.portEpoch = 1U;
    context.contextRevision = 1U;
    context.networkBootId = 77U;
    context.observedAtMs = observedAtMs;
    context.timestampKnown = true;
    context.linkUp = true;
    context.registrationVerified = true;
    context.registered = true;
    context.connectionVerified = true;
    context.connected = true;
    context.sessionVerified = true;
    context.p6AuthorizationVerified = true;
    return context;
}

static DomainRequestCoordinator_AdmissionEvidence_t AdmissionEvidence(
    uint16_t linkSequence,
    uint32_t capturedAtMs)
{
    DomainRequestCoordinator_AdmissionEvidence_t evidence;
    (void)memset(&evidence, 0, sizeof(evidence));
    evidence.deviceContextId = 10U;
    evidence.sessionId = 20U;
    evidence.portEpoch = 1U;
    evidence.contextRevision = 1U;
    evidence.networkBootId = 77U;
    evidence.linkSequence = linkSequence;
    evidence.capturedAtMs = capturedAtMs;
    evidence.timestampKnown = true;
    evidence.p6AuthorizationVerified = true;
    return evidence;
}

static int ConfigureAdmission(
    Fixture_t *fixture,
    uint32_t validityMs,
    uint32_t observedAtMs)
{
    DomainRequestCoordinator_ContextSnapshot_t context =
        AdmissionContext(observedAtMs);
    CHECK(DomainRequestCoordinator_ConfigureAdmissionEvidence(
              &fixture->coordinator, validityMs) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture->coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    return 0;
}

static int FixtureInit(Fixture_t *fixture)
{
    DomainRequestCoordinator_Ports_t ports;
    ResultManager_Policy_t resultPolicy;

    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->fake.contextAvailable = true;
    fixture->fake.transportAvailable = true;
    fixture->fake.submitResult = DOMAIN_IF_OK;
    fixture->fake.publishResult = DOMAIN_IF_OK;
    fixture->fake.auth.registrationVerified = true;
    fixture->fake.auth.registered = true;
    fixture->fake.auth.connectionVerified = true;
    fixture->fake.auth.connected = true;
    fixture->fake.auth.sessionVerified = true;
    fixture->fake.auth.currentDeviceContextId = 10U;
    fixture->fake.auth.currentSessionId = 20U;
    fixture->fake.vehicle.door.supportKnown = true;
    fixture->fake.vehicle.door.supported = true;
    fixture->fake.vehicle.door.permissionKnown = true;
    fixture->fake.vehicle.door.operationAllowed = true;
    fixture->fake.vehicle.doorLockState = REQUEST_MANAGER_DOOR_UNLOCKED;
    fixture->fake.vehicle.doorLockQuality = VEHICLE_STATE_QUALITY_OK;
    fixture->fake.vehicle.doorOpenState = REQUEST_MANAGER_DOOR_CLOSED;
    fixture->fake.vehicle.doorOpenQuality = VEHICLE_STATE_QUALITY_OK;

    (void)memset(&ports, 0, sizeof(ports));
    ports.getContext = FakeGetContext;
    ports.isCommandTransportAvailable = FakeTransportAvailable;
    ports.submitDoorCommand = FakeSubmit;
    ports.publishResult = FakePublish;
    ports.context = &fixture->fake;

    (void)memset(&resultPolicy, 0, sizeof(resultPolicy));
    resultPolicy.firstResponseTimeoutEnabled = true;
    resultPolicy.firstResponseTimeoutMs = 100U;
    resultPolicy.finalResultTimeoutEnabled = true;
    resultPolicy.finalResultTimeoutMs = 200U;
    resultPolicy.doorEvidenceTtlEnabled = true;
    resultPolicy.doorEvidenceTtlMs = 500U;

    CHECK(DomainRequestCoordinator_Init(
              &fixture->coordinator,
              fixture->commands, TEST_CAPACITY,
              fixture->results, TEST_CAPACITY,
              fixture->tracks, TEST_CAPACITY,
              1000U, &resultPolicy, &ports) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    return 0;
}

static DomainIf_BcmCommandResult_t DoorResult(
    const Fixture_t *fixture,
    RequestResult_t result,
    ResultReason_t reason)
{
    DomainIf_BcmCommandResult_t bcm;
    (void)memset(&bcm, 0, sizeof(bcm));
    bcm.commandId = fixture->fake.lastCommand.context.commandId;
    bcm.kind = BCM_COMMAND_KIND_DOOR;
    bcm.result = result;
    bcm.reason = reason;
    bcm.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    bcm.hasDoorEvidence = true;
    bcm.doorEvidence.occurrenceId = 1U;
    bcm.doorEvidence.target = fixture->fake.lastCommand.target;
    bcm.doorEvidence.quality = DATA_QUALITY_OK;
    if (result == REQUEST_RESULT_DONE)
    {
        bcm.doorEvidence.type =
            (bcm.doorEvidence.target == DOOR_TARGET_LOCK)
                ? BCM_DOOR_EVIDENCE_LOCK_COMPLETED
                : BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED;
    }
    else if (result == REQUEST_RESULT_REJECTED)
    {
        bcm.doorEvidence.type = BCM_DOOR_EVIDENCE_REQUEST_REJECTED;
    }
    else
    {
        bcm.doorEvidence.type = BCM_DOOR_EVIDENCE_REQUEST_FAILED;
    }
    return bcm;
}

static int TestLockAndUnlock(void)
{
    Fixture_t lockFixture;
    Fixture_t unlockFixture;
    DomainIf_MobileRequest_t request;
    CHECK(FixtureInit(&lockFixture) == 0);
    request = DoorRequest(1U, DOOR_TARGET_LOCK);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &lockFixture.coordinator, &request, 10U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(lockFixture.fake.submitCount == 1U);
    CHECK(lockFixture.fake.lastCommand.target == DOOR_TARGET_LOCK);
    CHECK(lockFixture.fake.lastCommand.context.commandId == 1000U);
    CHECK(lockFixture.fake.lastCommand.context.requestContext.requestId == 1U);
    CHECK(lockFixture.fake.lastResult.resultInfo.result == REQUEST_RESULT_ACCEPTED);

    CHECK(FixtureInit(&unlockFixture) == 0);
    unlockFixture.fake.vehicle.doorLockState = REQUEST_MANAGER_DOOR_LOCKED;
    request = DoorRequest(2U, DOOR_TARGET_UNLOCK);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &unlockFixture.coordinator, &request, 20U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(unlockFixture.fake.submitCount == 1U);
    CHECK(unlockFixture.fake.lastCommand.target == DOOR_TARGET_UNLOCK);
    return 0;
}

static int TestUnsupportedAndInvalid(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request;
    CHECK(FixtureInit(&fixture) == 0);
    request = DoorRequest(3U, DOOR_TARGET_LOCK);
    request.type = MOBILE_REQUEST_CLIMATE_AUTO_ENABLE;
    request.payload.enable = true;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(fixture.fake.submitCount == 0U);
    CHECK(fixture.fake.lastResult.resultInfo.reason ==
          RESULT_REASON_UNKNOWN_REQUEST);

    request = DoorRequest(0U, DOOR_TARGET_LOCK);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT);
    return 0;
}

static int TestAuthorizationAndQuality(void)
{
    Fixture_t authFixture;
    Fixture_t staleFixture;
    DomainIf_MobileRequest_t request = DoorRequest(4U, DOOR_TARGET_LOCK);
    CHECK(FixtureInit(&authFixture) == 0);
    authFixture.fake.auth.registrationVerified = false;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &authFixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(authFixture.fake.submitCount == 0U);
    CHECK(authFixture.fake.lastResult.resultInfo.reason == RESULT_REASON_NOT_READY);

    CHECK(FixtureInit(&staleFixture) == 0);
    staleFixture.fake.vehicle.doorLockQuality = VEHICLE_STATE_QUALITY_STALE;
    request.context.requestId = 5U;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &staleFixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(staleFixture.fake.submitCount == 0U);
    CHECK(staleFixture.fake.lastResult.resultInfo.reason == RESULT_REASON_STALE);
    return 0;
}

static int TestDuplicateAndConflict(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(6U, DOOR_TARGET_LOCK);
    CHECK(FixtureInit(&fixture) == 0);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 1U) ==
          DOMAIN_REQUEST_COORDINATOR_DUPLICATE);
    CHECK(fixture.fake.submitCount == 1U);
    request.payload.doorTarget = DOOR_TARGET_UNLOCK;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 2U) ==
          DOMAIN_REQUEST_COORDINATOR_ID_CONFLICT);
    CHECK(fixture.fake.submitCount == 1U);
    return 0;
}

static int TestTransportFailures(void)
{
    Fixture_t unavailable;
    Fixture_t rejected;
    DomainIf_MobileRequest_t request = DoorRequest(7U, DOOR_TARGET_LOCK);
    CHECK(FixtureInit(&unavailable) == 0);
    unavailable.fake.transportAvailable = false;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &unavailable.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_UNAVAILABLE);
    CHECK(unavailable.fake.submitCount == 0U);
    CHECK(unavailable.coordinator.submissionFailureCount == 1U);

    CHECK(FixtureInit(&rejected) == 0);
    rejected.fake.submitResult = DOMAIN_IF_REJECTED;
    request.context.requestId = 8U;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &rejected.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_TRANSPORT_FAILED);
    CHECK(rejected.fake.submitCount == 1U);
    CHECK(rejected.fake.lastResult.resultInfo.result == REQUEST_RESULT_ACCEPTED);
    return 0;
}

static int TestEcuResults(void)
{
    Fixture_t doneFixture;
    Fixture_t failedFixture;
    DomainIf_MobileRequest_t request = DoorRequest(9U, DOOR_TARGET_LOCK);
    DomainIf_BcmCommandResult_t result;
    CHECK(FixtureInit(&doneFixture) == 0);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &doneFixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    result = DoorResult(&doneFixture, REQUEST_RESULT_DONE, RESULT_REASON_NONE);
    CHECK(DomainRequestCoordinator_HandleBcmResult(
              &doneFixture.coordinator, &result, 50U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(doneFixture.fake.lastResult.hasVehicleResult);
    CHECK(doneFixture.fake.lastResult.resultInfo.result == REQUEST_RESULT_DONE);
    CHECK(DomainRequestCoordinator_HandleBcmResult(
              &doneFixture.coordinator, &result, 51U) ==
          DOMAIN_REQUEST_COORDINATOR_DUPLICATE);

    CHECK(FixtureInit(&failedFixture) == 0);
    request.context.requestId = 10U;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &failedFixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    result = DoorResult(&failedFixture, REQUEST_RESULT_FAILED,
                        RESULT_REASON_MOTOR_FAULT);
    CHECK(DomainRequestCoordinator_HandleBcmResult(
              &failedFixture.coordinator, &result, 60U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(failedFixture.fake.lastResult.resultInfo.result ==
          REQUEST_RESULT_FAILED);
    CHECK(failedFixture.fake.lastResult.resultInfo.reason ==
          RESULT_REASON_MOTOR_FAULT);
    return 0;
}

static int TestTimeoutAndRollover(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(11U, DOOR_TARGET_LOCK);
    size_t count = 0U;
    CHECK(FixtureInit(&fixture) == 0);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, UINT32_MAX - 50U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainRequestCoordinator_Poll(&fixture.coordinator, 148U, &count) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(count == 0U);
    CHECK(DomainRequestCoordinator_Poll(&fixture.coordinator, 149U, &count) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(count == 1U);
    CHECK(!fixture.fake.lastResult.hasVehicleResult);
    CHECK(fixture.fake.lastResult.resultInfo.confirmation ==
          RESULT_CONFIRMATION_UNCONFIRMED);
    return 0;
}

static int TestAlreadyAtTarget(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(12U, DOOR_TARGET_LOCK);
    CHECK(FixtureInit(&fixture) == 0);
    fixture.fake.vehicle.doorLockState = REQUEST_MANAGER_DOOR_LOCKED;
    CHECK(DomainRequestCoordinator_HandleMobileRequest(
              &fixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 0U);
    CHECK(fixture.fake.lastResult.resultInfo.result == REQUEST_RESULT_DONE);
    CHECK(fixture.fake.lastResult.resultInfo.reason ==
          RESULT_REASON_ALREADY_AT_TARGET);
    return 0;
}

static int TestC2IngressBinding(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(13U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t duplicate;
    DomainIf_MobileRequest_t next;
    uint32_t nowMs = 77U;
    size_t timeoutCount = 0U;
    CHECK(FixtureInit(&fixture) == 0);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    CHECK(DomainRequestCoordinator_BindIngress(
              &fixture.coordinator, FakeNow, &nowMs, 20U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(fixture.fake.submitCount == 0U);
    CHECK(fixture.fake.publishCount == 0U);

    duplicate = request;
    CHECK(DomainIf_RxMobileRequest(&duplicate) == DOMAIN_IF_OK);
    CHECK(fixture.coordinator.inboxCount == 1U);
    duplicate.payload.doorTarget = DOOR_TARGET_UNLOCK;
    CHECK(DomainIf_RxMobileRequest(&duplicate) == DOMAIN_IF_REJECTED);

    next = DoorRequest(14U, DOOR_TARGET_LOCK);
    CHECK(DomainIf_RxMobileRequest(&next) == DOMAIN_IF_OK);
    next = DoorRequest(15U, DOOR_TARGET_LOCK);
    CHECK(DomainIf_RxMobileRequest(&next) == DOMAIN_IF_OK);
    next = DoorRequest(16U, DOOR_TARGET_LOCK);
    CHECK(DomainIf_RxMobileRequest(&next) == DOMAIN_IF_OK);
    next = DoorRequest(17U, DOOR_TARGET_LOCK);
    CHECK(DomainIf_RxMobileRequest(&next) == DOMAIN_IF_QUEUE_FULL);

    request.payload.doorTarget = DOOR_TARGET_UNLOCK;
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, nowMs, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == DOMAIN_REQUEST_COORDINATOR_POLL_BUDGET);
    CHECK(fixture.coordinator.inboxCount == 2U);
    CHECK(fixture.fake.lastCommand.target == DOOR_TARGET_LOCK);
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, nowMs, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 4U);
    CHECK(fixture.coordinator.inboxCount == 0U);
    CHECK(fixture.fake.lastCommand.context.commandAgeMs == 0U);

    DomainRequestCoordinator_ResetBinding();
    CHECK(FixtureInit(&fixture) == 0);
    nowMs = UINT32_MAX - 2U;
    CHECK(DomainRequestCoordinator_BindIngress(
              &fixture.coordinator, FakeNow, &nowMs, 10U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    request = DoorRequest(18U, DOOR_TARGET_LOCK);
    request.meta.ageMs = 7U;
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_OK);
    CHECK(fixture.fake.submitCount == 0U);
    nowMs = 1U;
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, nowMs, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 0U);
    CHECK(fixture.fake.lastResult.resultInfo.reason == RESULT_REASON_STALE);

    request = DoorRequest(19U, DOOR_TARGET_LOCK);
    request.meta.quality = DATA_QUALITY_NO_DATA;
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_REJECTED);
    request.meta.quality = DATA_QUALITY_OK;
    request.meta.updateSequence = 0U;
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_REJECTED);
    request.meta.updateSequence = 19U;
    request.meta.ageMs = 10U;
    CHECK(DomainIf_RxMobileRequest(&request) == DOMAIN_IF_REJECTED);
    return 0;
}

static int TestAdmissionEvidenceCopyAndPoll(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request =
        AdmissionRequest(30U, DOOR_TARGET_LOCK);
    DomainRequestCoordinator_AdmissionEvidence_t evidence =
        AdmissionEvidence(1U, 100U);
    size_t timeoutCount = 0U;

    CHECK(FixtureInit(&fixture) == 0);
    CHECK(ConfigureAdmission(&fixture, 20U, 100U) == 0);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 100U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    request.payload.doorTarget = DOOR_TARGET_UNLOCK;
    evidence.portEpoch = 99U;
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, 101U, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 1U);
    CHECK(fixture.fake.lastCommand.target == DOOR_TARGET_LOCK);
    CHECK(fixture.coordinator.inboxCount == 0U);
    return 0;
}

static int TestAdmissionContextInvalidation(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request =
        AdmissionRequest(31U, DOOR_TARGET_LOCK);
    DomainRequestCoordinator_AdmissionEvidence_t evidence =
        AdmissionEvidence(1U, 100U);
    DomainRequestCoordinator_ContextSnapshot_t context =
        AdmissionContext(101U);

    CHECK(FixtureInit(&fixture) == 0);
    CHECK(ConfigureAdmission(&fixture, 20U, 100U) == 0);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 100U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    context.contextRevision = 2U;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.coordinator.inboxCount == 0U);
    CHECK(fixture.coordinator.admissionInvalidatedCount == 1U);
    CHECK(fixture.coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_CONTEXT_CHANGED);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 101U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);

    evidence.contextRevision = 2U;
    evidence.capturedAtMs = 101U;
    evidence.linkSequence = 2U;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 101U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    context.portEpoch = 2U;
    context.contextRevision = 3U;
    context.observedAtMs = 102U;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED);

    context.networkBootId = 78U;
    context.contextRevision = 4U;
    context.observedAtMs = 103U;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED);

    request = AdmissionRequest(32U, DOOR_TARGET_LOCK);
    evidence.portEpoch = 2U;
    evidence.contextRevision = 4U;
    evidence.networkBootId = 78U;
    evidence.linkSequence = 1U;
    evidence.capturedAtMs = 103U;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 103U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    context.linkUp = false;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(fixture.coordinator.inboxCount == 0U);
    CHECK(!fixture.coordinator.admissionContextValid);
    CHECK(fixture.coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN);

    context.linkUp = true;
    context.observedAtMs = 104U;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    request = AdmissionRequest(33U, DOOR_TARGET_LOCK);
    evidence.linkSequence = 1U;
    evidence.capturedAtMs = 104U;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 104U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    context.p6AuthorizationVerified = false;
    CHECK(DomainRequestCoordinator_UpdateAdmissionContext(
              &fixture.coordinator, &context) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(fixture.coordinator.inboxCount == 0U);
    CHECK(fixture.coordinator.lastAdmissionInvalidation ==
          DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
    return 0;
}

static int TestAdmissionReplayFreshnessAndRollover(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t first =
        AdmissionRequest(40U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t second =
        AdmissionRequest(41U, DOOR_TARGET_LOCK);
    DomainRequestCoordinator_AdmissionEvidence_t evidence =
        AdmissionEvidence(UINT16_MAX, UINT32_MAX - 2U);
    size_t timeoutCount = 0U;
    unsigned index;

    CHECK(FixtureInit(&fixture) == 0);
    CHECK(ConfigureAdmission(&fixture, 10U, UINT32_MAX - 2U) == 0);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &first, &evidence,
              UINT32_MAX - 2U) == DOMAIN_REQUEST_COORDINATOR_OK);
    evidence.linkSequence = 0U;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &second, &evidence,
              UINT32_MAX - 2U) == DOMAIN_REQUEST_COORDINATOR_OK);
    evidence.linkSequence = UINT16_MAX;
    first = AdmissionRequest(42U, DOOR_TARGET_LOCK);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &first,
              &evidence, 1U) == DOMAIN_REQUEST_COORDINATOR_REJECTED);
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, 1U, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 2U);

    CHECK(FixtureInit(&fixture) == 0);
    CHECK(ConfigureAdmission(&fixture, 10U, 110U) == 0);
    evidence = AdmissionEvidence(1U, 100U);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &first, &evidence, 110U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);

    for (index = 0U; index < DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
         ++index)
    {
        DomainIf_MobileRequest_t request = AdmissionRequest(
            (RequestId_t)(50U + index), DOOR_TARGET_LOCK);
        evidence = AdmissionEvidence((uint16_t)(index + 1U), 110U);
        CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
                  &fixture.coordinator, &request, &evidence, 110U) ==
              DOMAIN_REQUEST_COORDINATOR_OK);
    }
    first = AdmissionRequest(60U, DOOR_TARGET_LOCK);
    evidence = AdmissionEvidence(5U, 110U);
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &first, &evidence, 110U) ==
          DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED);
    return 0;
}

static int TestAdmissionAuthorizationFailClosed(void)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request =
        AdmissionRequest(70U, DOOR_TARGET_LOCK);
    DomainRequestCoordinator_AdmissionEvidence_t evidence =
        AdmissionEvidence(1U, 200U);
    size_t timeoutCount = 0U;

    CHECK(FixtureInit(&fixture) == 0);
    CHECK(ConfigureAdmission(&fixture, 20U, 200U) == 0);
    fixture.coordinator.pollActive = true;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 200U) ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    fixture.coordinator.pollActive = false;
    fixture.fake.contextAvailable = false;
    CHECK(DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
              &fixture.coordinator, &request, &evidence, 200U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(DomainRequestCoordinator_Poll(
              &fixture.coordinator, 201U, &timeoutCount) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 0U);
    CHECK(fixture.fake.lastResult.resultInfo.reason == RESULT_REASON_NOT_READY);
    CHECK(fixture.coordinator.lastInboxStatus ==
          DOMAIN_REQUEST_COORDINATOR_REJECTED);
    return 0;
}

int main(void)
{
    unsigned passed = 0U;
    CHECK(TestLockAndUnlock() == 0); ++passed;
    CHECK(TestUnsupportedAndInvalid() == 0); ++passed;
    CHECK(TestAuthorizationAndQuality() == 0); ++passed;
    CHECK(TestDuplicateAndConflict() == 0); ++passed;
    CHECK(TestTransportFailures() == 0); ++passed;
    CHECK(TestEcuResults() == 0); ++passed;
    CHECK(TestTimeoutAndRollover() == 0); ++passed;
    CHECK(TestAlreadyAtTarget() == 0); ++passed;
    CHECK(TestC2IngressBinding() == 0); ++passed;
    CHECK(TestAdmissionEvidenceCopyAndPoll() == 0); ++passed;
    CHECK(TestAdmissionContextInvalidation() == 0); ++passed;
    CHECK(TestAdmissionReplayFreshnessAndRollover() == 0); ++passed;
    CHECK(TestAdmissionAuthorizationFailClosed() == 0); ++passed;
    printf("domain_request_coordinator: %u/13 scenarios passed\n", passed);
    return 0;
}
