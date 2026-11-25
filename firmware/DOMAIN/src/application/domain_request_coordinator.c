#include "domain_request_coordinator.h"

#include <string.h>

typedef struct
{
    DomainRequestCoordinator_t *coordinator;
    DomainRequestCoordinator_TimeProvider_t getNowMs;
    void *timeContext;
    bool configured;
} DomainRequestCoordinator_Ingress_t;

static DomainRequestCoordinator_Ingress_t g_domainRequestIngress;

static bool DomainRequestCoordinator_SameIdentity(
    const DomainIf_MobileRequest_t *left,
    const DomainIf_MobileRequest_t *right)
{
    return (left->deviceContextId == right->deviceContextId) &&
           (left->context.sessionId == right->context.sessionId) &&
           (left->context.requestId == right->context.requestId);
}

static bool DomainRequestCoordinator_SamePayload(
    const DomainIf_MobileRequest_t *left,
    const DomainIf_MobileRequest_t *right)
{
    if ((left->type != right->type) ||
        (left->meta.quality != right->meta.quality) ||
        (left->meta.ageMs != right->meta.ageMs) ||
        (left->meta.updateSequence != right->meta.updateSequence))
    {
        return false;
    }
    switch (left->type)
    {
        case MOBILE_REQUEST_DOOR_LOCK:
            return left->payload.doorTarget == right->payload.doorTarget;
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
            return left->payload.targetTemperature ==
                   right->payload.targetTemperature;
        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
            return left->payload.enable == right->payload.enable;
        case MOBILE_REQUEST_FAN_LEVEL:
            return left->payload.fanLevel == right->payload.fanLevel;
        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
            return left->payload.levelPercent == right->payload.levelPercent;
        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            return (left->payload.color.red == right->payload.color.red) &&
                   (left->payload.color.green == right->payload.color.green) &&
                   (left->payload.color.blue == right->payload.color.blue);
        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            return left->payload.digitalKeySetting ==
                   right->payload.digitalKeySetting;
        default:
            return false;
    }
}

static uint32_t DomainRequestCoordinator_SaturatingAdd(
    uint32_t left,
    uint32_t right)
{
    return (right > (UINT32_MAX - left)) ? UINT32_MAX : (left + right);
}

static bool DomainRequestCoordinator_SourceAdmissible(
    const DomainIf_MobileRequest_t *request,
    uint32_t sourceValidityMs)
{
    return (request->meta.quality == DATA_QUALITY_OK) &&
           (request->meta.updateSequence != 0U) &&
           (request->meta.ageMs < sourceValidityMs);
}

static bool DomainRequestCoordinator_Fresh(
    uint32_t nowMs,
    uint32_t observedAtMs,
    uint32_t validityMs)
{
    return (validityMs != 0U) &&
           ((uint32_t)(nowMs - observedAtMs) < validityMs);
}

static bool DomainRequestCoordinator_ContextAuthoritative(
    const DomainRequestCoordinator_ContextSnapshot_t *snapshot)
{
    return (snapshot != NULL) && (snapshot->deviceContextId != 0U) &&
           (snapshot->sessionId != 0U) && (snapshot->portEpoch != 0U) &&
           (snapshot->contextRevision != 0U) &&
           (snapshot->networkBootId != 0U) && snapshot->timestampKnown &&
           snapshot->linkUp && snapshot->registrationVerified &&
           snapshot->registered && snapshot->connectionVerified &&
           snapshot->connected && snapshot->sessionVerified &&
           snapshot->p6AuthorizationVerified;
}

static bool DomainRequestCoordinator_SameAdmissionContext(
    const DomainRequestCoordinator_ContextSnapshot_t *left,
    const DomainRequestCoordinator_ContextSnapshot_t *right)
{
    return (left->deviceContextId == right->deviceContextId) &&
           (left->sessionId == right->sessionId) &&
           (left->portEpoch == right->portEpoch) &&
           (left->contextRevision == right->contextRevision) &&
           (left->networkBootId == right->networkBootId);
}

static void DomainRequestCoordinator_DiscardInbox(
    DomainRequestCoordinator_t *coordinator)
{
    coordinator->admissionInvalidatedCount +=
        (uint32_t)coordinator->inboxCount;
    (void)memset(coordinator->inbox, 0, sizeof(coordinator->inbox));
    coordinator->inboxHead = 0U;
    coordinator->inboxCount = 0U;
}

static bool DomainRequestCoordinator_EvidenceMatches(
    const DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    const DomainRequestCoordinator_AdmissionEvidence_t *evidence,
    uint32_t nowMs)
{
    const DomainRequestCoordinator_ContextSnapshot_t *context =
        &coordinator->admissionContext;

    return coordinator->admissionEvidenceEnabled &&
           coordinator->admissionContextValid &&
           DomainRequestCoordinator_ContextAuthoritative(context) &&
           (evidence != NULL) && evidence->timestampKnown &&
           evidence->p6AuthorizationVerified &&
           (evidence->deviceContextId == request->deviceContextId) &&
           (evidence->sessionId == request->context.sessionId) &&
           (evidence->deviceContextId == context->deviceContextId) &&
           (evidence->sessionId == context->sessionId) &&
           (evidence->portEpoch == context->portEpoch) &&
           (evidence->contextRevision == context->contextRevision) &&
           (evidence->networkBootId == context->networkBootId) &&
           DomainRequestCoordinator_Fresh(
               nowMs, context->observedAtMs,
               coordinator->admissionEvidenceValidityMs) &&
           DomainRequestCoordinator_Fresh(
               nowMs, evidence->capturedAtMs,
               coordinator->admissionEvidenceValidityMs);
}

