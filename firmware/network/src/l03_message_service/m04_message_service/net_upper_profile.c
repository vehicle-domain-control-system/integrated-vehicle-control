#include "net_upper_private.h"

bool net_upper_profile_link(NetWireLink link)
{
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST
    return link==NET_WIRE_CAN || link==NET_WIRE_UART_M || link==NET_WIRE_UART_C;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    return link==NET_WIRE_CAN || link==NET_WIRE_UART_C;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_DOMAIN
    return link==NET_WIRE_CAN || link==NET_WIRE_UART_M;
#else
    return link==NET_WIRE_CAN;
#endif
}

bool net_upper_profile_matches(uint32_t node)
{
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST
    return node==NET_WIRE_NODE_DOMAIN || node==NET_WIRE_NODE_BCM ||
        node==NET_WIRE_NODE_CIS || node==NET_WIRE_NODE_VSS ||
        node==NET_WIRE_NODE_WINDOW || node==NET_WIRE_NODE_ESP32 ||
        node==NET_WIRE_NODE_PI;
#else
    return node==NET_UPPER_PROFILE_NODE;
#endif
}

size_t net_upper_admission_index(size_t registry_index)
{
    const NetWireRegistryEntry *entry=net_wire_registry_at(registry_index);
    if(entry==NULL)return SIZE_MAX;
#if NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST
    return registry_index<NET_UPPER_ADMISSION_CAPACITY?registry_index:SIZE_MAX;
#else
    size_t i,rank=0u;
    if((entry->receivers&NET_UPPER_PROFILE_NODE)==0u || !net_upper_profile_link(entry->link))return SIZE_MAX;
    for(i=0u;i<registry_index;++i) {
        const NetWireRegistryEntry *previous=net_wire_registry_at(i);
        if(previous!=NULL && (previous->receivers&NET_UPPER_PROFILE_NODE)!=0u && net_upper_profile_link(previous->link))++rank;
    }
    return rank<NET_UPPER_ADMISSION_CAPACITY?rank:SIZE_MAX;
#endif
}

const NetUpperAdmission *net_upper_admission_at(const NetUpper *s,size_t registry_index)
{
    size_t index=net_upper_admission_index(registry_index);
    return s!=NULL && index<NET_UPPER_ADMISSION_CAPACITY?&s->admission[index]:NULL;
}
NetUpperAdmission *net_upper_admission_at_mut(NetUpper *s,size_t registry_index)
{
    size_t index=net_upper_admission_index(registry_index);
    return s!=NULL && index<NET_UPPER_ADMISSION_CAPACITY?&s->admission[index]:NULL;
}

const NetUpperUartReceiver *net_upper_receiver(const NetUpper *s,size_t logical_uart_index)
{
#if NET_UPPER_UART_RECEIVER_CAPACITY == 0
    (void)s;(void)logical_uart_index;return NULL;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST
    return s!=NULL && logical_uart_index<2u?&s->uart_rx[logical_uart_index]:NULL;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    return s!=NULL && logical_uart_index==1u?&s->uart_rx[0]:NULL;
#else
    return s!=NULL && logical_uart_index==0u?&s->uart_rx[0]:NULL;
#endif
}
NetUpperUartReceiver *net_upper_receiver_mut(NetUpper *s,size_t logical_uart_index)
{
#if NET_UPPER_UART_RECEIVER_CAPACITY == 0
    (void)s;(void)logical_uart_index;return NULL;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_HOST
    return s!=NULL && logical_uart_index<2u?&s->uart_rx[logical_uart_index]:NULL;
#elif NETWORK_UPPER_NODE_PROFILE == NET_UPPER_PROFILE_CIS
    return s!=NULL && logical_uart_index==1u?&s->uart_rx[0]:NULL;
#else
    return s!=NULL && logical_uart_index==0u?&s->uart_rx[0]:NULL;
#endif
}
