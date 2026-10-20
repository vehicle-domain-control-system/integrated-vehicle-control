#if defined(__MINGW32__)
#define __USE_MINGW_ANSI_STDIO 1
#endif
/* HAL-less node worker: the node profile table of this build
 * (src/node/net_node_profile_<node>.c) and its App Port facade
 * (net_app_port_<node>.c) run on the Host virtual backend instead of the RTD
 * backend. No SDK header, no mock SDK. The composition below follows
 * NetNode_Init() / NetNode_Step() (src/node/net_node.c) step by step; only the
 * three RTD backend calls (config / start, ISR ring drain, TX supervision) are
 * replaced by the virtual backend, which captures frames and bytes directly.
 *
 * The XxxApp_* callbacks are a small test application: they print what the
 * facade delivered and answer through the facade (XxxNet_*), so a frame from a
 * peer comes back as a frame of this node. Line protocol on stdin, JSON lines
 * on stdout, every command ends with a "done" line. */
#include "network/net_node.h"
#include "network/net_virtual.h"
#include "net_node_profile.h"
#include "virtual_cli.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
#define NODE_NAME "VSS"
#define NODE_BOOT 601u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#define NODE_NAME "CIS"
#define NODE_BOOT 401u
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
#define NODE_NAME "BCM"
#define NODE_BOOT 201u
#else
#error "virtual_node.c: no node profile table for this NETWORK_UPPER_NODE_PROFILE"
#endif
#define PORT_EPOCH 3u

static NetUpper service;
static NetHalAdapter hal;
static NetVirtualBackend backend;
static NetEcuAdapter ecu;
static NetUpperPublisher publisher;
static NetPermissionGuard permission;
static NetNodeConfig node_config;
static bool ready;

static NetStatus wire_output(void *ctx, const NetUpperPacket *p, uint64_t at)
{
    (void)ctx;
    vcli_wire(NODE_NAME, p, at);
    return NET_OK;
}

static void submit_line(const char *what, NetSubmitResult r)
{
    static const char *names[] = {"OK", "INVALID_ARGUMENT", "NOT_READY", "NO_CAPACITY"};
    printf("{\"event\":\"app_submit\",\"node\":\"%s\",\"call\":\"%s\",\"result\":\"%s\"}\n", NODE_NAME, what,
           (unsigned)r < sizeof(names) / sizeof(names[0]) ? names[r] : "UNKNOWN");
}

static void meta_json(const NetCanRxMeta *meta)
{
    printf("\"boot\":%u,\"tx\":%u,\"received_at\":%u", (unsigned)meta->source.boot_id,
           (unsigned)meta->source.tx_sequence, (unsigned)meta->received_at_ms);
}

