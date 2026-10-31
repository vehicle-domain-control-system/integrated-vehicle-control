/* Value container: value + validity + reason + age + update sequence. */
#ifndef CIS_VALUE_H
#define CIS_VALUE_H

#include "cis_types.h"

typedef struct {
    int32_t  value;
    uint32_t sequence;      /* +1 only when a NEW observation arrives */
    uint32_t sample_ms;     /* tick of the last new observation */
    bool     has_sample;
    bool     valid;         /* VALIDITY: true = VALID */
    cis_reason_t reason;
    cis_func_status_t status;
} cis_value_t;

void cis_value_init(cis_value_t *v);

/* New observation. Returns false (and changes nothing) while the function is
 * in FAULT: it must go through cis_value_begin_recovery() first. */
bool cis_value_new_sample(cis_value_t *v, int32_t value, uint32_t now_ms);

/* Value is unusable but the function is not faulted (e.g. out of range). */
void cis_value_invalidate(cis_value_t *v, cis_reason_t reason);

/* Function fault: invalid, status = FAULT. */
void cis_value_fault(cis_value_t *v, cis_reason_t reason);

/* The fault condition cleared. Stays invalid (RECOVERING) until the next new
 * valid sample. Only acts when status is FAULT. */
void cis_value_begin_recovery(cis_value_t *v);

/* Elapsed ms since the last new observation, saturated at 65535.
 * Safe across tick wrap-around. Resending a value does not reset it. */
uint16_t cis_value_age_ms(const cis_value_t *v, uint32_t now_ms);

/* If valid and older than limit_ms, mark INVALID/STALE. Returns true if changed. */
bool cis_value_check_stale(cis_value_t *v, uint32_t now_ms, uint32_t limit_ms);

#endif /* CIS_VALUE_H */
