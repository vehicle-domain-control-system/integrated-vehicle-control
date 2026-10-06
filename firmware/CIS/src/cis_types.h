/* CIS common types. No hardware dependency: builds on a PC and on S32K144. */
#ifndef CIS_TYPES_H
#define CIS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* QUALITY_REASON codes (network_design_draft_v0.1.md, proposal) */
typedef enum {
    CIS_REASON_NONE = 0,
    CIS_REASON_NOT_READY,
    CIS_REASON_OUT_OF_RANGE,
    CIS_REASON_SENSOR_FAULT,
    CIS_REASON_VISION_FAULT,
    CIS_REASON_STALE,
    CIS_REASON_NO_DATA,
    CIS_REASON_COMMUNICATION_FAULT,
    CIS_REASON_RECOVERING,
    CIS_REASON_DATA_INVALID
} cis_reason_t;

/* Per-function status */
typedef enum {
    CIS_FUNC_NOT_READY = 0,
    CIS_FUNC_READY,
    CIS_FUNC_ACTIVE,
    CIS_FUNC_FAULT,
    CIS_FUNC_RECOVERING
} cis_func_status_t;

/* Overall CIS state */
typedef enum {
    CIS_STATE_STARTUP = 0,
    CIS_STATE_READY,
    CIS_STATE_ACTIVE,
    CIS_STATE_FAULT
} cis_state_t;

/* Functions that carry their own status */
typedef enum {
    CIS_FN_VISION = 0,
    CIS_FN_TEMPERATURE,
    CIS_FN_HUMIDITY,
    CIS_FN_ILLUMINANCE,
    CIS_FN_REAR,
    CIS_FN_COUNT
} cis_function_t;

/* PROXIMITY_STATUS (network_design_draft_v0.1.md, proposal) */
typedef enum {
    CIS_PROX_VALID_DISTANCE = 0,
    CIS_PROX_NO_OBJECT,
    CIS_PROX_UNAVAILABLE,
    CIS_PROX_FAULT,
    CIS_PROX_RECOVERING
} cis_proximity_t;

/* FAULT_MASK bits. fault code = bit number + 1, code 0 = no fault. */
typedef enum {
    CIS_FAULT_INITIALIZATION_FAILURE = 0,
    CIS_FAULT_VISION,
    CIS_FAULT_TEMPERATURE_SENSOR,
    CIS_FAULT_HUMIDITY_SENSOR,
    CIS_FAULT_ILLUMINANCE_SENSOR,
    CIS_FAULT_PROXIMITY_SENSOR,
    CIS_FAULT_COMMUNICATION,
    CIS_FAULT_DATA_INVALID,
    CIS_FAULT_OUT_OF_RANGE,
    CIS_FAULT_BIT_COUNT
} cis_fault_bit_t;

#define CIS_AGE_SATURATED 65535u

#endif /* CIS_TYPES_H */
