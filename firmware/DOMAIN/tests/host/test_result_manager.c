#include "result_manager.h"

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

static DomainIf_MobileRequest_t DoorRequest(
    DeviceContextId_t device,
    SessionId_t session,
    RequestId_t requestId,
    DoorLockTarget_t target)
{
    DomainIf_MobileRequest_t request;

    (void)memset(&request, 0, sizeof(request));
    request.deviceContextId = device;
    request.context.sessionId = session;
    request.context.requestId = requestId;
    request.type = MOBILE_REQUEST_DOOR_LOCK;
    request.payload.doorTarget = target;
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
    DomainCommandId_t commandId,
    CommandManager_DoorTarget_t target)
{
    CommandManager_Intent_t intent;

    (void)memset(&intent, 0, sizeof(intent));
    intent.commandId = commandId;
    intent.decisionSequence = 9U;
    intent.targetEcu = COMMAND_MANAGER_ECU_BCM;
    intent.kind = COMMAND_MANAGER_KIND_DOOR;
    intent.target.door.target = target;
    intent.request.deviceContextId = request->deviceContextId;
    intent.request.sessionId = request->context.sessionId;
    intent.request.requestId = request->context.requestId;
    return intent;
}

static DomainIf_BcmCommandResult_t DoorResult(
    DomainCommandId_t commandId,
    RequestResult_t resultValue,
    ResultReason_t reason,
    ResultConfirmation_t confirmation,
    DoorLockTarget_t target)
{
    DomainIf_BcmCommandResult_t result;

    (void)memset(&result, 0, sizeof(result));
    result.commandId = commandId;
    result.kind = BCM_COMMAND_KIND_DOOR;
    result.result = resultValue;
    result.reason = reason;
    result.confirmation = confirmation;
    if (resultValue == REQUEST_RESULT_DONE)
    {
        result.hasDoorEvidence = true;
        result.doorEvidence.type = (target == DOOR_TARGET_LOCK)
                                       ? BCM_DOOR_EVIDENCE_LOCK_COMPLETED
                                       : BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED;
        result.doorEvidence.occurrenceId = 7U;
        result.doorEvidence.target = target;
        result.doorEvidence.quality = DATA_QUALITY_OK;
        result.doorEvidence.ageMs = 1U;
    }
    return result;
}

static int SetupAcceptedDoor(
    ResultManager_t *manager,
    ResultManager_Slot_t *slots,
    size_t capacity,
    DomainIf_MobileRequest_t *request,
    CommandManager_Intent_t *intent,
    DomainCommandId_t commandId)
{
    ResultManager_Policy_t policy = DefaultPolicy();
    ResultManager_RequestKey_t key;

    *request = DoorRequest(11U, 22U, commandId + 100U, DOOR_TARGET_LOCK);
    key = KeyFor(request);
    *intent = DoorIntent(request, commandId, COMMAND_MANAGER_DOOR_TARGET_LOCK);

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(manager, slots, capacity, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(manager, request, 100U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  manager, &key, REQUEST_RESULT_ACCEPTED, RESULT_REASON_NONE,
                  false, 101U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_BindCommand(manager, &key, intent));
    return 0;
}

static int TestNormalBinding(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[2];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    ResultManager_RequestKey_t key;
    ResultManager_Snapshot_t snapshot;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 2U, &request, &intent, 1U));
    key = KeyFor(&request);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_TRUE(snapshot.hasCommand);
    ASSERT_EQ(1U, snapshot.commandId);
    ASSERT_EQ(BCM_COMMAND_KIND_DOOR, snapshot.commandKind);
    ASSERT_EQ(request.context.sessionId, snapshot.request.context.sessionId);
    ASSERT_EQ(request.context.requestId, snapshot.request.context.requestId);
    ASSERT_EQ(RESULT_MANAGER_DISPATCH_UNKNOWN, snapshot.dispatchState);
    return 0;
}

