/* Network half of the VSS integration harness (see vss_integ_bus.h): DOMAIN
 * frames built with the repository codec, the CAN bus played by the mock SDK,
 * VSS_STATUS frames decoded with the codec, the Network diagnostics and the
 * board hooks of time base A (src/platform/rtd/net_board_cfg.h) that the VSS
 * firmware does not provide itself. Network headers only: never include a
 * VSS header here. */
#include "vss_integ_bus.h"
#include "network/net_node.h"
#include "network/net_wire_can.h"
#include "network/net_wire_can_nodes.h"
#include "network/app/net_if_vss.h"
#include "vss_integ_layout.h"
#include "mock_rtd.h"
#include <stddef.h>
#include <string.h>
#include <time.h>

enum { BUS = 0 };

static bool in_isr;

/* ---------------------------------------------------- board hooks ---- */
/* NET_BOARD_NODE_CONFIG_HOOK: VSS has no node configuration of its own yet
 * (BOOT_ID source NVM / noinit counter is open); the bench gives a constant. */
bool VssInteg_NodeConfig(NetNodeConfig *out);
/* NET_BOARD_US_HOOK: host wall clock, for the reference value main_max_us. */
uint32_t VssInteg_Us(void);

bool VssInteg_NodeConfig(NetNodeConfig *out)
{
    IntegVss_OnNodeConfig();
    out->boot_id = INTEG_NODE_BOOT;
    return true;
}

uint32_t VssInteg_Us(void)
{
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return 0u;
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u);
}

/* ------------------------------------------------------------ bus ---- */
bool IntegBus_InIsr(void) { return in_isr; }

void IntegBus_Reset(void)
{
    MockRtd_Reset();
    MockRtd_SetSystemTimerEnabled(false); /* time base A: OsIf is not used */
    in_isr = false;
}

static bool inject_frame(const NetWireCanEnvelope *e, bool fd, bool brs, uint8_t dlc, size_t length)
{
    MockCanFrame f;
    int mb;
    memset(&f, 0, sizeof(f));
    f.id = e->standard_id;
    f.fd = fd;
    f.brs = brs;
    f.dlc = dlc;
    f.length = (uint8)length;
    memcpy(f.data, e->bytes, length);
    in_isr = true;
    mb = MockRtd_CanInject(BUS, &f);
    in_isr = false;
    return mb >= 0;
}

static bool inject(const NetWireCanEnvelope *e) { return inject_frame(e, e->fdf, e->brs, e->dlc, e->length); }

static NetWireVssEventRaw event_raw(const IntegEvent *e)
{
    NetWireVssEventRaw r;
    memset(&r, 0, sizeof(r));
    r.boot_id = e->boot_id;
    r.tx_sequence = e->tx_sequence;
    r.event_kind = e->event_kind;
    r.occurrence_id = e->occurrence_id;
    r.original_age_ms = e->original_age_ms;
    r.use_limit_ms = e->use_limit_ms;
    r.value_quality = e->value_quality;
    r.input_availability = e->input_availability;
    r.related_command_id = e->related_command_id;
    return r;
}

static NetWireVssWarningStateRaw warning_raw(const IntegWarning *w)
{
    NetWireVssWarningStateRaw r;
    memset(&r, 0, sizeof(r));
    r.boot_id = w->boot_id;
    r.tx_sequence = w->tx_sequence;
    r.antipinch_source_boot = w->antipinch.source_boot;
    r.antipinch_source_sequence = w->antipinch.source_sequence;
    r.antipinch_original_age_ms = w->antipinch.original_age_ms;
    r.antipinch_state = w->antipinch.state;
    r.antipinch_quality = w->antipinch.quality;
    r.antipinch_reason = w->antipinch.reason;
    r.antipinch_availability = w->antipinch.availability;
    r.occupant_hazard_source_boot = w->occupant.source_boot;
    r.occupant_hazard_source_sequence = w->occupant.source_sequence;
    r.occupant_hazard_original_age_ms = w->occupant.original_age_ms;
    r.occupant_hazard_state = w->occupant.state;
    r.occupant_hazard_quality = w->occupant.quality;
    r.occupant_hazard_reason = w->occupant.reason;
    r.occupant_hazard_availability = w->occupant.availability;
    r.rear_obstacle_source_boot = w->rear.source_boot;
    r.rear_obstacle_source_sequence = w->rear.source_sequence;
    r.rear_obstacle_original_age_ms = w->rear.original_age_ms;
    r.rear_obstacle_state = w->rear.state;
    r.rear_obstacle_quality = w->rear.quality;
    r.rear_obstacle_reason = w->rear.reason;
    r.rear_obstacle_availability = w->rear.availability;
    r.rear_detection_activation = w->rear_detection_activation;
    r.activation_meta_format = w->activation_meta_format;
    r.rear_activation_quality = w->rear_activation_quality;
    r.rear_activation_reason = w->rear_activation_reason;
    r.rear_activation_sequence = w->rear_activation_sequence;
    r.rear_activation_age_ms = w->rear_activation_age_ms;
    return r;
}

