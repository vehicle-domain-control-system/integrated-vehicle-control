#include "domain_runtime.h"
#include "domain_snapshot_commit_private.h"

#include <string.h>

#define DOMAIN_RUNTIME_INTEGRITY_MARKER UINT32_C(0x4452544D)

static bool DomainRuntime_HasMarker(const DomainRuntime_t *runtime)
{
    uint32_t marker;

    if (runtime == NULL)
    {
        return false;
    }

    /* Character-copy inspection is safe for a caller-provided zeroed object. */
    (void)memcpy(&marker, &runtime->integrityMarker, sizeof(marker));
    return marker == DOMAIN_RUNTIME_INTEGRITY_MARKER;
}

static bool DomainRuntime_GetTime(
    const DomainRuntime_t *runtime,
    uint32_t *outNowMs)
{
    return (runtime->timeProvider != NULL) &&
           runtime->timeProvider(runtime->timeProviderContext, outNowMs);
}

static bool DomainRuntime_MapDoorQuality(
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

static bool DomainRuntime_RequestContextProvider(
    void *context,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_AuthContext_t *outAuth,
    RequestManager_Context_t *outVehicleContext,
    RequestManager_Policy_t *outPolicy)
{
    DomainRuntime_t *runtime = (DomainRuntime_t *)context;
    VehicleStateManager_Snapshot_t snapshot;
    DoorState_t door;
    VehicleStateManager_Status_t stateStatus;
    RequestManager_FunctionGate_t doorGate;
    VehicleStateQuality_t aggregateQuality;

    if ((runtime == NULL) || (request == NULL) || (outAuth == NULL) ||
        (outVehicleContext == NULL) || (outPolicy == NULL) ||
        !DomainRuntime_IsReady(runtime) ||
        (runtime->requestAuthorizationProvider == NULL))
    {
        return false;
    }
    if (!runtime->requestAuthorizationProvider(
            runtime->requestAuthorizationContext, request, outAuth, &doorGate))
    {
        return false;
    }

    (void)memset(&door, 0, sizeof(door));
    (void)memset(&snapshot, 0, sizeof(snapshot));
    (void)memset(outVehicleContext, 0, sizeof(*outVehicleContext));
    (void)memset(outPolicy, 0, sizeof(*outPolicy));
    outVehicleContext->door = doorGate;

    stateStatus = VehicleStateManager_GetSnapshot(
        &runtime->vehicleStateManager,
        DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR,
        nowMs,
        runtime->bcmDoorStateTtlMs,
        &door,
        sizeof(door),
        &snapshot);
    if (stateStatus == VEHICLE_STATE_MANAGER_NO_DATA)
    {
        outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCK_UNKNOWN;
        outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
        outVehicleContext->doorLockQuality = VEHICLE_STATE_QUALITY_NO_DATA;
        outVehicleContext->doorOpenQuality = VEHICLE_STATE_QUALITY_NO_DATA;
        return true;
    }
    if (stateStatus != VEHICLE_STATE_MANAGER_OK)
    {
        return false;
    }
    aggregateQuality = snapshot.quality;
    if (aggregateQuality != VEHICLE_STATE_QUALITY_OK)
    {
        outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCK_UNKNOWN;
        outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
        outVehicleContext->doorLockQuality = aggregateQuality;
        outVehicleContext->doorOpenQuality = aggregateQuality;
        return true;
    }
    if (!DomainRuntime_MapDoorQuality(door.compositeQuality,
                                      &aggregateQuality) ||
        (door.compositeState != DOOR_COMPOSITE_STATE_NORMAL))
    {
        outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCK_UNKNOWN;
        outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
        outVehicleContext->doorLockQuality = VEHICLE_STATE_QUALITY_INVALID;
        outVehicleContext->doorOpenQuality = VEHICLE_STATE_QUALITY_INVALID;
        return true;
    }
    if (aggregateQuality != VEHICLE_STATE_QUALITY_OK)
    {
        outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCK_UNKNOWN;
        outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
        outVehicleContext->doorLockQuality = aggregateQuality;
        outVehicleContext->doorOpenQuality = aggregateQuality;
        return true;
    }
    if (!DomainRuntime_MapDoorQuality(door.lockQuality,
                                      &outVehicleContext->doorLockQuality) ||
        !DomainRuntime_MapDoorQuality(door.openQuality,
                                      &outVehicleContext->doorOpenQuality))
    {
        return false;
    }
    switch (door.lockState)
    {
        case DOOR_LOCK_STATE_LOCKED:
            outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCKED;
            break;
        case DOOR_LOCK_STATE_UNLOCKED:
            outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_UNLOCKED;
            break;
        case DOOR_LOCK_STATE_UNKNOWN:
        default:
            outVehicleContext->doorLockState = REQUEST_MANAGER_DOOR_LOCK_UNKNOWN;
            break;
    }
    switch (door.openState)
    {
        case DOOR_OPEN_STATE_CLOSED:
            outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_CLOSED;
            break;
        case DOOR_OPEN_STATE_OPEN:
            outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN;
            break;
        case DOOR_OPEN_STATE_UNKNOWN:
        default:
            outVehicleContext->doorOpenState = REQUEST_MANAGER_DOOR_OPEN_UNKNOWN;
            break;
    }
    return true;
}

static bool DomainRuntime_RequestTransportAvailable(void *context)
{
    return DomainRuntime_IsReady((const DomainRuntime_t *)context);
}

static DomainIf_Status_t DomainRuntime_SubmitDoorCommand(
    void *context,
    const DomainIf_BcmDoorCommand_t *command)
{
    (void)context;
    return DomainIf_TxBcmDoorCommand(command);
}

static DomainIf_Status_t DomainRuntime_PublishRequestResult(
    void *context,
    const DomainIf_MobileRequestResult_t *result)
{
    (void)context;
    return DomainIf_TxMobileRequestResult(result);
}

static void DomainRuntime_PrepareVehicleStateStorage(
    DomainRuntime_t *runtime)
{
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR]
        .valueStorage = &runtime->vehicleStateStorage.bcmDoor;
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR]
        .valueCapacity = sizeof(runtime->vehicleStateStorage.bcmDoor);

    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_BCM_STATUS]
        .valueStorage = &runtime->vehicleStateStorage.bcmStatus;
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_BCM_STATUS]
        .valueCapacity = sizeof(runtime->vehicleStateStorage.bcmStatus);

    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_CIS_REAR]
        .valueStorage = &runtime->vehicleStateStorage.cisRear;
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_CIS_REAR]
        .valueCapacity = sizeof(runtime->vehicleStateStorage.cisRear);

    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_CIS_STATUS]
        .valueStorage = &runtime->vehicleStateStorage.cisStatus;
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_CIS_STATUS]
        .valueCapacity = sizeof(runtime->vehicleStateStorage.cisStatus);

    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_VSS_STATUS]
        .valueStorage = &runtime->vehicleStateStorage.vssStatus;
    runtime->vehicleStateSlotConfigs[DOMAIN_RUNTIME_STATE_SLOT_VSS_STATUS]
        .valueCapacity = sizeof(runtime->vehicleStateStorage.vssStatus);

    runtime->vehicleStateSlotMap.bcmDoorState =
        DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR;
    runtime->vehicleStateSlotMap.bcmStatus =
        DOMAIN_RUNTIME_STATE_SLOT_BCM_STATUS;
    runtime->vehicleStateSlotMap.cisRearState =
        DOMAIN_RUNTIME_STATE_SLOT_CIS_REAR;
    runtime->vehicleStateSlotMap.cisStatus =
        DOMAIN_RUNTIME_STATE_SLOT_CIS_STATUS;
    runtime->vehicleStateSlotMap.vssStatus =
        DOMAIN_RUNTIME_STATE_SLOT_VSS_STATUS;
}

