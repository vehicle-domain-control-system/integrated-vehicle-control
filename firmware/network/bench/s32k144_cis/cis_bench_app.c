/* S32K144 bench stand-in for the CIS application (not Network product code).
 * See cis_bench_app.h. */
#include "cis_bench_app.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#include <string.h>
#include "net_board_time144.h"

volatile CisBenchDiag g_cis_bench;
volatile NetNodeDiag g_net_diag;

static const uint32_t s_period_ms[CIS_BENCH_PERIODIC_COUNT] = {
    CIS_BENCH_REAR_PERIOD_MS, CIS_BENCH_ENVIRONMENT_PERIOD_MS, CIS_BENCH_STATUS_PERIOD_MS};

static NetNodeDiag s_diag;                              /* NetNode_GetDiag() target (non-volatile) */
static bool s_started;
static uint32_t s_sequence[CIS_BENCH_PERIODIC_COUNT];   /* current evaluation of each periodic record */
static uint32_t s_next_ms[CIS_BENCH_PERIODIC_COUNT];
static uint32_t s_next_diag_ms;
static uint32_t s_next_watch_ms;
static bool s_reason_known;

static void refresh_diag(void)
{
    if (NetNode_GetDiag(&s_diag) == NET_OK) {
        g_net_diag = s_diag;
    }
    g_cis_bench.diag_refreshes++;
}

/* The supervision reason now; a change (or the first sample) is counted with its time. */
static void watch_permission(uint32_t now)
{
    unsigned reason;

    if (NetNode_GetDiag(&s_diag) != NET_OK) return;
    reason = (unsigned)s_diag.permission.reason;
    if (reason >= CIS_BENCH_REASON_COUNT) return;
    if (!s_reason_known || reason != g_cis_bench.permission_reason) {
        if (s_reason_known) g_cis_bench.permission_reason_changes++;
        g_cis_bench.permission_reason = (uint8_t)reason;
        g_cis_bench.permission_reason_entries[reason]++;
        g_cis_bench.permission_reason_entered_ms[reason] = now;
        s_reason_known = true;
    }
}

static void count(unsigned which, NetSubmitResult result, uint32_t sequence)
{
    volatile CisBenchPublishDiag *d = &g_cis_bench.publish[which];

    d->submitted++;
    if ((uint32_t)result < 4u) d->result[result]++;
    d->last_result = (uint8_t)result;
    d->last_sequence = sequence;
    if (result != NET_SUBMIT_OK) g_cis_bench.refused++;
}

void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)
{
    g_cis_bench.permission_count++;
    if (message != NULL) g_cis_bench.last_permission = *message;
    if (meta != NULL) g_cis_bench.last_permission_meta = *meta;
}

/* The relay of the Pi judgement just delivered: identity, AGE, values and
 * each field's validity / reason as received (no restriction added), the
 * CIS function state ACTIVE (the CIS's own state, not compared with the Pi
 * VISION_STATUS). Network sends it in this pass. */
void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta)
{
    NetCisOccupant o;

    g_cis_bench.vision_result_count++;
    if (meta != NULL) g_cis_bench.last_vision_result_meta = *meta;
    if (message == NULL) return;
    g_cis_bench.last_vision_result = *message;
    memset(&o, 0, sizeof(o));
    o.pi_boot_id = message->pi_boot_id;
    o.vision_sequence = message->vision_sequence;
    o.vision_age_ms = message->vision_age_ms;
    o.occupant_presence = message->occupant_presence;
    o.presence_validity = message->presence_validity;
    o.presence_reason = message->presence_reason;
    o.occupant_count = message->occupant_count;
    o.count_validity = message->count_validity;
    o.count_reason = message->count_reason;
    o.vision_function_status = NET_SENSOR_FUNCTION_ACTIVE;
    count(CIS_BENCH_OCCUPANT, CisNet_PublishOccupant(&o), message->vision_sequence);
}

void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta)
{
    g_cis_bench.vision_status_count++;
    if (message != NULL) g_cis_bench.last_vision_status = *message;
    if (meta != NULL) g_cis_bench.last_vision_status_meta = *meta;
}

