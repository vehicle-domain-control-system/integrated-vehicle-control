#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "request_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Application-internal command intent store. These types are not substitutes
 * for DomainIf C2 types. A later adapter performs explicit typed conversion.
 *
 * Concurrency contract: one serialized owner calls all APIs, or the caller
 * provides synchronization around each complete API call.
 */

typedef uint32_t CommandManager_CommandId_t;
typedef uint32_t CommandManager_RequestId_t;

typedef enum
{
    COMMAND_MANAGER_OK = 0,
    COMMAND_MANAGER_INVALID_ARGUMENT,
    COMMAND_MANAGER_NOT_INITIALIZED,
    COMMAND_MANAGER_DECISION_NOT_ALLOWED,
    COMMAND_MANAGER_NO_COMMAND_REQUIRED,
    COMMAND_MANAGER_UNSUPPORTED_ACTION,
    COMMAND_MANAGER_DUPLICATE_REQUEST,
    COMMAND_MANAGER_CAPACITY_EXCEEDED,
    COMMAND_MANAGER_COMMAND_ID_EXHAUSTED,
    COMMAND_MANAGER_NOT_FOUND,
    COMMAND_MANAGER_INVALID_TRANSITION
} CommandManager_Status_t;

typedef enum
{
    COMMAND_MANAGER_ECU_BCM = 0
} CommandManager_TargetEcu_t;

typedef enum
{
    COMMAND_MANAGER_KIND_DOOR = 0,
    COMMAND_MANAGER_KIND_CLIMATE,
    COMMAND_MANAGER_KIND_INTERIOR_LIGHT
} CommandManager_CommandKind_t;

typedef enum
{
    COMMAND_MANAGER_DOOR_TARGET_LOCK = 0,
    COMMAND_MANAGER_DOOR_TARGET_UNLOCK
} CommandManager_DoorTarget_t;

typedef enum
{
    COMMAND_MANAGER_THERMAL_IDLE = 0,
    COMMAND_MANAGER_THERMAL_COOL,
    COMMAND_MANAGER_THERMAL_HEAT
} CommandManager_ThermalDirection_t;

typedef enum
{
    COMMAND_MANAGER_LIGHT_NORMAL = 0,
    COMMAND_MANAGER_LIGHT_GOODBYE,
    COMMAND_MANAGER_LIGHT_WARNING,
    COMMAND_MANAGER_LIGHT_FAULT
} CommandManager_InteriorLightType_t;

typedef struct
{
    CommandManager_DoorTarget_t target;
} CommandManager_DoorTargetSpec_t;

typedef struct
{
    RequestManager_FanLevel_t fanTarget;
    CommandManager_ThermalDirection_t thermalDirection;
    uint8_t thermalTargetLevelPercent;
} CommandManager_ClimateTargetSpec_t;

typedef struct
{
    CommandManager_InteriorLightType_t type;
    uint8_t levelPercent;
    RequestManager_RgbColor_t color;
} CommandManager_InteriorLightTargetSpec_t;

typedef union
{
    CommandManager_DoorTargetSpec_t door;
    CommandManager_ClimateTargetSpec_t climate;
    CommandManager_InteriorLightTargetSpec_t interiorLight;
} CommandManager_Target_t;

typedef struct
{
    CommandManager_CommandKind_t kind;
    CommandManager_Target_t target;
} CommandManager_CommandSpec_t;

typedef struct
{
    uint32_t deviceContextId;
    uint64_t sessionId;
    CommandManager_RequestId_t requestId;
} CommandManager_RequestLink_t;

typedef enum
{
    /* These states describe only the local C2/Network submission boundary. */
    COMMAND_MANAGER_SUBMISSION_NOT_SUBMITTED = 0,
    COMMAND_MANAGER_SUBMISSION_LOCALLY_ACCEPTED,
    COMMAND_MANAGER_SUBMISSION_FAILED_NOT_READY,
    COMMAND_MANAGER_SUBMISSION_FAILED_NO_CAPACITY,
    COMMAND_MANAGER_SUBMISSION_FAILED_REJECTED,
    COMMAND_MANAGER_SUBMISSION_FAILED_OTHER
} CommandManager_SubmissionState_t;

typedef enum
{
    COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED = 0,
    COMMAND_MANAGER_SUBMIT_FAILED_NOT_READY,
    COMMAND_MANAGER_SUBMIT_FAILED_NO_CAPACITY,
    COMMAND_MANAGER_SUBMIT_FAILED_REJECTED,
    COMMAND_MANAGER_SUBMIT_FAILED_OTHER
} CommandManager_SubmitOutcome_t;

typedef struct
{
    CommandManager_CommandId_t commandId;
    uint32_t decisionSequence;
    CommandManager_TargetEcu_t targetEcu;
    CommandManager_CommandKind_t kind;
    CommandManager_Target_t target;
    uint32_t createdAtMs;
    CommandManager_RequestLink_t request;
    CommandManager_SubmissionState_t submissionState;
    uint32_t submissionAttemptedAtMs;
    bool submissionAttempted;
} CommandManager_Intent_t;

typedef struct
{
    bool inUse;
    CommandManager_Intent_t intent;
} CommandManager_Slot_t;

typedef struct
{
    CommandManager_Slot_t *slots;
    size_t capacity;
    size_t activeCount;
    CommandManager_CommandId_t nextCommandId;
    uint32_t nextDecisionSequence;
    bool initialized;
} CommandManager_t;

CommandManager_Status_t CommandManager_Init(
    CommandManager_t *manager,
    CommandManager_Slot_t *slots,
    size_t capacity,
    CommandManager_CommandId_t firstCommandId);

CommandManager_Status_t CommandManager_CreateIntent(
    CommandManager_t *manager,
    const RequestManager_Request_t *request,
    CommandManager_RequestId_t requestId,
    const RequestManager_Decision_t *decision,
    const CommandManager_CommandSpec_t *spec,
    uint32_t createdAtMs,
    CommandManager_Intent_t *outIntent);

CommandManager_Status_t CommandManager_GetIntent(
    const CommandManager_t *manager,
    CommandManager_CommandId_t commandId,
    CommandManager_Intent_t *outIntent);

CommandManager_Status_t CommandManager_RecordSubmission(
    CommandManager_t *manager,
    CommandManager_CommandId_t commandId,
    CommandManager_SubmitOutcome_t outcome,
    uint32_t attemptedAtMs);

/*
 * An ID remains active from CreateIntent until Release. The future result
 * owner must call Release only after its terminal correlation evidence is
 * retained; local submission acceptance alone is not such evidence.
 */
CommandManager_Status_t CommandManager_Release(
    CommandManager_t *manager,
    CommandManager_CommandId_t commandId);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_MANAGER_H */
