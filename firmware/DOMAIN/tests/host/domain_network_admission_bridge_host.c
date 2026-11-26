#include "domain_network_admission_bridge_host.h"

#include <string.h>

static DomainNetworkAdmissionBridgeHost_t *g_boundBridge;

static DomainNetworkAdmissionBridgeHost_Status_t Bridge_Invalidate(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    DomainRequestCoordinator_AdmissionInvalidation_t reason)
{
    DomainRequestCoordinator_Status_t status;

    if ((bridge == NULL) || !bridge->initialized)
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_NOT_INITIALIZED;
    }
    status = DomainRequestCoordinator_InvalidateAdmissionContext(
        bridge->coordinator, reason);
    (void)memset(&bridge->currentContext, 0,
                 sizeof(bridge->currentContext));
    bridge->contextValid = false;
    bridge->requestCaptured = false;
    return (status == DOMAIN_REQUEST_COORDINATOR_OK)
               ? DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK
               : DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_REJECTED;
}

static uint32_t Bridge_NextRevision(
    DomainNetworkAdmissionBridgeHost_t *bridge)
{
    bridge->nextContextRevision += 1U;
    if (bridge->nextContextRevision == 0U)
    {
        bridge->nextContextRevision = 1U;
    }
    return bridge->nextContextRevision;
}

static bool Bridge_SameSecurityContext(
    const DomainRequestCoordinator_ContextSnapshot_t *left,
    const DomainRequestCoordinator_ContextSnapshot_t *right)
{
    return (left->deviceContextId == right->deviceContextId) &&
           (left->sessionId == right->sessionId) &&
           (left->portEpoch == right->portEpoch) &&
           (left->networkBootId == right->networkBootId) &&
           (left->linkUp == right->linkUp) &&
           (left->registrationVerified == right->registrationVerified) &&
           (left->registered == right->registered) &&
           (left->connectionVerified == right->connectionVerified) &&
           (left->connected == right->connected) &&
           (left->sessionVerified == right->sessionVerified) &&
           (left->p6AuthorizationVerified ==
            right->p6AuthorizationVerified);
}

static NetStatus Bridge_MapCoordinatorStatus(
    DomainRequestCoordinator_Status_t status)
{
    switch (status)
    {
        case DOMAIN_REQUEST_COORDINATOR_OK:
            return NET_OK;
        case DOMAIN_REQUEST_COORDINATOR_CAPACITY_EXCEEDED:
            return NET_NO_CAPACITY;
        case DOMAIN_REQUEST_COORDINATOR_INVALID_ARGUMENT:
            return NET_INVALID_ARGUMENT;
        case DOMAIN_REQUEST_COORDINATOR_NOT_INITIALIZED:
            return NET_NOT_CONFIGURED;
        default:
            return NET_HOLD;
    }
}

static DomainIf_Status_t Bridge_CaptureRequest(
    const DomainIf_MobileRequest_t *request)
{
    DomainNetworkAdmissionBridgeHost_t *bridge = g_boundBridge;

    if ((bridge == NULL) || !bridge->initialized ||
        !bridge->inOwnerCall || (request == NULL) ||
        bridge->requestCaptured)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    bridge->capturedRequest = *request;
    bridge->requestCaptured = true;
    return DOMAIN_IF_OK;
}

