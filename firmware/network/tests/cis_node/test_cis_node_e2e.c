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
#include "net_hal_rtd_internal.h" /* Verify the composed adapter, without adding a public API. */
#include "net_load_traffic.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define RUN(x) do { if ((x) != 0) return 1; } while (0)

static const char *case_name;

enum { NODE_BOOT = 0x43495331u, DOMAIN_BOOT = 0x444F4D31u, PI_BOOT = 0x50490001u, BUS = 0, LPUART = 11 };

#define PASS_US 1000u   /* default main pass period */
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
static uint16_t result_links[64];           /* LINK_SEQUENCE of each Vision Result callback */
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
    if (result_count < sizeof(result_links) / sizeof(result_links[0])) result_links[result_count] = meta->link_sequence;
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

static uint64_t pass_us = PASS_US;   /* main pass period of the case */
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

/* Superloop: one main pass at every pass_us boundary while time advances. */
static void advance_us(uint64_t us)
{
    uint64_t end = MockRtd_NowUs() + us;
    while (next_pass_us <= end) {
        MockRtd_AdvanceUs(next_pass_us - MockRtd_NowUs());
        pass();
        next_pass_us += pass_us;
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
    next_pass_us = pass_us;
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

/* DOMAIN_PERMISSION of a DOMAIN run on the bus (ISR context of the CAN
 * controller), with its operation quality. */
static int permission_op(uint32_t boot, uint32_t tx, uint32_t update, uint16_t age, uint8_t value,
                         uint8_t quality, uint8_t operation_quality)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope e;
    MockCanFrame f;
    int mb;
    memset(&p, 0, sizeof(p));
    p.boot_id = boot;
    p.tx_sequence = tx;
    p.vehicle_power_permission = value;
    p.power_permission_quality = quality;
    p.power_operation_state = 1u;
    p.operation_allowed = 1u;
    p.operation_quality = operation_quality;
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

static int permission_from(uint32_t boot, uint32_t tx, uint32_t update, uint16_t age, uint8_t value, uint8_t quality)
{
    return permission_op(boot, tx, update, age, value, quality, NET_QUALITY_OK);
}

static int permission_in(uint32_t tx, uint32_t update, uint16_t age, uint8_t value, uint8_t quality)
{
    return permission_from(DOMAIN_BOOT, tx, update, age, value, quality);
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

/* A relayed P_PERMISSION as the Pi receives it. UPDATE_SEQUENCE is the CIS
 * relay revision (04_COMMON/10_DATA_AND_ID_RULES.md number spaces: not the
 * DOMAIN number; from 1 in each CIS run, +1 per relay event). The power
 * value and quality are the DOMAIN's as received; the reason is the fixed one
 * of that quality: OK -> NONE, STALE -> STALE, INVALID -> DATA_INVALID,
 * NO_DATA -> NO_DATA. */
static int check_relay_quality(const UartOut *u, uint16_t link, uint32_t revision, uint8_t value,
                               uint8_t quality, uint8_t reason, uint16_t domain_age, uint64_t received_at)
{
    const NetWirePPermission *p = &u->m.data.p_permission;
    CHECK(u->m.type == 0x62u && u->m.sequence == link);
    CHECK(p->cis_boot_id == NODE_BOOT);                      /* CIS relay generation */
    CHECK(p->update_sequence == revision);                   /* CIS relay revision */
    CHECK(p->vehicle_power_permission == value);
    CHECK(p->value_quality == quality && p->quality_reason == reason);
    CHECK(p->use_limit_ms == 1500u);
    /* DOMAIN AGE + local wait from the CAN capture to the UART dispatch, once. */
    CHECK(p->source_age_ms == domain_age + (u->at - received_at));
    return 0;
}

static int check_relay(const UartOut *u, uint16_t link, uint32_t revision, uint8_t value,
                       uint16_t domain_age, uint64_t received_at)
{
    return check_relay_quality(u, link, revision, value, NET_QUALITY_OK, NET_QUALITY_REASON_NONE,
                               domain_age, received_at);
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
    CHECK(net_hal_rtd_core.adapter != NULL);
    CHECK(net_hal_rtd_core.adapter->platform == (NetHalPlatform)NETWORK_NODE_PLATFORM);
    CHECK(d.stage == NET_NODE_STAGE_READY && d.stage_status == NET_OK);
    CHECK(d.rtd.started && d.rtd.init_failed_step == NET_HAL_RTD_STEP_NONE);
    CHECK(d.rtd.uart[1].uart_present && d.rtd.uart[1].uart_rx_armed && !d.rtd.uart[0].uart_present);
    CHECK(MockRtd_UartInits(LPUART) == 1u && MockRtd_UartRxArmed(LPUART));
    CHECK(d.app.bound);
    /* M04 table: no message has a Network period. */
    CHECK(d.rear_tx.boot_id == NODE_BOOT && d.rear_tx.next_sequence == 1u && d.rear_tx.period_ms == 0u);
    CHECK(d.environment_tx.period_ms == 0u && d.occupant_tx.period_ms == 0u && d.status_tx.period_ms == 0u);
    /* P_PERMISSION is never repeated by Network (period 0): it leaves on a new
     * DOMAIN evaluation, an accepted DOMAIN run change or a quality
     * restriction; the Pi's 1500 ms receive timeout is fed by DOMAIN's
     * re-evaluation about every 200 ms (03_COMMUNICATION/10_CAN_FD.md
     * DOMAIN_PERMISSION). No relay revision before the first relay. */
    CHECK(d.pi_permission_tx.period_ms == 0u && d.pi_permission_tx.next_sequence == 1u);
    CHECK(d.pi_permission_tx.revision == 0u);
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
    RUN(check_relay(&uart_out[0], 1u, 1u, NET_TRISTATE_YES, 10u, last_perm_meta.received_at_ms));
    d = diag();
    CHECK(d.app.permission_delivered == 1u && d.app.relay_submitted == 1u && d.app.relay_refused == 0u);
    CHECK(d.app.last_relay_result == NET_SUBMIT_OK);
    CHECK(d.pi_permission_tx.counters.sent == 1u && d.pi_permission_tx.next_sequence == 2u);
    CHECK(d.pi_permission_tx.revision == 1u);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    /* Exact duplicate (same TX, same bytes): no callback, no relay. */
    d0 = d;
    RUN(permission_in(1u, 41u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(3u);
    d = diag();
    CHECK(perm_count == 1u && uart_n == 1u && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* Newer DOMAIN evaluation: delivered and relayed with the next LINK_SEQUENCE
     * and the next relay revision. */
    RUN(permission_in(2u, 42u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm.update_sequence == 42u && uart_n == 2u);
    RUN(check_relay(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, 5u, last_perm_meta.received_at_ms));
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
 * an evaluation add no frame and Network repeats no relay frame. */
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
        CHECK(uart_out[i].m.data.p_permission.update_sequence == 1u + i);  /* relay revision +1 */
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
 * one relay frame and nothing more reaches the Pi. Network never repeats
 * P_PERMISSION (period 0): it leaves only for a new evaluation, an accepted
 * DOMAIN run change or a quality restriction (30_UART_C.md P_PERMISSION), and
 * the Pi's 1500 ms receive timeout is fed by DOMAIN's re-evaluation about
 * every 200 ms (10_CAN_FD.md DOMAIN_PERMISSION). The CAN repetitions reach
 * the application but are not relayed; once DOMAIN stops evaluating, nothing
 * extends the earlier ALLOW. */
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
        /* No repetition is ever due, not at +500 ms either. */
        if (t == offered_at + 500u) CHECK(d.pi_permission_tx.counters.not_sent == 0u && uart_n == 1u);
        /* DOMAIN repeats the same evaluation every 50 ms, its AGE growing. */
        if (t < first + 1200u && (t - first) % 50u == 0u)
            RUN(permission_in(++tx, 7u, (uint16_t)(10u + (t - first)), NET_TRISTATE_YES, NET_QUALITY_OK));
    }
    d = diag();
    CHECK(uart_n == 1u && uart_bad == 0u);
    CHECK(d.pi_permission_tx.counters.sent == 1u && d.pi_permission_tx.counters.periodic_sent == 0u);
    CHECK(d.pi_permission_tx.counters.not_sent == 0u && d.pi_permission_tx.revision == 1u);
    CHECK(d.pi_permission_tx.next_sequence == 2u);               /* no LINK_SEQUENCE used */
    CHECK(d.app.relay_submitted == 1u && d.app.permission_repeats > 0u);
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
    RUN(check_relay(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, 3u, last_perm_meta.received_at_ms));
    return 0;
}

/* A new DOMAIN evaluation with a restricted power quality is relayed at once
 * as an explicit state (02_NODES/CIS/20_API.md internal relay: a
 * normal-format DENY / STALE / INVALID is kept and relayed; 30_UART_C.md
 * P_PERMISSION: an actual quality restriction is submitted at once): the
 * value and quality as received, the fixed reason DATA_INVALID for INVALID.
 * The relay is one frame; the earlier ALLOW is never sent again. */
static int f06_permission_quality(void)
{
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_in(1u, 20u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 1u);
    step_ms(100u);
    RUN(permission_in(2u, 21u, 5u, NET_TRISTATE_YES, NET_QUALITY_INVALID));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm.power_permission_quality == NET_QUALITY_INVALID);
    d = diag();
    CHECK(d.app.relay_submitted == 2u && d.app.relay_refused == 0u && d.app.last_relay_result == NET_SUBMIT_OK);
    CHECK(d.permission.reason == NET_PERMISSION_BAD_QUALITY && !d.permission.power_usable);
    CHECK(uart_n == 2u && uart_out[1].at == perm_delivered_at);   /* at once */
    RUN(check_relay_quality(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, NET_QUALITY_INVALID,
                            NET_QUALITY_REASON_DATA_INVALID, 5u, last_perm_meta.received_at_ms));
    step_ms(1500u);
    CHECK(uart_n == 2u);                                      /* never repeated */
    /* A confirmed DENY (OK quality) is relayed as it is. */
    RUN(permission_in(3u, 22u, 6u, NET_TRISTATE_NO, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 3u);
    RUN(check_relay(&uart_out[2], 3u, 3u, NET_TRISTATE_NO, 6u, last_perm_meta.received_at_ms));
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

/* CisNet_PublishOccupant relays the accepted Pi observation
 * (02_NODES/CIS/20_API.md CisNet_PublishOccupant, 10_DATA.md Pi original
 * observation and CIS evaluation): Pi identity, values and AGE basis kept;
 * the quality may be the Pi's or a CIS restriction of it, and
 * vision_function_status is the CIS's own function state. Before any Pi
 * judgement the record is NOT_READY; a record that can never be that relay is
 * INVALID_ARGUMENT (a resubmission could not succeed). */
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
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT); /* not the accepted judgement */
    occ = relay_of(&last_result, 0u);
    occ.vision_age_ms = 29u;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT); /* younger than the Pi AGE */
    step_ms(5u);
    CHECK(can_n == 0u);
    d = diag();
    CHECK(d.app.occupant_not_current == 3u);
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
    CHECK(orr.vision_function_status == NET_SENSOR_FUNCTION_ACTIVE);
    /* The same judgement with a CIS restriction and the CIS's own function
     * state: accepted and sent with that quality and state. */
    occ = relay_of(&last_result, 0u);
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_DATA_INVALID;
    occ.vision_function_status = NET_SENSOR_FUNCTION_READY;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    k = can_find(0x310u, 0u, 1u);
    CHECK(k >= 0 && can_count(0x310u, 0u) == 2u);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.vision_sequence == 800u && orr.occupant_presence == NET_TRISTATE_YES);
    CHECK(orr.presence_validity == NET_VALIDITY_INVALID && orr.presence_reason == NET_QUALITY_REASON_DATA_INVALID);
    CHECK(orr.count_validity == NET_VALIDITY_VALID && orr.vision_function_status == NET_SENSOR_FUNCTION_READY);
    /* A newer Pi judgement replaces the relay origin: the old record can never
     * be its relay. */
    occ = relay_of(&last_result, 0u);
    m = vision_message(2u, 801u, 12u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 2u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT);
    occ = relay_of(&last_result, 0u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x310u, 0u) == 3u);
    d = diag();
    CHECK(d.app.occupant_not_current == 4u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_OCCUPANT].last_result == NET_SUBMIT_OK);
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
    /* Bus-off: the CAN link is closed for the hold time (100 ms, longer than
     * this case); CAN publishes are not ready. */
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

/* CAN bus-off and recovery (net_hal_rtd_can.c): during the hold the
 * DOMAIN_PERMISSION supervision reports the link unavailable, a frame on the
 * bus is not delivered and CAN publishes are not ready; UART-C keeps working.
 * After the hold the link opens under a new port epoch: a newer
 * DOMAIN_PERMISSION is delivered and relayed to the Pi again, and CIS_REAR
 * goes out. */
static int f12b_busoff_recovery(void)
{
    NetCisRear rear = rear_sample(1u, 0u);
    NetNodeDiag d;
    uint64_t t0;
    unsigned rear_before;
    CHECK(start_node() == NET_OK);
    step_ms(5u);
    RUN(permission_in(1u, 41u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 1u && uart_n == 1u);
    CHECK(diag().permission.reason == NET_PERMISSION_CURRENT);
    MockRtd_CanRaiseBusOff(BUS);
    t0 = now_ms();
    step_ms(2u);
    d = diag();
    CHECK(d.rtd.can_link_state == 1u && d.permission.reason == NET_PERMISSION_LINK_UNAVAILABLE);
    CHECK(!d.permission.power_usable);
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_NOT_READY);
    RUN(permission_in(2u, 42u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(2u);
    CHECK(perm_count == 1u);
    {
        NetUpperUartMessage m = vision_message(1u, 1000u, 10u);
        uint64_t last;
        RUN(line_send(&m, &last));
        step_ms(1u);
        CHECK(result_count == 1u);
    }
    while (now_ms() < t0 + 102u) step_ms(1u);
    d = diag();
    CHECK(d.rtd.can_link_state == 0u && d.rtd.busoff_recoveries == 1u && d.upper.resets == 1u);
    RUN(permission_in(3u, 43u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && last_perm.update_sequence == 43u);
    d = diag();
    CHECK(d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    CHECK(uart_n == 2u && uart_bad == 0u);
    rear_before = can_count(0x0A0u, 0u);
    CHECK(CisNet_PublishRear(&rear) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x0A0u, 0u) == rear_before + 1u);
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
    RUN(check_relay(&uart_out[0], 1u, 1u, NET_TRISTATE_YES, 10u, injected_at));
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

/* The same DOMAIN evaluation (UPDATE_SEQUENCE kept) reports a lower power
 * quality with a new TX_SEQUENCE. 04_COMMON/10_DATA_AND_ID_RULES.md (repeat
 * and current quality): an actual quality drop of the same original
 * observation is accepted and delivered; a higher TX_SEQUENCE alone never
 * changes the value nor promotes the quality back. CIS/20_API.md
 * (CisApp_OnDomainPermission, relay path): the quality drop must not be
 * missed by the application nor by the internal relay path, and an earlier
 * ALLOW must not come back. 30_UART_C.md P_PERMISSION: the drop is relayed
 * at once, with a new relay revision. */
static int f15_permission_quality_drop_same_update(void)
{
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_in(1u, 20u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 1u && uart_n == 1u);
    d0 = diag();
    CHECK(d0.permission.reason == NET_PERMISSION_CURRENT && d0.permission.power_usable);
    /* TX2: same UPDATE 20, its AGE grown, power quality INVALID. */
    step_ms(49u);
    RUN(permission_in(2u, 20u, 60u, NET_TRISTATE_YES, NET_QUALITY_INVALID));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 2u && last_perm_meta.source.tx_sequence == 2u);
    CHECK(last_perm.update_sequence == 20u && last_perm.source_age_ms == 60u);
    CHECK(last_perm.power_permission_quality == NET_QUALITY_INVALID);
    CHECK(last_perm.vehicle_power_permission == NET_TRISTATE_YES);
    CHECK(d.upper.rx_held == d0.upper.rx_held && d.upper.duplicates == d0.upper.duplicates);
    CHECK(d.app.permission_delivered == 2u);
    /* The relay path got the same drop and sent it at once: the value and
     * AGE as received, INVALID with the reason DATA_INVALID, relay revision 2.
     * The earlier ALLOW is not sent again. */
    CHECK(d.app.relay_submitted == 2u && d.app.relay_refused == 0u && d.app.last_relay_result == NET_SUBMIT_OK);
    CHECK(d.permission.reason == NET_PERMISSION_BAD_QUALITY && !d.permission.power_usable);
    CHECK(uart_n == 2u && uart_out[1].at == perm_delivered_at);
    RUN(check_relay_quality(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, NET_QUALITY_INVALID,
                            NET_QUALITY_REASON_DATA_INVALID, 60u, last_perm_meta.received_at_ms));
    /* TX3: the same UPDATE back to OK (promotion by TX_SEQUENCE alone): held,
     * the restriction stays. */
    d0 = diag();
    step_ms(49u);
    RUN(permission_in(3u, 20u, 110u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 2u && d.upper.rx_held == d0.upper.rx_held + 1u);
    CHECK(uart_n == 2u && d.permission.reason == NET_PERMISSION_BAD_QUALITY && !d.permission.power_usable);
    /* TX4: the same UPDATE with another value: held. */
    step_ms(49u);
    RUN(permission_in(4u, 20u, 160u, NET_TRISTATE_NO, NET_QUALITY_INVALID));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 2u && d.upper.rx_held == d0.upper.rx_held + 2u);
    /* Nothing more reaches the Pi meanwhile: no repetition, no earlier ALLOW. */
    step_ms(1000u);
    CHECK(uart_n == 2u && uart_bad == 0u);
    /* TX5: a new DOMAIN evaluation, OK: delivered and relayed with the next
     * LINK_SEQUENCE and relay revision. */
    RUN(permission_in(5u, 21u, 4u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 3u && last_perm.update_sequence == 21u && uart_n == 3u);
    RUN(check_relay(&uart_out[2], 3u, 3u, NET_TRISTATE_YES, 4u, last_perm_meta.received_at_ms));
    CHECK(d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* Main pass every 3 ms through the node root (NetNode_Step / Net_MainFunction):
 * P_VISION_RESULT frames back to back (29 B, 2.5 ms each at 115200 bit/s)
 * bring more bytes per pass than one drain budget (16 B); DOMAIN_PERMISSION
 * frames arrive meanwhile. Every frame reaches the application whole, in
 * order, with the ISR time of its last byte (30_UART_C.md, CisApp_OnPiVision*). */
static int f16_vision_rx_main_3ms(void)
{
    enum { FRAMES = 12 };
    NetUpperUartMessage m;
    NetNodeDiag d;
    uint64_t last[FRAMES];
    unsigned i;
    pass_us = 3000u;
    CHECK(start_node() == NET_OK);
    step_ms(6u);
    for (i = 0u; i < FRAMES; ++i) {
        if ((i % 4u) == 0u) RUN(permission_in(1u + i / 4u, 30u + i / 4u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
        m = vision_message((uint16_t)(1u + i), 700u + i, 20u);
        RUN(line_send(&m, &last[i]));
    }
    step_ms(12u);
    CHECK(result_count == FRAMES && isr_callbacks == 0u);
    for (i = 0u; i < FRAMES; ++i) CHECK(result_links[i] == (uint16_t)(1u + i));
    CHECK(last_result.vision_sequence == 700u + FRAMES - 1u);
    CHECK(last_result_meta.received_at_ms == (uint32_t)last[FRAMES - 1u]);
    CHECK(perm_count == FRAMES / 4u);
    d = diag();
    CHECK(d.app.vision_result_delivered == FRAMES);
    CHECK(d.rtd.uart_poll_drained > 0u && d.rtd.main_drained_max <= d.rtd.isr_drain_budget);
    CHECK(d.rtd.uart[1].uart_isr_ring_overflow == 0u && d.rtd.uart[1].uart_rx_loss_marks == 0u);
    CHECK(d.upper.rx_held == 0u && d.upper.rx_invalid == 0u && d.upper.rx_dropped == 0u && d.upper.stale == 0u);
    return 0;
}

/* DOMAIN restart (04_COMMON/10_DATA_AND_ID_RULES.md generation; 30_UART_C.md
 * P_PERMISSION): run A, then run B with another BOOT_ID, TX from 1 and an
 * UPDATE_SEQUENCE below run A's. The first B frame reaches
 * CisApp_OnDomainPermission and, as an accepted change of the DOMAIN run, is
 * relayed to the Pi at once with the next CIS relay revision (the relay
 * number never goes back within this CIS run although the DOMAIN number
 * does). A late frame of run A is refused (no callback, no relay).
 * Repetitions of one evaluation are delivered, not relayed. */
static int f17_domain_reboot(void)
{
    enum { BOOT_A = 0x444F4D41u, BOOT_B = 0x444F4D42u };
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    RUN(permission_from(BOOT_A, 10u, 50u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 1u && uart_n == 1u);
    RUN(check_relay(&uart_out[0], 1u, 1u, NET_TRISTATE_YES, 10u, last_perm_meta.received_at_ms));
    /* The same evaluation of run A again, 50 ms later: delivered, not relayed. */
    step_ms(49u);
    RUN(permission_from(BOOT_A, 11u, 50u, 60u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && uart_n == 1u && last_perm_meta.source.tx_sequence == 11u);
    d0 = diag();
    CHECK(d0.app.permission_repeats == 1u && d0.app.relay_submitted == 1u);
    /* DOMAIN restarts: run B, TX 1, UPDATE_SEQUENCE 3 (below 50). */
    step_ms(30u);
    RUN(permission_from(BOOT_B, 1u, 3u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 3u);
    CHECK(last_perm_meta.source.boot_id == BOOT_B && last_perm_meta.source.tx_sequence == 1u);
    CHECK(last_perm.update_sequence == 3u && last_perm.source_age_ms == 5u);
    CHECK(uart_n == 2u && uart_bad == 0u);
    CHECK(uart_out[1].at == perm_delivered_at);              /* at once, in the same pass */
    RUN(check_relay(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, 5u, last_perm_meta.received_at_ms));
    d = diag();
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 1u);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    CHECK(d.permission.original.update_sequence == 3u);
    /* A late frame of run A: old generation, nothing delivered or relayed. */
    d0 = d;
    RUN(permission_from(BOOT_A, 12u, 51u, 4u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(2u);
    d = diag();
    CHECK(perm_count == 3u && uart_n == 2u);
    CHECK(d.upper.old_generation == d0.upper.old_generation + 1u && d.upper.stale == d0.upper.stale + 1u);
    CHECK(d.permission.original.update_sequence == 3u);
    /* Run B goes on: its repetition is delivered, not relayed; its next
     * evaluation is relayed. */
    step_ms(47u);
    RUN(permission_from(BOOT_B, 2u, 3u, 55u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 4u && uart_n == 2u);
    step_ms(49u);
    RUN(permission_from(BOOT_B, 3u, 4u, 2u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 5u && uart_n == 3u);
    RUN(check_relay(&uart_out[2], 3u, 3u, NET_TRISTATE_YES, 2u, last_perm_meta.received_at_ms));
    CHECK(isr_callbacks == 0u);
    return 0;
}

static NetUpperUartMessage vision_of(uint32_t pi_boot, uint16_t link, uint32_t vision_sequence, uint16_t age)
{
    NetUpperUartMessage m = vision_message(link, vision_sequence, age);
    m.data.p_vision_result.pi_boot_id = pi_boot;
    return m;
}

static NetUpperUartMessage status_of(uint32_t pi_boot, uint16_t link, uint32_t status_sequence)
{
    NetUpperUartMessage m = vision_status_message(link, status_sequence);
    m.data.p_vision_status.pi_boot_id = pi_boot;
    return m;
}

/* Pi restart (04_COMMON/10_DATA_AND_ID_RULES.md generation: Pi A -> B ->
 * late A): run A near LINK_SEQUENCE 1000, run B from LINK_SEQUENCE 0. The new
 * Pi BOOT is judged before the LINK_SEQUENCE window; P_VISION_RESULT and
 * P_VISION_STATUS share the link, so once B is accepted a late A frame of
 * either type is refused, and the first B frame of the other type is taken. */
static int f18_pi_reboot(void)
{
    enum { PI_A = 0x50490A01u, PI_B = 0x50490B01u };
    NetUpperUartMessage m;
    NetCisOccupant occ;
    NetNodeDiag d0, d;
    uint64_t last;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    m = vision_of(PI_A, 1000u, 500u, 20u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    m = status_of(PI_A, 1001u, 31u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u && status_count == 1u);
    CHECK(last_result.pi_boot_id == PI_A && last_result_meta.link_sequence == 1000u);
    d0 = diag();
    /* Pi restarts: run B, LINK_SEQUENCE from 0. */
    step_ms(20u);
    m = vision_of(PI_B, 0u, 1u, 10u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 2u && last_result.pi_boot_id == PI_B && last_result.vision_sequence == 1u);
    CHECK(last_result_meta.link_sequence == 0u && last_result_meta.received_at_ms == last);
    d = diag();
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 1u);
    /* A late P_VISION_STATUS of run A before B's first status: refused. */
    m = status_of(PI_A, 1002u, 32u);
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(status_count == 1u && d.upper.old_generation == d0.upper.old_generation + 1u);
    /* The first status of run B is taken in the same generation. */
    m = status_of(PI_B, 1u, 1u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(status_count == 2u && last_status.pi_boot_id == PI_B && last_status_meta.link_sequence == 1u);
    /* A late result of run A: refused. */
    m = vision_of(PI_A, 1003u, 501u, 15u);
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(result_count == 2u && d.upper.old_generation == d0.upper.old_generation + 2u);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 1u);
    /* Run B goes on, and its judgement is the CIS_OCCUPANT relay origin. */
    m = vision_of(PI_B, 2u, 2u, 8u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 3u && last_result.vision_sequence == 2u && last_result_meta.link_sequence == 2u);
    occ = relay_of(&last_result, 0u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x310u, 0u) == 1u);
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* DOMAIN as 10_DATA_AND_ID_RULES.md (timing) describes it for one second:
 * CAN every 50 ms, an actual new evaluation about every 200 ms, the frames in
 * between repeat the evaluation with a new TX_SEQUENCE and a grown AGE (one
 * evaluation alone cannot last a second: its original Power life is 500 ms).
 * Every frame reaches CisApp_OnDomainPermission (receive supervision); a
 * P_PERMISSION frame leaves only for a new evaluation (30_UART_C.md), never per
 * CAN frame. */
static int f19_permission_repeat_no_flood(void)
{
    NetNodeDiag d;
    uint32_t tx = 0u, update = 300u;
    uint16_t age = 0u;
    unsigned t, i, evaluations = 0u;
    uint64_t first_capture[8];
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    for (t = 0u; t < 1000u; t += 50u) {
        if (t % 200u == 0u) { ++update; age = 6u; first_capture[evaluations++ & 7u] = now_ms(); }
        else age = (uint16_t)(age + 50u);
        RUN(permission_in(++tx, update, age, NET_TRISTATE_YES, NET_QUALITY_OK));
        step_ms(1u);
        CHECK(perm_count == tx && last_perm_meta.source.tx_sequence == tx);
        CHECK(uart_n == evaluations);                 /* nothing for a repetition */
        step_ms(49u);
    }
    CHECK(perm_count >= 19u && perm_count == 20u);
    CHECK(evaluations == 5u && uart_n == 5u && uart_bad == 0u);
    for (i = 0u; i < uart_n; ++i) {
        CHECK(uart_out[i].m.sequence == i + 1u);
        CHECK(uart_out[i].m.data.p_permission.update_sequence == 1u + i);  /* relay revision +1 */
        CHECK(uart_out[i].at - first_capture[i] <= 1u);
    }
    d = diag();
    CHECK(d.app.permission_delivered == 20u && d.app.permission_repeats == 15u);
    CHECK(d.app.relay_submitted == 5u && d.app.relay_refused == 0u);
    CHECK(d.upper.repeat_delivered == 15u && d.upper.duplicates == 0u && d.upper.rx_held == 0u);
    CHECK(d.pi_permission_tx.counters.sent == 5u);
    return 0;
}

/* TX_SEQUENCE starts at 1 in every sender run (03_COMMUNICATION/10_CAN_FD.md,
 * TX_SEQUENCE: first 1, +1 per frame), so 0 is a reserved value. A
 * DOMAIN_PERMISSION carrying TX 0 is a structure error (VSS contract C5
 * (VSS_NODE_INTEGRATION.md): structure checks are Network's first; a
 * structure error frame causes no generation change): neither the very first
 * frame nor the first frame of a restarted DOMAIN run with TX 0 reaches
 * CisApp_OnDomainPermission or the Pi, and neither moves the generation. */
static int f20_permission_tx_zero(void)
{
    enum { BOOT_A = 0x444F4D41u, BOOT_B = 0x444F4D42u };
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* The very first frame of the key, TX 0. */
    d0 = diag();
    RUN(permission_from(BOOT_A, 0u, 50u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(2u);
    d = diag();
    CHECK(perm_count == 0u && uart_n == 0u && d.app.permission_delivered == 0u);
    CHECK(d.upper.rx_invalid == d0.upper.rx_invalid + 1u);
    CHECK(d.upper.generation_changes == 0u && d.upper.rx_delivered == d0.upper.rx_delivered);
    /* Run A from TX 1: taken and relayed as a first frame. */
    RUN(permission_from(BOOT_A, 1u, 50u, 10u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 1u && uart_n == 1u);
    CHECK(last_perm_meta.source.boot_id == BOOT_A && last_perm_meta.source.tx_sequence == 1u);
    RUN(check_relay(&uart_out[0], 1u, 1u, NET_TRISTATE_YES, 10u, last_perm_meta.received_at_ms));
    /* DOMAIN restarts as run B and its first frame carries TX 0: refused, the
     * key stays on run A. */
    step_ms(20u);
    d0 = diag();
    RUN(permission_from(BOOT_B, 0u, 3u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(2u);
    d = diag();
    CHECK(perm_count == 1u && uart_n == 1u);
    CHECK(d.upper.rx_invalid == d0.upper.rx_invalid + 1u);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes);
    CHECK(d.upper.old_generation == d0.upper.old_generation);
    CHECK(d.permission.original.update_sequence == 50u);
    /* Run A goes on in order: its next evaluation is taken and relayed. */
    RUN(permission_from(BOOT_A, 2u, 51u, 4u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && uart_n == 2u && last_perm_meta.source.boot_id == BOOT_A);
    RUN(check_relay(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, 4u, last_perm_meta.received_at_ms));
    /* Run B from TX 1 is the new generation; a late A frame is then refused. */
    RUN(permission_from(BOOT_B, 1u, 3u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 3u && uart_n == 3u && last_perm_meta.source.boot_id == BOOT_B);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 1u);
    RUN(check_relay(&uart_out[2], 3u, 3u, NET_TRISTATE_YES, 5u, last_perm_meta.received_at_ms));
    RUN(permission_from(BOOT_A, 3u, 52u, 4u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(2u);
    d = diag();
    CHECK(perm_count == 3u && uart_n == 3u && d.upper.old_generation == d0.upper.old_generation + 1u);
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* An actual quality restriction of the same DOMAIN evaluation is relayed at
 * once (30_UART_C.md P_PERMISSION: "an actual quality restriction ... is
 * submitted as soon as possible"; the quality drop of the same original
 * successful evaluation goes over the existing CIS relay, not deferred to the
 * next successful evaluation; 02_NODES/CIS/20_API.md internal relay: the drop
 * is not missed on that path). For STALE, INVALID and NO_DATA in turn: an
 * evaluation relayed OK, then the same UPDATE_SEQUENCE with that quality ->
 * one P_PERMISSION in the same pass. VALUE_QUALITY is the DOMAIN power
 * quality, QUALITY_REASON the fixed reason of it (STALE -> STALE,
 * INVALID -> DATA_INVALID, NO_DATA -> NO_DATA, 04_COMMON/
 * 10_DATA_AND_ID_RULES.md quality reason codes), the power value and AGE the
 * received ones (nothing made up), and each relay is a new relay revision. */
static int f21_permission_quality_drop_relayed(void)
{
    static const struct { uint8_t quality, reason; } drop[3] = {
        { NET_QUALITY_STALE, NET_QUALITY_REASON_STALE },
        { NET_QUALITY_INVALID, NET_QUALITY_REASON_DATA_INVALID },
        { NET_QUALITY_NO_DATA, NET_QUALITY_REASON_NO_DATA }
    };
    NetNodeDiag d;
    uint32_t tx = 0u, revision = 0u;
    unsigned i, before;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    for (i = 0u; i < 3u; ++i) {
        uint32_t update = 70u + i;
        /* A new evaluation, OK quality: relayed. */
        RUN(permission_in(++tx, update, 4u, NET_TRISTATE_YES, NET_QUALITY_OK));
        step_ms(1u);
        CHECK(uart_n == 2u * i + 1u);
        RUN(check_relay(&uart_out[uart_n - 1u], (uint16_t)uart_n, ++revision, NET_TRISTATE_YES, 4u,
                        last_perm_meta.received_at_ms));
        /* The same evaluation 50 ms later, its AGE grown, the restricted quality. */
        step_ms(49u);
        before = uart_n;
        RUN(permission_in(++tx, update, 54u, NET_TRISTATE_YES, drop[i].quality));
        step_ms(1u);
        CHECK(perm_count == tx && last_perm.update_sequence == update);
        CHECK(last_perm.power_permission_quality == drop[i].quality);
        CHECK(uart_n == before + 1u && uart_bad == 0u);              /* one frame */
        CHECK(uart_out[uart_n - 1u].at == perm_delivered_at);        /* at once */
        RUN(check_relay_quality(&uart_out[uart_n - 1u], (uint16_t)uart_n, ++revision, NET_TRISTATE_YES,
                                drop[i].quality, drop[i].reason, 54u, last_perm_meta.received_at_ms));
        /* Nothing more: P_PERMISSION is not repeated. */
        step_ms(149u);
        CHECK(uart_n == before + 1u);
    }
    d = diag();
    CHECK(d.app.relay_submitted == 6u && d.app.relay_refused == 0u && d.app.permission_repeats == 0u);
    CHECK(d.pi_permission_tx.revision == 6u && d.pi_permission_tx.counters.sent == 6u);
    CHECK(d.pi_permission_tx.counters.not_sent == 0u && d.pi_permission_tx.counters.periodic_sent == 0u);
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* P_PERMISSION UPDATE_SEQUENCE is the CIS relay revision
 * (04_COMMON/10_DATA_AND_ID_RULES.md number spaces: the CIS private relay's,
 * "not a lossless copy of the DOMAIN number or the CAN synthetic number";
 * 02_NODES/PI/10_DATA.md update_sequence: a new relay evaluation, reflecting a
 * DOMAIN revision / run change or a quality restriction, kept on repeated
 * frames; 30_UART_C.md UPDATE_SEQUENCE). It starts at 1 in this CIS run and
 * grows by 1 for a new evaluation, an accepted DOMAIN run change (a new BOOT
 * whose UPDATE is below the earlier run's) and a quality restriction; within
 * the run it never goes back. A DOMAIN repetition and a DOMAIN_PERMISSION
 * that changes nothing the relay carries (its operation quality only) are no
 * relay event: no frame, the revision stays. A run change with the same
 * UPDATE, value and quality is a relay event. Two relay events in one main
 * pass use two revisions but leave as one frame (the record no device has
 * taken yet is replaced: 02_NODES/CIS/20_API.md current value keeping). */
static int f22_relay_sequence_cis_owned(void)
{
    enum { BOOT_A = 0x444F4D41u, BOOT_B = 0x444F4D42u, BOOT_C = 0x444F4D43u };
    NetNodeDiag d;
    unsigned i;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* First relay: revision 1, whatever the DOMAIN number (900). */
    RUN(permission_from(BOOT_A, 1u, 900u, 5u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(uart_n == 1u);
    RUN(check_relay(&uart_out[0], 1u, 1u, NET_TRISTATE_YES, 5u, last_perm_meta.received_at_ms));
    /* DOMAIN repeats evaluation 900 (new TX, AGE grown): delivered, no frame. */
    step_ms(49u);
    RUN(permission_from(BOOT_A, 2u, 900u, 55u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 2u && uart_n == 1u);
    /* New evaluation 901: revision 2. */
    step_ms(49u);
    RUN(permission_from(BOOT_A, 3u, 901u, 3u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 3u && uart_n == 2u);
    RUN(check_relay(&uart_out[1], 2u, 2u, NET_TRISTATE_YES, 3u, last_perm_meta.received_at_ms));
    /* Evaluation 901 again with only its operation quality lowered: the
     * application gets it, the relay carries nothing new (no frame, revision 2). */
    step_ms(49u);
    RUN(permission_op(BOOT_A, 4u, 901u, 53u, NET_TRISTATE_YES, NET_QUALITY_OK, NET_QUALITY_STALE));
    step_ms(1u);
    d = diag();
    CHECK(perm_count == 4u && last_perm.operation_quality == NET_QUALITY_STALE && uart_n == 2u);
    CHECK(d.app.relay_unchanged == 1u && d.pi_permission_tx.revision == 2u);
    /* DOMAIN restarts: run B, TX 1, UPDATE 4 (below 901): revision 3. */
    step_ms(30u);
    RUN(permission_from(BOOT_B, 1u, 4u, 6u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 5u && last_perm_meta.source.boot_id == BOOT_B && uart_n == 3u);
    RUN(check_relay(&uart_out[2], 3u, 3u, NET_TRISTATE_YES, 6u, last_perm_meta.received_at_ms));
    /* Quality restriction of run B's evaluation 4: revision 4. */
    step_ms(49u);
    RUN(permission_from(BOOT_B, 2u, 4u, 56u, NET_TRISTATE_YES, NET_QUALITY_STALE));
    step_ms(1u);
    CHECK(perm_count == 6u && uart_n == 4u);
    RUN(check_relay_quality(&uart_out[3], 4u, 4u, NET_TRISTATE_YES, NET_QUALITY_STALE,
                            NET_QUALITY_REASON_STALE, 56u, last_perm_meta.received_at_ms));
    /* Run B's next evaluation: revision 5. */
    step_ms(49u);
    RUN(permission_from(BOOT_B, 3u, 5u, 2u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 7u && uart_n == 5u);
    RUN(check_relay(&uart_out[4], 5u, 5u, NET_TRISTATE_YES, 2u, last_perm_meta.received_at_ms));
    /* DOMAIN restarts as run C with the same UPDATE, value and quality as the
     * last relay: an accepted run change is a relay event of its own (one
     * frame, revision 6). */
    step_ms(49u);
    RUN(permission_from(BOOT_C, 1u, 5u, 2u, NET_TRISTATE_YES, NET_QUALITY_OK));
    step_ms(1u);
    CHECK(perm_count == 8u && last_perm_meta.source.boot_id == BOOT_C && uart_n == 6u);
    RUN(check_relay(&uart_out[5], 6u, 6u, NET_TRISTATE_YES, 2u, last_perm_meta.received_at_ms));
    /* A new evaluation and its quality drop within one main pass: both reach
     * the application and both are relay events (revisions 7 and 8), but the
     * record not yet sent is replaced: one frame, revision 8, LINK_SEQUENCE
     * +1 only. */
    step_ms(49u);
    RUN(permission_from(BOOT_C, 2u, 6u, 3u, NET_TRISTATE_YES, NET_QUALITY_OK));
    RUN(permission_from(BOOT_C, 3u, 6u, 3u, NET_TRISTATE_YES, NET_QUALITY_INVALID));
    step_ms(1u);
    CHECK(perm_count == 10u && last_perm.power_permission_quality == NET_QUALITY_INVALID && uart_n == 7u);
    RUN(check_relay_quality(&uart_out[6], 7u, 8u, NET_TRISTATE_YES, NET_QUALITY_INVALID,
                            NET_QUALITY_REASON_DATA_INVALID, 3u, last_perm_meta.received_at_ms));
    step_ms(20u);
    CHECK(uart_n == 7u);
    for (i = 1u; i < uart_n; ++i)
        CHECK(uart_out[i].m.data.p_permission.update_sequence > uart_out[i - 1u].m.data.p_permission.update_sequence);
    d = diag();
    CHECK(d.pi_permission_tx.revision == 8u && d.app.relay_submitted == 8u && d.app.relay_refused == 0u);
    CHECK(d.pi_permission_tx.counters.replaced == 1u && d.pi_permission_tx.next_sequence == 8u);
    CHECK(d.app.permission_repeats == 1u && uart_bad == 0u && isr_callbacks == 0u);
    return 0;
}

/* Wait (main passes running) until the given ms time. */
static void run_until_ms(uint64_t at)
{
    while (now_ms() < at) step_ms(1u);
}

/* 02_NODES/CIS/20_API.md receive delivery (also 30_UART_C.md): a normal
 * repetition of the same Pi observation accepted with a new LINK_SEQUENCE
 * reaches the existing callback each time (it feeds the CIS receive
 * supervision, Result 100 / 300 ms and Status 1000 / 3000 ms), the original
 * observation number kept and the current AGE grown. A repetition that
 * arrives late, its AGE short of the previous AGE plus the receive gap by at
 * most the UART delivery allowance of 50 ms (04_COMMON/10_DATA_AND_ID_RULES.md
 * quality and AGE, candidate), is delivered with its AGE raised to that
 * bound, never lower; further short is held. An exact duplicate (same
 * LINK_SEQUENCE, same bytes) is not delivered. */
static int f23_vision_repeat_delivered(void)
{
    NetUpperUartMessage m;
    NetNodeDiag d0, d;
    uint64_t last, prev_at;
    uint16_t prev_age, sent_age, link = 1u;
    unsigned i, before;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    m = vision_message(link, 300u, 20u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u);
    prev_at = last_result_meta.received_at_ms;
    prev_age = 20u;
    d0 = diag();
    /* The Pi repeats the judgement every 100 ms, its AGE grown by 100. */
    for (i = 0u; i < 3u; ++i) {
        run_until_ms(prev_at + 96u);
        sent_age = (uint16_t)(prev_age + 100u);
        m = vision_message(++link, 300u, sent_age);
        RUN(line_send(&m, &last));
        step_ms(1u);
        CHECK(result_count == 2u + i);
        CHECK(last_result.vision_sequence == 300u && last_result.pi_boot_id == PI_BOOT);
        CHECK(last_result_meta.link_sequence == link && last_result_meta.received_at_ms == last);
        CHECK(last_result.vision_age_ms == sent_age);             /* on time: the AGE sent */
        CHECK(last_result.vision_age_ms >= prev_age + (last_result_meta.received_at_ms - prev_at));
        prev_at = last_result_meta.received_at_ms;
        prev_age = last_result.vision_age_ms;
    }
    d = diag();
    CHECK(d.upper.repeat_delivered == d0.upper.repeat_delivered + 3u);
    CHECK(d.upper.repeat_age_raised == d0.upper.repeat_age_raised);
    /* 30 ms late (AGE still +100): delivered, its AGE raised to the previous
     * AGE plus the receive gap, not below. */
    run_until_ms(prev_at + 126u);
    sent_age = (uint16_t)(prev_age + 100u);
    m = vision_message(++link, 300u, sent_age);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 5u && last_result_meta.link_sequence == link);
    CHECK(last_result_meta.received_at_ms - prev_at >= 128u);
    CHECK(last_result.vision_age_ms == prev_age + (last_result_meta.received_at_ms - prev_at));
    CHECK(last_result.vision_age_ms > sent_age);
    d = diag();
    CHECK(d.upper.repeat_age_raised == d0.upper.repeat_age_raised + 1u);
    CHECK(d.upper.repeat_delivered == d0.upper.repeat_delivered + 4u);
    prev_at = last_result_meta.received_at_ms;
    prev_age = last_result.vision_age_ms;
    /* Exact duplicate of that frame (same LINK_SEQUENCE, same bytes): not delivered. */
    d0 = diag();
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(result_count == 5u && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* 60 ms late: short by more than the allowance, held. */
    run_until_ms(prev_at + 156u);
    m = vision_message(++link, 300u, (uint16_t)(prev_age + 100u));
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(result_count == 5u && d.upper.rx_held == d0.upper.rx_held + 1u);
    /* P_VISION_STATUS: its repetition reaches the callback as well, late
     * within the allowance included; an exact duplicate does not. */
    m = vision_status_message(++link, 41u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(status_count == 1u);
    prev_at = last_status_meta.received_at_ms;
    prev_age = last_status.status_age_ms;
    before = status_count;
    for (i = 0u; i < 2u; ++i) {
        run_until_ms(prev_at + (i == 0u ? 96u : 126u));         /* on time, then 30 ms late */
        m = vision_status_message(++link, 41u);
        m.data.p_vision_status.status_age_ms = (uint16_t)(prev_age + 100u);
        RUN(line_send(&m, &last));
        step_ms(1u);
        CHECK(status_count == before + 1u + i);
        CHECK(last_status.status_sequence == 41u && last_status_meta.link_sequence == link);
        CHECK(last_status.status_age_ms >= prev_age + (last_status_meta.received_at_ms - prev_at));
        prev_at = last_status_meta.received_at_ms;
        prev_age = last_status.status_age_ms;
    }
    d0 = diag();
    RUN(line_send(&m, &last));
    step_ms(2u);
    d = diag();
    CHECK(status_count == before + 2u && d.upper.duplicates == d0.upper.duplicates + 1u);
    d = diag();
    CHECK(d.app.vision_result_delivered == 5u && d.app.vision_status_delivered == 3u);
    CHECK(isr_callbacks == 0u && can_n == 0u && uart_n == 0u);
    return 0;
}

/* 02_NODES/CIS/20_API.md CisNet_PublishOccupant ("when the quality of the same
 * original observation drops, the current quality and the actual restriction
 * are submitted with the original Pi value, BOOT / VISION_SEQUENCE and origin
 * kept"; a higher TX_SEQUENCE never allows a value change or INVALID -> VALID)
 * and 10_DATA.md (Pi original observation and CIS evaluation; the CIS
 * function state is the CIS's own): with the same Pi identity and values,
 * a validity / reason equal to the Pi's or more restricted (INVALID with the
 * CIS reason) is accepted and sent with that quality; a changed value or a
 * promoted quality can never be that relay (INVALID_ARGUMENT, nothing sent);
 * before any Pi judgement the record is NOT_READY (send return: original
 * context not ready). */
static int f24_occupant_restricted_quality(void)
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
    /* Before any Pi judgement. */
    memset(&occ, 0, sizeof(occ));
    occ.pi_boot_id = PI_BOOT;
    occ.vision_sequence = 900u;
    occ.vision_age_ms = 25u;
    occ.occupant_presence = NET_TRISTATE_YES;
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_DATA_INVALID;
    occ.occupant_count = 2u;
    occ.count_validity = NET_VALIDITY_VALID;
    occ.vision_function_status = NET_SENSOR_FUNCTION_ACTIVE;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_NOT_READY);
    /* Pi judgement 900: presence YES / count 2, both VALID. */
    m = vision_message(1u, 900u, 25u);
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 1u);
    /* The same judgement with presence restricted (INVALID, DATA_INVALID):
     * accepted and sent with that quality. */
    occ = relay_of(&last_result, 0u);
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_DATA_INVALID;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x310u, 0u) == 1u);
    k = can_find(0x310u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.pi_boot_id == PI_BOOT && orr.vision_sequence == 900u);
    CHECK(orr.occupant_presence == NET_TRISTATE_YES && orr.occupant_count == 2u);
    CHECK(orr.presence_validity == NET_VALIDITY_INVALID && orr.presence_reason == NET_QUALITY_REASON_DATA_INVALID);
    CHECK(orr.count_validity == NET_VALIDITY_VALID && orr.count_reason == NET_QUALITY_REASON_NONE);
    CHECK(orr.vision_age_ms == 25u + (can_out[k].at - last_result_meta.received_at_ms));
    /* Count restricted too, after a lost link (COMMUNICATION_FAULT), with the
     * CIS's own function state: accepted. */
    occ = relay_of(&last_result, 0u);
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_COMMUNICATION_FAULT;
    occ.count_validity = NET_VALIDITY_INVALID;
    occ.count_reason = NET_QUALITY_REASON_COMMUNICATION_FAULT;
    occ.vision_function_status = NET_SENSOR_FUNCTION_FAULT;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x310u, 0u) == 2u);
    k = can_find(0x310u, 0u, 1u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.count_validity == NET_VALIDITY_INVALID && orr.count_reason == NET_QUALITY_REASON_COMMUNICATION_FAULT);
    CHECK(orr.vision_function_status == NET_SENSOR_FUNCTION_FAULT && orr.occupant_count == 2u);
    /* A changed value under the same identity: never that relay. */
    occ = relay_of(&last_result, 0u);
    occ.occupant_count = 3u;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT);
    occ = relay_of(&last_result, 0u);
    occ.occupant_presence = NET_TRISTATE_UNKNOWN;
    occ.presence_validity = NET_VALIDITY_INVALID;
    occ.presence_reason = NET_QUALITY_REASON_NO_DATA;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT);
    step_ms(2u);
    CHECK(can_count(0x310u, 0u) == 2u);
    /* Pi judgement 901 with count INVALID (OUT_OF_RANGE). */
    m = vision_message(2u, 901u, 15u);
    m.data.p_vision_result.count_validity = NET_VALIDITY_INVALID;
    m.data.p_vision_result.count_reason = NET_QUALITY_REASON_OUT_OF_RANGE;
    RUN(line_send(&m, &last));
    step_ms(1u);
    CHECK(result_count == 2u && last_result.count_validity == NET_VALIDITY_INVALID);
    /* Promotion of count to VALID: never that relay. */
    occ = relay_of(&last_result, 0u);
    occ.count_validity = NET_VALIDITY_VALID;
    occ.count_reason = NET_QUALITY_REASON_NONE;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_INVALID_ARGUMENT);
    step_ms(2u);
    CHECK(can_count(0x310u, 0u) == 2u);
    /* The Pi quality as it is, then a CIS reason on the INVALID count: both accepted. */
    occ = relay_of(&last_result, 0u);
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x310u, 0u) == 3u);
    occ.count_reason = NET_QUALITY_REASON_STALE;
    CHECK(CisNet_PublishOccupant(&occ) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x310u, 0u) == 4u);
    k = can_find(0x310u, 0u, 3u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
    CHECK(orr.vision_sequence == 901u && orr.count_validity == NET_VALIDITY_INVALID);
    CHECK(orr.count_reason == NET_QUALITY_REASON_STALE && orr.presence_validity == NET_VALIDITY_VALID);
    d = diag();
    CHECK(d.app.occupant_not_current == 4u);
    CHECK(d.app.publish[NET_APP_PORT_CIS_OCCUPANT].not_accepted == 4u);
    CHECK(d.upper.rx_held == 0u && isr_callbacks == 0u);
    /* The application keeps its copy of the first delivery of judgement 903
     * while the Pi repeats it twice (receive delivery: the current AGE grows).
     * The restricted relay of that first copy is accepted: its AGE is not
     * below the AGE of the judgement's first receipt. The frame carries the
     * last repetition's AGE plus the time since that receipt. */
    {
        NetCisOccupant first;
        uint64_t prev_at;
        uint16_t prev_age = 30u, link = 3u;
        unsigned n;
        m = vision_message(link, 903u, prev_age);
        RUN(line_send(&m, &last));
        step_ms(1u);
        CHECK(result_count == 3u && last_result.vision_sequence == 903u);
        first = relay_of(&last_result, 0u);
        prev_at = last_result_meta.received_at_ms;
        for (n = 0u; n < 2u; ++n) {
            run_until_ms(prev_at + 96u);
            prev_age = (uint16_t)(prev_age + 100u);
            m = vision_message(++link, 903u, prev_age);
            RUN(line_send(&m, &last));
            step_ms(1u);
            CHECK(result_count == 4u + n && last_result.vision_age_ms == prev_age);
            prev_at = last_result_meta.received_at_ms;
        }
        CHECK(first.vision_age_ms == 30u && last_result.vision_age_ms == 230u);
        first.presence_validity = NET_VALIDITY_INVALID;
        first.presence_reason = NET_QUALITY_REASON_DATA_INVALID;
        CHECK(CisNet_PublishOccupant(&first) == NET_SUBMIT_OK);
        step_ms(1u);
        CHECK(can_count(0x310u, 0u) == 5u);
        k = can_find(0x310u, 0u, 4u);
        CHECK(k >= 0);
        e = envelope(&can_out[k].f);
        CHECK(net_wire_can_decode_cis_occupant(NET_WIRE_CANDIDATE_ONLY, &e, &orr) == NET_WIRE_OK);
        CHECK(orr.vision_sequence == 903u && orr.presence_validity == NET_VALIDITY_INVALID);
        CHECK(orr.vision_age_ms == 230u + (can_out[k].at - prev_at));
        /* A record below the first receipt's AGE is still refused. */
        first.vision_age_ms = 29u;
        CHECK(CisNet_PublishOccupant(&first) == NET_SUBMIT_INVALID_ARGUMENT);
        d = diag();
        CHECK(d.upper.rx_held == 0u && d.app.occupant_not_current == 5u);
    }
    return 0;
}

/* P_VISION_STATUS keeps one state evaluation for seconds and P_VISION_RESULT
 * may repeat one judgement for seconds (30_UART_C.md: STATUS_SEQUENCE /
 * VISION_SEQUENCE grow only on a new evaluation / judgement and repetitions
 * keep the original AGE basis), so a repetition's AGE passes 2000 ms. Each
 * repetition still reaches the existing callback (02_NODES/CIS/20_API.md
 * receive delivery: it feeds the CIS receive supervision, 300 / 3000 ms); the
 * use of an old observation is the CIS application's decision. The same
 * status is repeated every 1000 ms five times and the same judgement every
 * 100 ms for 3 s: every one is delivered with its AGE, nothing is held. */
static int f25_vision_repeat_long_age(void)
{
    NetUpperUartMessage m;
    NetNodeDiag d0, d;
    uint64_t last, start;
    uint16_t link = 0u;
    unsigned t, results = 0u, statuses = 0u;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    d0 = diag();
    start = now_ms();
    for (t = 0u; t <= 4000u; t += 100u) {
        run_until_ms(start + t);
        if (t < 3000u) {
            /* Judgement 500 again, its AGE grown by the 100 ms. */
            m = vision_message(++link, 500u, (uint16_t)(20u + t));
            RUN(line_send(&m, &last));
            step_ms(1u);
            ++results;
            CHECK(result_count == results && last_result.vision_sequence == 500u);
            /* The AGE sent, or raised by a ms when the receive gap rounds up. */
            CHECK(last_result.vision_age_ms >= 20u + t && last_result.vision_age_ms <= 22u + t);
            CHECK(last_result_meta.link_sequence == link);
        }
        if (t % 1000u == 0u) {
            /* Status evaluation 77 again, its AGE grown by the 1000 ms. */
            m = vision_status_message(++link, 77u);
            m.data.p_vision_status.status_age_ms = (uint16_t)(40u + t);
            RUN(line_send(&m, &last));
            step_ms(1u);
            ++statuses;
            CHECK(status_count == statuses && last_status.status_sequence == 77u);
            CHECK(last_status.status_age_ms >= 40u + t && last_status.status_age_ms <= 42u + t);
            CHECK(last_status_meta.link_sequence == link);
        }
    }
    CHECK(results == 30u && statuses == 5u);
    CHECK(last_result.vision_age_ms >= 2920u && last_status.status_age_ms >= 4040u);
    d = diag();
    CHECK(d.upper.rx_held == d0.upper.rx_held && d.upper.rx_held == 0u);
    CHECK(d.upper.repeat_delivered == d0.upper.repeat_delivered + 29u + 4u);
    CHECK(d.app.vision_result_delivered == 30u && d.app.vision_status_delivered == 5u);
    CHECK(isr_callbacks == 0u && can_n == 0u && uart_n == 0u);
    return 0;
}

/* ------------------------------------------------ documented traffic ---- */
/* Ten seconds of the documented traffic (net_load_traffic.h):
 *   CAN    : the periodic frames of DOMAIN, BCM and VSS as their senders put
 *            them on the bus (10_CAN_FD.md message list, phase 0: seven of
 *            them start together every second). The RX message buffers
 *            accept every ID, so all of them enter the CIS receive path; the
 *            routes of the other nodes end in the registry, not as a loss.
 *   UART-C : the Pi sends P_VISION_RESULT (29 B) every 100 ms and
 *            P_VISION_STATUS (26 B) every 1 s, back to back, 87 us a byte
 *            (30_UART_C.md message list, 115200 bit/s).
 *   App    : CIS_REAR, CIS_ENVIRONMENT and CIS_STATUS at their 10_CAN_FD.md
 *            periods, in the pass that reaches the due time, and CIS_OCCUPANT
 *            for every Pi judgement delivered (100 ms), after NetNode_Step();
 *            the next pass sends them.
 *   Main   : a pass every 10 ms, every tenth one 7 ms late (17 ms).
 * At an instant shared by a pass and bus frames the pass runs first, so those
 * frames wait a whole pass. Nothing may be lost (ISR rings, adapter and
 * service queues, M04 slots, every frame for the CIS delivered), an own frame
 * leaves within its period plus one pass (17 ms), P_PERMISSION within its
 * 500 ms period plus one pass, and no queue goes past its capacity. A frame
 * that found no device room waits one pass in its M04 slot (queue_full) and
 * keeps its number: a delay, not a loss. The LOAD line reports the counts and
 * the high-water marks against the capacities of this build.
 * Second boundary (load_second_boundary): the same traffic with the
 * application before the Network pass and the Pi judgements 12 ms before a
 * whole second, so the CIS_OCCUPANT of a judgement, the other CIS CAN
 * messages due at that second and the P_PERMISSION relay of a new DOMAIN
 * evaluation want more device room in one pass than NET_HAL_TX_CAPACITY: one
 * frame waits a pass (queue_full >= 1), and still nothing is lost, no
 * TX_SEQUENCE or LINK_SEQUENCE is skipped and every period holds. Its line
 * starts with LOAD-BOUNDARY. */
enum { LOAD_OWN = 4, LOAD_QUEUE = 128 };
#define LOAD_PASS_US 10000u
#define LOAD_LATE_US 7000u
#define LOAD_SLACK_MS 17u
#define LOAD_FLUSH_US 100000u
#define LOAD_PI_PERMISSION_PERIOD_MS 500u
#define LOAD_BOUNDARY_PI_EARLY_US (1000000u - 12000u)

static NetLoadTx load_tx[LOAD_OWN];
static uint64_t load_app_due[LOAD_OWN];
static uint32_t load_app_sequence[LOAD_OWN];
static bool load_app_on;
static bool load_app_first;              /* the application before the Network pass */
static unsigned load_occupant_results;   /* Pi judgements already relayed */
static unsigned load_app_submitted, load_app_refused, load_foreign_tx, load_inject_lost;
static unsigned load_relays, load_relay_bad, load_relay_link_gaps, load_relay_violations;
static uint64_t load_relay_last_ms;
static uint16_t load_relay_last_link;
static uint8_t load_q_byte[LOAD_QUEUE];
static uint64_t load_q_at[LOAD_QUEUE];
static unsigned load_q_head, load_q_tail;
static uint64_t load_line_free_us;

static void load_init_own(void)
{
    size_t k;
    unsigned j = 0u;
    for (k = 0u; k < NET_LOAD_MESSAGE_COUNT && j < LOAD_OWN; ++k) {
        if (net_load_messages[k].sender != NET_LOAD_CIS) continue;
        net_load_tx_init(&load_tx[j], net_load_messages[k].id, net_load_messages[k].period_ms, LOAD_SLACK_MS);
        load_app_due[j] = 0u;
        load_app_sequence[j] = 0u;
        ++j;
    }
}

static NetSubmitResult load_submit(uint16_t id, uint32_t n)
{
    switch (id) {
    case 0x0A0u: { NetCisRear r = rear_sample(n, 5u); return CisNet_PublishRear(&r); }
    case 0x300u: { NetCisEnvironment v = environment_sample(n, 40u); return CisNet_PublishEnvironment(&v); }
    case 0x310u: { NetCisOccupant o = relay_of(&last_result, 0u); return CisNet_PublishOccupant(&o); }
    default: { NetCisStatus s = status_ready(n, 3u); return CisNet_PublishStatus(&s); }
    }
}

/* The CIS application in the superloop: what is due is submitted now.
 * CIS_OCCUPANT is the occupant update of a Pi judgement (02_NODES/CIS/
 * 20_API.md: linked to the Pi original BOOT / VISION_SEQUENCE / AGE), so it
 * follows P_VISION_RESULT: one relay per judgement delivered. */
static void load_app(uint64_t now)
{
    unsigned j;
    if (!load_app_on) return;
    for (j = 0u; j < LOAD_OWN; ++j) {
        if (load_tx[j].id == 0x310u) {
            if (result_count == load_occupant_results) continue;
            load_occupant_results = result_count;
        } else {
            if (now < load_app_due[j]) continue;
            load_app_due[j] += load_tx[j].period_ms;
        }
        ++load_app_submitted;
        if (load_submit(load_tx[j].id, ++load_app_sequence[j]) != NET_SUBMIT_OK) ++load_app_refused;
    }
}

/* The far ends take what the node sent: CAN frames (TX_COMPLETE) and the
 * P_PERMISSION relay on UART-C (END_TRANSFER). */
static void load_collect(void)
{
    MockCanFrame f;
    NetWireCanEnvelope e;
    uint8_t bytes[NET_WIRE_UART_FRAME_MAX];
    uint32 n;
    uint64_t now = now_ms();
    in_isr = true;
    while (MockRtd_CanTransmitNext(BUS, &f) >= 0) {
        e = envelope(&f);
        if (!net_load_tx_seen(load_tx, LOAD_OWN, &e, now)) ++load_foreign_tx;
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
        if (r == NULL || net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_C, r->direction, &w, &m) != NET_WIRE_OK ||
            m.type != 0x62u) {
            ++load_relay_bad;
            return;
        }
        /* LINK_SEQUENCE of the CIS -> Pi direction: from 1, +1 per frame. */
        if (m.sequence != (load_relays == 0u ? 1u : (uint16_t)(load_relay_last_link + 1u))) ++load_relay_link_gaps;
        load_relay_last_link = m.sequence;
        if (load_relays > 0u && now - load_relay_last_ms > LOAD_PI_PERMISSION_PERIOD_MS + LOAD_SLACK_MS)
            ++load_relay_violations;
        load_relay_last_ms = now;
        ++load_relays;
    }
}

/* One superloop pass in the documented order (net_node.h,
 * VSS_NODE_INTEGRATION.md): the Network pass, then the application. The
 * second-boundary case runs the application first, so what it submits goes
 * out in the same pass. */
static void load_pass(void)
{
    if (load_app_first) load_app(now_ms());
#ifdef CIS_NODE_TEST_ENTRY
    Net_MainFunction((uint32_t)now_ms());
#else
    NetNode_Step();
#endif
    load_collect();
    if (!load_app_first) load_app(now_ms());
}

/* Frame n of message k on the bus (ISR context of the CAN controller). */
static int load_inject(size_t k, uint32_t n)
{
    NetWireCanEnvelope e;
    MockCanFrame f;
    CHECK(net_load_frame(k, n, &e));
    memset(&f, 0, sizeof(f));
    f.id = e.standard_id; f.fd = e.fdf; f.brs = e.brs; f.dlc = e.dlc; f.length = (uint8)e.length;
    memcpy(f.data, e.bytes, e.length);
    in_isr = true;
    if (MockRtd_CanInject(BUS, &f) < 0) ++load_inject_lost;
    in_isr = false;
    return 0;
}

/* A Pi frame due at due_us goes on the line once the line is free; each byte
 * arrives 87 us after the previous one. */
static int load_line_queue(const NetUpperUartMessage *m, uint64_t due_us)
{
    NetWireUartFrame f;
    size_t i;
    uint64_t start;
    memset(&f, 0, sizeof(f));
    CHECK(net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY, m, &f) == NET_WIRE_OK);
    start = due_us > load_line_free_us ? due_us : load_line_free_us;
    for (i = 0u; i < f.length; ++i) {
        CHECK(load_q_tail - load_q_head < LOAD_QUEUE);
        load_q_byte[load_q_tail % LOAD_QUEUE] = f.bytes[i];
        load_q_at[load_q_tail % LOAD_QUEUE] = start + (uint64_t)(i + 1u) * BYTE_US;
        ++load_q_tail;
    }
    load_line_free_us = start + (uint64_t)f.length * BYTE_US;
    return 0;
}

/* A frame that found no device room waits one pass in its slot (queue_full)
 * and keeps its number: a delay, counted apart. Lost is what never left. */
static uint32_t load_slot_lost(const NetUpperPublishView *v)
{
    return v->counters.not_sent + v->counters.tx_failed + v->counters.replaced;
}

static uint32_t load_missing(uint32_t expected, uint32_t delivered)
{
    return expected > delivered ? expected - delivered : 0u;
}

static int load_run(bool app_first)
{
    const uint64_t end_us = (uint64_t)NET_LOAD_DURATION_MS * 1000u;
    uint32_t sent[NET_LOAD_MESSAGE_COUNT];
    uint64_t due_us[NET_LOAD_MESSAGE_COUNT];
    /* Second boundary: Pi judgements 12 ms early, so the occupant of a
     * judgement and the CAN messages due at a whole second go in one pass. */
    const uint64_t pi_offset_us = app_first ? LOAD_BOUNDARY_PI_EARLY_US : 0u;
    uint64_t pass_at = LOAD_PASS_US, pi_at = pi_offset_us, t;
    uint32_t passes = 0u, results = 0u, statuses = 0u, permissions = 0u, injected = 0u;
    uint32_t lost, violations, tx_frames;
    uint16_t link = 0u;
    size_t k;
    NetNodeDiag d;
    NetUpperUartMessage m;
    CHECK(start_node() == NET_OK);
    load_init_own();
    load_app_first = app_first;
    load_app_on = true;
    for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) {
        sent[k] = 0u;
        due_us[k] = net_load_messages[k].sender == NET_LOAD_CIS ? UINT64_MAX : 0u;
    }
    for (;;) {
        t = pass_at;
        for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) if (due_us[k] < t) t = due_us[k];
        if (pi_at < t) t = pi_at;
        if (load_q_head != load_q_tail && load_q_at[load_q_head % LOAD_QUEUE] < t) t = load_q_at[load_q_head % LOAD_QUEUE];
        if (t >= end_us) break;
        MockRtd_AdvanceUs(t - MockRtd_NowUs());
        if (t == pass_at) {
            load_pass();
            ++passes;
            pass_at += (passes % 10u == 9u) ? LOAD_PASS_US + LOAD_LATE_US : LOAD_PASS_US;
        }
        for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) {
            if (due_us[k] != t) continue;
            RUN(load_inject(k, sent[k]++));
            ++injected;
            if (net_load_messages[k].id == 0x080u) ++permissions;
            due_us[k] += (uint64_t)net_load_messages[k].period_ms * 1000u;
        }
        if (t == pi_at) {
            m = vision_message(++link, ++results, 20u);
            RUN(load_line_queue(&m, t));
            if ((t - pi_offset_us) % 1000000u == 0u) {
                m = vision_status_message(++link, ++statuses);
                RUN(load_line_queue(&m, t));
            }
            pi_at += 100000u;
        }
        while (load_q_head != load_q_tail && load_q_at[load_q_head % LOAD_QUEUE] <= t) {
            in_isr = true;
            if (MockRtd_UartInject(LPUART, &load_q_byte[load_q_head % LOAD_QUEUE], 1u) != 1u) ++load_inject_lost;
            in_isr = false;
            ++load_q_head;
        }
    }
    /* No new traffic: the passes take what still waits. */
    load_app_on = false;
    while (pass_at <= end_us + LOAD_FLUSH_US) {
        MockRtd_AdvanceUs(pass_at - MockRtd_NowUs());
        load_pass();
        ++passes;
        pass_at += (passes % 10u == 9u) ? LOAD_PASS_US + LOAD_LATE_US : LOAD_PASS_US;
    }
    d = diag();
    {
        const NetLoadCount loss[] = {
            { "upper.rx_dropped", d.upper.rx_dropped },
            { "hal.rx_dropped", d.hal.rx_dropped },
            { "rtd.isr_ring_overflow", d.rtd.isr_ring_overflow },
            { "rtd.rx_overruns", d.rtd.rx_overruns },
            { "rtd.rx_queue_rejected", d.rtd.rx_queue_rejected },
            { "rtd.uart[1].uart_isr_ring_overflow", d.rtd.uart[1].uart_isr_ring_overflow },
            { "rtd.uart[1].uart_rx_overrun", d.rtd.uart[1].uart_rx_overrun },
            { "rtd.uart[1].uart_rx_queue_rejected", d.rtd.uart[1].uart_rx_queue_rejected },
            { "line_bytes_not_taken", MockRtd_UartRxLost(LPUART) },
            { "bus_frames_not_taken", load_inject_lost },
            { "app_refused", load_app_refused },
            { "rear_tx", load_slot_lost(&d.rear_tx) },
            { "environment_tx", load_slot_lost(&d.environment_tx) },
            { "occupant_tx", load_slot_lost(&d.occupant_tx) },
            { "status_tx", load_slot_lost(&d.status_tx) },
            { "pi_permission_tx", load_slot_lost(&d.pi_permission_tx) },
            { "permission_not_delivered", load_missing(permissions, d.app.permission_delivered) },
            { "vision_result_not_delivered", load_missing(results, d.app.vision_result_delivered) },
            { "vision_status_not_delivered", load_missing(statuses, d.app.vision_status_delivered) },
            { "tx_sequence_gaps", net_load_tx_gaps(load_tx, LOAD_OWN) },
            { "relay_link_sequence_gaps", load_relay_link_gaps },
            { "relay_frames_bad", load_relay_bad }
        };
        const NetLoadCount late[] = {
            { "can_periods", net_load_tx_violations(load_tx, LOAD_OWN, NET_LOAD_DURATION_MS) },
            { "relay_periods", load_relay_violations },
            { "relay_none", load_relays == 0u ? 1u : 0u },
            { "relay_end", load_relays > 0u && NET_LOAD_DURATION_MS - load_relay_last_ms >
                               LOAD_PI_PERMISSION_PERIOD_MS + LOAD_SLACK_MS ? 1u : 0u }
        };
        lost = net_load_report("load lost", loss, sizeof(loss) / sizeof(loss[0]));
        violations = net_load_report("load late", late, sizeof(late) / sizeof(late[0]));
    }
    tx_frames = net_load_tx_frames(load_tx, LOAD_OWN);
    printf("%s CIS rx_frames=%lu tx_frames=%lu lost=%lu period_violations=%lu can_ring_max=%lu/%lu "
           "hal_rx_max=%lu/%lu upper_rx_max=%lu/%lu upper_tx_max=%lu/%lu uart_ring_max=%lu/%lu\n",
           app_first ? "LOAD-BOUNDARY" : "LOAD", (unsigned long)d.rtd.rx_frames, (unsigned long)tx_frames, (unsigned long)lost, (unsigned long)violations,
           (unsigned long)d.rtd.isr_ring_max_depth, (unsigned long)NET_BOARD_CAN_ISR_RING,
           (unsigned long)d.hal.rx_max_depth, (unsigned long)NET_HAL_RX_CAPACITY,
           (unsigned long)d.upper.rx_max_depth, (unsigned long)NET_UPPER_RX_CAPACITY,
           (unsigned long)d.upper.tx_max_depth, (unsigned long)NET_UPPER_TX_CAPACITY,
           (unsigned long)d.rtd.uart[1].uart_isr_ring_max_depth, (unsigned long)NET_BOARD_UART_ISR_RING);
    printf("load detail: passes=%lu injected=%lu permission=%lu/%lu vision_result=%lu/%lu vision_status=%lu/%lu "
           "submitted=%u refused=%u relays=%u drained_max=%lu/%lu ring_overflow=%lu hal_dropped=%lu "
           "upper_dropped=%lu rx_invalid=%lu rx_held=%lu queue_full=%lu,%lu,%lu,%lu,%lu max_gap_ms=%lu,%lu,%lu,%lu\n",
           (unsigned long)passes, (unsigned long)injected, (unsigned long)d.app.permission_delivered,
           (unsigned long)permissions, (unsigned long)d.app.vision_result_delivered, (unsigned long)results,
           (unsigned long)d.app.vision_status_delivered, (unsigned long)statuses, load_app_submitted, load_app_refused,
           load_relays, (unsigned long)d.rtd.main_drained_max, (unsigned long)d.rtd.isr_drain_budget,
           (unsigned long)d.rtd.isr_ring_overflow, (unsigned long)d.hal.rx_dropped, (unsigned long)d.upper.rx_dropped,
           (unsigned long)d.upper.rx_invalid, (unsigned long)d.upper.rx_held,
           (unsigned long)d.rear_tx.counters.queue_full, (unsigned long)d.environment_tx.counters.queue_full,
           (unsigned long)d.occupant_tx.counters.queue_full, (unsigned long)d.status_tx.counters.queue_full,
           (unsigned long)d.pi_permission_tx.counters.queue_full,
           (unsigned long)load_tx[0].max_gap_ms, (unsigned long)load_tx[1].max_gap_ms,
           (unsigned long)load_tx[2].max_gap_ms, (unsigned long)load_tx[3].max_gap_ms);
    fflush(stdout);
    CHECK(d.rtd.isr_ring_capacity == NET_BOARD_CAN_ISR_RING);
    CHECK(d.rtd.rx_frames == injected);       /* every bus frame entered the receive path */
    CHECK(tx_frames == load_app_submitted);   /* one frame per submission */
    CHECK(d.app.permission_delivered <= permissions && d.app.vision_result_delivered <= results);
    CHECK(d.app.vision_status_delivered <= statuses);
    CHECK(load_foreign_tx == 0u && isr_callbacks == 0u);
    CHECK(lost == 0u);
    CHECK(violations == 0u);
    CHECK(d.rtd.isr_ring_max_depth <= NET_BOARD_CAN_ISR_RING && d.hal.rx_max_depth <= NET_HAL_RX_CAPACITY);
    CHECK(d.upper.rx_max_depth <= NET_UPPER_RX_CAPACITY && d.upper.tx_max_depth <= NET_UPPER_TX_CAPACITY);
    CHECK(d.rtd.uart[1].uart_isr_ring_max_depth <= NET_BOARD_UART_ISR_RING);
    /* The high-water marks saw the traffic: seven frames of the bus at once,
     * the four CAN messages of the CIS submitted in one pass. */
    CHECK(d.rtd.isr_ring_max_depth >= 7u && d.upper.tx_max_depth >= 4u);
    CHECK(d.hal.rx_max_depth >= 1u && d.upper.rx_max_depth >= 1u && d.rtd.uart[1].uart_isr_ring_max_depth >= 1u);
    if (app_first) {
        /* The overlap happened: in some pass the four CAN messages and the
         * P_PERMISSION relay wanted more device room than NET_HAL_TX_CAPACITY,
         * one frame waited a pass (no loss, no number gap, periods kept). */
        CHECK(d.rear_tx.counters.queue_full + d.environment_tx.counters.queue_full + d.occupant_tx.counters.queue_full +
              d.status_tx.counters.queue_full + d.pi_permission_tx.counters.queue_full >= 1u);
    }
    return 0;
}

static int load_doc_traffic(void)
{
    return load_run(false);
}

/* The application before the Network pass: at the second boundary the CIS
 * CAN messages and a new DOMAIN evaluation meet in one pass. */
static int load_second_boundary(void)
{
    return load_run(true);
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
        { "f12b_busoff_recovery", f12b_busoff_recovery },
#ifdef CIS_NODE_TEST_ENTRY
        { "f13_entry", f13_entry },
#endif
        { "f14_permission_repeat_delivered", f14_permission_repeat_delivered },
        { "f15_permission_quality_drop_same_update", f15_permission_quality_drop_same_update },
        { "f16_vision_rx_main_3ms", f16_vision_rx_main_3ms },
        { "f17_domain_reboot", f17_domain_reboot },
        { "f18_pi_reboot", f18_pi_reboot },
        { "f19_permission_repeat_no_flood", f19_permission_repeat_no_flood },
        { "f20_permission_tx_zero", f20_permission_tx_zero },
        { "f21_permission_quality_drop_relayed", f21_permission_quality_drop_relayed },
        { "f22_relay_sequence_cis_owned", f22_relay_sequence_cis_owned },
        { "f23_vision_repeat_delivered", f23_vision_repeat_delivered },
        { "f24_occupant_restricted_quality", f24_occupant_restricted_quality },
        { "load_doc_traffic", load_doc_traffic },
        { "load_second_boundary", load_second_boundary },
        { "f25_vision_repeat_long_age", f25_vision_repeat_long_age }
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
