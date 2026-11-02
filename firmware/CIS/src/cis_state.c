#include "cis_state.h"

cis_state_t cis_state_eval(cis_init_t init, bool common_fault,
                           const cis_func_status_t fn[CIS_FN_COUNT])
{
    unsigned i;
    if (init == CIS_INIT_FAILED || (init == CIS_INIT_DONE && common_fault)) {
        return CIS_STATE_FAULT;
    }
    if (init == CIS_INIT_PENDING) {
        return CIS_STATE_STARTUP;
    }
    for (i = 0; i < CIS_FN_COUNT; i++) {
        if (fn[i] == CIS_FUNC_ACTIVE) {
            return CIS_STATE_ACTIVE;
        }
    }
    return CIS_STATE_READY;
}
