/* LPUART part of the RTD backend end to end on the host: the real product
 * sources (RTD backend -> NetHalAdapter -> NetUpper -> typed ECU adapter)
 * linked with the independently written mock SDK, whose LPUART virtual line
 * plays the peer (bytes in through RX_FULL, bytes out of AsyncSend).
 * Contract: docs/network/interface/03_COMMUNICATION/20_UART_M.md (framing,
 *           CRC, LINK_SEQUENCE), 30_UART_C.md (Pi <-> CIS messages).
 * Frames are built and checked with the repository codec (net_upper_uart_*).
 * One case per process. Builds of this file:
 *   CIS profile (UART-C, LPUART11), OsIf counter; HAL defines the callback
 *   CIS, routing (UART_HAL_TEST_ROUTE): this file is the board callback
 *   CIS, application tick hooks (UART_HAL_TEST_TIME_TICK)
 *   CIS, NET_BOARD_NOW_MS_HOOK (UART_HAL_TEST_TIME_HOOK)
 *   DOMAIN profile (UART-M), OsIf counter
 * The node root for these profiles does not exist yet: the composition below
 * is the minimum one (backend, adapter, NetUpper, typed ECU adapter). */
#include "network/net_hal_rtd.h"
#include "network/net_upper_control.h"
#include "network/net_ecu_adapter.h"
#include "network/net_wire_can.h"
#include "net_upper_private.h"
#include "net_hal_rtd_uart.h"
#include "Lpuart_Uart_Ip_Sa_PBcfg.h"
#include "mock_rtd.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define RUN(x) do { if ((x) != 0) return 1; } while (0)

static const char *case_name;

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#define NODE    NET_WIRE_NODE_CIS
#define LINK    NET_WIRE_UART_C
#define LPUART  11u       /* net_board_cfg.h default (DS344 LPUART11) */
#define DIAG    1u
#define RX_TYPE 0x60u     /* P_VISION_RESULT, PI -> CIS */
#define TX_TYPE 0x62u     /* P_PERMISSION, CIS -> PI */
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
#define NODE    NET_WIRE_NODE_DOMAIN
#define LINK    NET_WIRE_UART_M
#define LPUART  0u        /* net_board_cfg.h placeholder default */
#define DIAG    0u
#define RX_TYPE 0x01u     /* M_CONTEXT, ESP32 -> DOMAIN */
#define TX_TYPE 0x02u     /* M_DOMAIN_ALIVE, DOMAIN -> ESP32 */
#else
#error "test_uart_hal_e2e: CIS or DOMAIN profile"
#endif

enum { CAN_BUS = 0, PI_BOOT = 0x50490001u, CIS_BOOT = 0x43495301u, DOMAIN_BOOT = 0x444F4D31u };

/* ------------------------------------------------------------ time ----- */
#if defined(UART_HAL_TEST_TIME_TICK)
/* Application clock: 1 tick = 1.25 ms (as the VSS timer), identity-free conversion. */
uint32_t MockBoard_IsrTicks(void);
uint32_t MockBoard_TickToMs(uint32_t tick);
uint32_t MockBoard_IsrTicks(void) { return (uint32_t)(MockRtd_NowUs() / 1250u); }
uint32_t MockBoard_TickToMs(uint32_t tick) { return (uint32_t)(((uint64_t)tick * 5u) / 4u); }
static uint64_t line_ms(void) { return MockBoard_TickToMs(MockBoard_IsrTicks()); }
#define TIMER_ON false
#elif defined(UART_HAL_TEST_TIME_HOOK)
uint64_t MockBoard_NowMs(void);
uint64_t MockBoard_NowMs(void) { return MockRtd_NowUs() / 1000u; }
static uint64_t line_ms(void) { return MockRtd_NowUs() / 1000u; }
#define TIMER_ON false
#else
static uint64_t line_ms(void) { return MockRtd_NowUs() / 1000u; }
#define TIMER_ON true
#endif

#define PASS_US 1000u                /* main pass period */
static uint64_t next_pass_us = PASS_US;

/* ------------------------------------------- ISR context bookkeeping ---- */
static bool in_isr;
static unsigned isr_callbacks;      /* typed callbacks seen while an "ISR" ran */

#ifdef UART_HAL_TEST_ROUTE
/* Board code of the routing mode (DS344: LPUART0 console and LPUART11 share
 * the generated callback): the console stays here, LPUART11 goes to Network. */
static unsigned board_console_events;
static uint8 console_byte;
void Uart344_BringupCallback(const uint8 HwInstance, const Lpuart_Uart_Ip_EventType Event, const void *UserData);
void Uart344_BringupCallback(const uint8 HwInstance, const Lpuart_Uart_Ip_EventType Event, const void *UserData)
{
    (void)UserData;
    if (HwInstance == LPUART) { NetHalRtdUart_OnEvent(HwInstance, Event); return; }
    ++board_console_events;
    if (Event == LPUART_UART_IP_EVENT_RX_FULL) Lpuart_Uart_Ip_SetRxBuffer(HwInstance, &console_byte, 1U);
}
#endif

/* ---------------------------------------------------- typed callbacks ---- */
static unsigned rx_count, perm_count;
static NetUpperUartMessage rx_last;
static NetUpperDelivery rx_meta;

