#ifndef DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_H
#define DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include "domain_request_coordinator.h"
#include "network/net_domain_c2.h"

/*
 * Host-only integration seam. It wraps the real Network DOMAIN owner so the
 * DTO produced by the product decoder and its transport delivery metadata can
 * be copied atomically into the Domain admission inbox. It is not a public C2
 * API and is never installed by the Production pinned owner binding.
 */

typedef bool (*DomainNetworkAdmissionBridgeHost_ContextProvider_t)(
    void *context,
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    DomainRequestCoordinator_ContextSnapshot_t *outSnapshot);

typedef bool (*DomainNetworkAdmissionBridgeHost_TimeMap_t)(
    void *context,
    uint64_t networkMs,
    uint32_t *outDomainMs);

typedef bool (*DomainNetworkAdmissionBridgeHost_NowProvider_t)(
    void *context,
    uint32_t *outNowMs);

typedef enum
{
    DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK = 0,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_INVALID_ARGUMENT,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_NOT_INITIALIZED,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_ALREADY_BOUND,
    DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_REJECTED
} DomainNetworkAdmissionBridgeHost_Status_t;

typedef struct
{
    DomainRequestCoordinator_t *coordinator;
    NetUpperOwner delegate;
    DomainNetworkAdmissionBridgeHost_ContextProvider_t getContextEvidence;
    DomainNetworkAdmissionBridgeHost_TimeMap_t mapNetworkTime;
    DomainNetworkAdmissionBridgeHost_NowProvider_t getNowMs;
    void *providerContext;
    DomainRequestCoordinator_ContextSnapshot_t currentContext;
    DomainIf_MobileRequest_t capturedRequest;
    uint32_t networkBootId;
    uint32_t nextContextRevision;
    uint32_t requestDeliveryCount;
    bool contextValid;
    bool requestCaptured;
    bool inOwnerCall;
    bool initialized;
} DomainNetworkAdmissionBridgeHost_t;

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_Init(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    DomainRequestCoordinator_t *coordinator,
    uint32_t networkBootId,
    DomainNetworkAdmissionBridgeHost_ContextProvider_t getContextEvidence,
    DomainNetworkAdmissionBridgeHost_TimeMap_t mapNetworkTime,
    DomainNetworkAdmissionBridgeHost_NowProvider_t getNowMs,
    void *providerContext);

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_BindApi(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    NetDomainC2Api *api);

NetUpperOwner DomainNetworkAdmissionBridgeHost_WrapOwner(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    NetUpperOwner delegate);

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnLinkDown(
    DomainNetworkAdmissionBridgeHost_t *bridge);

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnPortEpochChanged(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    uint64_t newPortEpoch);

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnNetworkBootChanged(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    uint32_t newNetworkBootId);

void DomainNetworkAdmissionBridgeHost_ResetBinding(void);

#endif /* DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_H */
