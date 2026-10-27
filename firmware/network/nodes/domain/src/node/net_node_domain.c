/* Network node composition root of the DOMAIN profile (CAN + UART-M to the
 * ESP32). The DOMAIN C2 adapter (network/net_domain_c2.h) is the Owner of the
 * service: received frames reach the DOMAIN application through its C2 Rx
 * functions, and the application transmits through the C2 TxPort the adapter
 * registers here. DOMAIN sends DOMAIN_PERMISSION, so there is no permission
 * supervision. VSS, CIS and BCM use src/node/net_node.c; for them this file
 * compiles to nothing.
 * Policies (docs/network/interface):
 *   DOMAIN_PERMISSION 0x080 (CAN TX): 50 ms period; the 500 ms original Power
 *        life bounds a submission and every repetition (03_COMMUNICATION/
 *        10_CAN_FD.md, 04_COMMON/10_DATA_AND_ID_RULES.md consumption boundary)
 *   VSS_WARNING_STATE 0x0C0 (CAN TX): 50 ms period, per-lane AGEs judged by VSS
 *   M_DOMAIN_ALIVE 0x02 (UART-M TX)  : 250 ms period (03_COMMUNICATION/20_UART_M.md)
 *   CIS_ENVIRONMENT 0x300, BCM_STATUS 0x230, CIS_STATUS 0x320, VSS_STATUS 0x340
 *        (CAN RX): per-field AGE, the bound covers the local wait only; DOMAIN
 *        judges freshness
 *   every other receive route: 2000 ms delivery age bound
 *   UART-M stream: 50 ms frame assembly bound, LINK_SEQUENCE from 1 per
 *        direction (20_UART_M.md shared framing)
 * Order: DOMAIN initializes its interface (DomainIf_Init), then Net_Init;
 * NetNode_Init registers the C2 TxPort. Main context only, never an ISR. */
#include "network/net_node.h"
#include "net_node_profile.h"
#include "net_board_cfg.h"
#include <string.h>

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN

/* The MCU (NETWORK_NODE_PLATFORM) is chosen in net_board_cfg.h from the S32DS
 * CPU define or an explicit -D; the node role does not choose it. */
#ifndef NETWORK_NODE_PLATFORM
#error "net_node_domain.c: no MCU selected (S32DS CPU define CPU_S32K144HFT0VLLT / CPU_S32K344, or NETWORK_NODE_PLATFORM=144 / 344)"
#endif

#ifdef NET_BOARD_DOMAIN_C2_HOOK
/* Board / DOMAIN integration owned (net_board_cfg.h): the C2 boundary
 * functions, the semantic callbacks and the adapter settings. */
extern bool NET_BOARD_DOMAIN_C2_HOOK(NetDomainC2Api *api, NetDomainC2Config *config);
#endif

/* M04 publish table of the DOMAIN node.
 * DOMAIN_PERMISSION: DOMAIN submits a new evaluation or a changed meaning or
 * quality through the C2 TxPort (net_domain_tx.c synthesizes its CAN
 * UPDATE_SEQUENCE). Network sends the latest one every 50 ms with a new
 * TX_SEQUENCE and the original AGE basis (10_CAN_FD.md: 50 ms period / 150 ms
 * receive timeout). Freshness bound of a submission: 500 ms, the original
 * Power life the service applies to every 0x080 frame. A smaller bound would
 * refuse a withdrawal whose AGE is still below 500 ms while the older ALLOW
 * keeps repeating until 500 ms; a new restriction never loses to an older
 * permission.
 * VSS_WARNING_STATE: composite state of three independent lanes. Their AGEs
 * may be long (up to the saturated 65535) and VSS judges each lane, so the
 * bound only catches the saturated AGE plus margin; a smaller one would refuse
 * a new state and keep repeating the previous one.
 * M_DOMAIN_ALIVE: 250 ms (20_UART_M.md: 250 ms period / 750 ms receive
 * timeout). Without a DOMAIN source it carries SOURCE_AGE 0xFFFF, which the
 * bound still accepts. */
static const NetUpperPublishEntry publish_table[] = {
    { NET_WIRE_CAN, 0x080u, 50u, { true, 500u, true } },
    { NET_WIRE_CAN, 0x0C0u, 50u, { true, 65535u + 1000u, true } },
    { NET_WIRE_UART_M, 0x02u, 250u, { true, 65535u + 1000u, true } }
};
NET_NODE_PUBLISH_TABLE_FITS(publish_table);

