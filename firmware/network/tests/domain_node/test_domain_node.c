/* DOMAIN node on the host: real product sources (public entry -> DOMAIN
 * composition root -> NetUpper + C2 adapter + M04 + Query -> NetHalAdapter ->
 * RTD backend) linked with the independently written RTD mock SDK, which plays
 * the CAN bus and the UART-M line to the ESP32 and calls the FlexCAN / LPUART
 * callbacks like the IRQ. The DOMAIN C2 boundary is a fake: the DomainIf_*
 * functions below record what Network hands to DOMAIN, and the C2 TxPort the
 * node registers is called the way the DOMAIN application would.
 *
 * Expected values come from the contracts (docs/network/interface):
 *   - 03_COMMUNICATION/10_CAN_FD.md common header: BOOT_ID is the sending
 *     Network's, TX_SEQUENCE is kept per message, first 1, +1 per frame (a
 *     repetition included); TARGET_BOOT_ID is checked by DOMAIN Network on the
 *     last normal target state. DOMAIN_PERMISSION and VSS_WARNING_STATE:
 *     50 ms period / 150 ms receive timeout.
 *   - 04_COMMON/10_DATA_AND_ID_RULES.md: a repetition keeps the original
 *     observation and its AGE keeps growing; the DOMAIN_PERMISSION original
 *     Power life is 500 ms; a restriction is never deferred behind an older
 *     permission; the environment route is held until its transition is
 *     confirmed.
 *   - 03_COMMUNICATION/20_UART_M.md: M_DOMAIN_ALIVE 250 ms / 750 ms, its
 *     DOMAIN_STATE is the actual DOMAIN state; LINK_SEQUENCE per direction,
 *     50 ms frame assembly bound.
 * Board time (net_domain_board.h): the DOMAIN main loop has one ms function;
 * the ISR stamps a capture with the nowMs of the previous pass.
 * One case per process: Net_Init acts once per process. */
#include "network/net_node.h"
#include "network/app/net_if_entry.h"
#include "network/net_domain_c2.h"
#include "network/net_upper_can.h"
#include "network/net_upper_uart.h"
#include "network/net_hal_rtd.h"
#include "net_upper_private.h"
#include "net_domain_board.h"
#include "mock_rtd.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", case_name, __LINE__, #x); return 1; } } while (0)

static const char *case_name;

enum { BUS = 0, UART_M = 0 };
#define NODE_BOOT 0x444F4D31u
#define CIS_BOOT  0x43495331u
#define START_MS  1000u
#define WARN_BOOT 0x56535331u

/* ------------------------------------------------- fake DOMAIN boundary - */
static bool domain_initialized = true;
static DomainIf_TxPort_t port;
static unsigned set_port_calls;
static unsigned rear_calls, env_calls, cis_status_calls, bcm_status_calls, door_state_calls;
static unsigned result_calls, failure_calls, connection_calls, proximity_calls;
static CisRearState_t last_rear;
static CisCabinEnvironmentState_t last_env;
static DomainIf_CisStatusReport_t last_cis_status;
static DigitalKeyConnectionState_t last_connection;
static DeviceContextId_t last_connection_device;
static bool in_flush;
static unsigned calls_in_flush;

static void owner_call(void) { if (in_flush) ++calls_in_flush; }

