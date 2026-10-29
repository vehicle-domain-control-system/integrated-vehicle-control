#include <stdint.h>
#include "bcm_netlink.h"
#include "bcm_config.h"
#include "network/net_node.h"

static NetNodeDiag s_diag;
static bool        s_diagValid;

void BcmNetLink_Refresh(void)
{
    s_diagValid = (NetNode_GetDiag(&s_diag) == NET_OK);
}

bool BcmNetLink_InitOk(void)
{
    return s_diagValid && (s_diag.entry_init_result == (uint8_t)NET_ENTRY_INIT_OK)
        && (s_diag.stage == NET_NODE_STAGE_READY);
}

bool BcmNetLink_DomainKnown(void)
{
    return s_diagValid && s_diag.permission.has_original;
}

static bool SilenceIsMeasured(NetPermissionReason reason)
{
    switch (reason)
    {
    case NET_PERMISSION_NO_DATA:
    case NET_PERMISSION_CLOCK_HOLD:
    case NET_PERMISSION_LINK_UNAVAILABLE:
    case NET_PERMISSION_OWNER_FAILED:
    case NET_PERMISSION_OWNER_NO_DATA:
        return false;
    default:
        return true;
    }
}

bool BcmNetLink_DomainAlive(void)
{
    const NetPermissionView *view = &s_diag.permission;
    if (!BcmNetLink_DomainKnown() || !SilenceIsMeasured(view->reason))
    {
        return false;
    }
    return view->receive_silence_ms < (uint64_t)BCM_PERMISSION_TIMEOUT_MS;
}
