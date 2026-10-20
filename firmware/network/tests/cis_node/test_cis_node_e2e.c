/* CIS node end to end on the host: the real product sources (CIS App Port
 * facade -> typed ECU adapter -> NetUpper -> M04 publish slots ->
 * NetHalAdapter -> RTD backend, FlexCAN and LPUART parts) linked with the
 * independently written mock SDK, which plays the shared CAN bus (DOMAIN) and
 * the UART-C line (Pi, LPUART11).
 * Contract: docs/network/interface/02_NODES/CIS/{10_DATA,20_API}.md,
 *           02_NODES/PI/10_DATA.md, 03_COMMUNICATION/10_CAN_FD.md, 30_UART_C.md.
 * Frames are built and checked with the repository codec (net_wire_can_*,
 * net_upper_uart_*). One case per process: NetNode_Init succeeds once.
 * Builds of this file:
 *   OsIf system counter, NetNode_Init / NetNode_Step
 *   CIS_NODE_TEST_ENTRY: application tick hooks, Net_Init / Net_MainFunction */
#include "network/net_node.h"
#include "network/app/net_if_cis.h"
#include "network/net_wire_can.h"
#include "network/net_wire_can_nodes.h"
#include "network/net_upper_uart.h"
#include "mock_rtd.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define RUN(x) do { if ((x) != 0) return 1; } while (0)

static const char *case_name;

enum { NODE_BOOT = 0x43495331u, DOMAIN_BOOT = 0x444F4D31u, PI_BOOT = 0x50490001u, BUS = 0, LPUART = 11 };

#define PASS_US 1000u   /* main pass period */
#define BYTE_US 87u     /* 115200 bit/s 8N1 */

/* ------------------------------------------------------------- time ----- */
static uint64_t now_ms(void) { return MockRtd_NowUs() / 1000u; }

#ifdef CIS_NODE_TEST_ENTRY
/* Application clock of the board: 1 ms tick, identity conversion. */
uint32_t MockBoard_IsrTicks(void);
uint32_t MockBoard_TickToMs(uint32_t tick);
bool MockBoard_NodeConfig(NetNodeConfig *out);
uint32_t MockBoard_Us(void);
uint32_t MockBoard_IsrTicks(void) { return (uint32_t)(MockRtd_NowUs() / 1000u); }
uint32_t MockBoard_TickToMs(uint32_t tick) { return tick; }
bool MockBoard_NodeConfig(NetNodeConfig *out) { out->boot_id = NODE_BOOT; return true; }
uint32_t MockBoard_Us(void) { return (uint32_t)MockRtd_NowUs(); }
#define TIMER_ON false
#else
#define TIMER_ON true
#endif

/* ------------------------------------------------------- App callbacks --- */
static unsigned perm_count, result_count, status_count;
static NetDomainPermission last_perm;
static NetCanRxMeta last_perm_meta;
static uint64_t perm_delivered_at;        /* main pass time of the last permission callback */
static NetVisionResult last_result;
static NetUartRxMeta last_result_meta;
static NetVisionStatus last_status;
static NetUartRxMeta last_status_meta;
static bool in_isr;
static unsigned isr_callbacks;
/* Publish from inside callbacks. */
static int occupant_in_result;            /* CisNet_PublishOccupant calls per result callback */
static NetSubmitResult occupant_results[2];
static bool status_in_permission;
static NetSubmitResult status_result_in_permission;

static NetCisOccupant relay_of(const NetVisionResult *v, uint16_t extra_age)
{
    NetCisOccupant o;
    memset(&o, 0, sizeof(o));
    o.pi_boot_id = v->pi_boot_id;
    o.vision_sequence = v->vision_sequence;
    o.vision_age_ms = (uint16_t)(v->vision_age_ms + extra_age);
    o.occupant_presence = v->occupant_presence;
    o.presence_validity = v->presence_validity;
    o.presence_reason = v->presence_reason;
    o.occupant_count = v->occupant_count;
    o.count_validity = v->count_validity;
    o.count_reason = v->count_reason;
    o.vision_function_status = v->vision_status;
    return o;
}

static NetCisStatus status_ready(uint32_t update, uint16_t age);

void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)
{
    if (in_isr) ++isr_callbacks;
    ++perm_count;
    last_perm = *message;
    last_perm_meta = *meta;
    perm_delivered_at = now_ms();
    if (status_in_permission) {
        NetCisStatus s = status_ready(77u, 3u);
        status_result_in_permission = CisNet_PublishStatus(&s);
    }
}

void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta)
{
    int i;
    if (in_isr) ++isr_callbacks;
    ++result_count;
    last_result = *message;
    last_result_meta = *meta;
    for (i = 0; i < occupant_in_result && i < 2; ++i) {
        NetCisOccupant o = relay_of(message, (uint16_t)(5 * i));
        occupant_results[i] = CisNet_PublishOccupant(&o);
    }
}

void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta)
{
    if (in_isr) ++isr_callbacks;
    ++status_count;
    last_status = *message;
    last_status_meta = *meta;
}

/* ------------------------------------------------------ bus and line ---- */
typedef struct { uint64_t at; MockCanFrame f; } CanOut;
typedef struct { uint64_t at; NetUpperUartMessage m; } UartOut;
static CanOut can_out[512];
static unsigned can_n;
static UartOut uart_out[256];
static unsigned uart_n, uart_bad;

/* The far ends take what the node sent: CAN frames (TX_COMPLETE) and the
 * bytes of the active UART transfer (END_TRANSFER). */
