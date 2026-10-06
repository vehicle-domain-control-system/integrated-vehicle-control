#include "cis_value.h"

void cis_value_init(cis_value_t *v)
{
    v->value = 0;
    v->sequence = 0;
    v->sample_ms = 0;
    v->has_sample = false;
    v->valid = false;
    v->reason = CIS_REASON_NOT_READY;
    v->status = CIS_FUNC_NOT_READY;
}

bool cis_value_new_sample(cis_value_t *v, int32_t value, uint32_t now_ms)
{
    if (v->status == CIS_FUNC_FAULT) {
        return false;               /* recovery must be started explicitly */
    }
    v->value = value;
    v->sequence++;
    v->sample_ms = now_ms;
    v->has_sample = true;
    v->valid = true;
    v->reason = CIS_REASON_NONE;
    v->status = CIS_FUNC_ACTIVE;    /* also completes RECOVERING */
    return true;
}

void cis_value_invalidate(cis_value_t *v, cis_reason_t reason)
{
    v->valid = false;
    v->reason = reason;
}

void cis_value_fault(cis_value_t *v, cis_reason_t reason)
{
    v->valid = false;
    v->reason = reason;
    v->status = CIS_FUNC_FAULT;
}

void cis_value_begin_recovery(cis_value_t *v)
{
    if (v->status == CIS_FUNC_FAULT) {
        v->valid = false;
        v->reason = CIS_REASON_RECOVERING;
        v->status = CIS_FUNC_RECOVERING;
    }
}

uint16_t cis_value_age_ms(const cis_value_t *v, uint32_t now_ms)
{
    uint32_t age;
    if (!v->has_sample) {
        return (uint16_t)CIS_AGE_SATURATED;
    }
    age = now_ms - v->sample_ms;    /* unsigned: correct across wrap-around */
    return (age > CIS_AGE_SATURATED) ? (uint16_t)CIS_AGE_SATURATED : (uint16_t)age;
}

bool cis_value_check_stale(cis_value_t *v, uint32_t now_ms, uint32_t limit_ms)
{
    uint32_t age;
    if (!v->valid || !v->has_sample) {
        return false;
    }
    age = now_ms - v->sample_ms;
    if (age > limit_ms) {
        cis_value_invalidate(v, CIS_REASON_STALE);
        return true;
    }
    return false;
}
