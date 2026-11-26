#ifndef VSS_WARNING_MANAGER_H
#define VSS_WARNING_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "vehicle_state_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VSS_WARNING_MANAGER_REAR_TTL_MS 150U
#define VSS_WARNING_MANAGER_OCCUPANT_TTL_MS 1000U

typedef enum
{
    VSS_WARNING_MANAGER_OK = 0,
    VSS_WARNING_MANAGER_INVALID_ARGUMENT,
    VSS_WARNING_MANAGER_NOT_INITIALIZED,
    VSS_WARNING_MANAGER_NOT_AVAILABLE,
    VSS_WARNING_MANAGER_TX_PENDING
} VssWarningManager_Status_t;

typedef DomainIf_Status_t (*VssWarningManager_Tx_t)(
    const DomainIf_VssWarningState_t *state);

/* The caller must supply authoritative combined lineage for all three
 * inputs. Runtime does not construct this evidence until the cross-source
 * contract is approved. */
typedef struct
{
    bool vehicleUseEnded;
    bool userDeparted;
    bool occupantPresent;
    DomainIf_WarningInputEvidence_t combinedEvidence;
} VssWarningManager_OccupantInput_t;

typedef struct
{
    VehicleStateManager_t *vehicleStateManager;
    VehicleStateManager_SlotId_t rearSlot;
    VssWarningManager_Tx_t tx;
    DomainIf_VssWarningState_t pending;
    SourceGeneration_t lastGeneration;
    UpdateSequence_t lastSequence;
    DataQuality_t lastQuality;
    QualityReason_t lastReason;
    RearMeasurementState_t lastMeasurementState;
    uint16_t lastDistanceMm;
    bool hasLastEvaluation;
    bool pendingValid;
    bool initialized;
} VssWarningManager_t;

VssWarningManager_Status_t VssWarningManager_Init(
    VssWarningManager_t *manager,
    VehicleStateManager_t *vehicleStateManager,
    VehicleStateManager_SlotId_t rearSlot,
    VssWarningManager_Tx_t tx);

VssWarningManager_Status_t VssWarningManager_EvaluateOccupant(
    const VssWarningManager_OccupantInput_t *input,
    OccupantHazardState_t *outState,
    DomainIf_WarningInputEvidence_t *outEvidence);

/*
 * Evaluates only Domain-owned rear policy. Occupant remains UNCONFIRMED until
 * vehicle-use-ended and user-departure evidence have a representable,
 * approved provenance contract. Anti-pinch remains unsupported.
 */
VssWarningManager_Status_t VssWarningManager_Poll(
    VssWarningManager_t *manager,
    uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif /* VSS_WARNING_MANAGER_H */