static NetStatus note_rx(uint8_t type, const NetUpperDelivery *d)
{
    if (in_isr) ++isr_callbacks;
    rx_last.link = LINK;
    rx_last.type = type;
    rx_last.sequence = (uint16_t)d->source_sequence;
    rx_meta = *d;
    ++rx_count;
    return NET_OK;
}
static NetStatus on_vision_result(void *c, const NetWirePVisionResult *v, const NetUpperDelivery *d)
{ (void)c; memset(&rx_last, 0, sizeof(rx_last)); rx_last.data.p_vision_result = *v; return note_rx(0x60u, d); }
static NetStatus on_vision_status(void *c, const NetWirePVisionStatusRaw *v, const NetUpperDelivery *d)
{ (void)c; memset(&rx_last, 0, sizeof(rx_last)); rx_last.data.p_vision_status = *v; return note_rx(0x61u, d); }
static NetStatus on_m_context(void *c, const NetWireMContext *v, const NetUpperDelivery *d)
{ (void)c; memset(&rx_last, 0, sizeof(rx_last)); rx_last.data.m_context = *v; return note_rx(0x01u, d); }
static NetStatus on_permission(void *c, const NetWireDomainPermissionRaw *v, const NetUpperDelivery *d)
{ (void)c; (void)v; (void)d; if (in_isr) ++isr_callbacks; ++perm_count; return NET_OK; }

/* ------------------------------------------------- node composition ---- */
static NetUpper service;
static NetHalAdapter hal;
static NetEcuAdapter ecu;
static NetEcuCallbacks ops;

static NetStatus start(void)
{
    NetHalRtdConfig in;
    NetHalConfig hc;
    NetUpperConfig c;
    NetStatus st;
    size_t i;
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(TIMER_ON);
    next_pass_us = PASS_US;
    memset(&in, 0, sizeof(in));
    in.can_port_epoch = 1u;
    in.uart_m_port_epoch = 1u;
    in.uart_c_port_epoch = 1u;
    st = net_hal_rtd_config(&hal, &in, &hc);
    if (st != NET_OK) return st;
    st = net_s32k344_upper_port_init(&hal, &hc);
    if (st != NET_OK) return st;
    memset(&ops, 0, sizeof(ops));
    ops.domain_permission = on_permission;
    ops.p_vision_result = on_vision_result;
    ops.p_vision_status = on_vision_status;
    ops.m_context = on_m_context;
    st = net_ecu_adapter_init(&ecu, &service, NODE, &ops, NULL);
    if (st != NET_OK) return st;
    memset(&c, 0, sizeof(c));
    c.node = NODE;
    c.local_epoch = 1u;
    for (i = 0u; i < 3u; ++i) c.port_epoch[i] = 1u;
    c.profile = NET_WIRE_CANDIDATE_ONLY;
    c.port = net_hal_adapter_port(&hal);
    c.owner = net_ecu_adapter_owner(&ecu);
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        c.rx_policy[i].bound = true;
        c.rx_policy[i].maximum_age_ms = 2000u;
        c.rx_policy[i].expires_at_equality = true;
        if (r != NULL && r->link == NET_WIRE_CAN && r->id_type == 0x080u) c.rx_policy[i].maximum_age_ms = 500u;
    }
    /* 20_UART_M.md: 50 ms assembly bound (proposed); LINK_SEQUENCE from 1. */
    c.uart_policy[LINK - 1u].candidate_time_policy_selected = true;
    c.uart_policy[LINK - 1u].assembly_limit_ms = 50u;
    c.uart_policy[LINK - 1u].expires_at_equality = true;
    c.uart_sequence_bound[LINK - 1u] = true;
    c.uart_initial_sequence[LINK - 1u] = 1u;
    st = net_upper_init(&service, &c);
    if (st != NET_OK) return st;
    return net_hal_rtd_start();
}

/* One main pass, the order a node root uses (ISR ring first). */
static void step(void)
{
    (void)net_hal_rtd_set_now(line_ms());
    (void)net_hal_rtd_drain();
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET);
    net_hal_rtd_service();
    (void)net_upper_step(&service, NET_UPPER_POLL_BUDGET);
}

/* Superloop: one main pass at every 1 ms boundary while time advances. */
static void advance_us(uint64_t us)
{
    uint64_t end = MockRtd_NowUs() + us;
    while (next_pass_us <= end) {
        MockRtd_AdvanceUs(next_pass_us - MockRtd_NowUs());
        step();
        next_pass_us += PASS_US;
    }
    MockRtd_AdvanceUs(end - MockRtd_NowUs());
}

static void step_ms(unsigned ms) { advance_us((uint64_t)ms * 1000u); }

/* Main loop held up (no pass) for us: the ISRs keep running. */
static void stall_us(uint64_t us)
{
    MockRtd_AdvanceUs(us);
    next_pass_us = (MockRtd_NowUs() / PASS_US + 1u) * PASS_US;
}

static void dbg_dump(void);
static bool run_until_rx(unsigned want, unsigned max_ms)
{
    while (rx_count < want && max_ms-- > 0u) step_ms(1u);
    if (rx_count < want) dbg_dump();
    return rx_count >= want;
}

static NetHalRtdDiag rdiag(void)
{
    NetHalRtdDiag d;
    memset(&d, 0, sizeof(d));
    (void)net_hal_rtd_diag(&d);
    return d;
}

static NetUpperObservation observation(void)
{
    NetUpperObservation o;
    memset(&o, 0, sizeof(o));
    (void)net_upper_observation(&service, &o);
    return o;
}

static const NetWireUartStream *stream(void) { return &net_upper_receiver(&service, LINK - 1u)->stream; }

