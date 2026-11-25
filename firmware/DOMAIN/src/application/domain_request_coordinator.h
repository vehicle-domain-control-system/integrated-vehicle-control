#ifndef DOMAIN_REQUEST_COORDINATOR_H
#define DOMAIN_REQUEST_COORDINATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "command_manager.h"
#include "result_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Single-owner, non-ISR coordinator for MOBILE Door Lock/Unlock requests.
 * Authentication/state providers must return verified snapshots; absence of
 * either provider is fail-closed. Network callbacks are integration ports,
 * not implementations of a CAN/UART transport.
 */

#define DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY 4U
#define DOMAIN_REQUEST_COORDINATOR_POLL_BUDGET 2U

typedef enum
{
    DOMAIN_REQUEST_COORDINATOR_OK = 0,
    DOMAIN_REQUEST_COORDINATOR_DUPLICATE,
    DOMAIN_REQUEST_COORDINATOR_REJECTED,
    DOMAIN_REQUEST_COORDINATOR_UNAVAILABLE,
    DOMAIN_REQUEST_COORDINATOR_UNSUPPORTED,
    DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT,
    DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED,
    DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED,
    DOMAIN_REQUEST_COORDINATOR_ID_CONFLICT,
    DOMAIN_REQUEST_COORDINATOR_TRANSPORT_FAILED,
    DOMAIN_REQUEST_COORDINATOR_INTERNAL_ERROR
} DomainRequestCoordinator_Status_t;

typedef bool (*DomainRequestCoordinator_ContextProvider_t)(
    void *context,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs,
    RequestManager_AuthContext_t *outAuth,
    RequestManager_Context_t *outVehicleContext,
    RequestManager_Policy_t *outPolicy);

typedef bool (*DomainRequestCoordinator_TransportAvailable_t)(void *context);

typedef DomainIf_Status_t (*DomainRequestCoordinator_SubmitDoorCommand_t)(
    void *context,
    const DomainIf_BcmDoorCommand_t *command);

typedef DomainIf_Status_t (*DomainRequestCoordinator_PublishResult_t)(
    void *context,
    const DomainIf_MobileRequestResult_t *result);

typedef bool (*DomainRequestCoordinator_TimeProvider_t)(
    void *context,
    uint32_t *outNowMs);

typedef struct
{
    DomainRequestCoordinator_ContextProvider_t getContext;
    DomainRequestCoordinator_TransportAvailable_t isCommandTransportAvailable;
    DomainRequestCoordinator_SubmitDoorCommand_t submitDoorCommand;
    DomainRequestCoordinator_PublishResult_t publishResult;
    void *context;
} DomainRequestCoordinator_Ports_t;

typedef struct
{
    bool inUse;
    ResultManager_RequestKey_t request;
    bool hasCommand;
    DomainCommandId_t commandId;
    /* Bounded result handoff retry only; never an ECU command retry or auth cache. */
    DomainIf_MobileRequest_t originalRequest;
    bool publishPending;
} DomainRequestCoordinator_Track_t;

typedef enum
{
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_NONE = 0,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_CONTEXT_CHANGED,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED,
    DOMAIN_REQUEST_ADMISSION_INVALIDATION_FRESHNESS_EXPIRED
} DomainRequestCoordinator_AdmissionInvalidation_t;

/*
 * Domain-private admission facts. These fields are never serialized and do
 * not redefine SignalMeta_t. The future Network binding must populate them
 * only after its own framing/order checks and an authoritative P6 context
 * decision. Production has no such binding yet, so this path defaults off.
 */
typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    uint64_t portEpoch;
    uint32_t contextRevision;
    uint32_t networkBootId;
    uint32_t observedAtMs;
    bool timestampKnown;
    bool linkUp;
    bool registrationVerified;
    bool registered;
    bool connectionVerified;
    bool connected;
    bool sessionVerified;
    bool p6AuthorizationVerified;
    /* Independent Provider evidence for protected monitoring publication.
     * Not present on M_CONTEXT; never infer from sessionVerified or Session ID.
     * Zero/default remains false. Does not authorize a vehicle command. */
    bool monitoringSessionReady;
} DomainRequestCoordinator_ContextSnapshot_t;

typedef struct
{
    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    uint64_t portEpoch;
    uint32_t contextRevision;
    uint32_t networkBootId;
    uint16_t linkSequence;
    uint32_t capturedAtMs;
    bool timestampKnown;
    bool p6AuthorizationVerified;
} DomainRequestCoordinator_AdmissionEvidence_t;

