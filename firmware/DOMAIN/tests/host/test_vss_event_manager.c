#include "vss_event_manager.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; \
} } while (0)

static DomainIf_VssOneShotEvent_t g_events[16];
static size_t g_eventCount;
static DomainIf_Status_t g_txStatus;
static SourceGeneration_t g_generation;

static bool Generation(void *context, SourceGeneration_t *outGeneration)
{
    (void)context;
    *outGeneration = g_generation;
    return g_generation != 0U;
}

static int BindGeneration(VssEventManager_t *manager)
{
    CHECK(VssEventManager_BindGenerationProvider(
              manager, Generation, NULL) == VSS_EVENT_MANAGER_OK);
    return 0;
}

static DomainIf_Status_t Capture(const DomainIf_VssOneShotEvent_t *event)
{
    if (g_eventCount < 16U) g_events[g_eventCount++] = *event;
    return g_txStatus;
}

static DomainIf_BcmCommandResult_t DoorResult(
    DomainCommandId_t command,
    DoorLockTarget_t target,
    DomainIf_BcmDoorEvidenceType_t evidence,
    RequestResult_t result,
    ResultReason_t reason,
    uint32_t sourceOccurrence)
{
    DomainIf_BcmCommandResult_t value;
    (void)memset(&value, 0, sizeof(value));
    value.commandId = command;
    value.kind = BCM_COMMAND_KIND_DOOR;
    value.result = result;
    value.reason = reason;
    value.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    value.hasDoorEvidence = true;
    value.doorEvidence.type = evidence;
    value.doorEvidence.occurrenceId = sourceOccurrence;
    value.doorEvidence.target = target;
    value.doorEvidence.quality = DATA_QUALITY_OK;
    return value;
}

static int DoorEvents(void)
{
    VssEventManager_t manager;
    DomainIf_BcmCommandResult_t lock;
    DomainIf_BcmCommandResult_t unlock;
    DomainIf_BcmCommandResult_t error;
    DomainIf_BcmCommandResult_t ignored;
    g_eventCount = 0U; g_txStatus = DOMAIN_IF_OK;
    g_generation = 101U;
    CHECK(VssEventManager_Init(&manager, Capture) == VSS_EVENT_MANAGER_OK);
    CHECK(BindGeneration(&manager) == 0);
    lock = DoorResult(10U, DOOR_TARGET_LOCK,
        BCM_DOOR_EVIDENCE_LOCK_COMPLETED, REQUEST_RESULT_DONE,
        RESULT_REASON_NONE, 50U);
    unlock = DoorResult(11U, DOOR_TARGET_UNLOCK,
        BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED, REQUEST_RESULT_DONE,
        RESULT_REASON_NONE, 51U);
    error = DoorResult(12U, DOOR_TARGET_LOCK,
        BCM_DOOR_EVIDENCE_REQUEST_FAILED, REQUEST_RESULT_FAILED,
        RESULT_REASON_NO_FEEDBACK, 52U);
    ignored = DoorResult(13U, DOOR_TARGET_UNLOCK,
        BCM_DOOR_EVIDENCE_REQUEST_FAILED, REQUEST_RESULT_FAILED,
        RESULT_REASON_NO_FEEDBACK, 53U);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &lock, 100U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &lock, 101U) ==
          VSS_EVENT_MANAGER_DUPLICATE);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &unlock, 102U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &error, 103U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &ignored, 104U) ==
          VSS_EVENT_MANAGER_IGNORED);
    CHECK(VssEventManager_Poll(&manager, 110U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 3U);
    CHECK(g_events[0].event == VSS_EVENT_DOOR_LOCK_COMPLETE);
    CHECK(g_events[1].event == VSS_EVENT_DOOR_UNLOCK_COMPLETE);
    CHECK(g_events[2].event == VSS_EVENT_DOOR_LOCK_ERROR);
    CHECK(g_events[0].occurrenceId == 1U &&
          g_events[1].occurrenceId == 2U &&
          g_events[2].occurrenceId == 3U);
    return 0;
}

