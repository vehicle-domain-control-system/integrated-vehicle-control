#ifndef DOMAIN_NETWORK_BRINGUP_H
#define DOMAIN_NETWORK_BRINGUP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DOMAIN_NETWORK_BRINGUP_BOOTSTRAP = 0,
    DOMAIN_NETWORK_BRINGUP_STANDBY,
    DOMAIN_NETWORK_BRINGUP_NETWORK_INITIALIZING,
    DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED,
    DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED,
    DOMAIN_NETWORK_BRINGUP_ACTIVE,
    DOMAIN_NETWORK_BRINGUP_FAULT
} DomainNetworkBringup_State_t;

typedef enum
{
    DOMAIN_NETWORK_BRINGUP_FAULT_NONE = 0,
    DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION,
    DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_ROLLBACK,
    DOMAIN_NETWORK_BRINGUP_FAULT_NORMAL_REQUEST,
    DOMAIN_NETWORK_BRINGUP_FAULT_ACTIVE_EVIDENCE_TIMEOUT,
    DOMAIN_NETWORK_BRINGUP_FAULT_EXTERNAL,
    DOMAIN_NETWORK_BRINGUP_FAULT_INVALID_CONTEXT
} DomainNetworkBringup_Fault_t;

typedef enum
{
    DOMAIN_NETWORK_BRINGUP_OK = 0,
    DOMAIN_NETWORK_BRINGUP_PENDING,
    DOMAIN_NETWORK_BRINGUP_ALREADY,
    DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT,
    DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED,
    DOMAIN_NETWORK_BRINGUP_INVALID_STATE,
    DOMAIN_NETWORK_BRINGUP_NOT_APPROVED,
    DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_UNAVAILABLE,
    DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NOT_STANDBY,
    DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NORMAL_REQUEST_FAILED,
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_INVALID,
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_EXPIRED,
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CONTRACT_OPEN,
    DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED
} DomainNetworkBringup_Status_t;

/*
 * Candidate evidence sources only. BINDING_OPEN: Network has not yet approved
 * either source as sufficient proof of physical CAN activity. Consequently,
 * ConfirmActive validates the envelope but does not enter ACTIVE in this phase.
 */
typedef enum
{
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_NONE = 0,
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CAN_RX_ACCEPTED,
    DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CAN_TX_CONFIRMED
} DomainNetworkBringup_ActiveEvidenceSource_t;

typedef struct
{
    DomainNetworkBringup_ActiveEvidenceSource_t source;
    uint32_t networkBootId;
    uint32_t portEpoch;
    uint32_t observedAtMs;
    uint32_t expiresAtMs;
} DomainNetworkBringup_ActiveEvidence_t;

/* Caller-owned, single non-ISR owner state; zero-initialize before Init. */
typedef struct
{
    bool initialized;
    bool hardwareActivationApproved;
    bool standbyRecoveryAttempted;
    bool standbyRecoveryFailed;
    DomainNetworkBringup_State_t state;
    DomainNetworkBringup_Fault_t fault;
    uint32_t expectedNetworkBootId;
    uint32_t expectedPortEpoch;
    uint32_t normalRequestedAtMs;
    uint32_t activeEvidenceDeadlineMs;
} DomainNetworkBringup_t;

DomainNetworkBringup_Status_t DomainNetworkBringup_Init(
    DomainNetworkBringup_t *context,
    bool hardwareActivationApproved);

DomainNetworkBringup_Status_t DomainNetworkBringup_EnterStandby(
    DomainNetworkBringup_t *context);

DomainNetworkBringup_Status_t DomainNetworkBringup_BeginNetworkInitialization(
    DomainNetworkBringup_t *context);

DomainNetworkBringup_Status_t DomainNetworkBringup_ObserveNetworkStatus(
    DomainNetworkBringup_t *context);

/*
 * Requests STB LOW only when Network is still PREPARED and all fail-closed
 * preconditions hold. The caller supplies the current Network identity and an
 * absolute evidence deadline; no Production timeout is invented here. Times
 * must be on one uint32_t millisecond axis and less than 2^31 ms apart.
 * Boot ID is checked against Net_GetBootId(). Port epoch remains BINDING_OPEN:
 * the current public Network entry API has no authoritative epoch getter.
 */
DomainNetworkBringup_Status_t DomainNetworkBringup_RequestNormal(
    DomainNetworkBringup_t *context,
    uint32_t networkBootId,
    uint32_t portEpoch,
    uint32_t nowMs,
    uint32_t activeEvidenceDeadlineMs);

/*
 * Validates source, identity, ordering and expiry. ACTIVE remains blocked until
 * the Network/Board owners approve an evidence source contract.
 */
DomainNetworkBringup_Status_t DomainNetworkBringup_ConfirmActive(
    DomainNetworkBringup_t *context,
    const DomainNetworkBringup_ActiveEvidence_t *evidence,
    uint32_t nowMs);

/* Call from the future bootstrap owner while NORMAL_REQUESTED. */
DomainNetworkBringup_Status_t DomainNetworkBringup_CheckActiveEvidenceDeadline(
    DomainNetworkBringup_t *context,
    uint32_t nowMs);

DomainNetworkBringup_Status_t DomainNetworkBringup_ReportFault(
    DomainNetworkBringup_t *context,
    DomainNetworkBringup_Fault_t fault);

DomainNetworkBringup_State_t DomainNetworkBringup_GetState(
    const DomainNetworkBringup_t *context);

DomainNetworkBringup_Fault_t DomainNetworkBringup_GetFault(
    const DomainNetworkBringup_t *context);

bool DomainNetworkBringup_WasStandbyRecoveryAttempted(
    const DomainNetworkBringup_t *context);

bool DomainNetworkBringup_DidStandbyRecoveryFail(
    const DomainNetworkBringup_t *context);

#endif /* DOMAIN_NETWORK_BRINGUP_H */
