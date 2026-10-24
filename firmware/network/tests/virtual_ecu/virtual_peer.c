#if defined(__MINGW32__)
#define __USE_MINGW_ANSI_STDIO 1
#endif
/* HAL-less peer worker for the node workers (virtual_node.c), HOST profile.
 * Role DOMAIN: the CAN counterpart of VSS / CIS / BCM. It is the
 * product service with node DOMAIN and a test owner, not the DOMAIN profile
 * with its C2 adapter (that one needs the pinned C2 target header).
 * Role PI: the UART-C counterpart of CIS.
 * The product service encodes and decodes every frame; this file only fills
 * raw DTO fields, prints what the service delivered and retires finished TX
 * attempts. Line protocol on stdin, JSON lines on stdout, "done" per command. */
#include "network/net_virtual.h"
#include "virtual_cli.h"

#if NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_HOST
#error "virtual_peer.c is the HOST profile peer"
#endif
#define PORT_EPOCH 3u
#define DOMAIN_BOOT 301u
#define PI_BOOT 6401u

static NetUpper service;
static NetHalAdapter hal;
static NetVirtualBackend backend;
static uint32_t node;
static const char *node_name;
/* Pi BOOT_ID of this run: PI_BOOT, or the decimal second argument of a
 * restarted Pi worker (a new Pi run). */
static uint32_t pi_boot = PI_BOOT;
static uint32_t next_tx = 1u;
static uint64_t next_observation = 1u;
static NetTxKey tracked[NET_UPPER_TX_CAPACITY];
static bool used[NET_UPPER_TX_CAPACITY];

static NetStatus wire_output(void *ctx, const NetUpperPacket *p, uint64_t at)
{
    (void)ctx;
    vcli_wire(node_name, p, at);
    return NET_OK;
}

static void remember(NetTxKey key)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i)
        if (!used[i]) { used[i] = true; tracked[i] = key; return; }
}

static void retire_finished(void)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_TX_CAPACITY; ++i) {
        NetUpperTxSnapshot snap;
        if (!used[i]) continue;
        if (net_upper_tx_snapshot(&service, tracked[i], &snap) == NET_OK && snap.reference_released &&
            snap.admission_known && (!snap.accepted || snap.outcome != NET_TX_UNKNOWN) &&
            net_upper_retire(&service, tracked[i]) == NET_OK)
            used[i] = false;
    }
}

static NetStatus step(void)
{
    NetStatus s = net_upper_step(&service, NET_UPPER_POLL_BUDGET);
    retire_finished();
    return s;
}

static NetSourceStamp stamp(uint16_t age)
{
    NetSourceStamp s;
    s.observation_id = next_observation++;
    s.observed_at_ms = backend.now_ms;
    s.original_age_ms = age;
    return s;
}

static NetStatus send_can(const NetUpperCanMessage *m, uint16_t age, bool inspection)
{
    NetSourceStamp source = stamp(age);
    NetAgePolicy policy = {true, 10000u, true};
    NetTxKey key;
    NetStatus s = inspection ? net_upper_submit_inspection_can(&service, m, &source, &policy, &key)
                             : net_upper_submit_can(&service, m, &source, &policy, &key);
    if (s == NET_OK) remember(key);
    printf("{\"event\":\"peer_send\",\"node\":\"%s\",\"id\":%u,\"status\":\"%s\"}\n", node_name, (unsigned)m->id,
           vcli_status(s));
    return s;
}

static NetStatus owner_can(void *ctx, const NetUpperCanMessage *m, const NetUpperDelivery *d)
{
    (void)ctx;
    printf("{\"event\":\"peer_rx\",\"node\":\"%s\",\"id\":%u,\"age\":%llu,\"source_boot\":%u,\"source_tx\":%u",
           node_name, (unsigned)m->id, (unsigned long long)d->age_ms, (unsigned)d->source_boot,
           (unsigned)d->source_sequence);
    switch (m->id) {
    case 0x340u:
        printf(",\"update\":%u,\"state\":%u,\"accepting\":%u", (unsigned)m->data.vss_status.update_sequence,
               (unsigned)m->data.vss_status.vss_state, (unsigned)m->data.vss_status.vss_accepting_events);
        break;
    case 0x0a0u:
        printf(",\"update\":%u,\"distance\":%u", (unsigned)m->data.cis_rear.update_sequence,
               (unsigned)m->data.cis_rear.rear_distance_deci_cm);
        break;
    case 0x310u:
        printf(",\"pi_boot\":%u,\"vision\":%u,\"count\":%u", (unsigned)m->data.cis_occupant.pi_boot_id,
               (unsigned)m->data.cis_occupant.vision_sequence, (unsigned)m->data.cis_occupant.occupant_count);
        break;
    case 0x140u:
        printf(",\"command_boot\":%u,\"command\":%u,\"result\":%u,\"reason\":%u",
               (unsigned)m->data.bcm_result.command_domain_boot, (unsigned)m->data.bcm_result.command_id,
               (unsigned)m->data.bcm_result.result, (unsigned)m->data.bcm_result.result_reason);
        break;
    case 0x200u:
        printf(",\"update\":%u,\"lock\":%u", (unsigned)m->data.bcm_door_state.update_sequence,
               (unsigned)m->data.bcm_door_state.door_lock_state);
        break;
    default:
        break;
    }
    puts("}");
    return NET_OK;
}