/* Composite observations and states received with per-field AGE (net_upper.h
 * rx_per_field_age: the owner's explicit choice for composite observations and
 * history). CIS_ENVIRONMENT carries three sensors of their own AGE, and the
 * DOMAIN environment TTL is longer than the common 2000 ms bound; BCM_STATUS,
 * CIS_STATUS and VSS_STATUS keep the AGE of their last change. BCM door,
 * climate and light states (0x200, 0x210, 0x220) repeat an unchanged
 * evaluation with its original sequence and accumulated AGE
 * (docs/network/interface/02_NODES/BCM/20_API.md, repeat keeps the original
 * sequence and AGE), so their AGE passes 2000 ms while the frames keep coming.
 * The bound then covers the local wait only, every field AGE and quality reach
 * DOMAIN, and DOMAIN judges freshness; Network makes no STALE. CIS_REAR and
 * DOMAIN_PERMISSION keep their whole-message bound (the service refuses
 * per-field AGE there). */
static const uint16_t per_field_age[] = { 0x300u, 0x230u, 0x320u, 0x340u, 0x200u, 0x210u, 0x220u };

static NetUpper service;
static NetHalAdapter hal;
static NetUpperQuery query;
static NetDomainC2 c2;
static NetUpperPublisher publisher;

static NetNodeStage stage;
static NetStatus stage_status;
static uint32_t steps;
static bool init_called, c2_reached;
static uint32_t node_boot;

/* M_DOMAIN_ALIVE: the last accepted submission and the counts. */
static struct {
    bool sent;
    NetDomainC2AliveState value;
    uint32_t submitted, refused;
} alive;

static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }
static NetStatus fail(NetNodeStage s, NetStatus st) { stage = s; stage_status = st; return st; }

static bool config_valid(const NetNodeConfig *config)
{
    return config != NULL && config->boot_id != 0u && config->local_epoch != 0u &&
           config->can_port_epoch != 0u && config->uart_m_port_epoch != 0u;
}

static void upper(NetUpperConfig *c)
{
    size_t i, k;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        c->rx_policy[i].bound = true;
        c->rx_policy[i].maximum_age_ms = 2000u;
        c->rx_policy[i].expires_at_equality = true;
        if (r == NULL || r->link != NET_WIRE_CAN) continue;
        for (k = 0u; k < sizeof(per_field_age) / sizeof(per_field_age[0]); ++k)
            if (r->id_type == per_field_age[k]) c->rx_per_field_age[i] = true;
    }
    /* UART-M receiver (ESP32 -> DOMAIN) and the DOMAIN -> ESP32 LINK_SEQUENCE,
     * set like UART-C on CIS: 50 ms assembly bound (20_UART_M.md shared
     * framing, proposed), LINK_SEQUENCE from 1, modulo 65536. */
    c->uart_policy[NET_WIRE_UART_M - 1u].candidate_time_policy_selected = true;
    c->uart_policy[NET_WIRE_UART_M - 1u].assembly_limit_ms = 50u;
    c->uart_policy[NET_WIRE_UART_M - 1u].expires_at_equality = true;
    c->uart_sequence_bound[NET_WIRE_UART_M - 1u] = true;
    c->uart_initial_sequence[NET_WIRE_UART_M - 1u] = 1u;
}

/* C2 adapter: the board hook supplies the boundary and the semantic choices;
 * the node sets what belongs to this Network run, whatever the hook wrote:
 *   - one BOOT_ID for every frame (NetNodeConfig.boot_id; the M04 slots carry
 *     the same one), TX_SEQUENCE per message from 1 (10_CAN_FD.md common header);
 *   - DOMAIN_PERMISSION and VSS_WARNING_STATE through the M04 slots;
 *   - command TARGET_BOOT_ID from the received BCM frames;
 *   - 0x300 / 0x310 only through their CIS transition barriers
 *     (docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md: the environment
 *     route is held until the private transition is confirmed; the DOMAIN Owner
 *     refuses both before its transition is applied). The transition binding
 *     is the hook's (NetDomainC2Config.transition, a DOMAIN-side function); the
 *     adapter calls it from net_domain_c2_step in this pass. Without it both
 *     barriers stay closed and both routes are held. */
