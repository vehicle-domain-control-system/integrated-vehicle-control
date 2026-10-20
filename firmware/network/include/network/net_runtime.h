#ifndef NETWORK_NET_RUNTIME_H
#define NETWORK_NET_RUNTIME_H
#include "network/net_port.h"
#include "network/net_source.h"

/* HOST scaffold capacities only, not approved ECU queue sizing. */
#define NET_RX_CAPACITY 8u
#define NET_TX_CAPACITY 4u
#define NET_POLL_BUDGET 8u
typedef struct {
    uint32_t rx_captured, rx_dropped, rx_invalid, rx_delivered, rx_held;
    uint32_t consumer_rejected, tx_submitted, tx_admitted, tx_rejected;
    uint32_t tx_succeeded, tx_failed, tx_released, duplicate_events;
    uint32_t stale_events, conflicting_events, port_errors;
} NetObservation;
typedef struct {
    bool used;
    NetTxKey key;
    NetSourceStamp source;
    NetFrame frame;
    bool admission_known, accepted, reference_released, conflict;
    NetTxOutcome outcome;
} NetTxRecord;
/* Borrowed only during callback, owner/superloop context. Spy for this scaffold;
   returns local consumer status, never remote execution or C2 commit proof. */
typedef NetStatus (*NetFrameConsumer)(void *context, const NetFrame *frame);
typedef struct {
    NetPort port;
    uint64_t local_epoch;
    NetFrameConsumer consumer;
    void *consumer_context;
    bool host_fixture_enabled;
} NetRuntimeConfig;
typedef struct {
    NetRuntimeConfig config;
    NetFrame rx[NET_RX_CAPACITY];
    size_t rx_head, rx_count;
    NetTxRecord tx[NET_TX_CAPACITY];
    uint64_t next_token, last_now_ms;
    bool clock_seen, initialized, in_call;
    NetObservation observation;
} NetRuntime;

/* Initialize fresh storage only. Never reinitialize a live/borrowed instance.
   Each new lifecycle requires a unique nonzero local_epoch from its owner. */
NetStatus net_runtime_init(NetRuntime *runtime, const NetRuntimeConfig *config);
/* Serialized capture copies bytes+original time; not an IRQ-safe ring yet.
   Real ISR critical sections/atomics remain a board binding requirement. */
NetStatus net_runtime_capture_rx(NetRuntime *runtime, const NetFrame *frame);
/* Bounded polling then bounded consumption, always outside a real ISR. */
NetStatus net_runtime_step(NetRuntime *runtime, size_t receive_budget);
NetStatus net_runtime_submit(NetRuntime *runtime, const NetFrame *frame,
                             const NetSourceStamp *source,
                             const NetAgePolicy *policy, NetTxKey *out_key);
NetStatus net_runtime_tx_snapshot(const NetRuntime *runtime, NetTxKey key,
                                  NetTxRecord *out);
/* Explicit local observer acknowledgement. Requires known admission,
   terminal (if accepted), release and no contradictory evidence. */
NetStatus net_runtime_retire(NetRuntime *runtime, NetTxKey key);
NetStatus net_runtime_observation(const NetRuntime *runtime, NetObservation *out);
#endif
