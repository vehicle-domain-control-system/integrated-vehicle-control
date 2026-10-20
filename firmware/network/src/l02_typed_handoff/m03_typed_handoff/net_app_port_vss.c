/* L02/M03 VSS Application Port facade.
 * Contract: docs/network/interface/02_NODES/VSS/{10_DATA,20_API}.md
 *   - RX: verified typed wire DTO -> NetVss* (explicit field copy) -> VssApp_*
 *         from the serialized NetNode_Step() context only.
 *   - TX: VssNet_PublishStatus -> explicit field copy -> M04 publish slot
 *         (local copy; not TX success). BOOT_ID, TX_SEQUENCE, the replacement
 *         of a status no device has taken yet and the waiting AGE are M04's
 *         (net_upper_publish.h; 20_API.md: Network only adds the sending
 *         BOOT/TX and the waiting AGE). Nothing here numbers or repeats a frame:
 *         the VSS application submits every new evaluation itself. */
#include "network/net_app_port_vss.h"
#include <string.h>

typedef struct {
    bool bound, in_publish;
    NetUpper *service;
    NetEcuAdapter *ecu;
    NetUpperPublisher *publisher;
    NetAppPortVssDiag diag;
} VssPort;

static VssPort port;

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

/* ---------------------------------------------------------------- RX ---- */
static NetStatus on_domain_permission(void *ctx, const NetWireDomainPermissionRaw *raw,
                                      const NetUpperDelivery *d)
{
    NetDomainPermission p;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    p.vehicle_power_permission = raw->vehicle_power_permission;
    p.power_permission_quality = raw->power_permission_quality;
    p.power_operation_state = raw->power_operation_state;
    p.operation_allowed = raw->operation_allowed;
    p.operation_quality = raw->operation_quality;
    p.update_sequence = raw->update_sequence;
    p.source_age_ms = raw->source_age_ms;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    VssNetPriv_OnDomainPermission(&p, &meta);
    return NET_OK;
}

static NetStatus on_vss_event(void *ctx, const NetWireVssEventRaw *raw, const NetUpperDelivery *d)
{
    NetVssEvent e;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    e.event_kind = raw->event_kind;
    e.occurrence_id = raw->occurrence_id;
    e.original_age_ms = raw->original_age_ms;
    e.use_limit_ms = raw->use_limit_ms;
    e.value_quality = raw->value_quality;
    e.input_availability = raw->input_availability;
    e.related_command_id = raw->related_command_id;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    VssApp_OnDomainEvent(&e, &meta);
    inc(&port.diag.event_delivered);
    return NET_OK;
}

static NetStatus on_vss_warning_state(void *ctx, const NetWireVssWarningStateRaw *raw,
                                      const NetUpperDelivery *d)
{
    NetVssWarningState w;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    w.antipinch_source_boot = raw->antipinch_source_boot;
    w.antipinch_source_sequence = raw->antipinch_source_sequence;
    w.antipinch_original_age_ms = raw->antipinch_original_age_ms;
    w.antipinch_state = raw->antipinch_state;
    w.antipinch_quality = raw->antipinch_quality;
    w.antipinch_reason = raw->antipinch_reason;
    w.antipinch_availability = raw->antipinch_availability;
    w.occupant_hazard_source_boot = raw->occupant_hazard_source_boot;
    w.occupant_hazard_source_sequence = raw->occupant_hazard_source_sequence;
    w.occupant_hazard_original_age_ms = raw->occupant_hazard_original_age_ms;
    w.occupant_hazard_state = raw->occupant_hazard_state;
    w.occupant_hazard_quality = raw->occupant_hazard_quality;
    w.occupant_hazard_reason = raw->occupant_hazard_reason;
    w.occupant_hazard_availability = raw->occupant_hazard_availability;
    w.rear_obstacle_source_boot = raw->rear_obstacle_source_boot;
    w.rear_obstacle_source_sequence = raw->rear_obstacle_source_sequence;
    w.rear_obstacle_original_age_ms = raw->rear_obstacle_original_age_ms;
    w.rear_obstacle_state = raw->rear_obstacle_state;
    w.rear_obstacle_quality = raw->rear_obstacle_quality;
    w.rear_obstacle_reason = raw->rear_obstacle_reason;
    w.rear_obstacle_availability = raw->rear_obstacle_availability;
    w.rear_detection_activation = raw->rear_detection_activation;
    w.activation_meta_format = raw->activation_meta_format;
    w.rear_activation_quality = raw->rear_activation_quality;
    w.rear_activation_reason = raw->rear_activation_reason;
    w.rear_activation_sequence = raw->rear_activation_sequence;
    w.rear_activation_age_ms = raw->rear_activation_age_ms;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    VssApp_OnDomainWarningState(&w, &meta);
    inc(&port.diag.warning_delivered);
    return NET_OK;
}

