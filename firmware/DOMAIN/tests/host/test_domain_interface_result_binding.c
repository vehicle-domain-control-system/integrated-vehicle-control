#include "domain_interface_result_binding.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
            return 1;                                                            \
        }                                                                        \
    } while (0)

#define ASSERT_EQ(expected, actual) ASSERT_TRUE((expected) == (actual))

typedef struct
{
    uint32_t nowMs;
    bool available;
    uint32_t callCount;
} TestClock_t;

static ResultManager_t *g_observerManager;
static ResultManager_RequestKey_t g_observerKey;
static DomainIf_Status_t g_observerReturn;
static uint32_t g_observerCalls;
static bool g_observerSawCommittedResult;
static DomainIf_BcmCommandResult_t g_observerResult;

static bool TestClock_GetNow(void *context, uint32_t *outNowMs)
{
    TestClock_t *clock = (TestClock_t *)context;

    ++clock->callCount;
    if (!clock->available)
    {
        return false;
    }
    *outNowMs = clock->nowMs;
    return true;
}

static ResultManager_Policy_t DefaultPolicy(void)
{
    ResultManager_Policy_t policy;

    policy.firstResponseTimeoutEnabled = true;
    policy.firstResponseTimeoutMs = 10U;
    policy.finalResultTimeoutEnabled = true;
    policy.finalResultTimeoutMs = 20U;
    policy.doorEvidenceTtlEnabled = true;
    policy.doorEvidenceTtlMs = 100U;
    return policy;
}

static DomainIf_MobileRequest_t DoorRequest(RequestId_t requestId)
{
    DomainIf_MobileRequest_t request;

    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = 11U;
    request.context.sessionId = 22U;
    request.context.requestId = requestId;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = DOOR_TARGET_LOCK;
    request.meta.quality = DATA_QUALITY_OK;
    request.meta.updateSequence = 1U;
    return request;
}

static ResultManager_RequestKey_t KeyFor(
    const DomainIf_MobileRequest_t *request)
{
    ResultManager_RequestKey_t key;

    key.deviceContextId = request->deviceContextId;
    key.context = request->context;
    return key;
}

static CommandManager_Intent_t DoorIntent(
    const DomainIf_MobileRequest_t *request,
    DomainCommandId_t commandId)
{
    CommandManager_Intent_t intent;

    (void)memset(&intent, 0, sizeof(intent));
    intent.commandId = commandId;
    intent.decisionSequence = 9U;
    intent.targetEcu = COMMAND_MANAGER_ECU_BCM;
    intent.kind = COMMAND_MANAGER_KIND_DOOR;
    intent.target.door.target = COMMAND_MANAGER_DOOR_TARGET_LOCK;
    intent.request.deviceContextId = request->deviceContextId;
    intent.request.sessionId = request->context.sessionId;
    intent.request.requestId = request->context.requestId;
    return intent;
}

static DomainIf_BcmCommandResult_t DoneDoorResult(
    DomainCommandId_t commandId)
{
    DomainIf_BcmCommandResult_t result;

    (void)memset(&result, 0, sizeof(result));
    result.commandId = commandId;
    result.kind = BCM_COMMAND_KIND_DOOR;
    result.result = REQUEST_RESULT_DONE;
    result.reason = RESULT_REASON_NONE;
    result.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    result.hasDoorEvidence = true;
    result.doorEvidence.type = BCM_DOOR_EVIDENCE_LOCK_COMPLETED;
    result.doorEvidence.occurrenceId = 77U;
    result.doorEvidence.target = DOOR_TARGET_LOCK;
    result.doorEvidence.quality = DATA_QUALITY_OK;
    result.doorEvidence.ageMs = 1U;
    return result;
}