static bool encode_event(const NetWireVssEventRaw *r, NetWireCanEnvelope *e)
{
    return net_wire_can_encode_vss_event(NET_WIRE_CANDIDATE_ONLY, r, e) == NET_WIRE_OK;
}

static bool encode_warning(const NetWireVssWarningStateRaw *r, NetWireCanEnvelope *e)
{
    return net_wire_can_encode_vss_warning_state(NET_WIRE_CANDIDATE_ONLY, r, e) == NET_WIRE_OK;
}

bool IntegBus_SendEvent(const IntegEvent *e)
{
    NetWireVssEventRaw r = event_raw(e);
    NetWireCanEnvelope env;
    return encode_event(&r, &env) && inject(&env);
}

bool IntegBus_SendWarning(const IntegWarning *w)
{
    NetWireVssWarningStateRaw r = warning_raw(w);
    NetWireCanEnvelope env;
    return encode_warning(&r, &env) && inject(&env);
}

bool IntegBus_SendPermission(uint32_t boot_id, uint32_t tx_sequence, uint32_t update_sequence)
{
    NetWireDomainPermissionRaw p;
    NetWireCanEnvelope env;
    memset(&p, 0, sizeof(p));
    p.boot_id = boot_id;
    p.tx_sequence = tx_sequence;
    p.vehicle_power_permission = NET_TRISTATE_YES;
    p.power_permission_quality = NET_QUALITY_OK;
    p.power_operation_state = NET_TRISTATE_YES;
    p.operation_allowed = NET_FLAG_YES;
    p.operation_quality = NET_QUALITY_OK;
    p.update_sequence = update_sequence;
    p.source_age_ms = 0u;
    return net_wire_can_encode_permission(NET_WIRE_CANDIDATE_ONLY, &p, &env) == NET_WIRE_OK && inject(&env);
}

/* Byte positions of a field, found through the codec itself: two valid
 * encodings that differ only in that field. */
static size_t field_bytes(const NetWireCanEnvelope *a, const NetWireCanEnvelope *b, size_t *at, size_t max)
{
    size_t i, n = 0u;
    for (i = 0u; i < a->length && i < b->length; ++i)
        if (a->bytes[i] != b->bytes[i] && n < max) at[n++] = i;
    return n;
}

/* CAN FD payload lengths that a DLC can carry (DLC 8..15). */
static const uint8_t fd_lengths[] = { 8u, 12u, 16u, 20u, 24u, 32u, 48u, 64u };

/* The FD length one DLC step above (up) or below the registered one, 0 if none. */
static size_t next_fd_length(size_t length, bool up)
{
    size_t i;
    for (i = 0u; i < sizeof(fd_lengths); ++i) {
        if (fd_lengths[i] != length) continue;
        if (up) return i + 1u < sizeof(fd_lengths) ? fd_lengths[i + 1u] : 0u;
        return i > 0u ? fd_lengths[i - 1u] : 0u;
    }
    return 0u;
}

/* Breaks one property of a valid frame after encoding, or sends it with the
 * wrong DLC or as Classic CAN. False when the defect does not apply. */