static bool DomainRequestCoordinator_LinkSequenceNewer(
    uint16_t current,
    uint16_t previous)
{
    uint16_t delta = (uint16_t)(current - previous);
    return (delta != 0U) && (delta < UINT16_C(0x8000));
}

static bool DomainRequestCoordinator_IsQualityValid(DataQuality_t quality)
{
    return (quality == DATA_QUALITY_OK) ||
           (quality == DATA_QUALITY_STALE) ||
           (quality == DATA_QUALITY_INVALID) ||
           (quality == DATA_QUALITY_NO_DATA);
}

static bool DomainRequestCoordinator_RequestIdentityValid(
    const DomainIf_MobileRequest_t *request)
{
    return (request != NULL) && (request->deviceContextId != 0U) &&
           (request->context.sessionId != 0U) &&
           (request->context.requestId != 0U) &&
           DomainRequestCoordinator_IsQualityValid(request->meta.quality);
}

static ResultManager_RequestKey_t DomainRequestCoordinator_RequestKey(
    const DomainIf_MobileRequest_t *request)
{
    ResultManager_RequestKey_t key;
    key.deviceContextId = request->deviceContextId;
    key.context = request->context;
    return key;
}

static DomainRequestCoordinator_Track_t *DomainRequestCoordinator_FindTrack(
    DomainRequestCoordinator_t *coordinator,
    const ResultManager_RequestKey_t *key)
{
    size_t index;
    for (index = 0U; index < coordinator->trackCapacity; ++index)
    {
        DomainRequestCoordinator_Track_t *track = &coordinator->tracks[index];
        if (track->inUse &&
            (track->request.deviceContextId == key->deviceContextId) &&
            (track->request.context.sessionId == key->context.sessionId) &&
            (track->request.context.requestId == key->context.requestId))
        {
            return track;
        }
    }
    return NULL;
}

static DomainRequestCoordinator_Track_t *DomainRequestCoordinator_FindFreeTrack(
    DomainRequestCoordinator_t *coordinator)
{
    size_t index;
    for (index = 0U; index < coordinator->trackCapacity; ++index)
    {
        if (!coordinator->tracks[index].inUse)
        {
            return &coordinator->tracks[index];
        }
    }
    return NULL;
}

static DomainRequestCoordinator_Track_t *DomainRequestCoordinator_FindCommand(
    DomainRequestCoordinator_t *coordinator,
    DomainCommandId_t commandId)
{
    size_t index;
    for (index = 0U; index < coordinator->trackCapacity; ++index)
    {
        DomainRequestCoordinator_Track_t *track = &coordinator->tracks[index];
        if (track->inUse && track->hasCommand &&
            (track->commandId == commandId))
        {
            return track;
        }
    }
    return NULL;
}

static bool DomainRequestCoordinator_MapQuality(
    DataQuality_t quality,
    VehicleStateQuality_t *outQuality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
            *outQuality = VEHICLE_STATE_QUALITY_OK;
            return true;
        case DATA_QUALITY_STALE:
            *outQuality = VEHICLE_STATE_QUALITY_STALE;
            return true;
        case DATA_QUALITY_INVALID:
            *outQuality = VEHICLE_STATE_QUALITY_INVALID;
            return true;
        case DATA_QUALITY_NO_DATA:
            *outQuality = VEHICLE_STATE_QUALITY_NO_DATA;
            return true;
        default:
            return false;
    }
}

static bool DomainRequestCoordinator_MapDoorRequest(
    const DomainIf_MobileRequest_t *request,
    DataQuality_t admittedQuality,
    RequestManager_Request_t *outRequest,
    CommandManager_CommandSpec_t *outSpec)
{
    VehicleStateQuality_t quality;

    if ((request->type != MOBILE_REQUEST_DOOR_LOCK) ||
        !DomainRequestCoordinator_MapQuality(admittedQuality, &quality))
    {
        return false;
    }
    if ((request->payload.doorTarget != DOOR_TARGET_LOCK) &&
        (request->payload.doorTarget != DOOR_TARGET_UNLOCK))
    {
        return false;
    }

    (void)memset(outRequest, 0, sizeof(*outRequest));
    (void)memset(outSpec, 0, sizeof(*outSpec));
    outRequest->deviceContextId = request->deviceContextId;
    outRequest->sessionId = request->context.sessionId;
    outRequest->kind = REQUEST_MANAGER_KIND_DOOR;
    outRequest->operation =
        (request->payload.doorTarget == DOOR_TARGET_LOCK)
            ? REQUEST_MANAGER_OPERATION_DOOR_LOCK
            : REQUEST_MANAGER_OPERATION_DOOR_UNLOCK;
    outRequest->quality = quality;
    outSpec->kind = COMMAND_MANAGER_KIND_DOOR;
    outSpec->target.door.target =
        (request->payload.doorTarget == DOOR_TARGET_LOCK)
            ? COMMAND_MANAGER_DOOR_TARGET_LOCK
            : COMMAND_MANAGER_DOOR_TARGET_UNLOCK;
    return true;
}

