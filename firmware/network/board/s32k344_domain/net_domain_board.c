/* Network side of the DOMAIN board project. See net_domain_board.h. */
#include "net_domain_board.h"
#include <string.h>

/* HOLD: the BOOT_ID source is not decided (include/network/net_boot_id.h
 * computes one from a noinit record; the record section, the reset cause and
 * the seed are board work not done yet). This test-bench constant stays the
 * same across resets, so after a reset every DOMAIN frame restarts at
 * TX_SEQUENCE 1 under the same BOOT_ID and receivers reject it as old
 * (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md common header: BOOT_ID
 * belongs to the sending Network, 0 reserved; it changes on every start). */
#define DOMAIN_BOARD_BOOT_ID 0x444F4D42u

bool NetDomainBoard_NodeConfig(NetNodeConfig *out)
{
    if (out == NULL) return false;
    out->boot_id = DOMAIN_BOARD_BOOT_ID; /* epochs: Net_Init defaults (1) */
    return true;
}

/* The capture stamp is already on the millisecond axis of the DOMAIN main loop
 * (net_hal_rtd_last_now_ms32). */
uint32_t NetDomainBoard_TickToMs(uint32_t tick)
{
    return tick;
}

/* C2 boundary: the pinned DOMAIN functions (net_domain_c2_pinned_owner_api).
 * Semantic callbacks: the two provisional rules of this board, the value
 * quality of VALIDITY / REASON and the Power / Operation bundle of
 * DOMAIN_PERMISSION; without them CIS_REAR reception and DOMAIN_PERMISSION
 * transmission are held. The other seven stay NULL, and their routes are held
 * until the DOMAIN team decides them: status_quality, window_permission_bundle,
 * window_alive_bundle, window_event_projection, local_failure_reason,
 * select_query_plan, confirm_snapshot. Limits (10_CAN_FD.md): ACCEPT_LIMIT is
 * the 200 ms candidate (the 1 s conflict is OPEN), VSS_EVENT USE_LIMIT the
 * 500 ms draft. The node root sets BOOT_ID, TX_SEQUENCE start, the M04 slots,
 * the command target and the CIS transition barriers itself. */
bool NetDomainBoard_C2Config(NetDomainC2Api *api, NetDomainC2Config *config)
{
    if (api == NULL || config == NULL) return false;
    net_domain_c2_pinned_owner_api(api);
    memset(config, 0, sizeof(*config));
    config->value_quality = net_domain_c2_value_quality_provisional;
    config->permission_bundle = net_domain_c2_permission_bundle_provisional;
    config->command_accept_limit_ms = 200u;
    config->event_use_limit_ms = 500u;
    /* Commands, events and UART-M results: the receiver judges the AGE; the
     * bound only catches the saturated wire AGE plus margin. */
    config->transmit_policy.bound = true;
    config->transmit_policy.maximum_age_ms = 65535u + 1000u;
    config->transmit_policy.expires_at_equality = true;
    /* DomainIf_RxNetworkLocalFailure records into the DOMAIN result owner. */
    config->failure_sink_ready = true;
    config->cis_transition_barriers = true;
    return true;
}

#ifdef NET_DOMAIN_BENCH_PERMISSION
/* TEST BENCH ONLY. Never called from a product path: it stands in for the
 * DOMAIN application, which does not send DOMAIN_PERMISSION yet, so that a
 * bench (S32K144 debug node) sees 0x080 and its withdrawal.
 * Every 200 ms: ten ALLOWs, each a new evaluation (number from 1, AGE 0), then
 * one UNKNOWN shaped like the DOMAIN permission manager reports a failed
 * evaluation: false + NO_DATA, the number of the last ALLOW, AGE the time since
 * that evaluation. */
void NetDomainBench_PermissionPoll(uint32_t nowMs)
{
    static bool started;
    static uint32_t next_ms, evaluation, evaluated_at_ms;
    static uint8_t allows;
    DomainIf_CisPowerPermission_t p;
    if (started && (int32_t)(nowMs - next_ms) < 0) return;
    started = true;
    next_ms = nowMs + 200u;
    memset(&p, 0, sizeof(p));
    if (allows < 10u) {
        evaluation = evaluation == UINT32_MAX ? 1u : evaluation + 1u;
        evaluated_at_ms = nowMs;
        ++allows;
        p.vehiclePowerAllowed = true;
        p.quality = DATA_QUALITY_OK;
        p.meta.quality = DATA_QUALITY_OK;
        p.meta.ageMs = 0u;
    } else {
        allows = 0u;
        p.vehiclePowerAllowed = false;
        p.quality = DATA_QUALITY_NO_DATA;
        p.meta.quality = DATA_QUALITY_NO_DATA;
        p.meta.ageMs = nowMs - evaluated_at_ms;
    }
    p.meta.updateSequence = evaluation;
    (void)DomainIf_TxCisPowerPermission(&p);
}
#endif
