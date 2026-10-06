#include "cis_fault.h"

uint16_t cis_fault_code(cis_fault_bit_t bit)
{
    return (uint16_t)(bit + 1u);
}

void cis_fault_init(cis_fault_t *f)
{
    f->mask = 0;
    f->active_fault = 0;
    f->last_fault = 0;
    f->last_occurrence = 0;
    f->occurrence_counter = 0;
}

void cis_fault_set(cis_fault_t *f, cis_fault_bit_t bit)
{
    uint16_t m = (uint16_t)(1u << bit);
    if (f->mask & m) {
        return;                     /* already active: not a new occurrence */
    }
    f->mask |= m;
    f->active_fault = cis_fault_code(bit);
    f->last_fault = f->active_fault;
    f->occurrence_counter++;
    f->last_occurrence = f->occurrence_counter;
}

void cis_fault_clear(cis_fault_t *f, cis_fault_bit_t bit)
{
    uint16_t m = (uint16_t)(1u << bit);
    unsigned i;
    f->mask &= (uint16_t)~m;
    if (f->active_fault == cis_fault_code(bit)) {
        f->active_fault = 0;
        for (i = 0; i < CIS_FAULT_BIT_COUNT; i++) {
            if (f->mask & (1u << i)) {
                f->active_fault = (uint16_t)(i + 1u);
                break;
            }
        }
    }
    /* last_fault / last_occurrence are history and stay. */
}