static void collect(void)
{
    MockCanFrame f;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    uint32 n;
    in_isr = true;
    while (MockRtd_CanTransmitNext(BUS, &f) >= 0) {
        if (can_n < sizeof(can_out) / sizeof(can_out[0])) { can_out[can_n].at = now_ms(); can_out[can_n].f = f; }
        ++can_n;
    }
    n = MockRtd_UartTransmitTake(LPUART, bytes, sizeof(bytes));
    in_isr = false;
    if (n > 0u) {
        const NetWireRegistryEntry *r = net_wire_registry_find(NET_WIRE_UART_C, 0x62u);
        NetWireUartFrame w;
        NetUpperUartMessage m;
        memset(&w, 0, sizeof(w));
        w.length = n;
        memcpy(w.bytes, bytes, n);
        if (r == NULL || net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, r->direction, &w, &m) != NET_WIRE_OK) {
            ++uart_bad;
            return;
        }
        if (uart_n < sizeof(uart_out) / sizeof(uart_out[0])) { uart_out[uart_n].at = now_ms(); uart_out[uart_n].m = m; }
        ++uart_n;
    }
}

static uint64_t next_pass_us = PASS_US;

static void pass(void)
{
#ifdef CIS_NODE_TEST_ENTRY
    Net_MainFunction((uint32_t)now_ms());
#else
    NetNode_Step();
#endif
    collect();
}

/* Superloop: one main pass at every 1 ms boundary while time advances. */
static void advance_us(uint64_t us)
{
    uint64_t end = MockRtd_NowUs() + us;
    while (next_pass_us <= end) {
        MockRtd_AdvanceUs(next_pass_us - MockRtd_NowUs());
        pass();
        next_pass_us += PASS_US;
    }
    MockRtd_AdvanceUs(end - MockRtd_NowUs());
}

static void step_ms(unsigned ms) { advance_us((uint64_t)ms * 1000u); }

static NetNodeDiag diag(void)
{
    NetNodeDiag d;
    (void)NetNode_GetDiag(&d);
    return d;
}

static void reset_bench(void)
{
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(TIMER_ON);
    next_pass_us = PASS_US;
}

static NetStatus start_node(void)
{
#ifdef CIS_NODE_TEST_ENTRY
    reset_bench();
    Net_Init();
    return diag().stage == NET_NODE_STAGE_READY ? NET_OK : NET_NOT_CONFIGURED;
#else
    NetNodeConfig c;
    reset_bench();
    memset(&c, 0, sizeof(c));
    c.boot_id = NODE_BOOT;
    c.local_epoch = 1u;
    c.can_port_epoch = 1u;
    c.uart_c_port_epoch = 1u;
    return NetNode_Init(&c);
#endif
}

/* DOMAIN_PERMISSION on the bus (ISR context of the CAN controller). */
static int permission_in(uint32_t tx, uint32_t update, uint16_t age, uint8_t value, uint8_t quality)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope e;
    MockCanFrame f;
    int mb;
    memset(&p, 0, sizeof(p));
    p.boot_id = DOMAIN_BOOT;
    p.tx_sequence = tx;
    p.vehicle_power_permission = value;
    p.power_permission_quality = quality;
    p.power_operation_state = 1u;
    p.operation_allowed = 1u;
    p.operation_quality = 0u;
    p.update_sequence = update;
    p.source_age_ms = age;
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, &e) == NET_WIRE_OK);
    memset(&f, 0, sizeof(f));
    f.id = e.standard_id; f.fd = e.fdf; f.brs = e.brs; f.dlc = e.dlc; f.length = (uint8)e.length;
    memcpy(f.data, e.bytes, e.length);
    in_isr = true;
    mb = MockRtd_CanInject(BUS, &f);
    in_isr = false;
    CHECK(mb >= 0);
    return 0;
}

static NetUpperUartMessage vision_message(uint16_t link, uint32_t vision_sequence, uint16_t age)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x60u;
    m.sequence = link;
    m.data.p_vision_result.pi_boot_id = PI_BOOT;
    m.data.p_vision_result.vision_sequence = vision_sequence;
    m.data.p_vision_result.vision_age_ms = age;
    m.data.p_vision_result.occupant_presence = NET_TRISTATE_YES;
    m.data.p_vision_result.presence_validity = NET_VALIDITY_VALID;
    m.data.p_vision_result.presence_reason = NET_QUALITY_REASON_NONE;
    m.data.p_vision_result.occupant_count = 2u;
    m.data.p_vision_result.count_validity = NET_VALIDITY_VALID;
    m.data.p_vision_result.count_reason = NET_QUALITY_REASON_NONE;
    m.data.p_vision_result.vision_status = NET_SENSOR_FUNCTION_ACTIVE;
    m.data.p_vision_result.vision_fault_code = NET_VISION_FAULT_NONE;
    return m;
}

static NetUpperUartMessage vision_status_message(uint16_t link, uint32_t status_sequence)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_C;
    m.type = 0x61u;
    m.sequence = link;
    m.data.p_vision_status.pi_boot_id = PI_BOOT;
    m.data.p_vision_status.status_sequence = status_sequence;
    m.data.p_vision_status.vision_status = NET_SENSOR_FUNCTION_FAULT;
    m.data.p_vision_status.active_fault = NET_VISION_FAULT_CAMERA;
    m.data.p_vision_status.last_fault = NET_VISION_FAULT_INFERENCE;
    m.data.p_vision_status.status_age_ms = 40u;
    m.data.p_vision_status.affected_function = 0x03u;
    return m;
}

/* One UART-C frame from the Pi at the line rate while the main loop runs.
 * Returns 0 and the ms time of its last byte (its ISR capture). */
static int line_send(const NetUpperUartMessage *m, uint64_t *last_byte_ms)
{
    NetWireUartFrame f;
    size_t i;
    memset(&f, 0, sizeof(f));
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY, m, &f) == NET_WIRE_OK);
    for (i = 0u; i < f.length; ++i) {
        advance_us(BYTE_US);
        in_isr = true;
        CHECK(MockRtd_UartInject(LPUART, f.bytes + i, 1u) == 1u);
        in_isr = false;
    }
    *last_byte_ms = now_ms();
    return 0;
}

