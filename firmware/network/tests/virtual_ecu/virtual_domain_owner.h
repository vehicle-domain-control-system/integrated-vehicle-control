#ifndef VIRTUAL_DOMAIN_OWNER_H
#define VIRTUAL_DOMAIN_OWNER_H
#include "network/net_upper.h"
NetStatus virtual_domain_init(NetUpper *,NetUpperOwner *);
NetStatus virtual_domain_start(void);
void virtual_domain_step(uint64_t);
NetStatus virtual_domain_command(const char *);
#endif