static int SetupAcceptedDoor(
    ResultManager_t *manager,
    ResultManager_Slot_t *slots,
    DomainIf_MobileRequest_t *request,
    ResultManager_RequestKey_t *key,
    DomainCommandId_t commandId)
{
    ResultManager_Policy_t policy = DefaultPolicy();
    CommandManager_Intent_t intent;

    *request = DoorRequest((RequestId_t)(100U + commandId));
    *key = KeyFor(request);
    intent = DoorIntent(request, commandId);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(manager, slots, 1U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(manager, request, 100U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  manager, key, REQUEST_RESULT_ACCEPTED, RESULT_REASON_NONE,
                  false, 101U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_BindCommand(manager, key, &intent));
    return 0;
}

static DomainIf_Status_t ObserveBcmResult(
    const DomainIf_BcmCommandResult_t *result)
{
    ResultManager_Snapshot_t snapshot;

    ++g_observerCalls;
    g_observerResult = *result;
    g_observerSawCommittedResult =
        (g_observerManager != NULL) &&
        (ResultManager_GetSnapshot(g_observerManager, &g_observerKey,
                                   &snapshot) == RESULT_MANAGER_OK) &&
        snapshot.hasAppliedBcmResult &&
        (snapshot.appliedBcmResult.commandId == result->commandId);
    return g_observerReturn;
}

static int TestPreInitAndMissingDependencies(void)
{
    ResultManager_t manager;
    TestClock_t clock = {1U, true, 0U};
    DomainIf_BcmCommandResult_t result = DoneDoorResult(1U);

    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(DOMAIN_IF_NOT_INITIALIZED,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_Init());
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfResultBinding_Configure(NULL, TestClock_GetNow, NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIfResultBinding_Configure(&manager, NULL, NULL));
    return 0;
}

static int TestUninitializedManagerAndUnavailableClock(void)
{
    ResultManager_t manager;
    TestClock_t clock = {10U, true, 0U};
    DomainIf_BcmCommandResult_t result = DoneDoorResult(2U);

    (void)memset(&manager, 0, sizeof(manager));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_RxBcmCommandResult(&result));
    clock.available = false;
    ASSERT_EQ(DOMAIN_IF_NOT_AVAILABLE,
              DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(2U, clock.callCount);
    return 0;
}

static int TestDoneCommitBeforeObserver(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result = DoneDoorResult(3U);
    ResultManager_Snapshot_t snapshot;
    DomainIf_RxObserver_t observer;
    TestClock_t clock = {UINT32_MAX, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 3U));
    (void)memset(&observer, 0, sizeof(observer));
    observer.onBcmCommandResult = ObserveBcmResult;
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_SetRxObserver(&observer));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    g_observerManager = &manager;
    g_observerKey = key;
    g_observerReturn = DOMAIN_IF_OK;
    g_observerCalls = 0U;
    g_observerSawCommittedResult = false;

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(1U, g_observerCalls);
    ASSERT_TRUE(g_observerSawCommittedResult);
    ASSERT_EQ(result.commandId, g_observerResult.commandId);
    ASSERT_EQ(result.doorEvidence.occurrenceId,
              g_observerResult.doorEvidence.occurrenceId);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);
    ASSERT_EQ(UINT32_MAX, snapshot.resultUpdatedAtMs);
    ASSERT_EQ(RESULT_MANAGER_DISPATCH_UNKNOWN, snapshot.dispatchState);
    return 0;
}

static int TestObserverFailureDoesNotReplayCommit(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result = DoneDoorResult(4U);
    ResultManager_Snapshot_t snapshot;
    TestClock_t clock = {120U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 4U));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    g_observerManager = &manager;
    g_observerKey = key;
    g_observerReturn = DOMAIN_IF_REJECTED;
    g_observerCalls = 0U;
    g_observerSawCommittedResult = false;

    ASSERT_EQ(DOMAIN_IF_REJECTED,
              DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(1U, g_observerCalls);
    ASSERT_TRUE(g_observerSawCommittedResult);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);

    /* Retry is absorbed as a duplicate; Core and observer are not replayed. */
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(1U, g_observerCalls);
    return 0;
}

static int TestDoorDoneRequiresEvidence(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result = DoneDoorResult(5U);
    ResultManager_Snapshot_t snapshot;
    TestClock_t clock = {130U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 5U));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    g_observerCalls = 0U;
    result.hasDoorEvidence = false;
    ASSERT_EQ(DOMAIN_IF_REJECTED,
              DomainIf_RxBcmCommandResult(&result));
    ASSERT_EQ(0U, g_observerCalls);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    ASSERT_TRUE(!snapshot.terminal);
    ASSERT_TRUE(!snapshot.hasAppliedBcmResult);
    return 0;
}

static NetworkLocalFailure_t CommandFailure(
    const DomainIf_MobileRequest_t *request,
    DomainCommandId_t commandId)
{
    NetworkLocalFailure_t failure;

    (void)memset(&failure, 0, sizeof(failure));
    failure.link = NETWORK_LINK_CAN;
    failure.itemKind = NETWORK_LOCAL_ITEM_COMMAND;
    failure.reason = NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL;
    failure.identityGeneration = 0x12345678U;
    failure.elapsedSinceAdmissionMs = 9U;
    failure.hasCommandId = true;
    failure.commandId = commandId;
    failure.hasRequestContext = true;
    failure.requestDeviceContextId = request->deviceContextId;
    failure.requestContext = request->context;
    return failure;
}

