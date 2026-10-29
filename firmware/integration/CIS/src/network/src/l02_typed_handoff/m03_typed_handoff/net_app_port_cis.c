/* L02/M03 CIS Application Port facade.
 * Contract: docs/network/interface/02_NODES/CIS/{10_DATA,20_API}.md,
 *           02_NODES/PI/10_DATA.md (UART-C records)
 *   - RX: verified typed wire DTO -> NetDomainPermission / NetVision* (explicit
 *         field copy) -> CisApp_* from the serialized NetNode_Step() context.
 *         DOMAIN_PERMISSION goes, as the same verified input, to
 *         CisApp_OnDomainPermission() and to the private Pi relay
 *         (20_API.md CisNetPriv_RelayPiPermission: "CIS permission Dispatcher
 *         ... distributes the same verified original input").
 *   - TX: CisNet_Publish* -> explicit field copy -> M04 publish slot (local
 *         copy; not TX success). BOOT_ID, TX_SEQUENCE / LINK_SEQUENCE, the
 *         P_PERMISSION relay revision (UPDATE_SEQUENCE), the replacement of a
 *         record no device has taken yet, the waiting AGE and the repetition
 *         are M04's (net_upper_publish.h). Nothing here numbers or repeats a
 *         frame.
 * Compiled for the CIS node profile only (one profile per build). */
#include "network/net_upper_profile.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#include "network/net_app_port_cis.h"
#include "network/net_upper_control.h"
#include "network/net_wire_can_nodes.h"
#include <string.h>

typedef struct {
    bool seen;
    NetSourceStamp source;      /* the receipt as the service will record it */
} Receipt;

/* What the last relay record M04 took carried from its DOMAIN_PERMISSION. */
typedef struct {
    bool seen;
    uint32_t domain_boot, update_sequence;
    uint8_t value, quality;
} RelayBasis;

typedef struct {
    bool bound, in_publish;
    NetUpper *service;
    NetEcuAdapter *ecu;
    NetUpperPublisher *publisher;
    uint32_t cis_boot_id;
    uint16_t pi_use_limit_ms;
    /* DOMAIN_PERMISSION being dispatched (relay input check). */
    Receipt permission;
    NetDomainPermission permission_value;
    NetCanRxMeta permission_meta;
    RelayBasis relayed;
    /* Last accepted P_VISION_RESULT (CIS_OCCUPANT relay origin). */
    Receipt vision;
    NetWirePVisionResult vision_raw;
    /* Pi AGE of that judgement when it was first received (not a
     * repetition): the lowest vision_age_ms a relay of it may carry. */
    uint16_t vision_first_age;
    NetAppPortCisDiag diag;
} CisPort;

static CisPort port;

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

/* The receipt the service records once the callback returns OK: its id, its
 * capture time and the message's own AGE (net_upper_service.c delivery). */
static void receipt_of(const NetUpperDelivery *d, uint16_t age, Receipt *out)
{
    out->seen = true;
    out->source.observation_id = d->receipt_id;
    out->source.observed_at_ms = d->capture.captured_at_ms;
    out->source.original_age_ms = age;
}

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
    receipt_of(d, raw->source_age_ms, &port.permission);
    port.permission_value = p;
    port.permission_meta = meta;
    port.diag.last_permission = p;
    port.diag.last_permission_meta = meta;
    port.diag.permission_seen = true;
    CisApp_OnDomainPermission(&p, &meta);
    inc(&port.diag.permission_delivered);
    /* 30_UART_C.md: P_PERMISSION is submitted at once on a new original
     * evaluation, an accepted change of the DOMAIN run (its first frame, even
     * with an UPDATE_SEQUENCE not above the earlier run's) or an actual
     * quality restriction (a lower power quality of the same evaluation
     * arrives as a new observation, not as a repetition); a repetition of
     * the same evaluation with a new TX_SEQUENCE is none of these. It reaches
     * the application (receive supervision) but submits no relay record: no
     * P_PERMISSION per CAN frame, and Network does not repeat P_PERMISSION
     * (period 0, net_node_profile_cis.c). */
    if (d->repeat) {
        inc(&port.diag.permission_repeats);
        return NET_OK;
    }
    /* Same verified input to the private relay. Its local result is a
     * relay fact (diag), never a reason to refuse the DOMAIN receipt. */
    (void)CisNetPriv_RelayPiPermission(&p, &meta);
    return NET_OK;
}

