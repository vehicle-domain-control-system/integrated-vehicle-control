/* Public entry Net_Init() / Net_MainFunction(nowMs) of the VSS node on the
 * host: real product sources (entry -> composition root -> NetUpper ->
 * NetHalAdapter -> RTD backend) linked with the independently written RTD mock
 * SDK, which plays the CAN bus and calls the FlexCAN callbacks like the IRQ.
 *
 * Contract (firmware/vss src/net_if/vss_net_api.h, src/app/vss_main.c,
 * src/infra/vss_time.h):
 *   - boot: VssTime_Init(HalTimer_GetTicks) ... Net_Init(); loop:
 *     Net_MainFunction(VssTime_NowMs32()).
 *   - VssApp_On* run inside Net_MainFunction only, never from an interrupt.
 *   - received_at_ms is on the VSS clock axis: the CAN ISR records the raw
 *     tick (HalTimer_GetTicks, 1 tick = 1.25 ms), the main context converts it
 *     with VssTime_TickToMs32; no millisecond counter of Network's own.
 *   - one Net_MainFunction pass is short; the receive queue is emptied in
 *     bounded portions per pass.
 *   - temporary refusals are NOT_READY / NO_CAPACITY, only argument errors are
 *     INVALID_ARGUMENT.
 * The board hooks below play VSS: an 800 Hz tick, a VssTime_TickToMs32 like
 * conversion, the node configuration (BOOT_ID) and a microsecond counter.
 * Frames are built and checked with the repository codec (net_wire_can_*).
 * One case per process: Net_Init acts once per process. */
#include "network/net_node.h"
#include "network/app/net_if_vss.h"
#include "network/net_wire_can.h"
#include "network/net_wire_can_nodes.h"
#include "mock_rtd.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)

static const char *case_name;

enum { NODE_BOOT = 0x5653530Bu, PEER_BOOT = 0x444F4D31u, BUS = 0 };
/* VSS boots its clock, waits for the codec reference clock, then calls Net_Init. */
#define START_TICK UINT64_C(40)

/* ------------------------------------------------------- board hooks ---- */
/* Board tick, 800 Hz (1 tick = 1.25 ms) like HalTimer_GetTicks; 64-bit view
 * kept by the test, the hook returns the low 32 bits. */
static uint64_t board_tick;
static bool in_isr, in_main;
static uint32_t isr_tick_reads, tick_reads_outside_isr;
static uint32_t conversions, conversions_in_isr, conversions_outside_main;
static uint32_t board_us, callback_work_us;
static uint32_t config_calls, config_boot = NODE_BOOT;
static bool config_accept = true;
static NetNodeConfig config_seen;

uint32_t MockBoard_IsrTicks(void);
uint32_t MockBoard_TickToMs(uint32_t tick);
bool MockBoard_NodeConfig(NetNodeConfig *out);
uint32_t MockBoard_Us(void);

/* VssTime_TicksToMs: milliseconds covered by a tick count, rounded down. */
static uint64_t ms64_at(uint64_t tick) { return (tick * 5u) / 4u; }
static uint32_t ms_at(uint64_t tick) { return (uint32_t)ms64_at(tick); }
/* First tick whose millisecond value is at least ms. */
static uint64_t tick_for_ms(uint64_t ms) { return (ms * 4u + 4u) / 5u; }

uint32_t MockBoard_IsrTicks(void)
{
    if (in_isr) ++isr_tick_reads; else ++tick_reads_outside_isr;
    return (uint32_t)board_tick;
}

/* Like VssTime_TickToMs32: a tick read earlier (at most the current tick) on
 * the VssTime_NowMs32 axis; a tick it cannot place gives the current time. */
uint32_t MockBoard_TickToMs(uint32_t tick)
{
    uint32_t back = (uint32_t)board_tick - tick;
    ++conversions;
    if (in_isr) ++conversions_in_isr;
    if (!in_main) ++conversions_outside_main;
    if (back >= 0x80000000u || (uint64_t)back > board_tick) return ms_at(board_tick);
    return ms_at(board_tick - back);
}

bool MockBoard_NodeConfig(NetNodeConfig *out)
{
    ++config_calls;
    config_seen = *out;
    if (!config_accept) return false;
    out->boot_id = config_boot;
    return true;
}

uint32_t MockBoard_Us(void) { return board_us; }