static int TestNetworkFailurePreservesIdentityWithoutVehicleFailure(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    ResultManager_Snapshot_t snapshot;
    NetworkLocalFailure_t failure;
    TestClock_t clock = {140U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 6U));
    failure = CommandFailure(&request, 6U);
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxNetworkLocalFailure(&failure));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_TRUE(snapshot.hasNetworkLocalFailure);
    ASSERT_EQ(failure.identityGeneration,
              snapshot.networkLocalFailure.identityGeneration);
    ASSERT_EQ(failure.commandId, snapshot.networkLocalFailure.commandId);
    ASSERT_EQ(failure.requestContext.sessionId,
              snapshot.networkLocalFailure.requestContext.sessionId);
    ASSERT_EQ(failure.requestContext.requestId,
              snapshot.networkLocalFailure.requestContext.requestId);
    ASSERT_EQ(140U, snapshot.networkLocalFailureReceivedAtMs);
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    ASSERT_TRUE(snapshot.result != REQUEST_RESULT_FAILED);
    ASSERT_EQ(RESULT_MANAGER_DISPATCH_UNKNOWN, snapshot.dispatchState);
    ASSERT_TRUE(snapshot.outcomeUnconfirmed);

    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxNetworkLocalFailure(&failure));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(140U, snapshot.networkLocalFailureReceivedAtMs);
    return 0;
}

static int TestRequestResultFailureUsesOriginalRequestIdentity(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    ResultManager_Snapshot_t snapshot;
    NetworkLocalFailure_t failure;
    TestClock_t clock = {145U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 61U));
    (void)memset(&failure, 0, sizeof(failure));
    failure.link = NETWORK_LINK_UART_M;
    failure.itemKind = NETWORK_LOCAL_ITEM_REQUEST_RESULT;
    failure.reason = NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND;
    failure.identityGeneration = 33U;
    failure.elapsedSinceAdmissionMs = 44U;
    failure.hasRequestContext = true;
    failure.requestDeviceContextId = request.deviceContextId;
    failure.requestContext = request.context;

    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    ASSERT_EQ(DOMAIN_IF_OK, DomainIf_RxNetworkLocalFailure(&failure));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(NETWORK_LOCAL_ITEM_REQUEST_RESULT,
              snapshot.networkLocalFailure.itemKind);
    ASSERT_EQ(request.deviceContextId,
              snapshot.networkLocalFailure.requestDeviceContextId);
    ASSERT_EQ(request.context.sessionId,
              snapshot.networkLocalFailure.requestContext.sessionId);
    ASSERT_EQ(request.context.requestId,
              snapshot.networkLocalFailure.requestContext.requestId);
    ASSERT_EQ(33U, snapshot.networkLocalFailure.identityGeneration);
    ASSERT_EQ(RESULT_MANAGER_DISPATCH_UNKNOWN, snapshot.dispatchState);
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    return 0;
}

static int TestOccurrenceAndUnsupportedLinkRejected(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    NetworkLocalFailure_t failure;
    TestClock_t clock = {150U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 7U));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    failure = CommandFailure(&request, 7U);
    failure.itemKind = NETWORK_LOCAL_ITEM_OCCURRENCE;
    failure.hasCommandId = false;
    failure.commandId = 0U;
    failure.hasOccurrenceId = true;
    failure.occurrenceId = 99U;
    ASSERT_EQ(DOMAIN_IF_UNSUPPORTED,
              DomainIf_RxNetworkLocalFailure(&failure));
    ASSERT_EQ(0U, clock.callCount);

    failure = CommandFailure(&request, 7U);
    failure.link = NETWORK_LINK_UART_C;
    ASSERT_EQ(DOMAIN_IF_UNSUPPORTED,
              DomainIf_RxNetworkLocalFailure(&failure));
    ASSERT_EQ(0U, clock.callCount);
    return 0;
}

static int TestInvalidInputsAndUnknownIdentity(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result;
    NetworkLocalFailure_t failure;
    TestClock_t clock = {160U, true, 0U};

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, &request, &key, 8U));
    ASSERT_EQ(DOMAIN_IF_OK,
              DomainIfResultBinding_Configure(&manager, TestClock_GetNow,
                                               &clock));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_RxBcmCommandResult(NULL));
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_RxNetworkLocalFailure(NULL));

    result = DoneDoorResult(999U);
    ASSERT_EQ(DOMAIN_IF_REJECTED,
              DomainIf_RxBcmCommandResult(&result));
    failure = CommandFailure(&request, 8U);
    failure.itemKind = (NetworkLocalItemKind_t)99;
    ASSERT_EQ(DOMAIN_IF_INVALID_ARGUMENT,
              DomainIf_RxNetworkLocalFailure(&failure));
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestPreInitAndMissingDependencies();
    failures += TestUninitializedManagerAndUnavailableClock();
    failures += TestDoneCommitBeforeObserver();
    failures += TestObserverFailureDoesNotReplayCommit();
    failures += TestDoorDoneRequiresEvidence();
    failures += TestNetworkFailurePreservesIdentityWithoutVehicleFailure();
    failures += TestRequestResultFailureUsesOriginalRequestIdentity();
    failures += TestOccurrenceAndUnsupportedLinkRejected();
    failures += TestInvalidInputsAndUnknownIdentity();

    if (failures == 0)
    {
        printf("domain_interface_result_binding: 9 tests passed\n");
    }
    return failures;
}
