/* Node profile table: VSS (CAN only).
 * Policies below come from docs/network/interface:
 *   DOMAIN_PERMISSION 0x080 : period 50 ms / RX timeout 150 ms, original Power life 500 ms
 *   VSS_EVENT        0x160 : discrete; original-age cap 2000 ms (L = min(USE_LIMIT,500,2000)
 *                            is applied by the VSS application, not here)
 *   VSS_WARNING_STATE 0x0C0: per-lane ages (per-field mode); local delivery bound only
 *   VSS_STATUS (TX)  0x340 : freshness from source_age_ms; receiver judges staleness;
 *                            one frame per submission, never repeated (table below) */
#include "net_node_profile.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_VSS
#include <string.h>

/* M04 publish table of the VSS profile: messages whose BOOT_ID / TX_SEQUENCE
 * and waiting AGE Network owns, and their repetition period.
 * VSS_STATUS is submitted by the VSS application itself: immediately on every
 * change and otherwise as a new evaluation every 250 ms (firmware/vss
 * README §6 submission rule; 10_CAN_FD.md: 250 ms period / 750 ms receive
 * timeout). Network therefore sends one frame per submission and registers
 * no period here; a period would put a second frame on the bus each status
 * period. Freshness bound of a submission: source_age_ms plus the local wait;
 * the wire saturates at 65535 and DOMAIN judges staleness. */
static const NetUpperPublishEntry publish_table[] = {
    { NET_WIRE_CAN, 0x340u, 0u, { true, 65535u + 1000u, true } }
};

static void upper(NetUpperConfig *c)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        if (r == NULL || r->link != NET_WIRE_CAN) continue;
        if (r->id_type == 0x080u) c->rx_policy[i].maximum_age_ms = 500u;
        if (r->id_type == 0x0C0u) {
            c->rx_per_field_age[i] = true;          /* three independent lanes + activation */
            c->rx_policy[i].maximum_age_ms = 1000u; /* local capture->delivery bound */
        }
    }
}

static NetStatus bind(const NetNodeParts *parts)
{
    NetAppPortVssConfig app;
    memset(&app, 0, sizeof(app));
    app.publisher = parts->publisher;
    return net_app_port_vss_bind(parts->service, parts->ecu, &app);
}

static void permission(NetPermissionPolicy *p) { (void)p; } /* supervision only */

static void diag(const NetUpperPublisher *publisher, NetNodeDiag *out)
{
    (void)net_app_port_vss_diag(&out->app);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x340u, &out->status_tx);
}

const NetNodeProfile net_node_profile = {
    NET_WIRE_NODE_VSS, false, false,
    net_app_port_vss_callbacks,
    upper,
    publish_table, sizeof(publish_table) / sizeof(publish_table[0]),
    bind,
    permission,
    diag,
    NULL
};

#else
/* Not the VSS node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_node_profile_vss_absent;
#endif
