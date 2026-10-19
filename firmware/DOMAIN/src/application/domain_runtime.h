#ifndef DOMAIN_RUNTIME_H
#define DOMAIN_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "command_manager.h"
#include "cis_occupant_manager.h"
#include "domain_cis_occupant_private.h"
#include "domain_cis_environment_private.h"
#include "domain_interface_cis_occupant_binding.h"
#include "domain_interface_cis_environment_binding.h"
#include "domain_interface_query_binding.h"
#include "domain_interface_result_binding.h"
#include "permission_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed Application resource limits. These are local storage choices, not C2
 * or Network contract values. Capacity exhaustion remains an explicit Manager
 * result and never authorizes overwriting a live command or result record.
 */
#define DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT 4U
#define DOMAIN_RUNTIME_COMMAND_CAPACITY 8U
#define DOMAIN_RUNTIME_RESULT_CAPACITY 16U

/*
 * Board adapter contract.
 *
 * The provider is called only from the single serialized non-ISR owner. It
 * returns a monotonically advancing uint32_t millisecond value; normal
 * uint32_t rollover is required and consumers use modulo subtraction. false
 * means that no trustworthy timestamp is currently available. The provider
 * must not delay, sleep, busy-wait, call a Domain API, or retain outNowMs.
 * This interface does not configure SysTick, PIT, STM, GPT, an ISR, or RTOS.
 */
