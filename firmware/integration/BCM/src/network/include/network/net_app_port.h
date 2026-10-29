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

/* Discrete records (Result / Event) in front of one M04 slot.
 * A latest-value slot must not hold discrete traffic: a newer submission
 * would replace an older one no device has taken (net_upper_publish.h;
 * 04_COMMON/20_API_USAGE_AND_FAILURE.md and the node 20_API.md "discrete
 * result / occurrence": keep every record with its original identity, never
 * solve a lack of room by silently dropping another one). So a record goes
 * to the message's M04 slot only while that slot is idle (nothing waiting,
 * no unsettled frame); otherwise it waits here, in call order, with the
 * stamp of its submission so M04 still adds the whole local wait to its AGE
 * once. A full store refuses the new record (NO_CAPACITY); nothing kept is
 * overwritten. M04 keeps owning BOOT_ID / TX_SEQUENCE of every frame.
 * Bounded implementation choice, not an approved ECU sizing. */
#include "network/net_upper_publish.h"
#ifndef NET_APP_PORT_DISCRETE_DEPTH
#define NET_APP_PORT_DISCRETE_DEPTH 4u
#endif
typedef struct {
    NetUpperCanMessage message; /* content; BOOT_ID / TX_SEQUENCE are M04's */
    NetSourceStamp source;      /* submission time and the submitted AGE */
} NetAppPortDiscreteRecord;
typedef struct {
    uint16_t id;                /* CAN identifier of the message */
    size_t head, count;
    uint64_t last_observation;
    NetAppPortDiscreteRecord record[NET_APP_PORT_DISCRETE_DEPTH];
    uint32_t queued;            /* records that waited here behind an unsettled one */
    uint32_t queue_full;        /* submissions refused: store full (NO_CAPACITY) */
    uint32_t offer_refused;     /* waiting records M04 refused for good (e.g. expired) */
    uint32_t depth_max;
} NetAppPortDiscreteQueue;
void net_app_port_discrete_init(NetAppPortDiscreteQueue *q, uint16_t id);
/* true: nothing waits here and the message's M04 slot is idle, so the record
 * can go to M04 at once (net_upper_publish_can with a local stamp). */
bool net_app_port_discrete_direct(const NetAppPortDiscreteQueue *q, const NetUpperPublisher *p);
/* Keep a record that cannot go to M04 yet. now_ms: the submission time on
 * the service clock (the step's clock reading inside an RX callback).
 * NO_CAPACITY when full; HOLD when the local stamp numbering is exhausted. */
NetStatus net_app_port_discrete_enqueue(NetAppPortDiscreteQueue *q, const NetUpperCanMessage *m, uint64_t now_ms);
/* Once per superloop pass, outside the service step and before
 * net_upper_publish_service(): hands the oldest waiting record to the idle
 * M04 slot. A link that is down (HOLD) keeps the records waiting. */
void net_app_port_discrete_service(NetAppPortDiscreteQueue *q, NetUpperPublisher *p);

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
