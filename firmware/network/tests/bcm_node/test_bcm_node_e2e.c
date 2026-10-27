/* BCM node end to end on the host: the real product sources (BCM App Port
 * facade -> typed ECU adapter -> NetUpper -> M04 publish slots ->
 * NetHalAdapter -> RTD backend, FlexCAN part) linked with the independently
 * written mock SDK, which plays the shared CAN bus (DOMAIN).
 * Contract: docs/network/interface/02_NODES/BCM/{10_DATA,20_API}.md,
 *           04_COMMON/10_DATA_AND_ID_RULES.md, 03_COMMUNICATION/10_CAN_FD.md.
 * Frames are built and checked with the repository codec (net_wire_can_*).
 * One case per process: NetNode_Init succeeds once.
 * Builds of this file:
 *   OsIf system counter, NetNode_Init / NetNode_Step
 *   BCM_NODE_TEST_ENTRY: application tick hooks, Net_Init / Net_MainFunction */
#include "network/net_node.h"
#include "network/app/net_if_bcm.h"
#include "network/net_wire_can.h"
#include "network/net_wire_can_bcm.h"
#include "mock_rtd.h"
#include "net_hal_rtd_internal.h" /* Verify the composed adapter, without adding a public API. */
#include "net_load_traffic.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)
#define RUN(x) do { if ((x) != 0) return 1; } while (0)

static const char *case_name;

enum { NODE_BOOT = 0x42434D31u, OTHER_BOOT = 0x42434D30u, DOMAIN_BOOT = 0x444F4D31u, BUS = 0 };

#define PASS_US 1000u   /* main pass period */

/* ------------------------------------------------------------- time ----- */
static uint64_t now_ms(void) { return MockRtd_NowUs() / 1000u; }

#ifdef BCM_NODE_TEST_ENTRY
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
static unsigned door_count, climate_count, light_count;
static NetBcmDoorCommand last_door;
static NetBcmClimateCommand last_climate;
static NetBcmLightCommand last_light;
static NetCanRxMeta last_door_meta, last_climate_meta, last_light_meta;
static uint64_t door_delivered_at;
static bool in_isr;
static unsigned isr_callbacks;
/* Submissions from inside the door command callback. */
static bool submit_in_door;
static NetSubmitResult in_door_results[3];

static NetBcmResult result_sample(uint32_t command_id, NetVehicleResult result, uint32_t occurrence, uint16_t age);
static NetBcmDoorState door_state_sample(uint32_t update, uint16_t age);

void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta)
{
    if (in_isr) ++isr_callbacks;
    ++door_count;
    last_door = *message;
    last_door_meta = *meta;
    door_delivered_at = now_ms();
    if (submit_in_door) {
        /* Two stages of the same command and the door state, inside the callback. */
        NetBcmResult accepted = result_sample(message->command.command_id, NET_RESULT_ACCEPTED, 0u, 0u);
        NetBcmResult done = result_sample(message->command.command_id, NET_RESULT_DONE, 31u, 2u);
        NetBcmDoorState s = door_state_sample(900u, 1u);
        in_door_results[0] = BcmNet_SendResult(&accepted);
        in_door_results[1] = BcmNet_SendResult(&done);
        in_door_results[2] = BcmNet_PublishDoorState(&s);
    }
}

void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta)
{
    if (in_isr) ++isr_callbacks;
    ++climate_count;
    last_climate = *message;
    last_climate_meta = *meta;
}

void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta)
{
    if (in_isr) ++isr_callbacks;
    ++light_count;
    last_light = *message;
    last_light_meta = *meta;
}

/* ------------------------------------------------------------ bus ------- */
typedef struct { uint64_t at; MockCanFrame f; } CanOut;
static CanOut can_out[512];
static unsigned can_n;

/* The far end takes what the node sent (TX_COMPLETE). */
static void collect(void)
{
    MockCanFrame f;
    in_isr = true;
    while (MockRtd_CanTransmitNext(BUS, &f) >= 0) {
        if (can_n < sizeof(can_out) / sizeof(can_out[0])) { can_out[can_n].at = now_ms(); can_out[can_n].f = f; }
        ++can_n;
    }
    in_isr = false;
}

static uint64_t next_pass_us = PASS_US;

static void pass(void)
{
#ifdef BCM_NODE_TEST_ENTRY
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
#ifdef BCM_NODE_TEST_ENTRY
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
    return NetNode_Init(&c);
#endif
}

/* A DOMAIN frame on the bus (ISR context of the CAN controller). */
static int inject(const NetWireCanEnvelope *e)
{
    MockCanFrame f;
    int mb;
    memset(&f, 0, sizeof(f));
    f.id = e->standard_id; f.fd = e->fdf; f.brs = e->brs; f.dlc = e->dlc; f.length = (uint8)e->length;
    memcpy(f.data, e->bytes, e->length);
    in_isr = true;
    mb = MockRtd_CanInject(BUS, &f);
    in_isr = false;
    CHECK(mb >= 0);
    return 0;
}

static int door_from(uint32_t boot, uint32_t tx, uint32_t target, uint32_t command_id, uint16_t age, uint8_t lock_target)
{
    NetWireBcmDoorCommandRaw c;
    NetWireCanEnvelope e;
    memset(&c, 0, sizeof(c));
    c.boot_id = boot;
    c.tx_sequence = tx;
    c.target_boot_id = target;
    c.command_id = command_id;
    c.command_age_ms = age;
    c.accept_limit_ms = 200u;
    c.door_lock_target = lock_target;
    c.origin = NET_ORIGIN_MOBILE_MANUAL;
    CHECK(net_wire_can_encode_bcm_door_command(NET_WIRE_CANDIDATE_ONLY, &c, &e) == NET_WIRE_OK);
    return inject(&e);
}

static int door_in(uint32_t tx, uint32_t target, uint32_t command_id, uint16_t age, uint8_t lock_target)
{
    return door_from(DOMAIN_BOOT, tx, target, command_id, age, lock_target);
}

static int climate_in(uint32_t tx, uint32_t command_id, uint16_t age)
{
    NetWireBcmClimateCommandRaw c;
    NetWireCanEnvelope e;
    memset(&c, 0, sizeof(c));
    c.boot_id = DOMAIN_BOOT;
    c.tx_sequence = tx;
    c.target_boot_id = NODE_BOOT;
    c.command_id = command_id;
    c.command_age_ms = age;
    c.accept_limit_ms = 1000u;
    c.fan_target_level = NET_FAN_MEDIUM;
    c.thermal_direction = NET_THERMAL_COOL;
    c.thermal_target_percent = 65u;
    CHECK(net_wire_can_encode_bcm_climate_command(NET_WIRE_CANDIDATE_ONLY, &c, &e) == NET_WIRE_OK);
    return inject(&e);
}

static int light_in(uint32_t tx, uint32_t command_id, uint16_t age)
{
    NetWireBcmLightCommandRaw c;
    NetWireCanEnvelope e;
    memset(&c, 0, sizeof(c));
    c.boot_id = DOMAIN_BOOT;
    c.tx_sequence = tx;
    c.target_boot_id = NODE_BOOT;
    c.command_id = command_id;
    c.command_age_ms = age;
    c.accept_limit_ms = 200u;
    c.interior_light_type = NET_LIGHT_GOODBYE;
    c.light_level_percent = 80u;
    c.red = 10u; c.green = 200u; c.blue = 255u;
    CHECK(net_wire_can_encode_bcm_light_command(NET_WIRE_CANDIDATE_ONLY, &c, &e) == NET_WIRE_OK);
    return inject(&e);
}

