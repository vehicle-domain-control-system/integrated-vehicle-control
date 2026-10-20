#ifndef NETWORK_NET_APP_PORT_H
#define NETWORK_NET_APP_PORT_H

/* L02/M03 Application Port common helpers (Network-internal).
 * The per-node facades (net_app_port_<node>.c) implement the interface-doc
 * prototypes on top of the typed ECU adapter. Applications do not include
 * this header; they include network/app/net_if_<node>.h only. */
#include "network/net_upper.h"
#include "network/app/net_if_common.h"

/* Engine NetStatus -> contract NetSubmitResult (04_COMMON/20_API_USAGE_AND_FAILURE).
 *   OK                                           -> OK
 *   NO_CAPACITY                                  -> NO_CAPACITY
 *   NOT_CONFIGURED, HOLD, BUSY, NO_WORK          -> NOT_READY
 *   INVALID_ARGUMENT, REJECTED, UNSUPPORTED,
 *   EXPIRED, STALE, DUPLICATE                    -> INVALID_ARGUMENT
 *     (retrying the same data cannot succeed) */
NetSubmitResult net_app_port_submit_result(NetStatus status);

/* RX meta per contract: link identity from the wire header and the local
 * RX capture time (RX basis; payload ages are NOT advanced to delivery). */
void net_app_port_rx_meta(uint32_t boot_id, uint32_t tx_sequence,
                          const NetUpperDelivery *delivery, NetCanRxMeta *out);
/* UART RX meta per contract: the frame's LINK_SEQUENCE and the local capture
 * time of its last byte (same RX basis as the CAN meta). */
void net_app_port_uart_rx_meta(const NetUpperDelivery *delivery, NetUartRxMeta *out);

/* Bounded tracking of submitted TX attempts so finished slots are retired.
 * Retirement only follows the engine rule (released + admission known +
 * final outcome or not accepted). Nothing is retired early. */
typedef struct {
    bool used[NET_UPPER_TX_CAPACITY];
    NetTxKey key[NET_UPPER_TX_CAPACITY];
    uint32_t retired, untracked_full;
} NetAppPortTxTracker;
void net_app_port_track(NetAppPortTxTracker *t, NetTxKey key);
void net_app_port_retire_completed(NetAppPortTxTracker *t, NetUpper *service);
/* Cancel a still-queued (never submitted) attempt. true when cancelled. */
bool net_app_port_cancel_if_unsent(NetUpper *service, NetTxKey key);

#endif
