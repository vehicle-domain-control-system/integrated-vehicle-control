#include "domain_network_admission_bridge_private.h"
#include "domain_snapshot_commit_private.h"

#include <string.h>

static DomainNetworkAdmissionBridgePrivate_t *g_boundBridge;

static void Bridge_PollContext(uint32_t nowMs)
{
    (void)nowMs;
    if ((g_boundBridge != NULL) && g_boundBridge->contextValid)
    {
        /* Monitoring readiness is independent of ESP32 command SessionReady.
         * Revoke a previously ready monitoring context at Poll, even when no
         * frame reaches the dispatch guard. Never promote a query-only context. */
        if (g_boundBridge->monitoringOnly ||
            g_boundBridge->currentContext.monitoringSessionReady)
            (void)DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(
                g_boundBridge->currentContext.deviceContextId,
                g_boundBridge->currentContext.sessionId);
        else
            (void)DomainIf_ValidatePassiveQueryContext(
                g_boundBridge->currentContext.deviceContextId,
                g_boundBridge->currentContext.sessionId);
    }
}

static DomainNetworkAdmissionBridgePrivate_Status_t Bridge_Invalidate(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    DomainRequestCoordinator_AdmissionInvalidation_t reason)
{
    DomainRequestCoordinator_Status_t status;

    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled)
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_NOT_INITIALIZED;
    }
    status = bridge->monitoringOnly ? DOMAIN_REQUEST_COORDINATOR_OK :
        DomainRequestCoordinator_InvalidateAdmissionContext(bridge->coordinator, reason);
    (void)memset(&bridge->currentContext, 0,
                 sizeof(bridge->currentContext));
    bridge->contextValid = false;
    DomainSnapshotCommit_Notify(0U, NULL, 0U);
    bridge->requestCaptured = false;
    bridge->connectionCaptured = false;
    bridge->proximityCaptured = false;
    bridge->receivingDevice = 0U;
    bridge->receivingSession = 0U;
    (void)memset(&bridge->receivedConnection, 0,
                 sizeof(bridge->receivedConnection));
    (void)memset(&bridge->receivedProximity, 0,
                 sizeof(bridge->receivedProximity));
    return (status == DOMAIN_REQUEST_COORDINATOR_OK)
               ? DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK
               : DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_REJECTED;
}

