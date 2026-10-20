/* Node profile table: CIS (CAN + UART-C to the Pi).
 * Policies below come from docs/network/interface:
 *   DOMAIN_PERMISSION 0x080 (CAN RX): period 50 ms / receive timeout 150 ms,
 *                            original Power life 500 ms; delivered to
 *                            CisApp_OnDomainPermission and relayed to the Pi
 *   P_VISION_RESULT  0x60 (UART-C RX): 100 / 300 ms, P_VISION_STATUS 0x61: 1000 /
 *                            3000 ms; their receive supervision is the CIS
 *                            application's, through the two callbacks
 *                            (02_NODES/CIS/20_API.md, receive delivery)
 *   UART-C stream          : 50 ms frame assembly bound (20_UART_M.md shared
 *                            framing, proposed), LINK_SEQUENCE from 1 per direction
 *   CIS_REAR 0x0A0, CIS_ENVIRONMENT 0x300, CIS_OCCUPANT 0x310, CIS_STATUS 0x320
 *                            (CAN TX): submitted by the CIS application
 *   P_PERMISSION 0x62 (UART-C TX): Network-internal relay, 500 ms repetition */
#include "net_node_profile.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
#include <string.h>

/* P_PERMISSION USE_LIMIT: the existing 1500 ms candidate is kept
 * (02_NODES/PI/10_DATA.md NetPiPermission; the 500 ms value is a conditional
 * profile). The service still applies the 500 ms original Power life. */
#define CIS_PI_USE_LIMIT_MS 1500u

/* M04 publish table of the CIS profile.
 * CIS_REAR / CIS_ENVIRONMENT / CIS_OCCUPANT / CIS_STATUS: 20_API.md does not
 * say the application calls these only on a new evaluation, so Network does
 * not repeat them (period 0): every submission is one frame, the application
 * keeps the 50 / 200 / 100 / 250 ms periods of 10_CAN_FD.md. A Network period
 * on top of an application that already submits periodically would double
 * the traffic; a missing frame shows up in the receiver's timeout instead.
 * Freshness bound of a submission: its AGE plus the local wait; the wire
 * saturates at 65535 and DOMAIN judges staleness.
 * P_PERMISSION: Network-internal relay (20_API.md CisNetPriv_RelayPiPermission):
 * a new DOMAIN evaluation is submitted at once, the same relay record is
 * repeated every 500 ms with the same UPDATE_SEQUENCE and a growing AGE
 * (30_UART_C.md: 500 ms repetition / 1500 ms receive timeout). The service
 * refuses any P_PERMISSION whose original DOMAIN AGE reached 500 ms or that no
 * longer matches the DOMAIN_PERMISSION receipt it holds, repetitions included. */
static const NetUpperPublishEntry publish_table[] = {
    { NET_WIRE_CAN, 0x0A0u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x300u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x310u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x320u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_UART_C, 0x62u, 500u, { true, 500u, true } }
};

static void upper(NetUpperConfig *c)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        if (r != NULL && r->link == NET_WIRE_CAN && r->id_type == 0x080u)
            c->rx_policy[i].maximum_age_ms = 500u;
    }
    /* UART-C receiver (Pi -> CIS) and the CIS -> Pi LINK_SEQUENCE. */
    c->uart_policy[NET_WIRE_UART_C - 1u].candidate_time_policy_selected = true;
    c->uart_policy[NET_WIRE_UART_C - 1u].assembly_limit_ms = 50u;
    c->uart_policy[NET_WIRE_UART_C - 1u].expires_at_equality = true;
    c->uart_sequence_bound[NET_WIRE_UART_C - 1u] = true;
    c->uart_initial_sequence[NET_WIRE_UART_C - 1u] = 1u;
}

static NetStatus bind(const NetNodeParts *parts)
{
    NetAppPortCisConfig app;
    memset(&app, 0, sizeof(app));
    app.publisher = parts->publisher;
    app.cis_boot_id = parts->config->boot_id; /* CIS_BOOT_ID: this node's Network boot */
    app.pi_use_limit_ms = CIS_PI_USE_LIMIT_MS;
    return net_app_port_cis_bind(parts->service, parts->ecu, &app);
}

/* Supervision view only; the relay runs through the facade and M04, not the
 * guard's own relay option. */
static void permission(NetPermissionPolicy *p) { p->relay_to_pi = false; }

static void diag(const NetUpperPublisher *publisher, NetNodeDiag *out)
{
    (void)net_app_port_cis_diag(&out->app);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x0A0u, &out->rear_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x300u, &out->environment_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x310u, &out->occupant_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x320u, &out->status_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_UART_C, 0x62u, &out->pi_permission_tx);
}

const NetNodeProfile net_node_profile = {
    NET_WIRE_NODE_CIS, false, true,
    net_app_port_cis_callbacks,
    upper,
    publish_table, sizeof(publish_table) / sizeof(publish_table[0]),
    bind,
    permission,
    diag,
    NULL
};

#else
/* Not the CIS node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_node_profile_cis_absent;
#endif
