#include "domain_runtime.h"

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

    runtime->vehicleStateSlotMap.bcmDoorState =
        DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR;
    runtime->vehicleStateSlotMap.bcmStatus =
        DOMAIN_RUNTIME_STATE_SLOT_BCM_STATUS;
    runtime->vehicleStateSlotMap.cisRearState =
        DOMAIN_RUNTIME_STATE_SLOT_CIS_REAR;
    runtime->vehicleStateSlotMap.cisStatus =
        DOMAIN_RUNTIME_STATE_SLOT_CIS_STATUS;
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

    if (PermissionManager_Init(
            &runtime->permissionManager,
            &config->permissionPolicy) != PERMISSION_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_PERMISSION_MANAGER;

    if (CisOccupantManager_Init(
            &runtime->cisOccupantManager,
            &config->cisOccupantPolicy) != CIS_OCCUPANT_MANAGER_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_MANAGER;

    if (DomainCisOccupantPriv_Init(
            &runtime->cisOccupantPrivate,
            &runtime->cisOccupantManager) != DOMAIN_CIS_OCCUPANT_PRIV_OK)
    {
        return DOMAIN_RUNTIME_MANAGER_INIT_FAILED;
    }
    runtime->initStage = DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_PRIVATE;
    return DOMAIN_RUNTIME_OK;
}

static DomainRuntime_Status_t DomainRuntime_InitC2AndBindings(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config)
{
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

    if (DomainIfQueryBinding_ConfigureState(
            &runtime->vehicleStateManager,
            &runtime->vehicleStateSlotMap,
            &config->stateQueryPolicy) != DOMAIN_IF_OK)
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

    if (DomainIfCisOccupantBinding_Configure(
            &runtime->cisOccupantManager,
            &runtime->cisOccupantPrivate,
            runtime->timeProvider,
            runtime->timeProviderContext) != DOMAIN_IF_OK)
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
        (config->timeProvider == NULL))
    {
        return DOMAIN_RUNTIME_INVALID_ARGUMENT;
    }
    if (DomainRuntime_HasMarker(runtime) && runtime->initialized)
    {
        return DOMAIN_RUNTIME_ALREADY_INITIALIZED;
    }

    /* Network ingress is a caller precondition. Never retain an older owner. */
    DomainIfCisOccupantBinding_Reset();
    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->integrityMarker = DOMAIN_RUNTIME_INTEGRITY_MARKER;
    runtime->timeProvider = config->timeProvider;
    runtime->timeProviderContext = config->timeProviderContext;

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

    snapshot.initialized = runtime->initialized;
    snapshot.ready = DomainRuntime_IsReady(runtime);
    snapshot.initStage = runtime->initStage;
    snapshot.hasPolled = runtime->hasPolled;
    snapshot.lastPollAtMs = runtime->lastPollAtMs;
    snapshot.lastPollElapsedMs = runtime->lastPollElapsedMs;
    snapshot.pollCount = runtime->pollCount;
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

PermissionManager_t *DomainRuntime_GetPermissionManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->permissionManager : NULL;
}

CisOccupantManager_t *DomainRuntime_GetCisOccupantManager(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->cisOccupantManager
                                          : NULL;
}

DomainCisOccupantPriv_t *DomainRuntime_GetCisOccupantPrivate(
    DomainRuntime_t *runtime)
{
    return DomainRuntime_IsReady(runtime) ? &runtime->cisOccupantPrivate
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