typedef struct
{
    DomainIf_MobileRequest_t request;
    uint32_t enqueuedAtMs;
    DomainRequestCoordinator_AdmissionEvidence_t admission;
    bool hasAdmissionEvidence;
} DomainRequestCoordinator_InboxEntry_t;

typedef struct
{
    CommandManager_t ownedCommandManager;
    ResultManager_t ownedResultManager;
    CommandManager_t *commandManager;
    ResultManager_t *resultManager;
    DomainRequestCoordinator_Track_t *tracks;
    size_t trackCapacity;
    DomainRequestCoordinator_InboxEntry_t
        inbox[DOMAIN_REQUEST_COORDINATOR_INBOX_CAPACITY];
    size_t inboxHead;
    size_t inboxCount;
    uint32_t inboxSourceValidityMs;
    DomainRequestCoordinator_Status_t lastInboxStatus;
    uint32_t inboxAcceptedCount;
    uint32_t inboxProcessedCount;
    DomainRequestCoordinator_ContextSnapshot_t admissionContext;
    uint32_t admissionEvidenceValidityMs;
    uint16_t lastAdmissionLinkSequence;
    bool admissionContextValid;
    bool admissionEvidenceEnabled;
    bool admissionSequenceSeen;
    bool pollActive;
    DomainRequestCoordinator_AdmissionInvalidation_t
        lastAdmissionInvalidation;
    uint32_t admissionInvalidatedCount;
    DomainRequestCoordinator_Ports_t ports;
    uint32_t submissionFailureCount;
    DomainIf_Status_t lastSubmissionStatus;
    DomainIf_Status_t lastPublishStatus;
    bool initialized;
} DomainRequestCoordinator_t;

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
    const DomainRequestCoordinator_Ports_t *ports);

/* Runtime integration: attaches to already initialized, runtime-owned Managers. */
DomainRequestCoordinator_Status_t DomainRequestCoordinator_Attach(
    DomainRequestCoordinator_t *coordinator,
    CommandManager_t *commandManager,
    ResultManager_t *resultManager,
    DomainRequestCoordinator_Track_t *tracks,
    size_t trackCapacity,
    const DomainRequestCoordinator_Ports_t *ports);

DomainRequestCoordinator_Status_t DomainRequestCoordinator_HandleMobileRequest(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    uint32_t nowMs);

DomainRequestCoordinator_Status_t DomainRequestCoordinator_HandleBcmResult(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs);

/* Post-commit path used by DomainIfResultBinding's optional observer. */
DomainRequestCoordinator_Status_t
DomainRequestCoordinator_OnBcmResultCommitted(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_BcmCommandResult_t *result,
    uint32_t nowMs);

DomainIf_Status_t DomainRequestCoordinator_OnBcmCommandResult(
    const DomainIf_BcmCommandResult_t *result);

DomainRequestCoordinator_Status_t DomainRequestCoordinator_Poll(
    DomainRequestCoordinator_t *coordinator,
    uint32_t nowMs,
    size_t *outTimeoutCount);

/*
 * Optional enqueue-only C2 ingress binding. Configure only in the serialized
 * Domain owner before Network ingress. It does not enable a Network transport.
 */
DomainRequestCoordinator_Status_t DomainRequestCoordinator_BindIngress(
    DomainRequestCoordinator_t *coordinator,
    DomainRequestCoordinator_TimeProvider_t getNowMs,
    void *timeContext,
    uint32_t sourceValidityMs);

/*
 * Host/integration seam for a future private Network admission binding. It is
 * separate from DomainIf_RxMobileRequest(), which retains its existing
 * fail-closed SignalMeta_t contract.
 */
DomainRequestCoordinator_Status_t
DomainRequestCoordinator_ConfigureAdmissionEvidence(
    DomainRequestCoordinator_t *coordinator,
    uint32_t evidenceValidityMs);

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_UpdateAdmissionContext(
    DomainRequestCoordinator_t *coordinator,
    const DomainRequestCoordinator_ContextSnapshot_t *snapshot);

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_InvalidateAdmissionContext(
    DomainRequestCoordinator_t *coordinator,
    DomainRequestCoordinator_AdmissionInvalidation_t reason);

DomainRequestCoordinator_Status_t
DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
    DomainRequestCoordinator_t *coordinator,
    const DomainIf_MobileRequest_t *request,
    const DomainRequestCoordinator_AdmissionEvidence_t *evidence,
    uint32_t nowMs);

/* Init/retry helper; caller must keep Network ingress closed while resetting. */
void DomainRequestCoordinator_ResetBinding(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_REQUEST_COORDINATOR_H */