/* Counters printed when a frame did not arrive in time. */
static void dbg_dump(void)
{
    NetUpperObservation o = observation();
    NetHalRtdDiag d = rdiag();
    NetHalObservation h;
    const NetWireUartStream *st = stream();
    (void)net_hal_adapter_observation(&hal, &h);
    fprintf(stderr, "DBG upper cap=%u drop=%u inv=%u held=%u del=%u dup=%u stale=%u rej=%u err=%u th=%u\n",
        o.rx_captured, o.rx_dropped, o.rx_invalid, o.rx_held, o.rx_delivered, o.duplicates, o.stale, o.consumer_rejected, o.errors, o.time_holds);
    fprintf(stderr, "DBG hal cap=%u drop=%u err=%u ch=%u | rtd bytes=%u qrej=%u drained=%u fut=%u\n",
        h.rx_captured, h.rx_dropped, h.errors, h.clock_holds, d.uart[DIAG].uart_rx_bytes, d.uart[DIAG].uart_rx_queue_rejected, d.main_drained_max, d.tick_future_clamped);
    fprintf(stderr, "DBG stream count=%u disc=%u hdr=%u crc=%u to=%u loss=%u typed_rej=%u deliv=%u\n",
        (unsigned)st->count, st->discarded_bytes, st->bad_headers, st->bad_crc, st->assembly_timeouts, st->byte_losses,
        net_upper_receiver(&service, LINK - 1u)->typed_rejected, net_upper_receiver(&service, LINK - 1u)->delivered_frames);
}

/* ----------------------------------------------------------- line I/O ---- */
/* 115200 bit/s, 8N1: one character every 86.8 us. */
#define BYTE_US 87u

/* Bytes arriving back to back within the current instant (one RX_FULL each). */
static uint32 line_in(const uint8_t *bytes, size_t n)
{
    uint32 taken;
    in_isr = true;
    taken = MockRtd_UartInject(LPUART, bytes, (uint32)n);
    in_isr = false;
    return taken;
}

/* Bytes at the line rate while the main loop keeps running. */
static uint32 line_send(const uint8_t *bytes, size_t n)
{
    uint32 taken = 0u;
    size_t i;
    for (i = 0u; i < n; ++i) { advance_us(BYTE_US); taken += line_in(bytes + i, 1u); }
    return taken;
}

/* Bytes at the line rate while the main loop is held up. */
static uint32 line_send_stalled(const uint8_t *bytes, size_t n)
{
    uint32 taken = 0u;
    size_t i;
    for (i = 0u; i < n; ++i) { stall_us(BYTE_US); taken += line_in(bytes + i, 1u); }
    return taken;
}

static uint32 line_out(uint8_t *out, size_t max)
{
    uint32 n;
    in_isr = true;
    n = MockRtd_UartTransmitTake(LPUART, out, (uint32)max);
    in_isr = false;
    return n;
}

static void line_error(Lpuart_Uart_Ip_StatusType status)
{
    in_isr = true;
    MockRtd_UartRaiseError(LPUART, status);
    in_isr = false;
}

/* ------------------------------------------------------------ frames ---- */
static NetWireUartFrame encode(const NetUpperUartMessage *m)
{
    NetWireUartFrame f;
    memset(&f, 0, sizeof(f));
    if (net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY, m, &f) != NET_WIRE_OK) f.length = 0u;
    return f;
}

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
/* M_CONTEXT field values of the repository vector (test_upper_uart.c golden_m_context). */
static const uint8_t golden_m_context[] = {
    0xa5u,0x5au,0x02u,0x01u,0x26u,0x00u,0xa5u,0x5au,0x78u,0x56u,0x34u,0x12u,0xefu,0xcdu,0xabu,0x89u,
    0x67u,0x45u,0x23u,0x01u,0x01u,0x01u,0x00u,0x00u,0x40u,0x30u,0x20u,0x10u,0x34u,0x12u,0x01u,0x01u,
    0x00u,0x80u,0x70u,0x60u,0x50u,0x78u,0x56u,0x00u,0xefu,0xcdu,0xabu,0x90u,0xbcu,0x9au,0x3du,0xfdu};
#endif

/* A distinct receivable message per value (new observation, new context). */
static NetUpperUartMessage rx_message(uint16_t sequence, uint32_t value)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = LINK;
    m.type = RX_TYPE;
    m.sequence = sequence;
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    m.data.p_vision_result.pi_boot_id = PI_BOOT;
    m.data.p_vision_result.vision_sequence = value;
    m.data.p_vision_result.vision_age_ms = 20u;
    m.data.p_vision_result.occupant_presence = 1u;
    m.data.p_vision_result.presence_validity = 1u;
    m.data.p_vision_result.presence_reason = 0u;
    m.data.p_vision_result.occupant_count = 2u;
    m.data.p_vision_result.count_validity = 1u;
    m.data.p_vision_result.count_reason = 0u;
    m.data.p_vision_result.vision_status = 1u;
    m.data.p_vision_result.vision_fault_code = 0u;
#else
    {
        NetWireUartFrame g;
        uint16_t s;
        memset(&g, 0, sizeof(g));
        g.length = sizeof(golden_m_context);
        memcpy(g.bytes, golden_m_context, sizeof(golden_m_context));
        (void)net_wire_m_context_decode(NET_WIRE_CANDIDATE_ONLY, &g, &m.data.m_context, &s);
        /* A new evaluation on every lane, fresh ages (2000 ms receive bound). */
        m.data.m_context.app_update_sequence = value;
        m.data.m_context.proximity_update_sequence = value;
        m.data.m_context.registration_connection_update_sequence = value;
        m.data.m_context.app_age_ms = 10u;
        m.data.m_context.proximity_age_ms = 10u;
        m.data.m_context.registration_connection_age_ms = 10u;
    }
