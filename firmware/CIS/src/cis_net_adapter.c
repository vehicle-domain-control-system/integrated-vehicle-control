#ifdef CIS_USE_NETWORK_API

#include "cis_net_adapter.h"
#include "cis_app.h"
#include "cis_fault.h"
#include "cis_state.h"
#include "net_api_shim.h"

/* The Network stack accepts CIS_OCCUPANT only as the relay of the last P_VISION_RESULT it
 * delivered (same values, validities, reasons, function state). The original is kept here. */
static NetVisionResult s_pi_orig;
static bool            s_pi_orig_valid;

static uint32_t s_t_rear, s_t_occ, s_t_env, s_t_stat;
static bool     s_due_rear, s_due_occ, s_due_env, s_due_stat;

/* CIS status evaluation: a new evaluation number only when something changed. */
static cis_fault_t s_fault;
static NetCisStatus s_last_status;
static bool     s_have_status;
static uint32_t s_status_sequence;
static uint32_t s_status_eval_ms;

void cis_net_adapter_init(void)
{
    Net_Init();
    s_t_rear = s_t_occ = s_t_env = s_t_stat = 0u;
    s_due_rear = s_due_occ = s_due_env = s_due_stat = true;
    cis_fault_init(&s_fault);
    s_pi_orig_valid = false;
    s_have_status = false;
    s_status_sequence = 0u;
    s_status_eval_ms = 0u;
}

void cis_net_adapter_poll(uint32_t now_ms)
{
    Net_MainFunction(now_ms);
}

static uint16_t sat16(uint32_t v)
{
    return (v > 65535u) ? 65535u : (uint16_t)v;
}

/* ---- receive side: copy into the CIS and return ---------------------------------------- */

void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta)
{
    if (message == 0 || meta == 0) {
        return;
    }
    /* quality OK (0) is the only quality that may allow */
    cis_app_on_permission(message->vehicle_power_permission,
                          message->power_permission_quality == 0u,
                          message->source_age_ms, meta->received_at_ms);
}

void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta)
{
    pi_vision_result_t f;

    if (message == 0 || meta == 0) {
        return;
    }
    s_pi_orig = *message;
    s_pi_orig_valid = true;
    f.pi_boot_id = message->pi_boot_id;
    f.vision_sequence = message->vision_sequence;
    f.vision_age_ms = message->vision_age_ms;
    f.presence = message->occupant_presence;
    f.presence_validity = message->presence_validity;
    f.presence_reason = message->presence_reason;
    f.count = message->occupant_count;
    f.count_validity = message->count_validity;
    f.count_reason = message->count_reason;
    f.vision_status = message->vision_status;
    f.fault_code = message->vision_fault_code;
    cis_app_on_vision_result(&f, meta->received_at_ms);
}

void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta)
{
    pi_vision_status_t f;

    if (message == 0 || meta == 0) {
        return;
    }
    f.pi_boot_id = message->pi_boot_id;
    f.status_sequence = message->status_sequence;
    f.vision_status = message->vision_status;
    f.active_fault = message->active_fault;
    f.last_fault = message->last_fault;
    f.status_age_ms = message->status_age_ms;
    f.affected_function = message->affected_function;
    cis_app_on_vision_status(&f, meta->received_at_ms);
}

/* ---- send side -------------------------------------------------------------------------- */

static void fill_rear(NetCisRear *m, uint32_t now_ms)
{
    const rear_output_t *o = cis_app_rear_output();
    const rear_state_t *r = cis_app_rear();

    m->update_sequence = o->sequence;
    m->source_age_ms = cis_value_age_ms(&r->value, now_ms);
    /* the measurement is in mm, which is the same number as 0.1 cm: no division */
    m->rear_distance_deci_cm = (o->status == CIS_PROX_VALID_DISTANCE) ? sat16(o->distance_mm)
                                                                      : (uint16_t)REAR_NO_VALUE;
    m->validity = o->valid ? NET_VALIDITY_VALID : NET_VALIDITY_INVALID;
    m->quality_reason = (NetQualityReason)o->reason;
    m->proximity_status = (NetProximityStatus)o->status;
}

/* True while the CIS has not restricted either value: the current state is still the
 * Pi's own judgement, so it can go out as the relay of the original result. A value the
 * CIS has restricted (stale, link lost, contradiction, new Pi boot) is not submitted:
 * the Network stack refuses it, and the DOMAIN supervises the missing message itself. */