static int permission_from(uint32_t boot, uint32_t tx, uint32_t update, uint16_t age)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope e;
    memset(&p, 0, sizeof(p));
    p.boot_id = boot;
    p.tx_sequence = tx;
    p.vehicle_power_permission = NET_TRISTATE_YES;
    p.power_permission_quality = NET_QUALITY_OK;
    p.power_operation_state = 1u;
    p.operation_allowed = 1u;
    p.operation_quality = NET_QUALITY_OK;
    p.update_sequence = update;
    p.source_age_ms = age;
    CHECK(net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, &e) == NET_WIRE_OK);
    return inject(&e);
}

static int permission_in(uint32_t tx, uint32_t update, uint16_t age)
{
    return permission_from(DOMAIN_BOOT, tx, update, age);
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

/* ----------------------------------------------------------- samples ---- */
static NetBcmResult result_sample(uint32_t command_id, NetVehicleResult result, uint32_t occurrence, uint16_t age)
{
    NetBcmResult r;
    memset(&r, 0, sizeof(r));
    r.command_domain_boot = DOMAIN_BOOT;
    r.command_id = command_id;
    r.command_kind = NET_BCM_COMMAND_DOOR;
    r.result = result;
    r.result_reason = NET_REASON_NONE;
    r.door_event = occurrence != 0u ? NET_DOOR_EVENT_LOCK_COMPLETED : NET_DOOR_EVENT_NONE;
    r.occurrence_id = occurrence;
    r.event_age_ms = age;
    r.event_quality = NET_QUALITY_OK;
    r.door_target = NET_DOOR_TARGET_LOCK;
    return r;
}

static NetBcmEvent event_sample(uint32_t occurrence, uint16_t age)
{
    NetBcmEvent v;
    memset(&v, 0, sizeof(v));
    v.event_kind = NET_BCM_EVENT_OVERHEAT_DETECTED;
    v.occurrence_id = occurrence;
    v.event_age_ms = age;
    v.event_quality = NET_QUALITY_OK;
    v.related_domain_boot = DOMAIN_BOOT;
    v.related_command_id = 77u;
    v.affected_function = 0x02u;
    return v;
}

static NetBcmDoorState door_state_sample(uint32_t update, uint16_t age)
{
    NetBcmDoorState s;
    memset(&s, 0, sizeof(s));
    s.door_lock_state = NET_DOOR_LOCK_LOCKED;
    s.door_open_state = NET_DOOR_OPEN_CLOSED;
    s.door_composite_state = NET_DOOR_COMPOSITE_NORMAL;
    s.lock_quality = NET_QUALITY_OK;
    s.open_quality = NET_QUALITY_STALE;
    s.composite_quality = NET_QUALITY_OK;
    s.update_sequence = update;
    s.source_age_ms = age;
    return s;
}

static NetBcmClimateState climate_state_sample(uint32_t update, uint16_t age)
{
    NetBcmClimateState s;
    memset(&s, 0, sizeof(s));
    s.fan_command_level = NET_FAN_HIGH;
    s.fan_measured_level = NET_FAN_UNKNOWN;
    s.thermal_direction_state = NET_THERMAL_HEAT;
    s.thermal_output_percent = 40u;
    s.heat_removal_state = NET_HEAT_REMOVAL_UNAVAILABLE;
    s.fan_command_quality = NET_QUALITY_OK;
    s.fan_measured_quality = NET_QUALITY_NO_DATA;
    s.thermal_direction_quality = NET_QUALITY_OK;
    s.thermal_output_quality = NET_QUALITY_OK;
    s.heat_removal_quality = NET_QUALITY_INVALID;
    s.update_sequence = update;
    s.source_age_ms = age;
    return s;
}

static NetBcmLightState light_state_sample(uint32_t update, uint16_t age)
{
    NetBcmLightState s;
    memset(&s, 0, sizeof(s));
    s.interior_light_type_state = NET_LIGHT_WARNING;
    s.light_level_percent = 55u;
    s.interior_light_color_state.r = 1u;
    s.interior_light_color_state.g = 2u;
    s.interior_light_color_state.b = 3u;
    s.interior_light_apply_result = NET_LIGHT_APPLY_APPLIED;
    s.light_physical_feedback_capability = NET_FLAG_NO;
    s.light_type_quality = NET_QUALITY_OK;
    s.light_level_quality = NET_QUALITY_OK;
    s.light_color_quality = NET_QUALITY_OK;
    s.light_apply_quality = NET_QUALITY_OK;
    s.update_sequence = update;
    s.source_age_ms = age;
    return s;
}

static NetBcmStatus status_sample(uint32_t update, uint16_t age)
{
    NetBcmStatus s;
    memset(&s, 0, sizeof(s));
    s.bcm_ecu_state = NET_CONTROLLER_DEGRADED;
    s.fault_mask = NET_BCM_FAULT_FAN_SENSOR_FAULT | NET_BCM_FAULT_COMM_TIMEOUT_FAULT;
    s.active_fault_category = NET_FAULT_CATEGORY_SENSOR;
    s.active_fault_code = NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT;
    s.affected_function = 0x02u;
    s.recovering = NET_FLAG_YES;
    s.update_sequence = update;
    s.source_age_ms = age;
    return s;
}

static int decode_result(unsigned k, NetWireBcmResultRaw *out)
{
    NetWireCanEnvelope e = envelope(&can_out[k].f);
    CHECK(can_out[k].f.fd && can_out[k].f.brs);
    CHECK(net_wire_can_decode_bcm_result(NET_WIRE_CANDIDATE_ONLY, &e, out) == NET_WIRE_OK);
    return 0;
}

/* -------------------------------------------------------------- cases ---- */
static int b01_init(void)
{
    NetNodeDiag d;
#ifndef BCM_NODE_TEST_ENTRY
    NetNodeConfig c;
    reset_bench();
    memset(&c, 0, sizeof(c));
    c.boot_id = NODE_BOOT;
    c.local_epoch = 1u;
    c.can_port_epoch = 1u;
    c.uart_c_port_epoch = 0u;                 /* no UART link in this profile: ignored */
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
    CHECK(!d.rtd.uart[0].uart_present && !d.rtd.uart[1].uart_present);
    CHECK(d.app.bound);
    /* M04 table: six messages, Network BOOT_ID, TX from 1, none repeated. */
    CHECK(d.result_tx.boot_id == NODE_BOOT && d.result_tx.next_sequence == 1u && d.result_tx.period_ms == 0u);
    CHECK(d.event_tx.boot_id == NODE_BOOT && d.event_tx.period_ms == 0u);
    CHECK(d.door_state_tx.period_ms == 0u && d.climate_state_tx.period_ms == 0u);
    CHECK(d.light_state_tx.period_ms == 0u && d.status_tx.period_ms == 0u);
    CHECK(!d.result_tx.has_source && !d.status_tx.has_source);
    step_ms(3u);
    CHECK(diag().steps == 3u);
    CHECK(can_n == 0u);                       /* nothing before a first submission */
    return 0;
}

static int b02_door_command_rx(void)
{
    NetNodeDiag d, d0;
    uint64_t injected_at;
    CHECK(start_node() == NET_OK);
    step_ms(4u);
    MockRtd_AdvanceUs(300u);
    injected_at = now_ms();
    RUN(door_in(1u, NODE_BOOT, 501u, 12u, NET_DOOR_TARGET_UNLOCK));
    CHECK(door_count == 0u);                  /* never from the ISR */
    step_ms(1u);
    CHECK(door_count == 1u && isr_callbacks == 0u);
    CHECK(climate_count == 0u && light_count == 0u);
    CHECK(last_door.command.target_boot_id == NODE_BOOT && last_door.command.command_id == 501u);
    CHECK(last_door.command.command_age_ms == 12u);          /* RX basis, not advanced */
    CHECK(last_door.command.accept_limit_ms == 200u);
    CHECK(last_door.door_lock_target == NET_DOOR_TARGET_UNLOCK);
    CHECK(last_door.origin == NET_ORIGIN_MOBILE_MANUAL);
    CHECK(last_door_meta.source.boot_id == DOMAIN_BOOT && last_door_meta.source.tx_sequence == 1u);
    CHECK(last_door_meta.received_at_ms == injected_at);
    d = diag();
    CHECK(d.app.door_delivered == 1u && d.app.command_wrong_target == 0u);
    /* Exact duplicate (same TX, same bytes): no callback. */
    d0 = d;
    RUN(door_in(1u, NODE_BOOT, 501u, 12u, NET_DOOR_TARGET_UNLOCK));
    step_ms(2u);
    d = diag();
    CHECK(door_count == 1u && d.upper.duplicates == d0.upper.duplicates + 1u);
    /* The same original command retransmitted with a new TX: no second drive. */
    RUN(door_in(2u, NODE_BOOT, 501u, 40u, NET_DOOR_TARGET_UNLOCK));
    step_ms(2u);
    CHECK(door_count == 1u);
    /* A new command: delivered. */
    RUN(door_in(3u, NODE_BOOT, 502u, 3u, NET_DOOR_TARGET_LOCK));
    step_ms(1u);
    CHECK(door_count == 2u && last_door.command.command_id == 502u && last_door_meta.source.tx_sequence == 3u);
    CHECK(last_door.door_lock_target == NET_DOOR_TARGET_LOCK);
    /* An older TX: not delivered. */
    d0 = diag();
    RUN(door_in(2u, NODE_BOOT, 503u, 3u, NET_DOOR_TARGET_LOCK));
    step_ms(2u);
    d = diag();
    CHECK(door_count == 2u && d.upper.stale == d0.upper.stale + 1u);
    CHECK(isr_callbacks == 0u && can_n == 0u);
    return 0;
}

static int b03_climate_light_command_rx(void)
{
    uint64_t at;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    at = now_ms();
    RUN(climate_in(1u, 601u, 20u));
    RUN(light_in(1u, 602u, 30u));
    step_ms(1u);
    CHECK(climate_count == 1u && light_count == 1u && door_count == 0u);
    CHECK(last_climate.command.target_boot_id == NODE_BOOT && last_climate.command.command_id == 601u);
    CHECK(last_climate.command.command_age_ms == 20u && last_climate.command.accept_limit_ms == 1000u);
    CHECK(last_climate.fan_target_level == NET_FAN_MEDIUM);
    CHECK(last_climate.thermal_direction == NET_THERMAL_COOL && last_climate.thermal_target_percent == 65u);
    CHECK(last_climate_meta.source.boot_id == DOMAIN_BOOT && last_climate_meta.source.tx_sequence == 1u);
    CHECK(last_climate_meta.received_at_ms == at);
    CHECK(last_light.command.command_id == 602u && last_light.command.command_age_ms == 30u);
    CHECK(last_light.interior_light_type == NET_LIGHT_GOODBYE && last_light.light_level_percent == 80u);
    CHECK(last_light.interior_light_color.r == 10u && last_light.interior_light_color.g == 200u);
    CHECK(last_light.interior_light_color.b == 255u);
    CHECK(last_light_meta.source.tx_sequence == 1u && last_light_meta.received_at_ms == at);
    CHECK(diag().app.climate_delivered == 1u && diag().app.light_delivered == 1u);
    /* Duplicates: no further callback. */
    RUN(climate_in(1u, 601u, 20u));
    RUN(light_in(1u, 602u, 30u));
    step_ms(2u);
    CHECK(climate_count == 1u && light_count == 1u && isr_callbacks == 0u);
    return 0;
}

/* Only commands addressed to this BCM boot and within the delivery bound reach
 * the application. */
static int b04_command_order_target(void)
{
    NetNodeDiag d, d0;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    d0 = diag();
    RUN(door_in(1u, OTHER_BOOT, 700u, 5u, NET_DOOR_TARGET_LOCK));   /* earlier BCM boot */
    step_ms(2u);
    d = diag();
    CHECK(door_count == 0u && d.app.command_wrong_target == 1u);
    CHECK(d.upper.consumer_rejected == d0.upper.consumer_rejected + 1u);
    /* Addressed correctly with the next TX: delivered. */
    RUN(door_in(2u, NODE_BOOT, 701u, 5u, NET_DOOR_TARGET_LOCK));
    step_ms(1u);
    CHECK(door_count == 1u && last_door.command.command_id == 701u);
    /* Command AGE at the 2000 ms delivery bound: isolated by the Network. */
    RUN(door_in(3u, NODE_BOOT, 702u, 2000u, NET_DOOR_TARGET_UNLOCK));
    step_ms(2u);
    CHECK(door_count == 1u);
    RUN(door_in(4u, NODE_BOOT, 703u, 1900u, NET_DOOR_TARGET_UNLOCK));
    step_ms(1u);
    CHECK(door_count == 2u && last_door.command.command_id == 703u && last_door.command.command_age_ms == 1900u);
    CHECK(can_n == 0u);
    return 0;
}

static int b05_permission_rx(void)
{
    NetNodeDiag d;
    uint64_t at;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    at = now_ms();
    RUN(permission_in(1u, 9u, 10u));
    step_ms(1u);
    d = diag();
    CHECK(door_count == 0u && climate_count == 0u && light_count == 0u);   /* no public App callback */
    CHECK(d.app.permission_received == 1u && d.app.permission_seen);
    CHECK(d.app.last_permission.vehicle_power_permission == NET_TRISTATE_YES);
    CHECK(d.app.last_permission.power_permission_quality == NET_QUALITY_OK);
    CHECK(d.app.last_permission.operation_allowed == NET_FLAG_YES);
    CHECK(d.app.last_permission.update_sequence == 9u && d.app.last_permission.source_age_ms == 10u);
    CHECK(d.app.last_permission_meta.source.boot_id == DOMAIN_BOOT && d.app.last_permission_meta.source.tx_sequence == 1u);
    CHECK(d.app.last_permission_meta.received_at_ms == at);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    /* 150 ms of silence: the supervision reports the receive timeout. */
    step_ms((unsigned)(at + 149u - now_ms()));
    CHECK(diag().permission.reason == NET_PERMISSION_CURRENT);
    step_ms(2u);
    d = diag();
    CHECK(d.permission.reason == NET_PERMISSION_RX_TIMEOUT && !d.permission.power_usable);
    CHECK(can_n == 0u);                       /* supervision sends nothing */
    return 0;
}

static int b06_publish_states(void)
{
    NetBcmDoorState door = door_state_sample(11u, 7u);
    NetBcmClimateState climate = climate_state_sample(21u, 30u);
    NetBcmLightState light = light_state_sample(31u, 4u);
    NetBcmStatus status = status_sample(41u, 2u);
    NetWireCanEnvelope e;
    NetWireBcmDoorStateRaw dr;
    NetWireBcmClimateStateRaw cr;
    NetWireBcmLightStateRaw lr;
    NetWireBcmStatusRaw sr;
    NetNodeDiag d;
    uint64_t submitted;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_AdvanceUs(400u);
    submitted = now_ms();
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishClimateState(&climate) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishLightState(&light) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_OK);
    CHECK(can_n == 0u);                       /* local copy only; sent by the next pass */
    step_ms(1u);
    CHECK(can_count(0x200u, 0u) == 1u && can_count(0x210u, 0u) == 1u);
    CHECK(can_count(0x220u, 0u) == 1u && can_count(0x230u, 0u) == 1u);

    k = can_find(0x200u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(can_out[k].f.fd && can_out[k].f.brs);
    CHECK(net_wire_can_decode_bcm_door_state(NET_WIRE_CANDIDATE_ONLY, &e, &dr) == NET_WIRE_OK);
    CHECK(dr.boot_id == NODE_BOOT && dr.tx_sequence == 1u);
    CHECK(dr.door_lock_state == NET_DOOR_LOCK_LOCKED && dr.door_open_state == NET_DOOR_OPEN_CLOSED);
    CHECK(dr.door_composite_state == NET_DOOR_COMPOSITE_NORMAL);
    CHECK(dr.lock_quality == NET_QUALITY_OK && dr.open_quality == NET_QUALITY_STALE && dr.composite_quality == NET_QUALITY_OK);
    CHECK(dr.update_sequence == 11u);
    CHECK(dr.source_age_ms == 7u + (can_out[k].at - submitted));      /* local wait once */

    k = can_find(0x210u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_climate_state(NET_WIRE_CANDIDATE_ONLY, &e, &cr) == NET_WIRE_OK);
    CHECK(cr.boot_id == NODE_BOOT && cr.tx_sequence == 1u);
    CHECK(cr.fan_command_level == NET_FAN_HIGH && cr.fan_measured_level == NET_FAN_UNKNOWN);
    CHECK(cr.thermal_direction_state == NET_THERMAL_HEAT && cr.thermal_output_percent == 40u);
    CHECK(cr.heat_removal_state == NET_HEAT_REMOVAL_UNAVAILABLE);
    CHECK(cr.fan_command_quality == NET_QUALITY_OK && cr.fan_measured_quality == NET_QUALITY_NO_DATA);
    CHECK(cr.thermal_direction_quality == NET_QUALITY_OK && cr.thermal_output_quality == NET_QUALITY_OK);
    CHECK(cr.heat_removal_quality == NET_QUALITY_INVALID && cr.update_sequence == 21u);
    CHECK(cr.source_age_ms == 30u + (can_out[k].at - submitted));

    k = can_find(0x220u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_light_state(NET_WIRE_CANDIDATE_ONLY, &e, &lr) == NET_WIRE_OK);
    CHECK(lr.boot_id == NODE_BOOT && lr.tx_sequence == 1u);
    CHECK(lr.interior_light_type_state == NET_LIGHT_WARNING && lr.light_level_percent == 55u);
    CHECK(lr.red == 1u && lr.green == 2u && lr.blue == 3u);
    CHECK(lr.interior_light_apply_result == NET_LIGHT_APPLY_APPLIED);
    CHECK(lr.light_physical_feedback_capability == NET_FLAG_NO);
    CHECK(lr.light_type_quality == NET_QUALITY_OK && lr.light_level_quality == NET_QUALITY_OK);
    CHECK(lr.light_color_quality == NET_QUALITY_OK && lr.light_apply_quality == NET_QUALITY_OK);
    CHECK(lr.update_sequence == 31u && lr.source_age_ms == 4u + (can_out[k].at - submitted));

    k = can_find(0x230u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_status(NET_WIRE_CANDIDATE_ONLY, &e, &sr) == NET_WIRE_OK);
    CHECK(sr.boot_id == NODE_BOOT && sr.tx_sequence == 1u);
    CHECK(sr.bcm_ecu_state == NET_CONTROLLER_DEGRADED);
    CHECK(sr.fault_mask == (NET_BCM_FAULT_FAN_SENSOR_FAULT | NET_BCM_FAULT_COMM_TIMEOUT_FAULT));
    CHECK(sr.active_fault_category == NET_FAULT_CATEGORY_SENSOR);
    CHECK(sr.active_fault_code == NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT);
    CHECK(sr.affected_function == 0x02u && sr.recovering == NET_FLAG_YES && sr.update_sequence == 41u);
    CHECK(sr.source_age_ms == 2u + (can_out[k].at - submitted));

    /* Next submissions: TX_SEQUENCE +1 per message. Two before a pass: the
     * newer replaces the older one no device took (current value). */
    step_ms(5u);
    door = door_state_sample(12u, 1u);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_OK);
    door = door_state_sample(13u, 1u);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_OK);
    status = status_sample(42u, 1u);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x200u, 0u) == 2u && can_count(0x230u, 0u) == 2u);
    k = can_find(0x200u, 0u, 1u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_door_state(NET_WIRE_CANDIDATE_ONLY, &e, &dr) == NET_WIRE_OK);
    CHECK(dr.tx_sequence == 2u && dr.update_sequence == 13u);
    k = can_find(0x230u, 0u, 1u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_status(NET_WIRE_CANDIDATE_ONLY, &e, &sr) == NET_WIRE_OK);
    CHECK(sr.tx_sequence == 2u && sr.update_sequence == 42u);
    d = diag();
    CHECK(d.door_state_tx.counters.submitted == 3u && d.door_state_tx.counters.replaced == 1u);
    CHECK(d.door_state_tx.next_sequence == 3u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_DOOR_STATE].not_accepted == 0u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_DOOR_STATE].last_result == NET_SUBMIT_OK);
    return 0;
}