static NetStatus on_vision_result(void *ctx, const NetWirePVisionResult *raw, const NetUpperDelivery *d)
{
    NetVisionResult v;
    NetUartRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    v.pi_boot_id = raw->pi_boot_id;
    v.vision_sequence = raw->vision_sequence;
    v.vision_age_ms = raw->vision_age_ms;
    v.occupant_presence = raw->occupant_presence;
    v.presence_validity = raw->presence_validity;
    v.presence_reason = raw->presence_reason;
    v.occupant_count = raw->occupant_count;
    v.count_validity = raw->count_validity;
    v.count_reason = raw->count_reason;
    v.vision_status = raw->vision_status;
    v.vision_fault_code = raw->vision_fault_code;
    net_app_port_uart_rx_meta(d, &meta);
    /* Recorded before the callback: an occupant published inside it relays
     * this observation. */
    receipt_of(d, raw->vision_age_ms, &port.vision);
    port.vision_raw = *raw;
    /* A repetition of the same judgement keeps the AGE of its first receipt:
     * an application may still hold the copy it took then. */
    if (!d->repeat) port.vision_first_age = raw->vision_age_ms;
    CisApp_OnPiVisionResult(&v, &meta);
    inc(&port.diag.vision_result_delivered);
    return NET_OK;
}

static NetStatus on_vision_status(void *ctx, const NetWirePVisionStatusRaw *raw, const NetUpperDelivery *d)
{
    NetVisionStatus s;
    NetUartRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    s.pi_boot_id = raw->pi_boot_id;
    s.status_sequence = raw->status_sequence;
    s.vision_status = raw->vision_status;
    s.active_fault = raw->active_fault;
    s.last_fault = raw->last_fault;
    s.status_age_ms = raw->status_age_ms;
    s.affected_function = raw->affected_function;
    net_app_port_uart_rx_meta(d, &meta);
    CisApp_OnPiVisionStatus(&s, &meta);
    inc(&port.diag.vision_status_delivered);
    return NET_OK;
}

static const NetEcuCallbacks cis_callbacks = {
    .domain_permission = on_domain_permission,
    .p_vision_result = on_vision_result,
    .p_vision_status = on_vision_status
};

const NetEcuCallbacks *net_app_port_cis_callbacks(void) { return &cis_callbacks; }

/* ------------------------------------------------------------ relay ---- */
static bool same_permission(const NetDomainPermission *a, const NetDomainPermission *b)
{
    return a->vehicle_power_permission == b->vehicle_power_permission &&
        a->power_permission_quality == b->power_permission_quality &&
        a->power_operation_state == b->power_operation_state &&
        a->operation_allowed == b->operation_allowed &&
        a->operation_quality == b->operation_quality &&
        a->update_sequence == b->update_sequence &&
        a->source_age_ms == b->source_age_ms;
}

static NetSubmitResult relay_result(NetStatus st)
{
    port.diag.last_relay_engine = st;
    port.diag.last_relay_result = net_app_port_submit_result(st);
    if (st == NET_OK) inc(&port.diag.relay_submitted);
    else inc(&port.diag.relay_refused);
    return port.diag.last_relay_result;
}

/* A relay event (30_UART_C.md P_PERMISSION, 04_COMMON/10_DATA_AND_ID_RULES.md
 * number spaces): a new original DOMAIN evaluation, an accepted change of the
 * DOMAIN run, or an actual change of the relayed power quality (or value)
 * against the last relay record M04 took. Each one gets the next relay
 * revision in M04. A DOMAIN_PERMISSION that changes nothing the relay
 * carries (for example its operation quality only) is none of them. */
static bool relay_event(const NetDomainPermission *p, const NetCanRxMeta *meta)
{
    const RelayBasis *r = &port.relayed;
    return !r->seen || r->domain_boot != meta->source.boot_id ||
        r->update_sequence != p->update_sequence ||
        r->value != p->vehicle_power_permission || r->quality != p->power_permission_quality;
}