#endif
    return m;
}

static NetWireUartFrame rx_frame(uint16_t sequence, uint32_t value)
{
    NetUpperUartMessage m = rx_message(sequence, value);
    return encode(&m);
}

/* The last typed callback carried exactly this message (re-encoded wire equal). */
static bool delivered_is(uint16_t sequence, uint32_t value)
{
    NetUpperUartMessage want = rx_message(sequence, value);
    NetWireUartFrame a = encode(&rx_last), b = encode(&want);
    return a.length != 0u && a.length == b.length && memcmp(a.bytes, b.bytes, a.length) == 0;
}

/* ------------------------------------------------------------- TX ------ */
static uint32_t permission_update;  /* UPDATE_SEQUENCE of the last DOMAIN_PERMISSION in */
static uint64_t permission_at;      /* its capture time */
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
static uint64_t tx_observed_at;     /* source stamp of the last publish */
#endif

static int permission_in(uint32_t tx, uint32_t update)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope e;
    MockCanFrame f;
    int mb;
    memset(&p, 0, sizeof(p));
    p.boot_id = DOMAIN_BOOT;
    p.tx_sequence = tx;
    p.vehicle_power_permission = 1u;
    p.power_permission_quality = 0u;
    p.power_operation_state = 1u;
    p.operation_allowed = 1u;
    p.operation_quality = 0u;
    p.update_sequence = update;
    p.source_age_ms = 10u;
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, &e) == NET_WIRE_OK);
    memset(&f, 0, sizeof(f));
    f.id = e.standard_id; f.fd = e.fdf; f.brs = e.brs; f.dlc = e.dlc; f.length = (uint8)e.length;
    memcpy(f.data, e.bytes, e.length);
    in_isr = true;
    mb = MockRtd_CanInject(CAN_BUS, &f);
    in_isr = false;
    CHECK(mb >= 0);
    permission_update = update;
    permission_at = line_ms();
    return 0;
}

/* CIS relays the accepted DOMAIN_PERMISSION; DOMAIN publishes its alive state. */
static void tx_prepare(void)
{
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    unsigned before = perm_count;
    if (permission_in(1u, 77u) != 0) return;
    while (perm_count == before && line_ms() < 1000u) step_ms(1u);
#endif
}

static NetStatus tx_publish(NetTxKey *key)
{
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    NetAgePolicy policy = { true, 1000u, true };
    return net_upper_cis_publish_pi_permission(&service, CIS_BOOT, 1500u, &policy, key);
#else
    NetWireMDomainAliveRaw a;
    NetSourceStamp s;
    NetAgePolicy policy = { true, 10000u, true };
    memset(&a, 0, sizeof(a));
    a.domain_boot_id = DOMAIN_BOOT;
    a.update_sequence = 900u;
    a.source_age_ms = 5u;
    a.domain_state = 1u;
    a.value_quality = 0u;
    tx_observed_at = line_ms();
    s.observation_id = 1u;
    s.observed_at_ms = tx_observed_at;
    s.original_age_ms = a.source_age_ms;     /* the stamp states the message's own age */
    return net_ecu_publish_m_domain_alive(&ecu, &a, &s, &policy, key);
#endif
}

/* Wire bytes decode to the published message with this LINK_SEQUENCE, the
 * age grown by the wait until dispatch (dispatched_at), and re-encode equal. */
static int check_tx_bytes(const uint8_t *bytes, size_t n, uint16_t sequence, uint64_t dispatched_at)
{
    NetWireUartFrame f, again;
    NetUpperUartMessage m;
    const NetWireRegistryEntry *r = net_wire_registry_find(LINK, TX_TYPE);
    CHECK(r != NULL && n <= sizeof(f.bytes));
    memset(&f, 0, sizeof(f));
    f.length = n;
    memcpy(f.bytes, bytes, n);
    CHECK(net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, LINK, r->direction, &f, &m) == NET_WIRE_OK);
    CHECK(m.type == TX_TYPE && m.sequence == sequence);
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    CHECK(m.data.p_permission.cis_boot_id == CIS_BOOT);
    CHECK(m.data.p_permission.update_sequence == permission_update);
    CHECK(m.data.p_permission.vehicle_power_permission == 1u);
    CHECK(m.data.p_permission.value_quality == 0u && m.data.p_permission.quality_reason == 0u);
    CHECK(m.data.p_permission.use_limit_ms == 1500u);
    CHECK(m.data.p_permission.source_age_ms == 10u + (dispatched_at - permission_at));
#else
    CHECK(m.data.m_domain_alive.domain_boot_id == DOMAIN_BOOT);
    CHECK(m.data.m_domain_alive.update_sequence == 900u);
    CHECK(m.data.m_domain_alive.domain_state == 1u && m.data.m_domain_alive.value_quality == 0u);
    CHECK(m.data.m_domain_alive.source_age_ms == 5u + (dispatched_at - tx_observed_at));
#endif
    again = encode(&m);
    CHECK(again.length == n && memcmp(again.bytes, bytes, n) == 0);
    return 0;
}

static int snapshot_is(NetTxKey k, bool admitted, NetTxOutcome outcome, bool released)
{
    NetUpperTxSnapshot s;
    CHECK(net_upper_tx_snapshot(&service, k, &s) == NET_OK);
    CHECK(s.submitted && s.admission_known == admitted && s.outcome == outcome && s.reference_released == released);
    if (admitted) CHECK(s.accepted);
    return 0;
}

