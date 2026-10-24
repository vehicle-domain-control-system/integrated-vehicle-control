/* Network node composition root: the part common to every node profile.
 * Init order, the main pass and the diagnostics view live here; what differs
 * per node (links, receive policies, App Port facade, permission supervision,
 * M04 publish table) comes from the profile table net_node_profile
 * (net_node_profile_<node>.c, one per build).
 * Common policies (docs/network/interface):
 *   every receive route  : bound delivery age, 2000 ms unless the profile narrows it
 *   DOMAIN_PERMISSION 0x080: period 50 ms / receive timeout 150 ms (supervision);
 *                            every wired node receives it */
#include "network/net_node.h"
#include "net_node_profile.h"
#include "net_board_cfg.h"
#include <string.h>

#if NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_VSS && NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_CIS && \
    NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_BCM
#error "net_node.c: no node profile table for this NETWORK_UPPER_NODE_PROFILE (wired: VSS=6, CIS=1, BCM=3)"
#endif

/* The MCU (NETWORK_NODE_PLATFORM) is chosen in net_board_cfg.h from the S32DS
 * CPU define or an explicit -D; the node role does not choose it. */
#ifndef NETWORK_NODE_PLATFORM
#error "net_node.c: no MCU selected (S32DS CPU define CPU_S32K144HFT0VLLT / CPU_S32K344, or NETWORK_NODE_PLATFORM=144 / 344)"
#endif

static NetUpper service;
static NetHalAdapter hal;
static NetEcuAdapter ecu;
static NetUpperPublisher publisher;
static NetPermissionGuard permission;

static NetNodeStage stage;
static NetStatus stage_status;
static uint32_t steps;
static bool init_called;

static NetStatus fail(NetNodeStage s, NetStatus st) { stage = s; stage_status = st; return st; }

static void rx_defaults(NetUpperConfig *c)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        c->rx_policy[i].bound = true;
        c->rx_policy[i].maximum_age_ms = 2000u;
        c->rx_policy[i].expires_at_equality = true;
    }
}

static bool config_valid(const NetNodeConfig *config)
{
    const NetNodeProfile *p = &net_node_profile;
    if (config == NULL || config->boot_id == 0u || config->local_epoch == 0u || config->can_port_epoch == 0u)
        return false;
    /* A link of the profile needs its own epoch; one it lacks is ignored. */
    if (p->uart_m && config->uart_m_port_epoch == 0u) return false;
    if (p->uart_c && config->uart_c_port_epoch == 0u) return false;
    return true;
}

NetStatus NetNode_Init(const NetNodeConfig *config)
{
    const NetNodeProfile *p = &net_node_profile;
    NetHalRtdConfig rtd_in;
    NetHalConfig hal_cfg;
    NetUpperConfig c;
    NetUpperPublishConfig pub;
    NetPermissionPolicy perm;
    NetNodeParts parts;
    NetStatus st;

    /* Engine storage is fresh-storage only: once composition has started, a
     * second call must not touch the running (or failed) node. */
    if (init_called) return NET_BUSY;
    stage = NET_NODE_STAGE_NONE; stage_status = NET_OK; steps = 0u;
    if (!config_valid(config)) return fail(NET_NODE_STAGE_NONE, NET_INVALID_ARGUMENT);
    init_called = true;

    /* Every field: NetHalRtdConfig also carries the UART link epochs. */
    memset(&rtd_in, 0, sizeof(rtd_in));
    rtd_in.can_port_epoch = config->can_port_epoch;
    if (p->uart_m) rtd_in.uart_m_port_epoch = config->uart_m_port_epoch;
    if (p->uart_c) rtd_in.uart_c_port_epoch = config->uart_c_port_epoch;
    st = net_hal_rtd_config(&hal, &rtd_in, &hal_cfg);
    if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_CONFIG, st);
    st = net_hal_adapter_init(&hal, &hal_cfg, (NetHalPlatform)NETWORK_NODE_PLATFORM);
    if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_ADAPTER, st);

    st = net_ecu_adapter_init(&ecu, &service, p->node, p->callbacks(), NULL);
    if (st != NET_OK) return fail(NET_NODE_STAGE_ECU_ADAPTER, st);

    memset(&c, 0, sizeof(c));
    c.node = p->node;
    c.local_epoch = config->local_epoch;
    c.port_epoch[NET_WIRE_CAN] = config->can_port_epoch;
    /* Links the profile lacks still need a nonzero epoch in the service. */
    c.port_epoch[NET_WIRE_UART_M] = p->uart_m ? config->uart_m_port_epoch : config->can_port_epoch;
    c.port_epoch[NET_WIRE_UART_C] = p->uart_c ? config->uart_c_port_epoch : config->can_port_epoch;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port = net_hal_rtd_port(&hal);   /* adapter port; UART ISR bytes taken before the M06 timer check */
    c.owner = net_ecu_adapter_owner(&ecu);
    rx_defaults(&c);
    p->upper(&c);
    st = net_upper_init(&service, &c);
    if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);

    memset(&pub, 0, sizeof(pub));
    pub.can_boot_id = config->boot_id;
    pub.entries = p->publish;
    pub.entry_count = p->publish_count;
    st = net_upper_publish_init(&publisher, &service, &pub);
    if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);

    parts.service = &service;
    parts.ecu = &ecu;
    parts.publisher = &publisher;
    parts.config = config;
    st = p->bind(&parts);
    if (st != NET_OK) return fail(NET_NODE_STAGE_APP_PORT, st);

    memset(&perm, 0, sizeof(perm));
    perm.receive_timeout.bound = true;
    perm.receive_timeout.maximum_age_ms = 150u;
    perm.receive_timeout.expires_at_equality = true;
    p->permission(&perm);
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
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET); /* RX dispatch -> XxxApp_* ; TX facts */
    (void)net_permission_step(&permission);                /* DOMAIN_PERMISSION supervision */
    net_hal_rtd_service();                                 /* stuck-TX abort, error counters */
    if (net_node_profile.service != NULL) net_node_profile.service(); /* App Port -> idle M04 slots */
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
    net_node_profile.diag(&publisher, out);
    out->permission_valid = net_permission_snapshot(&permission, &out->permission) == NET_OK;
    return NET_OK;
}
