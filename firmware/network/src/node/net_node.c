/* Network node composition root for the VSS profile.
 * Policies below come from docs/network/interface:
 *   DOMAIN_PERMISSION 0x080 : period 50 ms / RX timeout 150 ms, original Power life 500 ms
 *   VSS_EVENT        0x160 : discrete; original-age cap 2000 ms (L = min(USE_LIMIT,500,2000)
 *                            is applied by the VSS application, not here)
 *   VSS_WARNING_STATE 0x0C0: per-lane ages (per-field mode); local delivery bound only
 *   VSS_STATUS (TX)  0x340 : freshness from source_age_ms; receiver judges staleness;
 *                            one frame per submission, never repeated (table below) */
#include "network/net_node.h"
#include <string.h>

#if NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_VSS
#error "net_node.c currently wires the VSS profile only (NETWORK_UPPER_NODE_PROFILE=6)"
#endif

#define NODE_ID NET_WIRE_NODE_VSS

static NetUpper service;
static NetHalAdapter hal;
static NetEcuAdapter ecu;
static NetUpperPublisher publisher;
static NetPermissionGuard permission;

/* M04 publish table of the VSS profile: messages whose BOOT_ID / TX_SEQUENCE
 * and waiting AGE Network owns, and their repetition period.
 * VSS_STATUS is submitted by the VSS application itself: immediately on every
 * change and otherwise as a new evaluation every 250 ms (firmware/vss
 * README §6 submission rule; 10_CAN_FD.md: 250 ms period / 750 ms receive
 * timeout). Network therefore sends one frame per submission and registers
 * no period here; a period would put a second frame on the bus each status
 * period. Freshness bound of a submission: source_age_ms plus the local wait;
 * the wire saturates at 65535 and DOMAIN judges staleness. */
static const NetUpperPublishEntry publish_table[] = {
    { NET_WIRE_CAN, 0x340u, 0u, { true, 65535u + 1000u, true } }
};
static NetNodeStage stage;
static NetStatus stage_status;
static uint32_t steps;
static bool init_called;

static NetStatus fail(NetNodeStage s, NetStatus st) { stage = s; stage_status = st; return st; }

static void rx_policies(NetUpperConfig *c)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        c->rx_policy[i].bound = true;
        c->rx_policy[i].maximum_age_ms = 2000u;
        c->rx_policy[i].expires_at_equality = true;
        if (r == NULL || r->link != NET_WIRE_CAN) continue;
        if (r->id_type == 0x080u) c->rx_policy[i].maximum_age_ms = 500u;
        if (r->id_type == 0x0C0u) {
            c->rx_per_field_age[i] = true;          /* three independent lanes + activation */
            c->rx_policy[i].maximum_age_ms = 1000u; /* local capture->delivery bound */
        }
    }
}

NetStatus NetNode_Init(const NetNodeConfig *config)
{
    NetHalRtdConfig rtd_in;
    NetHalConfig hal_cfg;
    NetUpperConfig c;
    NetAppPortVssConfig app;
    NetUpperPublishConfig pub;
    NetPermissionPolicy perm;
    NetStatus st;

    /* Engine storage is fresh-storage only: once composition has started, a
     * second call must not touch the running (or failed) node. */
    if (init_called) return NET_BUSY;
    stage = NET_NODE_STAGE_NONE; stage_status = NET_OK; steps = 0u;
    if (config == NULL || config->boot_id == 0u || config->local_epoch == 0u || config->can_port_epoch == 0u)
        return fail(NET_NODE_STAGE_NONE, NET_INVALID_ARGUMENT);
    init_called = true;

    rtd_in.can_port_epoch = config->can_port_epoch;
    st = net_hal_rtd_config(&hal, &rtd_in, &hal_cfg);
    if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_CONFIG, st);
    st = net_s32k344_upper_port_init(&hal, &hal_cfg);
    if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_ADAPTER, st);

    st = net_ecu_adapter_init(&ecu, &service, NODE_ID, net_app_port_vss_callbacks(), NULL);
    if (st != NET_OK) return fail(NET_NODE_STAGE_ECU_ADAPTER, st);

    memset(&c, 0, sizeof(c));
    c.node = NODE_ID;
    c.local_epoch = config->local_epoch;
    c.port_epoch[NET_WIRE_CAN] = config->can_port_epoch;
    c.port_epoch[NET_WIRE_UART_M] = config->can_port_epoch; /* unused links still need nonzero */
    c.port_epoch[NET_WIRE_UART_C] = config->can_port_epoch;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port = net_hal_adapter_port(&hal);
    c.owner = net_ecu_adapter_owner(&ecu);
    rx_policies(&c);
    st = net_upper_init(&service, &c);
    if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);

    memset(&pub, 0, sizeof(pub));
    pub.can_boot_id = config->boot_id;
    pub.entries = publish_table;
    pub.entry_count = sizeof(publish_table) / sizeof(publish_table[0]);
    st = net_upper_publish_init(&publisher, &service, &pub);
    if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);

    memset(&app, 0, sizeof(app));
    app.publisher = &publisher;
    st = net_app_port_vss_bind(&service, &ecu, &app);
    if (st != NET_OK) return fail(NET_NODE_STAGE_APP_PORT, st);

    memset(&perm, 0, sizeof(perm));
    perm.receive_timeout.bound = true;
    perm.receive_timeout.maximum_age_ms = 150u;
    perm.receive_timeout.expires_at_equality = true;
    st = net_permission_init(&permission, &service, &perm);
    if (st != NET_OK) return fail(NET_NODE_STAGE_PERMISSION, st);

    st = net_hal_rtd_start();
    if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_START, st);

    stage = NET_NODE_STAGE_READY;
    return NET_OK;
}

void NetNode_Step(void)
{
    if (stage != NET_NODE_STAGE_READY) return;
    if (steps != UINT32_MAX) ++steps;
    (void)net_hal_rtd_drain();                             /* ISR ring -> adapter, bounded, main context */
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET); /* RX dispatch -> VssApp_* ; TX facts */
    (void)net_permission_step(&permission);                /* DOMAIN_PERMISSION supervision */
    net_hal_rtd_service();                                 /* stuck-TX abort, error counters */
    (void)net_upper_publish_service(&publisher);           /* M04: retire, BOOT/TX, due frames */
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET); /* dispatch what M04 offered */
}

NetStatus NetNode_GetDiag(NetNodeDiag *out)
{
    if (out == NULL) return NET_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->stage = stage;
    out->stage_status = stage_status;
    out->steps = steps;
    net_entry_diag(out);
    if (stage != NET_NODE_STAGE_READY) { (void)net_hal_rtd_diag(&out->rtd); return NET_OK; }
    (void)net_upper_observation(&service, &out->upper);
    (void)net_hal_adapter_observation(&hal, &out->hal);
    (void)net_hal_rtd_diag(&out->rtd);
    (void)net_app_port_vss_diag(&out->app);
    (void)net_upper_publish_view(&publisher, NET_WIRE_CAN, 0x340u, &out->status_tx);
    out->permission_valid = net_permission_snapshot(&permission, &out->permission) == NET_OK;
    return NET_OK;
}
