/* CIS_STATE is derived from common init/fault flags and per-function status. */
#ifndef CIS_STATE_H
#define CIS_STATE_H

#include "cis_types.h"

typedef enum {
    CIS_INIT_PENDING = 0,
    CIS_INIT_DONE,
    CIS_INIT_FAILED
} cis_init_t;

/* FAULT   : initialization failed, or common processing/provision base is faulted
 * STARTUP : initialization not finished
 * ACTIVE  : at least one function provides a new valid result
 * READY   : otherwise
 * A single function fault never makes CIS_STATE = FAULT. */
cis_state_t cis_state_eval(cis_init_t init, bool common_fault,
                           const cis_func_status_t fn[CIS_FN_COUNT]);

#endif /* CIS_STATE_H */