static bool send_defect(NetWireCanEnvelope *env, const NetWireCanEnvelope *alt_boot, IntegDefect defect)
{
    size_t at[4], n, i, length;
    switch (defect) {
    case INTEG_DEFECT_LONGER:
    case INTEG_DEFECT_SHORTER:
        length = next_fd_length(env->length, defect == INTEG_DEFECT_LONGER);
        if (length == 0u) return false;
        if (length > env->length) memset(env->bytes + env->length, 0, length - env->length);
        return inject_frame(env, true, true, MockRtd_DlcForLength((uint8)length), length);
    case INTEG_DEFECT_CLASSIC:
        return inject_frame(env, false, false, 8u, 8u);
    case INTEG_DEFECT_BOOT_ZERO:
        n = field_bytes(env, alt_boot, at, 4u);
        if (n != 4u) return false;
        for (i = 0u; i < n; ++i) env->bytes[at[i]] = 0u;
        return inject(env);
    default:
        return false;
    }
}

bool IntegBus_SendEventDefect(const IntegEvent *valid, IntegDefect defect)
{
    NetWireVssEventRaw r = event_raw(valid), r2 = r;
    NetWireCanEnvelope env, other;
    size_t at[1];
    if (!encode_event(&r, &env)) return false;
    if (defect == INTEG_DEFECT_EVENT_KIND_0 || defect == INTEG_DEFECT_EVENT_KIND_6) {
        r2.event_kind = (uint8_t)(r.event_kind == NET_VSS_EVENT_WELCOME ? NET_VSS_EVENT_GOODBYE
                                                                        : NET_VSS_EVENT_WELCOME);
        if (!encode_event(&r2, &other) || field_bytes(&env, &other, at, 1u) != 1u) return false;
        env.bytes[at[0]] = (uint8_t)(defect == INTEG_DEFECT_EVENT_KIND_0 ? 0u : 6u);
        return inject(&env);
    }
    r2.boot_id = ~r.boot_id;
    if (!encode_event(&r2, &other)) return false;
    return send_defect(&env, &other, defect);
}

bool IntegBus_SendWarningDefect(const IntegWarning *valid, IntegDefect defect)
{
    NetWireVssWarningStateRaw r = warning_raw(valid), a, b;
    NetWireCanEnvelope env, ea, eb;
    size_t at[1];
    if (!encode_warning(&r, &env)) return false;
    if (defect == INTEG_DEFECT_META_FORMAT_2) {
        /* Format 1 and format 0 differ only in that byte when the activation
         * evidence is all zero (the format 0 rule). */
        a = r;
        a.rear_detection_activation = 0u;
        a.rear_activation_quality = 0u;
        a.rear_activation_reason = 0u;
        a.rear_activation_sequence = 0u;
        a.rear_activation_age_ms = 0u;
        a.activation_meta_format = 1u;
        b = a;
        b.activation_meta_format = 0u;
        if (!encode_warning(&a, &ea) || !encode_warning(&b, &eb) || field_bytes(&ea, &eb, at, 1u) != 1u)
            return false;
        env.bytes[at[0]] = 2u;
        return inject(&env);
    }
    if (defect == INTEG_DEFECT_REAR_STATE_3) {
        a = r;
        a.rear_obstacle_state = (uint8_t)(r.rear_obstacle_state == NET_REAR_WARNING_CLEAR
                                          ? NET_REAR_WARNING_CAUTION : NET_REAR_WARNING_CLEAR);
        if (!encode_warning(&a, &ea) || field_bytes(&env, &ea, at, 1u) != 1u) return false;
        env.bytes[at[0]] = 3u;
        return inject(&env);
    }
    a = r;
    a.boot_id = ~r.boot_id;
    if (!encode_warning(&a, &ea)) return false;
    return send_defect(&env, &ea, defect);
}

void IntegBus_RaiseBusOff(void)
{
    in_isr = true;
    MockRtd_CanRaiseBusOff(BUS);
    in_isr = false;
}

