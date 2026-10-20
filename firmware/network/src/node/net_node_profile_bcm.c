/* Node profile table: BCM (CAN only).
 * Policies below come from docs/network/interface:
 *   DOMAIN_PERMISSION 0x080 (RX): period 50 ms / receive timeout 150 ms,
 *                            original Power life 500 ms; BCM Network-private
 *                            supervision (02_NODES/BCM/20_API.md)
 *   BCM_DOOR_COMMAND 0x100, BCM_CLIMATE_COMMAND 0x110, BCM_LIGHT_COMMAND 0x120
 *                     (RX): on occurrence / stage change, no receive timeout
 *                            (03_COMMUNICATION/10_CAN_FD.md); common 2000 ms
 *                            delivery bound. ACCEPT_LIMIT (200 ms vs 1 s) is
 *                            open and not applied here.
 *   BCM_RESULT 0x140, BCM_EVENT 0x141 (TX): discrete, one frame per record
 *   BCM_DOOR_STATE 0x200, BCM_CLIMATE_STATE 0x210, BCM_LIGHT_STATE 0x220,
 *   BCM_STATUS 0x230 (TX): submitted by the BCM application */
#include "net_node_profile.h"

#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_BCM
#include <string.h>

/* M04 publish table of the BCM profile. Every entry has period 0: one frame
 * per submission, never repeated by the Network.
 * BCM_RESULT / BCM_EVENT: discrete records (10_CAN_FD.md: on occurrence /
 * stage change). They reach their slot one at a time through the App Port
 * store (net_app_port.h), never as a replaceable current value.
 * BCM_DOOR_STATE / CLIMATE_STATE / LIGHT_STATE / STATUS: 20_API.md asks the
 * application to submit each evaluation with its own UPDATE_SEQUENCE / AGE
 * and says repetitions keep them, but not that the application calls only on
 * a new evaluation and leaves the 100 / 200 / 200 / 250 ms periods of
 * 10_CAN_FD.md to the Network. So the application keeps the periods; a
 * Network period on top would double the traffic, while a missing frame
 * shows up in the DOMAIN receive timeout.
 * Freshness bound of a submission: its AGE plus the local wait; the wire
 * saturates at 65535 and DOMAIN judges staleness. */
static const NetUpperPublishEntry publish_table[] = {
    { NET_WIRE_CAN, 0x140u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x141u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x200u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x210u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x220u, 0u, { true, 65535u + 1000u, true } },
    { NET_WIRE_CAN, 0x230u, 0u, { true, 65535u + 1000u, true } }
};

/* DOMAIN_PERMISSION (50 / 150 ms, 10_CAN_FD.md): repetitions of the same
 * evaluation with a new TX_SEQUENCE reach BcmNetPriv_OnDomainPermission (the
 * supervision record). The commands (on occurrence / stage change, no
 * receive timeout) are never delivered twice for one command. */
static void upper(NetUpperConfig *c)
{
    size_t i;
    for (i = 0u; i < NET_UPPER_ROUTE_COUNT; ++i) {
        const NetWireRegistryEntry *r = net_wire_registry_at(i);
        if (r != NULL && r->link == NET_WIRE_CAN && r->id_type == 0x080u) {
            c->rx_policy[i].maximum_age_ms = 500u;
            c->rx_repeat_delivery[i] = true;
        }
    }
}

static NetStatus bind(const NetNodeParts *parts)
{
    NetAppPortBcmConfig app;
    memset(&app, 0, sizeof(app));
    app.publisher = parts->publisher;
    app.bcm_boot_id = parts->config->boot_id; /* command TARGET_BOOT_ID */
    return net_app_port_bcm_bind(parts->service, parts->ecu, &app);
}

static void permission(NetPermissionPolicy *p) { (void)p; } /* supervision only */

static void diag(const NetUpperPublisher *publisher, NetNodeDiag *out)
{
    (void)net_app_port_bcm_diag(&out->app);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x140u, &out->result_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x141u, &out->event_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x200u, &out->door_state_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x210u, &out->climate_state_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x220u, &out->light_state_tx);
    (void)net_upper_publish_view(publisher, NET_WIRE_CAN, 0x230u, &out->status_tx);
}

const NetNodeProfile net_node_profile = {
    NET_WIRE_NODE_BCM, false, false,
    net_app_port_bcm_callbacks,
    upper,
    publish_table, sizeof(publish_table) / sizeof(publish_table[0]),
    bind,
    permission,
    diag,
    net_app_port_bcm_service
};

#else
/* Not the BCM node profile: nothing is compiled (ISO C needs one declaration
 * per translation unit). */
typedef int net_node_profile_bcm_absent;
#endif