static bool occupant_is_relay(const vision_state_t *v)
{
    if (!s_pi_orig_valid || !v->link_up || !v->have_context ||
        v->pi_boot_id != s_pi_orig.pi_boot_id ||
        v->count.sequence != s_pi_orig.vision_sequence) {
        return false;
    }
    return (v->presence.valid == (s_pi_orig.presence_validity == NET_VALIDITY_VALID)) &&
           (v->count.valid == (s_pi_orig.count_validity == NET_VALIDITY_VALID)) &&
           (v->presence.valid || (uint8_t)v->presence.reason == s_pi_orig.presence_reason) &&
           (v->count.valid || (uint8_t)v->count.reason == s_pi_orig.count_reason);
}

/* The original Pi record, unchanged. Its AGE is the Pi's: the Network stack adds the time
 * since it received the result, once. */
static bool fill_occupant(NetCisOccupant *m)
{
    if (!occupant_is_relay(cis_app_vision())) {
        return false;
    }
    m->pi_boot_id = s_pi_orig.pi_boot_id;
    m->vision_sequence = s_pi_orig.vision_sequence;
    m->vision_age_ms = s_pi_orig.vision_age_ms;
    m->occupant_presence = s_pi_orig.occupant_presence;
    m->presence_validity = s_pi_orig.presence_validity;
    m->presence_reason = s_pi_orig.presence_reason;
    m->occupant_count = s_pi_orig.occupant_count;
    m->count_validity = s_pi_orig.count_validity;
    m->count_reason = s_pi_orig.count_reason;
    m->vision_function_status = s_pi_orig.vision_status;       /* the Pi's own code */
    return true;
}

/* A value that is not valid is sent as the "invalid" marker of its field, never as a number. */
static void fill_environment(NetCisEnvironment *m, uint32_t now_ms)
{
    const env_state_t *e = cis_app_env();
    const lux_state_t *l = cis_app_lux();

    m->temperature_centi_c = e->temperature.valid ? (int16_t)e->temperature.value : (int16_t)-32768;
    m->temperature_sequence = e->temperature.sequence;
    m->temperature_age_ms = cis_value_age_ms(&e->temperature, now_ms);
    m->temperature_validity = e->temperature.valid ? NET_VALIDITY_VALID : NET_VALIDITY_INVALID;
    m->temperature_reason = (NetQualityReason)e->temperature.reason;

    m->humidity_centi_percent = e->humidity.valid ? (uint16_t)e->humidity.value : (uint16_t)65535u;
    m->humidity_sequence = e->humidity.sequence;
    m->humidity_age_ms = cis_value_age_ms(&e->humidity, now_ms);
    m->humidity_validity = e->humidity.valid ? NET_VALIDITY_VALID : NET_VALIDITY_INVALID;
    m->humidity_reason = (NetQualityReason)e->humidity.reason;

    m->illuminance_lux = l->illuminance.valid ? (uint32_t)l->illuminance.value : 0xFFFFFFFFu;
    m->illuminance_sequence = l->illuminance.sequence;
    m->illuminance_age_ms = cis_value_age_ms(&l->illuminance, now_ms);
    m->illuminance_validity = l->illuminance.valid ? NET_VALIDITY_VALID : NET_VALIDITY_INVALID;
    m->illuminance_reason = (NetQualityReason)l->illuminance.reason;
}

static void track_fault(cis_fault_bit_t bit, cis_func_status_t st)
{
    if (st == CIS_FUNC_FAULT) {
        cis_fault_set(&s_fault, bit);
    } else {
        cis_fault_clear(&s_fault, bit);
    }
}

