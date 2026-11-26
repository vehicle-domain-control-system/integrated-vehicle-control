#include "network/net_domain_c2.h"
#include <string.h>
#include "net_domain_private.h"
#include "net_domain_snapshot_private.h"
#include "domain_snapshot_commit_private.h"
#include "domain_network_admission_bridge_private.h"

typedef struct {
    uint16_t id;
    bool valid, pending;
    uint64_t receipt, lastReceipt, epoch, localEpoch;
    uint32_t boot, sequence, domainBoot;
    uint32_t recipientDevice;
    uint64_t recipientSession, recipientEpoch;
    NetDomainC2 *adapter;
} SnapshotCommit;

static SnapshotCommit snapshot[7];
static NetDomainC2 *publisherAdapter;
static bool revokeQueuedStates;

NetStatus NetDomainSnapshot_Submit(const NetUpperPortOps *delegate,
    void *context, const NetUpperPacket *packet, NetTxKey key,
    uint32_t domainBoot)
{
    NetUpperUartMessage message;
    uint32_t device = 0U, boot = 0U;
    uint64_t session = 0U;
    DomainIf_Status_t authority;
    if (delegate == NULL || delegate->submit == NULL || packet == NULL)
        return NET_INVALID_ARGUMENT;
    if (packet->link == NET_WIRE_UART_M) {
        if (net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M,
                NET_WIRE_DOWNLINK, &packet->uart, &message) != NET_WIRE_OK)
            return NET_REJECTED;
        switch (message.type) {
#define MONITOR_RECIPIENT(type_, member_) \
        case type_: device = message.data.member_.device_context_id; \
            session = message.data.member_.session_id; \
            boot = message.data.member_.domain_boot_id; break
        MONITOR_RECIPIENT(0x30U, m_bcm_door_state);
        MONITOR_RECIPIENT(0x33U, m_bcm_status);
        MONITOR_RECIPIENT(0x34U, m_cis_environment);
        MONITOR_RECIPIENT(0x35U, m_cis_occupant);
        MONITOR_RECIPIENT(0x36U, m_cis_rear);
        MONITOR_RECIPIENT(0x37U, m_cis_status);
        MONITOR_RECIPIENT(0x3aU, m_vss_status);
        MONITOR_RECIPIENT(0x20U, m_result);
        MONITOR_RECIPIENT(0x40U, m_query_end);
#undef MONITOR_RECIPIENT
        default: return delegate->submit(context, packet, key);
        }
        /* Session IDs alone are not authorization. Reuse the existing P6
         * Provider and Context TTL check; absent/unbound evidence rejects.
         * A rejection promises no borrow and the service rejects that queued
         * record, including when revocation happened in this same RX pass. */
        if (domainBoot == 0U || boot != domainBoot || device == 0U || session == 0U)
            return NET_REJECTED;
        authority = (message.type == 0x20U || message.type == 0x40U)
            ? DomainIf_ValidatePassiveQueryContext(device, session)
            : DomainNetworkAdmissionBridgePrivate_ValidateMonitoringContext(device, session);
        if (authority != DOMAIN_IF_OK) return NET_REJECTED;
    }
    return delegate->submit(context, packet, key);
}
static struct {
    bool busy, committed, eligible;
    SnapshotCommit candidate;
    NdCanValue expected;
    SnapshotCommit *slot;
} capture;

static int snapshot_index(uint16_t id)
{
    switch (id) {
    case 0x200U: return 0; case 0x230U: return 1;
    case 0x300U: return 2; case 0x310U: return 3;
    case 0x0a0U: return 4; case 0x320U: return 5;
    case 0x340U: return 6; default: return -1;
    }
}

static size_t snapshot_size(uint16_t id)
{
    switch (id) {
    case 0x200U: return sizeof(DoorState_t);
    case 0x230U: return sizeof(DomainIf_BcmStatusReport_t);
    case 0x300U: return sizeof(CisCabinEnvironmentState_t);
    case 0x310U: return sizeof(CisOccupantState_t);
    case 0x0a0U: return sizeof(CisRearState_t);
    case 0x320U: return sizeof(DomainIf_CisStatusReport_t);
    case 0x340U: return sizeof(DomainIf_VssStateReport_t);
    default: return 0U;
    }
}

static void snapshot_commit(uint16_t id, const void *value, size_t size)
{
    int index = snapshot_index(id);
    if (id == 0U) {
        revokeQueuedStates = true;
        memset(snapshot, 0, sizeof(snapshot));
        memset(&capture, 0, sizeof(capture));
        return;
    }
    if (index < 0) return;
    snapshot[index].valid = false;
    snapshot[index].pending = false;
    if (capture.busy && capture.slot == &snapshot[index]) {
        capture.committed = capture.eligible && value != NULL &&
            size == snapshot_size(id) &&
            memcmp(value, &capture.expected, size) == 0;
    }
}