/* ----------------------------------------------- test application (RX) --- */
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
void VssApp_OnDomainEvent(const NetVssEvent *m, const NetCanRxMeta *meta)
{
    NetVssStatus s;
    printf("{\"event\":\"app_rx\",\"node\":\"VSS\",\"call\":\"VssApp_OnDomainEvent\",\"kind\":%u,\"occurrence\":%u,"
           "\"age\":%u,\"use_limit\":%u,\"related\":%u,",
           (unsigned)m->event_kind, (unsigned)m->occurrence_id, (unsigned)m->original_age_ms,
           (unsigned)m->use_limit_ms, (unsigned)m->related_command_id);
    meta_json(meta);
    puts("}");
    /* Answer with a status evaluation whose number echoes the occurrence. */
    memset(&s, 0, sizeof(s));
    s.vss_state = NET_VSS_STATE_READY;
    s.vss_availability = NET_VSS_AVAILABILITY_FULL;
    s.vss_accepting_events = 1u;
    s.last_fault_quality = NET_QUALITY_NO_DATA;
    s.update_sequence = m->occurrence_id;
    s.source_age_ms = 1u;
    submit_line("VssNet_PublishStatus", VssNet_PublishStatus(&s));
}
void VssApp_OnDomainWarningState(const NetVssWarningState *m, const NetCanRxMeta *meta)
{
    printf("{\"event\":\"app_rx\",\"node\":\"VSS\",\"call\":\"VssApp_OnDomainWarningState\",\"rear_state\":%u,",
           (unsigned)m->rear_obstacle_state);
    meta_json(meta);
    puts("}");
}
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
void CisApp_OnDomainPermission(const NetDomainPermission *m, const NetCanRxMeta *meta)
{
    printf("{\"event\":\"app_rx\",\"node\":\"CIS\",\"call\":\"CisApp_OnDomainPermission\",\"power\":%u,"
           "\"update\":%u,\"age\":%u,",
           (unsigned)m->vehicle_power_permission, (unsigned)m->update_sequence, (unsigned)m->source_age_ms);
    meta_json(meta);
    puts("}");
}
void CisApp_OnPiVisionResult(const NetVisionResult *m, const NetUartRxMeta *meta)
{
    NetCisOccupant o;
    printf("{\"event\":\"app_rx\",\"node\":\"CIS\",\"call\":\"CisApp_OnPiVisionResult\",\"pi_boot\":%u,"
           "\"vision\":%u,\"count\":%u,\"age\":%u,\"link_sequence\":%u,\"received_at\":%u}\n",
           (unsigned)m->pi_boot_id, (unsigned)m->vision_sequence, (unsigned)m->occupant_count,
           (unsigned)m->vision_age_ms, (unsigned)meta->link_sequence, (unsigned)meta->received_at_ms);
    /* Relay the judgement unchanged as CIS_OCCUPANT (02_NODES/CIS/20_API.md). */
    memset(&o, 0, sizeof(o));
    o.pi_boot_id = m->pi_boot_id;
    o.vision_sequence = m->vision_sequence;
    o.vision_age_ms = m->vision_age_ms;
    o.occupant_presence = m->occupant_presence;
    o.presence_validity = m->presence_validity;
    o.presence_reason = m->presence_reason;
    o.occupant_count = m->occupant_count;
    o.count_validity = m->count_validity;
    o.count_reason = m->count_reason;
    o.vision_function_status = m->vision_status;
    submit_line("CisNet_PublishOccupant", CisNet_PublishOccupant(&o));
}
void CisApp_OnPiVisionStatus(const NetVisionStatus *m, const NetUartRxMeta *meta)
{
    printf("{\"event\":\"app_rx\",\"node\":\"CIS\",\"call\":\"CisApp_OnPiVisionStatus\",\"status\":%u,"
           "\"link_sequence\":%u}\n",
           (unsigned)m->vision_status, (unsigned)meta->link_sequence);
}
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *m, const NetCanRxMeta *meta)
{
    NetBcmResult r;
    printf("{\"event\":\"app_rx\",\"node\":\"BCM\",\"call\":\"BcmApp_OnDomainDoorCommand\",\"command\":%u,"
           "\"target_boot\":%u,\"target\":%u,\"age\":%u,",
           (unsigned)m->command.command_id, (unsigned)m->command.target_boot_id, (unsigned)m->door_lock_target,
           (unsigned)m->command.command_age_ms);
    meta_json(meta);
    puts("}");
    /* Host owner: an explicit rejection (no actuator, no fabricated DONE). */
    memset(&r, 0, sizeof(r));
    r.command_domain_boot = meta->source.boot_id;
    r.command_id = m->command.command_id;
    r.command_kind = NET_BCM_COMMAND_DOOR;
    r.result = NET_RESULT_REJECTED;
    r.result_reason = NET_REASON_NOT_READY;
    r.door_event = NET_DOOR_EVENT_NONE;
    r.event_quality = NET_QUALITY_OK;
    r.door_target = m->door_lock_target;
    submit_line("BcmNet_SendResult", BcmNet_SendResult(&r));
}
void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *m, const NetCanRxMeta *meta)
{
    printf("{\"event\":\"app_rx\",\"node\":\"BCM\",\"call\":\"BcmApp_OnDomainClimateCommand\",\"command\":%u,",
           (unsigned)m->command.command_id);
    meta_json(meta);
    puts("}");
}
void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *m, const NetCanRxMeta *meta)
{
    printf("{\"event\":\"app_rx\",\"node\":\"BCM\",\"call\":\"BcmApp_OnDomainLightCommand\",\"command\":%u,",
           (unsigned)m->command.command_id);
    meta_json(meta);
    puts("}");
}
#endif