/* Application-periodic states (and discrete records): Network never repeats. */
static int b07_states_not_repeated(void)
{
    NetBcmDoorState door = door_state_sample(1u, 0u);
    NetBcmClimateState climate = climate_state_sample(1u, 0u);
    NetBcmLightState light = light_state_sample(1u, 0u);
    NetBcmStatus status = status_sample(1u, 0u);
    NetBcmResult result = result_sample(5u, NET_RESULT_DONE, 0u, 0u);
    NetBcmEvent ev = event_sample(3u, 0u);
    NetNodeDiag d;
    unsigned t;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishClimateState(&climate) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishLightState(&light) == NET_SUBMIT_OK);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_OK);
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_OK);
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_OK);
    for (t = 0u; t < 2000u; t += 100u) step_ms(100u);
    CHECK(can_count(0x200u, 0u) == 1u && can_count(0x210u, 0u) == 1u);
    CHECK(can_count(0x220u, 0u) == 1u && can_count(0x230u, 0u) == 1u);
    CHECK(can_count(0x140u, 0u) == 1u && can_count(0x141u, 0u) == 1u);
    CHECK(can_n == 6u);
    d = diag();
    CHECK(d.door_state_tx.counters.periodic_sent == 0u && d.climate_state_tx.counters.periodic_sent == 0u);
    CHECK(d.light_state_tx.counters.periodic_sent == 0u && d.status_tx.counters.periodic_sent == 0u);
    CHECK(d.result_tx.counters.periodic_sent == 0u && d.event_tx.counters.periodic_sent == 0u);
    return 0;
}

