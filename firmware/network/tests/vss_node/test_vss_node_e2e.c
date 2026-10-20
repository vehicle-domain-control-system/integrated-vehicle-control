/* VSS node end-to-end on the host: real product sources (App Port facade ->
 * typed ECU adapter -> NetUpper -> NetHalAdapter -> RTD backend) linked with
 * the independently written RTD mock SDK, which plays the CAN bus.
 * Contract: docs/network/interface/02_NODES/VSS/{10_DATA,20_API}.md,
 *           docs/network/interface/03_COMMUNICATION/10_CAN_FD.md.
 * Frames are built and checked with the repository codec (net_wire_can_*).
 * One case per process: NetNode_Init succeeds once per process.
 * Time source: OsIf system counter (default build) or NET_BOARD_NOW_MS_HOOK
 * (VSS_NODE_TEST_TIME_HOOK build, OsIf timer OFF as on the bring-up board). */
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

#ifdef VSS_NODE_TEST_TIME_HOOK
/* Board-owned monotonic ms clock (NET_BOARD_NOW_MS_HOOK=MockBoard_NowMs). */
uint64_t MockBoard_NowMs(void);
uint64_t MockBoard_NowMs(void) { return MockRtd_NowUs() / 1000u; }
#define TIMER_ON_FOR_THIS_BUILD false
#else
#define TIMER_ON_FOR_THIS_BUILD true
#endif

/* ------------------------------------------------------ App callbacks ---- */
static int event_count, warning_count;
static NetVssEvent last_event;
static NetVssWarningState last_warning;
static NetCanRxMeta last_event_meta, last_warning_meta;
static NetVssEvent events[32];
static NetCanRxMeta event_metas[32];
static int publish_in_callback;          /* number of Publish calls per event callback */
static NetSubmitResult callback_results[2];
static NetVssStatus callback_status[2];

void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)
{
    int i;
    if (event_count < 32) { events[event_count] = *message; event_metas[event_count] = *meta; }
    ++event_count;
    last_event = *message;
    last_event_meta = *meta;
    for (i = 0; i < publish_in_callback && i < 2; ++i)
        callback_results[i] = VssNet_PublishStatus(&callback_status[i]);
}

void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)
{
    ++warning_count;
    last_warning = *message;
    last_warning_meta = *meta;
}

/* ------------------------------------------------------------ helpers ---- */
static uint32_t now_ms(void) { return (uint32_t)(MockRtd_NowUs() / 1000u); }

static NetStatus start_node(void)
{
    NetNodeConfig c;
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(TIMER_ON_FOR_THIS_BUILD);
    memset(&c, 0, sizeof(c));
    c.boot_id = NODE_BOOT;
    c.local_epoch = 1u;
    c.can_port_epoch = 1u;
    return NetNode_Init(&c);
}

static void step_ms(unsigned ms)
{
    unsigned i;
    for (i = 0u; i < ms; ++i) { MockRtd_AdvanceUs(1000u); NetNode_Step(); }
}

static NetNodeDiag diag(void)
{
    NetNodeDiag d;
    (void)NetNode_GetDiag(&d);
    return d;
}

static int inject_frame(const NetWireCanEnvelope *e, bool fd, bool brs, uint8 dlc, bool overrun)
{
    MockCanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = e->standard_id;
    f.fd = fd;
    f.brs = brs;
    f.dlc = dlc;
    f.length = (uint8)e->length;
    f.overrun = overrun;
    memcpy(f.data, e->bytes, e->length);
    return MockRtd_CanInject(BUS, &f);
}

static int inject(const NetWireCanEnvelope *e) { return inject_frame(e, e->fdf, e->brs, e->dlc, false); }

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