static int TestAcceptedProgressDone(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result;
    ResultManager_Snapshot_t snapshot;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 2U));
    key = KeyFor(&request);
    result = DoorResult(2U, REQUEST_RESULT_IN_PROGRESS, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_UNCONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_ApplyBcmResult(&manager, &result, 105U));
    result = DoorResult(2U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_CONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_ApplyBcmResult(&manager, &result, 110U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);
    ASSERT_TRUE(snapshot.terminal);
    ASSERT_TRUE(!snapshot.outcomeUnconfirmed);
    return 0;
}

static int TestAcceptedDirectDoneAndDuplicate(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    DomainIf_BcmCommandResult_t result;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 3U));
    result = DoorResult(3U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_CONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_ApplyBcmResult(&manager, &result, 110U));
    ASSERT_EQ(RESULT_MANAGER_DUPLICATE,
              ResultManager_ApplyBcmResult(&manager, &result, 111U));

    ++result.doorEvidence.occurrenceId;
    ASSERT_EQ(RESULT_MANAGER_FINAL_CONFLICT,
              ResultManager_ApplyBcmResult(&manager, &result, 111U));

    result.result = REQUEST_RESULT_IN_PROGRESS;
    result.reason = RESULT_REASON_NONE;
    result.confirmation = RESULT_CONFIRMATION_UNCONFIRMED;
    result.hasDoorEvidence = false;
    ASSERT_EQ(RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED,
              ResultManager_ApplyBcmResult(&manager, &result, 112U));
    return 0;
}

static int TestRejectedCancelledFailed(void)
{
    RequestResult_t finals[3] = {
        REQUEST_RESULT_REJECTED,
        REQUEST_RESULT_CANCELLED,
        REQUEST_RESULT_FAILED};
    ResultReason_t reasons[3] = {
        RESULT_REASON_NOT_ALLOWED,
        RESULT_REASON_SUPERSEDED,
        RESULT_REASON_MOTOR_FAULT};
    size_t index;

    for (index = 0U; index < 3U; ++index)
    {
        ResultManager_t manager;
        ResultManager_Slot_t slots[1];
        DomainIf_MobileRequest_t request;
        CommandManager_Intent_t intent;
        DomainIf_BcmCommandResult_t result;
        ResultManager_RequestKey_t key;
        ResultManager_Snapshot_t snapshot;

        ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent,
                                       (DomainCommandId_t)(10U + index)));
        key = KeyFor(&request);
        result = DoorResult((DomainCommandId_t)(10U + index), finals[index],
                            reasons[index], RESULT_CONFIRMATION_CONFIRMED,
                            DOOR_TARGET_LOCK);
        ASSERT_EQ(RESULT_MANAGER_OK,
                  ResultManager_ApplyBcmResult(&manager, &result, 120U));
        ASSERT_EQ(RESULT_MANAGER_OK,
                  ResultManager_GetSnapshot(&manager, &key, &snapshot));
        ASSERT_EQ(finals[index], snapshot.result);
    }
    return 0;
}

static int TestWrongCommandAndKind(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    DomainIf_BcmCommandResult_t result;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 20U));
    result = DoorResult(99U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_CONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_NOT_FOUND,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));
    result.commandId = 20U;
    result.kind = BCM_COMMAND_KIND_CLIMATE;
    result.hasDoorEvidence = false;
    ASSERT_EQ(RESULT_MANAGER_KIND_MISMATCH,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));
    return 0;
}

static int TestDoorEvidenceValidation(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    DomainIf_BcmCommandResult_t result;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 21U));
    result = DoorResult(21U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_CONFIRMED, DOOR_TARGET_LOCK);
    result.hasDoorEvidence = false;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));

    result.hasDoorEvidence = true;
    result.doorEvidence.quality = DATA_QUALITY_STALE;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));

    result.doorEvidence.quality = DATA_QUALITY_OK;
    result.doorEvidence.ageMs = 100U;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));

    result.doorEvidence.ageMs = 1U;
    result.doorEvidence.target = DOOR_TARGET_UNLOCK;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));

    result.doorEvidence.target = DOOR_TARGET_LOCK;
    result.doorEvidence.type = BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));
    result.doorEvidence.type = (DomainIf_BcmDoorEvidenceType_t)99;
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_ApplyBcmResult(&manager, &result, 130U));
    return 0;
}