static int b08_send_result_event(void)
{
    NetBcmResult result = result_sample(801u, NET_RESULT_DONE, 17u, 15u);
    NetBcmEvent ev = event_sample(9u, 25u);
    NetWireBcmResultRaw rr;
    NetWireBcmEventRaw er;
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint64_t submitted;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    MockRtd_AdvanceUs(500u);
    submitted = now_ms();
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_OK);
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_OK);
    CHECK(can_n == 0u);
    step_ms(1u);
    CHECK(can_count(0x140u, 0u) == 1u && can_count(0x141u, 0u) == 1u);
    k = can_find(0x140u, 0u, 0u);
    CHECK(k >= 0);
    RUN(decode_result((unsigned)k, &rr));
    CHECK(rr.boot_id == NODE_BOOT && rr.tx_sequence == 1u);
    CHECK(rr.command_domain_boot == DOMAIN_BOOT && rr.command_id == 801u);       /* original command echoed */
    CHECK(rr.command_kind == NET_BCM_COMMAND_DOOR && rr.result == NET_RESULT_DONE);
    CHECK(rr.result_reason == NET_REASON_NONE && rr.door_event == NET_DOOR_EVENT_LOCK_COMPLETED);
    CHECK(rr.occurrence_id == 17u && rr.event_quality == NET_QUALITY_OK && rr.door_target == NET_DOOR_TARGET_LOCK);
    CHECK(rr.event_age_ms == 15u + (can_out[k].at - submitted));
    k = can_find(0x141u, 0u, 0u);
    CHECK(k >= 0);
    e = envelope(&can_out[k].f);
    CHECK(net_wire_can_decode_bcm_event(NET_WIRE_CANDIDATE_ONLY, &e, &er) == NET_WIRE_OK);
    CHECK(er.boot_id == NODE_BOOT && er.tx_sequence == 1u);
    CHECK(er.event_kind == NET_BCM_EVENT_OVERHEAT_DETECTED && er.occurrence_id == 9u);
    CHECK(er.event_quality == NET_QUALITY_OK && er.related_domain_boot == DOMAIN_BOOT);
    CHECK(er.related_command_id == 77u && er.affected_function == 0x02u);
    CHECK(er.event_age_ms == 25u + (can_out[k].at - submitted));
    /* A later result: TX_SEQUENCE +1, its own identity. */
    step_ms(5u);
    result = result_sample(802u, NET_RESULT_REJECTED, 0u, 0u);
    result.result_reason = NET_REASON_DOOR_OPEN;
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_OK);
    step_ms(1u);
    k = can_find(0x140u, 0u, 1u);
    CHECK(k >= 0);
    RUN(decode_result((unsigned)k, &rr));
    CHECK(rr.tx_sequence == 2u && rr.command_id == 802u && rr.result == NET_RESULT_REJECTED);
    CHECK(rr.result_reason == NET_REASON_DOOR_OPEN && rr.door_event == NET_DOOR_EVENT_NONE && rr.occurrence_id == 0u);
    d = diag();
    CHECK(d.result_tx.counters.sent == 2u && d.result_tx.counters.replaced == 0u && d.result_tx.next_sequence == 3u);
    CHECK(d.event_tx.counters.sent == 1u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_RESULT].last_result == NET_SUBMIT_OK);
    return 0;
}

