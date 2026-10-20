#ifndef NETWORK_TEST_VIRTUAL_ECU_H
#define NETWORK_TEST_VIRTUAL_ECU_H
#include "network/net_upper.h"
NetUpper *virtual_service(void);
uint64_t virtual_now(void);
NetStatus virtual_send_can(const NetUpperCanMessage *);
NetStatus virtual_send_uart(const NetUpperUartMessage *);
#endif
