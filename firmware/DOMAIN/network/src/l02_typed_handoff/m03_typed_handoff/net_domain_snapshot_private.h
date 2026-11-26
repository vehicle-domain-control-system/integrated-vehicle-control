#ifndef NET_DOMAIN_SNAPSHOT_PRIVATE_H
#define NET_DOMAIN_SNAPSHOT_PRIVATE_H

#include "network/net_domain_c2.h"

/* DOMAIN/board integration only; does not alter the C2 or wire API. */
void NetDomainSnapshot_Bind(void);
bool NetDomainSnapshot_Begin(NetDomainC2 *adapter,
    const NetUpperCanMessage *message, const NetUpperDelivery *delivery);
void NetDomainSnapshot_End(NetStatus status);
bool NetDomainSnapshot_Confirm(void *context, uint16_t id, uint64_t receipt,
    uint32_t boot, uint32_t sequence);
/* DOMAIN composition boundary: revalidate addressed monitoring/query frames
 * immediately before the existing driver port can borrow the packet. */
NetStatus NetDomainSnapshot_Submit(const NetUpperPortOps *delegate,
    void *context, const NetUpperPacket *packet, NetTxKey key,
    uint32_t domainBoot);

#endif