/* A distance that moves 50.0 .. 149.0 cm, valid. */
static NetSubmitResult publish_rear(uint32_t sequence)
{
    NetCisRear r;

    memset(&r, 0, sizeof(r));
    r.update_sequence = sequence;
    r.source_age_ms = 0u;
    r.rear_distance_deci_cm = (uint16_t)(500u + (sequence % 100u) * 10u);
    r.validity = NET_VALIDITY_VALID;
    r.quality_reason = NET_QUALITY_REASON_NONE;
    r.proximity_status = NET_PROXIMITY_VALID_DISTANCE;
    return CisNet_PublishRear(&r);
}

/* 23.50 .. 23.59 degC, 45.00 %RH, 300 .. 349 lx, all valid; one measurement
 * number for the three values. */
static NetSubmitResult publish_environment(uint32_t sequence)
{
    NetCisEnvironment e;

    memset(&e, 0, sizeof(e));
    e.temperature_centi_c = (int16_t)(2350 + (int16_t)(sequence % 10u));
    e.temperature_sequence = sequence;
    e.temperature_age_ms = 0u;
    e.temperature_validity = NET_VALIDITY_VALID;
    e.temperature_reason = NET_QUALITY_REASON_NONE;
    e.humidity_centi_percent = 4500u;
    e.humidity_sequence = sequence;
    e.humidity_age_ms = 0u;
    e.humidity_validity = NET_VALIDITY_VALID;
    e.humidity_reason = NET_QUALITY_REASON_NONE;
    e.illuminance_lux = 300u + (sequence % 50u);
    e.illuminance_sequence = sequence;
    e.illuminance_age_ms = 0u;
    e.illuminance_validity = NET_VALIDITY_VALID;
    e.illuminance_reason = NET_QUALITY_REASON_NONE;
    return CisNet_PublishEnvironment(&e);
}

/* ACTIVE, every function ACTIVE, both interfaces READY, no fault. */
static NetSubmitResult publish_status(uint32_t sequence)
{
    NetCisStatus s;

    memset(&s, 0, sizeof(s));
    s.cis_state = NET_CIS_STATE_ACTIVE;
    s.vision_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.temperature_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.humidity_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.illuminance_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.rear_status = NET_SENSOR_FUNCTION_ACTIVE;
    s.pi_uart_status = NET_INTERFACE_READY;
    s.can_interface_status = NET_INTERFACE_READY;
    s.fault_mask = 0u;
    s.active_fault = NET_CIS_FAULT_CODE_NONE;
    s.recovering = NET_FLAG_NO;
    s.last_fault = NET_CIS_FAULT_CODE_NONE;
    s.last_fault_occurrence = 0u;
    s.affected_function = 0u;
    s.update_sequence = sequence;
    s.source_age_ms = 0u;
    return CisNet_PublishStatus(&s);
}

static void submit(unsigned which)
{
    uint32_t seq = ++s_sequence[which];
    NetSubmitResult result;

    switch (which) {
    case CIS_BENCH_REAR: result = publish_rear(seq); break;
    case CIS_BENCH_ENVIRONMENT: result = publish_environment(seq); break;
    default: result = publish_status(seq); break;
    }
    count(which, result, seq);
}

void CisBench_Init(void)
{
    refresh_diag();
}

void CisBench_Service(void)
{
    uint32_t now = Board_NowMs32();
    unsigned i;

    for (i = 0u; i < CIS_BENCH_PERIODIC_COUNT; i++) {
        if (!s_started || ((int32_t)(now - s_next_ms[i]) >= 0)) {
            s_next_ms[i] = now + s_period_ms[i];
            submit(i);
        }
    }
    s_started = true;
    if ((int32_t)(now - s_next_watch_ms) >= 0) {
        s_next_watch_ms = now + CIS_BENCH_WATCH_PERIOD_MS;
        watch_permission(now);
    }
    if ((int32_t)(now - s_next_diag_ms) >= 0) {
        s_next_diag_ms = now + CIS_BENCH_DIAG_PERIOD_MS;
        refresh_diag();
    }
}
#else
/* Not the CIS node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int cis_bench_app_absent;
#endif
