/* Network side of the DOMAIN board project. See net_domain_board.h. */
#include "net_domain_board.h"
#include <string.h>
#include "network/net_boot_id.h"

/* BOOT_ID: the product source is not decided (include/network/net_boot_id.h
 * also takes a noinit record and the reset cause; the record section and the
 * reset cause are board work not done yet). Until then the value comes from
 * the core cycle counter (board_boot_seed), so it changes on every warm reset
 * such as the reset pin and receivers take the restarted TX_SEQUENCE of every
 * DOMAIN frame as a new run (docs/network/interface/03_COMMUNICATION/10_CAN_FD.md common header:
 * BOOT_ID belongs to the sending Network, 0 reserved; it changes on every
 * start). DOMAIN_BOARD_BOOT_ID is mixed into the seed and is the value with
 * NET_BOARD_BOOT_ID_FIXED. */
#define DOMAIN_BOARD_BOOT_ID 0x444F4D42u

#ifndef NET_BOARD_BOOT_ID_FIXED
/* BOOT_ID seed (temporary, until a product source such as an NVM reset counter
 * or a hardware RNG): the core cycle counter DWT_CYCCNT. It belongs to the debug
 * domain of the core, which a warm (functional) reset such as the reset pin
 * does not clear, so the value read here carries the length of the earlier
 * runs and changes on every such reset. A power-on reset, and a destructive
 * reset (on the S32K344 some software and debugger resets), starts it again:
 * the value is then the one of every power-on start, and a peer that saw it
 * earlier drops the frames as an old run until the board is reset once more.
 * Fixed ARMv7-M addresses, no SDK header. Other builds (host): 0.
 * NET_BOARD_BOOT_ID_FIXED keeps the bench constant. */
static uint32_t board_boot_seed(void)
{
#if defined(__ARM_ARCH_PROFILE) && (__ARM_ARCH_PROFILE == 'M')
    volatile uint32_t *const demcr = (volatile uint32_t *)0xE000EDFCu;
    volatile uint32_t *const dwt_ctrl = (volatile uint32_t *)0xE0001000u;
    volatile uint32_t *const dwt_cyccnt = (volatile uint32_t *)0xE0001004u;
    *demcr |= 1u << 24;                                  /* DEMCR.TRCENA: DWT on */
    *(volatile uint32_t *)0xE0001FB0u = 0xC5ACCE55u;     /* DWT_LAR: unlock (Cortex-M7 software lock) */
    if ((*dwt_ctrl & (1u << 25)) != 0u) return 0u;       /* DWT_CTRL.NOCYCCNT: no cycle counter */
    *dwt_ctrl |= 1u;                                     /* CYCCNTENA (kept when already on) */
    return *dwt_cyccnt;
#else
    return 0u;
#endif
}
#endif

bool NetDomainBoard_NodeConfig(NetNodeConfig *out)
{
    if (out == NULL) return false;
#ifdef NET_BOARD_BOOT_ID_FIXED
    out->boot_id = DOMAIN_BOARD_BOOT_ID; /* test-bench constant */
#else
    {
        /* No record kept across resets: the seed alone gives the value (never 0). */
        NetBootIdRecord none = { 0u, 0u, 0u };
        if (net_boot_id_next(&none, NET_BOOT_RESET_POWER_ON, board_boot_seed() ^ DOMAIN_BOARD_BOOT_ID,
                             &out->boot_id) != NET_OK)
            return false;
    }
#endif
    /* epochs: Net_Init defaults (1) */
    return true;
}

/* The capture stamp is already on the millisecond axis of the DOMAIN main loop
 * (net_hal_rtd_last_now_ms32). */
uint32_t NetDomainBoard_TickToMs(uint32_t tick)
{
    return tick;
}

/* CIS transition binding handed over by the DOMAIN integration
 * (NetDomainBoard_SetTransitionBinding); empty until then. */
static NetDomainC2TransitionBinding transition_binding;

void NetDomainBoard_SetTransitionBinding(const NetDomainC2TransitionBinding *binding)
{
    if (binding == NULL) memset(&transition_binding, 0, sizeof(transition_binding));
    else transition_binding = *binding;
}

/* C2 boundary: the pinned DOMAIN functions (net_domain_c2_pinned_owner_api).
 * Semantic callbacks: the four provisional rules of this board, the value
 * quality of VALIDITY / REASON, the Power / Operation bundle of
 * DOMAIN_PERMISSION, the status report quality (OK for an admitted
 * BCM_STATUS / CIS_STATUS / VSS_STATUS, DOMAIN judges freshness on the AGE)
 * and the local failure reason of a command, VSS_EVENT or M_RESULT refused
 * before the device took it (by its cause: context invalidated or driver
 * submission final, otherwise held); without them CIS_REAR reception,
 * DOMAIN_PERMISSION transmission and the three status reports are held, and
 * such a refused record keeps its C2 and M04 TX slots. The other five stay
 * NULL, and their routes are held until the DOMAIN team decides them:
 * window_permission_bundle, window_alive_bundle, window_event_projection,
 * select_query_plan, confirm_snapshot. Limits (10_CAN_FD.md): ACCEPT_LIMIT is
 * the 200 ms candidate (the 1 s conflict is OPEN), VSS_EVENT USE_LIMIT the
 * 500 ms draft. The node root sets BOOT_ID, TX_SEQUENCE start, the M04 slots,
 * the command target and the CIS transition barriers itself. The transition
 * binding is the one DOMAIN handed over (none: CIS_ENVIRONMENT and
 * CIS_OCCUPANT stay held). */
bool NetDomainBoard_C2Config(NetDomainC2Api *api, NetDomainC2Config *config)
{
    if (api == NULL || config == NULL) return false;
    net_domain_c2_pinned_owner_api(api);
    memset(config, 0, sizeof(*config));
    config->value_quality = net_domain_c2_value_quality_provisional;
    config->permission_bundle = net_domain_c2_permission_bundle_provisional;
    config->status_quality = net_domain_c2_status_quality_provisional;
    config->local_failure_reason = net_domain_c2_local_failure_reason_provisional;
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
    config->transition = transition_binding;
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