bool DomainIf_IsInitialized(void) { return domain_initialized; }
DomainIf_Status_t DomainIf_SetRxObserver(const DomainIf_RxObserver_t *observer) { (void)observer; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_SetTxPort(const DomainIf_TxPort_t *p)
{
    ++set_port_calls;
    if (p != NULL) port = *p; else memset(&port, 0, sizeof(port));
    return DOMAIN_IF_OK;
}
DomainIf_Status_t DomainIf_RxBcmDoorState(const DoorState_t *s) { (void)s; owner_call(); ++door_state_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxBcmStatus(const DomainIf_BcmStatusReport_t *s) { (void)s; owner_call(); ++bcm_status_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxBcmCommandResult(const DomainIf_BcmCommandResult_t *r) { (void)r; owner_call(); ++result_calls; return DOMAIN_IF_OK; }
/* BCM result with a return value the case chooses (n18), bound by the test
 * hook below in place of the pinned function. */
static DomainIf_Status_t result_return = DOMAIN_IF_OK;
static DomainIf_Status_t result_rx(const DomainIf_BcmCommandResult_t *r) { (void)r; owner_call(); ++result_calls; return result_return; }
/* Not implemented by DOMAIN develop (no pinned function): bound by the test
 * hook below so that the per-field AGE of 0x210 / 0x220 is seen at C2. */
static unsigned climate_calls, light_calls;
static ClimateState_t last_climate;
static InteriorLightState_t last_light;
static DomainIf_Status_t climate_rx(const ClimateState_t *s) { owner_call(); last_climate = *s; ++climate_calls; return DOMAIN_IF_OK; }
static DomainIf_Status_t light_rx(const InteriorLightState_t *s) { owner_call(); last_light = *s; ++light_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxCisRear(const CisRearState_t *s) { owner_call(); last_rear = *s; ++rear_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxCisStatus(const DomainIf_CisStatusReport_t *s) { owner_call(); last_cis_status = *s; ++cis_status_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxCisCabinEnvironment(const CisCabinEnvironmentState_t *s) { owner_call(); last_env = *s; ++env_calls; return DOMAIN_IF_OK; }
static unsigned occupant_calls;
DomainIf_Status_t DomainIf_RxCisOccupant(const CisOccupantState_t *s) { (void)s; owner_call(); ++occupant_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *f) { (void)f; owner_call(); ++failure_calls; return DOMAIN_IF_OK; }
DomainIf_Status_t DomainIf_RxDigitalKeyConnection(DeviceContextId_t device, const DigitalKeyConnectionState_t *c)
{
    owner_call();
    last_connection_device = device;
    last_connection = *c;
    ++connection_calls;
    return DOMAIN_IF_OK;
}
DomainIf_Status_t DomainIf_RxProximity(DeviceContextId_t device, const ProximityInput_t *p) { (void)device; (void)p; owner_call(); ++proximity_calls; return DOMAIN_IF_OK; }
/* Query getters: this fake keeps no DOMAIN state. */
DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t at, DoorState_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t at, DomainIf_BcmStatusReport_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t at, CisRearState_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t at, DomainIf_CisStatusReport_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t at, CisCabinEnvironmentState_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t at, CisOccupantState_t *o) { (void)at; (void)o; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetRequestResult(DeviceContextId_t device, const RequestContext_t *target, uint32_t at,
    DomainIf_MobileRequestResult_t *o, bool *has)
{ (void)device; (void)target; (void)at; (void)o; (void)has; return DOMAIN_IF_NOT_AVAILABLE; }
DomainIf_Status_t DomainIf_QueryGetRecentRequestResults(DeviceContextId_t device, uint32_t at,
    DomainIf_MobileRequestResult_t *o, size_t capacity, size_t *count)
{ (void)device; (void)at; (void)o; (void)capacity; (void)count; return DOMAIN_IF_NOT_AVAILABLE; }

/* ------------------------------------------------------- board hooks ---- */
bool MockBoard_NodeConfig(NetNodeConfig *out);
bool MockBoard_DomainC2(NetDomainC2Api *api, NetDomainC2Config *config);
static unsigned node_config_calls, c2_hook_calls;
static bool use_board_hook, alive_hook;
static NetDomainC2AliveState alive_now;
static NetStatus alive_return = NET_OK;

static NetStatus alive_source(void *context, NetDomainC2AliveState *out)
{
    (void)context;
    if (alive_return != NET_OK) return alive_return;
    *out = alive_now;
    return NET_OK;
}
/* The status quality rule is a DOMAIN team decision (HOLD on the board); the
 * test binds one so that the status routes reach C2. */
static NetStatus status_quality(void *context, uint16_t id, const NetUpperCanMessage *m, DataQuality_t *q)
{
    (void)context; (void)id; (void)m;
    *q = DATA_QUALITY_OK;
    return NET_OK;
}

bool MockBoard_NodeConfig(NetNodeConfig *out)
{
    ++node_config_calls;
    out->boot_id = NODE_BOOT;
    return true;
}

bool MockBoard_DomainC2(NetDomainC2Api *api, NetDomainC2Config *config)
{
    ++c2_hook_calls;
    if (use_board_hook) return NetDomainBoard_C2Config(api, config);
    net_domain_c2_pinned_owner_api(api);
    api->RxDigitalKeyConnection = DomainIf_RxDigitalKeyConnection;
    api->RxProximity = DomainIf_RxProximity;
    api->RxBcmCommandResult = result_rx;
    api->RxBcmClimateState = climate_rx;
    api->RxBcmInteriorLightState = light_rx;
    config->value_quality = net_domain_c2_value_quality_provisional;
    config->permission_bundle = net_domain_c2_permission_bundle_provisional;
    config->status_quality = status_quality;
    config->command_accept_limit_ms = 200u;
    config->event_use_limit_ms = 500u;
    config->transmit_policy.bound = true;
    config->transmit_policy.maximum_age_ms = 65535u + 1000u;
    config->transmit_policy.expires_at_equality = true;
    config->failure_sink_ready = true;
    /* Values the node replaces: its own BOOT_ID, the received BCM BOOT_ID. */
    config->domain_boot = 0x0BADu;
    config->bcm_boot = 0x0BADu;
    if (alive_hook) config->alive_state = alive_source;
    /* cis_transition_barriers stays off here: the node turns the barriers on. */
    return true;
}

/* ------------------------------------------------- bus and main loop ---- */
#define CAN_LOG 512u
#define UART_LOG 64u
typedef struct { uint32_t at; NetUpperCanMessage m; } CanSeen;
typedef struct { uint32_t at; NetUpperUartMessage m; } UartSeen;
static CanSeen can_seen[CAN_LOG];
static UartSeen uart_seen[UART_LOG];
static unsigned can_count, can_bad, uart_count, uart_bad;
static uint32_t now_ms; /* the DOMAIN main-loop ms function */

/* The bus takes every frame the node offered (TX_COMPLETE from the IRQ); the
 * ESP32 end of UART-M takes the active transfer (TX complete from the IRQ). */
static void collect(void)
{
    MockCanFrame f;
    for (;;) {
        NetWireCanEnvelope e;
        NetUpperCanMessage m;
        if (MockRtd_CanTransmitNext(BUS, &f) < 0) break;
        memset(&e, 0, sizeof(e));
        e.standard_id = f.id;
        e.fdf = f.fd;
        e.brs = f.brs;
        e.dlc = f.dlc;
        e.length = f.length;
        memcpy(e.bytes, f.data, f.length);
        if (net_upper_can_decode(NET_WIRE_CANDIDATE_ONLY, &e, &m) != NET_WIRE_OK) { ++can_bad; continue; }
        if (can_count < CAN_LOG) { can_seen[can_count].at = now_ms; can_seen[can_count].m = m; }
        ++can_count;
    }
    for (;;) {
        NetWireUartFrame fr;
        NetUpperUartMessage u;
        uint32 left = MockRtd_UartTxRemaining(UART_M);
        if (left == 0u) break;
        memset(&fr, 0, sizeof(fr));
        if (left > sizeof(fr.bytes)) { ++uart_bad; (void)MockRtd_UartTransmitTake(UART_M, fr.bytes, (uint32)sizeof(fr.bytes)); continue; }
        fr.length = MockRtd_UartTransmitTake(UART_M, fr.bytes, (uint32)sizeof(fr.bytes));
        if (net_upper_uart_decode(NET_WIRE_CANDIDATE_ONLY, NET_WIRE_UART_M, NET_WIRE_DOWNLINK, &fr, &u) != NET_WIRE_OK) {
            ++uart_bad;
            continue;
        }
        if (uart_count < UART_LOG) { uart_seen[uart_count].at = now_ms; uart_seen[uart_count].m = u; }
        ++uart_count;
    }
}

/* One super-loop pass: Net_MainFunction(ms) once, then the line ends. */
static void pass(void) { Net_MainFunction(now_ms); collect(); }
/* The application ran after Net_MainFunction: send-only pass, same nowMs. */
static void flush(void) { in_flush = true; Net_TxFlush(now_ms); in_flush = false; collect(); }
/* Passes every millisecond up to and including until. */
static void run_to(uint32_t until) { while ((int32_t)(until - now_ms) > 0) { ++now_ms; pass(); } }

static NetNodeDiag diag(void)
{
    NetNodeDiag d;
    (void)NetNode_GetDiag(&d);
    return d;
}

static void board_reset(uint32_t ms)
{
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(false); /* the DOMAIN board time is the main-loop ms */
    now_ms = ms;
}

/* DOMAIN initialized its interface, then Net_Init, then the first pass. */
static bool start_at(uint32_t ms)
{
    board_reset(ms);
    Net_Init();
    if (diag().stage != NET_NODE_STAGE_READY) return false;
    pass();
    return true;
}
static bool start(void) { return start_at(START_MS); }

/* The bus delivers a frame: the mock calls the FlexCAN callback like the IRQ. */
static int inject(const NetUpperCanMessage *m)
{
    NetWireCanEnvelope e;
    MockCanFrame f;
    if (net_upper_can_encode(NET_WIRE_CANDIDATE_ONLY, m, &e) != NET_WIRE_OK) return -2;
    memset(&f, 0, sizeof(f));
    f.id = e.standard_id;
    f.fd = e.fdf;
    f.brs = e.brs;
    f.dlc = e.dlc;
    f.length = (uint8)e.length;
    memcpy(f.data, e.bytes, e.length);
    return MockRtd_CanInject(BUS, &f);
}

/* ------------------------------------------------------------ frames ---- */
static NetUpperCanMessage rear(uint32_t tx, uint32_t update, uint16_t age)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x0A0u;
    m.data.cis_rear.boot_id = CIS_BOOT;
    m.data.cis_rear.tx_sequence = tx;
    m.data.cis_rear.update_sequence = update;
    m.data.cis_rear.source_age_ms = age;
    m.data.cis_rear.rear_distance_deci_cm = 1234u;
    m.data.cis_rear.validity = 1u;
    return m;
}

static NetUpperCanMessage bcm_status(uint32_t boot, uint32_t tx)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x230u;
    m.data.bcm_status.boot_id = boot;
    m.data.bcm_status.tx_sequence = tx;
    m.data.bcm_status.update_sequence = tx;
    m.data.bcm_status.source_age_ms = 10u;
    return m;
}

static NetUpperCanMessage door_state(uint32_t boot, uint32_t tx)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x200u;
    m.data.bcm_door_state.boot_id = boot;
    m.data.bcm_door_state.tx_sequence = tx;
    m.data.bcm_door_state.update_sequence = tx;
    return m;
}

#define BCM_BOOT 0x42434D31u
static NetUpperCanMessage climate_state(uint32_t tx, uint16_t age)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x210u;
    m.data.bcm_climate_state.boot_id = BCM_BOOT;
    m.data.bcm_climate_state.tx_sequence = tx;
    m.data.bcm_climate_state.update_sequence = 7u;
    m.data.bcm_climate_state.source_age_ms = age;
    return m;
}

static NetUpperCanMessage light_state(uint32_t tx, uint16_t age)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x220u;
    m.data.bcm_light_state.boot_id = BCM_BOOT;
    m.data.bcm_light_state.tx_sequence = tx;
    m.data.bcm_light_state.update_sequence = 8u;
    m.data.bcm_light_state.source_age_ms = age;
    return m;
}

/* BCM_COMMAND_RESULT of a DOMAIN command (this node's BOOT_ID). */
static NetUpperCanMessage bcm_result(uint32_t tx, uint32_t command)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x140u;
    m.data.bcm_result.boot_id = BCM_BOOT;
    m.data.bcm_result.tx_sequence = tx;
    m.data.bcm_result.command_domain_boot = NODE_BOOT;
    m.data.bcm_result.command_id = command;
    m.data.bcm_result.command_kind = 1u;
    m.data.bcm_result.result = 2u;
    m.data.bcm_result.result_reason = 9u;
    return m;
}