static bool encode_event(const NetWireVssEventRaw *r, NetWireCanEnvelope *e)
{
    return net_wire_can_encode_vss_event(NET_WIRE_CANDIDATE_ONLY, r, e) == NET_WIRE_OK;
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

/* Pull the next frame the node put on the bus and decode it as VSS_STATUS. */
static int pull_status(MockCanFrame *out, NetWireVssStatusRaw *raw)
{
    NetWireCanEnvelope e;
    int mb = MockRtd_CanTransmitNext(BUS, out);
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

/* -------------------------------------------------------------- cases ---- */
static int h01_init(void)
{
    NetNodeConfig again = { NODE_BOOT + 1u, 1u, 1u, 0u, 0u }; /* no UART link in VSS */
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY);
    CHECK(d.stage_status == NET_OK);
    CHECK(d.rtd.started);
    CHECK(d.rtd.init_failed_step == NET_HAL_RTD_STEP_NONE);
    CHECK(d.app.bound && d.app.boot_id == NODE_BOOT && d.app.next_status_sequence == 1u);
    /* A second Init is refused and leaves the running node untouched. */
    CHECK(NetNode_Init(&again) == NET_BUSY);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY && d.rtd.started && d.app.boot_id == NODE_BOOT);
    step_ms(3u);
    CHECK(diag().steps == 3u);
    return 0;
}

static int h02_event_rx(void)
{
    NetWireVssEventRaw r;
    NetWireCanEnvelope e;
    uint32_t injected_at;
    CHECK(start_node() == NET_OK);
    step_ms(5u);
    r = event_raw(1u, 7u);
    r.event_kind = NET_VSS_EVENT_LOCK_COMPLETE;
    r.original_age_ms = 37u;
    r.use_limit_ms = 400u;
    r.related_command_id = 0x01020304u;
    CHECK(encode_event(&r, &e));
    CHECK(e.standard_id == 0x160u && e.fdf && e.brs);
    MockRtd_AdvanceUs(2500u);
    injected_at = now_ms();
    CHECK(inject(&e) >= 0);
    CHECK(event_count == 0); /* never from the ISR */
    step_ms(1u);
    CHECK(event_count == 1);
    CHECK(warning_count == 0);
    CHECK(last_event.event_kind == NET_VSS_EVENT_LOCK_COMPLETE);
    CHECK(last_event.occurrence_id == 7u);
    CHECK(last_event.original_age_ms == 37u); /* RX basis: not advanced to delivery */
    CHECK(last_event.use_limit_ms == 400u);
    CHECK(last_event.value_quality == NET_QUALITY_OK);
    CHECK(last_event.input_availability == NET_AVAILABILITY_AVAILABLE);
    CHECK(last_event.related_command_id == 0x01020304u);
    CHECK(last_event_meta.source.boot_id == PEER_BOOT);
    CHECK(last_event_meta.source.tx_sequence == 1u);
    CHECK(last_event_meta.received_at_ms == injected_at);
    CHECK(diag().app.event_delivered == 1u);
    return 0;
}

static int h03_order_duplicate(void)
{
    NetWireVssEventRaw r;
    NetWireCanEnvelope e1, e2, e3;
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(1u);
    r = event_raw(1u, 7u);
    CHECK(encode_event(&r, &e1));
    r = event_raw(2u, 8u);
    CHECK(encode_event(&r, &e2));
    r = event_raw(3u, 9u);
    CHECK(encode_event(&r, &e3));

    CHECK(inject(&e1) >= 0);
    step_ms(2u);
    CHECK(event_count == 1);
    d0 = diag();
    /* Exact duplicate (same TX, same bytes): no callback. */
    CHECK(inject(&e1) >= 0);
    step_ms(2u);
    CHECK(event_count == 1);
    d = diag();
    CHECK(d.upper.duplicates == d0.upper.duplicates + 1u);
    /* Newer TX: callback. */
    CHECK(inject(&e2) >= 0);
    step_ms(2u);
    CHECK(event_count == 2 && last_event.occurrence_id == 8u && last_event_meta.source.tx_sequence == 2u);
    /* Older TX: no callback. */
    d0 = diag();
    CHECK(inject(&e1) >= 0);
    step_ms(2u);
    CHECK(event_count == 2);
    d = diag();
    CHECK(d.upper.stale == d0.upper.stale + 1u);
    /* Next newer TX still delivered. */
    CHECK(inject(&e3) >= 0);
    step_ms(2u);
    CHECK(event_count == 3 && last_event.occurrence_id == 9u && last_event_meta.source.tx_sequence == 3u);
    return 0;
}