typedef bool (*DomainRuntime_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

typedef enum
{
    DOMAIN_RUNTIME_OK = 0,
    DOMAIN_RUNTIME_INVALID_ARGUMENT,
    DOMAIN_RUNTIME_NOT_INITIALIZED,
    DOMAIN_RUNTIME_ALREADY_INITIALIZED,
    DOMAIN_RUNTIME_TIME_UNAVAILABLE,
    DOMAIN_RUNTIME_MANAGER_INIT_FAILED,
    DOMAIN_RUNTIME_C2_INIT_FAILED,
    DOMAIN_RUNTIME_BINDING_INIT_FAILED
} DomainRuntime_Status_t;

typedef enum
{
    DOMAIN_RUNTIME_INIT_STAGE_NONE = 0,
    DOMAIN_RUNTIME_INIT_STAGE_CONFIG_VALIDATED,
    DOMAIN_RUNTIME_INIT_STAGE_VEHICLE_STATE_MANAGER,
    DOMAIN_RUNTIME_INIT_STAGE_COMMAND_MANAGER,
    DOMAIN_RUNTIME_INIT_STAGE_RESULT_MANAGER,
    DOMAIN_RUNTIME_INIT_STAGE_PERMISSION_MANAGER,
    DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_MANAGER,
    DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_PRIVATE,
    DOMAIN_RUNTIME_INIT_STAGE_C2_CORE,
    DOMAIN_RUNTIME_INIT_STAGE_C2_UNBOUND,
    DOMAIN_RUNTIME_INIT_STAGE_STATE_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_RESULT_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_QUERY_STATE_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_QUERY_RESULT_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_CIS_OCCUPANT_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_CIS_ENVIRONMENT_BINDING,
    DOMAIN_RUNTIME_INIT_STAGE_READY
} DomainRuntime_InitStage_t;

typedef enum
{
    DOMAIN_RUNTIME_STATE_SLOT_BCM_DOOR = 0,
    DOMAIN_RUNTIME_STATE_SLOT_BCM_STATUS,
    DOMAIN_RUNTIME_STATE_SLOT_CIS_REAR,
    DOMAIN_RUNTIME_STATE_SLOT_CIS_STATUS
} DomainRuntime_StateSlot_t;

typedef struct
{
    DomainRuntime_TimeProvider_t timeProvider;
    void *timeProviderContext;
    CommandManager_CommandId_t firstCommandId;
    ResultManager_Policy_t resultPolicy;
    PermissionManager_Policy_t permissionPolicy;
    CisOccupantManager_Policy_t cisOccupantPolicy;
    DomainIfQueryBinding_StatePolicy_t stateQueryPolicy;
} DomainRuntime_Config_t;

typedef struct
{
    DoorState_t bcmDoor;
    DomainIf_BcmStatusReport_t bcmStatus;
    CisRearState_t cisRear;
    DomainIf_CisStatusReport_t cisStatus;
} DomainRuntime_VehicleStateStorage_t;

typedef struct
{
    bool initialized;
    bool ready;
    DomainRuntime_InitStage_t initStage;
    bool hasPolled;
    uint32_t lastPollAtMs;
    uint32_t lastPollElapsedMs;
    uint32_t pollCount;
} DomainRuntime_Snapshot_t;

/*
 * One caller-owned, fixed-size Domain Application runtime.
 *
 * Declare this object with static storage duration or zero-initialize it before
 * the first call. It owns every Manager context and backing array below. The
 * binding modules retain pointers to these members, so the object must remain
 * at the same address for the whole runtime lifetime.
 */
typedef struct
{
    uint32_t integrityMarker;
    DomainRuntime_TimeProvider_t timeProvider;
    void *timeProviderContext;

    VehicleStateManager_t vehicleStateManager;
    VehicleStateManager_Slot_t
        vehicleStateSlots[DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT];
    VehicleStateManager_SlotConfig_t
        vehicleStateSlotConfigs[DOMAIN_RUNTIME_VEHICLE_STATE_SLOT_COUNT];
    DomainRuntime_VehicleStateStorage_t vehicleStateStorage;
    DomainIfStateBinding_SlotMap_t vehicleStateSlotMap;

    CommandManager_t commandManager;
    CommandManager_Slot_t commandSlots[DOMAIN_RUNTIME_COMMAND_CAPACITY];

    ResultManager_t resultManager;
    ResultManager_Slot_t resultSlots[DOMAIN_RUNTIME_RESULT_CAPACITY];

    PermissionManager_t permissionManager;
    CisOccupantManager_t cisOccupantManager;
    DomainCisOccupantPriv_t cisOccupantPrivate;
    CisEnvironmentManager_t cisEnvironmentManager;
    DomainCisEnvironmentPriv_t cisEnvironmentPrivate;

    DomainRuntime_InitStage_t initStage;
    bool initialized;
    bool ready;
    bool hasPolled;
    uint32_t lastPollAtMs;
    uint32_t lastPollElapsedMs;
    uint32_t pollCount;
} DomainRuntime_t;

/*
 * Initializes Managers, C2, and bindings in dependency order. Observer and
 * TxPort are explicitly left unregistered. Network ingress must remain closed
 * until this function returns OK and DomainRuntime_IsReady() is true.
 *
 * Calling Init again on an initialized context returns ALREADY_INITIALIZED and
 * changes nothing. A context whose earlier Init failed before Ready may be
 * retried after correcting the configuration, while ingress remains closed.
 */
DomainRuntime_Status_t DomainRuntime_Init(
    DomainRuntime_t *runtime,
    const DomainRuntime_Config_t *config);

/*
 * Bounded, non-blocking owner-context step. The current Core only samples the
 * injected clock and records rollover-safe loop timing. It does not infer a
 * Permission, dispatch a request, call a TxPort, or perform Network I/O.
 */
DomainRuntime_Status_t DomainRuntime_Poll(DomainRuntime_t *runtime);

bool DomainRuntime_IsReady(const DomainRuntime_t *runtime);

/* Read-only value copy; failure leaves outSnapshot unchanged. */
DomainRuntime_Status_t DomainRuntime_GetSnapshot(
    const DomainRuntime_t *runtime,
    DomainRuntime_Snapshot_t *outSnapshot);

/*
 * Runtime-owned Manager access for future Application coordinators. Mutable
 * access is intentionally restricted to a Ready runtime and the same single
 * non-ISR owner. RequestManager has no context or Init API and is therefore
 * not represented by a fabricated runtime object.
 */
VehicleStateManager_t *DomainRuntime_GetVehicleStateManager(
    DomainRuntime_t *runtime);
CommandManager_t *DomainRuntime_GetCommandManager(DomainRuntime_t *runtime);
ResultManager_t *DomainRuntime_GetResultManager(DomainRuntime_t *runtime);
PermissionManager_t *DomainRuntime_GetPermissionManager(
    DomainRuntime_t *runtime);
CisOccupantManager_t *DomainRuntime_GetCisOccupantManager(
    DomainRuntime_t *runtime);
DomainCisOccupantPriv_t *DomainRuntime_GetCisOccupantPrivate(
    DomainRuntime_t *runtime);
CisEnvironmentManager_t *DomainRuntime_GetCisEnvironmentManager(
    DomainRuntime_t *runtime);
DomainCisEnvironmentPriv_t *DomainRuntime_GetCisEnvironmentPrivate(
    DomainRuntime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_RUNTIME_H */
