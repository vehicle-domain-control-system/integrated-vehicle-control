#include "../infra/vss_config.h"

#if VSS_CFG_NET_STUB

#include <stddef.h>

#include "net_stub.h"

static NetVssStatus s_lastStatus;
static bool s_lastStatusPresent;
static uint32_t s_publishCount;
#if VSS_CFG_HOST_TEST
static NetSubmitResult s_forcedResult = NET_SUBMIT_OK;
static uint32_t s_forcedCount;
static uint32_t s_attemptCount;
#endif

void Net_Init(void)
{
}

void Net_MainFunction(uint32_t nowMs)
{
    (void)nowMs;
}

NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)
{
#if VSS_CFG_HOST_TEST
    if (s_attemptCount != UINT32_MAX)
    {
        ++s_attemptCount;
    }
#endif
    if (message == NULL)
    {
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
#if VSS_CFG_HOST_TEST
    if (s_forcedCount != 0u)
    {
        --s_forcedCount;
        return s_forcedResult;
    }
#endif
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

#if VSS_CFG_HOST_TEST
void NetStub_RefuseForTest(NetSubmitResult result, uint32_t count)
{
    s_forcedResult = result;
    s_forcedCount = count;
}

uint32_t NetStub_AttemptCount(void)
{
    return s_attemptCount;
}

void NetStub_ResetForTest(void)
{
    s_lastStatusPresent = false;
    s_publishCount = 0u;
    s_forcedResult = NET_SUBMIT_OK;
    s_forcedCount = 0u;
    s_attemptCount = 0u;
}
#endif

#else

/* ISO C requires a translation unit to contain at least one declaration. */
typedef int net_stub_disabled;

#endif /* VSS_CFG_NET_STUB */