static NetStatus owner_uart(void *ctx, const NetUpperUartDelivery *u, const NetUpperDelivery *d)
{
    const NetUpperUartMessage *m = &u->message;
    (void)ctx;
    printf("{\"event\":\"peer_rx\",\"node\":\"%s\",\"type\":%u,\"link_sequence\":%u,\"age\":%llu", node_name,
           (unsigned)m->type, (unsigned)m->sequence, (unsigned long long)d->age_ms);
    if (m->type == 0x62u)
        printf(",\"cis_boot\":%u,\"update\":%u,\"power\":%u,\"use_limit\":%u",
               (unsigned)m->data.p_permission.cis_boot_id, (unsigned)m->data.p_permission.update_sequence,
               (unsigned)m->data.p_permission.vehicle_power_permission,
               (unsigned)m->data.p_permission.use_limit_ms);
    puts("}");
    return NET_OK;
}

static NetStatus held_can(void *ctx, const NetUpperCanMessage *m, const NetUpperDelivery *d)
{
    (void)ctx;
    printf("{\"event\":\"peer_held\",\"node\":\"%s\",\"id\":%u,\"observation_only\":%s}\n", node_name,
           (unsigned)m->id, d->observation_only ? "true" : "false");
    return d->observation_only ? NET_OK : NET_REJECTED;
}

static NetStatus domain_command(const char *line)
{
    unsigned a = 0u, b = 0u, c = 0u;
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    if (sscanf(line, "permission %u %u", &a, &b) == 2) {
        NetWireDomainPermissionRaw *p = &m.data.domain_permission;
        if (a > 1u) return NET_INVALID_ARGUMENT;
        m.id = 0x080u;
        p->boot_id = DOMAIN_BOOT; p->tx_sequence = next_tx++;
        p->vehicle_power_permission = (uint8_t)a; p->power_permission_quality = 0u;
        p->power_operation_state = 1u; p->operation_allowed = 1u; p->operation_quality = 0u;
        p->update_sequence = b; p->source_age_ms = 5u;
        return send_can(&m, p->source_age_ms, false);
    }
    if (sscanf(line, "vss_event %u %u", &a, &b) == 2) {
        NetWireVssEventRaw *e = &m.data.vss_event;
        if (b == 0u || b > 5u) return NET_INVALID_ARGUMENT;
        m.id = 0x160u;
        e->boot_id = DOMAIN_BOOT; e->tx_sequence = next_tx++;
        e->event_kind = (uint8_t)b; e->occurrence_id = a; e->original_age_ms = 4u; e->use_limit_ms = 500u;
        e->value_quality = 0u; e->input_availability = 0u; e->related_command_id = 0u;
        return send_can(&m, e->original_age_ms, false);
    }
    if (sscanf(line, "door_command %u %u %u", &a, &b, &c) == 3) {
        NetWireBcmDoorCommandRaw *d = &m.data.bcm_door_command;
        if (c > 1u) return NET_INVALID_ARGUMENT;
        m.id = 0x100u;
        d->boot_id = DOMAIN_BOOT; d->tx_sequence = next_tx++;
        d->target_boot_id = b; d->command_id = a; d->command_age_ms = 3u; d->accept_limit_ms = 1000u;
        d->door_lock_target = (uint8_t)c; d->origin = 1u;
        return send_can(&m, d->command_age_ms, false);
    }
    return NET_UNSUPPORTED;
}