static NetWireVssWarningStateRaw warning_raw(uint32_t tx)
{
    NetWireVssWarningStateRaw w;
    memset(&w, 0, sizeof(w));
    w.boot_id = PEER_BOOT;
    w.tx_sequence = tx;
    w.antipinch_source_boot = 0x57494E31u;
    w.antipinch_source_sequence = 11u;
    w.antipinch_original_age_ms = 120u;
    w.antipinch_state = NET_FLAG_YES;
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
    w.rear_obstacle_state = NET_REAR_WARNING_CAUTION;
    w.rear_obstacle_quality = NET_QUALITY_OK;
    w.rear_obstacle_reason = NET_QUALITY_REASON_NONE;
    w.rear_obstacle_availability = NET_AVAILABILITY_AVAILABLE;
    w.rear_detection_activation = NET_FLAG_YES;
    w.activation_meta_format = 1u;
    w.rear_activation_quality = NET_QUALITY_OK;
    w.rear_activation_reason = NET_QUALITY_REASON_NONE;
    w.rear_activation_sequence = 44u;
    w.rear_activation_age_ms = 55u;
    return w;
}

static int h04_warning_rx(void)
{
    NetWireVssWarningStateRaw w = warning_raw(1u);
    NetWireCanEnvelope e;
    uint32_t injected_at;
    CHECK(start_node() == NET_OK);
    step_ms(3u);
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, &e) == NET_WIRE_OK);
    CHECK(e.standard_id == 0x0C0u && e.length == 64u && e.fdf);
    injected_at = now_ms();
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(warning_count == 1);
    CHECK(event_count == 0);
    CHECK(last_warning_meta.source.boot_id == PEER_BOOT && last_warning_meta.source.tx_sequence == 1u);
    CHECK(last_warning_meta.received_at_ms == injected_at);
    CHECK(last_warning.antipinch_source_boot == w.antipinch_source_boot);
    CHECK(last_warning.antipinch_source_sequence == w.antipinch_source_sequence);
    CHECK(last_warning.antipinch_original_age_ms == 120u);
    CHECK(last_warning.antipinch_state == w.antipinch_state);
    CHECK(last_warning.antipinch_quality == w.antipinch_quality);
    CHECK(last_warning.antipinch_reason == w.antipinch_reason);
    CHECK(last_warning.antipinch_availability == w.antipinch_availability);
    CHECK(last_warning.occupant_hazard_source_boot == w.occupant_hazard_source_boot);
    CHECK(last_warning.occupant_hazard_source_sequence == w.occupant_hazard_source_sequence);
    CHECK(last_warning.occupant_hazard_original_age_ms == 230u);
    CHECK(last_warning.occupant_hazard_state == w.occupant_hazard_state);
    CHECK(last_warning.occupant_hazard_quality == w.occupant_hazard_quality);
    CHECK(last_warning.occupant_hazard_reason == w.occupant_hazard_reason);
    CHECK(last_warning.occupant_hazard_availability == w.occupant_hazard_availability);
    CHECK(last_warning.rear_obstacle_source_boot == w.rear_obstacle_source_boot);
    CHECK(last_warning.rear_obstacle_source_sequence == w.rear_obstacle_source_sequence);
    CHECK(last_warning.rear_obstacle_original_age_ms == 340u);
    CHECK(last_warning.rear_obstacle_state == w.rear_obstacle_state);
    CHECK(last_warning.rear_obstacle_quality == w.rear_obstacle_quality);
    CHECK(last_warning.rear_obstacle_reason == w.rear_obstacle_reason);
    CHECK(last_warning.rear_obstacle_availability == w.rear_obstacle_availability);
    CHECK(last_warning.rear_detection_activation == w.rear_detection_activation);
    CHECK(last_warning.activation_meta_format == w.activation_meta_format);
    CHECK(last_warning.rear_activation_quality == w.rear_activation_quality);
    CHECK(last_warning.rear_activation_reason == w.rear_activation_reason);
    CHECK(last_warning.rear_activation_sequence == w.rear_activation_sequence);
    CHECK(last_warning.rear_activation_age_ms == 55u);
    CHECK(diag().app.warning_delivered == 1u);
    return 0;
}

