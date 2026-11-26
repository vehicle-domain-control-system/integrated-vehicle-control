#ifndef DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_H
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

#include "domain_request_coordinator.h"
#include "network/net_domain_c2.h"

/*
 * Production-private integration seam. It wraps the real Network DOMAIN owner
 * so the DTO produced by the product decoder and its transport delivery
 * metadata are copied atomically into the Domain admission inbox. Zeroed
 * storage is disabled; Init requires authoritative P6/time providers. The
 * separate InitMonitoring uses the approved demo Gateway trust contract and
 * cannot create P6/Request/Query authorization. This is
 * not a public C2 API and is not installed by the pinned owner by default.
 */

typedef bool (*DomainNetworkAdmissionBridgePrivate_ContextProvider_t)(
    void *context,
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    DomainRequestCoordinator_ContextSnapshot_t *outSnapshot);

typedef bool (*DomainNetworkAdmissionBridgePrivate_TimeMap_t)(
    void *context,
    uint64_t networkMs,
    uint32_t *outDomainMs);

typedef bool (*DomainNetworkAdmissionBridgePrivate_NowProvider_t)(
    void *context,
    uint32_t *outNowMs);

typedef enum
{
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK = 0,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_NOT_INITIALIZED,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_ALREADY_BOUND,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_REJECTED
} DomainNetworkAdmissionBridgePrivate_Status_t;

typedef struct
{
    DomainRequestCoordinator_t *coordinator;
    NetUpperOwner delegate;
    DomainNetworkAdmissionBridgePrivate_ContextProvider_t getContextEvidence;
    DomainNetworkAdmissionBridgePrivate_TimeMap_t mapNetworkTime;
    DomainNetworkAdmissionBridgePrivate_NowProvider_t getNowMs;
    void *providerContext;
    DomainRequestCoordinator_ContextSnapshot_t currentContext;
    DomainIf_MobileRequest_t capturedRequest;
    /* Decoded observations only; neither is proof of P6 authorization.
     * Borrowed C2 arguments are copied before the Network callback returns. */
    DigitalKeyConnectionState_t receivedConnection;
    ProximityInput_t receivedProximity;
    DeviceContextId_t receivingDevice;
    SessionId_t receivingSession;
    DomainIf_Status_t (*connectionOwner)(DeviceContextId_t,
        const DigitalKeyConnectionState_t *);
    DomainIf_Status_t (*proximityOwner)(DeviceContextId_t,
        const ProximityInput_t *);
    uint32_t networkBootId;
    uint32_t nextContextRevision;
    uint32_t requestDeliveryCount;
    bool contextValid;
    bool requestCaptured;
    bool connectionCaptured;
    bool proximityCaptured;
    bool inOwnerCall;
    bool initialized;
    bool enabled;
    /* Demo-only trusted physical Gateway path; never P6/Request evidence. */
    NetDomainC2 *monitoringAdapter;
    uint32_t monitoringDeliveredAtMs;
    uint32_t monitoringConnectionAge, monitoringAppAge;
    uint32_t monitoringConnectionSequence, monitoringAppSequence;
    bool monitoringOnly;
} DomainNetworkAdmissionBridgePrivate_t;

/* Adopted demo policy, independent from control admission and status TTLs. */
#define DOMAIN_MONITORING_CONTEXT_VALIDITY_MS 750U
#define DOMAIN_MONITORING_CONNECTION_TTL_MS 600U
#define DOMAIN_MONITORING_APP_TTL_MS 1000U
DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_InitMonitoring(
    DomainNetworkAdmissionBridgePrivate_t *bridge, NetDomainC2 *adapter);

bool DomainNetworkAdmissionBridgePrivate_IsEnabled(
    const DomainNetworkAdmissionBridgePrivate_t *bridge);

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_Init(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    DomainRequestCoordinator_t *coordinator,
    uint32_t networkBootId,
    DomainNetworkAdmissionBridgePrivate_ContextProvider_t getContextEvidence,
    DomainNetworkAdmissionBridgePrivate_TimeMap_t mapNetworkTime,
    DomainNetworkAdmissionBridgePrivate_NowProvider_t getNowMs,
    void *providerContext);

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_BindApi(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    NetDomainC2Api *api);

NetUpperOwner DomainNetworkAdmissionBridgePrivate_WrapOwner(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    NetUpperOwner delegate);

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnLinkDown(
    DomainNetworkAdmissionBridgePrivate_t *bridge);

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnPortEpochChanged(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    uint64_t newPortEpoch);

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnNetworkBootChanged(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    uint32_t newNetworkBootId);

void DomainNetworkAdmissionBridgePrivate_ResetBinding(void);

/* Protected state publication only. Existing Query/Result authorization is
 * separate; this function never enables M_REQUEST or its Coordinator. */
DomainIf_Status_t DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(
    DeviceContextId_t deviceContextId, SessionId_t sessionId);

#endif /* DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_H */