/* CIS_ENVIRONMENT: temperature AGE 2200 (still inside the DOMAIN 2500 ms
 * TTL), humidity 100, illuminance NO_DATA with the saturated AGE. */
static NetUpperCanMessage environment(uint32_t tx)
{
    NetUpperCanMessage m;
    NetWireCisEnvironmentRaw *r = &m.data.cis_environment;
    memset(&m, 0, sizeof(m));
    m.id = 0x300u;
    r->boot_id = CIS_BOOT;
    r->tx_sequence = tx;
    r->temperature_centi_c = 2150;
    r->temperature_sequence = 5u + tx;
    r->temperature_age_ms = 2200u;
    r->temperature_validity = 1u;
    r->humidity_centi_percent = 4500u;
    r->humidity_sequence = 6u + tx;
    r->humidity_age_ms = 100u;
    r->humidity_validity = 1u;
    r->illuminance_lux = 0u;
    r->illuminance_sequence = 0u;
    r->illuminance_age_ms = 0xFFFFu;
    r->illuminance_validity = 0u;
    r->illuminance_reason = 6u; /* NO_DATA */
    return m;
}

static NetUpperCanMessage cis_status(uint32_t tx, uint16_t age)
{
    NetUpperCanMessage m;
    memset(&m, 0, sizeof(m));
    m.id = 0x320u;
    m.data.cis_status.boot_id = CIS_BOOT;
    m.data.cis_status.tx_sequence = tx;
    m.data.cis_status.update_sequence = 3u;
    m.data.cis_status.source_age_ms = age;
    return m;
}

static bool context_frame(uint16_t sequence, uint64_t session, NetWireUartFrame *out)
{
    NetUpperUartMessage m;
    memset(&m, 0, sizeof(m));
    m.link = NET_WIRE_UART_M;
    m.type = 0x01u;
    m.sequence = sequence;
    m.data.m_context.device_context_id = 7u;
    m.data.m_context.session_id = session;
    m.data.m_context.device_registration_state = 1u;
    m.data.m_context.bt_connection_state = 1u;
    m.data.m_context.app_active_state = 1u;
    m.data.m_context.proximity_validity = 1u;
    return net_upper_uart_encode(NET_WIRE_CANDIDATE_ONLY, &m, out) == NET_WIRE_OK;
}

static uint32 line_in(const uint8_t *bytes, size_t n) { return MockRtd_UartInject(UART_M, bytes, (uint32)n); }

/* -------------------------------------------------- C2 TxPort inputs ---- */
static DomainIf_CisPowerPermission_t power(bool allowed, DataQuality_t q, uint32_t evaluation, uint32_t age)
{
    DomainIf_CisPowerPermission_t p;
    memset(&p, 0, sizeof(p));
    p.vehiclePowerAllowed = allowed;
    p.quality = q;
    p.meta.quality = q;
    p.meta.updateSequence = evaluation;
    p.meta.ageMs = age;
    return p;
}
static DomainIf_Status_t tx_power(bool allowed, DataQuality_t q, uint32_t evaluation, uint32_t age)
{
    DomainIf_CisPowerPermission_t p = power(allowed, q, evaluation, age);
    return port.txCisPowerPermission(&p);
}

static DomainIf_WarningInputEvidence_t evidence(uint32_t boot, uint32_t sequence, uint32_t age)
{
    DomainIf_WarningInputEvidence_t e;
    memset(&e, 0, sizeof(e));
    e.quality = DATA_QUALITY_OK;
    e.reason = QUALITY_REASON_NONE;
    e.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    e.source.generation = boot;
    e.source.sequence = sequence;
    e.source.ageMs = age;
    return e;
}
static DomainIf_Status_t tx_warning(void)
{
    DomainIf_VssWarningState_t w;
    memset(&w, 0, sizeof(w));
    w.windowAntiPinchState = ANTIPINCH_STATE_CLEAR;
    w.antiPinchEvidence = evidence(WARN_BOOT, 11u, 120u);
    w.occupantHazardState = OCCUPANT_HAZARD_CLEAR;
    w.occupantEvidence = evidence(CIS_BOOT, 22u, 230u);
    w.rearObstacleState = REAR_OBSTACLE_STATE_CLEAR;
    w.rearEvidence = evidence(CIS_BOOT, 33u, 340u);
    w.rearDetectionActivation = REAR_DETECTION_ACTIVE;
    w.rearActivationMeta.quality = DATA_QUALITY_OK;
    w.rearActivationMeta.updateSequence = 44u;
    w.rearActivationMeta.ageMs = 55u;
    w.rearActivationReason = QUALITY_REASON_NONE;
    return port.txVssWarningState(&w);
}

static DomainIf_Status_t tx_door(uint32_t command)
{
    DomainIf_BcmDoorCommand_t c;
    memset(&c, 0, sizeof(c));
    c.context.commandId = command;
    c.context.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    c.target = DOOR_TARGET_UNLOCK;
    return port.txBcmDoorCommand(&c);
}

static DomainIf_Status_t tx_climate(uint32_t command)
{
    DomainIf_BcmClimateCommand_t c;
    memset(&c, 0, sizeof(c));
    c.context.commandId = command;
    c.context.origin = COMMAND_ORIGIN_CLIMATE_POLICY;
    c.fanTarget = FAN_LEVEL_LOW;
    c.thermalDirection = THERMAL_DIRECTION_COOL;
    c.thermalTargetLevelPercent = 50u;
    return port.txBcmClimateCommand(&c);
}

static DomainIf_Status_t tx_event(uint32_t occurrence)
{
    DomainIf_VssOneShotEvent_t e;
    memset(&e, 0, sizeof(e));
    e.event = VSS_EVENT_VEHICLE_WELCOME;
    e.occurrenceId = occurrence;
    e.meta.quality = DATA_QUALITY_OK;
    e.meta.ageMs = 10u;
    e.availability = FUNCTION_AVAILABILITY_AVAILABLE;
    return port.txVssOneShotEvent(&e);
}