/* ----------------------------------------------- test application (TX) --- */
static NetStatus app_command(const char *line)
{
    unsigned a = 0u, b = 0u;
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
    if (sscanf(line, "vss_status %u", &a) == 1) {
        NetVssStatus s;
        memset(&s, 0, sizeof(s));
        s.vss_state = NET_VSS_STATE_READY;
        s.vss_availability = NET_VSS_AVAILABILITY_FULL;
        s.vss_accepting_events = 1u;
        s.last_fault_quality = NET_QUALITY_NO_DATA;
        s.update_sequence = a;
        submit_line("VssNet_PublishStatus", VssNet_PublishStatus(&s));
        return NET_OK;
    }
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    if (sscanf(line, "cis_rear %u %u", &a, &b) == 2) {
        NetCisRear r;
        if (b > UINT16_MAX) return NET_INVALID_ARGUMENT;
        memset(&r, 0, sizeof(r));
        r.update_sequence = a;
        r.source_age_ms = 2u;
        r.rear_distance_deci_cm = (uint16_t)b;
        r.validity = NET_VALIDITY_VALID;
        r.quality_reason = NET_QUALITY_REASON_NONE;
        r.proximity_status = NET_PROXIMITY_VALID_DISTANCE;
        submit_line("CisNet_PublishRear", CisNet_PublishRear(&r));
        return NET_OK;
    }
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
    if (sscanf(line, "bcm_door_state %u", &a) == 1) {
        NetBcmDoorState s;
        memset(&s, 0, sizeof(s));
        s.door_lock_state = NET_DOOR_LOCK_LOCKED;
        s.door_open_state = NET_DOOR_OPEN_CLOSED;
        s.door_composite_state = NET_DOOR_COMPOSITE_NORMAL;
        s.lock_quality = NET_QUALITY_OK;
        s.open_quality = NET_QUALITY_OK;
        s.composite_quality = NET_QUALITY_OK;
        s.update_sequence = a;
        s.source_age_ms = 1u;
        submit_line("BcmNet_PublishDoorState", BcmNet_PublishDoorState(&s));
        return NET_OK;
    }
#endif
    (void)a;
    (void)b;
    (void)line;
    return NET_UNSUPPORTED;
}

/* --------------------------------------------------------- composition --- */
static int initialize(void)
{
    const NetNodeProfile *p = &net_node_profile;
    NetHalConfig hal_cfg;
    NetUpperConfig c;
    NetUpperPublishConfig pub;
    NetPermissionPolicy perm;
    NetNodeParts parts;
    uint32_t links = NET_HAL_LINK_BIT(NET_WIRE_CAN);
    size_t i;

    memset(&node_config, 0, sizeof(node_config));
    node_config.boot_id = NODE_BOOT;
    node_config.local_epoch = 10u + p->node;
    node_config.can_port_epoch = PORT_EPOCH;
    node_config.uart_m_port_epoch = PORT_EPOCH;
    node_config.uart_c_port_epoch = PORT_EPOCH;
    if (p->uart_m) links |= NET_HAL_LINK_BIT(NET_WIRE_UART_M);
    if (p->uart_c) links |= NET_HAL_LINK_BIT(NET_WIRE_UART_C);

    /* In place of net_hal_rtd_config(): the virtual backend is the HAL. */
    if (net_virtual_init(&backend, &hal, links, PORT_EPOCH, 1000u, wire_output, NULL, NULL) != NET_OK) return 3;
    hal_cfg = net_virtual_hal_config(&backend);
    if (net_s32k344_upper_port_init(&hal, &hal_cfg) != NET_OK) return 4;
    if (net_ecu_adapter_init(&ecu, &service, p->node, p->callbacks(), NULL) != NET_OK) return 5;

    memset(&c, 0, sizeof(c));
    c.node = p->node;
    c.local_epoch = node_config.local_epoch;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = PORT_EPOCH;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port = net_hal_adapter_port(&hal);
    c.owner = net_ecu_adapter_owner(&ecu);
    /* Common receive default of the node root: every route bound, 2000 ms. */
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        c.rx_policy[i].bound = true;
        c.rx_policy[i].maximum_age_ms = 2000u;
        c.rx_policy[i].expires_at_equality = true;
    }
    p->upper(&c);
    if (net_upper_init(&service, &c) != NET_OK) return 6;

    memset(&pub, 0, sizeof(pub));
    pub.can_boot_id = node_config.boot_id;
    pub.entries = p->publish;
    pub.entry_count = p->publish_count;
    if (net_upper_publish_init(&publisher, &service, &pub) != NET_OK) return 7;

    parts.service = &service;
    parts.ecu = &ecu;
    parts.publisher = &publisher;
    parts.config = &node_config;
    if (p->bind(&parts) != NET_OK) return 8;

    memset(&perm, 0, sizeof(perm));
    perm.receive_timeout.bound = true;
    perm.receive_timeout.maximum_age_ms = 150u;
    perm.receive_timeout.expires_at_equality = true;
    p->permission(&perm);
    if (net_permission_init(&permission, &service, &perm) != NET_OK) return 9;
    /* In place of net_hal_rtd_start(): the backend links are connected at init. */
    ready = true;
    printf("{\"event\":\"ready\",\"node\":\"%s\",\"profile\":%u,\"boot\":%u,\"uart_c\":%s,\"publish_entries\":%u}\n",
           NODE_NAME, (unsigned)NETWORK_UPPER_NODE_PROFILE, (unsigned)NODE_BOOT, p->uart_c ? "true" : "false",
           (unsigned)p->publish_count);
    return 0;
}