/* One complete send: publish -> AsyncSend at the next pass -> the line takes
 * the bytes -> END_TRANSFER -> SUCCEEDED + release at the next pass. */
static int tx_cycle(uint16_t sequence)
{
    NetTxKey k;
    uint8_t out[NET_WIRE_UART_FRAME_MAX];
    uint32 sends = MockRtd_UartSends(LPUART), n;
    NetHalRtdDiag d0 = rdiag(), d;
    NetUpperObservation o0 = observation(), o;
    uint64_t dispatched_at;
    CHECK(tx_publish(&k) == NET_OK);
    CHECK(MockRtd_UartSends(LPUART) == sends);          /* nothing before the main pass */
    step_ms(1u);
    dispatched_at = line_ms();
    CHECK(MockRtd_UartSends(LPUART) == sends + 1u);
    d = rdiag();
    CHECK(d.uart[DIAG].uart_tx_sent == d0.uart[DIAG].uart_tx_sent + 1u);
    RUN(snapshot_is(k, true, NET_TX_UNKNOWN, false));   /* admitted at AsyncSend */
    n = line_out(out, sizeof(out));
    CHECK(n > 0u && MockRtd_UartTxRemaining(LPUART) == 0u);
    d = rdiag();
    CHECK(d.uart[DIAG].uart_tx_complete == d0.uart[DIAG].uart_tx_complete + 1u);
    RUN(snapshot_is(k, true, NET_TX_UNKNOWN, false));   /* ISR fact only, not yet reported */
    step_ms(1u);
    RUN(snapshot_is(k, true, NET_TX_SUCCEEDED, true));
    CHECK(net_upper_retire(&service, k) == NET_OK);
    o = observation();
    CHECK(o.tx_succeeded == o0.tx_succeeded + 1u && o.tx_released == o0.tx_released + 1u);
    RUN(check_tx_bytes(out, n, sequence, dispatched_at));
    CHECK(isr_callbacks == 0u);
    return 0;
}

/* ---------------------------------------------------- shared checks ---- */
/* One frame on the line, byte by byte at the line rate, is delivered once,
 * from a main pass, with the ISR capture time of its last byte. */
static int rx_once(uint16_t sequence, uint32_t value)
{
    NetWireUartFrame f = rx_frame(sequence, value);
    NetHalRtdDiag d0 = rdiag(), d;
    unsigned before = rx_count;
    uint64_t at;
    CHECK(f.length != 0u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    at = line_ms();                                     /* last byte's RX_FULL */
    CHECK(rx_count == before);                          /* not before a main pass */
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_bytes == d0.uart[DIAG].uart_rx_bytes + f.length);
    CHECK(run_until_rx(before + 1u, 30u));
    CHECK(rx_count == before + 1u && delivered_is(sequence, value));
    CHECK(rx_meta.source_sequence == sequence);
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    CHECK(rx_meta.source_boot == PI_BOOT);
#endif
    CHECK(rx_meta.capture.timestamp_known && rx_meta.capture.port_epoch == 1u);
    CHECK(rx_meta.capture.captured_at_ms == at);        /* ISR time, not the main pass */
    CHECK(rx_meta.delivered_at_ms >= at);
    CHECK(rx_meta.local_elapsed_ms == rx_meta.delivered_at_ms - at);
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    CHECK(rx_meta.age_ms == 20u + rx_meta.local_elapsed_ms);
#endif
    step_ms(5u);
    CHECK(rx_count == before + 1u);                     /* exactly once */
    CHECK(isr_callbacks == 0u);
    return 0;
}

static int link_healthy(void)
{
    NetUpperObservation o = observation();
    CHECK(!service.link_failed[LINK] && !hal.faulted[LINK]);
    CHECK(o.errors == 0u);
    return 0;
}

/* -------------------------------------------------------------- cases ---- */
static int u01_rx_frame(void)
{
    NetHalRtdDiag d;
    CHECK(start() == NET_OK);
    d = rdiag();
    CHECK(d.started && d.uart[DIAG].uart_present && d.uart[DIAG].uart_rx_armed);
    CHECK(!d.uart[1u - DIAG].uart_present);             /* the other link is not compiled */
    CHECK(d.uart_isr_ring_capacity == 256u);
    CHECK(MockRtd_UartInits(LPUART) == 1u && MockRtd_UartRxArmed(LPUART));
    step_ms(2u);
    RUN(rx_once(1u, 100u));
    d = rdiag();
    /* At the line rate a 1 ms pass finds about 12 bytes: never above the budget. */
    CHECK(d.uart[DIAG].uart_isr_ring_max_depth >= 1u && d.uart[DIAG].uart_isr_ring_max_depth < d.isr_drain_budget);
    CHECK(d.main_drained_max <= d.isr_drain_budget);
    CHECK(d.uart[DIAG].uart_rx_errors == 0u && d.uart[DIAG].uart_isr_ring_overflow == 0u);
    CHECK(d.uart_unexpected_instance == 0u && d.tick_future_clamped == 0u);
    RUN(link_healthy());
    return 0;
}