static int h05_permission_rx(void)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint32_t injected_at;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    memset(&p, 0, sizeof(p));
    p.boot_id = PEER_BOOT;
    p.tx_sequence = 1u;
    p.vehicle_power_permission = NET_TRISTATE_YES;
    p.power_permission_quality = NET_QUALITY_OK;
    p.power_operation_state = NET_TRISTATE_YES;
    p.operation_allowed = NET_FLAG_YES;
    p.operation_quality = NET_QUALITY_OK;
    p.update_sequence = 77u;
    p.source_age_ms = 10u;
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, &e) == NET_WIRE_OK);
    CHECK(e.standard_id == 0x080u);
    injected_at = now_ms();
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(event_count == 0 && warning_count == 0); /* never a VSS App callback */
    d = diag();
    CHECK(d.app.permission_received == 1u && d.app.permission_seen);
    CHECK(d.app.last_permission.vehicle_power_permission == p.vehicle_power_permission);
    CHECK(d.app.last_permission.power_permission_quality == p.power_permission_quality);
    CHECK(d.app.last_permission.power_operation_state == p.power_operation_state);
    CHECK(d.app.last_permission.operation_allowed == p.operation_allowed);
    CHECK(d.app.last_permission.operation_quality == p.operation_quality);
    CHECK(d.app.last_permission.update_sequence == 77u);
    CHECK(d.app.last_permission.source_age_ms == 10u);
    CHECK(d.app.last_permission_meta.source.boot_id == PEER_BOOT);
    CHECK(d.app.last_permission_meta.source.tx_sequence == 1u);
    CHECK(d.app.last_permission_meta.received_at_ms == injected_at);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT);
    /* Receive timeout 150 ms (10_CAN_FD.md DOMAIN_PERMISSION). */
    while (now_ms() < injected_at + 149u) step_ms(1u);
    d = diag();
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT);
    step_ms(1u);
    d = diag();
    CHECK(now_ms() == injected_at + 150u);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_RX_TIMEOUT);
    CHECK(!d.permission.power_usable);
    CHECK(d.app.permission_received == 1u);
    return 0;
}

static int h06_malformed_rx(void)
{
    NetWireVssEventRaw r = event_raw(1u, 7u), r2;
    NetWireCanEnvelope good, other, bad;
    NetNodeDiag d0, d;
    size_t kind_at = 0u, i;
    CHECK(start_node() == NET_OK);
    step_ms(1u);
    CHECK(encode_event(&r, &good));
    /* Locate EVENT_KIND through the codec itself (two encodings that differ
     * only in that field), then corrupt it to a reserved value. */
    r2 = r;
    r2.event_kind = NET_VSS_EVENT_GOODBYE;
    CHECK(encode_event(&r2, &other));
    for (i = 0u; i < good.length; ++i) if (good.bytes[i] != other.bytes[i]) { kind_at = i; break; }
    CHECK(i < good.length);
    d0 = diag();

    bad = good;
    bad.bytes[kind_at] = 0u;                       /* reserved kind 0 */
    CHECK(inject(&bad) >= 0);
    bad.bytes[kind_at] = 6u;                       /* past LOCK_ERROR */
    CHECK(inject(&bad) >= 0);
    step_ms(1u);
    /* Length / DLC not the registered 24 B: longer (32 B) and shorter (20 B). */
    bad = good;
    bad.length = 32u;
    CHECK(inject_frame(&bad, true, true, MockRtd_DlcForLength(32u), false) >= 0);
    bad.length = 20u;
    CHECK(inject_frame(&bad, true, true, MockRtd_DlcForLength(20u), false) >= 0);
    step_ms(1u);
    /* Classic CAN frame (no FDF): first 8 bytes. */
    bad = good;
    bad.length = 8u;
    CHECK(inject_frame(&bad, false, false, 8u, false) >= 0);
    step_ms(2u);
    CHECK(event_count == 0);
    d = diag();
    CHECK(d.upper.rx_invalid == d0.upper.rx_invalid + 5u);
    CHECK(d.rtd.rx_frames == d0.rtd.rx_frames + 5u);
    CHECK(d.upper.rx_delivered == d0.upper.rx_delivered);
    /* Nothing was admitted: the valid TX=1 frame is still new. */
    CHECK(inject(&good) >= 0);
    step_ms(1u);
    CHECK(event_count == 1 && last_event_meta.source.tx_sequence == 1u);
    return 0;
}

