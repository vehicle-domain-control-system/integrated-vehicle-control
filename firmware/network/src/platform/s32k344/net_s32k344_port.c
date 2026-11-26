#include "net_s32k344_port.h"

static NetStatus s32k344_now(void *context, uint64_t *out)
{
    (void)context;
    (void)out;
    return NET_NOT_CONFIGURED;
}

static NetStatus s32k344_submit(void *context, const NetFrame *frame, NetTxKey key)
{
    (void)context;
    (void)frame;
    (void)key;
    return NET_NOT_CONFIGURED;
}

static NetStatus s32k344_poll(void *context, NetPortEvent *out)
{
    (void)context;
    (void)out;
    return NET_NOT_CONFIGURED;
}

NetPort net_s32k344_port(void)
{
    static const NetPortOps ops = {s32k344_now, s32k344_submit, s32k344_poll};
    NetPort port = {&ops, NULL};
    return port;
}