/* ----------------------------------------------------- frame queries ---- */
static unsigned count_id(uint16_t id, unsigned from)
{
    unsigned i, n = 0u;
    for (i = from; i < can_count && i < CAN_LOG; ++i) if (can_seen[i].m.id == id) ++n;
    return n;
}
/* k-th frame of id at or after index from, NULL when there is none. */
static const CanSeen *nth_id(uint16_t id, unsigned from, unsigned k)
{
    unsigned i;
    for (i = from; i < can_count && i < CAN_LOG; ++i)
        if (can_seen[i].m.id == id && k-- == 0u) return &can_seen[i];
    return NULL;
}
static unsigned allows_from(unsigned from)
{
    unsigned i, n = 0u;
    for (i = from; i < can_count && i < CAN_LOG; ++i)
        if (can_seen[i].m.id == 0x080u && can_seen[i].m.data.domain_permission.vehicle_power_permission == 1u) ++n;
    return n;
}
static uint32_t tx_of(const CanSeen *s)
{
    uint32_t boot = 0u, sequence = 0u;
    (void)net_upper_can_identity(&s->m, &boot, &sequence);
    return sequence;
}
static uint32_t boot_of(const CanSeen *s)
{
    uint32_t boot = 0u, sequence = 0u;
    (void)net_upper_can_identity(&s->m, &boot, &sequence);
    return boot;
}
/* Every frame of id from index from on: TX_SEQUENCE first, first+1, ... */
static bool consecutive(uint16_t id, unsigned from, uint32_t first)
{
    unsigned k;
    const CanSeen *s;
    for (k = 0u; (s = nth_id(id, from, k)) != NULL; ++k)
        if (tx_of(s) != first + k || boot_of(s) != NODE_BOOT) return false;
    return k > 0u;
}

/* -------------------------------------------------------------- cases ---- */
/* Net_Init -> READY with the C2 TxPort registered. Before Net_Init nothing
 * runs; a second Net_Init changes nothing. The node sets the BOOT_ID, the
 * TX_SEQUENCE start, the M04 slots, the command target and the closed CIS
 * transition barriers whatever the hook wrote. No CAN frame before the DOMAIN
 * application submits one. */
static int n01_init(void)
{
    NetNodeDiag d;
    NetDomainC2 *a;
    board_reset(START_MS);
    Net_MainFunction(now_ms);
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_NONE && d.entry_init_result == NET_ENTRY_INIT_NONE && d.entry_main_calls == 1u);
    CHECK(set_port_calls == 0u && c2_hook_calls == 0u && NetNode_DomainC2() == NULL && Net_GetBootId() == 0u);

    Net_Init();
    d = diag();
    CHECK(d.entry_init_result == NET_ENTRY_INIT_OK && d.stage == NET_NODE_STAGE_READY && d.stage_status == NET_OK);
    CHECK(node_config_calls == 1u && c2_hook_calls == 1u && d.rtd.started);
    CHECK(set_port_calls == 1u && port.txCisPowerPermission != NULL && port.txBcmDoorCommand != NULL);
    CHECK(Net_GetBootId() == NODE_BOOT && NetNode_GetBootId() == NODE_BOOT);
    a = NetNode_DomainC2();
    CHECK(a != NULL && a->registered && a->config.domain_boot == NODE_BOOT && a->config.first_can_sequence == 1u);
    CHECK(a->config.cis_transition_barriers && a->config.bcm_target_from_rx && a->config.publisher != NULL);
    CHECK(!d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].open && !d.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].open);
    CHECK(d.permission_tx.period_ms == 50u && d.warning_tx.period_ms == 50u && d.alive_tx.period_ms == 250u);
    CHECK(d.permission_tx.boot_id == NODE_BOOT && d.warning_tx.boot_id == NODE_BOOT);
    CHECK(d.permission_tx.next_sequence == 1u && d.warning_tx.next_sequence == 1u && !d.permission_tx.has_source);
    CHECK(d.bcm_target_boot == 0u && !d.permission_valid);

    Net_Init();
    d = diag();
    CHECK(d.entry_init_calls == 2u && node_config_calls == 1u && c2_hook_calls == 1u && set_port_calls == 1u);
    pass();
    run_to(START_MS + 300u);
    CHECK(can_count == 0u && can_bad == 0u && uart_bad == 0u);
    CHECK(diag().steps == 301u);
    return 0;
}

/* DOMAIN interface not initialized before Net_Init: the C2 TxPort cannot be
 * registered, the node stops at the App Port stage and sends nothing. */
static int n02_init_order(void)
{
    NetNodeDiag d;
    domain_initialized = false;
    board_reset(START_MS);
    Net_Init();
    d = diag();
    CHECK(d.entry_init_result == NET_ENTRY_INIT_NODE_FAILED);
    CHECK(d.stage == NET_NODE_STAGE_APP_PORT && d.stage_status == NET_NOT_CONFIGURED);
    CHECK(c2_hook_calls == 1u && set_port_calls == 0u && port.txCisPowerPermission == NULL);
    CHECK(Net_GetBootId() == 0u && !d.rtd.started);
    pass();
    run_to(START_MS + 300u);
    flush();
    CHECK(can_count == 0u && uart_count == 0u && MockRtd_UartSends(UART_M) == 0u && diag().steps == 0u);
    /* Once per power cycle: initializing DOMAIN later does not revive it. */
    domain_initialized = true;
    Net_Init();
    d = diag();
    CHECK(d.stage == NET_NODE_STAGE_APP_PORT && set_port_calls == 0u && c2_hook_calls == 1u);
    return 0;
}

/* Received CAN frames reach the C2 Rx functions in the pass after the ISR
 * stored them; an exact duplicate does not; CIS_ENVIRONMENT and CIS_OCCUPANT
 * wait behind their closed transition barriers. */
static int n03_can_rx_to_c2(void)
{
    NetUpperCanMessage m;
    NetNodeDiag d;
    CHECK(start());
    m = rear(1u, 17u, 23u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(rear_calls == 1u && last_rear.rearDistanceMm == 1234u && last_rear.sourceGeneration == CIS_BOOT);
    CHECK(last_rear.distanceQuality.meta.updateSequence == 17u && last_rear.distanceQuality.meta.quality == DATA_QUALITY_OK);
    /* Stamped with the previous pass (1000), delivered at 1001. */
    CHECK(last_rear.distanceQuality.meta.ageMs == 23u + 1u);
    CHECK(inject(&m) >= 0); /* the same frame again */
    run_to(START_MS + 5u);
    CHECK(rear_calls == 1u);
    m = door_state(201u, 1u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 10u);
    CHECK(door_state_calls == 1u);
    m = environment(1u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 15u);
    d = diag();
    CHECK(env_calls == 0u && d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].held == 1u);
    return 0;
}

/* M_CONTEXT from the ESP32 reaches the C2 connection / proximity functions
 * and becomes the Query context. */
static int n04_uart_m_context(void)
{
    NetWireUartFrame fr;
    NetDomainC2 *a;
    CHECK(start());
    CHECK(context_frame(1u, 11u, &fr));
    CHECK(line_in(fr.bytes, fr.length) == fr.length);
    run_to(START_MS + 5u);
    CHECK(connection_calls == 1u && proximity_calls == 1u);
    CHECK(last_connection_device == 7u && last_connection.sessionId == 11u);
    a = NetNode_DomainC2();
    CHECK(a->service->context_seen && a->service->context.device_context_id == 7u && a->service->context.session_id == 11u);
    CHECK(a->query->context_known);
    return 0;
}

/* One ALLOW evaluation is sent every 50 ms with the same synthesized
 * UPDATE_SEQUENCE, TX_SEQUENCE +1 and its AGE growing, until the 500 ms
 * original Power life ends it. The first frame of the next evaluation takes
 * the next TX_SEQUENCE (the refused repetitions took none). */