static uint32_t Bridge_NextRevision(
    DomainNetworkAdmissionBridgePrivate_t *bridge)
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
           (left->monitoringSessionReady == right->monitoringSessionReady) &&
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
    DomainNetworkAdmissionBridgePrivate_t *bridge = g_boundBridge;

    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        !bridge->inOwnerCall || (request == NULL) ||
        bridge->requestCaptured)
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    bridge->capturedRequest = *request;
    bridge->requestCaptured = true;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t Bridge_CaptureConnection(
    DeviceContextId_t device, const DigitalKeyConnectionState_t *connection)
{
    DomainNetworkAdmissionBridgePrivate_t *bridge = g_boundBridge;
    DomainIf_Status_t status;
    if ((bridge == NULL) || !bridge->enabled || !bridge->inOwnerCall ||
        (connection == NULL) || bridge->connectionCaptured ||
        (device == 0U) || (device != bridge->receivingDevice) ||
        (connection->sessionId != bridge->receivingSession))
        return DOMAIN_IF_NOT_AVAILABLE;
    if (bridge->connectionOwner != NULL)
    {
        status = bridge->connectionOwner(device, connection);
        if (status != DOMAIN_IF_OK) return status;
    }
    bridge->receivedConnection = *connection;
    bridge->connectionCaptured = true;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t Bridge_CaptureProximity(
    DeviceContextId_t device, const ProximityInput_t *proximity)
{
    DomainNetworkAdmissionBridgePrivate_t *bridge = g_boundBridge;
    DomainIf_Status_t status;
    if ((bridge == NULL) || !bridge->enabled || !bridge->inOwnerCall ||
        (proximity == NULL) || !bridge->connectionCaptured ||
        bridge->proximityCaptured || (device != bridge->receivingDevice))
        return DOMAIN_IF_NOT_AVAILABLE;
    if (bridge->proximityOwner != NULL)
    {
        status = bridge->proximityOwner(device, proximity);
        if (status != DOMAIN_IF_OK) return status;
    }
    bridge->receivedProximity = *proximity;
    bridge->proximityCaptured = true;
    return DOMAIN_IF_OK;
}

static NetStatus Bridge_AcceptContext(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    const NetUpperUartDelivery *delivery,
    const NetUpperDelivery *metadata)
{
    DomainRequestCoordinator_ContextSnapshot_t snapshot;
    const NetWireMContext *wireContext;
    DomainRequestCoordinator_Status_t status;
    uint32_t observedAtMs;

    if (!bridge->connectionCaptured || !bridge->proximityCaptured ||
        bridge->receivedConnection.meta.quality != DATA_QUALITY_OK ||
        bridge->receivedConnection.appMeta.quality != DATA_QUALITY_OK ||
        /* Preserve the existing DOMAIN 2000 ms delivery-age bound for
         * required lanes when optional proximity carries unknown AGE.
         * This is not an approved Production SOURCE_AGE TTL. */
        bridge->receivedConnection.meta.ageMs >= 2000U ||
        bridge->receivedConnection.appMeta.ageMs >= 2000U ||
        metadata->local_elapsed_ms >= 2000U ||
        (!bridge->monitoringOnly && bridge->getContextEvidence == NULL) ||
        (!bridge->monitoringOnly && bridge->mapNetworkTime == NULL) ||
        (delivery->port_epoch == 0U) ||
        (metadata->capture.port_epoch != delivery->port_epoch) ||
        !metadata->capture.timestamp_known ||
        (!bridge->monitoringOnly && !bridge->mapNetworkTime(bridge->providerContext,
                                metadata->capture.captured_at_ms,
                                &observedAtMs)))
    {
        (void)Bridge_Invalidate(
            bridge,
            DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return NET_HOLD;
    }
    wireContext = &delivery->message.data.m_context;
    (void)memset(&snapshot, 0, sizeof(snapshot));
    if (bridge->monitoringOnly)
    {
        const DigitalKeyConnectionState_t *c = &bridge->receivedConnection;
        NetUpper *service = bridge->monitoringAdapter->service;
        uint32_t now = (uint32_t)metadata->delivered_at_ms;
        uint32_t elapsed = now - bridge->monitoringDeliveredAtMs;
        uint64_t connectionAge = c->meta.ageMs, appAge = c->appMeta.ageMs;
        bool same = bridge->contextValid &&
            bridge->currentContext.deviceContextId == wireContext->device_context_id &&
            bridge->currentContext.sessionId == wireContext->session_id &&
            bridge->currentContext.portEpoch == delivery->port_epoch;
        /* P6 is verified by the approved, exclusive ESP32 Gateway producer.
         * These are read-only channel assertions, never Domain P6 evidence.
         * A repeated source sequence must not move its observation forward. */
        if (wireContext->device_context_id == 0U || wireContext->session_id == 0U ||
            c->registration != DEVICE_REGISTRATION_REGISTERED ||
            c->btConnection != BT_CONNECTION_CONNECTED || c->appActive != APP_ACTIVE_ACTIVE ||
            c->meta.updateSequence == 0U || c->appMeta.updateSequence == 0U ||
            c->meta.ageMs >= DOMAIN_MONITORING_CONNECTION_TTL_MS ||
            c->appMeta.ageMs >= DOMAIN_MONITORING_APP_TTL_MS ||
            service->link_failed[NET_WIRE_UART_M] || service->quiescing[NET_WIRE_UART_M] ||
            delivery->port_epoch != service->config.port_epoch[NET_WIRE_UART_M] ||
            (uint32_t)(now - (uint32_t)metadata->capture.captured_at_ms) >=
                DOMAIN_MONITORING_CONTEXT_VALIDITY_MS)
        {
            (void)Bridge_Invalidate(bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
            return NET_HOLD;
        }
        if (same)
        {
            uint32_t d1 = c->meta.updateSequence - bridge->monitoringConnectionSequence;
            uint32_t d2 = c->appMeta.updateSequence - bridge->monitoringAppSequence;
            /* Receiver-local conservative lease only; decoded source metadata
             * stays untouched. Repetition cannot renew an old source, and
             * independent clock rounding need not create a false revocation. */
            if (d1 == 0U && connectionAge < (uint64_t)bridge->monitoringConnectionAge + elapsed)
                connectionAge = (uint64_t)bridge->monitoringConnectionAge + elapsed;
            if (d2 == 0U && appAge < (uint64_t)bridge->monitoringAppAge + elapsed)
                appAge = (uint64_t)bridge->monitoringAppAge + elapsed;
            if (d1 >= 0x80000000U || d2 >= 0x80000000U ||
                connectionAge >= DOMAIN_MONITORING_CONNECTION_TTL_MS ||
                appAge >= DOMAIN_MONITORING_APP_TTL_MS)
            {
                (void)Bridge_Invalidate(bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_CONTEXT_CHANGED);
                return NET_HOLD;
            }
        }
        else DomainSnapshotCommit_Notify(0U, NULL, 0U);
        snapshot.deviceContextId = wireContext->device_context_id;
        snapshot.sessionId = wireContext->session_id;
        snapshot.portEpoch = delivery->port_epoch;
        snapshot.networkBootId = bridge->networkBootId;
        snapshot.observedAtMs = (uint32_t)metadata->capture.captured_at_ms;
        snapshot.monitoringSessionReady = true;
        /* registrationVerified/sessionVerified/p6AuthorizationVerified remain
         * false: this context cannot enter the existing Admission path. */
        bridge->currentContext = snapshot;
        bridge->monitoringDeliveredAtMs = now;
        bridge->monitoringConnectionAge = (uint32_t)connectionAge;
        bridge->monitoringAppAge = (uint32_t)appAge;
        bridge->monitoringConnectionSequence = c->meta.updateSequence;
        bridge->monitoringAppSequence = c->appMeta.updateSequence;
        bridge->contextValid = true;
        return NET_OK;
    }
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
        DomainSnapshotCommit_Notify(0U, NULL, 0U);
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
    DomainNetworkAdmissionBridgePrivate_t *bridge,
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
    DomainNetworkAdmissionBridgePrivate_t *bridge =
        (DomainNetworkAdmissionBridgePrivate_t *)context;
    NetStatus status;

    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        (delivery == NULL) || (metadata == NULL) ||
        (bridge->delegate.uart == NULL) || bridge->inOwnerCall)
    {
        return NET_INVALID_ARGUMENT;
    }
    bridge->inOwnerCall = true;
    if (bridge->monitoringOnly && delivery->message.link == NET_WIRE_UART_M &&
        delivery->message.type == 0x10U)
    {
        bridge->inOwnerCall = false;
        return NET_HOLD;
    }
    if ((delivery->message.link == NET_WIRE_UART_M) &&
        (delivery->message.type == 0x10U))
    {
        status = Bridge_DeliverRequest(bridge, delivery, metadata);
    }
    else
    {
        if ((delivery->message.link == NET_WIRE_UART_M) &&
            (delivery->message.type == 0x01U))
        {
            bridge->receivingDevice = delivery->message.data.m_context.device_context_id;
            bridge->receivingSession = delivery->message.data.m_context.session_id;
            bridge->connectionCaptured = false;
            bridge->proximityCaptured = false;
        }
        status = bridge->delegate.uart(
            bridge->delegate.context, delivery, metadata);
        if ((status == NET_OK) &&
            (delivery->message.link == NET_WIRE_UART_M) &&
            (delivery->message.type == 0x01U))
        {
            status = Bridge_AcceptContext(bridge, delivery, metadata);
        }
        else if ((delivery->message.link == NET_WIRE_UART_M) &&
                 (delivery->message.type == 0x01U))
        {
            (void)Bridge_Invalidate(bridge,
                DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
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
    DomainNetworkAdmissionBridgePrivate_t *bridge = context;
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
    DomainNetworkAdmissionBridgePrivate_t *bridge = context;
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
    DomainNetworkAdmissionBridgePrivate_t *bridge = context;
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
    DomainNetworkAdmissionBridgePrivate_t *bridge = context;
    return (bridge->delegate.held_uart == NULL)
               ? NET_HOLD
               : bridge->delegate.held_uart(
                     bridge->delegate.context, delivery, metadata);
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_InitMonitoring(
    DomainNetworkAdmissionBridgePrivate_t *bridge, NetDomainC2 *adapter)
{
    if (bridge == NULL || adapter == NULL || adapter->service == NULL ||
        adapter->config.domain_boot == 0U ||
        adapter->service->config.port_epoch[NET_WIRE_UART_M] == 0U)
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT;
    (void)memset(bridge, 0, sizeof(*bridge));
    bridge->monitoringAdapter = adapter;
    bridge->networkBootId = adapter->config.domain_boot;
    bridge->monitoringOnly = true;
    bridge->initialized = bridge->enabled = true;
    return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK;
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_Init(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    DomainRequestCoordinator_t *coordinator,
    uint32_t networkBootId,
    DomainNetworkAdmissionBridgePrivate_ContextProvider_t getContextEvidence,
    DomainNetworkAdmissionBridgePrivate_TimeMap_t mapNetworkTime,
    DomainNetworkAdmissionBridgePrivate_NowProvider_t getNowMs,
    void *providerContext)
{
    if ((bridge == NULL) || (coordinator == NULL) ||
        (networkBootId == 0U) || (getContextEvidence == NULL) ||
        (mapNetworkTime == NULL) || (getNowMs == NULL))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT;
    }
    (void)memset(bridge, 0, sizeof(*bridge));
    bridge->coordinator = coordinator;
    bridge->networkBootId = networkBootId;
    bridge->getContextEvidence = getContextEvidence;
    bridge->mapNetworkTime = mapNetworkTime;
    bridge->getNowMs = getNowMs;
    bridge->providerContext = providerContext;
    bridge->initialized = true;
    bridge->enabled = true;
    return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK;
}

bool DomainNetworkAdmissionBridgePrivate_IsEnabled(
    const DomainNetworkAdmissionBridgePrivate_t *bridge)
{
    return (bridge != NULL) && bridge->initialized && bridge->enabled;
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_BindApi(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    NetDomainC2Api *api)
{
    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        (api == NULL))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT;
    }
    if ((g_boundBridge != NULL) && (g_boundBridge != bridge))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_ALREADY_BOUND;
    }
    g_boundBridge = bridge;
    DomainSnapshotCommit_SetContextPoll(Bridge_PollContext);
    if (api->RxDigitalKeyConnection != Bridge_CaptureConnection)
        bridge->connectionOwner = api->RxDigitalKeyConnection;
    if (api->RxProximity != Bridge_CaptureProximity)
        bridge->proximityOwner = api->RxProximity;
    api->RxDigitalKeyConnection = Bridge_CaptureConnection;
    api->RxProximity = Bridge_CaptureProximity;
    if (!bridge->monitoringOnly) api->RxMobileRequest = Bridge_CaptureRequest;
    return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK;
}

NetUpperOwner DomainNetworkAdmissionBridgePrivate_WrapOwner(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    NetUpperOwner delegate)
{
    NetUpperOwner owner;

    (void)memset(&owner, 0, sizeof(owner));
    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        (delegate.uart == NULL))
    {
        return owner;
    }
    bridge->delegate = delegate;
    owner.context = bridge;
    /* NULL selects the existing generic CAN Owner for CIS Rear. Installing a
     * wrapper over a missing specialized callback would suppress that route. */
    owner.rear = delegate.rear != NULL ? Bridge_Rear : NULL;
    owner.can = Bridge_Can;
    owner.uart = Bridge_Uart;
    owner.held_can = Bridge_HeldCan;
    owner.held_uart = Bridge_HeldUart;
    return owner;
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnLinkDown(
    DomainNetworkAdmissionBridgePrivate_t *bridge)
{
    return Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_LINK_DOWN);
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnPortEpochChanged(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    uint64_t newPortEpoch)
{
    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        (newPortEpoch == 0U))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT;
    }
    if (bridge->contextValid &&
        (newPortEpoch == bridge->currentContext.portEpoch))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK;
    }
    return Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_PORT_EPOCH_CHANGED);
}