static ResultReason_t DomainRequestCoordinator_MapReason(
    RequestManager_Reason_t reason)
{
    switch (reason)
    {
        case REQUEST_MANAGER_REASON_DEVICE_NOT_REGISTERED:
            return RESULT_REASON_NOT_REGISTERED;
        case REQUEST_MANAGER_REASON_SESSION_UNAVAILABLE:
        case REQUEST_MANAGER_REASON_DEVICE_MISMATCH:
        case REQUEST_MANAGER_REASON_SESSION_MISMATCH:
            return RESULT_REASON_SESSION_INVALID;
        case REQUEST_MANAGER_REASON_REQUEST_STALE:
        case REQUEST_MANAGER_REASON_STATE_STALE:
            return RESULT_REASON_STALE;
        case REQUEST_MANAGER_REASON_REQUEST_NO_DATA:
        case REQUEST_MANAGER_REASON_STATE_NO_DATA:
            return RESULT_REASON_NO_DATA;
        case REQUEST_MANAGER_REASON_REQUEST_INVALID:
        case REQUEST_MANAGER_REASON_STATE_INVALID:
        case REQUEST_MANAGER_REASON_STATE_VALUE_UNKNOWN:
            return RESULT_REASON_STATE_UNTRUSTED;
        case REQUEST_MANAGER_REASON_DOOR_OPEN:
            return RESULT_REASON_DOOR_OPEN;
        case REQUEST_MANAGER_REASON_UNKNOWN_REQUEST_KIND:
        case REQUEST_MANAGER_REASON_UNSUPPORTED_OPERATION:
            return RESULT_REASON_UNKNOWN_REQUEST;
        case REQUEST_MANAGER_REASON_ARGUMENT_OUT_OF_RANGE:
            return RESULT_REASON_OUT_OF_RANGE;
        case REQUEST_MANAGER_REASON_REGISTRATION_UNAVAILABLE:
        case REQUEST_MANAGER_REASON_CONNECTION_UNAVAILABLE:
        case REQUEST_MANAGER_REASON_FUNCTION_SUPPORT_UNAVAILABLE:
        case REQUEST_MANAGER_REASON_PERMISSION_UNAVAILABLE:
        case REQUEST_MANAGER_REASON_POLICY_UNAVAILABLE:
            return RESULT_REASON_NOT_READY;
        case REQUEST_MANAGER_REASON_NOT_CONNECTED:
            return RESULT_REASON_LINK_LOST;
        case REQUEST_MANAGER_REASON_FUNCTION_UNSUPPORTED:
        case REQUEST_MANAGER_REASON_OPERATION_NOT_ALLOWED:
            return RESULT_REASON_NOT_ALLOWED;
        case REQUEST_MANAGER_REASON_NONE:
        default:
            return RESULT_REASON_OTHER;
    }
}

static CommandManager_SubmitOutcome_t DomainRequestCoordinator_MapSubmit(
    DomainIf_Status_t status)
{
    switch (status)
    {
        case DOMAIN_IF_OK:
            return COMMAND_MANAGER_SUBMIT_LOCALLY_ACCEPTED;
        case DOMAIN_IF_NOT_INITIALIZED:
        case DOMAIN_IF_NOT_AVAILABLE:
            return COMMAND_MANAGER_SUBMIT_FAILED_NOT_READY;
        case DOMAIN_IF_QUEUE_FULL:
            return COMMAND_MANAGER_SUBMIT_FAILED_NO_CAPACITY;
        case DOMAIN_IF_REJECTED:
        case DOMAIN_IF_UNSUPPORTED:
        case DOMAIN_IF_INVALID_ARGUMENT:
            return COMMAND_MANAGER_SUBMIT_FAILED_REJECTED;
        default:
            return COMMAND_MANAGER_SUBMIT_FAILED_OTHER;
    }
}