static NetStatus Bridge_AcceptContext(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    const NetUpperUartDelivery *delivery,
    const NetUpperDelivery *metadata)
{
    DomainRequestCoordinator_ContextSnapshot_t snapshot;
    const NetWireMContext *wireContext;
    DomainRequestCoordinator_Status_t status;
    uint32_t observedAtMs;

    if ((bridge->getContextEvidence == NULL) ||
        (bridge->mapNetworkTime == NULL) ||
        (delivery->port_epoch == 0U) ||
        (metadata->capture.port_epoch != delivery->port_epoch) ||
        !metadata->capture.timestamp_known ||
        !bridge->mapNetworkTime(bridge->providerContext,
                                metadata->capture.captured_at_ms,
                                &observedAtMs))
    {
        (void)Bridge_Invalidate(
            bridge,
            DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return NET_HOLD;
    }
    wireContext = &delivery->message.data.m_context;
    (void)memset(&snapshot, 0, sizeof(snapshot));
    if (!bridge->getContextEvidence(bridge->providerContext,
                                    wireContext->device_context_id,
                                    wireContext->session_id,
                                    &snapshot))
    {
        (void)Bridge_Invalidate(
            bridge,
            DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return NET_HOLD;
    }
    snapshot.deviceContextId = wireContext->device_context_id;
    snapshot.sessionId = wireContext->session_id;
    snapshot.portEpoch = delivery->port_epoch;
    snapshot.networkBootId = bridge->networkBootId;
    snapshot.observedAtMs = observedAtMs;
    snapshot.timestampKnown = true;
    if (!bridge->contextValid ||
        !Bridge_SameSecurityContext(&bridge->currentContext, &snapshot))
    {
        snapshot.contextRevision = Bridge_NextRevision(bridge);
    }
    else
    {
        snapshot.contextRevision =
            bridge->currentContext.contextRevision;
    }
    status = DomainRequestCoordinator_UpdateAdmissionContext(
        bridge->coordinator, &snapshot);
    if (status != DOMAIN_REQUEST_COORDINATOR_OK)
    {
        bridge->contextValid = false;
        return NET_HOLD;
    }
    bridge->currentContext = snapshot;
    bridge->contextValid = true;
    return NET_OK;
}

static NetStatus Bridge_DeliverRequest(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    const NetUpperUartDelivery *delivery,
    const NetUpperDelivery *metadata)
{
    const NetWireMRequest *wireRequest =
        &delivery->message.data.m_request;
    DomainRequestCoordinator_AdmissionEvidence_t evidence;
    DomainRequestCoordinator_Status_t status;
    uint32_t capturedAtMs;
    uint32_t nowMs;
    NetStatus delegateStatus;

    if (!bridge->contextValid ||
        !bridge->currentContext.p6AuthorizationVerified ||
        (wireRequest->device_context_id !=
         bridge->currentContext.deviceContextId) ||
        (wireRequest->session_id != bridge->currentContext.sessionId) ||
        (delivery->port_epoch != bridge->currentContext.portEpoch) ||
        (metadata->capture.port_epoch != delivery->port_epoch) ||
        !metadata->capture.timestamp_known ||
        (bridge->mapNetworkTime == NULL) ||
        (bridge->getNowMs == NULL) ||
        !bridge->mapNetworkTime(bridge->providerContext,
                                metadata->capture.captured_at_ms,
                                &capturedAtMs) ||
        !bridge->getNowMs(bridge->providerContext, &nowMs))
    {
        return NET_HOLD;
    }

    bridge->requestCaptured = false;
    delegateStatus = bridge->delegate.uart(
        bridge->delegate.context, delivery, metadata);
    if ((delegateStatus != NET_OK) || !bridge->requestCaptured)
    {
        bridge->requestCaptured = false;
        return (delegateStatus == NET_OK) ? NET_HOLD : delegateStatus;
    }

    (void)memset(&evidence, 0, sizeof(evidence));
    evidence.deviceContextId = bridge->capturedRequest.deviceContextId;
    evidence.sessionId = bridge->capturedRequest.context.sessionId;
    evidence.portEpoch = delivery->port_epoch;
    evidence.contextRevision = bridge->currentContext.contextRevision;
    evidence.networkBootId = bridge->networkBootId;
    evidence.linkSequence = delivery->message.sequence;
    evidence.capturedAtMs = capturedAtMs;
    evidence.timestampKnown = true;
    evidence.p6AuthorizationVerified = true;
    status = DomainRequestCoordinator_EnqueueWithAdmissionEvidence(
        bridge->coordinator, &bridge->capturedRequest, &evidence, nowMs);
    bridge->requestCaptured = false;
    if (status == DOMAIN_REQUEST_COORDINATOR_OK)
    {
        bridge->requestDeliveryCount += 1U;
    }
    return Bridge_MapCoordinatorStatus(status);
}

static NetStatus Bridge_Uart(
    void *context,
    const NetUpperUartDelivery *delivery,
    const NetUpperDelivery *metadata)
{
    DomainNetworkAdmissionBridgeHost_t *bridge =
        (DomainNetworkAdmissionBridgeHost_t *)context;
    NetStatus status;

    if ((bridge == NULL) || !bridge->initialized ||
        (delivery == NULL) || (metadata == NULL) ||
        (bridge->delegate.uart == NULL) || bridge->inOwnerCall)
    {
        return NET_INVALID_ARGUMENT;
    }
    bridge->inOwnerCall = true;
    if ((delivery->message.link == NET_WIRE_UART_M) &&
        (delivery->message.type == 0x10U))
    {
        status = Bridge_DeliverRequest(bridge, delivery, metadata);
    }
    else
    {
        status = bridge->delegate.uart(
            bridge->delegate.context, delivery, metadata);
        if ((status == NET_OK) &&
            (delivery->message.link == NET_WIRE_UART_M) &&
            (delivery->message.type == 0x01U))
        {
            status = Bridge_AcceptContext(bridge, delivery, metadata);
        }
    }
    bridge->inOwnerCall = false;
    return status;
}

static NetStatus Bridge_Rear(
    void *context,
    const NetUpperRear *rear,
    const NetUpperDelivery *metadata)
{
    DomainNetworkAdmissionBridgeHost_t *bridge = context;
    return (bridge->delegate.rear == NULL)
               ? NET_HOLD
               : bridge->delegate.rear(
                     bridge->delegate.context, rear, metadata);
}

static NetStatus Bridge_Can(
    void *context,
    const NetUpperCanMessage *message,
    const NetUpperDelivery *metadata)
{
    DomainNetworkAdmissionBridgeHost_t *bridge = context;
    return (bridge->delegate.can == NULL)
               ? NET_HOLD
               : bridge->delegate.can(
                     bridge->delegate.context, message, metadata);
}

static NetStatus Bridge_HeldCan(
    void *context,
    const NetUpperCanMessage *message,
    const NetUpperDelivery *metadata)
{
    DomainNetworkAdmissionBridgeHost_t *bridge = context;
    return (bridge->delegate.held_can == NULL)
               ? NET_HOLD
               : bridge->delegate.held_can(
                     bridge->delegate.context, message, metadata);
}

static NetStatus Bridge_HeldUart(
    void *context,
    const NetUpperUartDelivery *delivery,
    const NetUpperDelivery *metadata)
{
    DomainNetworkAdmissionBridgeHost_t *bridge = context;
    return (bridge->delegate.held_uart == NULL)
               ? NET_HOLD
               : bridge->delegate.held_uart(
                     bridge->delegate.context, delivery, metadata);
}

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_Init(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    DomainRequestCoordinator_t *coordinator,
    uint32_t networkBootId,
    DomainNetworkAdmissionBridgeHost_ContextProvider_t getContextEvidence,
    DomainNetworkAdmissionBridgeHost_TimeMap_t mapNetworkTime,
    DomainNetworkAdmissionBridgeHost_NowProvider_t getNowMs,
    void *providerContext)
{
    if ((bridge == NULL) || (coordinator == NULL) ||
        (networkBootId == 0U) || (mapNetworkTime == NULL) ||
        (getNowMs == NULL))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_INVALID_ARGUMENT;
    }
    (void)memset(bridge, 0, sizeof(*bridge));
    bridge->coordinator = coordinator;
    bridge->networkBootId = networkBootId;
    bridge->getContextEvidence = getContextEvidence;
    bridge->mapNetworkTime = mapNetworkTime;
    bridge->getNowMs = getNowMs;
    bridge->providerContext = providerContext;
    bridge->initialized = true;
    return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK;
}

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_BindApi(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    NetDomainC2Api *api)
{
    if ((bridge == NULL) || !bridge->initialized || (api == NULL))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_INVALID_ARGUMENT;
    }
    if ((g_boundBridge != NULL) && (g_boundBridge != bridge))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_ALREADY_BOUND;
    }
    g_boundBridge = bridge;
    api->RxMobileRequest = Bridge_CaptureRequest;
    return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK;
}