static int n05_permission_50ms(void)
{
    const CanSeen *s;
    unsigned k;
    NetNodeDiag d;
    CHECK(start());
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 0u) == DOMAIN_IF_OK); /* evaluated now (1000) */
    run_to(START_MS + 600u);
    CHECK(count_id(0x080u, 0u) == 10u && can_bad == 0u);
    for (k = 0u; k < 10u; ++k) {
        const NetWireDomainPermissionRaw *r;
        s = nth_id(0x080u, 0u, k);
        CHECK(s != NULL);
        r = &s->m.data.domain_permission;
        CHECK(s->at == START_MS + 1u + 50u * k);
        CHECK(r->boot_id == NODE_BOOT && r->tx_sequence == k + 1u);
        CHECK(r->update_sequence == 1u && r->vehicle_power_permission == 1u && r->power_permission_quality == 0u);
        CHECK(r->source_age_ms == 1u + 50u * k && r->source_age_ms < 500u);
    }
    d = diag();
    CHECK(d.permission_tx.counters.periodic_sent == 9u && d.permission_tx.counters.not_sent >= 1u);
    CHECK(d.permission_tx.next_sequence == 11u);
    CHECK(tx_power(true, DATA_QUALITY_OK, 18u, 0u) == DOMAIN_IF_OK); /* new evaluation at 1600 */
    run_to(START_MS + 610u);
    s = nth_id(0x080u, 0u, 10u);
    CHECK(s != NULL && s->at == START_MS + 601u);
    CHECK(s->m.data.domain_permission.tx_sequence == 11u && s->m.data.domain_permission.update_sequence == 2u);
    CHECK(s->m.data.domain_permission.source_age_ms == 1u);
    return 0;
}

/* An ALLOW the bus has not taken yet is replaced by the withdrawal submitted
 * after it: the bus never carries that ALLOW. */
static int n06_permission_withdraw_replaces(void)
{
    const CanSeen *s;
    unsigned k;
    CHECK(start());
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 0u) == DOMAIN_IF_OK);
    CHECK(tx_power(false, DATA_QUALITY_NO_DATA, 17u, 0u) == DOMAIN_IF_OK); /* same pass, before any send */
    run_to(START_MS + 200u);
    CHECK(allows_from(0u) == 0u && count_id(0x080u, 0u) == 4u);
    for (k = 0u; k < 4u; ++k) {
        s = nth_id(0x080u, 0u, k);
        CHECK(s->at == START_MS + 1u + 50u * k && s->m.data.domain_permission.tx_sequence == k + 1u);
        CHECK(s->m.data.domain_permission.vehicle_power_permission == 255u);
        CHECK(s->m.data.domain_permission.power_permission_quality == 3u);
        CHECK(s->m.data.domain_permission.update_sequence == 2u); /* the ALLOW took 1, the withdrawal 2 */
    }
    CHECK(diag().permission_tx.counters.replaced == 1u && diag().permission_tx.counters.submitted == 2u);
    return 0;
}

/* ALLOW evaluated 100 ms ago. (a) Its withdrawal on the same evaluation basis
 * 100 ms later (AGE 200) is sent at once and no ALLOW follows. (b) A next
 * ALLOW, and its withdrawal only after 520 ms of AGE: the withdrawal is
 * refused (beyond the 500 ms life), and the ALLOW does not come back either:
 * the life ended it already. */
static int n06b_withdraw_aged_no_allow(void)
{
    const CanSeen *s;
    unsigned mark;
    CHECK(start());
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 100u) == DOMAIN_IF_OK);
    run_to(START_MS + 100u);
    CHECK(allows_from(0u) == 2u); /* 1001 (AGE 101), 1051 (151) */
    mark = can_count;
    CHECK(tx_power(false, DATA_QUALITY_NO_DATA, 17u, 200u) == DOMAIN_IF_OK);
    run_to(START_MS + 700u);
    CHECK(allows_from(mark) == 0u);
    s = nth_id(0x080u, mark, 0u);
    CHECK(s != NULL && s->at == START_MS + 101u);
    CHECK(s->m.data.domain_permission.vehicle_power_permission == 255u && s->m.data.domain_permission.source_age_ms == 201u);
    CHECK(s->m.data.domain_permission.update_sequence == 2u);

    run_to(START_MS + 1000u);
    CHECK(tx_power(true, DATA_QUALITY_OK, 18u, 100u) == DOMAIN_IF_OK); /* at 2000 */
    run_to(START_MS + 1420u);
    mark = can_count;
    CHECK(tx_power(false, DATA_QUALITY_NO_DATA, 18u, 520u) == DOMAIN_IF_REJECTED);
    CHECK(diag().permission_sequence.sequence == 3u); /* the refused withdrawal used no number */
    run_to(START_MS + 2000u);
    CHECK(allows_from(mark) == 0u && count_id(0x080u, mark) == 0u);
    return 0;
}

/* VSS_WARNING_STATE is sent every 50 ms with TX_SEQUENCE +1, the same lanes and
 * every lane AGE growing. */
static int n07_warning_50ms(void)
{
    unsigned k;
    CHECK(start());
    CHECK(tx_warning() == DOMAIN_IF_OK);
    run_to(START_MS + 300u);
    CHECK(count_id(0x0C0u, 0u) == 6u && can_bad == 0u);
    for (k = 0u; k < 6u; ++k) {
        const CanSeen *s = nth_id(0x0C0u, 0u, k);
        const NetWireVssWarningStateRaw *w = &s->m.data.vss_warning_state;
        uint16_t waited = (uint16_t)(1u + 50u * k);
        CHECK(s->at == START_MS + 1u + 50u * k && w->boot_id == NODE_BOOT && w->tx_sequence == k + 1u);
        CHECK(w->antipinch_source_sequence == 11u && w->occupant_hazard_source_sequence == 22u &&
              w->rear_obstacle_source_sequence == 33u && w->rear_activation_sequence == 44u);
        CHECK(w->antipinch_original_age_ms == 120u + waited && w->occupant_hazard_original_age_ms == 230u + waited);
        CHECK(w->rear_obstacle_original_age_ms == 340u + waited && w->rear_activation_age_ms == 55u + waited);
    }
    return 0;
}

/* TARGET_BOOT_ID: no BCM frame received yet -> no command (NOT_AVAILABLE);
 * BCM_STATUS of BOOT 201 -> 201; after the BCM restarts as 202 -> 202. */
static int n08_bcm_target_boot(void)
{
    NetUpperCanMessage m;
    const CanSeen *s;
    CHECK(start());
    CHECK(tx_door(77u) == DOMAIN_IF_NOT_AVAILABLE);
    run_to(START_MS + 10u);
    CHECK(count_id(0x100u, 0u) == 0u);
    m = bcm_status(201u, 1u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 20u);
    CHECK(bcm_status_calls == 1u && diag().bcm_target_boot == 201u);
    CHECK(tx_door(78u) == DOMAIN_IF_OK);
    run_to(START_MS + 30u);
    s = nth_id(0x100u, 0u, 0u);
    CHECK(s != NULL && s->m.data.bcm_door_command.target_boot_id == 201u && s->m.data.bcm_door_command.command_id == 78u);
    CHECK(s->m.data.bcm_door_command.boot_id == NODE_BOOT && s->m.data.bcm_door_command.tx_sequence == 1u);
    m = bcm_status(202u, 1u); /* the BCM restarted */
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 40u);
    CHECK(diag().bcm_target_boot == 202u);
    CHECK(tx_door(79u) == DOMAIN_IF_OK);
    run_to(START_MS + 50u);
    s = nth_id(0x100u, 0u, 1u);
    CHECK(s != NULL && s->m.data.bcm_door_command.target_boot_id == 202u && s->m.data.bcm_door_command.tx_sequence == 2u);
    CHECK(count_id(0x100u, 0u) == 2u);
    return 0;
}

