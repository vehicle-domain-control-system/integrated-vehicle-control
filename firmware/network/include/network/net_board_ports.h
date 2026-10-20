#ifndef NETWORK_NET_BOARD_PORTS_H
#define NETWORK_NET_BOARD_PORTS_H

#include "network/net_port.h"

/* Fail-closed boundaries only. Both targets remain BINDING_OPEN.
   No peripheral is configured, no submission pointer is retained, and every
   operation returns NET_NOT_CONFIGURED without changing output arguments. */
NetPort net_s32k144_port(void);
NetPort net_s32k344_port(void);

#endif