static DomainRuntime_Status_t DomainRuntime_InitManagers(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config)
{
    if (VehicleStateManager_Init(
            &runtime->vehicleStateManager,
            runtime->vehicleStateSlots,
            runtime->vehicleStateSlotConfigs,
            DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT) !=
        VEHICLE_STATE_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_VEHICLE_STATE_MANAGER;

    if (CommandManager_Init(
            &runtime->commandManager,
            runtime->commandSlots,
            DOMAIN_RUNTIME_COMMAND_CAPACITY,
            config->firstCommandId) != COMMAND_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_COMMAND_MANAGER;

    if (ResultManager_Init(
            &runtime->resultManager,
            runtime->resultSlots,
            DOMAIN_RUNTIME_RESULT_CAPACITY,
            &config->resultPolicy) != RESULT_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_RESULT_MANAGER;

    if (VssWarningManager_Init(
            &runtime->vssWarningManager, &runtime->vehicleStateManager,
            runtime->vehicleStateSlotMap.cisRearState,
            DomainIf_TxVssWarningState) != VSS_WARNING_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_VSS_WARNING_MANAGER;

    if (VssEventManager_Init(
            &runtime->vssEventManager, DomainIf_TxVssOneShotEvent) !=
        VSS_EVENT_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    if ((config->vssEventGenerationProvider != NULL) &&
        (VssEventManager_BindGenerationProvider(
             &runtime->vssEventManager,
             config->vssEventGenerationProvider,
             config->vssEventGenerationProviderContext) !=
         VSS_EVENT_MANAGER_OK))
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_VSS_EVENT_MANAGER;

    if (PermissionManager_Init(
            &runtime->permissionManager,
            &config->permissionPolicy) != PERMISSION_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_PERMISSION_MANAGER;

    if (config->permissionCoordinatorEnabled &&
        (PermissionCoordinator_Init(
             &runtime->permissionCoordinator,
             &runtime->permissionManager,
             config->permissionEvaluator,
             config->permissionEvaluatorContext) !=
         PERMISSION_COORDINATOR_OK))
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }

    if ((config->cisOccupantEnabled || config->cisOccupantStorageEnabled) &&
        (CisOccupantManager_Init(
             &runtime->cisOccupantManager,
             &config->cisOccupantPolicy) != CIS_OCCUPANT_MANAGER_OK))
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    if (config->cisOccupantEnabled || config->cisOccupantStorageEnabled)
    {
        runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_MANAGER;
    }

    if ((config->cisOccupantEnabled || config->cisOccupantStorageEnabled) &&
        (DomainCisOccupantPriv_Init(
             &runtime->cisOccupantPrivate,
             &runtime->cisOccupantManager) != DOMAIN_CIS_OCCUPANT_PRIV_OK))
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    if (config->cisOccupantEnabled || config->cisOccupantStorageEnabled)
    {
        runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_PRIVATE;
    }
    return DOMAIN_RUNTIME_OK;
}

static DomainRuntime_Status_t DomainRuntime_InitC2AndBindings(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config)
{
    DomainRequestCoordinator_Ports_t requestPorts;
    DomainIf_RxObserver_t observer;
    DomainIfQueryBinding_StatePolicy_t statePolicy = config->stateQueryPolicy;
    if (statePolicy.cisRearTtlMs == 0U)
        statePolicy.cisRearTtlMs = VSS_WARNING_MANAGER_REAR_TTL_MS;
    if (DomainIf_Init() != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_C2_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_C2_CORE;

    /*
     * Init is permitted only while Network ingress is closed. Explicitly
     * unbinding both tables makes partial initialization fail closed and does
     * not create Network callbacks in Application code.
     */
    if ((DomainIf_SetRxObserver(NULL) != DOMAIN_IF_OK) ||
        (DomainIf_SetTxPort(NULL) != DOMAIN_IF_OK))
    {
        return DOMAIN_RUNTIME_C2_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_C2_UNBOUND;

    if (DomainIfStateBinding_Configure(
            &runtime->vehicleStateManager,
            &runtime->vehicleStateSlotMap,
            runtime->timeProvider,
            runtime->timeProviderContext) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_STATE_BINDING;

    if (DomainIfResultBinding_Configure(
            &runtime->resultManager,
            runtime->timeProvider,
            runtime->timeProviderContext) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_RESULT_BINDING;

    if (DomainIfResultBinding_ConfigureVssEvents(
            &runtime->vssEventManager) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }

    if (config->requestCoordinatorEnabled)
    {
        (void)memset(&requestPorts, 0, sizeof(requestPorts));
        requestPorts.getContext = DomainRuntime_RequestContextProvider;
        requestPorts.isCommandTransportAvailable =
            DomainRuntime_RequestTransportAvailable;
        requestPorts.submitDoorCommand = DomainRuntime_SubmitDoorCommand;
        requestPorts.publishResult = DomainRuntime_PublishRequestResult;
        requestPorts.context = runtime;
        if (DomainRequestCoordinator_Attach(
                &runtime->requestCoordinator,
                &runtime->commandManager,
                &runtime->resultManager,
                runtime->requestTracks,
                DOMAIN_RUNTIME_REQUEST_TRACK_CAPACITY,
                &requestPorts) != DOMAIN_REQUEST_COORDINATOR_OK)
        {
            return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
        }
        if (DomainRequestCoordinator_BindIngress(
                &runtime->requestCoordinator,
                runtime->timeProvider,
                runtime->timeProviderContext,
                runtime->mobileRequestSourceValidityMs) !=
            DOMAIN_REQUEST_COORDINATOR_OK)
        {
            return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
        }
        (void)memset(&observer, 0, sizeof(observer));
        observer.onBcmCommandResult =
            DomainRequestCoordinator_OnBcmCommandResult;
        if (DomainIf_SetRxObserver(&observer) != DOMAIN_IF_OK)
        {
            return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
        }
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_REQUEST_COORDINATOR;

    if (DomainIfQueryBinding_ConfigureState(
            &runtime->vehicleStateManager,
            &runtime->vehicleStateSlotMap,
            &statePolicy) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_QUERY_STATE_BINDING;

    if (DomainIfQueryBinding_ConfigureResults(
            &runtime->resultManager) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_QUERY_RESULT_BINDING;

    if (config->cisOccupantEnabled || config->cisOccupantStorageEnabled)
    {
        if (DomainIfCisOccupantBinding_Configure(
                &runtime->cisOccupantManager,
                &runtime->cisOccupantPrivate,
                runtime->timeProvider,
                runtime->timeProviderContext) != DOMAIN_IF_OK)
        {
            return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
        }
    }
    else if (DomainIfCisOccupantBinding_Disable() != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_BINDING;

    if (DomainIfCisEnvironmentBinding_Initialize(
            &runtime->cisEnvironmentManager,
            &runtime->cisEnvironmentPrivate,
            runtime->timeProvider,
            runtime->timeProviderContext) != DOMAIN_IF_OK)
    {
        return DOMAIN_RUNTIME_BINDING_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_ENVIRONMENT_BINDING;
    return DOMAIN_RUNTIME_OK;
}

DomainRuntime_Status_t DomainRuntime_Init(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config)
{
    DomainRuntime_Status_t status;
    uint32_t initialNowMs;

    if ((runtime == NULL) || (config == NULL) ||
        (config->timeProvider == NULL) ||
        (config->requestCoordinatorEnabled &&
         ((config->bcmDoorStateTtlMs == 0U) ||
          (config->mobileRequestSourceValidityMs == 0U))))
    {
        return DOMAIN_RUNTIME_INVALID_ARGUMENT;
    }
    if (DomainRuntime_HasMarker(runtime) && runtime->initialized)
    {
        return DOMAIN_RUNTIME_ALREADY_INITIALIZED;
    }

    /* Network ingress is a caller precondition. Never retain an older owner. */
    DomainIfCisOccupantBinding_Reset();
    DomainRequestCoordinator_ResetBinding();
    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->integrityMarker = DOMAIN_RUNTIME_INTEGRITY_MARKER;
    runtime->timeProvider = config->timeProvider;
    runtime->timeProviderContext = config->timeProviderContext;
    runtime->permissionCoordinatorEnabled =
        config->permissionCoordinatorEnabled;
    runtime->requestCoordinatorEnabled = config->requestCoordinatorEnabled;
    runtime->bcmDoorStateTtlMs = config->bcmDoorStateTtlMs;
    runtime->mobileRequestSourceValidityMs =
        config->mobileRequestSourceValidityMs;
    runtime->requestAuthorizationProvider =
        config->requestAuthorizationProvider;
    runtime->requestAuthorizationContext =
        config->requestAuthorizationContext;
    runtime->cisOccupantEnabled = config->cisOccupantEnabled;
    runtime->cisOccupantStorageEnabled = config->cisOccupantEnabled || config->cisOccupantStorageEnabled;

    if (!DomainRuntime_GetTime(runtime, &initialNowMs))
    {
        return DOMAIN_RUNTIME_TIME_UNAVAILABLE;
    }
    (void)initialNowMs;
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CONFIG_VALIDATED;

    DomainRuntime_PrepareVehicleStateStorage(runtime);
    status = DomainRuntime_InitManagers(runtime, config);
    if (status != DOMAIN_RUNTIME_OK)
    {
        return status;
    }

    status = DomainRuntime_InitC2AndBindings(runtime, config);
    if (status != DOMAIN_RUNTIME_OK)
    {
        /* The Network-facing callback tables are kept explicitly unbound. */
        DomainRequestCoordinator_ResetBinding();
        (void)DomainIf_SetRxObserver(NULL);
        (void)DomainIf_SetTxPort(NULL);
        return status;
    }

    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_READY;
    runtime->initialized = true;
    runtime->ready = true;
    return DOMAIN_RUNTIME_OK;
}

DomainRuntime_Status_t DomainRuntime_Poll(DomainRuntime_t *runtime)
{
    uint32_t nowMs;
    uint32_t elapsedMs;

    if (runtime == NULL)
    {
        return DOMAIN_RUNTIME_INVALID_ARGUMENT;
    }
    if (!DomainRuntime_IsReady(runtime))
    {
        return DOMAIN_RUNTIME_NOT_INITIALIZED;
    }
    if (!DomainRuntime_GetTime(runtime, &nowMs))
    {
        return DOMAIN_RUNTIME_TIME_UNAVAILABLE;
    }

    elapsedMs = runtime->hasPolled
                    ? (uint32_t)(nowMs - runtime->lastPollAtMs)
                    : 0U;
    runtime->lastPollAtMs = nowMs;
    runtime->lastPollElapsedMs = elapsedMs;
    runtime->pollCount += 1U;
    runtime->hasPolled = true;

    if (runtime->permissionCoordinatorEnabled)
    {
        runtime->lastPermissionCoordinatorStatus = PermissionCoordinator_Poll(
            &runtime->permissionCoordinator, nowMs);
    }
    else
    {
        runtime->lastPermissionCoordinatorStatus =
            PERMISSION_COORDINATOR_NOT_INITIALIZED;
    }
    if (runtime->requestCoordinatorEnabled)
    {
        runtime->lastRequestCoordinatorStatus =
            DomainRequestCoordinator_Poll(
                &runtime->requestCoordinator,
                nowMs,
                &runtime->lastRequestTimeoutCount);
    }
    else
    {
        runtime->lastRequestCoordinatorStatus =
            DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED;
        runtime->lastRequestTimeoutCount = 0U;
    }
    runtime->lastVssWarningStatus = VssWarningManager_Poll(
        &runtime->vssWarningManager, nowMs);
    runtime->lastVssEventStatus = VssEventManager_Poll(
        &runtime->vssEventManager, nowMs);
    DomainSnapshotCommit_Poll(nowMs);
    /*
     * Evaluation and local Network admission failures remain observable in
     * the coordinator and leave Permission fail-closed. They do not stop the
     * bounded superloop; another attempt is made only at the next period.
     */
    return DOMAIN_RUNTIME_OK;
}

bool DomainRuntime_IsReady(const DomainRuntime_t *runtime)
{
    return DomainRuntime_HasMarker(runtime) && runtime->initialized &&
           runtime->ready &&
           (runtime->initStage == DOMAIN_RUNTIME_INIT_STAGE_READY);
}

DomainRuntime_Status_t DomainRuntime_GetSnapshot(
    const DomainRuntime_t *runtime,
    DomainRuntime_Snapshot_t *outSnapshot)
{
    DomainRuntime_Snapshot_t snapshot;

    if ((runtime == NULL) || (outSnapshot == NULL))
    {
        return DOMAIN_RUNTIME_INVALID_ARGUMENT;
    }
    if (!DomainRuntime_HasMarker(runtime))
    {
        return DOMAIN_RUNTIME_NOT_INITIALIZED;
    }

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.initialized = runtime->initialized;
    snapshot.ready = DomainRuntime_IsReady(runtime);
    snapshot.permissionCoordinatorEnabled =
        runtime->permissionCoordinatorEnabled;
    snapshot.requestCoordinatorEnabled = runtime->requestCoordinatorEnabled;
    snapshot.cisOccupantEnabled = runtime->cisOccupantEnabled;
    snapshot.cisOccupantStorageEnabled = runtime->cisOccupantStorageEnabled;
    snapshot.initStage = runtime->initStage;
    snapshot.hasPolled = runtime->hasPolled;
    snapshot.lastPollAtMs = runtime->lastPollAtMs;
    snapshot.lastPollElapsedMs = runtime->lastPollElapsedMs;
    snapshot.pollCount = runtime->pollCount;
    snapshot.lastPermissionCoordinatorStatus =
        runtime->lastPermissionCoordinatorStatus;
    snapshot.lastRequestCoordinatorStatus =
        runtime->lastRequestCoordinatorStatus;
    snapshot.lastRequestTimeoutCount = runtime->lastRequestTimeoutCount;
    snapshot.lastVssWarningStatus = runtime->lastVssWarningStatus;
    snapshot.lastVssEventStatus = runtime->lastVssEventStatus;
    *outSnapshot = snapshot;
    return DOMAIN_RUNTIME_OK;
}

VehicleStateManager_t *DomainRuntime_GetVehicleStateManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->vehicleStateManager
                                          : NULL;
}

CommandManager_t *DomainRuntime_GetCommandManager(DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->commandManager : NULL;
}

ResultManager_t *DomainRuntime_GetResultManager(DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->resultManager : NULL;
}

DomainRequestCoordinator_t *DomainRuntime_GetRequestCoordinator(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) && runtime->requestCoordinatorEnabled
               ? &runtime->requestCoordinator
               : NULL;
}

PermissionManager_t *DomainRuntime_GetPermissionManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->permissionManager : NULL;
}

CisOccupantManager_t *DomainRuntime_GetCisOccupantManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) && runtime->cisOccupantStorageEnabled
               ? &runtime->cisOccupantManager
               : NULL;
}

DomainCisOccupantPriv_t *DomainRuntime_GetCisOccupantPrivate(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) && runtime->cisOccupantStorageEnabled
               ? &runtime->cisOccupantPrivate
               : NULL;
}

CisEnvironmentManager_t *DomainRuntime_GetCisEnvironmentManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->cisEnvironmentManager
                                          : NULL;
}

DomainCisEnvironmentPriv_t *DomainRuntime_GetCisEnvironmentPrivate(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->cisEnvironmentPrivate
                                          : NULL;
}

VssWarningManager_t *DomainRuntime_GetVssWarningManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->vssWarningManager : NULL;
}

VssEventManager_t *DomainRuntime_GetVssEventManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->vssEventManager : NULL;
}
