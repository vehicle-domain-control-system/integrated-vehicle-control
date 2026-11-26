#ifndef NET_S32K144_PORT_H
#define NET_S32K144_PORT_H

#include "network/net_port.h"

/* S32K144 HAL binding owner. SDK/pin/clock/Driver lifetime remain OPEN. */
NetPort net_s32k144_port(void);

#endif
