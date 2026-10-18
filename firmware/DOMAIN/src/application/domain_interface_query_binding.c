#include "domain_interface_query_binding.h"

#define DOMAIN_IF_QUERY_RECENT_RESULTS_MAX 5U

typedef struct
{
    VehicleStateManager_t *stateManager;
    DomainIfStateBinding_SlotMap_t slots;
    DomainIfQueryBinding_StatePolicy_t statePolicy;
    ResultManager_t *resultManager;
    bool stateConfigured;
    bool resultsConfigured;
} DomainIfQueryBinding_Context_t;

static DomainIfQueryBinding_Context_t g_queryBinding;

static bool DomainIfQueryBinding_AreSlotsDistinct(
    const DomainIfStateBinding_SlotMap_t *slots)
{
    return (slots->bcmDoorState != slots->bcmStatus) &&
           (slots->bcmDoorState != slots->cisRearState) &&
           (slots->bcmDoorState != slots->cisStatus) &&
           (slots->bcmStatus != slots->cisRearState) &&
           (slots->bcmStatus != slots->cisStatus) &&
           (slots->cisRearState != slots->cisStatus);
}

static bool DomainIfQueryBinding_IsSlotReadable(
    const VehicleStateManager_t *manager,
    VehicleStateManager_SlotId_t slotId,
    void *value,
    size_t valueSize)
{
    VehicleStateManager_Snapshot_t snapshot;
    VehicleStateManager_Status_t status;

    status = VehicleStateManager_GetSnapshot(
        manager, slotId, 0U, UINT32_MAX,
        value, valueSize, &snapshot);
    return (status == VEHICLE_STATE_MANAGER_OK) ||
           (status == VEHICLE_STATE_MANAGER_NO_DATA);
}

static DomainIf_Status_t DomainIfQueryBinding_MapQuality(
    VehicleStateQuality_t quality,
    DataQuality_t *outQuality)
{
    switch (quality)
    {
        case VEHICLE_STATE_QUALITY_OK:
            *outQuality = DATA_QUALITY_OK;
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_QUALITY_STALE:
            *outQuality = DATA_QUALITY_STALE;
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_QUALITY_INVALID:
            *outQuality = DATA_QUALITY_INVALID;
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_QUALITY_NO_DATA:
            *outQuality = DATA_QUALITY_NO_DATA;
            return DOMAIN_IF_OK;
        default:
            return DOMAIN_IF_NOT_AVAILABLE;
    }
}

static DomainIf_Status_t DomainIfQueryBinding_MapStateManagerStatus(
    VehicleStateManager_Status_t status)
{
    switch (status)
    {
        case VEHICLE_STATE_MANAGER_OK:
            return DOMAIN_IF_OK;
        case VEHICLE_STATE_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case VEHICLE_STATE_MANAGER_NOT_INITIALIZED:
        case VEHICLE_STATE_MANAGER_INVALID_SLOT:
        case VEHICLE_STATE_MANAGER_CAPACITY_EXCEEDED:
        case VEHICLE_STATE_MANAGER_NO_DATA:
            return DOMAIN_IF_NOT_AVAILABLE;
        case VEHICLE_STATE_MANAGER_DUPLICATE:
        case VEHICLE_STATE_MANAGER_OUT_OF_ORDER:
        default:
            return DOMAIN_IF_REJECTED;
    }
}