static int u02_split(void)
{
    static const size_t cut[] = { 1u, 5u, 2u, 9u };
    NetWireUartFrame f;
    size_t i, at = 0u;
    uint64_t last;
    CHECK(start() == NET_OK);
    step_ms(2u);
    f = rx_frame(3u, 300u);
    for (i = 0u; i < sizeof(cut) / sizeof(cut[0]); ++i) {
        CHECK(line_in(f.bytes + at, cut[i]) == cut[i]);
        at += cut[i];
        step_ms(1u + (unsigned)i);
        CHECK(rx_count == 0u);
    }
    last = line_ms();
    CHECK(line_in(f.bytes + at, f.length - at) == f.length - at);
    CHECK(run_until_rx(1u, 30u));
    CHECK(rx_count == 1u && delivered_is(3u, 300u));
    CHECK(rx_meta.capture.captured_at_ms == last);      /* frame time = its last byte */
    step_ms(5u);
    CHECK(rx_count == 1u && isr_callbacks == 0u);
    return 0;
}

static int u03_resync(void)
{
    NetWireUartFrame f;
    uint8_t noise[10];
    CHECK(start() == NET_OK);
    step_ms(2u);
    f = rx_frame(4u, 400u);
    /* Noise, then a false SYNC with a plausible header whose claimed frame
     * swallows the start of the real one. */
    noise[0] = 0x00u; noise[1] = 0x11u; noise[2] = 0xA5u; noise[3] = 0x22u;
    noise[4] = 0xA5u; noise[5] = 0x5Au;
    noise[6] = f.bytes[2]; noise[7] = f.bytes[3]; noise[8] = f.bytes[4]; noise[9] = f.bytes[5];
    CHECK(line_send(noise, sizeof(noise)) == sizeof(noise));
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(1u, 30u));
    CHECK(rx_count == 1u && delivered_is(4u, 400u));
    CHECK(stream()->bad_crc >= 1u && stream()->discarded_bytes >= 4u);
    step_ms(5u);
    CHECK(rx_count == 1u);
    RUN(link_healthy());
    return 0;
}

static int u04_crc_error(void)
{
    NetWireUartFrame bad, good;
    CHECK(start() == NET_OK);
    step_ms(2u);
    bad = rx_frame(5u, 500u);
    bad.bytes[bad.length - 1u] ^= 0xFFu;
    good = rx_frame(6u, 501u);
    CHECK(line_send(bad.bytes, bad.length) == bad.length);
    CHECK(line_send(good.bytes, good.length) == good.length);
    CHECK(run_until_rx(1u, 30u));
    CHECK(rx_count == 1u && delivered_is(6u, 501u));
    CHECK(stream()->bad_crc == 1u);
    step_ms(5u);
    CHECK(rx_count == 1u);
    RUN(link_healthy());
    return 0;
}

static int u05_line_error(void)
{
    NetWireUartFrame f;
    NetHalRtdDiag d;
    uint32 lost;
    CHECK(start() == NET_OK);
    step_ms(2u);
    tx_prepare();
    /* Framing error inside frame 1: frame 1 is dropped, frame 2 delivered. */
    f = rx_frame(1u, 600u);
    CHECK(line_send(f.bytes, 10u) == 10u);
    line_error(LPUART_UART_IP_STATUS_FRAMING_ERROR);
    CHECK(MockRtd_UartRxArmed(LPUART));                 /* re-armed in the callback */
    CHECK(line_send(f.bytes + 10u, f.length - 10u) == f.length - 10u);
    f = rx_frame(2u, 601u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(1u, 30u));
    CHECK(rx_count == 1u && delivered_is(2u, 601u));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_errors == 1u && d.uart[DIAG].uart_rx_loss_marks == 1u);
    CHECK(d.uart[DIAG].uart_last_error_status == (uint32_t)LPUART_UART_IP_STATUS_FRAMING_ERROR);
    CHECK(stream()->byte_losses >= 1u);
    /* Overrun inside frame 3: frame 3 dropped, frame 4 delivered. */
    f = rx_frame(3u, 602u);
    CHECK(line_send(f.bytes, 5u) == 5u);
    line_error(LPUART_UART_IP_STATUS_RX_OVERRUN);
    CHECK(line_send(f.bytes + 5u, f.length - 5u) == f.length - 5u);
    f = rx_frame(4u, 603u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(2u, 30u));
    CHECK(rx_count == 2u && delivered_is(4u, 603u));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_overrun == 1u && d.uart[DIAG].uart_rx_loss_marks == 2u);
    /* Error between two frames: the next frame is intact (the loss is its
     * own capture, it does not consume the next SYNC byte). */
    line_error(LPUART_UART_IP_STATUS_PARITY_ERROR);
    f = rx_frame(5u, 604u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(3u, 30u));
    CHECK(rx_count == 3u && delivered_is(5u, 604u));
    /* A refused re-arm: the reception stays stopped until the main context
     * re-arms it; bytes meanwhile are lost on the line. */
    MockRtd_UartSetReceiveRefused(LPUART, true);
    line_error(LPUART_UART_IP_STATUS_NOISE_ERROR);
    d = rdiag();
    CHECK(!MockRtd_UartRxArmed(LPUART) && !d.uart[DIAG].uart_rx_armed && d.uart[DIAG].uart_rx_rearm_errors == 1u);
    lost = MockRtd_UartRxLost(LPUART);
    f = rx_frame(6u, 605u);
    CHECK(line_in(f.bytes, f.length) == 0u && MockRtd_UartRxLost(LPUART) == lost + f.length);
    MockRtd_UartSetReceiveRefused(LPUART, false);
    step_ms(1u);
    d = rdiag();
    CHECK(MockRtd_UartRxArmed(LPUART) && d.uart[DIAG].uart_rx_armed);
    f = rx_frame(7u, 606u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(4u, 30u));
    CHECK(rx_count == 4u && delivered_is(7u, 606u));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_errors == 3u && d.uart[DIAG].uart_rx_loss_marks == 4u);
    /* The link is never locked: RX went on, and a send still works. */
    RUN(link_healthy());
    RUN(tx_cycle(1u));
    CHECK(isr_callbacks == 0u);
    return 0;
}

