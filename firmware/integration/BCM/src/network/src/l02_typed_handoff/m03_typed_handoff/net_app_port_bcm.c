/* L02/M03 BCM Application Port facade.
 * Contract: docs/network/interface/02_NODES/BCM/{10_DATA,20_API}.md
 *   - RX: verified typed wire DTO -> NetBcm*Command / NetDomainPermission
 *         (explicit field copy) -> BcmApp_* / BcmNetPriv_* from the serialized
 *         NetNode_Step() context only.
 *   - TX: BcmNet_Publish* -> explicit field copy -> M04 publish slot (current
 *         value; a record no device has taken yet is replaced, 20_API.md
 *         "current value keeping").
 *         BcmNet_SendResult / SendEvent -> explicit field copy -> the M04 slot
 *         when it is idle, otherwise the bounded discrete store in front of it
 *         (net_app_port.h; 20_API.md "discrete result / occurrence keeping":
 *         every record keeps its identity, a lack of room never drops another).
 *         BOOT_ID, TX_SEQUENCE and the waiting AGE are M04's. Nothing here
 *         numbers or repeats a frame.
 * Compiled for the BCM node profile only (one profile per build). */
#include "network/net_upper_profile.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
#include "network/net_app_port_bcm.h"
#include <string.h>

typedef struct {
    bool bound, in_publish;
    bool in_rx;                 /* inside a BcmApp_* / BcmNetPriv_* call of the step */
    uint64_t rx_now;            /* the step's clock reading during that call */
    NetUpper *service;
    NetEcuAdapter *ecu;
    NetUpperPublisher *publisher;
    uint32_t bcm_boot_id;
    NetAppPortBcmDiag diag;
} BcmPort;

static BcmPort port;

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }

static void command_meta(uint32_t target_boot_id, uint32_t command_id, uint16_t command_age_ms,
                         uint16_t accept_limit_ms, NetCommandMeta *out)
{
    out->target_boot_id = target_boot_id;
    out->command_id = command_id;
    out->command_age_ms = command_age_ms;
    out->accept_limit_ms = accept_limit_ms;
}

/* 20_API.md: the command reaches the application only for the current BCM
 * generation it was addressed to. */
static bool for_this_boot(uint32_t target_boot_id)
{
    if (target_boot_id == port.bcm_boot_id) return true;
    inc(&port.diag.command_wrong_target);
    return false;
}

static void rx_enter(const NetUpperDelivery *d) { port.in_rx = true; port.rx_now = d->delivered_at_ms; }
static void rx_leave(void) { port.in_rx = false; }

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
    rx_enter(d);
    BcmNetPriv_OnDomainPermission(&p, &meta);
    rx_leave();
    return NET_OK;
}

static NetStatus on_door_command(void *ctx, const NetWireBcmDoorCommandRaw *raw, const NetUpperDelivery *d)
{
    NetBcmDoorCommand c;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    if (!for_this_boot(raw->target_boot_id)) return NET_REJECTED;
    command_meta(raw->target_boot_id, raw->command_id, raw->command_age_ms, raw->accept_limit_ms, &c.command);
    c.door_lock_target = raw->door_lock_target;
    c.origin = raw->origin;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    rx_enter(d);
    BcmApp_OnDomainDoorCommand(&c, &meta);
    rx_leave();
    inc(&port.diag.door_delivered);
    return NET_OK;
}

static NetStatus on_climate_command(void *ctx, const NetWireBcmClimateCommandRaw *raw, const NetUpperDelivery *d)
{
    NetBcmClimateCommand c;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    if (!for_this_boot(raw->target_boot_id)) return NET_REJECTED;
    command_meta(raw->target_boot_id, raw->command_id, raw->command_age_ms, raw->accept_limit_ms, &c.command);
    c.fan_target_level = raw->fan_target_level;
    c.thermal_direction = raw->thermal_direction;
    c.thermal_target_percent = raw->thermal_target_percent;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    rx_enter(d);
    BcmApp_OnDomainClimateCommand(&c, &meta);
    rx_leave();
    inc(&port.diag.climate_delivered);
    return NET_OK;
}

static NetStatus on_light_command(void *ctx, const NetWireBcmLightCommandRaw *raw, const NetUpperDelivery *d)
{
    NetBcmLightCommand c;
    NetCanRxMeta meta;
    (void)ctx;
    if (raw == NULL || d == NULL) return NET_INVALID_ARGUMENT;
    if (!port.bound) return NET_HOLD;
    if (!for_this_boot(raw->target_boot_id)) return NET_REJECTED;
    command_meta(raw->target_boot_id, raw->command_id, raw->command_age_ms, raw->accept_limit_ms, &c.command);
    c.interior_light_type = raw->interior_light_type;
    c.light_level_percent = raw->light_level_percent;
    c.interior_light_color.r = raw->red;
    c.interior_light_color.g = raw->green;
    c.interior_light_color.b = raw->blue;
    net_app_port_rx_meta(raw->boot_id, raw->tx_sequence, d, &meta);
    rx_enter(d);
    BcmApp_OnDomainLightCommand(&c, &meta);
    rx_leave();
    inc(&port.diag.light_delivered);
    return NET_OK;
}

