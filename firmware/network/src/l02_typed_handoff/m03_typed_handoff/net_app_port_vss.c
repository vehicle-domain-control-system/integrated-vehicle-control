/* L02/M03 VSS Application Port facade.
 * Contract: docs/network/interface/02_NODES/VSS/{10_DATA,20_API}.md
 *   - RX: verified typed wire DTO -> NetVss* (explicit field copy) -> VssApp_*
 *         from the serialized NetNode_Step() context only.
 *   - TX: VssNet_PublishStatus -> Network-owned BOOT_ID/TX_SEQUENCE ->
 *         typed ECU adapter -> M04 queue (local copy; not TX success).
 *   - A newer status replaces an older one that is still queued and never
 *     submitted ("send-not-started pending current state may be replaced"). */
#include "network/net_app_port_vss.h"
#include <string.h>

typedef struct {
    bool bound, in_publish, sequence_exhausted;
    NetUpper *service;
    NetEcuAdapter *ecu;
    NetAppPortVssConfig config;
    uint32_t next_status_sequence;
    uint64_t next_observation;
    bool status_key_valid, pending_valid;
    NetTxKey status_key;
    NetVssStatus pending;
    NetAppPortTxTracker tracker;
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
static NetStatus submit_status(const NetVssStatus *message)
{
    NetUpperCanMessage m;
    NetWireVssStatusRaw *r = &m.data.vss_status;
    NetSourceStamp stamp;
    NetTxKey key;
    NetStatus st;
    uint64_t now;

    net_app_port_retire_completed(&port.tracker, port.service);
    if (port.status_key_valid && net_app_port_cancel_if_unsent(port.service, port.status_key)) {
        inc(&port.diag.status_replaced);
        port.status_key_valid = false;
    }

    memset(&m, 0, sizeof(m));
    m.id = 0x340u;
    r->boot_id = port.config.boot_id;              /* Network-owned link identity */
    r->tx_sequence = port.next_status_sequence;    /* Network-owned, per message */
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

    st = net_upper_now(port.service, &now);
    if (st == NET_OK) {
        stamp.observation_id = ++port.next_observation;
        stamp.observed_at_ms = now;
        stamp.original_age_ms = net_upper_can_max_age(&m);
        st = net_ecu_publish_vss_status(port.ecu, r, &stamp, &port.config.status_tx_age, &key);
    }
    if (st == NET_OK) {
        net_app_port_track(&port.tracker, key);
        port.status_key = key;
        port.status_key_valid = true;
        inc(&port.diag.status_submitted);
        if (port.next_status_sequence == UINT32_MAX) port.sequence_exhausted = true; /* wrap HOLD */
        else ++port.next_status_sequence;
    } else if (st != NET_BUSY) {
        inc(&port.diag.status_not_accepted);
    }
    port.diag.last_status_engine = st;
    return st;
}

NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)
{
    NetStatus st;
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    if (!port.bound || port.in_publish || port.sequence_exhausted) return NET_SUBMIT_NOT_READY;
    port.in_publish = true;
    st = submit_status(message);
    if (st == NET_BUSY) {
        /* Called from inside an RX callback (engine step in progress). Keep a
         * bounded local copy, latest wins, and submit it right after the step
         * (net_app_port_vss_service). That copy is what NET_SUBMIT_OK means. */
        if (port.pending_valid) inc(&port.diag.status_replaced);
        port.pending = *message;
        port.pending_valid = true;
        inc(&port.diag.status_deferred);
        st = NET_OK;
    }
    port.diag.last_status_result = net_app_port_submit_result(st);
    port.in_publish = false;
    return port.diag.last_status_result;
}

/* ------------------------------------------------------------ binding --- */
NetStatus net_app_port_vss_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortVssConfig *config)
{
    if (service == NULL || ecu == NULL || config == NULL || config->boot_id == 0u ||
        !config->status_tx_age.bound) return NET_INVALID_ARGUMENT;
    memset(&port, 0, sizeof(port));
    port.service = service;
    port.ecu = ecu;
    port.config = *config;
    port.next_status_sequence = 1u;
    port.bound = true;
    return NET_OK;
}

void net_app_port_vss_service(void)
{
    if (!port.bound || port.in_publish) return;
    port.in_publish = true;
    if (port.pending_valid) {
        NetStatus st = submit_status(&port.pending);
        if (st != NET_BUSY) port.pending_valid = false; /* outcome recorded in diag */
    } else {
        net_app_port_retire_completed(&port.tracker, port.service);
    }
    port.in_publish = false;
}

NetStatus net_app_port_vss_diag(NetAppPortVssDiag *out)
{
    if (out == NULL) return NET_INVALID_ARGUMENT;
    *out = port.diag;
    out->bound = port.bound;
    out->boot_id = port.config.boot_id;
    out->next_status_sequence = port.next_status_sequence;
    return NET_OK;
}