static int u06_link_sequence(void)
{
    NetWireUartFrame f;
    NetUpperObservation o;
    CHECK(start() == NET_OK);
    step_ms(2u);
    RUN(rx_once(5u, 700u));
    /* Exact duplicate (same LINK_SEQUENCE, same bytes): not delivered again. */
    f = rx_frame(5u, 700u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    step_ms(5u);
    o = observation();
    CHECK(rx_count == 1u && o.duplicates == 1u);
    /* Older LINK_SEQUENCE: refused as stale. */
    f = rx_frame(4u, 701u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    step_ms(5u);
    o = observation();
    CHECK(rx_count == 1u && o.stale == 1u);
    /* Newer LINK_SEQUENCE: delivered. */
    RUN(rx_once(6u, 702u));
    CHECK(rx_count == 2u);
    return 0;
}

static int u07_tx(void)
{
    CHECK(start() == NET_OK);
    step_ms(2u);
    tx_prepare();
    RUN(tx_cycle(1u));
    RUN(tx_cycle(2u));                                  /* LINK_SEQUENCE per frame */
    CHECK(MockRtd_UartAborts(LPUART) == 0u);
    RUN(link_healthy());
    return 0;
}

static int u08_tx_timeout(void)
{
    NetTxKey k;
    NetHalRtdDiag d;
    uint8_t out[NET_WIRE_UART_FRAME_MAX];
    uint64_t started;
    CHECK(start() == NET_OK);
    step_ms(2u);
    tx_prepare();
    MockRtd_UartSetTxStall(LPUART, true);
    CHECK(tx_publish(&k) == NET_OK);
    step_ms(1u);
    started = line_ms();
    CHECK(MockRtd_UartSends(LPUART) == 1u);
    while (line_ms() < started + 99u) step_ms(1u);
    RUN(snapshot_is(k, true, NET_TX_UNKNOWN, false));   /* 99 ms: still on the line */
    CHECK(MockRtd_UartAborts(LPUART) == 0u);
    step_ms(1u);                                        /* 100 ms: aborted */
    CHECK(MockRtd_UartAborts(LPUART) == 1u);
    RUN(snapshot_is(k, true, NET_TX_FAILED, true));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_tx_timeout_aborts == 1u && d.uart[DIAG].uart_tx_complete == 0u);
    CHECK(net_upper_retire(&service, k) == NET_OK);
    MockRtd_UartSetTxStall(LPUART, false);
    CHECK(line_out(out, sizeof(out)) == 0u);            /* nothing left on the line */
    RUN(tx_cycle(2u));                                  /* next send is normal */
    RUN(link_healthy());
    return 0;
}

static int u09_tx_one_per_link(void)
{
    NetTxKey k1, k2;
    NetHalRtdDiag d;
    uint8_t first[NET_WIRE_UART_FRAME_MAX], second[NET_WIRE_UART_FRAME_MAX];
    uint32 n1, n2;
    uint64_t at1;
    CHECK(start() == NET_OK);
    step_ms(2u);
    tx_prepare();
    CHECK(tx_publish(&k1) == NET_OK);
    CHECK(tx_publish(&k2) == NET_OK);
    step_ms(1u);
    at1 = line_ms();
    CHECK(MockRtd_UartSends(LPUART) == 1u);             /* one transfer on the line */
    d = rdiag();
    CHECK(d.uart[DIAG].uart_tx_waiting == 1u && d.uart[DIAG].uart_tx_waiting_max == 1u);
    RUN(snapshot_is(k1, true, NET_TX_UNKNOWN, false));
    RUN(snapshot_is(k2, false, NET_TX_UNKNOWN, false)); /* borrowed, waiting, not admitted */
    step_ms(3u);
    CHECK(MockRtd_UartSends(LPUART) == 1u);
    n1 = line_out(first, sizeof(first));
    CHECK(n1 > 0u);
    step_ms(1u);                                        /* first reported, second started */
    CHECK(MockRtd_UartSends(LPUART) == 2u);
    RUN(snapshot_is(k1, true, NET_TX_SUCCEEDED, true));
    RUN(snapshot_is(k2, true, NET_TX_UNKNOWN, false));
    n2 = line_out(second, sizeof(second));
    CHECK(n2 > 0u);
    step_ms(1u);
    RUN(snapshot_is(k2, true, NET_TX_SUCCEEDED, true));
    CHECK(net_upper_retire(&service, k1) == NET_OK && net_upper_retire(&service, k2) == NET_OK);
    RUN(check_tx_bytes(first, n1, 1u, at1));
    /* Both frames were encoded when they were handed to the device. */
    RUN(check_tx_bytes(second, n2, 2u, at1));
    RUN(link_healthy());
    return 0;
}

#ifdef UART_HAL_TEST_ROUTE
static int u10_callback_routing(void)
{
    static const uint8_t console[] = { 'o', 'k', '\r', '\n' };
    NetHalRtdDiag d;
    CHECK(start() == NET_OK);
    /* Board code owns the console instance sharing the callback. */
    Lpuart_Uart_Ip_Init(0u, &Lpuart_Uart_Ip_xHwConfigPB_0);
    CHECK(Lpuart_Uart_Ip_AsyncReceive(0u, &console_byte, 1u) == LPUART_UART_IP_STATUS_SUCCESS);
    step_ms(2u);
    in_isr = true;
    CHECK(MockRtd_UartInject(0u, console, sizeof(console)) == sizeof(console));
    in_isr = false;
    CHECK(board_console_events == sizeof(console));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_bytes == 0u && d.uart_unexpected_instance == 0u);
    /* An instance that is no Network link reaching the entry: ignored, counted. */
    in_isr = true;
    NetHalRtdUart_OnEvent(5u, LPUART_UART_IP_EVENT_RX_FULL);
    in_isr = false;
    d = rdiag();
    CHECK(d.uart_unexpected_instance == 1u && d.uart[DIAG].uart_rx_bytes == 0u);
    /* Routed instance: RX and TX as in the direct mode. */
    RUN(rx_once(1u, 100u));
    tx_prepare();
    RUN(tx_cycle(1u));
    CHECK(board_console_events == sizeof(console));
    RUN(link_healthy());
    return 0;
}
#endif