bool NetDomainSnapshot_Begin(NetDomainC2 *a, const NetUpperCanMessage *m,
    const NetUpperDelivery *d)
{
    int index;
    SnapshotCommit *slot;
    if (capture.busy || a == NULL || a->service == NULL ||
        m == NULL || d == NULL) return false;
    memset(&capture, 0, sizeof(capture));
    capture.busy = true;
    publisherAdapter = a;
    index = snapshot_index(m->id);
    if (index < 0) return true;
    slot = &snapshot[index];
    capture.slot = slot;
    /* A service/port lifecycle change cannot inherit a prior receipt. */
    if (slot->adapter != a || slot->domainBoot != a->config.domain_boot ||
        slot->localEpoch != a->service->config.local_epoch ||
        slot->epoch != d->capture.port_epoch)
        memset(slot, 0, sizeof(*slot));
    capture.candidate = *slot;
    capture.candidate.id = m->id;
    capture.candidate.adapter = a;
    capture.candidate.domainBoot = a->config.domain_boot;
    capture.candidate.localEpoch = a->service->config.local_epoch;
    capture.candidate.epoch = d->capture.port_epoch;
    capture.candidate.receipt = d->receipt_id;
    capture.candidate.boot = d->source_boot;
    capture.candidate.sequence = d->source_sequence;
    /* Recipient binding is independent of CAN source/commit identity and
     * confers no authentication. Never retarget a pending observation. */
    capture.candidate.recipientDevice = a->service->context_seen ?
        a->service->context.device_context_id : 0U;
    capture.candidate.recipientSession = a->service->context_seen ?
        a->service->context.session_id : 0U;
    capture.candidate.recipientEpoch = a->service->context_port_epoch;
    capture.eligible = !d->repeat && !d->observation_only &&
        d->capture.timestamp_known && d->receipt_id != 0U &&
        d->receipt_id > slot->lastReceipt &&
        nd_map_can(a, m, d->local_elapsed_ms, &capture.expected) == NET_OK;
    /* A new observation invalidates the old commit even if its Owner rejects. */
    if (d->receipt_id != slot->receipt) slot->valid = slot->pending = false;
    return true;
}

void NetDomainSnapshot_End(NetStatus status)
{
    if (!capture.busy) return;
    if (capture.slot != NULL && capture.committed && status == NET_OK) {
        *capture.slot = capture.candidate;
        capture.slot->lastReceipt = capture.slot->receipt;
        capture.slot->valid = true;
        capture.slot->pending = capture.slot->recipientDevice != 0U &&
            capture.slot->recipientSession != 0U &&
            capture.slot->recipientEpoch == capture.slot->adapter->service->config.port_epoch[NET_WIRE_UART_M];
    }
    memset(&capture, 0, sizeof(capture));
}

bool NetDomainSnapshot_Confirm(void *context, uint16_t id, uint64_t receipt,
    uint32_t boot, uint32_t sequence)
{
    int index = snapshot_index(id);
    SnapshotCommit *slot;
    NetUpperCanMessage raw;
    NetUpperDelivery delivery;
    NetSourceStamp source;
    uint32_t rawBoot, rawSequence;
    (void)context;
    if (index < 0 || capture.busy) return false;
    slot = &snapshot[index];
    if (!slot->valid || slot->receipt != receipt || slot->boot != boot ||
        slot->sequence != sequence || slot->adapter == NULL) return false;
    if (!slot->adapter->registered ||
        slot->domainBoot != slot->adapter->config.domain_boot ||
        slot->localEpoch != slot->adapter->service->config.local_epoch ||
        net_upper_received_can(slot->adapter->service, id, &raw, &delivery,
                               &source) != NET_OK ||
        delivery.receipt_id != receipt ||
        delivery.capture.port_epoch != slot->epoch ||
        net_upper_can_identity(&raw, &rawBoot, &rawSequence) != NET_WIRE_OK ||
        rawBoot != boot || rawSequence != sequence) {
        slot->valid = slot->pending = false;
        return false;
    }
    return true;
}