static DomainRequestCoordinator_Status_t DomainRequestCoordinator_Publish(
    DomainRequestCoordinator_t *coordinator,
    const ResultManager_RequestKey_t *key,
    uint32_t nowMs)
{
    DomainIf_MobileRequestResult_t result;
    ResultManager_Status_t managerStatus;
    DomainRequestCoordinator_Track_t *track = DomainRequestCoordinator_FindTrack(coordinator, key);

    if (track != NULL) track->publishPending = false;

    if (coordinator->ports.publishResult == NULL)
    {
        coordinator->lastPublishStatus = DOMAIN_IF_NOT_AVAILABLE;
        return DOMAIN_REQUEST_COORDINATOR_UNAVAILABLE;
    }
    managerStatus = ResultManager_GetMobileResult(
        coordinator->resultManager, key, nowMs, &result);
    if (managerStatus != RESULT_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    coordinator->lastPublishStatus = coordinator->ports.publishResult(
        coordinator->ports.context, &result);
    /* Queue Full promises no local handoff. Keep only a retry marker; the next
     * attempt reads the existing ResultManager again so identity/evidence and
     * elapsed result AGE stay authoritative. Other rejections are not retried. */
    if (track != NULL)
        track->publishPending = coordinator->lastPublishStatus == DOMAIN_IF_QUEUE_FULL;
    return (coordinator->lastPublishStatus == DOMAIN_IF_OK)
               ? DOMAIN_REQUEST_COORDINATOR_OK
               : DOMAIN_REQUEST_COORDINATOR_TRANSPORT_FAILED;
}

static void DomainRequestCoordinator_RetryPublish(
    DomainRequestCoordinator_t *coordinator,
    DomainRequestCoordinator_Track_t *track,
    uint32_t nowMs)
{
    RequestManager_AuthContext_t auth;
    RequestManager_Context_t vehicle;
    RequestManager_Policy_t policy;
    (void)memset(&auth, 0, sizeof(auth));
    (void)memset(&vehicle, 0, sizeof(vehicle));
    (void)memset(&policy, 0, sizeof(policy));
    /* Reuse the original trusted control Context Provider, never Monitoring.
     * A new Session cannot inherit a failed automatic result submission. */
    if (coordinator->ports.getContext == NULL ||
        !coordinator->ports.getContext(coordinator->ports.context,
            &track->originalRequest, nowMs, &auth, &vehicle, &policy) ||
        !auth.registrationVerified || !auth.registered ||
        !auth.connectionVerified || !auth.connected || !auth.sessionVerified ||
        auth.currentDeviceContextId != track->request.deviceContextId ||
        auth.currentSessionId != track->request.context.sessionId)
    {
        track->publishPending = false;
        coordinator->lastPublishStatus = DOMAIN_IF_REJECTED;
        return; /* Stored result remains available through protected Query. */
    }
    (void)DomainRequestCoordinator_Publish(coordinator, &track->request, nowMs);
}

static DomainRequestCoordinator_Status_t DomainRequestCoordinator_RecordReject(
    DomainRequestCoordinator_t *coordinator,
    const ResultManager_RequestKey_t *key,
    ResultReason_t reason,
    uint32_t nowMs)
{
    ResultManager_Status_t status = ResultManager_RecordDomainResult(
        coordinator->resultManager, key, REQUEST_RESULT_REJECTED, reason,
        true, nowMs);
    if ((status != RESULT_MANAGER_OK) && (status != RESULT_MANAGER_DUPLICATE))
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    (void)DomainRequestCoordinator_Publish(coordinator, key, nowMs);
    return DOMAIN_REQUEST_COORDINATOR_REJECTED;
}

static void DomainRequestCoordinator_BuildDoorCommand(
    const CommandManager_Intent_t *intent,
    DomainIf_BcmDoorCommand_t *outCommand)
{
    (void)memset(outCommand, 0, sizeof(*outCommand));
    outCommand->context.commandId = intent->commandId;
    outCommand->context.decisionSequence = intent->decisionSequence;
    outCommand->context.hasRequestContext = true;
    outCommand->context.requestContext.sessionId = intent->request.sessionId;
    outCommand->context.requestContext.requestId = intent->request.requestId;
    outCommand->context.requestDeviceContextId = intent->request.deviceContextId;
    outCommand->context.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    outCommand->context.commandAgeMs = 0U;
    outCommand->context.meta.quality = DATA_QUALITY_OK;
    outCommand->context.meta.ageMs = 0U;
    outCommand->context.meta.updateSequence = intent->decisionSequence;
    outCommand->target =
        (intent->target.door.target == COMMAND_MANAGER_DOOR_TARGET_LOCK)
            ? DOOR_TARGET_LOCK
            : DOOR_TARGET_UNLOCK;
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_Init(
    DomainRequestCoordinator_t *coordinator,
    CommandManager_Slot_t *commandSlots,
    size_t commandCapacity,
    ResultManager_Slot_t *resultSlots,
    size_t resultCapacity,
    DomainRequestCoordinator_Track_t *tracks,
    size_t trackCapacity,
    DomainCommandId_t firstCommandId,
    const ResultManager_Policy_t *resultPolicy,
    const DomainRequestCoordinator_Ports_t *ports)
{
    size_t index;

    if ((coordinator == NULL) || (commandSlots == NULL) ||
        (resultSlots == NULL) || (tracks == NULL) ||
        (commandCapacity == 0U) || (resultCapacity == 0U) ||
        (trackCapacity == 0U) || (trackCapacity > resultCapacity) ||
        (resultPolicy == NULL) || (ports == NULL) ||
        (ports->getContext == NULL))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }

    (void)memset(coordinator, 0, sizeof(*coordinator));
    for (index = 0U; index < trackCapacity; ++index)
    {
        (void)memset(&tracks[index], 0, sizeof(tracks[index]));
    }
    if (CommandManager_Init(&coordinator->ownedCommandManager, commandSlots,
                            commandCapacity, firstCommandId) != COMMAND_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    if (ResultManager_Init(&coordinator->ownedResultManager, resultSlots,
                           resultCapacity, resultPolicy) != RESULT_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    coordinator->tracks = tracks;
    coordinator->trackCapacity = trackCapacity;
    coordinator->commandManager = &coordinator->ownedCommandManager;
    coordinator->resultManager = &coordinator->ownedResultManager;
    coordinator->ports = *ports;
    coordinator->lastSubmissionStatus = DOMAIN_IF_NOT_AVAILABLE;
    coordinator->lastPublishStatus = DOMAIN_IF_NOT_AVAILABLE;
    coordinator->lastInboxStatus = DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    coordinator->initialized = true;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_Attach(
    DomainRequestCoordinator_t *coordinator,
    CommandManager_t *commandManager,
    ResultManager_t *resultManager,
    DomainRequestCoordinator_Track_t *tracks,
    size_t trackCapacity,
    const DomainRequestCoordinator_Ports_t *ports)
{
    size_t index;

    if ((coordinator == NULL) || (commandManager == NULL) ||
        (resultManager == NULL) || (tracks == NULL) ||
        (trackCapacity == 0U) || (ports == NULL) ||
        (ports->getContext == NULL))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    (void)memset(coordinator, 0, sizeof(*coordinator));
    for (index = 0U; index < trackCapacity; ++index)
    {
        (void)memset(&tracks[index], 0, sizeof(tracks[index]));
    }
    coordinator->commandManager = commandManager;
    coordinator->resultManager = resultManager;
    coordinator->tracks = tracks;
    coordinator->trackCapacity = trackCapacity;
    coordinator->ports = *ports;
    coordinator->lastSubmissionStatus = DOMAIN_IF_NOT_AVAILABLE;
    coordinator->lastPublishStatus = DOMAIN_IF_NOT_AVAILABLE;
    coordinator->lastInboxStatus = DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    coordinator->initialized = true;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

static DomainRequestCoordinator_Status_t
DomainRequestCoordinator_HandleMobileRequestWithQuality(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    DataQuality_t admittedQuality)
{
    ResultManager_RequestKey_t key;
    DomainRequestCoordinator_Track_t *track;
    RequestManager_Request_t internalRequest;
    RequestManager_AuthContext_t auth;
    RequestManager_Context_t vehicleContext;
    RequestManager_Policy_t policy;
    RequestManager_Decision_t decision;
    CommandManager_CommandSpec_t spec;
    CommandManager_Intent_t intent;
    DomainIf_BcmDoorCommand_t command;
    ResultManager_Status_t resultStatus;
    CommandManager_Status_t commandStatus;
    DomainIf_Status_t submitStatus;

    if ((coordinator == NULL) || !coordinator->initialized)
    {
        return (coordinator == NULL) ? DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT
                                     : DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    }
    if (!DomainRequestCoordinator_RequestIdentityValid(request))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }

    key = DomainRequestCoordinator_RequestKey(request);
    track = DomainRequestCoordinator_FindTrack(coordinator, &key);
    if (track != NULL)
    {
        resultStatus = ResultManager_RegisterRequest(
            coordinator->resultManager, request, nowMs);
        return (resultStatus == RESULT_MANAGER_DUPLICATE)
                   ? DOMAIN_REQUEST_COORDINATOR_DUPLICATE
                   : DOMAIN_REQUEST_COORDINATOR_ID_CONFLICT;
    }
    track = DomainRequestCoordinator_FindFreeTrack(coordinator);
    if (track == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED;
    }

    resultStatus = ResultManager_RegisterRequest(
        coordinator->resultManager, request, nowMs);
    if (resultStatus == RESULT_MANAGER_ID_CONFLICT)
    {
        return DOMAIN_REQUEST_COORDINATOR_ID_CONFLICT;
    }
    if (resultStatus != RESULT_MANAGER_OK)
    {
        return (resultStatus == RESULT_MANAGER_CAPACITY_EXCEEDED)
                   ? DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED
                   : DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    (void)memset(track, 0, sizeof(*track));
    track->inUse = true;
    track->request = key;
    track->originalRequest = *request;

    if (!DomainRequestCoordinator_MapDoorRequest(request, admittedQuality,
                                                  &internalRequest, &spec))
    {
        return DomainRequestCoordinator_RecordReject(
            coordinator, &key, RESULT_REASON_UNKNOWN_REQUEST, nowMs);
    }

    (void)memset(&auth, 0, sizeof(auth));
    (void)memset(&vehicleContext, 0, sizeof(vehicleContext));
    (void)memset(&policy, 0, sizeof(policy));
    if (!coordinator->ports.getContext(coordinator->ports.context, request,
                                       nowMs, &auth, &vehicleContext, &policy))
    {
        return DomainRequestCoordinator_RecordReject(
            coordinator, &key, RESULT_REASON_NOT_READY, nowMs);
    }
    if (RequestManager_Evaluate(&internalRequest, &auth, &vehicleContext,
                                &policy, &decision) != REQUEST_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    if (decision.verdict != REQUEST_MANAGER_VERDICT_ALLOW)
    {
        return DomainRequestCoordinator_RecordReject(
            coordinator, &key, DomainRequestCoordinator_MapReason(decision.reason),
            nowMs);
    }

    if (decision.action == REQUEST_MANAGER_ACTION_NO_COMMAND_REQUIRED)
    {
        if (ResultManager_RecordDomainResult(
                coordinator->resultManager, &key, REQUEST_RESULT_ACCEPTED,
                RESULT_REASON_NONE, true, nowMs) != RESULT_MANAGER_OK)
        {
            return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
        }
        if (ResultManager_RecordDomainResult(
                coordinator->resultManager, &key, REQUEST_RESULT_DONE,
                RESULT_REASON_ALREADY_AT_TARGET, true, nowMs) != RESULT_MANAGER_OK)
        {
            return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
        }
        return DomainRequestCoordinator_Publish(coordinator, &key, nowMs);
    }

    commandStatus = CommandManager_CreateIntent(
        coordinator->commandManager, &internalRequest,
        request->context.requestId, &decision, &spec, nowMs, &intent);
    if (commandStatus != COMMAND_MANAGER_OK)
    {
        return (commandStatus == COMMAND_MANAGER_DUPLICATE_REQUEST)
                   ? DOMAIN_REQUEST_COORDINATOR_DUPLICATE
                   : DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    if (ResultManager_RecordDomainResult(
            coordinator->resultManager, &key, REQUEST_RESULT_ACCEPTED,
            RESULT_REASON_NONE, true, nowMs) != RESULT_MANAGER_OK)
    {
        (void)CommandManager_Release(coordinator->commandManager,
                                     intent.commandId);
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    if (ResultManager_BindCommand(coordinator->resultManager, &key,
                                  &intent) != RESULT_MANAGER_OK)
    {
        (void)CommandManager_Release(coordinator->commandManager,
                                     intent.commandId);
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    track->hasCommand = true;
    track->commandId = intent.commandId;
    (void)DomainRequestCoordinator_Publish(coordinator, &key, nowMs);

    if ((coordinator->ports.isCommandTransportAvailable == NULL) ||
        !coordinator->ports.isCommandTransportAvailable(coordinator->ports.context) ||
        (coordinator->ports.submitDoorCommand == NULL))
    {
        submitStatus = DOMAIN_IF_NOT_AVAILABLE;
    }
    else
    {
        DomainRequestCoordinator_BuildDoorCommand(&intent, &command);
        submitStatus = coordinator->ports.submitDoorCommand(
            coordinator->ports.context, &command);
    }
    coordinator->lastSubmissionStatus = submitStatus;
    if (CommandManager_RecordSubmission(
            coordinator->commandManager, intent.commandId,
            DomainRequestCoordinator_MapSubmit(submitStatus), nowMs) !=
        COMMAND_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
    }
    if (submitStatus != DOMAIN_IF_OK)
    {
        ++coordinator->submissionFailureCount;
        return (submitStatus == DOMAIN_IF_NOT_AVAILABLE)
                   ? DOMAIN_REQUEST_COORDINATOR_UNAVAILABLE
                   : DOMAIN_REQUEST_COORDINATOR_TRANSPORT_FAILED;
    }
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_HandleMobileRequest(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs)
{
    if (request == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    return DomainRequestCoordinator_HandleMobileRequestWithQuality(
        coordinator, request, nowMs, request->meta.quality);
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_HandleBcmResult(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs)
{
    DomainRequestCoordinator_Track_t *track;
    ResultManager_Status_t status;

    if ((coordinator == NULL) || !coordinator->initialized)
    {
        return (coordinator == NULL) ? DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT
                                     : DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    }
    if (result == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    track = DomainRequestCoordinator_FindCommand(coordinator, result->commandId);
    if (track == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    status = ResultManager_ApplyBcmResult(coordinator->resultManager,
                                          result, nowMs);
    if ((status == RESULT_MANAGER_DUPLICATE) ||
        (status == RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED))
    {
        return DOMAIN_REQUEST_COORDINATOR_DUPLICATE;
    }
    if (status == RESULT_MANAGER_UNCONFIRMED_RECORDED)
    {
        (void)DomainRequestCoordinator_Publish(coordinator, &track->request, nowMs);
        return DOMAIN_REQUEST_COORDINATOR_UNAVAILABLE;
    }
    if (status != RESULT_MANAGER_OK)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    return DomainRequestCoordinator_OnBcmResultCommitted(
        coordinator, result, nowMs);
}

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_OnBcmResultCommitted(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs)
{
    DomainRequestCoordinator_Track_t *track;
    ResultManager_Snapshot_t snapshot;

    if ((coordinator == NULL) || !coordinator->initialized)
    {
        return (coordinator == NULL) ? DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT
                                     : DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    }
    if (result == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    track = DomainRequestCoordinator_FindCommand(coordinator, result->commandId);
    if (track == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    (void)DomainRequestCoordinator_Publish(coordinator, &track->request, nowMs);
    if ((ResultManager_GetSnapshot(coordinator->resultManager, &track->request,
                                   &snapshot) == RESULT_MANAGER_OK) &&
        snapshot.terminal &&
        (snapshot.confirmation == RESULT_CONFIRMATION_CONFIRMED))
    {
        (void)CommandManager_Release(coordinator->commandManager,
                                     track->commandId);
    }
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainIf_Status_t DomainRequestCoordinator_OnBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result)
{
    DomainRequestCoordinator_Status_t status;
    uint32_t nowMs;

    if (result == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!g_domainRequestIngress.configured)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if (!g_domainRequestIngress.getNowMs(
            g_domainRequestIngress.timeContext, &nowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    status = DomainRequestCoordinator_OnBcmResultCommitted(
        g_domainRequestIngress.coordinator, result, nowMs);
    if ((status == DOMAIN_REQUEST_COORDINATOR_OK) ||
        (status == DOMAIN_REQUEST_COORDINATOR_DUPLICATE))
    {
        return DOMAIN_IF_OK;
    }
    if (status == DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (status == DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED)
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    return DOMAIN_IF_REJECTED;
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_Poll(
    DomainRequestCoordinator_t *coordinator,
    uint32_t nowMs,
    size_t *outTimeoutCount)
{
    size_t index;
    size_t processed = 0U;
    size_t count = 0U;

    if ((coordinator == NULL) || !coordinator->initialized)
    {
        return (coordinator == NULL) ? DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT
                                     : DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    }
    if (outTimeoutCount == NULL)
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    if (coordinator->pollActive)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    coordinator->pollActive = true;
    while ((processed < DOMAIN_REQUEST_COORDINATOR_POLL_BUDGET) &&
           (coordinator->inboxCount != 0U))
    {
        DomainRequestCoordinator_InboxEntry_t *entry =
            &coordinator->inbox[coordinator->inboxHead];
        DomainIf_MobileRequest_t request = entry->request;
        DomainRequestCoordinator_AdmissionEvidence_t admission =
            entry->admission;
        bool hasAdmissionEvidence = entry->hasAdmissionEvidence;
        uint32_t elapsedMs = (uint32_t)(nowMs - entry->enqueuedAtMs);

        coordinator->inboxHead =
            (coordinator->inboxHead + 1U) %
            DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
        coordinator->inboxCount -= 1U;
        if (hasAdmissionEvidence)
        {
            if (!DomainRequestCoordinator_Fresh(
                    nowMs, coordinator->admissionContext.observedAtMs,
                    coordinator->admissionEvidenceValidityMs))
            {
                (void)DomainRequestCoordinator_InvalidateAdmissionContext(
                    coordinator,
                    DOMAIN_REQUEST_ADMISSION_INVALIDATION_FRESHNESS_EXPIRED);
                coordinator->lastInboxStatus =
                    DOMAIN_REQUEST_COORDINATOR_REJECTED;
            }
            else if (!DomainRequestCoordinator_EvidenceMatches(
                         coordinator, &request, &admission, nowMs))
            {
                coordinator->admissionInvalidatedCount += 1U;
                coordinator->lastInboxStatus =
                    DOMAIN_REQUEST_COORDINATOR_REJECTED;
            }
            else
            {
                coordinator->lastInboxStatus =
                    DomainRequestCoordinator_HandleMobileRequestWithQuality(
                        coordinator, &request, nowMs, DATA_QUALITY_OK);
            }
        }
        else
        {
            request.meta.ageMs = DomainRequestCoordinator_SaturatingAdd(
                request.meta.ageMs, elapsedMs);
            if (request.meta.ageMs >= coordinator->inboxSourceValidityMs)
            {
                request.meta.quality = DATA_QUALITY_STALE;
            }
            coordinator->lastInboxStatus =
                DomainRequestCoordinator_HandleMobileRequest(
                    coordinator, &request, nowMs);
        }
        coordinator->inboxProcessedCount += 1U;
        processed += 1U;
    }
    for (index = 0U; index < coordinator->trackCapacity; ++index)
    {
        ResultManager_TimeoutState_t timeout;
        bool retried;
        if (!coordinator->tracks[index].inUse)
        {
            continue;
        }
        retried = coordinator->tracks[index].publishPending;
        if (retried)
            DomainRequestCoordinator_RetryPublish(coordinator, &coordinator->tracks[index], nowMs);
        if (ResultManager_EvaluateTimeout(
                coordinator->resultManager,
                &coordinator->tracks[index].request,
                nowMs, &timeout) != RESULT_MANAGER_OK)
        {
            coordinator->pollActive = false;
            return DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR;
        }
        if (timeout != RESULT_MANAGER_TIMEOUT_NONE)
        {
            ++count;
            if (!retried)
                (void)DomainRequestCoordinator_Publish(
                    coordinator, &coordinator->tracks[index].request, nowMs);
        }
    }
    *outTimeoutCount = count;
    coordinator->pollActive = false;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_ConfigureAdmissionEvidence(
    DomainRequestCoordinator_t *coordinator,
    uint32_t evidenceValidityMs)
{
    if ((coordinator == NULL) || !coordinator->initialized ||
        (evidenceValidityMs == 0U))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    (void)memset(&coordinator->admissionContext, 0,
                 sizeof(coordinator->admissionContext));
    coordinator->admissionEvidenceValidityMs = evidenceValidityMs;
    coordinator->admissionContextValid = false;
    coordinator->admissionEvidenceEnabled = true;
    coordinator->admissionSequenceSeen = false;
    coordinator->lastAdmissionInvalidation =
        DOMAIN_REQUEST_ADMISSION_INVALIDATION_NONE;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_UpdateAdmissionContext(
    DomainRequestCoordinator_t *coordinator,
    const DomainRequestCoordinator_ContextSnapshot_t *snapshot)
{
    DomainRequestCoordinator_AdmissionInvalidation_t reason =
        DOMAIN_REQUEST_ADMISSION_INVALIDATION_NONE;

    if ((coordinator == NULL) || !coordinator->initialized ||
        !coordinator->admissionEvidenceEnabled || (snapshot == NULL))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    if (!DomainRequestCoordinator_ContextAuthoritative(snapshot))
    {
        reason = snapshot->linkUp
                     ? DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED
                     : DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN;
        (void)DomainRequestCoordinator_InvalidateAdmissionContext(coordinator,
                                                                  reason);
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    if (coordinator->admissionContextValid &&
        !DomainRequestCoordinator_SameAdmissionContext(
            &coordinator->admissionContext, snapshot))
    {
        if (coordinator->admissionContext.networkBootId !=
            snapshot->networkBootId)
        {
            reason = DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED;
        }
        else if (coordinator->admissionContext.portEpoch != snapshot->portEpoch)
        {
            reason = DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED;
        }
        else
        {
            reason = DOMAIN_REQUEST_ADMISSION_INVALIDATION_CONTEXT_CHANGED;
        }
        DomainRequestCoordinator_DiscardInbox(coordinator);
        coordinator->admissionSequenceSeen = false;
        coordinator->lastAdmissionInvalidation = reason;
    }
    coordinator->admissionContext = *snapshot;
    coordinator->admissionContextValid = true;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_InvalidateAdmissionContext(
    DomainRequestCoordinator_t *coordinator,
    DomainRequestCoordinator_AdmissionInvalidation_t reason)
{
    if ((coordinator == NULL) || !coordinator->initialized ||
        (reason == DOMAIN_REQUEST_ADMISSION_INVALIDATION_NONE))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    DomainRequestCoordinator_DiscardInbox(coordinator);
    (void)memset(&coordinator->admissionContext, 0,
                 sizeof(coordinator->admissionContext));
    coordinator->admissionContextValid = false;
    coordinator->admissionSequenceSeen = false;
    coordinator->lastAdmissionInvalidation = reason;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    const DomainRequestCoordinator_AdmissionEvidence_t *evidence,
    uint32_t nowMs)
{
    size_t index;
    DomainRequestCoordinator_InboxEntry_t *entry;

    if ((coordinator == NULL) || !coordinator->initialized ||
        !coordinator->admissionEvidenceEnabled ||
        !DomainRequestCoordinator_RequestIdentityValid(request) ||
        (evidence == NULL))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    if (coordinator->pollActive)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    /* The evidence path must not disguise link facts as source SignalMeta. */
    if ((request->meta.quality != DATA_QUALITY_NO_DATA) ||
        (request->meta.ageMs != UINT32_MAX) ||
        (request->meta.updateSequence != 0U) ||
        !DomainRequestCoordinator_EvidenceMatches(
            coordinator, request, evidence, nowMs))
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    if (coordinator->admissionSequenceSeen)
    {
        if (evidence->linkSequence == coordinator->lastAdmissionLinkSequence)
        {
            return DOMAIN_REQUEST_COORDINATOR_DUPLICATE;
        }
        if (!DomainRequestCoordinator_LinkSequenceNewer(
                evidence->linkSequence,
                coordinator->lastAdmissionLinkSequence))
        {
            return DOMAIN_REQUEST_COORDINATOR_REJECTED;
        }
    }
    for (index = 0U; index < coordinator->inboxCount; ++index)
    {
        size_t slot = (coordinator->inboxHead + index) %
                      DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
        const DomainIf_MobileRequest_t *queued =
            &coordinator->inbox[slot].request;
        if (DomainRequestCoordinator_SameIdentity(queued, request))
        {
            return DomainRequestCoordinator_SamePayload(queued, request)
                       ? DOMAIN_REQUEST_COORDINATOR_DUPLICATE
                       : DOMAIN_REQUEST_COORDINATOR_ID_CONFLICT;
        }
    }
    if (coordinator->inboxCount >= DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY)
    {
        return DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED;
    }
    index = (coordinator->inboxHead + coordinator->inboxCount) %
            DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
    entry = &coordinator->inbox[index];
    entry->request = *request;
    entry->enqueuedAtMs = nowMs;
    entry->admission = *evidence;
    entry->hasAdmissionEvidence = true;
    coordinator->inboxCount += 1U;
    coordinator->inboxAcceptedCount += 1U;
    coordinator->lastAdmissionLinkSequence = evidence->linkSequence;
    coordinator->admissionSequenceSeen = true;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

DomainRequestCoordinator_Status_t DomainRequestCoordinator_BindIngress(
    DomainRequestCoordinator_t *coordinator,
    DomainRequestCoordinator_TimeProvider_t getNowMs,
    void *timeContext,
    uint32_t sourceValidityMs)
{
    if ((coordinator == NULL) || !coordinator->initialized ||
        (getNowMs == NULL) || (sourceValidityMs == 0U))
    {
        return DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
    }
    if (g_domainRequestIngress.configured)
    {
        return DOMAIN_REQUEST_COORDINATOR_REJECTED;
    }
    g_domainRequestIngress.coordinator = coordinator;
    g_domainRequestIngress.getNowMs = getNowMs;
    g_domainRequestIngress.timeContext = timeContext;
    coordinator->inboxSourceValidityMs = sourceValidityMs;
    coordinator->lastInboxStatus = DOMAIN_REQUEST_COORDINATOR_OK;
    g_domainRequestIngress.configured = true;
    return DOMAIN_REQUEST_COORDINATOR_OK;
}

void DomainRequestCoordinator_ResetBinding(void)
{
    (void)memset(&g_domainRequestIngress, 0, sizeof(g_domainRequestIngress));
}

DomainIf_Status_t DomainIf_RxMobileRequest(
    const DomainIf_MobileRequest_t *request)
{
    DomainRequestCoordinator_t *coordinator;
    size_t index;
    uint32_t nowMs;

    if (request == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_domainRequestIngress.configured)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    coordinator = g_domainRequestIngress.coordinator;
    if (coordinator->pollActive)
    {
        return DOMAIN_IF_REJECTED;
    }
    if (!DomainRequestCoordinator_RequestIdentityValid(request))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainRequestCoordinator_SourceAdmissible(
            request, coordinator->inboxSourceValidityMs))
    {
        return DOMAIN_IF_REJECTED;
    }
    if (!g_domainRequestIngress.getNowMs(
            g_domainRequestIngress.timeContext, &nowMs))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    for (index = 0U; index < coordinator->inboxCount; ++index)
    {
        size_t slot = (coordinator->inboxHead + index) %
                      DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
        const DomainIf_MobileRequest_t *queued =
            &coordinator->inbox[slot].request;
        if (DomainRequestCoordinator_SameIdentity(queued, request))
        {
            return DomainRequestCoordinator_SamePayload(queued, request)
                       ? DOMAIN_IF_OK
                       : DOMAIN_IF_REJECTED;
        }
    }
    if (coordinator->inboxCount >= DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY)
    {
        return DOMAIN_IF_QUEUE_FULL;
    }
    index = (coordinator->inboxHead + coordinator->inboxCount) %
            DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY;
    coordinator->inbox[index].request = *request;
    coordinator->inbox[index].enqueuedAtMs = nowMs;
    (void)memset(&coordinator->inbox[index].admission, 0,
                 sizeof(coordinator->inbox[index].admission));
    coordinator->inbox[index].hasAdmissionEvidence = false;
    coordinator->inboxCount += 1U;
    coordinator->inboxAcceptedCount += 1U;
    return DOMAIN_IF_OK;
}