/* M_DOMAIN_ALIVE without a DOMAIN source: INIT / NO_DATA / 0 / 0xFFFF and the
 * node BOOT_ID, from the first pass of the READY node, every 250 ms with
 * LINK_SEQUENCE +1. */
static int n09_alive_250ms(void)
{
    unsigned k;
    CHECK(start());
    run_to(START_MS + 1100u);
    CHECK(uart_count == 5u && uart_bad == 0u);
    for (k = 0u; k < 5u; ++k) {
        const NetWireMDomainAliveRaw *v = &uart_seen[k].m.data.m_domain_alive;
        CHECK(uart_seen[k].m.type == 0x02u && uart_seen[k].at == START_MS + 250u * k);
        CHECK(uart_seen[k].m.sequence == k + 1u);
        CHECK(v->domain_boot_id == NODE_BOOT && v->domain_state == 0u && v->value_quality == 3u);
        CHECK(v->update_sequence == 0u && v->source_age_ms == 0xFFFFu);
    }
    CHECK(diag().alive_submitted == 1u && diag().alive_tx.counters.periodic_sent == 4u);
    return 0;
}

/* With a DOMAIN source: the evaluation is sent as given and its AGE grows on
 * repetition; a changed state or evaluation number is sent in the pass it is
 * seen and restarts the period; an AGE that only grew is not a change; a
 * source that fails falls back to INIT / NO_DATA / 0 / 0xFFFF. */
static int n09b_alive_change(void)
{
    alive_hook = true;
    alive_now.domain_state = 1u; /* READY */
    alive_now.value_quality = 0u;
    alive_now.update_sequence = 5u;
    alive_now.source_age_ms = 10u;
    CHECK(start());
    CHECK(uart_count == 1u && uart_seen[0].at == START_MS);
    CHECK(uart_seen[0].m.data.m_domain_alive.domain_state == 1u && uart_seen[0].m.data.m_domain_alive.update_sequence == 5u);
    CHECK(uart_seen[0].m.data.m_domain_alive.source_age_ms == 10u);
    run_to(START_MS + 300u);
    CHECK(uart_count == 2u && uart_seen[1].at == START_MS + 250u && uart_seen[1].m.data.m_domain_alive.source_age_ms == 260u);
    run_to(START_MS + 309u);
    alive_now.domain_state = 2u; /* DEGRADED, new evaluation */
    alive_now.update_sequence = 6u;
    alive_now.source_age_ms = 0u;
    run_to(START_MS + 310u);
    CHECK(uart_count == 3u && uart_seen[2].at == START_MS + 310u);
    CHECK(uart_seen[2].m.data.m_domain_alive.domain_state == 2u && uart_seen[2].m.data.m_domain_alive.update_sequence == 6u);
    CHECK(uart_seen[2].m.data.m_domain_alive.source_age_ms == 0u && uart_seen[2].m.sequence == 3u);
    run_to(START_MS + 400u);
    alive_now.source_age_ms = 90u; /* only older */
    run_to(START_MS + 559u);
    CHECK(uart_count == 3u);
    run_to(START_MS + 560u);
    CHECK(uart_count == 4u && uart_seen[3].m.data.m_domain_alive.update_sequence == 6u);
    CHECK(uart_seen[3].m.data.m_domain_alive.source_age_ms == 250u);
    alive_return = NET_HOLD; /* the DOMAIN source fails */
    run_to(START_MS + 600u);
    CHECK(uart_count == 5u && uart_seen[4].at == START_MS + 561u);
    CHECK(uart_seen[4].m.data.m_domain_alive.domain_state == 0u && uart_seen[4].m.data.m_domain_alive.value_quality == 3u);
    CHECK(uart_seen[4].m.data.m_domain_alive.update_sequence == 0u && uart_seen[4].m.data.m_domain_alive.source_age_ms == 0xFFFFu);
    CHECK(uart_bad == 0u);
    return 0;
}

/* The application submits after Net_MainFunction; Net_TxFlush with the same
 * nowMs puts DOMAIN_PERMISSION (M04) and VSS_EVENT (C2 queue) on the bus in
 * that pass, calls no C2 Rx function although a frame waits, and the next
 * Net_MainFunction delivers it. */
static int n10_tx_flush_same_pass(void)
{
    NetUpperCanMessage m;
    unsigned before;
    CHECK(start());
    ++now_ms;
    pass(); /* 1001 */
    m = rear(1u, 17u, 23u);
    CHECK(inject(&m) >= 0);
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 0u) == DOMAIN_IF_OK);
    CHECK(tx_event(5u) == DOMAIN_IF_OK);
    before = can_count;
    flush();
    CHECK(calls_in_flush == 0u && rear_calls == 0u);
    CHECK(count_id(0x080u, before) == 1u && count_id(0x160u, before) == 1u);
    CHECK(nth_id(0x080u, before, 0u)->at == START_MS + 1u && nth_id(0x160u, before, 0u)->at == START_MS + 1u);
    CHECK(nth_id(0x080u, before, 0u)->m.data.domain_permission.source_age_ms == 0u);
    ++now_ms;
    pass();
    CHECK(rear_calls == 1u);
    return 0;
}

/* Every CAN message of the DOMAIN node keeps its own TX_SEQUENCE from 1: the
 * M04 messages (0x080, 0x0C0) and the C2 commands and events (0x100, 0x110,
 * 0x160) side by side. */
static int n11_tx_sequence_per_message(void)
{
    NetUpperCanMessage m;
    CHECK(start());
    m = bcm_status(201u, 1u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 0u) == DOMAIN_IF_OK);
    CHECK(tx_warning() == DOMAIN_IF_OK);
    CHECK(tx_door(77u) == DOMAIN_IF_OK);
    CHECK(tx_climate(78u) == DOMAIN_IF_OK);
    CHECK(tx_event(5u) == DOMAIN_IF_OK);
    run_to(START_MS + 60u);
    CHECK(tx_door(79u) == DOMAIN_IF_OK);
    CHECK(tx_event(6u) == DOMAIN_IF_OK);
    run_to(START_MS + 120u);
    CHECK(count_id(0x080u, 0u) == 3u && count_id(0x0C0u, 0u) == 3u);
    CHECK(count_id(0x100u, 0u) == 2u && count_id(0x110u, 0u) == 1u && count_id(0x160u, 0u) == 2u);
    CHECK(consecutive(0x080u, 0u, 1u) && consecutive(0x0C0u, 0u, 1u));
    CHECK(consecutive(0x100u, 0u, 1u) && consecutive(0x110u, 0u, 1u) && consecutive(0x160u, 0u, 1u));
    CHECK(can_bad == 0u);
    return 0;
}

/* One ms function on the board: the ISR stamps a capture with the nowMs of the
 * previous pass, so the delivered AGE adds the whole time since that pass,
 * never less than the real wait; across the 32-bit wrap of nowMs the axis
 * goes on. */