/* Several results before the first one is sent: every record is kept and
 * sent once, in call order, with its own identity and its whole wait in AGE. */
static int b09_discrete_kept_in_order(void)
{
    NetBcmResult r;
    NetBcmEvent ev;
    NetWireBcmResultRaw rr;
    NetWireBcmEventRaw er;
    NetWireCanEnvelope e;
    NetNodeDiag d;
    uint64_t submitted;
    unsigned i;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    submitted = now_ms();
    r = result_sample(900u, NET_RESULT_ACCEPTED, 0u, 0u);
    CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_OK);
    r = result_sample(900u, NET_RESULT_IN_PROGRESS, 0u, 0u);
    CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_OK);
    r = result_sample(900u, NET_RESULT_DONE, 44u, 6u);
    CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_OK);
    ev = event_sample(1u, 10u);
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_OK);
    ev = event_sample(2u, 20u);
    ev.event_kind = NET_BCM_EVENT_RECOVERY_CONFIRMED;
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_OK);
    d = diag();
    CHECK(d.app.result_store.count == 2u && d.app.event_store.count == 1u);
    step_ms(30u);
    CHECK(can_count(0x140u, 0u) == 3u && can_count(0x141u, 0u) == 2u);
    for (i = 0u; i < 3u; ++i) {
        k = can_find(0x140u, 0u, i);
        CHECK(k >= 0);
        RUN(decode_result((unsigned)k, &rr));
        CHECK(rr.tx_sequence == i + 1u && rr.command_id == 900u);
        CHECK(rr.result == (i == 0u ? NET_RESULT_ACCEPTED : i == 1u ? NET_RESULT_IN_PROGRESS : NET_RESULT_DONE));
        CHECK(rr.event_age_ms == (i == 2u ? 6u : 0u) + (can_out[k].at - submitted));
    }
    CHECK(rr.occurrence_id == 44u && rr.door_event == NET_DOOR_EVENT_LOCK_COMPLETED);
    for (i = 0u; i < 2u; ++i) {
        k = can_find(0x141u, 0u, i);
        CHECK(k >= 0);
        e = envelope(&can_out[k].f);
        CHECK(net_wire_can_decode_bcm_event(NET_WIRE_CANDIDATE_ONLY, &e, &er) == NET_WIRE_OK);
        CHECK(er.tx_sequence == i + 1u && er.occurrence_id == i + 1u);
        CHECK(er.event_age_ms == 10u * (i + 1u) + (can_out[k].at - submitted));
    }
    CHECK(er.event_kind == NET_BCM_EVENT_RECOVERY_CONFIRMED);
    d = diag();
    CHECK(d.result_tx.counters.submitted == 3u && d.result_tx.counters.replaced == 0u);
    CHECK(d.event_tx.counters.replaced == 0u);
    CHECK(d.app.result_store.count == 0u && d.app.result_store.queued == 2u && d.app.result_store.depth_max == 2u);
    CHECK(d.app.event_store.queued == 1u && d.app.result_store.offer_refused == 0u);
    step_ms(1000u);
    CHECK(can_count(0x140u, 0u) == 3u && can_count(0x141u, 0u) == 2u);   /* each once */
    return 0;
}

/* A full store refuses the new record; nothing kept is overwritten. */
static int b10_discrete_no_capacity(void)
{
    NetBcmResult r;
    NetWireBcmResultRaw rr;
    NetNodeDiag d;
    unsigned i;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* One goes to the idle slot, NET_APP_PORT_DISCRETE_DEPTH wait. */
    for (i = 0u; i < 1u + NET_APP_PORT_DISCRETE_DEPTH; ++i) {
        r = result_sample(1000u + i, NET_RESULT_DONE, 0u, 0u);
        CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_OK);
    }
    r = result_sample(2000u, NET_RESULT_DONE, 0u, 0u);
    CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_NO_CAPACITY);
    d = diag();
    CHECK(d.app.result_store.queue_full == 1u && d.app.result_store.count == NET_APP_PORT_DISCRETE_DEPTH);
    CHECK(d.app.publish[NET_APP_PORT_BCM_RESULT].last_result == NET_SUBMIT_NO_CAPACITY);
    step_ms(40u);
    CHECK(can_count(0x140u, 0u) == 1u + NET_APP_PORT_DISCRETE_DEPTH);
    for (i = 0u; i < 1u + NET_APP_PORT_DISCRETE_DEPTH; ++i) {
        k = can_find(0x140u, 0u, i);
        CHECK(k >= 0);
        RUN(decode_result((unsigned)k, &rr));
        CHECK(rr.tx_sequence == i + 1u && rr.command_id == 1000u + i);
    }
    /* Room again: accepted and sent. */
    r = result_sample(2001u, NET_RESULT_DONE, 0u, 0u);
    CHECK(BcmNet_SendResult(&r) == NET_SUBMIT_OK);
    step_ms(3u);
    k = can_find(0x140u, 0u, 1u + NET_APP_PORT_DISCRETE_DEPTH);
    CHECK(k >= 0);
    RUN(decode_result((unsigned)k, &rr));
    CHECK(rr.command_id == 2001u && rr.tx_sequence == 2u + NET_APP_PORT_DISCRETE_DEPTH);
    return 0;
}