static const NetEcuCallbacks bcm_callbacks = {
    .domain_permission = on_domain_permission,
    .bcm_door_command = on_door_command,
    .bcm_climate_command = on_climate_command,
    .bcm_light_command = on_light_command
};

const NetEcuCallbacks *net_app_port_bcm_callbacks(void) { return &bcm_callbacks; }

void BcmNetPriv_OnDomainPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta)
{
    /* Supervision record only. Receive timeout / original age are judged by
     * the node's permission supervision (net_node.c); 20_API.md installs no
     * public BcmApp permission callback. */
    if (permission == NULL || meta == NULL) return;
    port.diag.last_permission = *permission;
    port.diag.last_permission_meta = *meta;
    port.diag.permission_seen = true;
    inc(&port.diag.permission_received);
}

/* ---------------------------------------------------------------- TX ---- */
static NetSubmitResult finish(NetAppPortBcmPublish which, NetStatus st, bool in_step)
{
    NetAppPortBcmPublishDiag *d = &port.diag.publish[which];
    if (st == NET_OK) {
        if (in_step) inc(&d->deferred);
    } else {
        inc(&d->not_accepted);
    }
    d->last_engine = st;
    d->last_result = net_app_port_submit_result(st);
    port.in_publish = false;
    return d->last_result;
}

static NetSubmitResult refuse(NetAppPortBcmPublish which, NetStatus st)
{
    NetAppPortBcmPublishDiag *d = &port.diag.publish[which];
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

/* Current value: the M04 slot keeps the latest submission. */
static NetSubmitResult publish_value(NetAppPortBcmPublish which, const NetUpperCanMessage *m)
{
    NetUpperPublishNote note;
    NetStatus st;
    memset(&note, 0, sizeof(note));
    st = net_upper_publish_can(port.publisher, m, NULL, &note);
    return finish(which, st, note.in_step);
}

/* Discrete record: straight to the idle M04 slot, else kept in call order. */
static NetSubmitResult send_discrete(NetAppPortBcmPublish which, NetAppPortDiscreteQueue *q,
                                     const NetUpperCanMessage *m)
{
    NetUpperCanMessage probe = *m;
    NetWireCanEnvelope frame;
    NetUpperPublishNote note;
    NetStatus st;
    uint64_t now;
    memset(&note, 0, sizeof(note));
    /* Content check with a frame identity; M04 assigns the real one. */
    if (net_upper_can_set_identity(&probe, port.bcm_boot_id, 1u) != NET_WIRE_OK)
        return finish(which, NET_UNSUPPORTED, false);
    st = net_upper_wire_status(net_upper_can_encode(port.service->config.profile, &probe, &frame));
    if (st != NET_OK) return finish(which, st, false);
    /* Same link check as M04 (net_upper_publish.c): a failed or quiescing
     * link takes no record. */
    if (port.service->link_failed[NET_WIRE_CAN] || port.service->quiescing[NET_WIRE_CAN])
        return finish(which, NET_HOLD, false);
    if (net_app_port_discrete_direct(q, port.publisher)) {
        st = net_upper_publish_can(port.publisher, m, NULL, &note);
        return finish(which, st, note.in_step);
    }
    if (port.in_rx) now = port.rx_now;
    else {
        st = net_upper_now(port.service, &now);
        if (st != NET_OK) return finish(which, st, false);
    }
    st = net_app_port_discrete_enqueue(q, m, now);
    return finish(which, st, port.in_rx);
}

NetSubmitResult BcmNet_SendResult(const NetBcmResult *message)
{
    NetUpperCanMessage m;
    NetWireBcmResultRaw *r = &m.data.bcm_result;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_RESULT, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_RESULT, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x140u;
    /* boot_id / tx_sequence stay 0: M04 writes them into each frame. */
    r->command_domain_boot = message->command_domain_boot;
    r->command_id = message->command_id;
    r->command_kind = message->command_kind;
    r->result = message->result;
    r->result_reason = message->result_reason;
    r->door_event = message->door_event;
    r->occurrence_id = message->occurrence_id;
    r->event_age_ms = message->event_age_ms;
    r->event_quality = message->event_quality;
    r->door_target = message->door_target;
    return send_discrete(NET_APP_PORT_BCM_RESULT, &port.diag.result_store, &m);
}

NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message)
{
    NetUpperCanMessage m;
    NetWireBcmEventRaw *r = &m.data.bcm_event;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_EVENT, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_EVENT, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x141u;
    r->event_kind = message->event_kind;
    r->occurrence_id = message->occurrence_id;
    r->event_age_ms = message->event_age_ms;
    r->event_quality = message->event_quality;
    r->related_domain_boot = message->related_domain_boot;
    r->related_command_id = message->related_command_id;
    r->affected_function = message->affected_function;
    return send_discrete(NET_APP_PORT_BCM_EVENT, &port.diag.event_store, &m);
}

