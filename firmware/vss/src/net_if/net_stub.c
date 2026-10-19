#include "../infra/vss_config.h"

#if VSS_CFG_NET_STUB

#include <stddef.h>

#include "net_stub.h"

static NetVssStatus s_lastStatus;
static bool s_lastStatusPresent;
static uint32_t s_publishCount;

void Net_Init(void)
{
}

void Net_MainFunction(uint32_t nowMs)
{
    (void)nowMs;
}

NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)
{
    if (message == NULL)
    {
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    s_lastStatus = *message;
    s_lastStatusPresent = true;
    if (s_publishCount != UINT32_MAX)
    {
        ++s_publishCount;
    }
    return NET_SUBMIT_OK;
}

bool NetStub_ReadLastStatus(NetVssStatus *out)
{
    if ((out == NULL) || !s_lastStatusPresent)
    {
        return false;
    }
    *out = s_lastStatus;
    return true;
}

uint32_t NetStub_PublishCount(void)
{
    return s_publishCount;
}

#else

/* ISO C requires a translation unit to contain at least one declaration. */
typedef int net_stub_disabled;

#endif /* VSS_CFG_NET_STUB */
