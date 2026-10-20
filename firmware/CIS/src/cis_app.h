/*
 * CIS application: owns the sensor and vision state and runs the functions.
 *
 * cis_app_run() is the single entry point. It does not care who calls it: today the
 * main loop, later a task or the poll call of the Network stack. It never blocks for
 * more than the sensor reads (DHT22 ~7 ms, VEML7700 ~1 ms).
 * Inputs from outside (DOMAIN permission, Pi frames) come in through the on_* functions.
 */
#ifndef CIS_APP_H
#define CIS_APP_H

#include "rear_logic.h"
#include "env_logic.h"
#include "lux_logic.h"
#include "vision_logic.h"
#include "power_gate.h"

/*
 * Build with CIS_USE_NETWORK_API once the Network stack is linked: it then owns the Pi UART and
 * the P_PERMISSION relay, and cis_net_adapter.c connects the CIS to its API.
 */

#define CIS_BOOT_ID     1u       /* [TBD] provided by the Network stack; 0 is reserved */

void cis_app_init(void);

/* One pass over all functions. now_ms from timebase_now_ms(). */
void cis_app_run(uint32_t now_ms);

/* DOMAIN_PERMISSION arrived (the Network stack calls this from CisApp_OnDomainPermission). */
void cis_app_on_permission(uint8_t permission, bool quality_ok, uint32_t source_age_ms,
                           uint32_t now_ms);

/* Pi frames, delivered by the Network stack (CisApp_OnPiVision*) or by the own UART driver. */
void cis_app_on_vision_result(const pi_vision_result_t *f, uint32_t now_ms);
void cis_app_on_vision_status(const pi_vision_status_t *f, uint32_t now_ms);

/* Read-only views of the current state. */
const rear_state_t   *cis_app_rear(void);          /* raw measurement */
const rear_output_t  *cis_app_rear_output(void);   /* after the power gate: what is reported */
const env_state_t    *cis_app_env(void);
const lux_state_t    *cis_app_lux(void);
const vision_state_t *cis_app_vision(void);
const power_gate_t   *cis_app_gate(void);

#endif /* CIS_APP_H */
