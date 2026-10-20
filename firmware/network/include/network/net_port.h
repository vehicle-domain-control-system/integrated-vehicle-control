#ifndef NETWORK_NET_PORT_H
#define NETWORK_NET_PORT_H

#include "network/net_types.h"

typedef enum { NET_PORT_RX, NET_PORT_TX, NET_PORT_ERROR } NetPortEventKind;
typedef struct {
    NetTxKey key;
    bool admission_observed;
    bool accepted;
    bool release_observed;
    NetTxOutcome outcome;
} NetTxEvent;
typedef struct {
    NetPortEventKind kind;
    NetFrame frame;
    NetTxEvent tx;
    uint32_t error_code;
} NetPortEvent;
typedef struct {
    NetStatus (*now_ms)(void *context, uint64_t *out);
    /* OK means a borrow has begun, NOT admission or final TX success.
       Non-OK guarantees no retained pointer and no later event for this call.
       The frame stays immutable until a matching release event is consumed. */
    NetStatus (*submit)(void *context, const NetFrame *frame, NetTxKey key);
    /* Copy one captured event. NO_WORK leaves out untouched. No callbacks. */
    NetStatus (*poll)(void *context, NetPortEvent *out);
} NetPortOps;
typedef struct { const NetPortOps *ops; void *context; } NetPort;

NetStatus net_port_validate(const NetPort *port);
NetStatus net_port_now(const NetPort *port, uint64_t *out);
NetStatus net_port_submit(const NetPort *port, const NetFrame *frame, NetTxKey key);
NetStatus net_port_poll(const NetPort *port, NetPortEvent *out);

#endif