static NetWireCanEnvelope envelope(const MockCanFrame *f)
{
    NetWireCanEnvelope e;
    memset(&e, 0, sizeof(e));
    e.standard_id = (uint16_t)f->id;
    e.fdf = f->fd;
    e.brs = f->brs;
    e.dlc = f->dlc;
    e.length = f->length;
    memcpy(e.bytes, f->data, f->length);
    return e;
}

static unsigned can_count(uint32_t id, unsigned from)
{
    unsigned i, n = 0u;
    for (i = from; i < can_n && i < sizeof(can_out) / sizeof(can_out[0]); ++i)
        if (can_out[i].f.id == id) ++n;
    return n;
}

/* Index of the k-th (0-based) frame with this id from index from, or -1. */
static int can_find(uint32_t id, unsigned from, unsigned k)
{
    unsigned i;
    for (i = from; i < can_n && i < sizeof(can_out) / sizeof(can_out[0]); ++i)
        if (can_out[i].f.id == id && k-- == 0u) return (int)i;
    return -1;
}

static NetCisRear rear_sample(uint32_t update, uint16_t age)
{
    NetCisRear r;
    memset(&r, 0, sizeof(r));
    r.update_sequence = update;
    r.source_age_ms = age;
    r.rear_distance_deci_cm = 1234u;
    r.validity = NET_VALIDITY_VALID;
    r.quality_reason = NET_QUALITY_REASON_NONE;
    r.proximity_status = NET_PROXIMITY_VALID_DISTANCE;
    return r;
}

static NetCisEnvironment environment_sample(uint32_t sequence, uint16_t age)
{
    NetCisEnvironment v;
    memset(&v, 0, sizeof(v));
    v.temperature_centi_c = 2350;
    v.temperature_sequence = sequence;
    v.temperature_age_ms = age;
    v.temperature_validity = NET_VALIDITY_VALID;
    v.temperature_reason = NET_QUALITY_REASON_NONE;
    v.humidity_centi_percent = 4500u;
    v.humidity_sequence = sequence + 100u;
    v.humidity_age_ms = (uint16_t)(age + 10u);
    v.humidity_validity = NET_VALIDITY_VALID;
    v.humidity_reason = NET_QUALITY_REASON_NONE;
    v.illuminance_lux = NET_INVALID_ILLUMINANCE;
    v.illuminance_sequence = sequence + 200u;
    v.illuminance_age_ms = (uint16_t)(age + 20u);
    v.illuminance_validity = NET_VALIDITY_INVALID;
    v.illuminance_reason = NET_QUALITY_REASON_NO_DATA;
    return v;
}

static NetCisStatus status_ready(uint32_t update, uint16_t age)
{
    NetCisStatus s;
    memset(&s, 0, sizeof(s));
    s.cis_state = NET_CIS_STATE_ACTIVE;
    s.vision_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.temperature_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.humidity_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.illuminance_status = NET_SENSOR_FUNCTION_FAULT;
    s.rear_status = NET_SENSOR_FUNCTION_READY;
    s.pi_uart_status = NET_INTERFACE_READY;
    s.can_interface_status = NET_INTERFACE_READY;
    s.fault_mask = NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT;
    s.active_fault = NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT;
    s.recovering = NET_FLAG_NO;
    s.last_fault = NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT;
    s.last_fault_occurrence = 3u;
    s.affected_function = 0x08u;
    s.update_sequence = update;
    s.source_age_ms = age;
    return s;
}

/* A relayed P_PERMISSION as the Pi receives it. */
static int check_relay(const UartOut *u, uint16_t link, uint32_t update, uint8_t value,
                       uint16_t domain_age, uint64_t received_at)
{
    const NetWirePPermission *p = &u->m.data.p_permission;
    CHECK(u->m.type == 0x62u && u->m.sequence == link);
    CHECK(p->cis_boot_id == NODE_BOOT);                      /* CIS relay generation */
    CHECK(p->update_sequence == update);                     /* DOMAIN evaluation kept */
    CHECK(p->vehicle_power_permission == value);
    CHECK(p->value_quality == NET_QUALITY_OK && p->quality_reason == NET_QUALITY_REASON_NONE);
    CHECK(p->use_limit_ms == 1500u);
    /* DOMAIN AGE + local wait from the CAN capture to the UART dispatch, once. */
    CHECK(p->source_age_ms == domain_age + (u->at - received_at));
    return 0;
}

/* -------------------------------------------------------------- cases ---- */
static int f01_init(void)
{
    NetNodeDiag d;
#ifndef CIS_NODE_TEST_ENTRY
    NetNodeConfig c;
    reset_bench();
    memset(&c, 0, sizeof(c));
    c.boot_id = NODE_BOOT;
    c.local_epoch = 1u;
    c.can_port_epoch = 1u;
    /* UART-C is a link of this profile: its epoch is required. */
    CHECK(NetNode_Init(&c) == NET_INVALID_ARGUMENT);
    c.uart_c_port_epoch = 1u;
    c.uart_m_port_epoch = 0u;                 /* UART-M is not a CIS link: ignored */
    CHECK(NetNode_Init(&c) == NET_OK);
    CHECK(NetNode_Init(&c) == NET_BUSY);
#else
    CHECK(start_node() == NET_OK);
#endif
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY && d.stage_status == NET_OK);
    CHECK(d.rtd.started && d.rtd.init_failed_step == NET_HAL_RTD_STEP_NONE);
    CHECK(d.rtd.uart[1].uart_present && d.rtd.uart[1].uart_rx_armed && !d.rtd.uart[0].uart_present);
    CHECK(MockRtd_UartInits(LPUART) == 1u && MockRtd_UartRxArmed(LPUART));
    CHECK(d.app.bound);
    /* M04 table: four application messages without a period, the relay at 500 ms. */
    CHECK(d.rear_tx.boot_id == NODE_BOOT && d.rear_tx.next_sequence == 1u && d.rear_tx.period_ms == 0u);
    CHECK(d.environment_tx.period_ms == 0u && d.occupant_tx.period_ms == 0u && d.status_tx.period_ms == 0u);
    CHECK(d.pi_permission_tx.period_ms == 500u && d.pi_permission_tx.next_sequence == 1u);
    CHECK(!d.rear_tx.has_source && !d.pi_permission_tx.has_source);
    step_ms(3u);
    CHECK(diag().steps == 3u);
    CHECK(can_n == 0u && uart_n == 0u);       /* nothing before a first submission */
    return 0;
}