static int TestCommandIdCannotBindTwoRequests(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[2];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t first = DoorRequest(1U, 2U, 70U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t second = DoorRequest(1U, 2U, 71U, DOOR_TARGET_LOCK);
    ResultManager_RequestKey_t firstKey = KeyFor(&first);
    ResultManager_RequestKey_t secondKey = KeyFor(&second);
    CommandManager_Intent_t firstIntent =
        DoorIntent(&first, 70U, COMMAND_MANAGER_DOOR_TARGET_LOCK);
    CommandManager_Intent_t secondIntent =
        DoorIntent(&second, 70U, COMMAND_MANAGER_DOOR_TARGET_LOCK);

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 2U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &first, 1U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &second, 1U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &firstKey, REQUEST_RESULT_ACCEPTED,
                  RESULT_REASON_NONE, false, 2U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &secondKey, REQUEST_RESULT_ACCEPTED,
                  RESULT_REASON_NONE, false, 2U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_BindCommand(&manager, &firstKey, &firstIntent));
    ASSERT_EQ(RESULT_MANAGER_COMMAND_CONFLICT,
              ResultManager_BindCommand(&manager, &secondKey, &secondIntent));
    return 0;
}

static int TestUnconfirmedTerminalAndLateConfirmed(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    ResultManager_RequestKey_t key;
    DomainIf_BcmCommandResult_t result;
    ResultManager_Snapshot_t snapshot;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 80U));
    key = KeyFor(&request);
    result = DoorResult(80U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_UNCONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_UNCONFIRMED_RECORDED,
              ResultManager_ApplyBcmResult(&manager, &result, 110U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    ASSERT_TRUE(snapshot.outcomeUnconfirmed);
    ASSERT_TRUE(snapshot.hasUnconfirmedBcmResult);

    result.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_ApplyBcmResult(&manager, &result, 111U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);
    ASSERT_TRUE(!snapshot.outcomeUnconfirmed);
    return 0;
}

static int TestRequestIdConflict(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t first = DoorRequest(1U, 2U, 3U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t second = first;
    ResultManager_RequestKey_t key = KeyFor(&first);
    ResultManager_Snapshot_t snapshot;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 1U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &first, 10U));
    second.payload.doorTarget = DOOR_TARGET_UNLOCK;
    ASSERT_EQ(RESULT_MANAGER_ID_CONFLICT,
              ResultManager_RegisterRequest(&manager, &second, 11U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_TRUE(snapshot.identityConflictObserved);
    ASSERT_TRUE(!snapshot.hasResult);
    return 0;
}

static int TestTimeoutsAndLateFinal(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[2];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t first = DoorRequest(1U, 2U, 30U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t second = DoorRequest(1U, 2U, 31U, DOOR_TARGET_LOCK);
    ResultManager_RequestKey_t firstKey = KeyFor(&first);
    ResultManager_RequestKey_t secondKey = KeyFor(&second);
    CommandManager_Intent_t intent = DoorIntent(&second, 31U,
                                                COMMAND_MANAGER_DOOR_TARGET_LOCK);
    ResultManager_TimeoutState_t timeout;
    ResultManager_Snapshot_t snapshot;
    DomainIf_MobileRequestResult_t mobileResult;
    DomainIf_BcmCommandResult_t result;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 2U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &first, 100U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(&manager, &firstKey, 109U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_NONE, timeout);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(&manager, &firstKey, 110U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE, timeout);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetMobileResult(&manager, &firstKey, 111U,
                                            &mobileResult));
    ASSERT_TRUE(!mobileResult.hasVehicleResult);
    ASSERT_EQ(RESULT_CONFIRMATION_UNCONFIRMED,
              mobileResult.resultInfo.confirmation);

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &second, 200U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &secondKey, REQUEST_RESULT_ACCEPTED,
                  RESULT_REASON_NONE, false, 202U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_BindCommand(&manager, &secondKey, &intent));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(&manager, &secondKey, 222U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_FINAL_RESULT, timeout);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &secondKey, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    ASSERT_TRUE(snapshot.outcomeUnconfirmed);

    result = DoorResult(31U, REQUEST_RESULT_IN_PROGRESS, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_UNCONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED,
              ResultManager_ApplyBcmResult(&manager, &result, 223U));
    result = DoorResult(31U, REQUEST_RESULT_DONE, RESULT_REASON_NONE,
                        RESULT_CONFIRMATION_CONFIRMED, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_ApplyBcmResult(&manager, &result, 224U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &secondKey, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_NONE, snapshot.timeoutState);
    ASSERT_TRUE(!snapshot.outcomeUnconfirmed);
    return 0;
}

static int TestNetworkFailureIsNotVehicleFailed(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    DomainIf_MobileRequest_t request;
    CommandManager_Intent_t intent;
    ResultManager_RequestKey_t key;
    NetworkLocalFailure_t failure;
    ResultManager_Snapshot_t snapshot;
    DomainIf_MobileRequestResult_t recent[1];
    size_t recentCount = 0U;

    ASSERT_EQ(0, SetupAcceptedDoor(&manager, slots, 1U, &request, &intent, 40U));
    key = KeyFor(&request);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDispatchState(
                  &manager, 40U, RESULT_MANAGER_NOT_DISPATCHED));

    (void)memset(&failure, 0, sizeof(failure));
    failure.link = NETWORK_LINK_CAN;
    failure.itemKind = NETWORK_LOCAL_ITEM_COMMAND;
    failure.reason = NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND;
    failure.identityGeneration = 3U;
    failure.elapsedSinceAdmissionMs = 15U;
    failure.hasCommandId = true;
    failure.commandId = 40U;
    failure.hasRequestContext = true;
    failure.requestDeviceContextId = request.deviceContextId;
    failure.requestContext = request.context;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordNetworkLocalFailure(&manager, &failure, 130U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, snapshot.result);
    ASSERT_TRUE(snapshot.result != REQUEST_RESULT_FAILED);
    ASSERT_TRUE(snapshot.hasNetworkLocalFailure);
    ASSERT_EQ(RESULT_MANAGER_NOT_DISPATCHED, snapshot.dispatchState);
    ASSERT_TRUE(snapshot.outcomeUnconfirmed);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, request.deviceContextId, 131U,
                  recent, 1U, &recentCount));
    ASSERT_EQ(1U, recentCount);
    ASSERT_TRUE(!recent[0].hasVehicleResult);
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, recent[0].resultInfo.result);
    ASSERT_EQ(RESULT_CONFIRMATION_UNCONFIRMED,
              recent[0].resultInfo.confirmation);

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDispatchState(
                  &manager, 40U, RESULT_MANAGER_DISPATCHED));
    ASSERT_EQ(RESULT_MANAGER_INVALID_TRANSITION,
              ResultManager_RecordDispatchState(
                  &manager, 40U, RESULT_MANAGER_NOT_DISPATCHED));
    return 0;
}