static NetStatus c2_bind(const NetNodeConfig *config)
{
#ifndef NET_BOARD_DOMAIN_C2_HOOK
    (void)config;
    return NET_NOT_CONFIGURED;
#else
    NetDomainC2Api api;
    NetDomainC2Config cfg;
    NetStatus st;
    memset(&api, 0, sizeof(api));
    memset(&cfg, 0, sizeof(cfg));
    if (!NET_BOARD_DOMAIN_C2_HOOK(&api, &cfg)) return NET_NOT_CONFIGURED;
    cfg.domain_boot = config->boot_id;
    cfg.first_can_sequence = 1u;
    cfg.publisher = &publisher;
    cfg.bcm_target_from_rx = true;
    cfg.cis_transition_barriers = true;
    st = net_domain_c2_init(&c2, &service, &query, &api, &cfg);
    if (st != NET_OK) return st;
    c2_reached = true;
    /* SetTxPort; NOT_CONFIGURED while DomainIf is not initialized. */
    return net_domain_c2_register(&c2);
#endif
}

NetStatus NetNode_Init(const NetNodeConfig *config)
{
    NetStatus st;

    /* Fresh storage only: once composition has started, a second call must not
     * touch the running (or failed) node. */
    if (init_called) return NET_BUSY;
    stage = NET_NODE_STAGE_NONE; stage_status = NET_OK; steps = 0u;
    if (!config_valid(config)) return fail(NET_NODE_STAGE_NONE, NET_INVALID_ARGUMENT);
    init_called = true;
    node_boot = config->boot_id;

    /* Each stage builds its configuration in a block of its own; the init call
     * copies it and keeps no pointer to it (init path only). */
    {
        NetHalRtdConfig rtd_in;
        NetHalConfig hal_cfg;
        memset(&rtd_in, 0, sizeof(rtd_in));
        rtd_in.can_port_epoch = config->can_port_epoch;
        rtd_in.uart_m_port_epoch = config->uart_m_port_epoch;
        st = net_hal_rtd_config(&hal, &rtd_in, &hal_cfg);
        if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_CONFIG, st);
        st = net_hal_adapter_init(&hal, &hal_cfg, (NetHalPlatform)NETWORK_NODE_PLATFORM);
        if (st != NET_OK) return fail(NET_NODE_STAGE_HAL_ADAPTER, st);
    }

    {
        NetUpperConfig c;
        memset(&c, 0, sizeof(c));
        c.node = NET_WIRE_NODE_DOMAIN;
        c.local_epoch = config->local_epoch;
        c.port_epoch[NET_WIRE_CAN] = config->can_port_epoch;
        c.port_epoch[NET_WIRE_UART_M] = config->uart_m_port_epoch;
        c.port_epoch[NET_WIRE_UART_C] = config->can_port_epoch; /* no UART-C: any nonzero epoch */
        c.profile = NET_WIRE_CANDIDATE_ONLY;
        c.port = net_hal_rtd_port(&hal);
        c.owner = net_domain_c2_owner(&c2);
        upper(&c);
        st = net_upper_init(&service, &c);
        if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);
    }

    {
        NetUpperQueryOwner owner = net_domain_c2_query_owner(&c2);
        st = net_upper_query_init(&query, &service, config->boot_id, &owner);
        if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);
    }

    {
        NetUpperPublishConfig pub;
        memset(&pub, 0, sizeof(pub));
        pub.can_boot_id = config->boot_id;
        pub.entries = publish_table;
        pub.entry_count = sizeof(publish_table) / sizeof(publish_table[0]);
        st = net_upper_publish_init(&publisher, &service, &pub);
        if (st != NET_OK) return fail(NET_NODE_STAGE_UPPER, st);
    }

    st = c2_bind(config);
    if (st != NET_OK) return fail(NET_NODE_STAGE_APP_PORT, st);

    st = net_hal_rtd_start();
    if (st != NET_OK) {
        (void)net_domain_c2_unregister(&c2); /* no TxPort into a node that never runs */
        return fail(NET_NODE_STAGE_HAL_START, st);
    }

    stage = NET_NODE_STAGE_READY;
    return NET_OK;
}

/* M_DOMAIN_ALIVE (UART-M 0x02, 03_COMMUNICATION/20_UART_M.md): DOMAIN_BOOT_ID
 * is the node BOOT_ID; DOMAIN_STATE, VALUE_QUALITY, UPDATE_SEQUENCE and
 * SOURCE_AGE are the DOMAIN application's (alive_state of the C2 settings).
 * Without that source: INIT / NO_DATA / 0 / 0xFFFF. Network never makes up
 * READY, an evaluation number or an AGE 0. */