static void fill_status(NetCisStatus *m, uint32_t now_ms)
{
    const vision_state_t *v = cis_app_vision();
    cis_func_status_t fn[CIS_FN_COUNT];
    bool any_recovering = false;
    uint8_t affected = 0u;
    int i;

    fn[CIS_FN_VISION] = vision_function_status(v);
    fn[CIS_FN_TEMPERATURE] = cis_app_env()->temperature.status;
    fn[CIS_FN_HUMIDITY] = cis_app_env()->humidity.status;
    fn[CIS_FN_ILLUMINANCE] = cis_app_lux()->illuminance.status;
    fn[CIS_FN_REAR] = cis_app_rear()->value.status;

    track_fault(CIS_FAULT_VISION, fn[CIS_FN_VISION]);
    track_fault(CIS_FAULT_TEMPERATURE_SENSOR, fn[CIS_FN_TEMPERATURE]);
    track_fault(CIS_FAULT_HUMIDITY_SENSOR, fn[CIS_FN_HUMIDITY]);
    track_fault(CIS_FAULT_ILLUMINANCE_SENSOR, fn[CIS_FN_ILLUMINANCE]);
    track_fault(CIS_FAULT_PROXIMITY_SENSOR, fn[CIS_FN_REAR]);

    for (i = 0; i < (int)CIS_FN_COUNT; i++) {
        if (fn[i] == CIS_FUNC_RECOVERING) {
            any_recovering = true;
        }
        if (fn[i] == CIS_FUNC_FAULT || fn[i] == CIS_FUNC_RECOVERING) {
            affected |= (uint8_t)(1u << i);        /* bit0 vision .. bit4 rear */
        }
    }

    m->cis_state = (NetCisState)cis_state_eval(CIS_INIT_DONE, false, fn);
    m->vision_status = (NetSensorFunctionState)fn[CIS_FN_VISION];
    m->temperature_status = (NetSensorFunctionState)fn[CIS_FN_TEMPERATURE];
    m->humidity_status = (NetSensorFunctionState)fn[CIS_FN_HUMIDITY];
    m->illuminance_status = (NetSensorFunctionState)fn[CIS_FN_ILLUMINANCE];
    m->rear_status = (NetSensorFunctionState)fn[CIS_FN_REAR];
    m->pi_uart_status = v->link_up ? 0u : 1u;
    m->can_interface_status = 0u;                  /* [TBD] Network link observation (BINDING_OPEN) */
    m->fault_mask = s_fault.mask;
    m->active_fault = s_fault.active_fault;
    m->recovering = any_recovering ? 1u : 0u;
    m->last_fault = s_fault.last_fault;
    m->last_fault_occurrence = s_fault.last_occurrence;
    m->affected_function = affected;

    /* A new evaluation number only when the content changed. */
    if (!s_have_status ||
        m->cis_state != s_last_status.cis_state ||
        m->vision_status != s_last_status.vision_status ||
        m->temperature_status != s_last_status.temperature_status ||
        m->humidity_status != s_last_status.humidity_status ||
        m->illuminance_status != s_last_status.illuminance_status ||
        m->rear_status != s_last_status.rear_status ||
        m->pi_uart_status != s_last_status.pi_uart_status ||
        m->fault_mask != s_last_status.fault_mask ||
        m->active_fault != s_last_status.active_fault) {
        s_status_sequence++;
        s_status_eval_ms = now_ms;
    }
    m->update_sequence = s_status_sequence;
    m->source_age_ms = sat16(now_ms - s_status_eval_ms);
    s_last_status = *m;
    s_have_status = true;
}

static bool period_due(uint32_t now_ms, uint32_t *last, uint32_t period, bool *retry)
{
    return *retry || (uint32_t)(now_ms - *last) >= period;
}

void cis_net_adapter_run(uint32_t now_ms)
{
    if (period_due(now_ms, &s_t_rear, NETA_REAR_PERIOD_MS, &s_due_rear)) {
        NetCisRear m;
        fill_rear(&m, now_ms);
        s_due_rear = (CisNet_PublishRear(&m) != NET_SUBMIT_OK);   /* refused: offer again next time */
        s_t_rear = now_ms;
    }
    if (period_due(now_ms, &s_t_occ, NETA_OCCUPANT_PERIOD_MS, &s_due_occ)) {
        NetCisOccupant m;
        s_due_occ = fill_occupant(&m) && (CisNet_PublishOccupant(&m) != NET_SUBMIT_OK);
        s_t_occ = now_ms;
    }
    if (period_due(now_ms, &s_t_env, NETA_ENVIRONMENT_PERIOD_MS, &s_due_env)) {
        NetCisEnvironment m;
        fill_environment(&m, now_ms);
        s_due_env = (CisNet_PublishEnvironment(&m) != NET_SUBMIT_OK);
        s_t_env = now_ms;
    }
    if (period_due(now_ms, &s_t_stat, NETA_STATUS_PERIOD_MS, &s_due_stat)) {
        NetCisStatus m;
        fill_status(&m, now_ms);
        s_due_stat = (CisNet_PublishStatus(&m) != NET_SUBMIT_OK);
        s_t_stat = now_ms;
    }
}

#endif /* CIS_USE_NETWORK_API */
