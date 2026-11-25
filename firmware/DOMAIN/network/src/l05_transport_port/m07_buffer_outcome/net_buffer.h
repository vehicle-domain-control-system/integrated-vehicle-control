#ifndef NETWORK_PRIVATE_BUFFER_H
#define NETWORK_PRIVATE_BUFFER_H
#include "network/net_runtime.h"
NetStatus net_buffer_capture(NetRuntime *runtime, const NetFrame *frame);
NetStatus net_buffer_pop(NetRuntime *runtime, NetFrame *out);
NetTxRecord *net_buffer_find(NetRuntime *runtime, NetTxKey key);
NetStatus net_buffer_observe_tx(NetRuntime *runtime, const NetTxEvent *event);
#endif