static void snapshot_publish(uint32_t nowMs)
{
    size_t i;
    /* Context expiry/revocation can occur inside an RX Owner callback. Defer
     * cancellation to the serialized Poll; never mutate a borrowed TX buffer.
     * Only the existing state projections are in this publisher's scope. */
    if (revokeQueuedStates && publisherAdapter != NULL) {
        NetUpper *service = publisherAdapter->service;
        bool failed = false;
        if (service == NULL || service->in_call) return;
        for (i = 0U; i < NET_UPPER_TX_CAPACITY; ++i) {
            NetUpperTx *tx = &service->tx[i];
            uint8_t type = tx->uart.type;
            if (!tx->state.used || !tx->state.queued || tx->state.submitted ||
                tx->link != NET_WIRE_UART_M ||
                !(type == 0x30U || type == 0x33U || type == 0x34U ||
                  type == 0x35U || type == 0x36U || type == 0x37U || type == 0x3aU))
                continue;
            if (net_upper_cancel(service, tx->state.key) != NET_OK) failed = true;
        }
        if (failed) return;
        revokeQueuedStates = false;
    }
    for (i = 0U; i < sizeof(snapshot) / sizeof(snapshot[0]); ++i) {
        SnapshotCommit *s = &snapshot[i];
        NdCanValue value;
        DomainIf_Status_t status = DOMAIN_IF_NOT_AVAILABLE;
        if (s->pending && s->adapter != NULL) {
            const NetUpper *service = s->adapter->service;
            if (!service->context_seen || service->link_failed[NET_WIRE_UART_M] ||
                service->quiescing[NET_WIRE_UART_M] ||
                service->context_port_epoch != s->recipientEpoch ||
                service->config.port_epoch[NET_WIRE_UART_M] != s->recipientEpoch ||
                service->context.device_context_id != s->recipientDevice ||
                service->context.session_id != s->recipientSession) {
                s->pending = false;
            }
        }
        if (!s->pending || !NetDomainSnapshot_Confirm(NULL, s->id,
                s->receipt, s->boot, s->sequence)) continue;
        /* Existing getters preserve Domain freshness/quality policy. The
         * existing projection then checks every field against the CAN source.
         * TTL/quality disagreements stay HOLD rather than becoming fresh. */
        switch (s->id) {
#define SNAPSHOT_PUBLISH(id_, member_, getter_, tx_) \
        case id_: { \
            if (getter_(nowMs, &value.member_) == DOMAIN_IF_OK) { \
                status = tx_(&value.member_); \
            } \
            break; \
        }
        SNAPSHOT_PUBLISH(0x200U, door, DomainIf_QueryGetBcmDoorState, DomainIf_TxMobileBcmDoorState);
        SNAPSHOT_PUBLISH(0x230U, bcm_status, DomainIf_QueryGetBcmStatus, DomainIf_TxMobileBcmStatus);
        SNAPSHOT_PUBLISH(0x300U, environment, DomainIf_QueryGetCisEnvironment, DomainIf_TxMobileCisEnvironment);
        SNAPSHOT_PUBLISH(0x310U, occupant, DomainIf_QueryGetCisOccupant, DomainIf_TxMobileCisOccupant);
        SNAPSHOT_PUBLISH(0x0a0U, rear, DomainIf_QueryGetCisRear, DomainIf_TxMobileCisRear);
        SNAPSHOT_PUBLISH(0x320U, cis_status, DomainIf_QueryGetCisStatus, DomainIf_TxMobileCisStatus);
        SNAPSHOT_PUBLISH(0x340U, vss, DomainIf_QueryGetVssStatus, DomainIf_TxMobileVssStatus);
#undef SNAPSHOT_PUBLISH
        default: break;
        }
        /* OK is local queue acceptance only. Existing Network queues own
         * transport retries/completion; backpressure leaves this pending. */
        if (status == DOMAIN_IF_OK) s->pending = false;
    }
}

void NetDomainSnapshot_Bind(void)
{
    memset(snapshot, 0, sizeof(snapshot));
    memset(&capture, 0, sizeof(capture));
    DomainSnapshotCommit_Bind(snapshot_commit, snapshot_publish);
    publisherAdapter = NULL;
    revokeQueuedStates = false;
}
/* References Owner implementations, never defines or changes their ABI. This
 * object is pulled from the archive only when this explicit binder is used. */
void net_domain_c2_pinned_owner_api(NetDomainC2Api *out)
{
    if(out==NULL)return;
    memset(out,0,sizeof(*out));
    NetDomainSnapshot_Bind();
    out->IsInitialized=DomainIf_IsInitialized;
    out->SetRxObserver=DomainIf_SetRxObserver;
    out->SetTxPort=DomainIf_SetTxPort;
    out->RxBcmDoorState=DomainIf_RxBcmDoorState;
    out->RxBcmStatus=DomainIf_RxBcmStatus;
    out->RxBcmCommandResult=DomainIf_RxBcmCommandResult;
    out->RxCisRear=DomainIf_RxCisRear;
    out->RxCisStatus=DomainIf_RxCisStatus;
    out->RxCisCabinEnvironment=DomainIf_RxCisCabinEnvironment;
    out->RxCisOccupant=DomainIf_RxCisOccupant;
    out->RxVssState=DomainIf_RxVssState;
    out->RxNetworkLocalFailure=DomainIf_RxNetworkLocalFailure;
    out->ValidatePassiveQueryContext=DomainIf_ValidatePassiveQueryContext;
    out->QueryGetBcmDoorState=DomainIf_QueryGetBcmDoorState;
    out->QueryGetBcmStatus=DomainIf_QueryGetBcmStatus;
    out->QueryGetCisRear=DomainIf_QueryGetCisRear;
    out->QueryGetCisStatus=DomainIf_QueryGetCisStatus;
    out->QueryGetCisEnvironment=DomainIf_QueryGetCisEnvironment;
    out->QueryGetCisOccupant=DomainIf_QueryGetCisOccupant;
    out->QueryGetVssStatus=DomainIf_QueryGetVssStatus;
    out->QueryGetRequestResult=DomainIf_QueryGetRequestResult;
    out->QueryGetRecentRequestResults=DomainIf_QueryGetRecentRequestResults;
}
