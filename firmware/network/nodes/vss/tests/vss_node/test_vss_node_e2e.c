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
#include "net_hal_rtd_internal.h" /* Verify the composed adapter, without adding a public API. */
#include "net_load_traffic.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define RUN_CHECK(x) do { if ((x) != 0) return 1; } while (0)

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
    CHECK(net_hal_rtd_core.adapter != NULL);
    CHECK(net_hal_rtd_core.adapter->platform == (NetHalPlatform)NETWORK_NODE_PLATFORM);
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
    /* CAN link closed during the bus-off hold: RX not delivered, TX not
     * accepted (recovery after the hold: h11b). */
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

/* Bus-off recovery (net_hal_rtd_can.c): in the pass that takes the bus-off
 * the frame in the TX MB is aborted and reported FAILED + released (no
 * record of unknown fate is left) and the link closes: for the hold time
 * submissions are NOT_READY, received frames are dropped, nothing goes on the
 * bus. Then it opens under port epoch 2: the next submission goes out with
 * the next TX_SEQUENCE (the aborted frame is not sent again), the M04 slot is
 * free and reception works again. */
static int h11b_busoff_recovery(void)
{
    NetWireVssEventRaw r = event_raw(1u, 7u);
    NetWireCanEnvelope e;
    NetVssStatus s = status_ready(1u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t t0;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_CanSetAckMissing(BUS, true);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(diag().rtd.tx_sent == 1u);
    MockRtd_CanRaiseBusOff(BUS);
    t0 = now_ms();
    step_ms(1u);
    d = diag();
    CHECK(MockRtd_CanAborts(BUS) == 1u && d.rtd.busoff_tx_aborts == 1u && d.rtd.tx_timeout_aborts == 0u);
    CHECK(d.rtd.can_link_state == 1u && d.rtd.busoff_streak == 1u);
    CHECK(d.upper.tx_failed == 1u && d.upper.tx_released == 1u && d.upper.errors == 1u);
    CHECK(!d.status_tx.in_flight);
    step_ms(1u);
    s = status_ready(2u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_NOT_READY);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(2u);
    CHECK(event_count == 0 && diag().rtd.busoff_rx_dropped == 1u);
    MockRtd_CanSetAckMissing(BUS, false);
    while (now_ms() < t0 + 100u) step_ms(1u);
    d = diag();
    CHECK(d.rtd.can_link_state == 1u && d.rtd.busoff_recoveries == 0u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0);
    step_ms(2u);
    d = diag();
    CHECK(d.rtd.can_link_state == 0u && d.rtd.busoff_recoveries == 1u && d.upper.resets == 1u);
    CHECK(MockRtd_CanTransmitNext(BUS, NULL) < 0); /* nothing old went out */
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    CHECK(raw.update_sequence == 2u && raw.tx_sequence == 2u && raw.boot_id == NODE_BOOT);
    step_ms(2u);
    d = diag();
    CHECK(d.upper.tx_succeeded == 1u && d.rtd.busoff_streak == 0u && !d.status_tx.in_flight);
    r = event_raw(2u, 8u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(2u);
    CHECK(event_count == 1);
    return 0;
}

/* A frame that completed before the bus-off reached the main context is
 * reported SUCCEEDED (its ring entry comes first), not aborted; the link
 * failure follows it. */
static int h11c_busoff_after_complete(void)
{
    NetVssStatus s = status_ready(1u, 0u);
    MockCanFrame f;
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(MockRtd_CanTransmitNext(BUS, &f) >= 0);  /* TX_COMPLETE in the ring */
    MockRtd_CanRaiseBusOff(BUS);                    /* behind it in the ring */
    step_ms(1u);
    d = diag();
    CHECK(MockRtd_CanAborts(BUS) == 0u && d.rtd.tx_complete == 1u);
    CHECK(d.rtd.can_link_state == 1u);
    step_ms(1u);
    d = diag();
    CHECK(d.upper.tx_succeeded == 1u && d.upper.tx_failed == 0u && d.upper.errors == 1u);
    CHECK(!d.status_tx.in_flight);
    return 0;
}

/* An abort that does not finish at the bus-off: the frame is reported
 * FAILED + released, its MB stays out of use until its flag shows it idle,
 * and the link recovery does not wait for it. */
static int h11d_busoff_abort_timeout(void)
{
    NetVssStatus s = status_ready(1u, 0u);
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t t0;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_CanSetAckMissing(BUS, true);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    MockRtd_CanSetAbortResult(BUS, FLEXCAN_STATUS_TIMEOUT);
    MockRtd_CanRaiseBusOff(BUS);
    t0 = now_ms();
    step_ms(2u);
    d = diag();
    CHECK(d.rtd.tx_abort_timeouts == 1u && d.rtd.tx_quarantine_released == 0u && d.rtd.busoff_tx_aborts == 1u);
    CHECK(d.rtd.can_link_state == 1u);
    CHECK(d.upper.tx_failed == 1u && d.upper.tx_released == 1u && !d.status_tx.in_flight);
    MockRtd_CanSetAckMissing(BUS, false);
    while (now_ms() < t0 + 102u) step_ms(1u);
    CHECK(diag().rtd.can_link_state == 0u && diag().rtd.busoff_recoveries == 1u);
    s = status_ready(2u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);             /* through the other TX MB */
    CHECK(raw.update_sequence == 2u && raw.tx_sequence == 2u);
    CHECK(MockRtd_CanSendsOnAbortingMb(BUS) == 0u);  /* not on the quarantined one */
    MockRtd_CanFinishAbort(BUS);
    step_ms(2u);
    CHECK(diag().rtd.tx_quarantine_released == 1u);
    return 0;
}

/* The controller rejoined and sent the pending frame after the bus-off, before
 * the main context took the bus-off: the frame is reported SUCCEEDED when the
 * link closes (not aborted, its later ring entry is skipped), and that sent
 * frame starts the bus-off count again. */
static int h11f_busoff_then_complete(void)
{
    NetVssStatus s = status_ready(1u, 0u);
    MockCanFrame f;
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    MockRtd_CanRaiseBusOff(BUS);                    /* first in the ring */
    CHECK(MockRtd_CanTransmitNext(BUS, &f) >= 0);  /* TX_COMPLETE behind it */
    step_ms(1u);
    d = diag();
    CHECK(MockRtd_CanAborts(BUS) == 0u && d.rtd.busoff_tx_aborts == 0u && d.rtd.tx_complete == 1u);
    CHECK(d.rtd.can_link_state == 1u && d.rtd.busoff_streak == 1u);
    CHECK(d.upper.tx_succeeded == 1u && d.upper.tx_failed == 0u && d.upper.errors == 1u);
    CHECK(!d.status_tx.in_flight && d.rtd.unexpected_events == 0u);
    step_ms(2u);
    CHECK(diag().upper.tx_succeeded == 1u);
    return 0;
}

/* Frames on the bus while the link is closed are not delivered: those the
 * drain takes during the hold, and those still in the ISR ring when the link
 * opens again (stored before the reopen). The first frame after it is. */
static int h11g_busoff_rx_while_closed(void)
{
    NetWireVssEventRaw r;
    NetWireCanEnvelope e;
    uint32_t t0, k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_CanRaiseBusOff(BUS);
    t0 = now_ms();
    step_ms(1u);
    CHECK(diag().rtd.can_link_state == 1u);
    r = event_raw(1u, 7u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    while (now_ms() < t0 + 100u) step_ms(1u);
    CHECK(diag().rtd.can_link_state == 1u);
    for (k = 0u; k < 20u; ++k) {                    /* more than one drain budget (16) */
        r = event_raw(2u + k, 8u + k);
        CHECK(encode_event(&r, &e));
        CHECK(inject(&e) >= 0);
    }
    step_ms(1u);                                    /* drains 16, then opens again */
    CHECK(diag().rtd.can_link_state == 0u && diag().rtd.busoff_recoveries == 1u);
    step_ms(1u);                                    /* the 4 stored before the reopen */
    CHECK(event_count == 0 && diag().rtd.busoff_rx_dropped == 21u);
    r = event_raw(30u, 40u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(event_count == 1);
    return 0;
}

/* Hold time: NET_BOARD_CAN_BUSOFF_FAST_TRIES bus-offs with no frame sent in
 * between are held 100 ms, the next ones 1000 ms; a sent frame starts the
 * count again. */
static int h11e_busoff_hold_levels(void)
{
    NetVssStatus s;
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    uint32_t t0, k, held;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    for (k = 0u; k < 6u; ++k) {
        MockRtd_CanRaiseBusOff(BUS);
        t0 = now_ms();
        step_ms(1u);
        CHECK(diag().rtd.can_link_state == 1u && diag().rtd.busoff_streak == k + 1u);
        while (diag().rtd.can_link_state != 0u && now_ms() < t0 + 2000u) step_ms(1u);
        held = now_ms() - t0;
        if (k < 5u) CHECK(held >= 100u && held <= 102u);
        else CHECK(held >= 1000u && held <= 1002u);
    }
    CHECK(diag().rtd.busoff_recoveries == 6u);
    s = status_ready(1u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(pull_status(&f, &raw) >= 0);
    step_ms(1u);
    CHECK(diag().rtd.busoff_streak == 0u);
    MockRtd_CanRaiseBusOff(BUS);
    t0 = now_ms();
    step_ms(1u);
    while (diag().rtd.can_link_state != 0u && now_ms() < t0 + 2000u) step_ms(1u);
    CHECK(now_ms() - t0 <= 102u && diag().rtd.busoff_streak == 1u);
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
    CHECK(d.rtd.rx_loss_events == 0u);
    /* Controller overrun: an earlier frame in the MB was overwritten. That
     * loss is recorded (CAN_PORT_RECOVERY.md, RX overflow: a real loss, its
     * boundary only, nothing synthesized); the frame read now is complete and
     * is delivered as received, and the link keeps working. */
    r = event_raw(BURST + 1, 2000u);
    CHECK(encode_event(&r, &e));
    CHECK(inject_frame(&e, e.fdf, e.brs, e.dlc, true) >= 0);
    step_ms(1u);
    d = diag();
    CHECK(d.rtd.rx_overruns == 1u && d.rtd.rx_loss_events == 1u);
    CHECK(event_count == BURST + 1 && last_event.occurrence_id == 2000u);
    CHECK(last_event_meta.source.tx_sequence == (uint32_t)(BURST + 1));
    CHECK(d.upper.rx_held == d0.upper.rx_held && d.upper.errors == d0.upper.errors);
    r = event_raw(BURST + 2, 2001u);
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    CHECK(event_count == BURST + 2 && last_event.occurrence_id == 2001u);
    CHECK(diag().rtd.rx_loss_events == 1u);
    return 0;
}

static int send_warning(const NetWireVssWarningStateRaw *w)
{
    NetWireCanEnvelope e;
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, w, &e) == NET_WIRE_OK);
    CHECK(inject(&e) >= 0);
    return 0;
}

/* A warning lane that lost its quality keeps the last valid source sequence
 * and lets its AGE grow (VSS contract C12 (VSS_NODE_INTEGRATION.md)), while
 * another lane brings a new evaluation in the same frame. 04_COMMON/10_DATA_AND_ID_RULES.md (repeat and
 * current quality): an actual quality drop of the same original observation
 * with a new TX_SEQUENCE is accepted and delivered; a higher TX_SEQUENCE alone
 * never changes the original value nor promotes the quality back to OK.
 * VSS/10_DATA.md: the lanes keep independent quality and AGE. */
static int h15_warning_quality_drop(void)
{
    NetWireVssWarningStateRaw w;
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(3u);
    d0 = diag();
    w = warning_raw(1u);                       /* every lane OK */
    CHECK(send_warning(&w) == 0);
    step_ms(50u);
    CHECK(warning_count == 1);
    /* TX2: occupant lane loses its quality (same source sequence 22, STALE,
     * AGE 230 -> 400); rear lane brings a new evaluation (33 -> 34). The
     * unchanged lanes repeat with their AGE grown by the elapsed time. */
    w.tx_sequence = 2u;
    w.occupant_hazard_quality = NET_QUALITY_STALE;
    w.occupant_hazard_reason = NET_QUALITY_REASON_STALE;
    w.occupant_hazard_original_age_ms = 400u;
    w.rear_obstacle_source_sequence = 34u;
    w.rear_obstacle_state = NET_REAR_WARNING_EMERGENCY;
    w.rear_obstacle_original_age_ms = 5u;
    w.antipinch_original_age_ms = 170u;
    w.rear_activation_age_ms = 105u;
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 2 && last_warning_meta.source.tx_sequence == 2u);
    CHECK(last_warning.occupant_hazard_source_sequence == 22u);
    CHECK(last_warning.occupant_hazard_quality == NET_QUALITY_STALE);
    CHECK(last_warning.occupant_hazard_reason == NET_QUALITY_REASON_STALE);
    CHECK(last_warning.occupant_hazard_original_age_ms == 400u);
    CHECK(last_warning.occupant_hazard_state == NET_FLAG_NO);
    CHECK(last_warning.rear_obstacle_source_sequence == 34u);
    CHECK(last_warning.rear_obstacle_state == NET_REAR_WARNING_EMERGENCY);
    /* TX3: the same lost lane again (STALE -> INVALID with another reason is
     * still restricted), the rear lane another new evaluation. */
    step_ms(49u);
    w.tx_sequence = 3u;
    w.occupant_hazard_quality = NET_QUALITY_INVALID;
    w.occupant_hazard_reason = NET_QUALITY_REASON_COMMUNICATION_FAULT;
    w.occupant_hazard_original_age_ms = 450u;
    w.rear_obstacle_source_sequence = 35u;
    w.rear_obstacle_original_age_ms = 4u;
    w.antipinch_original_age_ms = 220u;
    w.rear_activation_age_ms = 155u;
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 3 && last_warning_meta.source.tx_sequence == 3u);
    CHECK(last_warning.occupant_hazard_quality == NET_QUALITY_INVALID);
    CHECK(last_warning.occupant_hazard_reason == NET_QUALITY_REASON_COMMUNICATION_FAULT);
    CHECK(last_warning.rear_obstacle_source_sequence == 35u);
    d = diag();
    CHECK(d.upper.rx_delivered == d0.upper.rx_delivered + 3u);
    CHECK(d.upper.rx_held == d0.upper.rx_held && d.app.warning_delivered == 3u);
    /* TX4: the lost lane back to OK under the same source sequence (a
     * promotion by TX_SEQUENCE alone): the frame is held as one result. */
    step_ms(49u);
    w.tx_sequence = 4u;
    w.occupant_hazard_quality = NET_QUALITY_OK;
    w.occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    w.occupant_hazard_original_age_ms = 500u;
    w.rear_obstacle_source_sequence = 36u;
    w.antipinch_original_age_ms = 270u;
    w.rear_activation_age_ms = 205u;
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    d = diag();
    CHECK(warning_count == 3 && d.upper.rx_held == d0.upper.rx_held + 1u);
    /* TX5: the same source sequence with another value: held. */
    step_ms(49u);
    w.tx_sequence = 5u;
    w.occupant_hazard_quality = NET_QUALITY_INVALID;
    w.occupant_hazard_reason = NET_QUALITY_REASON_COMMUNICATION_FAULT;
    w.occupant_hazard_state = NET_FLAG_YES;
    w.occupant_hazard_original_age_ms = 550u;
    w.rear_obstacle_source_sequence = 37u;
    w.antipinch_original_age_ms = 320u;
    w.rear_activation_age_ms = 255u;
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    d = diag();
    CHECK(warning_count == 3 && d.upper.rx_held == d0.upper.rx_held + 2u);
    /* TX6: a new occupant evaluation (new source sequence, OK, fresh AGE). */
    step_ms(49u);
    w.tx_sequence = 6u;
    w.occupant_hazard_source_sequence = 23u;
    w.occupant_hazard_quality = NET_QUALITY_OK;
    w.occupant_hazard_reason = NET_QUALITY_REASON_NONE;
    w.occupant_hazard_original_age_ms = 3u;
    w.rear_obstacle_source_sequence = 38u;
    w.antipinch_original_age_ms = 370u;
    w.rear_activation_age_ms = 305u;
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 4 && last_warning_meta.source.tx_sequence == 6u);
    CHECK(last_warning.occupant_hazard_source_sequence == 23u && last_warning.occupant_hazard_state == NET_FLAG_YES);
    CHECK(last_warning.occupant_hazard_quality == NET_QUALITY_OK);
    return 0;
}

