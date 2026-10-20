#include "network/net_upper.h"
void net_upper_observe_increment(uint32_t *value)
{ if (value != NULL && *value != UINT32_MAX) ++*value; }
NetStatus net_upper_observation(const NetUpper *service, NetUpperObservation *out)
{
    if(service==NULL || !service->initialized || out==NULL ||
       net_wire_ranges_overlap(service,sizeof(*service),out,sizeof(*out))) return NET_INVALID_ARGUMENT;
    *out=service->observation; return NET_OK;
}
