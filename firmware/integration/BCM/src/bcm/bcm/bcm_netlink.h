#ifndef BCM_NETLINK_H
#define BCM_NETLINK_H

#include <stdbool.h>

void BcmNetLink_Refresh(void);

bool BcmNetLink_InitOk(void);

bool BcmNetLink_DomainKnown(void);

bool BcmNetLink_DomainAlive(void);

#endif