static int h07_publish(void)
{
    NetVssStatus s;
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    uint32_t n;
    CHECK(start_node() == NET_OK);
    step_ms(10u);
    for (n = 1u; n <= 3u; ++n) {
        s = status_ready(100u + n * 7u, (uint16_t)(20u * n));
        if (n == 3u) {
            /* History present: quality OK with an original occurrence. */
            s.vss_state = NET_VSS_STATE_FAULT;
            s.vss_availability = NET_VSS_AVAILABILITY_DEGRADED;
            s.vss_accepting_events = NET_FLAG_NO;
            s.vss_fault_active = NET_FLAG_YES;
            s.fault_mask = NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE;
            s.vss_last_fault = NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE;
            s.last_fault_occurrence = 9u;
            s.last_fault_age_ms = 1000u;
            s.last_fault_quality = NET_QUALITY_OK;
        }
        CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
        /* Queue wait before the next step: 5 ms. */
        MockRtd_AdvanceUs(5000u);
        NetNode_Step();
        CHECK(pull_status(&f, &raw) >= 0);
        CHECK(f.id == 0x340u && f.length == 32u && f.fd && f.brs);
        CHECK(raw.boot_id == NODE_BOOT);
        CHECK(raw.tx_sequence == n);
        CHECK(raw.update_sequence == s.update_sequence);
        CHECK(raw.source_age_ms == (uint16_t)(s.source_age_ms + 5u));
        CHECK(raw.vss_state == s.vss_state);
        CHECK(raw.vss_availability == s.vss_availability);
        CHECK(raw.vss_accepting_events == s.vss_accepting_events);
        CHECK(raw.vss_fault_active == s.vss_fault_active);
        CHECK(raw.fault_mask == s.fault_mask);
        CHECK(raw.vss_last_fault == s.vss_last_fault);
        CHECK(raw.last_fault_occurrence == s.last_fault_occurrence);
        CHECK(raw.last_fault_quality == s.last_fault_quality);
        if (s.last_fault_quality == NET_QUALITY_OK) CHECK(raw.last_fault_age_ms == (uint16_t)(s.last_fault_age_ms + 5u));
        else CHECK(raw.last_fault_age_ms == 0u);
        CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0); /* exactly one frame */
        step_ms(3u);
    }
    CHECK(diag().app.status_submitted == 3u);
    CHECK(diag().app.next_status_sequence == 4u);
    return 0;
}

static int h08_publish_invalid(void)
{
    NetVssStatus s;
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(VssNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    /* History quality OK but no original occurrence. */
    s = status_ready(1u, 0u);
    s.vss_last_fault = NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE;
    s.last_fault_quality = NET_QUALITY_OK;
    s.last_fault_occurrence = 0u;
    s.last_fault_age_ms = 10u;
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_INVALID_ARGUMENT);
    /* Reserved state code. */
    s = status_ready(2u, 0u);
    s.vss_state = 9u;
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_INVALID_ARGUMENT);
    step_ms(5u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
    CHECK(diag().rtd.tx_sent == 0u);
    /* Rejected submissions did not consume a TX_SEQUENCE. */
    s = status_ready(3u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.tx_sequence == 1u && raw.update_sequence == 3u);
    return 0;
}