NetSubmitResult CisNetPriv_RelayPiPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta)
{
    NetUpperUartMessage m;
    NetWirePPermission *r = &m.data.p_permission;
    NetStatus st;
    if (permission == NULL || meta == NULL) return relay_result(NET_INVALID_ARGUMENT);
    if (!port.bound) return relay_result(NET_NOT_CONFIGURED);
    /* Only the DOMAIN original the dispatcher verified: never a permission
     * object created elsewhere (20_API.md: no separate Pi permission input). */
    if (!port.permission.seen || !same_permission(permission, &port.permission_value) ||
        meta->source.boot_id != port.permission_meta.source.boot_id ||
        meta->source.tx_sequence != port.permission_meta.source.tx_sequence ||
        meta->received_at_ms != port.permission_meta.received_at_ms)
        return relay_result(NET_HOLD);
    if (!relay_event(permission, meta)) {
        /* Nothing new for the Pi: the last relay record stands. */
        inc(&port.diag.relay_unchanged);
        port.diag.last_relay_engine = NET_OK;
        port.diag.last_relay_result = NET_SUBMIT_OK;
        return NET_SUBMIT_OK;
    }
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x62u;
    r->cis_boot_id = port.cis_boot_id;
    r->update_sequence = 0u;   /* the CIS relay revision: M04 gives it (net_upper_publish_uart) */
    /* The DOMAIN power value and quality as received, restricted ones too
     * (02_NODES/CIS/20_API.md internal relay: a normal-format DENY / STALE /
     * INVALID is kept and relayed as an explicit state; no allow is made up),
     * with the fixed reason of that quality. */
    r->vehicle_power_permission = permission->vehicle_power_permission;
    r->value_quality = permission->power_permission_quality;
    r->quality_reason = net_upper_pi_permission_reason(permission->power_permission_quality);
    r->use_limit_ms = port.pi_use_limit_ms;
    r->source_age_ms = permission->source_age_ms;       /* DOMAIN AGE; M04 adds the local wait once */
    st = net_upper_publish_uart(port.publisher, &m, &port.permission.source, NULL);
    if (st == NET_OK) {
        port.relayed.seen = true;
        port.relayed.domain_boot = meta->source.boot_id;
        port.relayed.update_sequence = permission->update_sequence;
        port.relayed.value = permission->vehicle_power_permission;
        port.relayed.quality = permission->power_permission_quality;
    }
    return relay_result(st);
}

/* ---------------------------------------------------------------- TX ---- */
static NetSubmitResult finish(NetAppPortCisPublish which, NetStatus st, const NetUpperPublishNote *note)
{
    NetAppPortCisPublishDiag *d = &port.diag.publish[which];
    if (st == NET_OK) {
        /* Inside an RX callback the copy is offered right after that step. */
        if (note->in_step) inc(&d->deferred);
    } else {
        inc(&d->not_accepted);
    }
    d->last_engine = st;
    d->last_result = net_app_port_submit_result(st);
    port.in_publish = false;
    return d->last_result;
}

static NetSubmitResult refuse(NetAppPortCisPublish which, NetStatus st)
{
    NetAppPortCisPublishDiag *d = &port.diag.publish[which];
    inc(&d->not_accepted);
    d->last_engine = st;
    d->last_result = net_app_port_submit_result(st);
    return d->last_result;
}

static bool enter(void)
{
    if (!port.bound || port.in_publish) return false;
    port.in_publish = true;
    return true;
}