static int b11_publish_invalid(void)
{
    NetBcmDoorState door = door_state_sample(1u, 0u);
    NetBcmClimateState climate = climate_state_sample(1u, 0u);
    NetBcmLightState light = light_state_sample(1u, 0u);
    NetBcmStatus status = status_sample(1u, 0u);
    NetBcmResult result = result_sample(1u, NET_RESULT_DONE, 0u, 0u);
    NetBcmEvent ev = event_sample(1u, 0u);
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(BcmNet_SendResult(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(BcmNet_SendEvent(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(BcmNet_PublishDoorState(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(BcmNet_PublishClimateState(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(BcmNet_PublishLightState(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(BcmNet_PublishStatus(NULL) == NET_SUBMIT_INVALID_ARGUMENT);
    /* Reserved codes and combinations. */
    door.door_composite_state = 3u;
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_INVALID_ARGUMENT);
    climate.thermal_output_percent = 101u;
    CHECK(BcmNet_PublishClimateState(&climate) == NET_SUBMIT_INVALID_ARGUMENT);
    light.interior_light_apply_result = 2u;
    CHECK(BcmNet_PublishLightState(&light) == NET_SUBMIT_INVALID_ARGUMENT);
    status.bcm_ecu_state = 4u;
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_INVALID_ARGUMENT);
    result.command_kind = 0u;
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_INVALID_ARGUMENT);
    result = result_sample(1u, NET_RESULT_DONE, 0u, 0u);
    result.door_target = NET_DOOR_TARGET_NOT_APPLICABLE;   /* door result without a door target */
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_INVALID_ARGUMENT);
    ev.event_kind = 4u;
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_INVALID_ARGUMENT);
    step_ms(20u);
    CHECK(can_n == 0u);
    d = diag();
    CHECK(d.app.publish[NET_APP_PORT_BCM_RESULT].not_accepted == 3u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_EVENT].not_accepted == 2u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_DOOR_STATE].last_result == NET_SUBMIT_INVALID_ARGUMENT);
    CHECK(d.app.result_store.count == 0u && d.result_tx.counters.submitted == 0u);
    /* Still usable afterwards. */
    result = result_sample(2u, NET_RESULT_DONE, 0u, 0u);
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_OK);
    step_ms(1u);
    CHECK(can_count(0x140u, 0u) == 1u);
    return 0;
}

/* Submissions inside a command callback: OK, offered in the same pass; the
 * second result of that callback waits for the first and keeps its order. */
static int b12_publish_in_callback(void)
{
    NetWireBcmResultRaw rr;
    NetNodeDiag d;
    unsigned before;
    int k;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    submit_in_door = true;
    RUN(door_in(1u, NODE_BOOT, 1234u, 5u, NET_DOOR_TARGET_LOCK));
    before = can_n;
    step_ms(1u);
    CHECK(door_count == 1u);
    CHECK(in_door_results[0] == NET_SUBMIT_OK && in_door_results[1] == NET_SUBMIT_OK);
    CHECK(in_door_results[2] == NET_SUBMIT_OK);
    /* Same pass: the first result and the door state. */
    CHECK(can_count(0x140u, before) == 1u && can_count(0x200u, before) == 1u);
    CHECK(can_out[can_find(0x140u, before, 0u)].at == door_delivered_at);
    d = diag();
    CHECK(d.app.publish[NET_APP_PORT_BCM_RESULT].deferred == 2u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_DOOR_STATE].deferred == 1u);
    CHECK(d.app.result_store.queued == 1u);
    step_ms(10u);
    CHECK(can_count(0x140u, 0u) == 2u);
    k = can_find(0x140u, 0u, 0u);
    CHECK(k >= 0);
    RUN(decode_result((unsigned)k, &rr));
    CHECK(rr.tx_sequence == 1u && rr.command_id == 1234u && rr.result == NET_RESULT_ACCEPTED);
    k = can_find(0x140u, 0u, 1u);
    CHECK(k >= 0);
    RUN(decode_result((unsigned)k, &rr));
    CHECK(rr.tx_sequence == 2u && rr.command_id == 1234u && rr.result == NET_RESULT_DONE);
    CHECK(rr.occurrence_id == 31u);
    /* AGE: submitted inside the callback, the wait since that pass added once. */
    CHECK(rr.event_age_ms == 2u + (can_out[k].at - door_delivered_at));
    submit_in_door = false;
    return 0;
}

static int b13_not_ready(void)
{
    NetBcmDoorState door = door_state_sample(1u, 0u);
    NetBcmClimateState climate = climate_state_sample(1u, 0u);
    NetBcmLightState light = light_state_sample(1u, 0u);
    NetBcmStatus status = status_sample(1u, 0u);
    NetBcmResult result = result_sample(1u, NET_RESULT_DONE, 0u, 0u);
    NetBcmEvent ev = event_sample(1u, 0u);
    NetNodeDiag d;
    /* Before the node runs: nothing is bound. */
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishClimateState(&climate) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishLightState(&light) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_NOT_READY);
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_OK);
    step_ms(2u);
    CHECK(can_count(0x200u, 0u) == 1u);
    /* Bus-off: the CAN link is closed for the hold time (100 ms, longer than
     * this case); nothing is taken. */
    MockRtd_CanRaiseBusOff(BUS);
    step_ms(2u);
    CHECK(BcmNet_SendResult(&result) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_SendEvent(&ev) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishDoorState(&door) == NET_SUBMIT_NOT_READY);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_NOT_READY);
    step_ms(20u);
    CHECK(can_n == 1u);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_READY && d.rtd.busoff_events == 1u);
    CHECK(d.app.result_store.count == 0u);
    CHECK(d.app.publish[NET_APP_PORT_BCM_RESULT].last_result == NET_SUBMIT_NOT_READY);
    return 0;
}

/* DOMAIN restart (04_COMMON/10_DATA_AND_ID_RULES.md generation): run A, then
 * run B with another BOOT_ID and TX from 1. BCM takes DOMAIN_PERMISSION and
 * the commands of run B again (each CAN ID on its own first accepted B frame);
 * a late frame of run A is refused. The command target rule stays: a B
 * command addressed to another BCM boot reaches no application and does not
 * move the CAN ID to run B. Repetitions of one DOMAIN_PERMISSION evaluation
 * reach the supervision record; a retransmitted command does not drive twice. */