/* Every lane's AGE grown by ms (the same original observation, later). */
static void warning_age_add(NetWireVssWarningStateRaw *w, uint16_t ms)
{
    w->antipinch_original_age_ms = (uint16_t)(w->antipinch_original_age_ms + ms);
    w->occupant_hazard_original_age_ms = (uint16_t)(w->occupant_hazard_original_age_ms + ms);
    w->rear_obstacle_original_age_ms = (uint16_t)(w->rear_obstacle_original_age_ms + ms);
    w->rear_activation_age_ms = (uint16_t)(w->rear_activation_age_ms + ms);
}

static int warning_ages_are(const NetVssWarningState *w, const NetWireVssWarningStateRaw *first, uint16_t grown)
{
    CHECK(w->antipinch_original_age_ms == (uint16_t)(first->antipinch_original_age_ms + grown));
    CHECK(w->occupant_hazard_original_age_ms == (uint16_t)(first->occupant_hazard_original_age_ms + grown));
    CHECK(w->rear_obstacle_original_age_ms == (uint16_t)(first->rear_obstacle_original_age_ms + grown));
    CHECK(w->rear_activation_age_ms == (uint16_t)(first->rear_activation_age_ms + grown));
    return 0;
}

/* DOMAIN repeats one warning evaluation every 50 ms with a new TX_SEQUENCE and
 * the AGE grown (VSS contract C11 (VSS_NODE_INTEGRATION.md): periodic,
 * receive watch 150 ms in VSS).
 * 04_COMMON/10_DATA_AND_ID_RULES.md (repeat and current quality) and
 * 02_NODES/CIS/20_API.md (receive delivery, same rule for every node
 * callback): a normal repetition with a new TX_SEQUENCE is delivered to the
 * existing callback even for the same original observation, its original
 * numbers kept and its AGE grown; an exact duplicate (same TX, same raw) is
 * not. A repetition that arrives a little late (its AGE short of the
 * accumulated bound by up to the CAN hop allowance, 5 ms candidate) is still
 * delivered with the AGE raised to that bound, never lowered; further short
 * is held. */
