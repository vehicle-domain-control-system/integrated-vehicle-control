/* S32K144 bench stand-in for the BCM application (not Network product code).
 * See bcm_bench_app.h. */
#include "bcm_bench_app.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
#include "net_board_time144.h"

volatile BcmBenchDiag g_bcm_bench;
volatile NetNodeDiag g_net_diag;

static const uint32_t s_period_ms[BCM_BENCH_STATE_COUNT] = {
    BCM_BENCH_DOOR_PERIOD_MS, BCM_BENCH_CLIMATE_PERIOD_MS, BCM_BENCH_LIGHT_PERIOD_MS, BCM_BENCH_STATUS_PERIOD_MS};

static NetNodeDiag s_diag;                              /* NetNode_GetDiag() target (non-volatile) */
static bool s_started;
static uint32_t s_update_sequence[BCM_BENCH_STATE_COUNT]; /* current evaluation of each state */
static uint32_t s_next_ms[BCM_BENCH_STATE_COUNT];
static uint32_t s_next_diag_ms;

static void refresh_diag(void)
{
    if (NetNode_GetDiag(&s_diag) == NET_OK) {
        g_net_diag = s_diag;
    }
    g_bcm_bench.diag_refreshes++;
}

void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta)
{
    (void)message;
    (void)meta;
    g_bcm_bench.door_command_count++;
}

void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta)
{
    (void)message;
    (void)meta;
    g_bcm_bench.climate_command_count++;
}

void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta)
{
    (void)message;
    (void)meta;
    g_bcm_bench.light_command_count++;
}

/* Locked, closed, consistent. */
static NetSubmitResult publish_door(uint32_t update_sequence)
{
    NetBcmDoorState st;

    st.door_lock_state = NET_DOOR_LOCK_LOCKED;
    st.door_open_state = NET_DOOR_OPEN_CLOSED;
    st.door_composite_state = NET_DOOR_COMPOSITE_NORMAL;
    st.lock_quality = NET_QUALITY_OK;
    st.open_quality = NET_QUALITY_OK;
    st.composite_quality = NET_QUALITY_OK;
    st.update_sequence = update_sequence;
    st.source_age_ms = 0u;
    return BcmNet_PublishDoorState(&st);
}

/* Fan commanded and measured OFF, no thermal output, heat removal normal. */
static NetSubmitResult publish_climate(uint32_t update_sequence)
{
    NetBcmClimateState st;

    st.fan_command_level = NET_FAN_OFF;
    st.fan_measured_level = NET_FAN_OFF;
    st.thermal_direction_state = NET_THERMAL_IDLE;
    st.thermal_output_percent = 0u;
    st.heat_removal_state = NET_HEAT_REMOVAL_NORMAL;
    st.fan_command_quality = NET_QUALITY_OK;
    st.fan_measured_quality = NET_QUALITY_OK;
    st.thermal_direction_quality = NET_QUALITY_OK;
    st.thermal_output_quality = NET_QUALITY_OK;
    st.heat_removal_quality = NET_QUALITY_OK;
    st.update_sequence = update_sequence;
    st.source_age_ms = 0u;
    return BcmNet_PublishClimateState(&st);
}

/* Normal light at 0 %, colour 0/0/0, applied; no physical feedback. */
static NetSubmitResult publish_light(uint32_t update_sequence)
{
    NetBcmLightState st;

    st.interior_light_type_state = NET_LIGHT_NORMAL;
    st.light_level_percent = 0u;
    st.interior_light_color_state.r = 0u;
    st.interior_light_color_state.g = 0u;
    st.interior_light_color_state.b = 0u;
    st.interior_light_apply_result = NET_LIGHT_APPLY_APPLIED;
    st.light_physical_feedback_capability = NET_FLAG_NO;
    st.light_type_quality = NET_QUALITY_OK;
    st.light_level_quality = NET_QUALITY_OK;
    st.light_color_quality = NET_QUALITY_OK;
    st.light_apply_quality = NET_QUALITY_OK;
    st.update_sequence = update_sequence;
    st.source_age_ms = 0u;
    return BcmNet_PublishLightState(&st);
}

/* READY, no fault. */
static NetSubmitResult publish_status(uint32_t update_sequence)
{
    NetBcmStatus st;

    st.bcm_ecu_state = NET_CONTROLLER_READY;
    st.fault_mask = 0u;
    st.active_fault_category = NET_FAULT_CATEGORY_NONE;
    st.active_fault_code = NET_BCM_FAULT_CODE_NONE;
    st.affected_function = 0u;
    st.recovering = NET_FLAG_NO;
    st.update_sequence = update_sequence;
    st.source_age_ms = 0u;
    return BcmNet_PublishStatus(&st);
}

static void submit(unsigned which)
{
    volatile BcmBenchStateDiag *d = &g_bcm_bench.state[which];
    uint32_t seq = ++s_update_sequence[which];
    NetSubmitResult result;

    switch (which) {
    case BCM_BENCH_DOOR: result = publish_door(seq); break;
    case BCM_BENCH_CLIMATE: result = publish_climate(seq); break;
    case BCM_BENCH_LIGHT: result = publish_light(seq); break;
    default: result = publish_status(seq); break;
    }
    d->submitted++;
    if ((uint32_t)result < 4u) d->result[result]++;
    d->last_result = (uint8_t)result;
    d->last_update_sequence = seq;
    if (result != NET_SUBMIT_OK) g_bcm_bench.refused++;
}

void BcmBench_Init(void)
{
    refresh_diag();
}

void BcmBench_Service(void)
{
    uint32_t now = Board_NowMs32();
    unsigned i;

    for (i = 0u; i < (unsigned)BCM_BENCH_STATE_COUNT; i++) {
        if (!s_started || ((int32_t)(now - s_next_ms[i]) >= 0)) {
            s_next_ms[i] = now + s_period_ms[i];
            submit(i);
        }
    }
    s_started = true;
    if ((int32_t)(now - s_next_diag_ms) >= 0) {
        s_next_diag_ms = now + BCM_BENCH_DIAG_PERIOD_MS;
        refresh_diag();
    }
}
#else
/* Not the BCM node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int bcm_bench_app_absent;
#endif
