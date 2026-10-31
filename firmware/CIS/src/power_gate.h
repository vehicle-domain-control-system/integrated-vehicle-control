/*
 * Rear detection power gate. No hardware access.
 *
 * DOMAIN decides whether the rear detection may run (DOMAIN_PERMISSION). The CIS only
 * consumes that decision. Without a confirmed permission the rear output is INACTIVE and
 * never a distance or NO_OBJECT.
 *
 * The permission is usable only while all of these hold:
 *  - a DOMAIN_PERMISSION frame arrived within POWER_RX_TIMEOUT_MS (CAN supervision)
 *  - the age of the DOMAIN evaluation, counted from the original judgement, is below
 *    POWER_ORIGIN_LIMIT_MS; a repeated frame does not restart this age
 *  - the value is ALLOW (1) with a good quality
 *
 * Output while the gate is closed (reason codes are a proposal):
 *  - confirmed DENY             -> INACTIVE, valid, reason NONE
 *  - unknown / never received   -> INACTIVE, invalid, NO_DATA
 *  - original evaluation expired-> INACTIVE, invalid, STALE
 *  - CAN frames stopped         -> INACTIVE, invalid, COMMUNICATION_FAULT
 * After the gate opens again, the rear result is not reported until a NEW measurement has
 * been made (UNAVAILABLE / NO_DATA meanwhile). The CIS starts closed.
 */
#ifndef POWER_GATE_H
#define POWER_GATE_H

#include "rear_logic.h"

#define POWER_RX_TIMEOUT_MS     150u    /* no DOMAIN_PERMISSION frame for this long */
#define POWER_ORIGIN_LIMIT_MS   500u    /* use limit of the original DOMAIN evaluation */

#define POWER_PERM_DENY         0u
#define POWER_PERM_ALLOW        1u
#define POWER_PERM_UNKNOWN      255u

typedef enum {
    POWER_GATE_OPEN = 0,
    POWER_GATE_DENIED,
    POWER_GATE_UNKNOWN,
    POWER_GATE_EXPIRED,
    POWER_GATE_LINK_LOST
} power_gate_state_t;

typedef struct {
    bool     have_rx;
    uint8_t  permission;
    bool     quality_ok;
    uint32_t rx_ms;
    uint32_t origin_age_ms;     /* age of the evaluation when the frame was received */
    bool     was_open;
    bool     wait_new_sample;
    uint32_t baseline_sequence;
} power_gate_t;

typedef struct {
    cis_proximity_t status;
    bool            valid;
    cis_reason_t    reason;
    uint32_t        distance_mm;    /* REAR_NO_VALUE when there is no distance */
    uint32_t        sequence;
} rear_output_t;

void power_gate_init(power_gate_t *g);

/* A DOMAIN_PERMISSION frame was received (also a repeat of the same evaluation).
 * source_age_ms is the age of the DOMAIN evaluation as given in the frame. */
void power_gate_on_permission(power_gate_t *g, uint8_t permission, bool quality_ok,
                              uint32_t source_age_ms, uint32_t now_ms);

power_gate_state_t power_gate_state(const power_gate_t *g, uint32_t now_ms);

/* Combines the permission with the rear measurement. Call every cycle. */
void power_gate_apply(power_gate_t *g, const rear_state_t *rear, uint32_t now_ms,
                      rear_output_t *out);

#endif /* POWER_GATE_H */