static void alive_value(NetDomainC2AliveState *out)
{
    NetDomainC2AliveState v;
    memset(&v, 0, sizeof(v));
    if (c2.config.alive_state != NULL && c2.config.alive_state(c2.config.context, &v) == NET_OK) {
        *out = v;
        return;
    }
    memset(out, 0, sizeof(*out));
    out->domain_state = 0u;          /* INIT */
    out->value_quality = 3u;         /* NO_DATA */
    out->update_sequence = 0u;
    out->source_age_ms = UINT16_MAX; /* no evaluation time */
}

/* The first submission is made in the first pass of the READY node (Network
 * time is then the nowMs of that pass). A changed state, quality or evaluation
 * number is submitted in the pass it is seen; M04 repeats the latest one every
 * 250 ms with its AGE growing from the submission, so a SOURCE_AGE that only
 * grew is not a change. A refused submission is tried again next pass. */
static void alive_service(void)
{
    NetDomainC2AliveState v;
    NetUpperUartMessage m;
    alive_value(&v);
    if (alive.sent && v.domain_state == alive.value.domain_state &&
        v.value_quality == alive.value.value_quality && v.update_sequence == alive.value.update_sequence)
        return;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_M;
    m.type = 0x02u;
    m.data.m_domain_alive.domain_boot_id = node_boot;
    m.data.m_domain_alive.update_sequence = v.update_sequence;
    m.data.m_domain_alive.source_age_ms = v.source_age_ms;
    m.data.m_domain_alive.domain_state = v.domain_state;
    m.data.m_domain_alive.value_quality = v.value_quality;
    if (net_upper_publish_uart(&publisher, &m, NULL, NULL) == NET_OK) {
        alive.sent = true;
        alive.value = v;
        inc(&alive.submitted);
    } else {
        inc(&alive.refused);
    }
}

void NetNode_Step(void)
{
    if (stage != NET_NODE_STAGE_READY) return;
    inc(&steps);
    (void)net_hal_rtd_drain();                             /* ISR ring -> adapter, bounded, main context */
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET); /* RX -> C2 Rx functions; TX facts */
    (void)net_domain_c2_step(&c2);                         /* CIS transitions; TX records: failure notices, retirement */
    (void)net_upper_query_step(&query);                    /* M_QUERY getters and responses */
    net_hal_rtd_service();                                 /* stuck-TX abort, error counters */
    alive_service();                                       /* M_DOMAIN_ALIVE -> its M04 slot */
    (void)net_upper_publish_service(&publisher);           /* M04: retire, BOOT/TX, due frames */
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET); /* dispatch what M04 and C2 offered */
}

/* Send half of the pass (Net_TxFlush): the M04 publish service and the
 * dispatch, no receive delivery and no application callback (as in
 * src/node/net_node.c). */
void NetNode_TxFlush(void)
{
    if (stage != NET_NODE_STAGE_READY) return;
    (void)net_upper_publish_service(&publisher);
    (void)net_upper_dispatch(&service);
}

uint32_t NetNode_GetBootId(void)
{
    return stage == NET_NODE_STAGE_READY ? publisher.can_boot_id : 0u;
}

NetDomainC2 *NetNode_DomainC2(void)
{
    return c2_reached ? &c2 : NULL;
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
    (void)net_upper_publish_view(&publisher, NET_WIRE_CAN, 0x080u, &out->permission_tx);
    (void)net_upper_publish_view(&publisher, NET_WIRE_CAN, 0x0C0u, &out->warning_tx);
    (void)net_upper_publish_view(&publisher, NET_WIRE_UART_M, 0x02u, &out->alive_tx);
    out->permission_sequence = c2.permission;
    memcpy(out->barrier, c2.barrier, sizeof(out->barrier));
    out->bcm_target_boot = c2.bcm_status_boot != 0u ? c2.bcm_status_boot : c2.bcm_frame_boot;
    out->c2_boundary_calls = c2.boundary_calls;
    out->c2_last_status = (uint8_t)c2.last_c2_status;
    out->alive = alive.value;
    out->alive_submitted = alive.submitted;
    out->alive_refused = alive.refused;
    out->permission_valid = false; /* DOMAIN sends DOMAIN_PERMISSION; no supervision here */
    return NET_OK;
}

#else
/* Not the DOMAIN node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_node_domain_absent;
#endif