static int f02_permission_rx_relay(void)
{
    NetNodeDiag d, d0;
    uint64_t injected_at;
    CHECK(start_node() == NET_OK);
    step_ms(5u);
    MockRtd_AdvanceUs(300u);
    injected_at = now_ms();
    RUN(permission_in(1u, 41u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    CHECK(perm_count == 0u && uart_n == 0u);  /* never from the ISR */
    step_ms(1u);
    CHECK(perm_count == 1u && isr_callbacks == 0u);
    CHECK(last_perm.vehicle_power_permission == NET_TRISTATE_YES);
    CHECK(last_perm.power_permission_quality == NET_QUALITY_OK);
    CHECK(last_perm.power_operation_state == 1u && last_perm.operation_allowed == NET_FLAG_YES);
    CHECK(last_perm.operation_quality == NET_QUALITY_OK);
    CHECK(last_perm.update_sequence == 41u && last_perm.source_age_ms == 10u); /* RX basis */
    CHECK(last_perm_meta.source.boot_id == DOMAIN_BOOT && last_perm_meta.source.tx_sequence == 1u);
    CHECK(last_perm_meta.received_at_ms == injected_at);
    /* Same verified input relayed to the Pi in the same pass. */
    CHECK(uart_n == 1u && uart_bad == 0u);
    CHECK(uart_out[0].at == perm_delivered_at);
    RUN(check_relay(&uart_out[0], 1u, 41u, NET_TRISTATE_YES, 10u, last_perm_meta.received_at_ms));
    d = diag();
    CHECK(d.app.permission_delivered == 1u && d.app.relay_submitted == 1u && d.app.relay_refused == 0u);
    CHECK(d.app.last_relay_result == NET_SUBMIT_OK);
    CHECK(d.pi_permission_tx.counters.sent == 1u && d.pi_permission_tx.next_sequence == 2u);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    /* Exact duplicate (same TX, same bytes): no callback, no relay. */
    d0 = d;
    RUN(permission_in(1u, 41u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(3u);
    d = diag();
    CHECK(perm_count == 1u && uart_n == 1u && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* Newer DOMAIN evaluation: delivered and relayed with the next LINK_SEQUENCE. */
    RUN(permission_in(2u, 42u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm.update_sequence == 42u && uart_n == 2u);
    RUN(check_relay(&uart_out[1], 2u, 42u, NET_TRISTATE_YES, 5u, last_perm_meta.received_at_ms));
    /* Older TX: neither delivered nor relayed. */
    d0 = diag();
    RUN(permission_in(1u, 43u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(3u);
    d = diag();
    CHECK(perm_count == 2u && uart_n == 2u && d.upper.stale == d0.upper.stale + 1u);
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* DOMAIN as 10_DATA.md describes it: CAN every 50 ms, a new evaluation about
 * every 200 ms. Each new evaluation is relayed at once; the CAN repetitions of
 * an evaluation add no frame and the 500 ms repetition never comes due. */
static int f03_permission_new_evaluations(void)
{
    NetNodeDiag d;
    uint32_t tx = 0u, update = 100u;
    uint16_t age = 0u;
    unsigned t, i;
    uint64_t first_capture[16];
    unsigned evaluations = 0u;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    for (t = 0u; t < 2000u; t += 50u) {
        if (t % 200u == 0u) { ++update; age = 4u; first_capture[evaluations++ & 15u] = now_ms(); }
        else age = (uint16_t)(age + 50u);
        RUN(permission_in(++tx, update, age, NET_TRISTATE_YES, NET_QUALITY_OK));
        step_ms(50u);
    }
    CHECK(evaluations == 10u);
    CHECK(uart_n == evaluations && uart_bad == 0u);
    for (i = 0u; i < uart_n; ++i) {
        CHECK(uart_out[i].m.sequence == i + 1u);                         /* LINK_SEQUENCE +1 */
        CHECK(uart_out[i].m.data.p_permission.update_sequence == 101u + i);
        CHECK(uart_out[i].at - first_capture[i] <= 1u);                  /* at once */
        CHECK(uart_out[i].m.data.p_permission.source_age_ms == 4u + (uart_out[i].at - first_capture[i]));
    }
    d = diag();
    CHECK(d.pi_permission_tx.counters.sent == evaluations);
    CHECK(d.pi_permission_tx.counters.periodic_sent == 0u && d.pi_permission_tx.counters.not_sent == 0u);
    CHECK(d.app.relay_submitted == evaluations && d.app.relay_refused == 0u);
    return 0;
}

/* The same DOMAIN evaluation keeps arriving (CAN repetitions, growing AGE):
 * one relay frame; the 500 ms repetitions come due and the service refuses
 * them (the original Power life of 500 ms has passed and the relay no longer
 * matches the DOMAIN receipt), so nothing more reaches the Pi. */
static int f04_permission_same_evaluation(void)
{
    NetNodeDiag d;
    uint32_t tx = 0u;
    uint64_t first, offered_at, t;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    first = now_ms();
    RUN(permission_in(++tx, 7u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 1u);
    offered_at = uart_out[0].at;
    while (now_ms() < first + 1700u) {
        step_ms(1u);
        t = now_ms();
        d = diag();
        if (t == offered_at + 499u) CHECK(d.pi_permission_tx.counters.not_sent == 0u);
        if (t == offered_at + 500u) CHECK(d.pi_permission_tx.counters.not_sent == 1u); /* due, refused */
        /* DOMAIN repeats the same evaluation every 50 ms, its AGE growing. */
        if (t < first + 1200u && (t - first) % 50u == 0u)
            RUN(permission_in(++tx, 7u, (uint16_t)(10u + (t - first)), NET_TRISTATE_YES, NET_QUALITY_OK));
    }
    d = diag();
    CHECK(uart_n == 1u && uart_bad == 0u);
    CHECK(d.pi_permission_tx.counters.sent == 1u && d.pi_permission_tx.counters.periodic_sent == 0u);
    CHECK(d.pi_permission_tx.counters.not_sent == 3u);           /* +500, +1000, +1500 */
    CHECK(d.pi_permission_tx.next_sequence == 2u);               /* no LINK_SEQUENCE used */
    return 0;
}

static int f05_permission_rx_timeout(void)
{
    NetNodeDiag d;
    uint64_t at;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_in(1u, 9u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    at = last_perm_meta.received_at_ms;
    CHECK(uart_n == 1u);
    step_ms((unsigned)(at + 149u - now_ms()));
    d = diag();
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT);
    step_ms(2u);
    d = diag();
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_RX_TIMEOUT && !d.permission.power_usable);
    /* DOMAIN silent: the Pi gets nothing more (its 1500 ms receive timeout applies). */
    step_ms(2000u);
    CHECK(uart_n == 1u && perm_count == 1u);
    /* A new evaluation after the silence is relayed at once. */
    RUN(permission_in(2u, 10u, 3u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    d = diag();
    CHECK(uart_n == 2u && d.permission.reason == NET_PERMISSION_CURRENT);
    RUN(check_relay(&uart_out[1], 2u, 10u, NET_TRISTATE_YES, 3u, last_perm_meta.received_at_ms));
    return 0;
}

static int f06_permission_quality(void)
{
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_in(1u, 20u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 1u);
    /* A non-OK power quality reaches the application but is not relayed (the
     * wire has no reason for it), and the earlier ALLOW is never sent again. */
    step_ms(100u);
    RUN(permission_in(2u, 21u, 5u, NET_TRISTATE_YES, NET_QUALITY_INVALID));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm.power_permission_quality == NET_QUALITY_INVALID);
    d = diag();
    CHECK(d.app.relay_refused == 1u && d.app.last_relay_result == NET_SUBMIT_NOT_READY);
    CHECK(d.permission.reason == NET_PERMISSION_BAD_QUALITY && !d.permission.power_usable);
    step_ms(1500u);
    CHECK(uart_n == 1u);
    /* A confirmed DENY (OK quality) is relayed as it is. */
    RUN(permission_in(3u, 22u, 6u, NET_TRISTATE_NO, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 2u);
    RUN(check_relay(&uart_out[1], 2u, 22u, NET_TRISTATE_NO, 6u, last_perm_meta.received_at_ms));
    d = diag();
    CHECK(d.permission.reason == NET_PERMISSION_RESTRICTED);
    return 0;
}

static int f07_vision_rx(void)
{
    NetUpperUartMessage m;
    NetNodeDiag d0, d;
    uint64_t last;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    m = vision_message(1u, 500u, 20u);
    RUN(line_send(&m, &last));
    CHECK(result_count == 0u);                /* not before a main pass */
    step_ms(1u);
    CHECK(result_count == 1u && status_count == 0u && isr_callbacks == 0u);
    CHECK(last_result.pi_boot_id == PI_BOOT && last_result.vision_sequence == 500u);
    CHECK(last_result.vision_age_ms == 20u);  /* RX basis: not advanced to delivery */
    CHECK(last_result.occupant_presence == NET_TRISTATE_YES);
    CHECK(last_result.presence_validity == NET_VALIDITY_VALID);
    CHECK(last_result.presence_reason == NET_QUALITY_REASON_NONE);
    CHECK(last_result.occupant_count == 2u);
    CHECK(last_result.count_validity == NET_VALIDITY_VALID);
    CHECK(last_result.count_reason == NET_QUALITY_REASON_NONE);
    CHECK(last_result.vision_status == NET_SENSOR_FUNCTION_ACTIVE);
    CHECK(last_result.vision_fault_code == NET_VISION_FAULT_NONE);
    CHECK(last_result_meta.link_sequence == 1u);
    CHECK(last_result_meta.received_at_ms == last);  /* ISR time of the last byte */
    /* Exact duplicate (same LINK_SEQUENCE, same bytes): no callback. */
    d0 = diag();
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(result_count == 1u && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* Newer LINK_SEQUENCE, then an older one: only the newer is delivered. */
    m = vision_message(3u, 501u, 15u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 2u && last_result.vision_sequence == 501u && last_result_meta.link_sequence == 3u);
    d0 = diag();
    m = vision_message(2u, 502u, 15u);
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(result_count == 2u && d.upper.stale == d0.upper.stale + 1u);
    /* The same Pi judgement repeated with a newer LINK_SEQUENCE is delivered
     * (it feeds the application's receive supervision), its AGE grown. */
    m = vision_message(4u, 501u, 115u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 3u && last_result.vision_sequence == 501u && last_result.vision_age_ms == 115u);
    CHECK(last_result_meta.link_sequence == 4u);
    /* P_VISION_STATUS. */
    m = vision_status_message(5u, 31u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(status_count == 1u && result_count == 3u);
    CHECK(last_status.pi_boot_id == PI_BOOT && last_status.status_sequence == 31u);
    CHECK(last_status.vision_status == NET_SENSOR_FUNCTION_FAULT);
    CHECK(last_status.active_fault == NET_VISION_FAULT_CAMERA && last_status.last_fault == NET_VISION_FAULT_INFERENCE);
    CHECK(last_status.status_age_ms == 40u && last_status.affected_function == 0x03u);
    CHECK(last_status_meta.link_sequence == 5u && last_status_meta.received_at_ms == last);
    d = diag();
    CHECK(d.app.vision_result_delivered == 3u && d.app.vision_status_delivered == 1u);
    CHECK(can_n == 0u && uart_n == 0u && isr_callbacks == 0u);
    return 0;
}

static int f08_publish_frames(void)
{
    NetCisRear rear = rear_sample(11u, 7u);
    NetCisEnvironment env = environment_sample(21u, 30u);
    NetCisStatus st = status_ready(31u, 2u);
    NetCisOccupant occ;
    NetUpperUartMessage m;
    NetWireCanEnvelope e;
    NetWireCisRearRaw rr;
    NetWireCisEnvironmentRaw er;
    NetWireCisOccupantRaw orr;
    NetWireCisStatusRaw sr;
    NetNodeDiag d;
    uint64_t submitted, last, received;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    m = vision_message(1u, 600u, 25u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u);
    received = last_result_meta.received_at_ms;
    occ = relay_of(&last_result, 0u);
    MockRtd_AdvanceUs(400u);
    submitted = now_ms();
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    CHECK(CisNet_PublishEnvironment(&env) == NET_SUBMIT_OK);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    CHECK(CisNet_PublishStatus(&st) == NET_SUBMIT_OK);
    CHECK(can_n == 0u);                       /* local copy only; sent by the next pass */
    step_ms(1u);
    CHECK(can_count(0x0A0u, 0u) == 1u && can_count(0x300u, 0u) == 1u);
    CHECK(can_count(0x310u, 0u) == 1u && can_count(0x320u, 0u) == 1u);

    k = can_find(0x0A0u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(can_out[k].f.fd && can_out[k].f.brs);
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY, &e, &rr) == NET_WIRE_OK);
    CHECK(rr.boot_id == NODE_BOOT && rr.tx_sequence == 1u);
    CHECK(rr.update_sequence == 11u && rr.rear_distance_deci_cm == 1234u);
    CHECK(rr.validity == NET_VALIDITY_VALID && rr.quality_reason == NET_QUALITY_REASON_NONE);
    CHECK(rr.proximity_status == NET_PROXIMITY_VALID_DISTANCE);
    CHECK(rr.source_age_ms == 7u + (can_out[k].at - submitted));    /* local wait once */

    k = can_find(0x300u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_environment(NET_WIRE_CANDIDATE_ONLY, &e, &er) == NET_WIRE_OK);
    CHECK(er.boot_id == NODE_BOOT && er.tx_sequence == 1u);
    CHECK(er.temperature_centi_c == 2350 && er.temperature_sequence == 21u);
    CHECK(er.temperature_age_ms == 30u + (can_out[k].at - submitted));
    CHECK(er.temperature_validity == NET_VALIDITY_VALID && er.temperature_reason == NET_QUALITY_REASON_NONE);
    CHECK(er.humidity_centi_percent == 4500u && er.humidity_sequence == 121u);
    CHECK(er.humidity_age_ms == 40u + (can_out[k].at - submitted));
    CHECK(er.illuminance_lux == NET_INVALID_ILLUMINANCE && er.illuminance_sequence == 221u);
    CHECK(er.illuminance_age_ms == 50u + (can_out[k].at - submitted));
    CHECK(er.illuminance_validity == NET_VALIDITY_INVALID && er.illuminance_reason == NET_QUALITY_REASON_NO_DATA);

    k = can_find(0x310u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.boot_id == NODE_BOOT && orr.tx_sequence == 1u);
    CHECK(orr.pi_boot_id == PI_BOOT && orr.vision_sequence == 600u);           /* Pi identity kept */
    CHECK(orr.vision_age_ms == 25u + (can_out[k].at - received));              /* Pi AGE + CIS hold and wait */
    CHECK(orr.occupant_presence == NET_TRISTATE_YES && orr.presence_validity == NET_VALIDITY_VALID);
    CHECK(orr.occupant_count == 2u && orr.count_validity == NET_VALIDITY_VALID);
    CHECK(orr.vision_function_status == NET_SENSOR_FUNCTION_ACTIVE);

    k = can_find(0x320u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_status(NET_WIRE_CANDIDATE_ONLY, &e, &sr) == NET_WIRE_OK);
    CHECK(sr.boot_id == NODE_BOOT && sr.tx_sequence == 1u);
    CHECK(sr.cis_state == NET_CIS_STATE_ACTIVE && sr.vision_status == NET_SENSOR_FUNCTION_ACTIVE);
    CHECK(sr.temperature_status == NET_SENSOR_FUNCTION_ACTIVE && sr.humidity_status == NET_SENSOR_FUNCTION_ACTIVE);
    CHECK(sr.illuminance_status == NET_SENSOR_FUNCTION_FAULT && sr.rear_status == NET_SENSOR_FUNCTION_READY);
    CHECK(sr.pi_uart_status == NET_INTERFACE_READY && sr.can_interface_status == NET_INTERFACE_READY);
    CHECK(sr.fault_mask == NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT);
    CHECK(sr.active_fault == NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT && sr.recovering == NET_FLAG_NO);
    CHECK(sr.last_fault == NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT && sr.last_fault_occurrence == 3u);
    CHECK(sr.affected_function == 0x08u && sr.update_sequence == 31u);
    CHECK(sr.source_age_ms == 2u + (can_out[k].at - submitted));

    /* Second submissions: TX_SEQUENCE +1 per message (Network-owned). */
    step_ms(5u);
    rear = rear_sample(12u, 1u);
    st = status_ready(32u, 1u);
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    CHECK(CisNet_PublishStatus(&st) == NET_SUBMIT_OK);
    step_ms(2u);
    k = can_find(0x0A0u, 0u, 1u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY, &e, &rr) == NET_WIRE_OK);
    CHECK(rr.boot_id == NODE_BOOT && rr.tx_sequence == 2u && rr.update_sequence == 12u);
    k = can_find(0x320u, 0u, 1u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_status(NET_WIRE_CANDIDATE_ONLY, &e, &sr) == NET_WIRE_OK);
    CHECK(sr.tx_sequence == 2u && sr.update_sequence == 32u);
    /* Application-periodic messages: Network never repeats them. */
    step_ms(2000u);
    CHECK(can_count(0x0A0u, 0u) == 2u && can_count(0x300u, 0u) == 1u);
    CHECK(can_count(0x310u, 0u) == 1u && can_count(0x320u, 0u) == 2u);
    d = diag();
    CHECK(d.rear_tx.counters.sent == 2u && d.rear_tx.counters.periodic_sent == 0u && d.rear_tx.next_sequence == 3u);
    CHECK(d.environment_tx.counters.periodic_sent == 0u && d.occupant_tx.counters.periodic_sent == 0u);
    CHECK(d.status_tx.counters.periodic_sent == 0u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_REAR].not_accepted == 0u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_REAR].last_result == NET_SUBMIT_OK);
    CHECK(uart_n == 0u);
    return 0;
}

static int f09_publish_invalid(void)
{
    NetCisRear rear = rear_sample(1u, 0u);
    NetCisEnvironment env = environment_sample(1u, 0u);
    NetCisStatus st = status_ready(1u, 0u);
    NetCisOccupant occ;
    NetUpperUartMessage m;
    NetNodeDiag d;
    uint64_t last;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    m = vision_message(1u, 70u, 10u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u);
    CHECK(CisNet_PublishRear(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(CisNet_PublishEnvironment(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(CisNet_PublishOccupant(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(CisNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    /* Reserved codes. */
    rear.proximity_status = 6u;
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_INVALID_ARGUMENT);
    env.temperature_validity = 2u;
    CHECK(CisNet_PublishEnvironment(&env) == NET_SUBMIT_INVALID_ARGUMENT);
    st.cis_state = 4u;
    CHECK(CisNet_PublishStatus(&st) == NET_SUBMIT_INVALID_ARGUMENT);
    occ = relay_of(&last_result, 0u);
    occ.occupant_presence = 7u;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT);
    step_ms(20u);
    CHECK(can_n == 0u);
    d = diag();
    CHECK(d.app.publish[NET_APP_PORT_CIS_REAR].not_accepted == 2u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_REAR].last_result == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(d.app.publish[NET_APP_PORT_CIS_ENVIRONMENT].not_accepted == 2u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_OCCUPANT].not_accepted == 2u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_STATUS].not_accepted == 2u);
    CHECK(!d.rear_tx.has_source && !d.environment_tx.has_source && !d.status_tx.has_source && !d.occupant_tx.has_source);
    /* The next valid record still goes out with TX_SEQUENCE 1. */
    rear.proximity_status = NET_PROXIMITY_NO_OBJECT;
    rear.rear_distance_deci_cm = NET_INVALID_U16;
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x0A0u, 0u) == 1u && diag().rear_tx.next_sequence == 2u);
    return 0;
}

/* CisNet_PublishOccupant is the relay of the accepted Pi observation. */
static int f10_publish_occupant_relay(void)
{
    NetUpperUartMessage m;
    NetCisOccupant occ;
    NetWireCanEnvelope e;
    NetWireCisOccupantRaw orr;
    NetNodeDiag d;
    uint64_t last;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    memset(&occ, 0, sizeof(occ));
    occ.pi_boot_id = PI_BOOT;
    occ.vision_sequence = 1u;
    occ.occupant_presence = NET_TRISTATE_YES;
    occ.presence_validity = NET_VALIDITY_VALID;
    occ.occupant_count = 1u;
    occ.count_validity = NET_VALIDITY_VALID;
    occ.vision_function_status = NET_SENSOR_FUNCTION_ACTIVE;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);       /* no Pi observation yet */
    m = vision_message(1u, 800u, 30u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u);
    occ = relay_of(&last_result, 0u);
    occ.vision_sequence = 799u;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);       /* not the accepted judgement */
    occ = relay_of(&last_result, 0u);
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_DATA_INVALID;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);       /* CIS restriction: not representable */
    occ = relay_of(&last_result, 0u);
    occ.vision_function_status = NET_SENSOR_FUNCTION_READY;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);
    occ = relay_of(&last_result, 0u);
    occ.vision_age_ms = 29u;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);       /* younger than the Pi AGE */
    step_ms(5u);
    CHECK(can_n == 0u);
    d = diag();
    CHECK(d.app.occupant_not_current == 5u);
    /* The relay itself, with the application's own hold time already added:
     * Network counts the time since the receipt once, not twice. */
    occ = relay_of(&last_result, 40u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    k = can_find(0x310u, 0u, 0u);
    CHECK(k >= 0 && can_count(0x310u, 0u) == 1u);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.vision_sequence == 800u && orr.pi_boot_id == PI_BOOT);
    CHECK(orr.vision_age_ms == 30u + (can_out[k].at - last_result_meta.received_at_ms));
    /* A newer Pi judgement replaces the relay origin: the old record is refused. */
    occ = relay_of(&last_result, 0u);
    m = vision_message(2u, 801u, 12u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 2u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);
    occ = relay_of(&last_result, 0u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x310u, 0u) == 2u);
    return 0;
}

static int f11_publish_in_callback(void)
{
    NetUpperUartMessage m;
    NetNodeDiag d;
    uint64_t last;
    unsigned before;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* Two relays of the same judgement inside the result callback: both OK,
     * the latest one is sent in that same pass, the earlier one replaced. */
    occupant_in_result = 2;
    m = vision_message(1u, 900u, 8u);
    RUN(line_send(&m, &last));
    before = can_n;
    step_ms(1u);
    CHECK(result_count == 1u);
    CHECK(occupant_results[0] == NET_SUBMIT_OK && occupant_results[1] == NET_SUBMIT_OK);
    CHECK(can_count(0x310u, before) == 1u);
    d = diag();
    CHECK(d.occupant_tx.counters.submitted == 2u && d.occupant_tx.counters.replaced == 1u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_OCCUPANT].deferred == 2u);
    occupant_in_result = 0;
    /* CIS_STATUS published inside the permission callback, next to the relay. */
    status_in_permission = true;
    RUN(permission_in(1u, 5u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    before = can_n;
    step_ms(1u);
    CHECK(perm_count == 1u && status_result_in_permission == NET_SUBMIT_OK);
    CHECK(can_count(0x320u, before) == 1u && uart_n == 1u);
    d = diag();
    CHECK(d.app.publish[NET_APP_PORT_CIS_STATUS].deferred == 1u);
    step_ms(50u);
    CHECK(can_count(0x310u, 0u) == 1u && can_count(0x320u, 0u) == 1u);
    return 0;
}

static int f12_not_ready(void)
{
    NetCisRear rear = rear_sample(1u, 0u);
    NetCisEnvironment env = environment_sample(1u, 0u);
    NetCisStatus st = status_ready(1u, 0u);
    NetCisOccupant occ;
    NetNodeDiag d;
    memset(&occ, 0, sizeof(occ));
    /* Before the node runs: nothing is bound. */
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_NOT_READY);
    CHECK(CisNet_PublishEnvironment(&env) == NET_SUBMIT_NOT_READY);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);
    CHECK(CisNet_PublishStatus(&st) == NET_SUBMIT_NOT_READY);
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x0A0u, 0u) == 1u);
    /* Bus-off: the CAN link is held until reset; CAN publishes are not ready. */
    MockRtd_CanRaiseBusOff(BUS);
    step_ms(2u);
    rear = rear_sample(2u, 0u);
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_NOT_READY);
    CHECK(CisNet_PublishEnvironment(&env) == NET_SUBMIT_NOT_READY);
    CHECK(CisNet_PublishStatus(&st) == NET_SUBMIT_NOT_READY);
    step_ms(20u);
    CHECK(can_count(0x0A0u, 0u) == 1u && can_n == 1u);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY && d.rtd.busoff_events == 1u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_REAR].last_result == NET_SUBMIT_NOT_READY);
    /* UART-C keeps working: the Pi link is not locked by the CAN bus-off. */
    {
        NetUpperUartMessage m = vision_message(1u, 1000u, 10u);
        uint64_t last;
        RUN(line_send(&m, &last));
        step_ms(1u);
        CHECK(result_count == 1u);
    }
    return 0;
}

