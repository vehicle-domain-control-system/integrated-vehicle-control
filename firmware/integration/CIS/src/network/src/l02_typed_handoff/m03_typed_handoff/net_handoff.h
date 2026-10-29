#ifndef NETWORK_PRIVATE_HANDOFF_H
#define NETWORK_PRIVATE_HANDOFF_H
#include "network/net_runtime.h"
NetStatus net_handoff_deliver(const NetRuntimeConfig *config, const NetFrame *frame);
#endif