DomainNetworkAdmissionBridgePrivate_Status_t
DomainNetworkAdmissionBridgePrivate_OnNetworkBootChanged(
    DomainNetworkAdmissionBridgePrivate_t *bridge,
    uint32_t newNetworkBootId)
{
    DomainNetworkAdmissionBridgePrivate_Status_t status;

    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        (newNetworkBootId == 0U))
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_INVALID_ARGUMENT;
    }
    if (newNetworkBootId == bridge->networkBootId)
    {
        return DOMAIN_NETWORK_ADMISSION_BRIDGE_PRIVATE_OK;
    }
    status = Bridge_Invalidate(
        bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_NETWORK_BOOT_CHANGED);
    bridge->networkBootId = newNetworkBootId;
    return status;
}

void DomainNetworkAdmissionBridgePrivate_ResetBinding(void)
{
    DomainSnapshotCommit_SetContextPoll(NULL);
    DomainSnapshotCommit_Notify(0U, NULL, 0U);
    g_boundBridge = NULL;
}

static DomainIf_Status_t Bridge_ValidateContext(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    bool requireMonitoringReady)
{
    DomainNetworkAdmissionBridgePrivate_t *bridge = g_boundBridge;
    DomainRequestCoordinator_ContextSnapshot_t evidence;
    uint32_t nowMs;

    if ((deviceContextId == 0U) || (sessionId == 0U))
    {
        return DOMAIN_IF_INVALID_ARGUMENT;
    }
    if ((bridge == NULL) || !bridge->initialized || !bridge->enabled ||
        !bridge->contextValid || (!bridge->monitoringOnly && bridge->getContextEvidence == NULL))
    {
        return DOMAIN_IF_NOT_AVAILABLE;
    }
    if ((bridge->currentContext.deviceContextId != deviceContextId) ||
        (bridge->currentContext.sessionId != sessionId))
    {
        return DOMAIN_IF_REJECTED;
    }

    if (bridge->monitoringOnly)
    {
        NetDomainC2 *adapter = bridge->monitoringAdapter;
        NetUpper *s = adapter->service;
        uint64_t networkNow;
        uint32_t elapsed;
        if (!requireMonitoringReady) return DOMAIN_IF_NOT_AVAILABLE;
        networkNow = s->last_now_ms; /* Serialized dispatch already read the clock. */
        if (!s->in_call && net_upper_now(s, &networkNow) != NET_OK)
        {
            (void)Bridge_Invalidate(bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
            return DOMAIN_IF_REJECTED;
        }
        elapsed = (uint32_t)networkNow - bridge->monitoringDeliveredAtMs;
        if (!adapter->registered || adapter->config.domain_boot != bridge->networkBootId ||
            s->link_failed[NET_WIRE_UART_M] || s->quiescing[NET_WIRE_UART_M] ||
            !s->context_seen || s->context.device_context_id != deviceContextId ||
            s->context.session_id != sessionId ||
            s->context_port_epoch != bridge->currentContext.portEpoch ||
            s->config.port_epoch[NET_WIRE_UART_M] != bridge->currentContext.portEpoch ||
            (uint32_t)((uint32_t)networkNow - bridge->currentContext.observedAtMs) >=
                DOMAIN_MONITORING_CONTEXT_VALIDITY_MS ||
            (uint64_t)bridge->monitoringConnectionAge + elapsed >= DOMAIN_MONITORING_CONNECTION_TTL_MS ||
            (uint64_t)bridge->monitoringAppAge + elapsed >= DOMAIN_MONITORING_APP_TTL_MS)
        {
            (void)Bridge_Invalidate(bridge, DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
            return DOMAIN_IF_REJECTED;
        }
        return DOMAIN_IF_OK;
    }

    /* Reuse the Context lifetime already enforced by the Admission owner;
     * this is not a new Production TTL or an authorization shortcut. */
    if ((bridge->coordinator == NULL) ||
        !bridge->coordinator->admissionEvidenceEnabled ||
        !bridge->coordinator->admissionContextValid ||
        (bridge->coordinator->admissionEvidenceValidityMs == 0U) ||
        (bridge->coordinator->admissionContext.contextRevision !=
         bridge->currentContext.contextRevision) ||
        (bridge->currentContext.networkBootId != bridge->networkBootId) ||
        !bridge->currentContext.timestampKnown || (bridge->getNowMs == NULL) ||
        !bridge->getNowMs(bridge->providerContext, &nowMs) ||
        ((uint32_t)(nowMs - bridge->currentContext.observedAtMs) >=
         bridge->coordinator->admissionEvidenceValidityMs))
    {
        (void)Bridge_Invalidate(bridge,
            DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return DOMAIN_IF_REJECTED;
    }

    (void)memset(&evidence, 0, sizeof(evidence));
    if (!bridge->getContextEvidence(bridge->providerContext,
                                    deviceContextId,
                                    sessionId,
                                    &evidence) ||
        !evidence.linkUp || !evidence.registrationVerified ||
        !evidence.registered || !evidence.connectionVerified ||
        !evidence.connected || !evidence.sessionVerified ||
        !evidence.p6AuthorizationVerified)
    {
        (void)Bridge_Invalidate(bridge,
            DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return DOMAIN_IF_REJECTED;
    }
    if (requireMonitoringReady &&
        (!bridge->currentContext.monitoringSessionReady ||
         !evidence.monitoringSessionReady))
    {
        if (bridge->currentContext.monitoringSessionReady)
            (void)Bridge_Invalidate(bridge,
                DOMAIN_REQUEST_ADMISSION_INVALIDATION_AUTHORIZATION_REVOKED);
        return DOMAIN_IF_REJECTED;
    }
    return DOMAIN_IF_OK;
}

DomainIf_Status_t DomainIf_ValidatePassiveQueryContext(
    DeviceContextId_t deviceContextId, SessionId_t sessionId)
{
    return Bridge_ValidateContext(deviceContextId, sessionId, false);
}

DomainIf_Status_t DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(
    DeviceContextId_t deviceContextId, SessionId_t sessionId)
{
    return Bridge_ValidateContext(deviceContextId, sessionId, true);
}
