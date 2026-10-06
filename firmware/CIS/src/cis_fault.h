/* FAULT_MASK / ACTIVE_FAULT / LAST_FAULT. Current faults and history are kept apart. */
#ifndef CIS_FAULT_H
#define CIS_FAULT_H

#include "cis_types.h"

typedef struct {
    uint16_t mask;              /* current faults, bit n = fault code n+1 */
    uint16_t active_fault;      /* main current fault code, 0 = none */
    uint16_t last_fault;        /* most recent fault code since boot (history) */
    uint32_t last_occurrence;   /* occurrence number of last_fault */
    uint32_t occurrence_counter;
} cis_fault_t;

void cis_fault_init(cis_fault_t *f);
void cis_fault_set(cis_fault_t *f, cis_fault_bit_t bit);
void cis_fault_clear(cis_fault_t *f, cis_fault_bit_t bit);   /* history is kept */
uint16_t cis_fault_code(cis_fault_bit_t bit);

#endif /* CIS_FAULT_H */