static int TestCapacityNullEnumsAndRollover(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t first = DoorRequest(1U, 2U, 50U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t second = DoorRequest(1U, 2U, 51U, DOOR_TARGET_LOCK);
    ResultManager_RequestKey_t key = KeyFor(&first);
    ResultManager_TimeoutState_t timeout;

    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_Init(NULL, slots, 1U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 1U, &policy));
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_RegisterRequest(&manager, NULL, 0U));
    second.type = (DomainIf_MobileRequestType_t)99;
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_RegisterRequest(&manager, &second, 0U));

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &first,
                                            UINT32_MAX - 5U));
    second = DoorRequest(1U, 2U, 51U, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_CAPACITY_EXCEEDED,
              ResultManager_RegisterRequest(&manager, &second, 0U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_EvaluateTimeout(&manager, &key, 4U, &timeout));
    ASSERT_EQ(RESULT_MANAGER_TIMEOUT_FIRST_RESPONSE, timeout);
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_GetSnapshot(&manager, &key, NULL));
    return 0;
}

static int TestNoCommandCompletionNeedsEvidence(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[1];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t request =
        DoorRequest(1U, 2U, 60U, DOOR_TARGET_LOCK);
    ResultManager_RequestKey_t key = KeyFor(&request);
    ResultManager_Snapshot_t snapshot;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 1U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &request, 1U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &key, REQUEST_RESULT_ACCEPTED, RESULT_REASON_NONE,
                  false, 2U));
    ASSERT_EQ(RESULT_MANAGER_EVIDENCE_REJECTED,
              ResultManager_RecordDomainResult(
                  &manager, &key, REQUEST_RESULT_DONE,
                  RESULT_REASON_ALREADY_AT_TARGET, false, 3U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &key, REQUEST_RESULT_DONE,
                  RESULT_REASON_ALREADY_AT_TARGET, true, 4U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetSnapshot(&manager, &key, &snapshot));
    ASSERT_EQ(REQUEST_RESULT_DONE, snapshot.result);
    return 0;
}