NetSubmitResult BcmNet_PublishDoorState(const NetBcmDoorState *message)
{
    NetUpperCanMessage m;
    NetWireBcmDoorStateRaw *r = &m.data.bcm_door_state;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_DOOR_STATE, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_DOOR_STATE, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x200u;
    r->door_lock_state = message->door_lock_state;
    r->door_open_state = message->door_open_state;
    r->door_composite_state = message->door_composite_state;
    r->lock_quality = message->lock_quality;
    r->open_quality = message->open_quality;
    r->composite_quality = message->composite_quality;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    return publish_value(NET_APP_PORT_BCM_DOOR_STATE, &m);
}

NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message)
{
    NetUpperCanMessage m;
    NetWireBcmClimateStateRaw *r = &m.data.bcm_climate_state;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_CLIMATE_STATE, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_CLIMATE_STATE, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x210u;
    r->fan_command_level = message->fan_command_level;
    r->fan_measured_level = message->fan_measured_level;
    r->thermal_direction_state = message->thermal_direction_state;
    r->thermal_output_percent = message->thermal_output_percent;
    r->heat_removal_state = message->heat_removal_state;
    r->fan_command_quality = message->fan_command_quality;
    r->fan_measured_quality = message->fan_measured_quality;
    r->thermal_direction_quality = message->thermal_direction_quality;
    r->thermal_output_quality = message->thermal_output_quality;
    r->heat_removal_quality = message->heat_removal_quality;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    return publish_value(NET_APP_PORT_BCM_CLIMATE_STATE, &m);
}

NetSubmitResult BcmNet_PublishLightState(const NetBcmLightState *message)
{
    NetUpperCanMessage m;
    NetWireBcmLightStateRaw *r = &m.data.bcm_light_state;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_LIGHT_STATE, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_LIGHT_STATE, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x220u;
    r->interior_light_type_state = message->interior_light_type_state;
    r->light_level_percent = message->light_level_percent;
    r->red = message->interior_light_color_state.r;
    r->green = message->interior_light_color_state.g;
    r->blue = message->interior_light_color_state.b;
    r->interior_light_apply_result = message->interior_light_apply_result;
    r->light_physical_feedback_capability = message->light_physical_feedback_capability;
    r->light_type_quality = message->light_type_quality;
    r->light_level_quality = message->light_level_quality;
    r->light_color_quality = message->light_color_quality;
    r->light_apply_quality = message->light_apply_quality;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    return publish_value(NET_APP_PORT_BCM_LIGHT_STATE, &m);
}

NetSubmitResult BcmNet_PublishStatus(const NetBcmStatus *message)
{
    NetUpperCanMessage m;
    NetWireBcmStatusRaw *r = &m.data.bcm_status;
    if (message == NULL) return refuse(NET_APP_PORT_BCM_STATUS, NET_INVALID_ARGUMENT);
    if (!enter()) return refuse(NET_APP_PORT_BCM_STATUS, NET_NOT_CONFIGURED);
    memset(&m, 0, sizeof(m));
    m.id = 0x230u;
    r->bcm_ecu_state = message->bcm_ecu_state;
    r->fault_mask = message->fault_mask;
    r->active_fault_category = message->active_fault_category;
    r->active_fault_code = message->active_fault_code;
    r->affected_function = message->affected_function;
    r->recovering = message->recovering;
    r->update_sequence = message->update_sequence;
    r->source_age_ms = message->source_age_ms;
    return publish_value(NET_APP_PORT_BCM_STATUS, &m);
}

void net_app_port_bcm_service(void)
{
    if (!port.bound) return;
    net_app_port_discrete_service(&port.diag.result_store, port.publisher);
    net_app_port_discrete_service(&port.diag.event_store, port.publisher);
}

/* ------------------------------------------------------------ binding --- */
NetStatus net_app_port_bcm_bind(NetUpper *service, NetEcuAdapter *ecu,
                                const NetAppPortBcmConfig *config)
{
    static const uint16_t need[] = { 0x140u, 0x141u, 0x200u, 0x210u, 0x220u, 0x230u };
    NetUpperPublishView view;
    size_t i;
    if (service == NULL || ecu == NULL || config == NULL || config->publisher == NULL ||
        config->publisher->service != service || config->bcm_boot_id == 0u)
        return NET_INVALID_ARGUMENT;
    for (i = 0u; i < sizeof(need) / sizeof(need[0]); ++i)
        if (net_upper_publish_view(config->publisher, NET_WIRE_CAN, need[i], &view) != NET_OK)
            return NET_INVALID_ARGUMENT;
    memset(&port, 0, sizeof(port));
    port.service = service;
    port.ecu = ecu;
    port.publisher = config->publisher;
    port.bcm_boot_id = config->bcm_boot_id;
    net_app_port_discrete_init(&port.diag.result_store, 0x140u);
    net_app_port_discrete_init(&port.diag.event_store, 0x141u);
    port.bound = true;
    return NET_OK;
}

NetStatus net_app_port_bcm_diag(NetAppPortBcmDiag *out)
{
    if (out == NULL) return NET_INVALID_ARGUMENT;
    *out = port.diag;
    out->bound = port.bound;
    return NET_OK;
}

#else
/* Not the BCM node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_app_port_bcm_absent;
#endif