static int n12_entry_clock_axis(void)
{
    unsigned i;
    NetNodeDiag d;
    board_reset(0xFFFFFF00u);
    Net_Init();
    CHECK(diag().stage == NET_NODE_STAGE_READY);
    pass();
    for (i = 0u; i < 40u; ++i) {
        NetUpperCanMessage m = rear(i + 1u, 17u + i, 10u);
        uint32_t stamp = now_ms;
        CHECK(net_hal_rtd_last_now_ms32() == stamp);
        CHECK(inject(&m) >= 0); /* arrives about 2 ms after the pass at stamp */
        now_ms += 7u;
        pass();
        CHECK(rear_calls == i + 1u);
        /* 10 + (pass - stamp) = 17; the real wait was about 5 ms */
        CHECK(last_rear.distanceQuality.meta.ageMs == 17u && last_rear.distanceQuality.meta.ageMs >= 10u + 5u);
        now_ms += 3u;
        pass();
    }
    d = diag();
    CHECK((uint32_t)now_ms == 0xFFFFFF00u + 400u); /* wrapped */
    CHECK(d.entry_now_ms == (uint64_t)0xFFFFFF00u + 400u && d.entry_now_backward == 0u);
    CHECK(d.rtd.tick_future_clamped == 0u && net_hal_rtd_last_now_ms32() == now_ms);
    return 0;
}

/* Composite observations keep every field AGE: CIS_ENVIRONMENT with a
 * temperature AGE of 2200 ms (beyond the common 2000 ms bound) and a NO_DATA
 * illuminance, CIS_STATUS kept for 5000 ms, reach C2 with those AGEs; the
 * whole-message CIS_REAR with AGE 2100 does not. The environment barrier is
 * opened by the test for that CIS BOOT_ID: no transition is bound to the node
 * yet, so it stays closed otherwise. */