static int u11_can_uart_mixed(void)
{
    NetWireUartFrame a, b, c;
    NetUpperObservation o;
    unsigned i;
    size_t at = 0u;
    CHECK(start() == NET_OK);
    step_ms(2u);
    a = rx_frame(1u, 800u);
    b = rx_frame(2u, 801u);
    c = rx_frame(3u, 802u);
    /* CAN frames and UART bytes interleaved within the same passes. */
    for (i = 0u; i < 10u; ++i) {
        RUN(permission_in(10u + i, 100u + i));
        if (at < a.length) {
            size_t n = a.length - at < 6u ? a.length - at : 6u;
            CHECK(line_in(a.bytes + at, n) == n);
            at += n;
        } else if (i == 6u) {
            CHECK(line_send(b.bytes, b.length) == b.length);
        }
        if ((i & 1u) == 1u) step_ms(1u);
    }
    CHECK(run_until_rx(2u, 30u));
    step_ms(3u);
    CHECK(perm_count == 10u && rx_count == 2u && delivered_is(2u, 801u));
    /* UART send relaying the latest CAN permission while UART RX goes on. */
    CHECK(line_send(c.bytes, 7u) == 7u);
    RUN(tx_cycle(1u));
    CHECK(line_send(c.bytes + 7u, c.length - 7u) == c.length - 7u);
    RUN(permission_in(30u, 130u));
    CHECK(run_until_rx(3u, 30u));
    step_ms(3u);
    CHECK(rx_count == 3u && delivered_is(3u, 802u) && perm_count == 11u);
    o = observation();
    CHECK(o.rx_invalid == 0u && o.rx_dropped == 0u && o.stale == 0u && o.duplicates == 0u);
    CHECK(isr_callbacks == 0u);
    RUN(link_healthy());
    return 0;
}

static int u12_isr_ring_overflow(void)
{
    NetWireUartFrame f;
    NetHalRtdDiag d;
    NetUpperObservation o;
    uint32_t i, total = 0u;
    CHECK(start() == NET_OK);
    step_ms(2u);
    /* Main loop held up for ten frames (about 25 ms): the ISR keeps storing
     * bytes, the 256-byte ring keeps the first 256, the rest is lost and
     * counted. */
    for (i = 0u; i < 10u; ++i) {
        f = rx_frame((uint16_t)(1u + i), 900u + i);
        CHECK(line_send_stalled(f.bytes, f.length) == f.length);
        total += (uint32_t)f.length;
    }
    d = rdiag();
    CHECK(d.uart[DIAG].uart_isr_ring_max_depth == 256u);
    CHECK(d.uart[DIAG].uart_isr_ring_overflow == total - 256u);
    CHECK(d.uart[DIAG].uart_rx_bytes == total);
    /* Passes resume: one budget per pass. Bytes captured before an earlier
     * pass's time are refused by M06 (net_upper_uart.h receiver poll
     * contract), so no partial or mixed frame is ever delivered. */
    step_ms(30u);
    d = rdiag();
    CHECK(d.main_drained_max <= d.isr_drain_budget);
    o = observation();
    CHECK(o.rx_invalid == 0u && o.errors == 0u);
    /* The next byte brings the loss in stream order; the next frame on the
     * line is delivered whole. */
    f = rx_frame(20u, 990u);
    CHECK(line_send(f.bytes, f.length) == f.length);
    CHECK(run_until_rx(rx_count + 1u, 30u));
    CHECK(delivered_is(20u, 990u));
    d = rdiag();
    CHECK(d.uart[DIAG].uart_rx_loss_marks == 1u);
    CHECK(stream()->byte_losses >= 1u);
    CHECK(isr_callbacks == 0u);
    RUN(link_healthy());
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "u01_rx_frame", u01_rx_frame }, { "u02_split", u02_split }, { "u03_resync", u03_resync },
        { "u04_crc_error", u04_crc_error }, { "u05_line_error", u05_line_error },
        { "u06_link_sequence", u06_link_sequence }, { "u07_tx", u07_tx },
        { "u08_tx_timeout", u08_tx_timeout }, { "u09_tx_one_per_link", u09_tx_one_per_link },
#ifdef UART_HAL_TEST_ROUTE
        { "u10_callback_routing", u10_callback_routing },
#endif
        { "u11_can_uart_mixed", u11_can_uart_mixed }, { "u12_isr_ring_overflow", u12_isr_ring_overflow }
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