static int h16_warning_repeat_delivered(void)
{
    NetWireVssWarningStateRaw first = warning_raw(1u), w;
    NetWireCanEnvelope e, last;
    NetNodeDiag d0, d;
    uint32_t t1;
    CHECK(start_node() == NET_OK);
    step_ms(3u);
    t1 = now_ms();
    CHECK(send_warning(&first) == 0);          /* TX1: the evaluation itself */
    step_ms(1u);
    CHECK(warning_count == 1);
    d0 = diag();
    /* TX2 at +50 ms, on time: AGE +50. */
    step_ms(49u);
    w = first; w.tx_sequence = 2u; warning_age_add(&w, 50u);
    CHECK(now_ms() == t1 + 50u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 2 && last_warning_meta.source.tx_sequence == 2u);
    CHECK(last_warning_meta.received_at_ms == t1 + 50u);
    CHECK(last_warning.occupant_hazard_source_sequence == first.occupant_hazard_source_sequence);
    RUN_CHECK(warning_ages_are(&last_warning, &first, 50u));
    /* TX3: AGE as of +100 ms, but the frame reaches the bus 2 ms late. Its AGE
     * is 2 ms short of TX2's AGE + 52 ms: delivered with the AGE raised to the
     * bound (+102), not lowered. */
    step_ms(51u);
    w = first; w.tx_sequence = 3u; warning_age_add(&w, 100u);
    CHECK(now_ms() == t1 + 102u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 3 && last_warning_meta.source.tx_sequence == 3u);
    CHECK(last_warning_meta.received_at_ms == t1 + 102u);
    RUN_CHECK(warning_ages_are(&last_warning, &first, 102u));
    /* TX4 at +150 ms, on time again. */
    step_ms(47u);
    w = first; w.tx_sequence = 4u; warning_age_add(&w, 150u);
    CHECK(now_ms() == t1 + 150u);
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, &last) == NET_WIRE_OK);
    CHECK(inject(&last) >= 0);
    step_ms(1u);
    CHECK(warning_count == 4 && last_warning_meta.source.tx_sequence == 4u);
    RUN_CHECK(warning_ages_are(&last_warning, &first, 150u));
    d = diag();
    /* Three repetitions passed on: not new observations, not duplicates. */
    CHECK(d.upper.repeat_delivered == d0.upper.repeat_delivered + 3u);
    CHECK(d.upper.repeat_age_raised == d0.upper.repeat_age_raised + 1u);
    CHECK(d.upper.rx_delivered == d0.upper.rx_delivered);
    CHECK(d.upper.duplicates == d0.upper.duplicates && d.upper.rx_held == d0.upper.rx_held);
    CHECK(d.app.warning_delivered == 4u);
    /* The exact TX4 frame again (same TX, same raw): no callback. */
    d0 = d;
    CHECK(inject(&last) >= 0);
    step_ms(2u);
    d = diag();
    CHECK(warning_count == 4 && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* TX5 at +200 ms whose AGE is 10 ms short of the bound (beyond the
     * allowance): held, no callback. */
    step_ms(47u);
    w = first; w.tx_sequence = 5u; warning_age_add(&w, 190u);
    CHECK(now_ms() == t1 + 200u);
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, &e) == NET_WIRE_OK);
    CHECK(inject(&e) >= 0);
    step_ms(1u);
    d = diag();
    CHECK(warning_count == 4 && d.upper.rx_held == d0.upper.rx_held + 1u);
    /* TX6 at +250 ms with its correct AGE: delivered again. */
    step_ms(49u);
    w = first; w.tx_sequence = 6u; warning_age_add(&w, 250u);
    CHECK(now_ms() == t1 + 250u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 5 && last_warning_meta.source.tx_sequence == 6u);
    RUN_CHECK(warning_ages_are(&last_warning, &first, 250u));
    return 0;
}