static NetStatus pi_command(const char *line)
{
    unsigned a = 0u, b = 0u;
    if (sscanf(line, "vision %u %u", &a, &b) == 2) {
        NetUpperUartMessage m;
        NetWirePVisionResult *v = &m.data.p_vision_result;
        NetSourceStamp source;
        NetAgePolicy policy = {true, 10000u, true};
        NetTxKey key;
        NetStatus s;
        if (b > 5u) return NET_INVALID_ARGUMENT;
        memset(&m, 0, sizeof(m));
        m.link = NET_WIRE_UART_C; m.type = 0x60u;
        v->pi_boot_id = pi_boot; v->vision_sequence = a; v->vision_age_ms = 6u;
        v->occupant_presence = b != 0u ? 1u : 0u; v->presence_validity = 1u; v->presence_reason = 0u;
        v->occupant_count = (uint8_t)b; v->count_validity = 1u; v->count_reason = 0u;
        v->vision_status = 2u; v->vision_fault_code = 0u;
        source = stamp(v->vision_age_ms);
        s = net_upper_submit_uart_auto(&service, &m, &source, &policy, &key);
        if (s == NET_OK) remember(key);
        printf("{\"event\":\"peer_send\",\"node\":\"%s\",\"type\":96,\"status\":\"%s\"}\n", node_name,
               vcli_status(s));
        return s;
    }
    return NET_UNSUPPORTED;
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
        (void)step();
        return NET_OK;
    }
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
        return step();
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
        (void)step();
        return s;
    }
    return node == NET_WIRE_NODE_DOMAIN ? domain_command(line) : pi_command(line);
}

static int initialize(const char *name)
{
    NetUpperConfig c;
    NetHalConfig h;
    uint32_t links = NET_HAL_LINK_BIT(NET_WIRE_CAN);
    size_t i;
    if (name != NULL && strcmp(name, "DOMAIN") == 0) { node = NET_WIRE_NODE_DOMAIN; node_name = "DOMAIN"; }
    else if (name != NULL && strcmp(name, "PI") == 0) {
        node = NET_WIRE_NODE_PI; node_name = "PI"; links = NET_HAL_LINK_BIT(NET_WIRE_UART_C);
    } else return 2;
    if (net_virtual_init(&backend, &hal, links, PORT_EPOCH, 1000u, wire_output, NULL, NULL) != NET_OK) return 3;
    h = net_virtual_hal_config(&backend);
    if (net_hal_adapter_init(&hal, &h, NET_HAL_S32K344) != NET_OK) return 4;
    memset(&c, 0, sizeof(c));
    c.node = node;
    c.local_epoch = 10u + node;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port = net_hal_adapter_port(&hal);
    c.owner.can = owner_can;
    c.owner.uart = owner_uart;
    c.owner.held_can = held_can;
    c.inspection_transport = node == NET_WIRE_NODE_DOMAIN;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = PORT_EPOCH;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        c.rx_policy[i].bound = true;
        c.rx_policy[i].maximum_age_ms = 10000u;
        c.rx_policy[i].expires_at_equality = true;
    }
    for (i = 0u; i < 2u; ++i) {
        c.uart_policy[i].candidate_time_policy_selected = true;
        c.uart_policy[i].assembly_limit_ms = 100u;
        c.uart_policy[i].expires_at_equality = true;
        c.uart_sequence_bound[i] = true;
        c.uart_initial_sequence[i] = 1u;
    }
    if (net_upper_init(&service, &c) != NET_OK) return 6;
    printf("{\"event\":\"ready\",\"node\":\"%s\",\"profile\":%u}\n", node_name, (unsigned)NETWORK_UPPER_NODE_PROFILE);
    return 0;
}

int main(int argc, char **argv)
{
    char line[1400];
    int rc;
    if (argc > 2) {
        uint64_t boot;
        if (!vcli_number(argv[2], &boot) || boot == 0u || boot > UINT32_MAX) return 2;
        pi_boot = (uint32_t)boot;
    }
    rc = initialize(argc > 1 ? argv[1] : NULL);
    if (rc != 0) {
        printf("{\"event\":\"init_failed\",\"code\":%d}\n", rc);
        return rc;
    }
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t n = strlen(line);
        NetStatus status;
        while (n > 0u && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) line[--n] = 0;
        if (strcmp(line, "quit") == 0) return 0;
        status = command(line);
        printf("{\"event\":\"done\",\"node\":\"%s\",\"status\":\"%s\"}\n", node_name, vcli_status(status));
        fflush(stdout);
    }
    return ferror(stdin) ? 10 : 0;
}