static DomainIf_Status_t DomainIfQueryBinding_GetState(
    VehicleStateManager_SlotId_t slotId,
    uint32_t nowMs,
    uint32_t ttlMs,
    void *outValue,
    size_t valueSize,
    SignalMeta_t *outMeta)
{
    VehicleStateManager_Snapshot_t snapshot;
    VehicleStateManager_Status_t managerStatus;
    DomainIf_Status_t status;
    DataQuality_t quality;

    if (!g_queryBinding.stateConfigured ||
        (g_queryBinding.stateManager == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    managerStatus = VehicleStateManager_GetSnapshot(
        g_queryBinding.stateManager, slotId, nowMs, ttlMs,
        outValue, valueSize, &snapshot);
    status = DomainIfQueryBinding_MapStateManagerStatus(managerStatus);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }
    if (!snapshot.hasValue || (snapshot.valueSize != valueSize))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    status = DomainIfQueryBinding_MapQuality(snapshot.quality, &quality);
    if (status != DOMAIN_IF_OK)
    {
        return status;
    }
    outMeta->quality = quality;
    outMeta->ageMs = snapshot.effectiveAgeMs;
    outMeta->updateSequence = snapshot.updateSequence;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t DomainIfQueryBinding_MapResultManagerStatus(
    ResultManager_Status_t status)
{
    switch (status)
    {
        case RESULT_MANAGER_OK:
            return DOMAIN_IF_OK;
        case RESULT_MANAGER_INVALID_ARGUMENT:
            return DOMAIN_IF_INVALID_ARGUMENT;
        case RESULT_MANAGER_NOT_INITIALIZED:
            return DOMAIN_IF_NOT_AVAILABLE;
        case RESULT_MANAGER_CAPACITY_EXCEEDED:
            return DOMAIN_IF_QUEUE_FULL;
        case RESULT_MANAGER_NOT_FOUND:
            return DOMAIN_IF_NOT_AVAILABLE;
        case RESULT_MANAGER_DUPLICATE:
        case RESULT_MANAGER_ID_CONFLICT:
        case RESULT_MANAGER_COMMAND_CONFLICT:
        case RESULT_MANAGER_KIND_MISMATCH:
        case RESULT_MANAGER_INVALID_TRANSITION:
        case RESULT_MANAGER_UNCONFIRMED_RECORDED:
        case RESULT_MANAGER_EVIDENCE_REJECTED:
        case RESULT_MANAGER_LATE_INTERMEDIATE_IGNORED:
        case RESULT_MANAGER_FINAL_CONFLICT:
        default:
            return DOMAIN_IF_REJECTED;
    }
}

DomainIf_Status_t DomainIfQueryBinding_ConfigureState(
    VehicleStateManager_t *stateManager,
    const DomainIfStateBinding_SlotMap_t *slotMap,
    const DomainIfQueryBinding_StatePolicy_t *policy)
{
    DoorState_t door;
    DomainIf_BcmStatusReport_t bcmStatus;
    CisRearState_t cisRear;
    DomainIf_CisStatusReport_t cisStatus;

    if ((stateManager == NULL) || (slotMap == NULL) || (policy == NULL) ||
        !DomainIfQueryBinding_AreSlotsDistinct(slotMap))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!DomainIfQueryBinding_IsSlotReadable(
            stateManager, slotMap->bcmDoorState, &door, sizeof(door)) ||
        !DomainIfQueryBinding_IsSlotReadable(
            stateManager, slotMap->bcmStatus, &bcmStatus,
            sizeof(bcmStatus)) ||
        !DomainIfQueryBinding_IsSlotReadable(
            stateManager, slotMap->cisRearState, &cisRear,
            sizeof(cisRear)) ||
        !DomainIfQueryBinding_IsSlotReadable(
            stateManager, slotMap->cisStatus, &cisStatus,
            sizeof(cisStatus)))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    g_queryBinding.stateManager = stateManager;
    g_queryBinding.slots = *slotMap;
    g_queryBinding.statePolicy = *policy;
    g_queryBinding.stateConfigured = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIfQueryBinding_ConfigureResults(
    ResultManager_t *resultManager)
{
    ResultManager_RequestKey_t probeKey;
    ResultManager_Snapshot_t probeSnapshot;
    ResultManager_Status_t probeStatus;

    if (resultManager == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    probeKey.deviceContextId = UINT32_MAX;
    probeKey.context.sessionId = UINT64_MAX;
    probeKey.context.requestId = UINT32_MAX;
    probeStatus = ResultManager_GetSnapshot(
        resultManager, &probeKey, &probeSnapshot);
    if ((probeStatus != RESULT_MANAGER_OK) &&
        (probeStatus != RESULT_MANAGER_NOT_FOUND))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    g_queryBinding.resultManager = resultManager;
    g_queryBinding.resultsConfigured = true;
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_QueryGetBcmDoorState(
    uint32_t nowMs,
    DoorState_t *outState)
{
    DoorState_t state;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfQueryBinding_GetState(
        g_queryBinding.slots.bcmDoorState, nowMs,
        g_queryBinding.statePolicy.bcmDoorTtlMs,
        &state, sizeof(state), &state.meta);
    if (status == DOMAIN_IF_OK)
    {
        *outState = state;
    }
    return status;
}

DomainIf_Status_t DomainIf_QueryGetBcmStatus(
    uint32_t nowMs,
    DomainIf_BcmStatusReport_t *outState)
{
    DomainIf_BcmStatusReport_t state;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfQueryBinding_GetState(
        g_queryBinding.slots.bcmStatus, nowMs,
        g_queryBinding.statePolicy.bcmStatusTtlMs,
        &state, sizeof(state), &state.meta);
    if (status == DOMAIN_IF_OK)
    {
        *outState = state;
    }
    return status;
}

DomainIf_Status_t DomainIf_QueryGetCisRear(
    uint32_t nowMs,
    CisRearState_t *outState)
{
    CisRearState_t state;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfQueryBinding_GetState(
        g_queryBinding.slots.cisRearState, nowMs,
        g_queryBinding.statePolicy.cisRearTtlMs,
        &state, sizeof(state), &state.distanceQuality.meta);
    if (status == DOMAIN_IF_OK)
    {
        *outState = state;
    }
    return status;
}

DomainIf_Status_t DomainIf_QueryGetCisStatus(
    uint32_t nowMs,
    DomainIf_CisStatusReport_t *outState)
{
    DomainIf_CisStatusReport_t state;
    DomainIf_Status_t status;

    if (outState == NULL)
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }

    status = DomainIfQueryBinding_GetState(
        g_queryBinding.slots.cisStatus, nowMs,
        g_queryBinding.statePolicy.cisStatusTtlMs,
        &state, sizeof(state), &state.meta);
    if (status == DOMAIN_IF_OK)
    {
        *outState = state;
    }
    return status;
}

DomainIf_Status_t DomainIf_QueryGetRequestResult(
    DeviceContextId_t requestingDevice,
    const RequestContext_t *targetRequest,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResult,
    bool *outHasRecord)
{
    ResultManager_RequestKey_t key;
    DomainIf_MobileRequestResult_t result;
    ResultManager_Status_t managerStatus;
    DomainIf_Status_t status;

    if ((requestingDevice == 0U) || (targetRequest == NULL) ||
        (targetRequest->sessionId == 0U) ||
        (targetRequest->requestId == 0U) ||
        (outResult == NULL) || (outHasRecord == NULL))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_queryBinding.resultsConfigured ||
        (g_queryBinding.resultManager == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    key.deviceContextId = requestingDevice;
    key.context = *targetRequest;
    managerStatus = ResultManager_GetMobileResult(
        g_queryBinding.resultManager, &key, nowMs, &result);
    if (managerStatus == RESULT_MANAGER_NOT_FOUND)
    {
        *outHasRecord = false;
        return DOMAIN_IF_OK;
    }

    status = DomainIfQueryBinding_MapResultManagerStatus(managerStatus);
    if (status == DOMAIN_IF_OK)
    {
        *outResult = result;
        *outHasRecord = true;
    }
    return status;
}

DomainIf_Status_t DomainIf_QueryGetRecentRequestResults(
    DeviceContextId_t requestingDevice,
    uint32_t nowMs,
    DomainIf_MobileRequestResult_t *outResults,
    size_t capacity,
    size_t *outCount)
{
    ResultManager_Status_t managerStatus;
    DomainIf_Status_t status;
    size_t managerCount;
    size_t queryCapacity = capacity;

    if ((requestingDevice == 0U) || (outCount == NULL) ||
        ((capacity > 0U) && (outResults == NULL)))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if (!DomainIf_IsInitialized())
    {
        return DOMAIN_IF_NOT_INITIALIZED;
    }
    if (!g_queryBinding.resultsConfigured ||
        (g_queryBinding.resultManager == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }

    if (queryCapacity > DOMAIN_IF_QUERY_RECENT_RESULTS_MAX)
    {
        queryCapacity = DOMAIN_IF_QUERY_RECENT_RESULTS_MAX;
    }
    managerStatus = ResultManager_GetRecentMobileResults(
        g_queryBinding.resultManager, requestingDevice, nowMs,
        outResults, queryCapacity, &managerCount);
    status = DomainIfQueryBinding_MapResultManagerStatus(managerStatus);
    if (status == DOMAIN_IF_OK)
    {
        if (managerCount > DOMAIN_IF_QUERY_RECENT_RESULTS_MAX)
        {
            managerCount = DOMAIN_IF_QUERY_RECENT_RESULTS_MAX;
        }
        *outCount = managerCount;
    }
    return status;
}