/* A VSS_WARNING_STATE that is a new evaluation on every lane. */
static NetWireVssWarningStateRaw warning_new(uint32_t boot, uint32_t tx, uint32_t n)
{
    NetWireVssWarningStateRaw w = warning_raw(tx);
    w.boot_id = boot;
    w.antipinch_source_sequence = 100u + n;
    w.occupant_hazard_source_sequence = 200u + n;
    w.rear_obstacle_source_sequence = 300u + n;
    w.rear_activation_sequence = 400u + n;
    return w;
}

static int send_event(uint32_t boot, uint32_t tx, uint32_t occurrence)
{
    NetWireVssEventRaw r = event_raw(tx, occurrence);
    NetWireCanEnvelope e;
    r.boot_id = boot;
    CHECK(encode_event(&r, &e));
    CHECK(inject(&e) >= 0);
    return 0;
}

/* DOMAIN restart (VSS contract C4, C6 (VSS_NODE_INTEGRATION.md);
 * 04_COMMON/10_DATA_AND_ID_RULES.md
 * generation): run A sends from TX 5, run B (another BOOT_ID) from TX 1 again.
 * Each CAN ID moves to B on its own first valid B frame and passes it on; a
 * late frame of A is then an old generation and is refused. A frame with a
 * structure error never moves a CAN ID to its BOOT (C5). */