/* ------------------------------------------------------ App callbacks ---- */
static int event_count, warning_count;
static int callbacks_in_isr, callbacks_outside_main, callback_depth, callback_depth_max;
static NetVssEvent events[64];
static NetCanRxMeta event_metas[64];
static NetVssEvent last_event;
static NetCanRxMeta last_event_meta, last_warning_meta;
static const NetWireCanEnvelope *isr_during_callback; /* a frame that arrives while a callback runs */

static int inject(const NetWireCanEnvelope *e);

static void callback_enter(void)
{
    if (in_isr) ++callbacks_in_isr;
    if (!in_main) ++callbacks_outside_main;
    if (++callback_depth > callback_depth_max) callback_depth_max = callback_depth;
    board_us += callback_work_us;
}

void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)
{
    callback_enter();
    if (event_count < 64) { events[event_count] = *message; event_metas[event_count] = *meta; }
    ++event_count;
    last_event = *message;
    last_event_meta = *meta;
    if (isr_during_callback != NULL) {
        const NetWireCanEnvelope *e = isr_during_callback;
        isr_during_callback = NULL;
        (void)inject(e);
    }
    --callback_depth;
}

void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)
{
    (void)message;
    callback_enter();
    ++warning_count;
    last_warning_meta = *meta;
    --callback_depth;
}

/* ------------------------------------------------------------ helpers ---- */
static uint64_t app_ms64(void) { return ms64_at(board_tick); }
static uint32_t app_now(void) { return ms_at(board_tick); } /* VssTime_NowMs32 */

/* One super-loop pass: Net_MainFunction(VssTime_NowMs32()). */
static void pass_with(uint32_t now_ms)
{
    in_main = true;
    Net_MainFunction(now_ms);
    in_main = false;
}
static void pass(void) { pass_with(app_now()); }
static void run_ticks(unsigned n) { while (n-- > 0u) { ++board_tick; pass(); } }
static void run_until_ms(uint64_t ms) { while (app_ms64() < ms) { ++board_tick; pass(); } }

static NetNodeDiag diag(void)
{
    NetNodeDiag d;
    (void)NetNode_GetDiag(&d);
    return d;
}

static void board_reset(uint64_t tick)
{
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(false); /* the board has no OsIf system timer */
    board_tick = tick;
}

static bool start_at(uint64_t tick)
{
    NetNodeDiag d;
    board_reset(tick);
    Net_Init();
    d = diag();
    return d.entry_init_result == NET_ENTRY_INIT_OK && d.stage == NET_NODE_STAGE_READY;
}
static bool start(void) { return start_at(START_TICK); }

/* The bus delivers a frame: the mock calls the FlexCAN callback like the IRQ. */
static int inject(const NetWireCanEnvelope *e)
{
    MockCanFrame f;
    bool was = in_isr;
    int mb;
    memset(&f, 0, sizeof(f));
    f.id = e->standard_id;
    f.fd = e->fdf;
    f.brs = e->brs;
    f.dlc = e->dlc;
    f.length = (uint8)e->length;
    memcpy(f.data, e->bytes, e->length);
    in_isr = true;
    mb = MockRtd_CanInject(BUS, &f);
    in_isr = was;
    return mb;
}

/* The bus takes the next frame the node offered (TX_COMPLETE from the IRQ). */
static int transmit_next(MockCanFrame *out)
{
    int mb;
    in_isr = true;
    mb = MockRtd_CanTransmitNext(BUS, out);
    in_isr = false;
    return mb;
}

static int pull_status(MockCanFrame *out, NetWireVssStatusRaw *raw)
{
    NetWireCanEnvelope e;
    int mb = transmit_next(out);
    if (mb < 0) return -1;
    memset(&e, 0, sizeof(e));
    e.standard_id = out->id;
    e.fdf = out->fd;
    e.brs = out->brs;
    e.dlc = out->dlc;
    e.length = out->length;
    memcpy(e.bytes, out->data, out->length);
    if (net_wire_can_decode_vss_status(NET_WIRE_CANDIDATE_ONLY, &e, raw) != NET_WIRE_OK) return -2;
    return mb;
}

static NetWireVssEventRaw event_raw(uint32_t tx, uint32_t occurrence)
{
    NetWireVssEventRaw r;
    memset(&r, 0, sizeof(r));
    r.boot_id = PEER_BOOT;
    r.tx_sequence = tx;
    r.event_kind = NET_VSS_EVENT_WELCOME;
    r.occurrence_id = occurrence;
    r.original_age_ms = 10u;
    r.use_limit_ms = 500u;
    r.value_quality = NET_QUALITY_OK;
    r.input_availability = NET_AVAILABILITY_AVAILABLE;
    r.related_command_id = 0u;
    return r;
}