static int b13_domain_reboot(void)
{
    enum { BOOT_A = 0x444F4D41u, BOOT_B = 0x444F4D42u };
    NetNodeDiag d0, d;
    CHECK(start_node() == NET_OK);
    step_ms(2u);
    /* Run A. */
    RUN(permission_from(BOOT_A, 4u, 20u, 10u));
    RUN(door_from(BOOT_A, 7u, NODE_BOOT, 501u, 5u, NET_DOOR_TARGET_LOCK));
    step_ms(1u);
    d = diag();
    CHECK(door_count == 1u && d.app.permission_received == 1u);
    CHECK(d.app.last_permission_meta.source.boot_id == BOOT_A);
    /* The same evaluation 50 ms later: the supervision record follows it. */
    step_ms(49u);
    RUN(permission_from(BOOT_A, 5u, 20u, 60u));
    step_ms(1u);
    d = diag();
    CHECK(d.app.permission_received == 2u && d.app.last_permission_meta.source.tx_sequence == 5u);
    CHECK(d.upper.repeat_delivered == 1u);
    /* The same command again with a new TX: no second drive. */
    RUN(door_from(BOOT_A, 8u, NODE_BOOT, 501u, 56u, NET_DOOR_TARGET_LOCK));
    step_ms(1u);
    CHECK(door_count == 1u);
    d0 = diag();
    /* DOMAIN restarts: run B. */
    step_ms(20u);
    RUN(permission_from(BOOT_B, 1u, 1u, 3u));
    step_ms(1u);
    d = diag();
    CHECK(d.app.permission_received == 3u);
    CHECK(d.app.last_permission_meta.source.boot_id == BOOT_B && d.app.last_permission_meta.source.tx_sequence == 1u);
    CHECK(d.app.last_permission.update_sequence == 1u);
    CHECK(d.permission_valid && d.permission.reason == NET_PERMISSION_CURRENT && d.permission.power_usable);
    /* A B command for another BCM boot: refused by the target rule; the
     * door CAN ID stays on run A. */
    RUN(door_from(BOOT_B, 1u, OTHER_BOOT, 1u, 2u, NET_DOOR_TARGET_UNLOCK));
    step_ms(1u);
    d = diag();
    CHECK(door_count == 1u && d.app.command_wrong_target == d0.app.command_wrong_target + 1u);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 1u);
    /* The next B command for this boot: delivered, the door CAN ID moves to B. */
    RUN(door_from(BOOT_B, 2u, NODE_BOOT, 2u, 3u, NET_DOOR_TARGET_UNLOCK));
    step_ms(1u);
    d = diag();
    CHECK(door_count == 2u && last_door.command.command_id == 2u);
    CHECK(last_door_meta.source.boot_id == BOOT_B && last_door_meta.source.tx_sequence == 2u);
    CHECK(d.upper.generation_changes == d0.upper.generation_changes + 2u);
    /* Late frames of run A: refused, nothing delivered. */
    d0 = d;
    RUN(door_from(BOOT_A, 9u, NODE_BOOT, 502u, 3u, NET_DOOR_TARGET_UNLOCK));
    RUN(permission_from(BOOT_A, 6u, 21u, 2u));
    step_ms(2u);
    d = diag();
    CHECK(door_count == 2u && d.app.permission_received == 3u);
    CHECK(d.upper.old_generation == d0.upper.old_generation + 2u);
    CHECK(d.app.last_permission_meta.source.boot_id == BOOT_B);
    CHECK(isr_callbacks == 0u);
    return 0;
}

#ifdef BCM_NODE_TEST_ENTRY
/* The public entry drives the BCM profile exactly like the VSS one. */
static int b14_entry(void)
{
    NetBcmStatus status = status_sample(3u, 4u);
    NetWireBcmStatusRaw sr;
    NetNodeDiag d;
    uint64_t injected_at;
    int k;
    CHECK(start_node() == NET_OK);
    d = diag();
    CHECK(d.entry_init_result == NET_ENTRY_INIT_OK && d.entry_init_calls == 1u);
    CHECK(d.rtd.time_source == NET_HAL_RTD_TIME_APP_TICK);
    CHECK(d.status_tx.boot_id == NODE_BOOT);  /* BOOT_ID from the board hook */
    Net_Init();                               /* later calls change nothing */
    CHECK(diag().entry_init_calls == 2u && diag().stage == NET_NODE_STAGE_READY);
    step_ms(3u);
    CHECK(diag().entry_main_calls == 3u);
    MockRtd_AdvanceUs(200u);
    injected_at = now_ms();
    RUN(door_in(1u, NODE_BOOT, 55u, 4u, NET_DOOR_TARGET_UNLOCK));
    step_ms(1u);
    CHECK(door_count == 1u && last_door_meta.received_at_ms == injected_at && isr_callbacks == 0u);
    CHECK(BcmNet_PublishStatus(&status) == NET_SUBMIT_OK);
    step_ms(1u);
    k = can_find(0x230u, 0u, 0u);
    CHECK(k >= 0);
    {
        NetWireCanEnvelope e = envelope(&can_out[k].f);
        CHECK(net_wire_can_decode_bcm_status(NET_WIRE_CANDIDATE_ONLY, &e, &sr) == NET_WIRE_OK);
        CHECK(sr.boot_id == NODE_BOOT && sr.tx_sequence == 1u && sr.update_sequence == 3u);
    }
    d = diag();
    CHECK(d.rtd.tick_future_clamped == 0u && d.entry_now_backward == 0u);
    return 0;
}
#endif

/* ------------------------------------------------ documented traffic ---- */
/* Ten seconds of the documented traffic (net_load_traffic.h):
 *   CAN  : the periodic frames of DOMAIN, CIS and VSS as their senders put
 *          them on the bus (10_CAN_FD.md message list, phase 0: seven of
 *          them start together every second). The RX message buffers accept
 *          every ID, so all of them enter the BCM receive path; the routes of
 *          the other nodes end in the registry, not as a loss.
 *   App  : BCM_DOOR_STATE, BCM_CLIMATE_STATE, BCM_LIGHT_STATE and BCM_STATUS
 *          at their 10_CAN_FD.md periods, in the pass that reaches the due
 *          time, after NetNode_Step(); the next pass sends them.
 *   Main : a pass every 10 ms.
 * At an instant shared by a pass and bus frames the pass runs first, so those
 * frames wait a whole pass. Nothing may be lost (ISR ring, adapter and
 * service queues, M04 slots, every DOMAIN_PERMISSION delivered to the
 * supervision), an own frame leaves within its period plus one pass (10 ms),
 * and no queue goes past its capacity. The LOAD line reports the counts and
 * the high-water marks against the capacities of this build. */
enum { LOAD_OWN = 4 };
#define LOAD_PASS_US 10000u
#define LOAD_SLACK_MS 10u
#define LOAD_FLUSH_US 100000u

static NetLoadTx load_tx[LOAD_OWN];
static uint64_t load_app_due[LOAD_OWN];
static uint32_t load_app_sequence[LOAD_OWN];
static bool load_app_on;
static unsigned load_app_submitted, load_app_refused, load_foreign_tx, load_inject_lost;

static void load_init_own(void)
{
    size_t k;
    unsigned j = 0u;
    for (k = 0u; k < NET_LOAD_MESSAGE_COUNT && j < LOAD_OWN; ++k) {
        if (net_load_messages[k].sender != NET_LOAD_BCM) continue;
        net_load_tx_init(&load_tx[j], net_load_messages[k].id, net_load_messages[k].period_ms, LOAD_SLACK_MS);
        load_app_due[j] = 0u;
        load_app_sequence[j] = 0u;
        ++j;
    }
}

static NetSubmitResult load_submit(uint16_t id, uint32_t n)
{
    switch (id) {
    case 0x200u: { NetBcmDoorState s = door_state_sample(n, 5u); return BcmNet_PublishDoorState(&s); }
    case 0x210u: { NetBcmClimateState s = climate_state_sample(n, 5u); return BcmNet_PublishClimateState(&s); }
    case 0x220u: { NetBcmLightState s = light_state_sample(n, 5u); return BcmNet_PublishLightState(&s); }
    default: { NetBcmStatus s = status_sample(n, 5u); return BcmNet_PublishStatus(&s); }
    }
}

/* The BCM application in the superloop: what is due is submitted now. */
static void load_app(uint64_t now)
{
    unsigned j;
    if (!load_app_on) return;
    for (j = 0u; j < LOAD_OWN; ++j) {
        if (now < load_app_due[j]) continue;
        load_app_due[j] += load_tx[j].period_ms;
        ++load_app_submitted;
        if (load_submit(load_tx[j].id, ++load_app_sequence[j]) != NET_SUBMIT_OK) ++load_app_refused;
    }
}

/* The far end takes what the node sent (TX_COMPLETE). */
static void load_collect(void)
{
    MockCanFrame f;
    NetWireCanEnvelope e;
    uint64_t now = now_ms();
    in_isr = true;
    while (MockRtd_CanTransmitNext(BUS, &f) >= 0) {
        e = envelope(&f);
        if (!net_load_tx_seen(load_tx, LOAD_OWN, &e, now)) ++load_foreign_tx;
    }
    in_isr = false;
}

/* One superloop pass in the documented order (net_node.h,
 * VSS_NODE_INTEGRATION.md): the Network pass, then the application. */