static int h17_domain_boot_switch(void)
{
    enum { BOOT_A = 0x444F4D41u, BOOT_B = 0x444F4D42u, BOOT_C = 0x444F4D43u };
    NetWireVssEventRaw r, r2;
    NetWireVssWarningStateRaw w;
    NetWireCanEnvelope good, other, bad;
    NetNodeDiag d0, d;
    size_t kind_at = 0u, i;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    d0 = diag();
    /* Run A, TX from 5. */
    RUN_CHECK(send_event(BOOT_A, 5u, 70u));
    w = warning_new(BOOT_A, 5u, 1u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(event_count == 1 && warning_count == 1);
    CHECK(last_event_meta.source.boot_id == BOOT_A && last_event_meta.source.tx_sequence == 5u);
    RUN_CHECK(send_event(BOOT_A, 6u, 71u));
    step_ms(1u);
    CHECK(event_count == 2);
    /* BOOT C with a structure error (reserved event kind; a warning one DLC
     * step shorter than the registered 64 B). */
    r = event_raw(1u, 1u);
    r.boot_id = BOOT_C;
    CHECK(encode_event(&r, &good));
    r2 = r;
    r2.event_kind = NET_VSS_EVENT_GOODBYE;
    CHECK(encode_event(&r2, &other));
    for (i = 0u; i < good.length; ++i) if (good.bytes[i] != other.bytes[i]) { kind_at = i; break; }
    CHECK(i < good.length);
    bad = good;
    bad.bytes[kind_at] = 0u;
    CHECK(inject(&bad) >= 0);
    w = warning_new(BOOT_C, 1u, 2u);
    CHECK(net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, &w, &bad) == NET_WIRE_OK);
    bad.length = 48u;
    CHECK(inject_frame(&bad, true, true, MockRtd_DlcForLength(48u), false) >= 0);
    step_ms(1u);
    d = diag();
    CHECK(event_count == 2 && warning_count == 1);
    CHECK(d.upper.rx_invalid == d0.upper.rx_invalid + 2u);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes);
    /* Run A is still the accepted one on both CAN IDs. */
    RUN_CHECK(send_event(BOOT_A, 7u, 72u));
    w = warning_new(BOOT_A, 6u, 3u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(event_count == 3 && warning_count == 2);
    CHECK(last_event_meta.source.boot_id == BOOT_A && last_warning_meta.source.boot_id == BOOT_A);
    /* DOMAIN restarts: run B, TX from 1 on each CAN ID. */
    step_ms(20u);
    w = warning_new(BOOT_B, 1u, 4u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(warning_count == 3 && last_warning_meta.source.boot_id == BOOT_B && last_warning_meta.source.tx_sequence == 1u);
    /* VSS_EVENT still on A until its own first B frame: a late A event is
     * still its run. */
    RUN_CHECK(send_event(BOOT_A, 8u, 73u));
    step_ms(1u);
    CHECK(event_count == 4 && last_event_meta.source.boot_id == BOOT_A);
    RUN_CHECK(send_event(BOOT_B, 1u, 1u));
    step_ms(1u);
    CHECK(event_count == 5 && last_event_meta.source.boot_id == BOOT_B && last_event_meta.source.tx_sequence == 1u);
    CHECK(last_event.occurrence_id == 1u);
    d0 = diag();
    CHECK(d0.upper.generation_changes == d.upper.generation_changes + 2u);
    /* Late frames of run A on both CAN IDs: old generation, refused. */
    RUN_CHECK(send_event(BOOT_A, 9u, 74u));
    w = warning_new(BOOT_A, 7u, 5u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    d = diag();
    CHECK(event_count == 5 && warning_count == 3);
    CHECK(d.upper.old_generation == d0.upper.old_generation + 2u);
    CHECK(d.upper.stale == d0.upper.stale + 2u);
    /* Run B goes on. */
    RUN_CHECK(send_event(BOOT_B, 2u, 2u));
    w = warning_new(BOOT_B, 2u, 6u);
    CHECK(send_warning(&w) == 0);
    step_ms(1u);
    CHECK(event_count == 6 && warning_count == 4);
    CHECK(last_event_meta.source.boot_id == BOOT_B && last_event_meta.source.tx_sequence == 2u);
    CHECK(last_warning_meta.source.boot_id == BOOT_B && last_warning_meta.source.tx_sequence == 2u);
    return 0;
}

/* Wait out the TX timeout (100 ms after the frame went to its MB) and let
 * the abort report settle. */
static void past_tx_timeout(uint32_t sent_at)
{
    while (now_ms() < sent_at + 100u) step_ms(1u);
    step_ms(2u);
}

/* The result of FlexCAN_Ip_AbortTransfer after the TX timeout decides the
 * report (RTD: NO_TRANSFER_IN_PROGRESS when the frame was transmitted before
 * the abort took the MB, and then no TX_COMPLETE comes; TIMEOUT when the abort
 * did not finish, the MB still held by the controller):
 *   NO_TRANSFER_IN_PROGRESS: the frame is on the bus, so the transfer is
 *     settled as sent (not failed) and counted apart;
 *   TIMEOUT: FAILED (no success claim), and that MB is not handed out again
 *     until its message buffer flag shows the controller finished with it;
 *     other MBs carry the traffic meanwhile. */
static int h18_abort_result(void)
{
    NetVssStatus s;
    NetWireVssStatusRaw raw;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t sent_at, n;
    int mb, held_mb;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* 1) No ACK; the abort finds the frame already transmitted. */
    MockRtd_CanSetAckMissing(BUS, true);
    s = status_ready(1u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    sent_at = now_ms();
    CHECK(diag().rtd.tx_sent == 1u);
    MockRtd_CanSetAbortResult(BUS, FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS);
    past_tx_timeout(sent_at);
    d = diag();
    CHECK(MockRtd_CanAborts(BUS) == 1u);
    CHECK(d.rtd.tx_timeout_aborts == 1u && d.rtd.tx_abort_already_sent == 1u && d.rtd.tx_abort_timeouts == 0u);
    CHECK(d.rtd.tx_abort_last_status == (uint32_t)FLEXCAN_STATUS_NO_TRANSFER_IN_PROGRESS);
    CHECK(d.upper.tx_succeeded == 1u && d.upper.tx_failed == 0u && d.upper.tx_released == 1u);
    CHECK(d.status_tx.counters.tx_failed == 0u && d.rtd.tx_complete == 0u);
    mb = pull_status(&f, &raw);                     /* the frame the bus got */
    CHECK(mb >= 0 && raw.update_sequence == 1u && raw.tx_sequence == 1u);
    /* 2) The abort times out: FAILED, the MB held. */
    s = status_ready(2u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    sent_at = now_ms();
    MockRtd_CanSetAbortResult(BUS, FLEXCAN_STATUS_TIMEOUT);
    past_tx_timeout(sent_at);
    d = diag();
    CHECK(MockRtd_CanAborts(BUS) == 2u);
    CHECK(d.rtd.tx_timeout_aborts == 2u && d.rtd.tx_abort_timeouts == 1u && d.rtd.tx_abort_already_sent == 1u);
    CHECK(d.rtd.tx_abort_last_status == (uint32_t)FLEXCAN_STATUS_TIMEOUT);
    CHECK(d.upper.tx_failed == 1u && d.upper.tx_succeeded == 1u && d.upper.tx_released == 2u);
    CHECK(d.status_tx.counters.tx_failed == 1u && d.rtd.tx_quarantine_released == 0u);
    held_mb = mb;                                   /* the first free TX MB again */
    /* 3) Traffic goes on through the other MBs while that one is held. */
    MockRtd_CanSetAckMissing(BUS, false);
    for (n = 0u; n < 4u; ++n) {
        s = status_ready(3u + n, 0u);
        CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
        step_ms(1u);
        mb = pull_status(&f, &raw);
        CHECK(mb >= 0 && mb != held_mb);
        CHECK(raw.update_sequence == 3u + n && raw.tx_sequence == 3u + n);
        step_ms(2u);
    }
    d = diag();
    CHECK(MockRtd_CanSendsOnAbortingMb(BUS) == 0u && d.rtd.tx_quarantine_released == 0u);
    CHECK(d.upper.tx_succeeded == 5u && d.rtd.tx_complete == 4u);
    /* 4) The controller finishes the abort: the MB is released and used again. */
    MockRtd_CanFinishAbort(BUS);
    step_ms(1u);
    d = diag();
    CHECK(d.rtd.tx_quarantine_released == 1u);
    s = status_ready(7u, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(1u);
    mb = pull_status(&f, &raw);
    CHECK(mb == held_mb && raw.update_sequence == 7u && raw.tx_sequence == 7u);
    step_ms(2u);
    d = diag();
    CHECK(MockRtd_CanSendsOnAbortingMb(BUS) == 0u);
    CHECK(d.upper.tx_succeeded == 6u && d.upper.tx_failed == 1u && d.rtd.tx_complete == 5u);
    return 0;
}

/* A refused RX re-arm (FlexCAN_Ip_Receive in the RX ISR, other than BUSY)
 * leaves that MB without a reception; the main context arms it again in a
 * later pass. While the driver keeps refusing, each RX MB that takes a frame
 * drops out (rtd.rx_unarmed) until none receives and the bus frame is lost on
 * this node. Once the driver accepts again, every MB receives again and the
 * next events reach VSS, more of them than there are RX MBs. */
static int h19_rx_rearm_retry(void)
{
    NetWireVssEventRaw r;
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint32_t armed, k;
    int events0;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    armed = MockRtd_CanRxArmed(BUS);
    CHECK(armed >= 2u);
    MockRtd_CanSetReceiveRefused(BUS, true);
    for (k = 0u; k < armed; ++k) {
        r = event_raw(1u + k, 10u + k);
        CHECK(encode_event(&r, &e) && inject(&e) >= 0);
        step_ms(1u);
        CHECK(event_count == (int)(k + 1u));
        d = diag();
        CHECK(d.rtd.rx_unarmed == k + 1u && d.rtd.rx_rearm_errors == k + 1u && d.rtd.rx_rearm_recovered == 0u);
    }
    CHECK(MockRtd_CanRxArmed(BUS) == 0u);
    r = event_raw(1u + armed, 10u + armed);
    CHECK(encode_event(&r, &e) && inject(&e) < 0);     /* no RX MB armed: lost on this node */
    step_ms(5u);
    d = diag();
    CHECK(d.rtd.rx_unarmed == armed && d.rtd.rx_rearm_recovered == 0u && d.rtd.rx_rearm_errors == armed);
    MockRtd_CanSetReceiveRefused(BUS, false);
    step_ms(1u);
    d = diag();
    CHECK(d.rtd.rx_unarmed == 0u && d.rtd.rx_rearm_recovered == armed && MockRtd_CanRxArmed(BUS) == armed);
    events0 = event_count;
    for (k = 0u; k <= armed; ++k) {
        r = event_raw(2u + armed + k, 20u + k);
        CHECK(encode_event(&r, &e) && inject(&e) >= 0);
        step_ms(1u);
    }
    CHECK(event_count == events0 + (int)armed + 1);
    CHECK(diag().rtd.rx_unarmed == 0u && diag().rtd.rx_rearm_errors == armed);
    return 0;
}

/* A timed-out abort whose message buffer flag is then lost (the RTD cleared
 * it right after the timeout while the controller finished the MB): nothing
 * shows that MB free again, so it stays quarantined until a reset, counted in
 * rtd.tx_quarantined, also after many passes. One after the other, every TX MB
 * ends this way: then no CAN frame leaves, and the next status report waits in
 * the service queue (upper.tx_deferred) instead of going into an MB the
 * controller may still own; no Send writes a quarantined MB. */
static int h20_quarantine_flag_lost(void)
{
    NetVssStatus s;
    MockCanFrame f;
    NetNodeDiag d;
    uint32_t sent_at, k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_CanSetAckMissing(BUS, true);
    for (k = 0u; k < NET_HAL_TX_CAPACITY; ++k) {
        s = status_ready(1u + k, 0u);
        CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
        step_ms(1u);
        sent_at = now_ms();
        CHECK(diag().rtd.tx_sent == k + 1u);
        MockRtd_CanSetAbortResult(BUS, FLEXCAN_STATUS_TIMEOUT);
        past_tx_timeout(sent_at);
        MockRtd_CanFinishAbortFlagLost(BUS);
        step_ms(128u);
        d = diag();
        CHECK(d.rtd.tx_abort_timeouts == k + 1u && d.rtd.tx_quarantined == k + 1u);
        CHECK(d.rtd.tx_quarantine_released == 0u && d.upper.tx_failed == k + 1u);
    }
    MockRtd_CanSetAckMissing(BUS, false);
    s = status_ready(1u + k, 0u);
    CHECK(VssNet_PublishStatus(&s) == NET_SUBMIT_OK);
    step_ms(20u);
    d = diag();
    CHECK(d.rtd.tx_sent == NET_HAL_TX_CAPACITY && d.upper.tx_deferred >= 1u);
    CHECK(d.rtd.tx_quarantined == NET_HAL_TX_CAPACITY && d.rtd.tx_quarantine_released == 0u);
    CHECK(MockRtd_CanTransmitNext(BUS, &f) < 0 && MockRtd_CanSendsOnAbortingMb(BUS) == 0u);
    return 0;
}

/* ------------------------------------------------ documented traffic ---- */
/* Ten seconds of the documented traffic (net_load_traffic.h):
 *   CAN  : the periodic frames of DOMAIN, CIS and BCM as their senders put
 *          them on the bus (10_CAN_FD.md message list, phase 0: ten of them
 *          start together every second). The RX message buffers accept every
 *          ID, so all of them enter the VSS receive path; the routes of the
 *          other nodes end in the registry, not as a loss.
 *   App  : VSS_STATUS every 250 ms (VSS_NODE_INTEGRATION.md, VSS_STATUS
 *          transmit rule), in the pass that reaches the due time, after the
 *          Network pass; the next pass sends it.
 *   Main : a pass every 20 ms.
 * At an instant shared by a pass and bus frames the pass runs first, so those
 * frames wait a whole pass. Nothing may be lost (ISR ring, adapter and
 * service queues, the M04 slot, every DOMAIN_PERMISSION and
 * VSS_WARNING_STATE delivered), VSS_STATUS leaves within its period plus one
 * pass (20 ms), and no queue goes past its capacity. The LOAD line reports
 * the counts and the high-water marks against the capacities of this build. */
enum { LOAD_OWN = 1 };
#define LOAD_PASS_US 20000u
#define LOAD_SLACK_MS 20u
#define LOAD_FLUSH_US 100000u

static NetLoadTx load_tx[LOAD_OWN];
static uint64_t load_app_due;
static uint32_t load_app_sequence;
static bool load_app_on;
static unsigned load_app_submitted, load_app_refused, load_foreign_tx, load_inject_lost;

/* The VSS application in the superloop: VSS_STATUS when it is due. */
static void load_app(uint64_t now)
{
    NetVssStatus s;
    if (!load_app_on || now < load_app_due) return;
    load_app_due += load_tx[0].period_ms;
    s = status_ready(++load_app_sequence, 5u);
    ++load_app_submitted;
    if (VssNet_PublishStatus(&s) != NET_SUBMIT_OK) ++load_app_refused;
}

/* The far end takes what the node sent (TX_COMPLETE). */
static void load_collect(void)
{
    MockCanFrame f;
    NetWireCanEnvelope e;
    while (MockRtd_CanTransmitNext(BUS, &f) >= 0) {
        memset(&e, 0, sizeof(e));
        e.standard_id = f.id;
        e.fdf = f.fd;
        e.brs = f.brs;
        e.dlc = f.dlc;
        e.length = f.length;
        memcpy(e.bytes, f.data, f.length);
        if (!net_load_tx_seen(load_tx, LOAD_OWN, &e, now_ms())) ++load_foreign_tx;
    }
}

/* One superloop pass in the documented order (VSS_NODE_INTEGRATION.md): the
 * Network pass, then the application. */
static void load_pass(void)
{
    NetNode_Step();
    load_collect();
    load_app(now_ms());
}

/* Frame n of message k on the bus (ISR context of the CAN controller). */
static int load_inject(size_t k, uint32_t n)
{
    NetWireCanEnvelope e;
    CHECK(net_load_frame(k, n, &e));
    if (inject(&e) < 0) ++load_inject_lost;
    return 0;
}

static int load_doc_traffic(void)
{
    const uint64_t end_us = (uint64_t)NET_LOAD_DURATION_MS * 1000u;
    uint32_t sent[NET_LOAD_MESSAGE_COUNT];
    uint64_t due_us[NET_LOAD_MESSAGE_COUNT];
    uint64_t pass_at = LOAD_PASS_US, t;
    uint32_t passes = 0u, permissions = 0u, warnings = 0u, injected = 0u;
    uint32_t lost, violations, tx_frames, slot_lost;
    size_t k;
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) {
        if (net_load_messages[k].sender == NET_LOAD_VSS)
            net_load_tx_init(&load_tx[0], net_load_messages[k].id, net_load_messages[k].period_ms, LOAD_SLACK_MS);
        sent[k] = 0u;
        due_us[k] = net_load_messages[k].sender == NET_LOAD_VSS ? UINT64_MAX : 0u;
    }
    CHECK(load_tx[0].id == 0x340u);
    load_app_due = 0u;
    load_app_on = true;
    for (;;) {
        t = pass_at;
        for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) if (due_us[k] < t) t = due_us[k];
        if (t >= end_us) break;
        MockRtd_AdvanceUs(t - MockRtd_NowUs());
        if (t == pass_at) {
            load_pass();
            ++passes;
            pass_at += LOAD_PASS_US;
        }
        for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) {
            if (due_us[k] != t) continue;
            RUN_CHECK(load_inject(k, sent[k]++));
            ++injected;
            if (net_load_messages[k].id == 0x080u) ++permissions;
            if (net_load_messages[k].id == 0x0C0u) ++warnings;
            due_us[k] += (uint64_t)net_load_messages[k].period_ms * 1000u;
        }
    }
    /* No new traffic: the passes take what still waits. */
    load_app_on = false;
    while (pass_at <= end_us + LOAD_FLUSH_US) {
        MockRtd_AdvanceUs(pass_at - MockRtd_NowUs());
        load_pass();
        ++passes;
        pass_at += LOAD_PASS_US;
    }
    d = diag();
    /* queue_full is a one-pass wait that keeps the number; lost is what never left. */
    slot_lost = d.status_tx.counters.not_sent + d.status_tx.counters.tx_failed + d.status_tx.counters.replaced;
    {
        const NetLoadCount loss[] = {
            { "upper.rx_dropped", d.upper.rx_dropped },
            { "hal.rx_dropped", d.hal.rx_dropped },
            { "rtd.isr_ring_overflow", d.rtd.isr_ring_overflow },
            { "rtd.rx_overruns", d.rtd.rx_overruns },
            { "rtd.rx_queue_rejected", d.rtd.rx_queue_rejected },
            { "bus_frames_not_taken", load_inject_lost },
            { "app_refused", load_app_refused },
            { "status_tx", slot_lost },
            { "permission_not_delivered", permissions > d.app.permission_received ?
                                              permissions - d.app.permission_received : 0u },
            { "warning_not_delivered", warnings > (uint32_t)warning_count ? warnings - (uint32_t)warning_count : 0u },
            { "tx_sequence_gaps", net_load_tx_gaps(load_tx, LOAD_OWN) }
        };
        const NetLoadCount late[] = {
            { "can_periods", net_load_tx_violations(load_tx, LOAD_OWN, NET_LOAD_DURATION_MS) }
        };
        lost = net_load_report("load lost", loss, sizeof(loss) / sizeof(loss[0]));
        violations = net_load_report("load late", late, sizeof(late) / sizeof(late[0]));
    }
    tx_frames = net_load_tx_frames(load_tx, LOAD_OWN);
    printf("LOAD VSS rx_frames=%lu tx_frames=%lu lost=%lu period_violations=%lu can_ring_max=%lu/%lu "
           "hal_rx_max=%lu/%lu upper_rx_max=%lu/%lu upper_tx_max=%lu/%lu\n",
           (unsigned long)d.rtd.rx_frames, (unsigned long)tx_frames, (unsigned long)lost, (unsigned long)violations,
           (unsigned long)d.rtd.isr_ring_max_depth, (unsigned long)NET_BOARD_CAN_ISR_RING,
           (unsigned long)d.hal.rx_max_depth, (unsigned long)NET_HAL_RX_CAPACITY,
           (unsigned long)d.upper.rx_max_depth, (unsigned long)NET_UPPER_RX_CAPACITY,
           (unsigned long)d.upper.tx_max_depth, (unsigned long)NET_UPPER_TX_CAPACITY);
    printf("load detail: passes=%lu injected=%lu permission=%lu/%lu warning=%lu/%lu submitted=%u refused=%u "
           "drained_max=%lu/%lu rx_invalid=%lu rx_held=%lu queue_full=%lu max_gap_ms=%lu\n",
           (unsigned long)passes, (unsigned long)injected, (unsigned long)d.app.permission_received,
           (unsigned long)permissions, (unsigned long)warning_count, (unsigned long)warnings,
           load_app_submitted, load_app_refused, (unsigned long)d.rtd.main_drained_max,
           (unsigned long)d.rtd.isr_drain_budget, (unsigned long)d.upper.rx_invalid, (unsigned long)d.upper.rx_held,
           (unsigned long)d.status_tx.counters.queue_full, (unsigned long)load_tx[0].max_gap_ms);
    fflush(stdout);
    CHECK(d.rtd.isr_ring_capacity == NET_BOARD_CAN_ISR_RING);
    CHECK(d.rtd.rx_frames == injected);       /* every bus frame entered the receive path */
    CHECK(tx_frames == load_app_submitted);   /* one frame per submission */
    CHECK(d.app.permission_received <= permissions && (uint32_t)warning_count <= warnings);
    CHECK(event_count == 0 && load_foreign_tx == 0u);
    CHECK(lost == 0u);
    CHECK(violations == 0u);
    CHECK(d.rtd.isr_ring_max_depth <= NET_BOARD_CAN_ISR_RING && d.hal.rx_max_depth <= NET_HAL_RX_CAPACITY);
    CHECK(d.upper.rx_max_depth <= NET_UPPER_RX_CAPACITY && d.upper.tx_max_depth <= NET_UPPER_TX_CAPACITY);
    /* The high-water marks saw the traffic: ten frames of the bus at once. */
    CHECK(d.rtd.isr_ring_max_depth >= 10u && d.upper.tx_max_depth >= 1u);
    CHECK(d.hal.rx_max_depth >= 1u && d.upper.rx_max_depth >= 1u);
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
        { "h11b_busoff_recovery", h11b_busoff_recovery },
        { "h11c_busoff_after_complete", h11c_busoff_after_complete },
        { "h11d_busoff_abort_timeout", h11d_busoff_abort_timeout },
        { "h11e_busoff_hold_levels", h11e_busoff_hold_levels },
        { "h11f_busoff_then_complete", h11f_busoff_then_complete },
        { "h11g_busoff_rx_while_closed", h11g_busoff_rx_while_closed },
        { "h11_busoff", h11_busoff }, { "h12_time_source", h12_time_source },
        { "h13_status_not_repeated", h13_status_not_repeated }, { "h13_pending_age", h13_pending_age },
        { "h14_rx_burst", h14_rx_burst }, { "h15_warning_quality_drop", h15_warning_quality_drop },
        { "h16_warning_repeat_delivered", h16_warning_repeat_delivered },
        { "h17_domain_boot_switch", h17_domain_boot_switch },
        { "h19_rx_rearm_retry", h19_rx_rearm_retry }, { "h20_quarantine_flag_lost", h20_quarantine_flag_lost },
        { "load_doc_traffic", load_doc_traffic },
        { "h18_abort_result", h18_abort_result }
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
