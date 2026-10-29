#include "net_observation.h"
#include "network/net_runtime.h"

void net_observe_increment(uint32_t *counter)
{
    if (*counter != UINT32_MAX) ++*counter;
}
NetStatus net_runtime_observation(const NetRuntime *runtime, NetObservation *out)
{
    if (runtime == NULL || !runtime->initialized || out == NULL)
        return NET_INVALID_ARGUMENT;
    *out = runtime->observation;
    return NET_OK;
}