NetSubmitResult CisNet_PublishRear(const NetCisRear *message)
{
    NetUpperCanMessage m;
    NetWireCisRearRaw *r = &m.data.cis_rear;
    NetUpperPublishNote note;
    if (message == NULL) return refuse(NET_APP_PORT_CIS_REAR, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_CIS_REAR, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x0A0u;
    /* boot_id / tx_sequence stay 0: M04 writes them into each frame. */
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    r->rear_distance_deci_cm = message->rear_distance_deci_cm;
    r->validity = message->validity;
    r->quality_reason = message->quality_reason;
    r->proximity_status = message->proximity_status;
    memset(&note, 0, sizeof(note));
    return finish(NET_APP_PORT_CIS_REAR, net_upper_publish_can(port.publisher, &m, NULL, &note), &note);
}

NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message)
{
    NetUpperCanMessage m;
    NetWireCisEnvironmentRaw *r = &m.data.cis_environment;
    NetUpperPublishNote note;
    if (message == NULL) return refuse(NET_APP_PORT_CIS_ENVIRONMENT, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_CIS_ENVIRONMENT, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x300u;
    r->temperature_centi_c = message->temperature_centi_c;
    r->temperature_sequence = message->temperature_sequence;
    r->temperature_age_ms = message->temperature_age_ms;
    r->temperature_validity = message->temperature_validity;
    r->temperature_reason = message->temperature_reason;
    r->humidity_centi_percent = message->humidity_centi_percent;
    r->humidity_sequence = message->humidity_sequence;
    r->humidity_age_ms = message->humidity_age_ms;
    r->humidity_validity = message->humidity_validity;
    r->humidity_reason = message->humidity_reason;
    r->illuminance_lux = message->illuminance_lux;
    r->illuminance_sequence = message->illuminance_sequence;
    r->illuminance_age_ms = message->illuminance_age_ms;
    r->illuminance_validity = message->illuminance_validity;
    r->illuminance_reason = message->illuminance_reason;
    memset(&note, 0, sizeof(note));
    return finish(NET_APP_PORT_CIS_ENVIRONMENT, net_upper_publish_can(port.publisher, &m, NULL, &note), &note);
}

/* One field against the Pi field: the same validity and reason, or a
 * restriction (INVALID, any reason); never a promotion. The same rule as the
 * service's CIS_OCCUPANT origin check (net_upper_service.c vision_origin). */
static bool field_restricted(uint8_t pi_validity, uint8_t pi_reason, uint8_t validity, uint8_t reason)
{
    return validity == NET_VALIDITY_INVALID || (validity == pi_validity && reason == pi_reason);
}

/* The record against the accepted Pi judgement it relays (see header):
 *   NET_HOLD (NOT_READY): no Pi judgement accepted yet;
 *   NET_INVALID_ARGUMENT: a record that can never be that relay, so a
 *     resubmission cannot succeed (VSS contract C10 (VSS_NODE_INTEGRATION.md),
 *     02_NODES/CIS/20_API.md send return: combination error): another Pi
 *     identity (BOOT, VISION_SEQUENCE), an AGE below the Pi AGE of that
 *     judgement's first receipt, another presence / count, or a validity
 *     above the Pi one;
 *   NET_OK: the same identity and values, each field's validity / reason the
 *     Pi's or a restriction of it. vision_function_status is the CIS's own
 *     function state (02_NODES/CIS/10_DATA.md) and is not compared.
 * The AGE bound is the first receipt's: repetitions of the judgement
 * (02_NODES/CIS/20_API.md receive delivery) raise the current AGE while the
 * application may still hold its copy of the first delivery. The AGE sent is
 * not the record's: it is the latest receipt's Pi AGE plus the time since. */
static NetStatus occupant_origin(const NetCisOccupant *o)
{
    const NetWirePVisionResult *v = &port.vision_raw;
    if (!port.vision.seen) return NET_HOLD;
    if (o->pi_boot_id != v->pi_boot_id || o->vision_sequence != v->vision_sequence ||
        o->vision_age_ms < port.vision_first_age ||
        o->occupant_presence != v->occupant_presence || o->occupant_count != v->occupant_count ||
        !field_restricted(v->presence_validity, v->presence_reason, o->presence_validity, o->presence_reason) ||
        !field_restricted(v->count_validity, v->count_reason, o->count_validity, o->count_reason))
        return NET_INVALID_ARGUMENT;
    return NET_OK;
}

NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message)
{
    NetUpperCanMessage m;
    NetWireCisOccupantRaw *r = &m.data.cis_occupant;
    NetWireCanEnvelope probe;
    NetUpperPublishNote note;
    NetStatus origin;
    if (message == NULL) return refuse(NET_APP_PORT_CIS_OCCUPANT, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_CIS_OCCUPANT, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x310u;
    r->boot_id = port.cis_boot_id;   /* format check only; M04 assigns the identity */
    r->tx_sequence = 1u;
    r->pi_boot_id = message->pi_boot_id;
    r->vision_sequence = message->vision_sequence;
    r->vision_age_ms = message->vision_age_ms;
    r->occupant_presence = message->occupant_presence;
    r->presence_validity = message->presence_validity;
    r->presence_reason = message->presence_reason;
    r->occupant_count = message->occupant_count;
    r->count_validity = message->count_validity;
    r->count_reason = message->count_reason;
    r->vision_function_status = message->vision_function_status;
    if (net_wire_can_encode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, r, &probe) != NET_WIRE_OK) {
        port.in_publish = false;
        return refuse(NET_APP_PORT_CIS_OCCUPANT, NET_INVALID_ARGUMENT);
    }
    origin = occupant_origin(message);
    if (origin != NET_OK) {
        port.in_publish = false;
        inc(&port.diag.occupant_not_current);
        return refuse(NET_APP_PORT_CIS_OCCUPANT, origin);
    }
    /* Pi AGE at the receipt; the time since it is counted by the service, once. */
    r->vision_age_ms = port.vision_raw.vision_age_ms;
    r->boot_id = 0u;
    r->tx_sequence = 0u;
    memset(&note, 0, sizeof(note));
    return finish(NET_APP_PORT_CIS_OCCUPANT,
                  net_upper_publish_can(port.publisher, &m, &port.vision.source, &note), &note);
}

NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message)
{
    NetUpperCanMessage m;
    NetWireCisStatusRaw *r = &m.data.cis_status;
    NetUpperPublishNote note;
    if (message == NULL) return refuse(NET_APP_PORT_CIS_STATUS, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_CIS_STATUS, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x320u;
    r->cis_state = message->cis_state;
    r->vision_status = message->vision_status;
    r->temperature_status = message->temperature_status;
    r->humidity_status = message->humidity_status;
    r->illuminance_status = message->illuminance_status;
    r->rear_status = message->rear_status;
    r->pi_uart_status = message->pi_uart_status;
    r->can_interface_status = message->can_interface_status;
    r->fault_mask = message->fault_mask;
    r->active_fault = message->active_fault;
    r->recovering = message->recovering;
    r->last_fault = message->last_fault;
    r->last_fault_occurrence = message->last_fault_occurrence;
    r->affected_function = message->affected_function;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    memset(&note, 0, sizeof(note));
    return finish(NET_APP_PORT_CIS_STATUS, net_upper_publish_can(port.publisher, &m, NULL, &note), &note);
}

/* ------------------------------------------------------------ binding --- */
NetStatus net_app_port_cis_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortCisConfig *config)
{
    static const struct { NetWireLink link; uint16_t id; } need[] = {
        { NET_WIRE_CAN, 0x0A0u }, { NET_WIRE_CAN, 0x300u }, { NET_WIRE_CAN, 0x310u },
        { NET_WIRE_CAN, 0x320u }, { NET_WIRE_UART_C, 0x62u }
    };
    NetUpperPublishView view;
    size_t i;
    if (service == NULL || ecu == NULL || config == NULL || config->publisher == NULL ||
        config->publisher->service != service || config->cis_boot_id == 0u || config->pi_use_limit_ms == 0u)
        return NET_INVALID_ARGUMENT;
    for (i = 0u; i < sizeof(need) / sizeof(need[0]); ++i)
        if (net_upper_publish_view(config->publisher, need[i].link, need[i].id, &view) != NET_OK)
            return NET_INVALID_ARGUMENT;
    memset(&port, 0, sizeof(port));
    port.service = service;
    port.ecu = ecu;
    port.publisher = config->publisher;
    port.cis_boot_id = config->cis_boot_id;
    port.pi_use_limit_ms = config->pi_use_limit_ms;
    port.bound = true;
    return NET_OK;
}

NetStatus net_app_port_cis_diag(NetAppPortCisDiag *out)
{
    if (out == NULL) return NET_INVALID_ARGUMENT;
    *out = port.diag;
    out->bound = port.bound;
    return NET_OK;
}

#else
/* Not the CIS node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_app_port_cis_absent;
#endif