bool IntegBus_TakeFrame(IntegFrame *out)
{
    MockCanFrame f;
    NetWireCanEnvelope e;
    NetWireVssStatusRaw raw;
    int mb;
    in_isr = true;
    mb = MockRtd_CanTransmitNext(BUS, &f);
    in_isr = false;
    if (mb < 0) return false;
    memset(out, 0, sizeof(*out));
    out->id = f.id;
    out->fd = f.fd;
    out->brs = f.brs;
    out->dlc = f.dlc;
    out->length = f.length;
    memset(&e, 0, sizeof(e));
    e.standard_id = f.id;
    e.fdf = f.fd;
    e.brs = f.brs;
    e.dlc = f.dlc;
    e.length = f.length;
    memcpy(e.bytes, f.data, f.length);
    if (net_wire_can_decode_vss_status(NET_WIRE_CANDIDATE_ONLY, &e, &raw) == NET_WIRE_OK) {
        out->decoded = true;
        out->boot_id = raw.boot_id;
        out->tx_sequence = raw.tx_sequence;
        out->vss_state = raw.vss_state;
        out->vss_availability = raw.vss_availability;
        out->vss_accepting_events = raw.vss_accepting_events;
        out->vss_fault_active = raw.vss_fault_active;
        out->fault_mask = raw.fault_mask;
        out->vss_last_fault = raw.vss_last_fault;
        out->last_fault_occurrence = raw.last_fault_occurrence;
        out->last_fault_age_ms = raw.last_fault_age_ms;
        out->last_fault_quality = raw.last_fault_quality;
        out->update_sequence = raw.update_sequence;
        out->source_age_ms = raw.source_age_ms;
    }
    return true;
}

/* -------------------------------------------------------- diagnostics ---- */
bool IntegBus_ReadDiag(IntegNetDiag *out)
{
    NetNodeDiag d;
    if (out == NULL || NetNode_GetDiag(&d) != NET_OK) return false;
    memset(out, 0, sizeof(*out));
    out->ready = d.entry_init_result == NET_ENTRY_INIT_OK && d.stage == NET_NODE_STAGE_READY;
    out->init_calls = d.entry_init_calls;
    out->main_calls = d.entry_main_calls;
    out->node_boot_id = d.app.boot_id;
    out->app_tick_time = d.rtd.time_source == NET_HAL_RTD_TIME_APP_TICK;
    out->rx_frames = d.rtd.rx_frames;
    out->rx_delivered = d.upper.rx_delivered;
    out->rx_invalid = d.upper.rx_invalid;
    out->rx_held = d.upper.rx_held;
    out->stale = d.upper.stale;
    out->duplicates = d.upper.duplicates;
    out->rx_conflicts = d.upper.rx_conflicts;
    out->repeat_delivered = d.upper.repeat_delivered;
    out->repeat_age_raised = d.upper.repeat_age_raised;
    out->generation_changes = d.upper.generation_changes;
    out->old_generation = d.upper.old_generation;
    out->event_delivered = d.app.event_delivered;
    out->warning_delivered = d.app.warning_delivered;
    out->permission_received = d.app.permission_received;
    out->status_not_accepted = d.app.status_not_accepted;
    out->last_status_result = (uint8_t)d.app.last_status_result;
    out->drain_budget = d.rtd.isr_drain_budget;
    out->ring_capacity = d.rtd.isr_ring_capacity;
    out->ring_overflow = d.rtd.isr_ring_overflow;
    out->rx_loss_events = d.rtd.rx_loss_events;
    out->ring_max_depth = d.rtd.isr_ring_max_depth;
    out->main_drained_max = d.rtd.main_drained_max;
    out->tick_future_clamped = d.rtd.tick_future_clamped;
    out->busoff_events = d.rtd.busoff_events;
    out->tx_complete = d.rtd.tx_complete;
    out->main_max_us = d.main_max_us;
    return true;
}

/* ------------------------------------------------- API layout (vi0) ---- */
#define LAYOUT_PUT(x) do { if (n < cap) out[n] = (uint32_t)(x); n++; } while (0);
#define LAYOUT_SIZE(T) LAYOUT_PUT(sizeof(T))
#define LAYOUT_FIELD(T, f) LAYOUT_PUT(offsetof(T, f)) LAYOUT_PUT(sizeof(((T *)0)->f))
#define LAYOUT_VALUE(v) LAYOUT_PUT(v)

size_t IntegBus_ApiLayout(uint32_t *out, size_t cap)
{
    size_t n = 0u;
    VSS_INTEG_API_LAYOUT(LAYOUT_SIZE, LAYOUT_FIELD, LAYOUT_VALUE)
    return n;
}