static int TestRecentHistoryEmptyAndArgumentContract(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[2];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequestResult_t output[2];
    DomainIf_MobileRequestResult_t before[2];
    size_t count = 99U;

    (void)memset(&manager, 0, sizeof(manager));
    (void)memset(output, 0xA5, sizeof(output));
    (void)memcpy(before, output, sizeof(output));
    ASSERT_EQ(RESULT_MANAGER_NOT_INITIALIZED,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 10U, output, 2U, &count));
    ASSERT_EQ(99U, count);
    ASSERT_TRUE(memcmp(output, before, sizeof(output)) == 0);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 2U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 10U, output, 2U, &count));
    ASSERT_EQ(0U, count);
    ASSERT_TRUE(memcmp(output, before, sizeof(output)) == 0);

    count = 99U;
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_GetRecentMobileResults(
                  &manager, 0U, 10U, output, 2U, &count));
    ASSERT_EQ(99U, count);
    ASSERT_TRUE(memcmp(output, before, sizeof(output)) == 0);
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 10U, NULL, 1U, &count));
    ASSERT_EQ(RESULT_MANAGER_INVALID_ARGUMENT,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 10U, output, 1U, NULL));
    return 0;
}

static int TestRecentHistoryRetentionAndOrdering(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[RESULT_MANAGER_HISTORY_PER_DEVICE_MAX];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequestResult_t output[RESULT_MANAGER_HISTORY_PER_DEVICE_MAX];
    DomainIf_MobileRequest_t request;
    ResultManager_RequestKey_t oldestKey;
    ResultManager_Snapshot_t snapshot;
    size_t index;
    size_t count = 0U;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(
                  &manager, slots, RESULT_MANAGER_HISTORY_PER_DEVICE_MAX,
                  &policy));
    for (index = 1U; index <= RESULT_MANAGER_HISTORY_PER_DEVICE_MAX; ++index)
    {
        request = DoorRequest(1U, 10U, (RequestId_t)index,
                              DOOR_TARGET_LOCK);
        ASSERT_EQ(RESULT_MANAGER_OK,
                  ResultManager_RegisterRequest(
                      &manager, &request, (uint32_t)index));
    }
    oldestKey.deviceContextId = 1U;
    oldestKey.context.sessionId = 10U;
    oldestKey.context.requestId = 1U;

    request = DoorRequest(1U, 10U, 9U, DOOR_TARGET_LOCK);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &request, 9U));
    ASSERT_EQ(RESULT_MANAGER_NOT_FOUND,
              ResultManager_GetSnapshot(&manager, &oldestKey, &snapshot));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 20U, output,
                  RESULT_MANAGER_HISTORY_PER_DEVICE_MAX, &count));
    ASSERT_EQ(RESULT_MANAGER_HISTORY_PER_DEVICE_MAX, count);
    for (index = 0U; index < count; ++index)
    {
        ASSERT_EQ((RequestId_t)(9U - index),
                  output[index].resultInfo.context.requestId);
    }

    count = 0U;
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 20U, NULL, 0U, &count));
    ASSERT_EQ(RESULT_MANAGER_HISTORY_PER_DEVICE_MAX, count);
    return 0;
}