static int h09_publish_in_callback(void)
{
    NetWireVssEventRaw r = event_raw(1u, 7u);
    NetWireCanEnvelope e;
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    callback_status[0] = status_ready(11u, 0u);
    callback_status[1] = status_ready(12u, 3u);
    publish_in_callback = 2;
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    MockRtd_AdvanceUs(1000u);
    NetNode_Step();                     /* one step: delivery, Publish x2, TX */
    CHECK(event_count == 1);
    CHECK(callback_results[0] == NET_SUBMIT_OK);
    CHECK(callback_results[1] == NET_SUBMIT_OK);
    CHECK(pull_status(&f, &raw) >= 0);  /* sent inside the same step */
    CHECK(raw.update_sequence == 12u);  /* latest wins */
    CHECK(raw.tx_sequence == 1u);
    CHECK(raw.source_age_ms == 3u);
    publish_in_callback = 0;
    step_ms(5u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0); /* the replaced one is never sent */
    d = diag();
    CHECK(d.app.status_deferred == 2u);
    CHECK(d.app.status_replaced == 1u);
    CHECK(d.app.status_submitted == 1u);
    return 0;
}

static int h10_ack_missing(void)
{
    NetVssStatus s = status_ready(1u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t sent_at;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_CanSetAckMissing(BUS, true);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    sent_at = now_ms();
    CHECK(diag().rtd.tx_sent == 1u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0); /* nobody ACKs: stays in the MB */
    while (now_ms() < sent_at + 99u) step_ms(1u);
    d = diag();
    CHECK(d.rtd.tx_timeout_aborts == 0u && MockRtd_CanAborts(BUS) == 0u);
    step_ms(1u);                        /* 100 ms after submission */
    d = diag();
    CHECK(d.rtd.tx_timeout_aborts == 1u);
    CHECK(MockRtd_CanAborts(BUS) == 1u);
    step_ms(2u);
    d = diag();
    CHECK(d.upper.tx_failed == 1u && d.upper.tx_succeeded == 0u);
    CHECK(d.upper.tx_released == 1u);   /* slot returned */
    CHECK(d.rtd.tx_complete == 0u);
    /* ACK back: the next Publish goes out. */
    MockRtd_CanSetAckMissing(BUS, false);
    s = status_ready(2u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.update_sequence == 2u && raw.tx_sequence == 2u);
    step_ms(2u);
    d = diag();
    CHECK(d.rtd.tx_complete == 1u && d.upper.tx_succeeded == 1u);
    return 0;
}

static int h11_busoff(void)
{
    NetWireVssEventRaw r = event_raw(1u, 7u);
    NetWireCanEnvelope e;
    NetVssStatus s = status_ready(1u, 0u);
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    d0 = diag();
    MockRtd_CanRaiseBusOff(BUS);
    d = diag();
    CHECK(d.rtd.busoff_events == 1u && d.rtd.err_events == 1u);
    step_ms(1u);
    d = diag();
    CHECK(d.upper.errors == d0.upper.errors + 1u); /* link error reported to M04 */
    /* CAN link held until reset: RX not delivered, TX not accepted. */
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(2u);
    CHECK(event_count == 0);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    step_ms(5u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY);
    CHECK(d.upper.rx_delivered == d0.upper.rx_delivered);
    CHECK(d.rtd.tx_sent == 0u);
    CHECK(d.permission_valid && !d.permission.power_usable);
    return 0;
}

static int h12_time_source(void)
{
#ifdef VSS_NODE_TEST_TIME_HOOK
    /* OsIf timer OFF (bring-up configuration) + board hook: READY, hook time. */
    NetWireVssEventRaw r = event_raw(1u, 7u);
    NetWireCanEnvelope e;
    CHECK(start_node() == NET_OK);
    CHECK(OsIf_MicrosToTicks(1000u, OSIF_COUNTER_SYSTEM) == 0u);
    CHECK(diag().stage == NET_NODE_STAGE_READY);
    MockRtd_AdvanceUs(37000u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(event_count == 1 && last_event_meta.received_at_ms == 37u);
    return 0;
#else
    /* OsIf timer OFF and no hook: no millisecond time base. */
    NetNodeConfig c = { NODE_BOOT, 1u, 1u, 0u, 0u }; /* no UART link in VSS */
    NetNodeDiag d;
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(false);
    CHECK(NetNode_Init(&c) == NET_NOT_CONFIGURED);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_HAL_START);
    CHECK(d.stage_status == NET_NOT_CONFIGURED);
    CHECK(d.rtd.init_failed_step == NET_HAL_RTD_STEP_TIME);
    CHECK(!d.rtd.started);
    step_ms(5u);                        /* Step is inert, no crash */
    CHECK(diag().steps == 0u);
    return 0;
#endif
}

/* VSS_STATUS is an application-periodic message: the VSS application submits
 * a new evaluation on every change and otherwise every 250 ms, so Network
 * sends one frame per submission and never repeats it (a repetition would
 * double the frames each period). 10_CAN_FD.md: TX_SEQUENCE +1 per frame,
 * UPDATE_SEQUENCE is the evaluation's own number. */
static int h13_status_not_repeated(void)
{
    NetVssStatus s = status_ready(500u, 12u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    unsigned ms, frames = 0u;
    uint32_t n;
    CHECK(start_node() == NET_OK);
    step_ms(10u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    for (ms = 0u; ms < 2000u; ++ms) {
        step_ms(1u);
        while (pull_status(&f, &raw) >= 0) {
            ++frames;
            CHECK(f.id == 0x340u && raw.tx_sequence == 1u && raw.update_sequence == 500u);
        }
    }
    CHECK(frames == 1u);                /* exactly one frame in 2 s after one submission */
    /* The application's 250 ms cadence: one frame per submission, at once. */
    for (n = 1u; n <= 8u; ++n) {
        s = status_ready(600u + n, 3u);
        CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
        step_ms(1u);
        CHECK(pull_status(&f, &raw) >= 0);
        CHECK(raw.boot_id == NODE_BOOT && raw.tx_sequence == 1u + n);
        CHECK(raw.update_sequence == 600u + n);    /* the submitted number, unchanged */
        CHECK(raw.source_age_ms == 3u + 1u);       /* submitted AGE + 1 ms until the next step */
        CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
        step_ms(249u);
        CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
    }
    d = diag();
    CHECK(d.status_tx.period_ms == 0u && d.status_tx.counters.periodic_sent == 0u);
    CHECK(d.status_tx.counters.sent == 9u && d.app.status_submitted == 9u);
    CHECK(d.app.next_status_sequence == 10u);
    return 0;
}

/* A submission made while the previous VSS_STATUS frame still occupies the TX
 * message buffer waits in M04; the wait is added to SOURCE_AGE when it is
 * actually sent (20_API.md: Network adds the sending BOOT/TX and the waiting AGE). */
static int h13_pending_age(void)
{
    NetVssStatus s1 = status_ready(1u, 0u), s2 = status_ready(2u, 17u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t submitted_at;
    CHECK(start_node() == NET_OK);
    step_ms(5u);
    CHECK(VssNet_PublishStatus(&s1) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(diag().rtd.tx_sent == 1u);    /* in the MB; the bus has not taken it yet */
    step_ms(4u);
    submitted_at = now_ms();
    CHECK(VssNet_PublishStatus(&s2) == NET_SUBMIT_OK);
    step_ms(30u);
    d = diag();
    CHECK(d.rtd.tx_sent == 1u);         /* waits: one unsettled frame per message */
    CHECK(d.status_tx.fresh && d.status_tx.in_flight);
    CHECK(pull_status(&f, &raw) >= 0);  /* the bus takes the first frame now */
    CHECK(raw.tx_sequence == 1u && raw.update_sequence == 1u && raw.source_age_ms == 1u); /* 1 ms to its step */
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.tx_sequence == 2u && raw.update_sequence == 2u);
    CHECK(now_ms() - submitted_at == 31u);
    CHECK(raw.source_age_ms == 17u + (now_ms() - submitted_at)); /* submitted AGE + wait */
    d = diag();
    CHECK(d.status_tx.counters.pending_age_added_ms_max == 31u);
    CHECK(d.status_tx.counters.periodic_sent == 0u && d.status_tx.counters.replaced == 0u);
    step_ms(1000u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
    return 0;
}

static int h14_rx_burst(void)
{
    enum { BURST = 12 };                /* > RX MBs (4), within the HAL RX queue */
    NetWireVssEventRaw r;
    NetWireCanEnvelope e;
    NetNodeDiag d0, d;
    int i;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    d0 = diag();
    for (i = 0; i < BURST; ++i) {
        r = event_raw((uint32_t)(i + 1), (uint32_t)(1000 + i));
        r.event_kind = (uint8_t)(NET_VSS_EVENT_WELCOME + (i % 5));
        r.original_age_ms = (uint16_t)(i * 3);
        CHECK(encode_event(&r, &e));
        CHECK(inject(&e) >= 0);         /* MB re-armed in the ISR: none lost */
    }
    CHECK(event_count == 0);
    step_ms(1u);
    CHECK(event_count == BURST);
    for (i = 0; i < BURST; ++i) {
        CHECK(event_metas[i].source.tx_sequence == (uint32_t)(i + 1));
        CHECK(event_metas[i].source.boot_id == PEER_BOOT);
        CHECK(events[i].occurrence_id == (uint32_t)(1000 + i));
        CHECK(events[i].event_kind == (uint8_t)(NET_VSS_EVENT_WELCOME + (i % 5)));
        CHECK(events[i].original_age_ms == (uint16_t)(i * 3));
        CHECK(events[i].use_limit_ms == 500u);
    }
    d = diag();
    CHECK(d.rtd.rx_frames == d0.rtd.rx_frames + BURST);
    CHECK(d.rtd.rx_queue_rejected == 0u);
    CHECK(d.rtd.rx_overruns == 0u);
    /* Controller overrun: an earlier frame in the MB was overwritten. The loss
     * is counted and flagged on the capture; the flagged frame is not delivered
     * as if nothing had been lost, and the link keeps working. */
    r = event_raw(BURST + 1, 2000u);
    CHECK(encode_event(&r, &e));
    CHECK(inject_frame(&e, e.fdf, e.brs, e.dlc, true) >= 0);
    step_ms(1u);
    d = diag();
    CHECK(d.rtd.rx_overruns == 1u);
    CHECK(event_count == BURST);
    r = event_raw(BURST + 2, 2001u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(event_count == BURST + 1 && last_event.occurrence_id == 2001u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "h01_init", h01_init }, { "h02_event_rx", h02_event_rx },
        { "h03_order_duplicate", h03_order_duplicate }, { "h04_warning_rx", h04_warning_rx },
        { "h05_permission_rx", h05_permission_rx }, { "h06_malformed_rx", h06_malformed_rx },
        { "h07_publish", h07_publish }, { "h08_publish_invalid", h08_publish_invalid },
        { "h09_publish_in_callback", h09_publish_in_callback }, { "h10_ack_missing", h10_ack_missing },
        { "h11_busoff", h11_busoff }, { "h12_time_source", h12_time_source },
        { "h13_status_not_repeated", h13_status_not_repeated }, { "h13_pending_age", h13_pending_age },
        { "h14_rx_burst", h14_rx_burst }
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
