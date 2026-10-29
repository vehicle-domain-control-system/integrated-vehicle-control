#include "net_handoff.h"

NetStatus net_handoff_deliver(const NetRuntimeConfig *config, const NetFrame *frame)
{
    if (config == NULL || frame == NULL) return NET_INVALID_ARGUMENT;
    if (!config->host_fixture_enabled || config->consumer == NULL) return NET_HOLD;
    return config->consumer(config->consumer_context, frame);
}