/* NetNode_Step() order without the RTD drain / service calls: the virtual
 * backend hands its captures to the adapter when the frame arrives. */
static void node_step(void)
{
    if (!ready) return;
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET);
    (void)net_permission_step(&permission);
    if (net_node_profile.service != NULL) net_node_profile.service();
    (void)net_upper_publish_service(&publisher);
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET);
}

static NetStatus observe(void)
{
    NetUpperObservation o;
    NetHalObservation h;
    NetNodeDiag d;
    (void)net_upper_observation(&service, &o);
    (void)net_hal_adapter_observation(&hal, &h);
    memset(&d, 0, sizeof(d));
    net_node_profile.diag(&publisher, &d);
    printf("{\"event\":\"observation\",\"node\":\"%s\",\"now\":%llu,\"rx_delivered\":%u,\"rx_invalid\":%u,"
           "\"rx_held\":%u,\"duplicates\":%u,\"stale\":%u,\"consumer_rejected\":%u,\"tx_submitted\":%u,"
           "\"tx_succeeded\":%u,\"hal_rx_dropped\":%u}\n",
           NODE_NAME, (unsigned long long)backend.now_ms, o.rx_delivered, o.rx_invalid, o.rx_held, o.duplicates,
           o.stale, o.consumer_rejected, o.tx_submitted, o.tx_succeeded, h.rx_dropped);
    return NET_OK;
}

static NetStatus command(const char *line)
{
    unsigned a, b, c;
    uint64_t at, epoch;
    char at_text[32], epoch_text[32], hex[NET_WIRE_STREAM_INPUT_MAX * 2u + 1u];
    if (sscanf(line, "tick %31s", at_text) == 1) {
        NetStatus s;
        if (!vcli_number(at_text, &at)) return NET_INVALID_ARGUMENT;
        s = net_virtual_tick(&backend, at);
        if (s == NET_HOLD) return s;
        node_step();
        return NET_OK;
    }
    if (strcmp(line, "step") == 0) { node_step(); return NET_OK; }
    if (sscanf(line, "canrx %u %u %u %31s %31s %1024s", &a, &b, &c, epoch_text, at_text, hex) == 6) {
        NetWireCanEnvelope f;
        NetStatus s;
        memset(&f, 0, sizeof(f));
        f.length = vcli_from_hex(hex, f.bytes, sizeof(f.bytes));
        if (f.length == SIZE_MAX) return NET_INVALID_ARGUMENT;
        if (!vcli_number(epoch_text, &epoch) || !vcli_number(at_text, &at)) return NET_INVALID_ARGUMENT;
        f.standard_id = a; f.dlc = (uint8_t)b;
        f.fdf = (c & 1u) != 0u; f.brs = (c & 2u) != 0u; f.extended = (c & 4u) != 0u; f.rtr = (c & 8u) != 0u;
        f.driver_error = (c & 16u) != 0u; f.esi = (c & 32u) != 0u;
        s = net_virtual_can(&backend, &f, epoch, at);
        if (s != NET_OK) return s;
        node_step();
        return NET_OK;
    }
    if (sscanf(line, "uartrx %u %31s %31s %1024s", &a, epoch_text, at_text, hex) == 4) {
        uint8_t bytes[NET_WIRE_STREAM_INPUT_MAX];
        uint64_t times[NET_WIRE_STREAM_INPUT_MAX];
        size_t n, i, consumed = 0u;
        NetStatus s;
        n = vcli_from_hex(hex, bytes, sizeof(bytes));
        if (n == SIZE_MAX) return NET_INVALID_ARGUMENT;
        if (!vcli_number(epoch_text, &epoch) || !vcli_number(at_text, &at)) return NET_INVALID_ARGUMENT;
        for (i = 0u; i < n; ++i) times[i] = at;
        s = net_virtual_uart(&backend, (NetWireLink)a, bytes, times, n, epoch, &consumed);
        node_step();
        return s;
    }
    if (strcmp(line, "observe") == 0) return observe();
    return app_command(line);
}

int main(void)
{
    char line[1400];
    int rc = initialize();
    if (rc != 0) {
        printf("{\"event\":\"init_failed\",\"node\":\"%s\",\"code\":%d}\n", NODE_NAME, rc);
        return rc;
    }
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t n = strlen(line);
        NetStatus status;
        while (n > 0u && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) line[--n] = 0;
        if (strcmp(line, "quit") == 0) return 0;
        status = command(line);
        printf("{\"event\":\"done\",\"node\":\"%s\",\"status\":\"%s\"}\n", NODE_NAME, vcli_status(status));
        fflush(stdout);
    }
    return ferror(stdin) ? 10 : 0;
}