#ifdef CIS_NODE_TEST_ENTRY
/* The public entry drives the CIS profile exactly like the VSS one. */
static int f13_entry(void)
{
    NetUpperUartMessage m;
    NetCisRear rear = rear_sample(3u, 4u);
    NetNodeDiag d;
    uint64_t last, injected_at;
    int k;
    CHECK(start_node() == NET_OK);
    d = diag();
    CHECK(d.entry_init_result == NET_ENTRY_INIT_OK && d.entry_init_calls == 1u);
    CHECK(d.rtd.time_source == NET_HAL_RTD_TIME_APP_TICK);
    CHECK(d.rear_tx.boot_id == NODE_BOOT);    /* BOOT_ID from the board hook */
    Net_Init();                               /* later calls change nothing */
    d = diag();
    CHECK(d.entry_init_calls == 2u && d.stage == NET_NODE_STAGE_READY);
    step_ms(3u);
    CHECK(diag().entry_main_calls == 3u);
    /* DOMAIN_PERMISSION -> CisApp + relay, on the application clock axis. */
    MockRtd_AdvanceUs(200u);
    injected_at = now_ms();
    RUN(permission_in(1u, 61u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 1u && last_perm_meta.received_at_ms == injected_at && isr_callbacks == 0u);
    CHECK(uart_n == 1u);
    RUN(check_relay(&uart_out[0], 1u, 61u, NET_TRISTATE_YES, 10u, injected_at));
    /* Pi result -> CisApp. */
    m = vision_message(1u, 70u, 9u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u && last_result_meta.received_at_ms == last);
    /* Publish -> frame in the next Net_MainFunction(). */
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    step_ms(1u);
    k = can_find(0x0A0u, 0u, 0u);
    CHECK(k >= 0);
    {
        NetWireCanEnvelope e = envelope(&can_out[k].f);
        NetWireCisRearRaw rr;
        CHECK(net_wire_can_decode_rear(NET_WIRE_CANDIDATE_ONLY, &e, &rr) == NET_WIRE_OK);
        CHECK(rr.boot_id == NODE_BOOT && rr.tx_sequence == 1u && rr.update_sequence == 3u);
    }
    d = diag();
    CHECK(d.rtd.tick_future_clamped == 0u && d.entry_now_backward == 0u);
    return 0;
}
#endif

/* 20_API.md receive delivery: a normal repetition accepted with a new
 * TX_SEQUENCE is delivered to the existing callback even for the same
 * original evaluation. Registered DISABLED (see the CMake comment). */
static int f14_permission_repeat_delivered(void)
{
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_in(1u, 3u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(50u);
    RUN(permission_in(2u, 3u, 60u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm_meta.source.tx_sequence == 2u && last_perm.source_age_ms == 60u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "f01_init", f01_init },
        { "f02_permission_rx_relay", f02_permission_rx_relay },
        { "f03_permission_new_evaluations", f03_permission_new_evaluations },
        { "f04_permission_same_evaluation", f04_permission_same_evaluation },
        { "f05_permission_rx_timeout", f05_permission_rx_timeout },
        { "f06_permission_quality", f06_permission_quality },
        { "f07_vision_rx", f07_vision_rx },
        { "f08_publish_frames", f08_publish_frames },
        { "f09_publish_invalid", f09_publish_invalid },
        { "f10_publish_occupant_relay", f10_publish_occupant_relay },
        { "f11_publish_in_callback", f11_publish_in_callback },
        { "f12_not_ready", f12_not_ready },
#ifdef CIS_NODE_TEST_ENTRY
        { "f13_entry", f13_entry },
#endif
        { "f14_permission_repeat_delivered", f14_permission_repeat_delivered }
    };
    size_t i;
    if (argc != 2) { fprintf(stderr, "usage: %s <case>\n", argv[0]); return 2; }
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            case_name = cases[i].name;
            return cases[i].run();
        }
    }
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
