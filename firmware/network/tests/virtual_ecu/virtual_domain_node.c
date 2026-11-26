#if defined(__MINGW32__)
#define __USE_MINGW_ANSI_STDIO 1
#endif
/* HAL-less DOMAIN node worker: the DOMAIN node composition root
 * (nodes/domain/src/node/net_node_domain.c: NetNode_Init / NetNode_Step, its M04 publish
 * table, receive policies and C2 adapter settings), the DOMAIN board file
 * (nodes/domain/board/s32k344_domain/net_domain_board.c) and the actual DOMAIN runtime
 * (pinned DOMAIN sources, compiled by path) run on the Host virtual backend.
 * Only the RTD backend functions the root calls (net_hal_rtd.h) are replaced
 * below: the virtual backend is the HAL and captures frames and bytes
 * directly. No SDK header, no mock SDK.
 *
 * TEST ONLY parts:
 *   - the C2 hook: the board settings plus observation wrappers around five
 *     DOMAIN Rx functions and a test Power permission bundle;
 *   - the CIS transition binding (nodes/domain/tests/product/domain_transition_binding.c),
 *     handed to the board file before NetNode_Init as the DOMAIN integration
 *     would do;
 *   - the driver commands: the DOMAIN application does not call its TxPort
 *     yet, so this worker calls DomainIf_Tx* on the driver's request.
 * Line protocol on stdin, JSON lines on stdout, "done" per command (as
 * virtual_node.c). */
#include "network/net_node.h"
#include "network/net_hal_rtd.h"
#include "network/net_virtual.h"
#include "net_domain_board.h"
#include "domain_runtime.h"
#include "domain_transition_binding.h"
#include "virtual_cli.h"

#if NETWORK_UPPER_NODE_PROFILE != NET_UPPER_PROFILE_DOMAIN
#error "virtual_domain_node.c is the DOMAIN node worker"
#endif
#define NODE_BOOT 301u
#define PORT_EPOCH 3u

static NetVirtualBackend backend;
static DomainRuntime_t runtime;
static NetDomainC2Api pinned;          /* the pinned DOMAIN functions behind the wrappers */
static uint8_t unknown_power;          /* test bundle value for a non-OK quality: 0 or 255 */

/* ------------------------------------- RTD backend replacement (Host) --- */
static NetStatus wire_output(void *ctx, const NetUpperPacket *p, uint64_t at)
{
    (void)ctx;
    vcli_wire("DOMAIN", p, at);
    return NET_OK;
}

/* One port epoch for CAN and UART-M (the virtual backend keeps one per link,
 * set from the same value). */
NetStatus net_hal_rtd_config(NetHalAdapter *adapter, const NetHalRtdConfig *in, NetHalConfig *out)
{
    NetStatus s;
    if (adapter == NULL || in == NULL || out == NULL) return NET_INVALID_ARGUMENT;
    if (in->can_port_epoch == 0u || in->can_port_epoch != in->uart_m_port_epoch) return NET_INVALID_ARGUMENT;
    s = net_virtual_init(&backend, adapter, NET_HAL_LINK_BIT(NET_WIRE_CAN) | NET_HAL_LINK_BIT(NET_WIRE_UART_M),
                         in->can_port_epoch, 1000u, wire_output, NULL, NULL);
    if (s != NET_OK) return s;
    *out = net_virtual_hal_config(&backend);
    return NET_OK;
}
NetUpperPort net_hal_rtd_port(NetHalAdapter *adapter) { return net_hal_adapter_port(adapter); }
/* The backend links are connected at init; captures reach the adapter when
 * the frame arrives (no ISR ring), there is no TX mailbox to supervise. */
NetStatus net_hal_rtd_start(void) { return backend.initialized ? NET_OK : NET_NOT_CONFIGURED; }
uint32_t net_hal_rtd_drain(void) { return 0u; }
void net_hal_rtd_service(void) { }
NetStatus net_hal_rtd_diag(NetHalRtdDiag *out)
{
    if (out == NULL) return NET_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->started = backend.initialized;
    return NET_OK;
}
/* No public entry (Net_Init / Net_MainFunction) in this worker. */
void net_entry_diag(NetNodeDiag *out) { (void)out; }

static uint32_t now32(void) { return (uint32_t)(backend.initialized ? backend.now_ms : 1000u); }

/* --------------------------------------------------- DOMAIN runtime ---- */
static bool owner_clock(void *context, uint32_t *out)
{
    (void)context;
    *out = now32();
    return true;
}

