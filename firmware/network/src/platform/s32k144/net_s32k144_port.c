#include "net_s32k144_port.h"

static NetStatus s32k144_now(void *context, uint64_t *out)
{
    (void)context;
    (void)out;
    return NET_NOT_CONFIGURED;
}

static NetStatus s32k144_submit(void *context, const NetFrame *frame, NetTxKey key)
{
    (void)context;
    (void)frame;
    (void)key;
    return NET_NOT_CONFIGURED;
}

static NetStatus s32k144_poll(void *context, NetPortEvent *out)
{
    (void)context;
    (void)out;
    return NET_NOT_CONFIGURED;
}

NetPort net_s32k144_port(void)
{
    static const NetPortOps ops = {s32k144_now, s32k144_submit, s32k144_poll};
    NetPort port = {&ops, NULL};
    return port;
}
