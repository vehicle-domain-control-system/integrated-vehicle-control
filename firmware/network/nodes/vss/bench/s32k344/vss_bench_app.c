/* S32K344 bench stand-in for the VSS application (not Network product code).
 * See vss_bench_app.h. */
#include "vss_bench_app.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
#include "net_board_time.h"

volatile VssBenchDiag g_vss_bench;
volatile NetNodeDiag g_net_diag;

static NetNodeDiag s_diag;          /* NetNode_GetDiag() target (non-volatile) */
static bool s_started;
static bool s_waiting;              /* refused with NOT_READY / NO_CAPACITY */
static uint32_t s_update_sequence;  /* current evaluation */
static uint32_t s_evaluated_ms;     /* when the current evaluation was made */
static uint32_t s_next_status_ms;
static uint32_t s_next_diag_ms;

static void refresh_diag(void)
{
    if (NetNode_GetDiag(&s_diag) == NET_OK) {
        g_net_diag = s_diag;
    }
    g_vss_bench.diag_refreshes++;
}

void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta)
{
    if ((message == NULL) || (meta == NULL)) return;
    g_vss_bench.last_event = *message;
    g_vss_bench.last_event_meta = *meta;
    g_vss_bench.event_count++;
}

void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta)
{
    if ((message == NULL) || (meta == NULL)) return;
    g_vss_bench.last_warning = *message;
    g_vss_bench.last_warning_meta = *meta;
    g_vss_bench.warning_count++;
}

/* READY / FULL / accepting / no fault, no fault history (quality NO_DATA).
 * SOURCE_AGE is the age of the evaluation when it is offered: 0 for a new one,
 * the waiting time for a resubmitted one. */
static NetSubmitResult submit(uint32_t now_ms)
{
    NetVssStatus st;
    uint32_t age = now_ms - s_evaluated_ms;
    NetSubmitResult result;

    st.vss_state = NET_VSS_STATE_READY;
    st.vss_availability = NET_VSS_AVAILABILITY_FULL;
    st.vss_accepting_events = NET_FLAG_YES;
    st.vss_fault_active = NET_FLAG_NO;
    st.fault_mask = 0u;
    st.vss_last_fault = NET_VSS_FAULT_CODE_NONE;
    st.last_fault_occurrence = 0u;
    st.last_fault_age_ms = 0u;
    st.last_fault_quality = NET_QUALITY_NO_DATA;
    st.update_sequence = s_update_sequence;
    st.source_age_ms = (uint16_t)((age > 65535u) ? 65535u : age);
    result = VssNet_PublishStatus(&st);
    if ((uint32_t)result < 4u) g_vss_bench.status_result[result]++;
    g_vss_bench.last_result = (uint8_t)result;
    g_vss_bench.last_update_sequence = s_update_sequence;
    return result;
}

void VssBench_Init(void)
{
    refresh_diag();
}

void VssBench_Service(void)
{
    uint32_t now = Board_NowMs32();
    NetSubmitResult result;

    if (!s_started || ((int32_t)(now - s_next_status_ms) >= 0)) {
        if (s_waiting) g_vss_bench.status_replaced++;
        s_started = true;
        s_update_sequence++;
        s_evaluated_ms = now;
        s_next_status_ms = now + VSS_BENCH_STATUS_PERIOD_MS;
        g_vss_bench.status_built++;
        s_waiting = true;
    } else if (s_waiting) {
        g_vss_bench.status_resubmitted++;
    }
    if (s_waiting) {
        result = submit(now);
        s_waiting = (result == NET_SUBMIT_NOT_READY) || (result == NET_SUBMIT_NO_CAPACITY);
        if (result == NET_SUBMIT_INVALID_ARGUMENT) g_vss_bench.status_dropped++;
        g_vss_bench.status_waiting = s_waiting ? 1u : 0u;
    }
    if ((int32_t)(now - s_next_diag_ms) >= 0) {
        s_next_diag_ms = now + VSS_BENCH_DIAG_PERIOD_MS;
        refresh_diag();
    }
}
#else
/* Not the VSS node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int vss_bench_app_absent;
#endif