static int TestRecentHistoryDeviceSessionAndUpdateIdentity(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[5];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t first =
        DoorRequest(1U, 10U, 7U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t secondSession =
        DoorRequest(1U, 11U, 7U, DOOR_TARGET_LOCK);
    DomainIf_MobileRequest_t otherDevice =
        DoorRequest(2U, 10U, 7U, DOOR_TARGET_LOCK);
    ResultManager_RequestKey_t firstKey = KeyFor(&first);
    DomainIf_MobileRequestResult_t output[5];
    size_t count = 0U;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 5U, &policy));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &first, 1U));
    ASSERT_EQ(RESULT_MANAGER_DUPLICATE,
              ResultManager_RegisterRequest(&manager, &first, 2U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RecordDomainResult(
                  &manager, &firstKey, REQUEST_RESULT_ACCEPTED,
                  RESULT_REASON_NONE, false, 3U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &secondSession, 4U));
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_RegisterRequest(&manager, &otherDevice, 5U));

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 1U, 10U, output, 5U, &count));
    ASSERT_EQ(2U, count);
    ASSERT_EQ(11U, output[0].resultInfo.context.sessionId);
    ASSERT_EQ(10U, output[1].resultInfo.context.sessionId);
    ASSERT_TRUE(output[1].hasVehicleResult);
    ASSERT_EQ(REQUEST_RESULT_ACCEPTED, output[1].resultInfo.result);
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 2U, 10U, output, 5U, &count));
    ASSERT_EQ(1U, count);
    ASSERT_EQ(2U, output[0].deviceContextId);
    return 0;
}

static int TestRecentHistorySequenceAndTimeRollover(void)
{
    ResultManager_t manager;
    ResultManager_Slot_t slots[3];
    ResultManager_Policy_t policy = DefaultPolicy();
    DomainIf_MobileRequest_t request;
    DomainIf_MobileRequestResult_t output[3];
    size_t index;
    size_t count = 0U;

    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_Init(&manager, slots, 3U, &policy));
    for (index = 1U; index <= 3U; ++index)
    {
        request = DoorRequest(3U, 20U, (RequestId_t)index,
                              DOOR_TARGET_LOCK);
        ASSERT_EQ(RESULT_MANAGER_OK,
                  ResultManager_RegisterRequest(
                      &manager, &request, UINT32_MAX - 3U + (uint32_t)index));
        if (index == 1U)
        {
            /* Force the per-Device serial near wrap, then use public writes. */
            slots[0].historySequence = UINT32_MAX - 1U;
        }
    }
    ASSERT_EQ(RESULT_MANAGER_OK,
              ResultManager_GetRecentMobileResults(
                  &manager, 3U, 2U, output, 3U, &count));
    ASSERT_EQ(3U, count);
    ASSERT_EQ(3U, output[0].resultInfo.context.requestId);
    ASSERT_EQ(2U, output[1].resultInfo.context.requestId);
    ASSERT_EQ(1U, output[2].resultInfo.context.requestId);
    ASSERT_EQ(3U, output[0].resultAgeMs);
    return 0;
}

int main(void)
{
    int failures = 0;

    failures += TestNormalBinding();
    failures += TestAcceptedProgressDone();
    failures += TestAcceptedDirectDoneAndDuplicate();
    failures += TestRejectedCancelledFailed();
    failures += TestWrongCommandAndKind();
    failures += TestDoorEvidenceValidation();
    failures += TestCommandIdCannotBindTwoRequests();
    failures += TestUnconfirmedTerminalAndLateConfirmed();
    failures += TestRequestIdConflict();
    failures += TestTimeoutsAndLateFinal();
    failures += TestNetworkFailureIsNotVehicleFailed();
    failures += TestCapacityNullEnumsAndRollover();
    failures += TestNoCommandCompletionNeedsEvidence();
    failures += TestRecentHistoryEmptyAndArgumentContract();
    failures += TestRecentHistoryRetentionAndOrdering();
    failures += TestRecentHistoryDeviceSessionAndUpdateIdentity();
    failures += TestRecentHistorySequenceAndTimeRollover();

    if (failures == 0)
    {
        printf("result_manager: all tests passed\n");
    }
    return failures;
}