/* ------------------------------------------------ C2 hook (test only) --- */
bool VirtualDomain_C2Config(NetDomainC2Api *api, NetDomainC2Config *config);

static void quality_json(const char *name, const ValueQuality_t *q)
{
    printf(",\"%s\":{\"validity\":%u,\"quality\":%u,\"sequence\":%u,\"age\":%u}", name, (unsigned)q->validity,
           (unsigned)q->meta.quality, (unsigned)q->meta.updateSequence, (unsigned)q->meta.ageMs);
}

static DomainIf_Status_t rx_rear(const CisRearState_t *v)
{
    DomainIf_Status_t s = pinned.RxCisRear(v);
    printf("{\"event\":\"c2_rx\",\"node\":\"DOMAIN\",\"call\":\"RxCisRear\",\"status\":%u,\"mm\":%u,\"source\":%u",
           (unsigned)s, (unsigned)v->rearDistanceMm, (unsigned)v->sourceGeneration);
    quality_json("distance", &v->distanceQuality);
    puts("}");
    return s;
}
static DomainIf_Status_t rx_environment(const CisCabinEnvironmentState_t *v)
{
    DomainIf_Status_t s = pinned.RxCisCabinEnvironment(v);
    printf("{\"event\":\"c2_rx\",\"node\":\"DOMAIN\",\"call\":\"RxCisCabinEnvironment\",\"status\":%u,"
           "\"temperature\":%d,\"humidity\":%u,\"lux\":%u",
           (unsigned)s, (int)v->cabinTemperature, (unsigned)v->cabinHumidity, (unsigned)v->cabinIlluminance);
    quality_json("temperature_q", &v->temperatureQuality);
    puts("}");
    return s;
}
static DomainIf_Status_t rx_occupant(const CisOccupantState_t *v)
{
    DomainIf_Status_t s = pinned.RxCisOccupant(v);
    printf("{\"event\":\"c2_rx\",\"node\":\"DOMAIN\",\"call\":\"RxCisOccupant\",\"status\":%u,\"count\":%u,"
           "\"generation\":%u",
           (unsigned)s, (unsigned)v->occupantCount, (unsigned)v->sourceGeneration);
    quality_json("presence", &v->presenceQuality);
    puts("}");
    return s;
}
static DomainIf_Status_t rx_door_state(const DoorState_t *v)
{
    DomainIf_Status_t s = pinned.RxBcmDoorState(v);
    printf("{\"event\":\"c2_rx\",\"node\":\"DOMAIN\",\"call\":\"RxBcmDoorState\",\"status\":%u}\n", (unsigned)s);
    return s;
}
static DomainIf_Status_t rx_command_result(const DomainIf_BcmCommandResult_t *v)
{
    DomainIf_Status_t s = pinned.RxBcmCommandResult(v);
    printf("{\"event\":\"c2_rx\",\"node\":\"DOMAIN\",\"call\":\"RxBcmCommandResult\",\"status\":%u,\"command\":%u,"
           "\"kind\":%u,\"result\":%u,\"reason\":%u,\"confirmation\":%u}\n",
           (unsigned)s, (unsigned)v->commandId, (unsigned)v->kind, (unsigned)v->result, (unsigned)v->reason,
           (unsigned)v->confirmation);
    return s;
}

/* Power / Operation bundle of the test: the provisional rule of the board for
 * an OK quality (unchanged output); for a non-OK quality the power value is
 * the one the driver chose (0 or 255). The adapter sends UNKNOWN 255 for both
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md DOMAIN_PERMISSION). */
static NetStatus test_permission_bundle(void *context, const DomainIf_CisPowerPermission_t *in,
                                        NetWireDomainPermissionRaw *out)
{
    NetStatus s = net_domain_c2_permission_bundle_provisional(context, in, out);
    if (s == NET_OK && in->quality != DATA_QUALITY_OK) out->vehicle_power_permission = unknown_power;
    return s;
}

/* The board settings (NetDomainBoard_C2Config: pinned API, the two provisional
 * rules, limits, the transition binding handed over), then the test parts. */
bool VirtualDomain_C2Config(NetDomainC2Api *api, NetDomainC2Config *config)
{
    if (!NetDomainBoard_C2Config(api, config)) return false;
    pinned = *api;
    api->RxCisRear = rx_rear;
    api->RxCisCabinEnvironment = rx_environment;
    api->RxCisOccupant = rx_occupant;
    api->RxBcmDoorState = rx_door_state;
    api->RxBcmCommandResult = rx_command_result;
    config->permission_bundle = test_permission_bundle;
    return true;
}