static int RetryExpiryAndLocalFailure(void)
{
    VssEventManager_t manager;
    DomainIf_BcmCommandResult_t lock;
    NetworkLocalFailure_t failure;
    g_eventCount = 0U; g_txStatus = DOMAIN_IF_NOT_AVAILABLE;
    g_generation = 201U;
    CHECK(VssEventManager_Init(&manager, Capture) == VSS_EVENT_MANAGER_OK);
    CHECK(BindGeneration(&manager) == 0);
    lock = DoorResult(20U, DOOR_TARGET_LOCK,
        BCM_DOOR_EVIDENCE_LOCK_COMPLETED, REQUEST_RESULT_DONE,
        RESULT_REASON_NONE, 60U);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &lock,
                                               UINT32_MAX - 20U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&manager, UINT32_MAX - 10U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 1U && g_events[0].meta.ageMs == 10U);
    g_txStatus = DOMAIN_IF_OK;
    CHECK(VssEventManager_Poll(&manager, 5U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 2U && g_events[1].occurrenceId == 1U);
    (void)memset(&failure, 0, sizeof(failure));
    failure.itemKind = NETWORK_LOCAL_ITEM_OCCURRENCE;
    failure.hasOccurrenceId = true;
    failure.occurrenceId = 1U;
    failure.identityGeneration = 202U;
    CHECK(VssEventManager_RecordNetworkLocalFailure(&manager, &failure, 6U) ==
          VSS_EVENT_MANAGER_GENERATION_MISMATCH);
    CHECK(VssEventManager_Poll(&manager, 7U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 2U);
    failure.identityGeneration = 201U;
    CHECK(VssEventManager_RecordNetworkLocalFailure(&manager, &failure, 6U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&manager, 8U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 3U && g_events[2].occurrenceId == 1U);
    CHECK(VssEventManager_Poll(&manager, 480U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 3U);
    return 0;
}

static int VehicleTransitionsAndBootReset(void)
{
    VssEventManager_t first;
    VssEventManager_t rebooted;
    NetworkLocalFailure_t failure;
    g_eventCount = 0U; g_txStatus = DOMAIN_IF_OK;
    g_generation = 301U;
    CHECK(VssEventManager_Init(&first, Capture) == VSS_EVENT_MANAGER_OK);
    CHECK(BindGeneration(&first) == 0);
    CHECK(VssEventManager_RecordVehicleUseTransition(
              &first, VSS_EVENT_MANAGER_VEHICLE_USE_STARTED, 10U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_RecordVehicleUseTransition(
              &first, VSS_EVENT_MANAGER_VEHICLE_USE_ENDED, 20U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&first, 20U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_events[0].event == VSS_EVENT_VEHICLE_WELCOME);
    CHECK(g_events[1].event == VSS_EVENT_VEHICLE_GOODBYE);
    g_generation = 302U;
    CHECK(VssEventManager_Init(&rebooted, Capture) == VSS_EVENT_MANAGER_OK);
    CHECK(BindGeneration(&rebooted) == 0);
    CHECK(VssEventManager_RecordVehicleUseTransition(
              &rebooted, VSS_EVENT_MANAGER_VEHICLE_USE_STARTED, 30U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&rebooted, 30U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_events[2].occurrenceId == 1U);
    (void)memset(&failure, 0, sizeof(failure));
    failure.itemKind = NETWORK_LOCAL_ITEM_OCCURRENCE;
    failure.hasOccurrenceId = true;
    failure.occurrenceId = 1U;
    failure.identityGeneration = 301U;
    CHECK(VssEventManager_RecordNetworkLocalFailure(
              &rebooted, &failure, 31U) ==
          VSS_EVENT_MANAGER_GENERATION_MISMATCH);
    CHECK(VssEventManager_Poll(&rebooted, 32U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 3U);
    failure.identityGeneration = 302U;
    CHECK(VssEventManager_RecordNetworkLocalFailure(
              &rebooted, &failure, 33U) == VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&rebooted, 34U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 4U && g_events[3].occurrenceId == 1U);
    return 0;
}

static int MissingAndChangedGeneration(void)
{
    VssEventManager_t manager;
    DomainIf_BcmCommandResult_t lock;

    g_eventCount = 0U;
    g_txStatus = DOMAIN_IF_NOT_AVAILABLE;
    g_generation = 0U;
    CHECK(VssEventManager_Init(&manager, Capture) == VSS_EVENT_MANAGER_OK);
    CHECK(BindGeneration(&manager) == 0);
    lock = DoorResult(30U, DOOR_TARGET_LOCK,
        BCM_DOOR_EVIDENCE_LOCK_COMPLETED, REQUEST_RESULT_DONE,
        RESULT_REASON_NONE, 70U);
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &lock, 100U) ==
          VSS_EVENT_MANAGER_GENERATION_UNAVAILABLE);
    CHECK(VssEventManager_Poll(&manager, 101U) ==
          VSS_EVENT_MANAGER_GENERATION_UNAVAILABLE);
    CHECK(g_eventCount == 0U);

    g_generation = 401U;
    CHECK(VssEventManager_RecordBcmDoorResult(&manager, &lock, 102U) ==
          VSS_EVENT_MANAGER_OK);
    CHECK(VssEventManager_Poll(&manager, 103U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 1U);
    g_generation = 402U;
    CHECK(VssEventManager_Poll(&manager, 104U) == VSS_EVENT_MANAGER_OK);
    CHECK(g_eventCount == 1U);
    return 0;
}

int main(void)
{
    int failures = 0;
    failures += DoorEvents();
    failures += RetryExpiryAndLocalFailure();
    failures += VehicleTransitionsAndBootReset();
    failures += MissingAndChangedGeneration();
    if (failures == 0) (void)printf("PASS vss_event_manager\n");
    return failures == 0 ? 0 : 1;
}
