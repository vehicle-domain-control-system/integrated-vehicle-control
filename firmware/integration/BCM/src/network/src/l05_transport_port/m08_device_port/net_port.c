#include "network/net_port.h"

NetStatus net_port_validate(const NetPort *port)
{
    if (port == NULL || port->ops == NULL || port->ops->now_ms == NULL ||
        port->ops->submit == NULL || port->ops->poll == NULL)
        return NET_INVALID_ARGUMENT;
    return NET_OK;
}
NetStatus net_port_now(const NetPort *port, uint64_t *out)
{
    if (net_port_validate(port) != NET_OK || out == NULL) return NET_INVALID_ARGUMENT;
    return port->ops->now_ms(port->context, out);
}
NetStatus net_port_submit(const NetPort *port, const NetFrame *frame, NetTxKey key)
{
    if (net_port_validate(port) != NET_OK || frame == NULL ||
        frame->length > NET_FRAME_CAPACITY || key.epoch == 0u || key.token == 0u)
        return NET_INVALID_ARGUMENT;
    return port->ops->submit(port->context, frame, key);
}
NetStatus net_port_poll(const NetPort *port, NetPortEvent *out)
{
    if (net_port_validate(port) != NET_OK || out == NULL) return NET_INVALID_ARGUMENT;
    return port->ops->poll(port->context, out);
}