static const NetEcuCallbacks vss_callbacks = {
    .domain_permission = on_domain_permission,
    .vss_event = on_vss_event,
    .vss_warning_state = on_vss_warning_state
};

const NetEcuCallbacks *net_app_port_vss_callbacks(void) { return &vss_callbacks; }

void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)
{
    /* Supervision record only. The receive-timeout / original-age judgement
     * runs in NetPermissionGuard (net_node.c); nothing here grants VSS recovery. */
    if (message == NULL || meta == NULL) return;
    port.diag.last_permission = *message;
    port.diag.last_permission_meta = *meta;
    port.diag.permission_seen = true;
    inc(&port.diag.permission_received);
}

/* ---------------------------------------------------------------- TX ---- */
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)
{
    NetUpperCanMessage m;
    NetWireVssStatusRaw *r = &m.data.vss_status;
    NetUpperPublishNote note;
    NetStatus st;
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    if (!port.bound || port.in_publish) return NET_SUBMIT_NOT_READY;
    port.in_publish = true;
    memset(&m, 0, sizeof(m));
    m.id = 0x340u;
    /* boot_id / tx_sequence stay 0: M04 writes them into each frame. */
    r->vss_state = message->vss_state;
    r->vss_availability = message->vss_availability;
    r->vss_accepting_events = message->vss_accepting_events;
    r->vss_fault_active = message->vss_fault_active;
    r->fault_mask = message->fault_mask;
    r->vss_last_fault = message->vss_last_fault;
    r->last_fault_occurrence = message->last_fault_occurrence;
    r->last_fault_age_ms = message->last_fault_age_ms;
    r->last_fault_quality = message->last_fault_quality;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    memset(&note, 0, sizeof(note));
    st = net_upper_publish_can(port.publisher, &m, NULL, &note);
    if (st == NET_OK) {
        /* Inside an RX callback the copy is offered right after that step. */
        if (note.in_step) inc(&port.diag.status_deferred);
    } else {
        inc(&port.diag.status_not_accepted);
    }
    port.diag.last_status_engine = st;
    port.diag.last_status_result = net_app_port_submit_result(st);
    port.in_publish = false;
    return port.diag.last_status_result;
}

/* ------------------------------------------------------------ binding --- */
NetStatus net_app_port_vss_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortVssConfig *config)
{
    NetUpperPublishView view;
    if (service == NULL || ecu == NULL || config == NULL || config->publisher == NULL ||
        config->publisher->service != service ||
        net_upper_publish_view(config->publisher, NET_WIRE_CAN, 0x340u, &view) != NET_OK)
        return NET_INVALID_ARGUMENT;
    memset(&port, 0, sizeof(port));
    port.service = service;
    port.ecu = ecu;
    port.publisher = config->publisher;
    port.bound = true;
    return NET_OK;
}

NetStatus net_app_port_vss_diag(NetAppPortVssDiag *out)
{
    NetUpperPublishView view;
    if (out == NULL) return NET_INVALID_ARGUMENT;
    *out = port.diag;
    out->bound = port.bound;
    if (port.bound && net_upper_publish_view(port.publisher, NET_WIRE_CAN, 0x340u, &view) == NET_OK) {
        out->boot_id = view.boot_id;
        out->next_status_sequence = view.next_sequence;
        out->status_submitted = view.counters.sent;
        out->status_replaced = view.counters.replaced;
    }
    return NET_OK;
}