static bool encode_event(uint32_t tx, uint32_t occurrence, NetWireCanEnvelope *e)
{
    NetWireVssEventRaw r = event_raw(tx, occurrence);
    return net_wire_can_encode_vss_event(NET_WIRE_CANDIDATE_ONLY, &r, e) == NET_WIRE_OK;
}

static bool encode_warning(uint32_t tx, NetWireCanEnvelope *e)
{
    NetWireVssWarningStateRaw w;
    memset(&w, 0, sizeof(w));
    w.boot_id = PEER_BOOT;
    w.tx_sequence = tx;
    w.antipinch_source_boot = 0x57494E31u;
    w.antipinch_source_sequence = 11u;
    w.antipinch_original_age_ms = 120u;
    w.antipinch_state = NET_FLAG_NO;
    w.antipinch_quality = NET_QUALITY_OK;
    w.antipinch_reason = NET_QUALITY_REASON_NONE;
    w.antipinch_availability = NET_AVAILABILITY_AVAILABLE;
    w.occupant_hazard_source_boot = 0x43495331u;
    w.occupant_hazard_source_sequence = 22u;
    w.occupant_hazard_original_age_ms = 230u;
    w.occupant_hazard_state = NET_FLAG_NO;
    w.occupant_hazard_quality = NET_QUALITY_OK;
    w.occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    w.occupant_hazard_availability = NET_AVAILABILITY_AVAILABLE;
    w.rear_obstacle_source_boot = 0x43495332u;
    w.rear_obstacle_source_sequence = 33u;
    w.rear_obstacle_original_age_ms = 340u;
    w.rear_obstacle_state = NET_REAR_WARNING_CLEAR;
    w.rear_obstacle_quality = NET_QUALITY_OK;
    w.rear_obstacle_reason = NET_QUALITY_REASON_NONE;
    w.rear_obstacle_availability = NET_AVAILABILITY_AVAILABLE;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_quality = NET_QUALITY_OK;
    w.rear_activation_reason = NET_QUALITY_REASON_NONE;
    w.rear_activation_sequence = 44u;
    w.rear_activation_age_ms = 55u;
    return net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, e) == NET_WIRE_OK;
}

static bool encode_permission(uint32_t tx, NetWireCanEnvelope *e)
{
    NetWireDomainPermissionRaw p;
    memset(&p, 0, sizeof(p));
    p.boot_id = PEER_BOOT;
    p.tx_sequence = tx;
    p.vehicle_power_permission = NET_TRISTATE_YES;
    p.power_permission_quality = NET_QUALITY_OK;
    p.power_operation_state = NET_TRISTATE_YES;
    p.operation_allowed = NET_FLAG_YES;
    p.operation_quality = NET_QUALITY_OK;
    p.update_sequence = 77u;
    p.source_age_ms = 10u;
    return net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, e) == NET_WIRE_OK;
}

static NetVssStatus status_ready(uint32_t update, uint16_t source_age)
{
    NetVssStatus s;
    memset(&s, 0, sizeof(s));
    s.vss_state = NET_VSS_STATE_READY;
    s.vss_availability = NET_VSS_AVAILABILITY_FULL;
    s.vss_accepting_events = NET_FLAG_YES;
    s.vss_fault_active = NET_FLAG_NO;
    s.fault_mask = 0u;
    s.vss_last_fault = NET_VSS_FAULT_CODE_NONE;
    s.last_fault_occurrence = 0u;
    s.last_fault_age_ms = 0u;
    s.last_fault_quality = NET_QUALITY_NO_DATA;
    s.update_sequence = update;
    s.source_age_ms = source_age;
    return s;
}

/* -------------------------------------------------------------- cases ---- */
/* Net_Init -> Net_MainFunction: READY. Before Net_Init the main function does
 * nothing; a second Net_Init changes nothing. */