static void load_pass(void)
{
#ifdef BCM_NODE_TEST_ENTRY
    Net_MainFunction((uint32_t)now_ms());
#else
    NetNode_Step();
#endif
    load_collect();
    load_app(now_ms());
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

/* A frame that found no device room waits one pass in the service queue
 * (upper.tx_deferred) and keeps its number: a delay, counted apart. Lost is
 * what never left. */
static uint32_t load_slot_lost(const NetUpperPublishView *v)
{
    return v->counters.not_sent + v->counters.tx_failed + v->counters.replaced;
}

static int load_doc_traffic(void)
{
    const uint64_t end_us = (uint64_t)NET_LOAD_DURATION_MS * 1000u;
    uint32_t sent[NET_LOAD_MESSAGE_COUNT];
    uint64_t due_us[NET_LOAD_MESSAGE_COUNT];
    uint64_t pass_at = LOAD_PASS_US, t;
    uint32_t passes = 0u, permissions = 0u, injected = 0u;
    uint32_t lost, violations, tx_frames;
    size_t k;
    NetNodeDiag d;
    CHECK(start_node() == NET_OK);
    load_init_own();
    load_app_on = true;
    for (k = 0u; k < NET_LOAD_MESSAGE_COUNT; ++k) {
        sent[k] = 0u;
        due_us[k] = net_load_messages[k].sender == NET_LOAD_BCM ? UINT64_MAX : 0u;
    }
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
            RUN(load_inject(k, sent[k]++));
            ++injected;
            if (net_load_messages[k].id == 0x080u) ++permissions;
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
    {
        const NetLoadCount loss[] = {
            { "upper.rx_dropped", d.upper.rx_dropped },
            { "hal.rx_dropped", d.hal.rx_dropped },
            { "rtd.isr_ring_overflow", d.rtd.isr_ring_overflow },
            { "rtd.rx_overruns", d.rtd.rx_overruns },
            { "rtd.rx_queue_rejected", d.rtd.rx_queue_rejected },
            { "bus_frames_not_taken", load_inject_lost },
            { "app_refused", load_app_refused },
            { "result_tx", load_slot_lost(&d.result_tx) },
            { "event_tx", load_slot_lost(&d.event_tx) },
            { "door_state_tx", load_slot_lost(&d.door_state_tx) },
            { "climate_state_tx", load_slot_lost(&d.climate_state_tx) },
            { "light_state_tx", load_slot_lost(&d.light_state_tx) },
            { "status_tx", load_slot_lost(&d.status_tx) },
            { "permission_not_delivered", permissions > d.app.permission_received ?
                                              permissions - d.app.permission_received : 0u },
            { "tx_sequence_gaps", net_load_tx_gaps(load_tx, LOAD_OWN) }
        };
        const NetLoadCount late[] = {
            { "can_periods", net_load_tx_violations(load_tx, LOAD_OWN, NET_LOAD_DURATION_MS) }
        };
        lost = net_load_report("load lost", loss, sizeof(loss) / sizeof(loss[0]));
        violations = net_load_report("load late", late, sizeof(late) / sizeof(late[0]));
    }
    tx_frames = net_load_tx_frames(load_tx, LOAD_OWN);
    printf("LOAD BCM rx_frames=%lu tx_frames=%lu lost=%lu period_violations=%lu can_ring_max=%lu/%lu "
           "hal_rx_max=%lu/%lu upper_rx_max=%lu/%lu upper_tx_max=%lu/%lu\n",
           (unsigned long)d.rtd.rx_frames, (unsigned long)tx_frames, (unsigned long)lost, (unsigned long)violations,
           (unsigned long)d.rtd.isr_ring_max_depth, (unsigned long)NET_BOARD_CAN_ISR_RING,
           (unsigned long)d.hal.rx_max_depth, (unsigned long)NET_HAL_RX_CAPACITY,
           (unsigned long)d.upper.rx_max_depth, (unsigned long)NET_UPPER_RX_CAPACITY,
           (unsigned long)d.upper.tx_max_depth, (unsigned long)NET_UPPER_TX_CAPACITY);
    printf("load detail: passes=%lu injected=%lu permission=%lu/%lu submitted=%u refused=%u drained_max=%lu/%lu "
           "rx_invalid=%lu rx_held=%lu queue_full=%lu,%lu,%lu,%lu,%lu,%lu max_gap_ms=%lu,%lu,%lu,%lu\n",
           (unsigned long)passes, (unsigned long)injected, (unsigned long)d.app.permission_received,
           (unsigned long)permissions, load_app_submitted, load_app_refused,
           (unsigned long)d.rtd.main_drained_max, (unsigned long)d.rtd.isr_drain_budget,
           (unsigned long)d.upper.rx_invalid, (unsigned long)d.upper.rx_held,
           (unsigned long)d.result_tx.counters.queue_full, (unsigned long)d.event_tx.counters.queue_full,
           (unsigned long)d.door_state_tx.counters.queue_full, (unsigned long)d.climate_state_tx.counters.queue_full,
           (unsigned long)d.light_state_tx.counters.queue_full, (unsigned long)d.status_tx.counters.queue_full,
           (unsigned long)load_tx[0].max_gap_ms, (unsigned long)load_tx[1].max_gap_ms,
           (unsigned long)load_tx[2].max_gap_ms, (unsigned long)load_tx[3].max_gap_ms);
    fflush(stdout);
    CHECK(d.rtd.isr_ring_capacity == NET_BOARD_CAN_ISR_RING);
    CHECK(d.rtd.rx_frames == injected);       /* every bus frame entered the receive path */
    CHECK(tx_frames == load_app_submitted);   /* one frame per submission */
    CHECK(d.app.permission_received <= permissions);
    CHECK(door_count == 0u && climate_count == 0u && light_count == 0u);   /* no command on the bus */
    CHECK(load_foreign_tx == 0u && isr_callbacks == 0u);
    CHECK(lost == 0u);
    CHECK(violations == 0u);
    CHECK(d.rtd.isr_ring_max_depth <= NET_BOARD_CAN_ISR_RING && d.hal.rx_max_depth <= NET_HAL_RX_CAPACITY);
    CHECK(d.upper.rx_max_depth <= NET_UPPER_RX_CAPACITY && d.upper.tx_max_depth <= NET_UPPER_TX_CAPACITY);
    /* The high-water marks saw the traffic: seven frames of the bus at once,
     * the four messages of the BCM submitted in one pass. */
    CHECK(d.rtd.isr_ring_max_depth >= 7u && d.upper.tx_max_depth >= 4u);
    CHECK(d.hal.rx_max_depth >= 1u && d.upper.rx_max_depth >= 1u);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "b01_init", b01_init },
        { "b02_door_command_rx", b02_door_command_rx },
        { "b03_climate_light_command_rx", b03_climate_light_command_rx },
        { "b04_command_order_target", b04_command_order_target },
        { "b05_permission_rx", b05_permission_rx },
        { "b06_publish_states", b06_publish_states },
        { "b07_states_not_repeated", b07_states_not_repeated },
        { "b08_send_result_event", b08_send_result_event },
        { "b09_discrete_kept_in_order", b09_discrete_kept_in_order },
        { "b10_discrete_no_capacity", b10_discrete_no_capacity },
        { "b11_publish_invalid", b11_publish_invalid },
        { "b12_publish_in_callback", b12_publish_in_callback },
        { "b13_not_ready", b13_not_ready },
        { "b13_domain_reboot", b13_domain_reboot },
        { "load_doc_traffic", load_doc_traffic },
#ifdef BCM_NODE_TEST_ENTRY
        { "b14_entry", b14_entry },
#endif
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