NetUpperOwner DomainNetworkAdmissionBridgeHost_WrapOwner(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    NetUpperOwner delegate)
{
    NetUpperOwner owner;

    (void)memset(&owner, 0, sizeof(owner));
    if ((bridge == NULL) || !bridge->initialized ||
        (delegate.uart == NULL))
    {
        return owner;
    }
    bridge->delegate = delegate;
    owner.context = bridge;
    owner.rear = Bridge_Rear;
    owner.can = Bridge_Can;
    owner.uart = Bridge_Uart;
    owner.held_can = Bridge_HeldCan;
    owner.held_uart = Bridge_HeldUart;
    return owner;
}

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnLinkDown(
    DomainNetworkAdmissionBridgeHost_t *bridge)
{
    return Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN);
}

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnPortEpochChanged(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    uint64_t newPortEpoch)
{
    if ((bridge == NULL) || !bridge->initialized || (newPortEpoch == 0U))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_INVALID_ARGUMENT;
    }
    if (bridge->contextValid &&
        (newPortEpoch == bridge->currentContext.portEpoch))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK;
    }
    return Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED);
}

DomainNetworkAdmissionBridgeHost_Status_t
DomainNetworkAdmissionBridgeHost_OnNetworkBootChanged(
    DomainNetworkAdmissionBridgeHost_t *bridge,
    uint32_t newNetworkBootId)
{
    DomainNetworkAdmissionBridgeHost_Status_t status;

    if ((bridge == NULL) || !bridge->initialized ||
        (newNetworkBootId == 0U))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_INVALID_ARGUMENT;
    }
    if (newNetworkBootId == bridge->networkBootId)
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_HOST_OK;
    }
    status = Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED);
    bridge->networkBootId = newNetworkBootId;
    return status;
}

void DomainNetworkAdmissionBridgeHost_ResetBinding(void)
{
    g_boundBridge = NULL;
}