static int e1_init(void)
{
    NetWireCanEnvelope e;
    NetNodeDiag d;
    board_reset(START_TICK);
    pass();
    pass();
    d = diag();
    CHECK(d.entry_init_result == NET_ENTRY_INIT_NONE);
    CHECK(d.entry_init_calls == 0u && d.entry_main_calls == 2u);
    CHECK(d.stage == NET_NODE_STAGE_NONE && d.steps == 0u && !d.rtd.started);
    CHECK(d.entry_now_ms == 0u && config_calls == 0u);

    Net_Init();
    d = diag();
    CHECK(config_calls == 1u);
    CHECK(d.entry_init_result == NET_ENTRY_INIT_OK && d.entry_init_calls == 1u);
    CHECK(d.stage == NET_NODE_STAGE_READY && d.stage_status == NET_OK);
    CHECK(d.rtd.started && d.rtd.init_failed_step == NET_HAL_RTD_STEP_NONE);
    CHECK(d.rtd.time_source == NET_HAL_RTD_TIME_APP_TICK);
    CHECK(d.app.bound && d.app.boot_id == NODE_BOOT && d.app.next_status_sequence == 1u);
    run_ticks(3u);
    d = diag();
    CHECK(d.steps == 3u && d.entry_main_calls == 5u);
    CHECK(d.entry_now_ms == app_ms64());

    /* Second call: no second configuration read, the running node is untouched. */
    config_boot = NODE_BOOT + 1u;
    Net_Init();
    d = diag();
    CHECK(d.entry_init_calls == 2u && config_calls == 1u);
    CHECK(d.entry_init_result == NET_ENTRY_INIT_OK);
    CHECK(d.stage == NET_NODE_STAGE_READY && d.rtd.started && d.app.boot_id == NODE_BOOT);
    CHECK(d.steps == 3u);
    CHECK(encode_event(1u, 7u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    CHECK(event_count == 1 && diag().steps == 4u);
    return 0;
}

/* received_at_ms is the converted ISR tick, not the nowMs of the pass that
 * delivers it; the ISR reads the tick only, the conversion runs in main. */
static int e2_clock_axis(void)
{
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint64_t t_rx;
    CHECK(start());
    run_ticks(10u);
    CHECK(encode_event(1u, 7u, &e));
    t_rx = board_tick;
    CHECK(inject(&e) >= 0);
    CHECK(event_count == 0);
    CHECK(isr_tick_reads == 1u && conversions == 0u);
    board_tick += 7u;                   /* the super-loop comes back 8.75 ms later */
    pass();
    CHECK(event_count == 1);
    CHECK(last_event_meta.received_at_ms == ms_at(t_rx));
    CHECK(app_now() - last_event_meta.received_at_ms == 9u);
    CHECK(last_event_meta.source.boot_id == PEER_BOOT && last_event_meta.source.tx_sequence == 1u);
    CHECK(last_event.occurrence_id == 7u && last_event.original_age_ms == 10u);

    /* VSS_WARNING_STATE and DOMAIN_PERMISSION use the same axis. */
    CHECK(encode_warning(1u, &e));
    t_rx = board_tick;
    CHECK(inject(&e) >= 0);
    board_tick += 3u;
    pass();
    CHECK(warning_count == 1 && last_warning_meta.received_at_ms == ms_at(t_rx));
    CHECK(encode_permission(1u, &e));
    t_rx = board_tick;
    CHECK(inject(&e) >= 0);
    board_tick += 5u;
    pass();
    d = diag();
    CHECK(d.app.permission_received == 1u);
    CHECK(d.app.last_permission_meta.received_at_ms == ms_at(t_rx));

    CHECK(conversions == 3u && conversions_in_isr == 0u && conversions_outside_main == 0u);
    CHECK(isr_tick_reads == 3u && tick_reads_outside_isr == 0u); /* no own clock reads in main */
    CHECK(d.rtd.tick_future_clamped == 0u);
    return 0;
}

/* A frame whose tick is newer than the nowMs of the pass (it arrived after the
 * application read its clock) is delivered at Network now and counted; it is
 * not refused as a capture from the future. */
static int e2_future_tick_clamped(void)
{
    NetWireCanEnvelope e;
    uint32_t stale;
    uint64_t t_rx;
    CHECK(start());
    run_ticks(10u);
    CHECK(encode_event(1u, 7u, &e));
    stale = app_now();
    board_tick += 2u;                   /* ISR during the pass, after nowMs was taken */
    CHECK(inject(&e) >= 0);
    CHECK(ms_at(board_tick) > stale);
    pass_with(stale);
    CHECK(event_count == 1);
    CHECK(last_event_meta.received_at_ms == stale);
    CHECK(diag().rtd.tick_future_clamped == 1u);
    /* Next frame, regular order again: its own tick, nothing clamped. */
    run_ticks(4u);
    CHECK(encode_event(2u, 8u, &e));
    t_rx = board_tick;
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    CHECK(event_count == 2 && last_event_meta.received_at_ms == ms_at(t_rx));
    CHECK(diag().rtd.tick_future_clamped == 1u);
    return 0;
}

/* nowMs wraps at 32 bits: permission supervision, TX supervision, receive
 * time and the waiting AGE of VSS_STATUS all run across the wrap. */
static int e3_now_wrap(void)
{
    const uint64_t wrap = UINT64_C(0x100000000);
    NetWireCanEnvelope pe, ee;
    NetVssStatus s1 = status_ready(1u, 0u), s2 = status_ready(2u, 5u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint64_t perm_at, sent_at, s2_at, s2_sent = 0u, t_ev = 0u;
    bool ev_injected = false, ev_checked = false, abort_seen = false, s2_checked = false;

    CHECK(start_at(tick_for_ms(0xFFFFFF00u)));  /* 256 ms before the wrap */
    pass();
    CHECK(app_now() == 0xFFFFFF00u);
    run_until_ms(0xFFFFFFB0u);

    /* DOMAIN_PERMISSION: 150 ms receive timeout ends after the wrap. */
    CHECK(encode_permission(1u, &pe));
    perm_at = app_ms64();
    CHECK(inject(&pe) >= 0);
    pass();
    d = diag();
    CHECK(d.app.permission_received == 1u);
    CHECK(d.app.last_permission_meta.received_at_ms == (uint32_t)perm_at);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT);

    /* VSS_STATUS with nobody ACKing: the 100 ms TX supervision ends after the wrap. */
    MockRtd_CanSetAckMissing(BUS, true);
    CHECK(VssNet_PublishStatus(&s1) == NET_SUBMIT_OK);
    run_ticks(1u);
    sent_at = app_ms64();
    CHECK(diag().rtd.tx_sent == 1u);
    /* A newer evaluation waits for that frame; its waiting AGE spans the wrap. */
    run_ticks(1u);
    CHECK(VssNet_PublishStatus(&s2) == NET_SUBMIT_OK);
    s2_at = app_ms64();

    while (app_ms64() < wrap + 200u) {
        ++board_tick;
        if (!ev_injected && app_ms64() >= wrap - 3u) {
            /* VSS_EVENT received just before the wrap, delivered after it. */
            CHECK(encode_event(1u, 7u, &ee));
            t_ev = board_tick;
            CHECK(inject(&ee) >= 0);
            ev_injected = true;
            board_tick += 6u;
        }
        pass();
        d = diag();
        CHECK(d.permission_valid);
        if (app_ms64() - perm_at < 150u) CHECK(d.permission.reason == NET_PERMISSION_CURRENT);
        else CHECK(d.permission.reason == NET_PERMISSION_RX_TIMEOUT);
        if (app_ms64() - sent_at < 100u) {
            CHECK(d.rtd.tx_timeout_aborts == 0u);
        } else {
            CHECK(d.rtd.tx_timeout_aborts == 1u);
            if (!abort_seen) { abort_seen = true; MockRtd_CanSetAckMissing(BUS, false); }
        }
        if (s2_sent == 0u && d.rtd.tx_sent == 2u) {
            s2_sent = app_ms64();
            CHECK(pull_status(&f, &raw) >= 0);
            CHECK(raw.boot_id == NODE_BOOT && raw.update_sequence == 2u && raw.tx_sequence == 2u);
            CHECK(s2_at < wrap && s2_sent >= wrap);
            CHECK(raw.source_age_ms == 5u + (s2_sent - s2_at)); /* submitted AGE + wait */
            s2_checked = true;
        }
        if (ev_injected && !ev_checked) {
            CHECK(event_count == 1);
            CHECK(last_event_meta.received_at_ms == ms_at(t_ev));
            CHECK(ms_at(t_ev) >= 0xFFFFFFFDu && app_now() < 0x10u);
            ev_checked = true;
        }
    }
    CHECK(ev_checked && abort_seen && s2_checked);
    CHECK(s2_sent - sent_at >= 100u);
    d = diag();
    CHECK(d.upper.tx_failed == 1u);
    CHECK(d.entry_now_ms == app_ms64() && d.entry_now_ms > wrap);
    CHECK(d.entry_now_backward == 0u);
    CHECK(d.rtd.tick_future_clamped == 0u);
    return 0;
}

/* VSS_STATUS is submitted by the application (on change and every 250 ms):
 * one frame per submission through the public entry, never repeated. */
static int e4_status_not_repeated(void)
{
    NetVssStatus s = status_ready(500u, 12u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint64_t submitted_at;
    unsigned frames = 0u;
    uint32_t n;
    CHECK(start());
    run_ticks(8u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    submitted_at = app_ms64();
    while (app_ms64() < submitted_at + 2000u) {
        run_ticks(1u);
        while (pull_status(&f, &raw) >= 0) {
            ++frames;
            CHECK(f.id == 0x340u && f.fd && f.brs && f.length == 32u);
            CHECK(raw.boot_id == NODE_BOOT && raw.tx_sequence == 1u && raw.update_sequence == 500u);
        }
    }
    CHECK(frames == 1u);
    for (n = 1u; n <= 8u; ++n) {
        s = status_ready(600u + n, 3u);
        CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
        submitted_at = app_ms64();
        run_ticks(1u);
        CHECK(pull_status(&f, &raw) >= 0);
        CHECK(raw.boot_id == NODE_BOOT && raw.tx_sequence == 1u + n);
        CHECK(raw.update_sequence == 600u + n);
        CHECK(raw.source_age_ms == 3u + (app_ms64() - submitted_at)); /* submitted AGE + wait to the pass */
        CHECK(transmit_next(NULL) < 0);
        run_until_ms(submitted_at + 250u);
        CHECK(transmit_next(NULL) < 0);
    }
    d = diag();
    CHECK(d.status_tx.period_ms == 0u && d.status_tx.counters.periodic_sent == 0u);
    CHECK(d.status_tx.counters.sent == 9u && d.app.status_submitted == 9u);
    CHECK(d.app.next_status_sequence == 10u);
    return 0;
}

/* Three drain budgets of frames between two passes: each pass takes at most
 * one budget, the rest follows in the next passes, nothing is lost and every
 * frame keeps the time of its own ISR. */
static int e5_drain_budget(void)
{
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint64_t ticks[64];
    uint32_t budget, total, i;
    CHECK(start());
    run_ticks(2u);
    d = diag();
    budget = d.rtd.isr_drain_budget;
    total = 3u * budget;
    CHECK(budget >= 1u && total <= d.rtd.isr_ring_capacity && total <= 64u);
    for (i = 0u; i < total; ++i) {
        if (i % 4u == 0u) ++board_tick;  /* frames spread over several ticks */
        CHECK(encode_event(i + 1u, 1000u + i, &e));
        ticks[i] = board_tick;
        CHECK(inject(&e) >= 0);
    }
    CHECK(event_count == 0);
    d = diag();
    CHECK(d.rtd.isr_ring_max_depth == total);
    for (i = 1u; i <= 3u; ++i) {
        run_ticks(1u);
        d = diag();
        CHECK(event_count == (int)(i * budget));
        CHECK(d.rtd.main_drained_max == budget);
    }
    run_ticks(2u);
    CHECK(event_count == (int)total);
    for (i = 0u; i < total; ++i) {
        CHECK(event_metas[i].source.tx_sequence == i + 1u);
        CHECK(events[i].occurrence_id == 1000u + i);
        CHECK(event_metas[i].received_at_ms == ms_at(ticks[i]));
    }
    d = diag();
    CHECK(d.rtd.isr_ring_overflow == 0u && d.rtd.rx_queue_rejected == 0u);
    CHECK(d.hal.rx_dropped == 0u && d.upper.errors == 0u);
    CHECK(d.rtd.main_drained_max == budget);
    return 0;
}

/* Return values: before Net_Init and with the link locked (bus-off) the
 * refusal is temporary (NOT_READY, VSS resubmits); only argument errors are
 * INVALID_ARGUMENT. */
static int e6_submit_results(void)
{
    NetVssStatus s = status_ready(1u, 0u), bad = status_ready(2u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d0, d;
    /* History quality OK but no original occurrence: invalid field combination. */
    bad.vss_last_fault = NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE;
    bad.last_fault_quality = NET_QUALITY_OK;
    bad.last_fault_occurrence = 0u;
    bad.last_fault_age_ms = 10u;

    board_reset(START_TICK);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    CHECK(VssNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    pass();
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);

    Net_Init();
    CHECK(diag().entry_init_result == NET_ENTRY_INIT_OK);
    run_ticks(2u);
    CHECK(VssNet_PublishStatus(&bad) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(VssNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    run_ticks(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.update_sequence == 1u && raw.tx_sequence == 1u);
    CHECK(transmit_next(NULL) < 0);     /* the invalid one never reached the bus */
    run_ticks(2u);

    /* Bus-off from the error ISR: the link stays locked until reset. */
    d0 = diag();
    in_isr = true;
    MockRtd_CanRaiseBusOff(BUS);
    in_isr = false;
    run_ticks(1u);
    d = diag();
    CHECK(d.rtd.busoff_events == 1u && d.upper.errors == d0.upper.errors + 1u);
    s = status_ready(3u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    CHECK(VssNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    run_ticks(5u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    CHECK(transmit_next(NULL) < 0);
    CHECK(diag().stage == NET_NODE_STAGE_READY);
    return 0;
}

/* Callbacks run inside Net_MainFunction only: never during an ISR, never
 * nested; a frame arriving while a callback runs waits for a later pass. */
static int e7_callback_context(void)
{
    NetWireCanEnvelope ev, wa, pe, late;
    NetVssStatus s = status_ready(1u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    uint64_t t_late;
    CHECK(start());
    run_ticks(2u);
    CHECK(encode_event(1u, 7u, &ev));
    CHECK(encode_warning(1u, &wa));
    CHECK(encode_permission(1u, &pe));
    CHECK(encode_event(2u, 8u, &late));
    CHECK(inject(&ev) >= 0);
    CHECK(inject(&wa) >= 0);
    CHECK(inject(&pe) >= 0);
    CHECK(event_count == 0 && warning_count == 0);
    isr_during_callback = &late;        /* arrives while VssApp_OnDomainEvent runs */
    t_late = board_tick + 1u;
    run_ticks(1u);
    CHECK(event_count == 1 && warning_count == 1);
    CHECK(isr_during_callback == NULL);
    CHECK(diag().app.permission_received == 1u);
    run_ticks(1u);
    CHECK(event_count == 2 && last_event.occurrence_id == 8u);
    CHECK(last_event_meta.received_at_ms == ms_at(t_late));
    /* TX completion from the ISR as well. */
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    run_ticks(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    run_ticks(2u);
    CHECK(diag().upper.tx_succeeded == 1u);
    CHECK(callbacks_in_isr == 0 && callbacks_outside_main == 0 && callback_depth_max == 1);
    CHECK(conversions_in_isr == 0u && conversions_outside_main == 0u);
    CHECK(isr_tick_reads == 5u && tick_reads_outside_isr == 0u); /* 4 RX + 1 TX completion */
    return 0;
}

/* The BOOT_ID comes from the board node configuration hook. */
static int e8_node_config_boot(void)
{
    NetVssStatus s = status_ready(9u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    config_boot = 0x13572468u;
    CHECK(start());
    CHECK(config_calls == 1u);
    /* The hook gets boot_id 0 and both epochs 1, and must set the BOOT_ID. */
    CHECK(config_seen.boot_id == 0u && config_seen.local_epoch == 1u && config_seen.can_port_epoch == 1u);
    d = diag();
    CHECK(d.app.boot_id == 0x13572468u && d.status_tx.boot_id == 0x13572468u);
    run_ticks(2u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    run_ticks(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.boot_id == 0x13572468u && raw.tx_sequence == 1u && raw.update_sequence == 9u);
    return 0;
}

static int inert_after_failed_init(uint8_t expected)
{
    NetWireCanEnvelope e;
    NetVssStatus s = status_ready(1u, 0u);
    NetNodeDiag d = diag();
    CHECK(config_calls == 1u);
    CHECK(d.entry_init_result == expected && d.entry_init_calls == 1u);
    CHECK(d.stage == NET_NODE_STAGE_NONE && !d.rtd.started);
    CHECK(encode_event(1u, 7u, &e));
    CHECK(inject(&e) < 0);              /* controller never started */
    run_ticks(3u);
    d = diag();
    CHECK(d.steps == 0u && d.entry_main_calls == 3u && d.entry_now_ms == 0u);
    CHECK(event_count == 0);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    CHECK(transmit_next(NULL) < 0);
    /* A later Net_Init does not retry: once per power cycle. */
    Net_Init();
    d = diag();
    CHECK(config_calls == 1u && d.entry_init_calls == 2u && d.entry_init_result == expected);
    CHECK(d.stage == NET_NODE_STAGE_NONE);
    return 0;
}

/* BOOT_ID 0 is reserved: Net_Init fails and says why. */
static int e8_node_config_boot_zero(void)
{
    config_boot = 0u;
    board_reset(START_TICK);
    Net_Init();
    return inert_after_failed_init((uint8_t)NET_ENTRY_INIT_BOOT_ID_ZERO);
}

static int e8_node_config_refused(void)
{
    config_accept = false;
    board_reset(START_TICK);
    Net_Init();
    return inert_after_failed_init((uint8_t)NET_ENTRY_INIT_CONFIG_REFUSED);
}

/* ISR ring full: the frame is dropped and counted, the loss is flagged on the
 * next captured frame (a real loss, nothing synthesized), the link keeps
 * working. */
static int e9_ring_overflow(void)
{
    NetWireCanEnvelope e;
    NetVssStatus s = status_ready(1u, 0u);
    NetNodeDiag d0, d;
    uint32_t capacity, i, lost = 3u;
    CHECK(start());
    run_ticks(2u);
    d0 = diag();
    capacity = d0.rtd.isr_ring_capacity;
    CHECK(capacity >= 1u && capacity + lost <= 64u);
    for (i = 0u; i < capacity + lost; ++i) {
        CHECK(encode_event(i + 1u, 1000u + i, &e));
        CHECK(inject(&e) >= 0);         /* the MB is re-armed: the ring drops it */
    }
    d = diag();
    CHECK(d.rtd.isr_ring_overflow == lost);
    CHECK(d.rtd.isr_ring_max_depth == capacity);
    run_ticks(capacity / d.rtd.isr_drain_budget + 2u);
    CHECK(event_count == (int)capacity);
    CHECK(event_metas[capacity - 1u].source.tx_sequence == capacity);
    /* The next stored frame carries the loss: not delivered as if nothing was lost. */
    CHECK(encode_event(capacity + lost + 1u, 3000u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    d = diag();
    CHECK(event_count == (int)capacity);
    CHECK(d.upper.rx_held == d0.upper.rx_held + 1u);
    CHECK(encode_event(capacity + lost + 2u, 3001u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    CHECK(event_count == (int)capacity + 1 && last_event.occurrence_id == 3001u);
    d = diag();
    CHECK(d.rtd.isr_ring_overflow == lost);
    CHECK(d.upper.errors == d0.upper.errors && d.hal.rx_dropped == 0u); /* no link lock */
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    run_ticks(1u);
    CHECK(transmit_next(NULL) >= 0);
    return 0;
}

/* Board microsecond hook: the longest Net_MainFunction pass is kept (modulo
 * 2^32 counter). VSS contract: a few ms, never above 15 ms. */
static int e10_main_max_us(void)
{
    NetWireCanEnvelope e;
    CHECK(start());
    CHECK(diag().main_max_us == 0u);
    board_us = 0xFFFFF000u;             /* the counter wraps during this case */
    callback_work_us = 3700u;           /* each delivered Event costs 3.7 ms */
    run_ticks(2u);
    CHECK(diag().main_max_us == 0u);
    CHECK(encode_event(1u, 1u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    CHECK(diag().main_max_us == 3700u);
    CHECK(encode_event(2u, 2u, &e));
    CHECK(inject(&e) >= 0);
    CHECK(encode_event(3u, 3u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(1u);
    CHECK(board_us < 0xFFFFF000u);      /* wrapped inside that pass */
    CHECK(event_count == 3 && diag().main_max_us == 7400u);
    CHECK(encode_event(4u, 4u, &e));
    CHECK(inject(&e) >= 0);
    run_ticks(3u);
    CHECK(diag().main_max_us == 7400u && diag().main_max_us < 15000u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "e1_init", e1_init }, { "e2_clock_axis", e2_clock_axis },
        { "e2_future_tick_clamped", e2_future_tick_clamped }, { "e3_now_wrap", e3_now_wrap },
        { "e4_status_not_repeated", e4_status_not_repeated }, { "e5_drain_budget", e5_drain_budget },
        { "e6_submit_results", e6_submit_results }, { "e7_callback_context", e7_callback_context },
        { "e8_node_config_boot", e8_node_config_boot },
        { "e8_node_config_boot_zero", e8_node_config_boot_zero },
        { "e8_node_config_refused", e8_node_config_refused },
        { "e9_ring_overflow", e9_ring_overflow }, { "e10_main_max_us", e10_main_max_us }
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