/* --------------------------------------------------------- composition --- */
static int initialize(void)
{
    DomainRuntime_Config_t rc;
    NetDomainC2TransitionBinding binding;
    NetNodeConfig cfg;
    NetNodeDiag d;
    NetDomainC2 *c2;

    /* DOMAIN first (managers, C2 core, bindings), with the CIS Occupant on. */
    memset(&rc, 0, sizeof(rc));
    rc.timeProvider = owner_clock;
    rc.firstCommandId = 1u;
    rc.resultPolicy.firstResponseTimeoutEnabled = true;
    rc.resultPolicy.firstResponseTimeoutMs = 100u;
    rc.resultPolicy.finalResultTimeoutEnabled = true;
    rc.resultPolicy.finalResultTimeoutMs = 1000u;
    rc.resultPolicy.doorEvidenceTtlEnabled = true;
    rc.resultPolicy.doorEvidenceTtlMs = 500u;
    rc.permissionPolicy.reevaluationPeriodMs = 200u;
    rc.permissionPolicy.sourceValidityMs = 500u;
    rc.cisOccupantEnabled = true;
    rc.cisOccupantPolicy.presenceTtlMs = 500u;
    rc.cisOccupantPolicy.countTtlMs = 500u;
    rc.stateQueryPolicy.bcmDoorTtlMs = 500u;
    rc.stateQueryPolicy.bcmStatusTtlMs = 500u;
    rc.stateQueryPolicy.cisRearTtlMs = 500u;
    rc.stateQueryPolicy.cisStatusTtlMs = 500u;
    if (DomainRuntime_Init(&runtime, &rc) != DOMAIN_RUNTIME_OK || !DomainRuntime_IsReady(&runtime)) return 2;
    binding = domain_transition_binding(&runtime);
    NetDomainBoard_SetTransitionBinding(&binding);

    /* Then Network: the DOMAIN node root. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.boot_id = NODE_BOOT;
    cfg.local_epoch = 15u;
    cfg.can_port_epoch = PORT_EPOCH;
    cfg.uart_m_port_epoch = PORT_EPOCH;
    if (NetNode_Init(&cfg) != NET_OK) return 3;
    if (NetNode_GetDiag(&d) != NET_OK || d.stage != NET_NODE_STAGE_READY) return 4;
    c2 = NetNode_DomainC2();
    if (c2 == NULL) return 5;
    printf("{\"event\":\"ready\",\"node\":\"DOMAIN\",\"profile\":%u,\"boot\":%u,\"stage\":%u,\"registered\":%s,"
           "\"barriers\":%s,\"transition_bound\":%s,\"occupant_enabled\":%s}\n",
           (unsigned)NETWORK_UPPER_NODE_PROFILE, (unsigned)NetNode_GetBootId(), (unsigned)d.stage,
           c2->registered ? "true" : "false", c2->config.cis_transition_barriers ? "true" : "false",
           (c2->config.transition.environment != NULL && c2->config.transition.occupant != NULL) ? "true" : "false",
           runtime.cisOccupantEnabled ? "true" : "false");
    return 0;
}

/* ------------------------------------------------- driver (test only) --- */
static NetStatus tx_line(const char *what, DomainIf_Status_t s)
{
    printf("{\"event\":\"c2_tx\",\"node\":\"DOMAIN\",\"call\":\"%s\",\"status\":%u,\"at\":%llu}\n", what,
           (unsigned)s, (unsigned long long)backend.now_ms);
    return s == DOMAIN_IF_OK ? NET_OK : NET_REJECTED;
}

static bool quality_of(const char *text, DataQuality_t *out)
{
    if (strcmp(text, "ok") == 0) *out = DATA_QUALITY_OK;
    else if (strcmp(text, "stale") == 0) *out = DATA_QUALITY_STALE;
    else if (strcmp(text, "invalid") == 0) *out = DATA_QUALITY_INVALID;
    else if (strcmp(text, "nodata") == 0) *out = DATA_QUALITY_NO_DATA;
    else return false;
    return true;
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

static void barrier_json(const char *name, const NetDomainC2Barrier *b)
{
    printf(",\"%s\":{\"state\":%u,\"id\":%u,\"held\":%u,\"next_epoch\":%llu,\"revision\":%u,\"calls\":%u,"
           "\"applied\":%u,\"last_result\":%u}",
           name, (unsigned)b->state, (unsigned)b->id, (unsigned)b->held, (unsigned long long)b->next_epoch,
           (unsigned)b->revision, (unsigned)b->calls, (unsigned)b->applied, (unsigned)b->last_result);
}

static NetStatus observe(void)
{
    NetNodeDiag d;
    if (NetNode_GetDiag(&d) != NET_OK) return NET_INVALID_ARGUMENT;
    printf("{\"event\":\"observation\",\"node\":\"DOMAIN\",\"now\":%llu,\"stage\":%u,\"rx_delivered\":%u,"
           "\"rx_held\":%u,\"tx_submitted\":%u,\"tx_succeeded\":%u,\"bcm_target_boot\":%u,"
           "\"permission\":{\"sequence\":%u,\"domain_sequence\":%u,\"power\":%u,\"advanced\":%u,\"kept\":%u}",
           (unsigned long long)backend.now_ms, (unsigned)d.stage, d.upper.rx_delivered, d.upper.rx_held,
           d.upper.tx_submitted, d.upper.tx_succeeded, (unsigned)d.bcm_target_boot,
           (unsigned)d.permission_sequence.sequence, (unsigned)d.permission_sequence.domain_sequence,
           (unsigned)d.permission_sequence.power, (unsigned)d.permission_sequence.advanced,
           (unsigned)d.permission_sequence.kept);
    barrier_json("environment_barrier", &d.barrier[NET_DOMAIN_C2_ENVIRONMENT_BARRIER]);
    barrier_json("occupant_barrier", &d.barrier[NET_DOMAIN_C2_OCCUPANT_BARRIER]);
    printf(",\"alive_submitted\":%u}\n", (unsigned)d.alive_submitted);
    return NET_OK;
}

static NetStatus query(const char *what)
{
    DomainIf_Status_t s;
    if (strcmp(what, "rear") == 0) {
        CisRearState_t q;
        memset(&q, 0, sizeof(q));
        s = DomainIf_QueryGetCisRear(now32(), &q);
        printf("{\"event\":\"c2_query\",\"node\":\"DOMAIN\",\"call\":\"QueryGetCisRear\",\"status\":%u,\"mm\":%u,"
               "\"source\":%u",
               (unsigned)s, (unsigned)q.rearDistanceMm, (unsigned)q.sourceGeneration);
        quality_json("distance", &q.distanceQuality);
        puts("}");
    } else if (strcmp(what, "environment") == 0) {
        CisCabinEnvironmentState_t q;
        DomainCisEnvironmentPriv_Snapshot_t snap;
        memset(&q, 0, sizeof(q));
        memset(&snap, 0, sizeof(snap));
        s = DomainIf_QueryGetCisEnvironment(now32(), &q);
        (void)DomainCisEnvironmentPriv_GetSnapshot(DomainRuntime_GetCisEnvironmentPrivate(&runtime), &snap);
        printf("{\"event\":\"c2_query\",\"node\":\"DOMAIN\",\"call\":\"QueryGetCisEnvironment\",\"status\":%u,"
               "\"temperature\":%d,\"humidity\":%u,\"lux\":%u",
               (unsigned)s, (int)q.cabinTemperature, (unsigned)q.cabinHumidity, (unsigned)q.cabinIlluminance);
        quality_json("temperature_q", &q.temperatureQuality);
        printf(",\"applied\":%s,\"admitted\":%u,\"epoch\":%llu,\"revision\":%u}\n",
               snap.hasAppliedTransition ? "true" : "false", (unsigned)snap.currentTransition.admittedCisBootId,
               (unsigned long long)snap.currentTransition.transitionEpoch, (unsigned)snap.domainBarrierRevision);
    } else if (strcmp(what, "occupant") == 0) {
        CisOccupantState_t q;
        DomainCisOccupantPriv_Snapshot_t snap;
        memset(&q, 0, sizeof(q));
        memset(&snap, 0, sizeof(snap));
        s = DomainIf_QueryGetCisOccupant(now32(), &q);
        (void)DomainCisOccupantPriv_GetSnapshot(DomainRuntime_GetCisOccupantPrivate(&runtime), &snap);
        printf("{\"event\":\"c2_query\",\"node\":\"DOMAIN\",\"call\":\"QueryGetCisOccupant\",\"status\":%u,"
               "\"count\":%u,\"generation\":%u",
               (unsigned)s, (unsigned)q.occupantCount, (unsigned)q.sourceGeneration);
        quality_json("presence", &q.presenceQuality);
        printf(",\"applied\":%s,\"admitted\":%u,\"epoch\":%llu,\"revision\":%u}\n",
               snap.hasAppliedTransition ? "true" : "false",
               (unsigned)snap.currentTransition.admittedSourceGeneration,
               (unsigned long long)snap.currentTransition.transitionEpoch, (unsigned)snap.domainBarrierRevision);
    } else {
        return NET_UNSUPPORTED;
    }
    return NET_OK;
}

static NetStatus driver(const char *line)
{
    unsigned a = 0u, b = 0u, c = 0u;
    char text[16];
    if (sscanf(line, "permission %u %15s %u %u", &a, text, &b, &c) == 4) {
        DomainIf_CisPowerPermission_t p;
        DataQuality_t q;
        if (a > 1u || !quality_of(text, &q)) return NET_INVALID_ARGUMENT;
        memset(&p, 0, sizeof(p));
        p.vehiclePowerAllowed = a == 1u;
        p.quality = q;
        p.meta.quality = q;
        p.meta.updateSequence = b;
        p.meta.ageMs = c;
        return tx_line("TxCisPowerPermission", DomainIf_TxCisPowerPermission(&p));
    }
    if (sscanf(line, "bundle_unknown %u", &a) == 1) {
        if (a != 0u && a != 255u) return NET_INVALID_ARGUMENT;
        unknown_power = (uint8_t)a;
        return NET_OK;
    }
    if (sscanf(line, "warning %u %u", &a, &b) == 2) {
        DomainIf_VssWarningState_t w;
        memset(&w, 0, sizeof(w));
        w.windowAntiPinchState = ANTIPINCH_STATE_CLEAR;
        w.antiPinchEvidence = evidence(501u, 11u, 120u);
        w.occupantHazardState = OCCUPANT_HAZARD_CLEAR;
        w.occupantEvidence = evidence(401u, 22u, 230u);
        w.rearObstacleState = b != 0u ? REAR_OBSTACLE_STATE_CAUTION : REAR_OBSTACLE_STATE_CLEAR;
        w.rearEvidence = evidence(401u, a, 40u);
        w.rearDetectionActivation = REAR_DETECTION_ACTIVE;
        w.rearActivationMeta.quality = DATA_QUALITY_OK;
        w.rearActivationMeta.updateSequence = a;
        w.rearActivationMeta.ageMs = 5u;
        w.rearActivationReason = QUALITY_REASON_NONE;
        return tx_line("TxVssWarningState", DomainIf_TxVssWarningState(&w));
    }
    if (sscanf(line, "door_command %u", &a) == 1) {
        DomainIf_BcmDoorCommand_t cmd;
        NetNodeDiag d;
        (void)NetNode_GetDiag(&d);
        memset(&cmd, 0, sizeof(cmd));
        cmd.context.commandId = a;
        cmd.context.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
        cmd.context.meta.quality = DATA_QUALITY_OK;
        /* A mobile request command carries its request (DOMAIN core check). */
        cmd.context.hasRequestContext = true;
        cmd.context.requestDeviceContextId = 7u;
        cmd.context.requestContext.sessionId = 77u;
        cmd.context.requestContext.requestId = a;
        cmd.target = DOOR_TARGET_UNLOCK;
        printf("{\"event\":\"c2_target\",\"node\":\"DOMAIN\",\"bcm_target_boot\":%u}\n", (unsigned)d.bcm_target_boot);
        return tx_line("TxBcmDoorCommand", DomainIf_TxBcmDoorCommand(&cmd));
    }
    if (sscanf(line, "query %15s", text) == 1) return query(text);
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
        NetNode_Step();
        return NET_OK;
    }
    if (strcmp(line, "step") == 0) { NetNode_Step(); return NET_OK; }
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
        NetNode_Step();
        return NET_OK;
    }
    if (strcmp(line, "observe") == 0) return observe();
    return driver(line);
}

int main(int argc, char **argv)
{
    char line[1400];
    int rc;
    (void)argc;
    (void)argv; /* the runner passes the role name; this worker has one role */
    rc = initialize();
    if (rc != 0) {
        printf("{\"event\":\"init_failed\",\"node\":\"DOMAIN\",\"code\":%d}\n", rc);
        return rc;
    }
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t n = strlen(line);
        NetStatus status;
        while (n > 0u && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) line[--n] = 0;
        if (strcmp(line, "quit") == 0) return 0;
        status = command(line);
        printf("{\"event\":\"done\",\"node\":\"DOMAIN\",\"status\":\"%s\"}\n", vcli_status(status));
        fflush(stdout);
    }
    return ferror(stdin) ? 10 : 0;
}