static int n13_composite_field_age(void)
{
    NetUpperCanMessage m;
    NetDomainC2 *a;
    CHECK(start());
    m = environment(1u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 5u);
    CHECK(env_calls == 0u && diag().barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].held == 1u);
    a = NetNode_DomainC2();
    a->barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].open = true; /* stands in for an applied transition */
    a->barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].id = CIS_BOOT;
    m = environment(2u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(env_calls == 1u && last_env.cabinTemperature == 2150 && last_env.cabinHumidity == 4500u);
    CHECK(last_env.temperatureQuality.meta.ageMs == 2201u && last_env.temperatureQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(last_env.humidityQuality.meta.ageMs == 101u && last_env.humidityQuality.meta.quality == DATA_QUALITY_OK);
    CHECK(last_env.illuminanceQuality.meta.ageMs == UINT32_MAX && last_env.illuminanceQuality.meta.quality == DATA_QUALITY_NO_DATA);
    m = cis_status(1u, 5000u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(cis_status_calls == 1u && last_cis_status.meta.ageMs == 5001u && last_cis_status.sourceGeneration == CIS_BOOT);
    m = rear(1u, 17u, 2100u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 20u);
    CHECK(rear_calls == 0u);
    m = rear(2u, 18u, 30u); /* the route itself works */
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 25u);
    CHECK(rear_calls == 1u);
    return 0;
}

/* UART-M receive policy: an M_CONTEXT in two pieces 40 ms apart is taken, one
 * in two pieces 60 ms apart is dropped (50 ms assembly bound), and the line
 * takes the next whole frame. */
static int n14_uart_m_receive_policy(void)
{
    NetWireUartFrame fr;
    const NetWireUartStream *line;
    size_t half;
    CHECK(start());
    line = &net_upper_receiver(NetNode_DomainC2()->service, NET_WIRE_UART_M - 1u)->stream;
    CHECK(context_frame(1u, 11u, &fr));
    half = fr.length / 2u;
    CHECK(line_in(fr.bytes, half) == half);
    run_to(START_MS + 40u);
    CHECK(connection_calls == 0u);
    CHECK(line_in(fr.bytes + half, fr.length - half) == fr.length - half);
    run_to(START_MS + 45u);
    CHECK(connection_calls == 1u && last_connection.sessionId == 11u && line->assembly_timeouts == 0u);

    CHECK(context_frame(2u, 12u, &fr));
    CHECK(line_in(fr.bytes, half) == half);
    run_to(START_MS + 105u);
    CHECK(line_in(fr.bytes + half, fr.length - half) == fr.length - half);
    run_to(START_MS + 110u);
    CHECK(connection_calls == 1u && line->assembly_timeouts == 1u);

    CHECK(context_frame(3u, 12u, &fr));
    CHECK(line_in(fr.bytes, fr.length) == fr.length);
    run_to(START_MS + 115u);
    CHECK(connection_calls == 2u && last_connection.sessionId == 12u);
    return 0;
}

/* The DOMAIN board hook: the pinned DOMAIN functions, the three provisional
 * rules (value quality, DOMAIN_PERMISSION bundle, status report quality), the
 * other six semantic callbacks NULL, the CIS transition barriers on. In the
 * node: CIS_REAR and CIS_STATUS reach C2, and an ALLOW goes out as 1 / OK with
 * the Operation fields UNKNOWN (255 / 0 / NO_DATA). */
static int n15_board_c2_hook(void)
{
    NetDomainC2Api api, pinned;
    NetDomainC2Config cfg;
    NetNodeConfig node;
    NetUpperCanMessage m;
    const CanSeen *s;
    memset(&api, 0xA5, sizeof(api));
    memset(&cfg, 0xA5, sizeof(cfg));
    CHECK(NetDomainBoard_C2Config(&api, &cfg));
    net_domain_c2_pinned_owner_api(&pinned);
    CHECK(memcmp(&api, &pinned, sizeof(api)) == 0);
    CHECK(cfg.value_quality == net_domain_c2_value_quality_provisional);
    CHECK(cfg.permission_bundle == net_domain_c2_permission_bundle_provisional);
    CHECK(cfg.status_quality == net_domain_c2_status_quality_provisional);
    CHECK(cfg.window_permission_bundle == NULL && cfg.window_alive_bundle == NULL);
    CHECK(cfg.window_event_projection == NULL && cfg.local_failure_reason == NULL);
    CHECK(cfg.select_query_plan == NULL && cfg.confirm_snapshot == NULL);
    CHECK(cfg.cis_transition_barriers && !cfg.window_transport_enabled && cfg.alive_state == NULL && cfg.publisher == NULL);
    CHECK(cfg.failure_sink_ready && cfg.command_accept_limit_ms == 200u && cfg.event_use_limit_ms == 500u);
    CHECK(!NetDomainBoard_C2Config(NULL, &cfg) && !NetDomainBoard_C2Config(&api, NULL));
    memset(&node, 0, sizeof(node));
    CHECK(NetDomainBoard_NodeConfig(&node) && node.boot_id != 0u && !NetDomainBoard_NodeConfig(NULL));
    CHECK(NetDomainBoard_TickToMs(0u) == 0u && NetDomainBoard_TickToMs(1234u) == 1234u &&
          NetDomainBoard_TickToMs(0xFFFFFFFFu) == 0xFFFFFFFFu);

    use_board_hook = true;
    CHECK(start());
    CHECK(c2_hook_calls == 1u && NetNode_DomainC2()->config.value_quality == net_domain_c2_value_quality_provisional);
    m = rear(1u, 17u, 23u);
    CHECK(inject(&m) >= 0);
    m = cis_status(1u, 100u);
    CHECK(inject(&m) >= 0);
    run_to(START_MS + 5u);
    CHECK(rear_calls == 1u && cis_status_calls == 1u && last_cis_status.meta.quality == DATA_QUALITY_OK);
    CHECK(tx_power(true, DATA_QUALITY_OK, 17u, 0u) == DOMAIN_IF_OK);
    run_to(START_MS + 10u);
    s = nth_id(0x080u, 0u, 0u);
    CHECK(s != NULL && s->m.data.domain_permission.vehicle_power_permission == 1u);
    CHECK(s->m.data.domain_permission.power_permission_quality == 0u && s->m.data.domain_permission.power_operation_state == 255u);
    CHECK(s->m.data.domain_permission.operation_allowed == 0u && s->m.data.domain_permission.operation_quality == 3u);
    return 0;
}

/* BCM door, climate and light states repeat an unchanged evaluation with its
 * original sequence and accumulated AGE (02_NODES/BCM/20_API.md: a repeat
 * keeps the original sequence and AGE). The DOMAIN node takes them with
 * per-field AGE: a frame whose AGE is past the common 2000 ms bound still
 * reaches C2 with meta.ageMs = wire AGE + local wait, and DOMAIN judges
 * freshness. */
static int n17_bcm_state_field_age(void)
{
    NetUpperCanMessage m;
    CHECK(start());
    m = door_state(BCM_BOOT, 1u);
    m.data.bcm_door_state.source_age_ms = 2500u;
    CHECK(inject(&m) >= 0);
    m = climate_state(1u, 3000u);
    CHECK(inject(&m) >= 0);
    m = light_state(1u, 60000u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(door_state_calls == 1u && climate_calls == 1u && light_calls == 1u);
    CHECK(last_climate.meta.ageMs == 3001u && last_climate.meta.updateSequence == 7u);
    CHECK(last_light.meta.ageMs == 60001u && last_light.meta.updateSequence == 8u);
    m = cis_status(1u, 2600u); /* the earlier per-field routes are unchanged */
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(cis_status_calls == 1u && last_cis_status.meta.ageMs == 2601u);
    return 0;
}

/* DOMAIN returns DOMAIN_IF_REJECTED for the late result of a command it has
 * no record of (ResultManager NOT_FOUND). The node drops that frame, counts it
 * in the adapter (rejected_frames), and the next BCM result still reaches
 * C2. */
static int n18_bcm_result_rejected(void)
{
    NetUpperCanMessage m;
    CHECK(start());
    result_return = DOMAIN_IF_REJECTED;
    m = bcm_result(1u, 901u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(result_calls == 1u && NetNode_DomainC2()->rejected_frames == 1u);
    result_return = DOMAIN_IF_OK;
    m = bcm_result(2u, 902u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(result_calls == 2u && NetNode_DomainC2()->rejected_frames == 1u);
    return 0;
}

/* The CIS transition binding DOMAIN hands to the board file reaches the
 * adapter through the board C2 hook. The node calls it in the main-loop pass
 * (adapter step, after the receive delivery), never in Net_TxFlush: the first
 * CIS_ENVIRONMENT of a CIS BOOT is held, epoch 1 is applied, the next frame
 * reaches C2. A binding without an Occupant function keeps CIS_OCCUPANT held. */
static unsigned transition_calls, transitions_in_flush;
static uint32_t transition_id;
static uint64_t transition_epoch;
static NetDomainC2TransitionStatus transition_result = NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED;
static NetDomainC2TransitionStatus env_transition(void *context, uint32_t id, uint64_t epoch, NetDomainC2TransitionReceipt *out)
{
    (void)context;
    ++transition_calls;
    if (in_flush) ++transitions_in_flush;
    transition_id = id;
    transition_epoch = epoch;
    if (transition_result == NET_DOMAIN_C2_TRANSITION_APPLIED) {
        out->id = id;
        out->epoch = epoch;
        out->revision = (uint32_t)epoch;
    }
    return transition_result;
}

static int n16_board_transition_binding(void)
{
    NetDomainC2TransitionBinding tb;
    NetDomainC2Config cfg;
    NetDomainC2Api api;
    NetUpperCanMessage m;
    NetNodeDiag d;
    memset(&tb, 0, sizeof(tb));
    tb.environment = env_transition;
    NetDomainBoard_SetTransitionBinding(&tb);
    CHECK(NetDomainBoard_C2Config(&api, &cfg) && cfg.transition.environment == env_transition && cfg.transition.occupant == NULL);
    NetDomainBoard_SetTransitionBinding(NULL);
    CHECK(NetDomainBoard_C2Config(&api, &cfg) && cfg.transition.environment == NULL);
    NetDomainBoard_SetTransitionBinding(&tb);
    use_board_hook = true;
    CHECK(start());
    CHECK(NetNode_DomainC2()->config.transition.environment == env_transition && NetNode_DomainC2()->config.cis_transition_barriers);
    m = environment(1u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    d = diag();
    CHECK(env_calls == 0u && transition_calls == 1u && transition_id == CIS_BOOT && transition_epoch == 1u);
    CHECK(d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].state == NET_DOMAIN_C2_BARRIER_PENDING);
    flush();
    CHECK(transition_calls == 1u && transitions_in_flush == 0u);
    transition_result = NET_DOMAIN_C2_TRANSITION_APPLIED;
    ++now_ms;
    pass();
    d = diag();
    CHECK(transition_calls == 2u && transition_epoch == 1u && transitions_in_flush == 0u);
    CHECK(d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].state == NET_DOMAIN_C2_BARRIER_OPEN);
    CHECK(d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].open && d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER].next_epoch == 2u);
    m = environment(2u);
    CHECK(inject(&m) >= 0);
    ++now_ms;
    pass();
    CHECK(env_calls == 1u && last_env.cabinTemperature == 2150 && transition_calls == 2u);
    /* No Occupant function: CIS_OCCUPANT is held and nothing is called. */
    memset(&m, 0, sizeof(m));
    m.id = 0x310u;
    m.data.cis_occupant.boot_id = CIS_BOOT;
    m.data.cis_occupant.tx_sequence = 1u;
    m.data.cis_occupant.pi_boot_id = 0x50493031u;
    m.data.cis_occupant.vision_sequence = 1u;
    m.data.cis_occupant.presence_validity = 1u;
    m.data.cis_occupant.count_validity = 1u;
    CHECK(inject(&m) >= 0);
    run_to(now_ms + 5u);
    d = diag();
    CHECK(occupant_calls == 0u && transition_calls == 2u && d.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].held == 1u);
    CHECK(d.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER].state == NET_DOMAIN_C2_BARRIER_CLOSED);
    return 0;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; int (*run)(void); } cases[] = {
        { "n01_init", n01_init }, { "n02_init_order", n02_init_order },
        { "n03_can_rx_to_c2", n03_can_rx_to_c2 }, { "n04_uart_m_context", n04_uart_m_context },
        { "n05_permission_50ms", n05_permission_50ms },
        { "n06_permission_withdraw_replaces", n06_permission_withdraw_replaces },
        { "n06b_withdraw_aged_no_allow", n06b_withdraw_aged_no_allow },
        { "n07_warning_50ms", n07_warning_50ms }, { "n08_bcm_target_boot", n08_bcm_target_boot },
        { "n09_alive_250ms", n09_alive_250ms }, { "n09b_alive_change", n09b_alive_change },
        { "n10_tx_flush_same_pass", n10_tx_flush_same_pass },
        { "n11_tx_sequence_per_message", n11_tx_sequence_per_message },
        { "n12_entry_clock_axis", n12_entry_clock_axis },
        { "n13_composite_field_age", n13_composite_field_age },
        { "n14_uart_m_receive_policy", n14_uart_m_receive_policy },
        { "n16_board_transition_binding", n16_board_transition_binding },
        { "n17_bcm_state_field_age", n17_bcm_state_field_age },
        { "n18_bcm_result_rejected", n18_bcm_result_rejected },
        { "n15_board_c2_hook", n15_board_c2_hook }
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
