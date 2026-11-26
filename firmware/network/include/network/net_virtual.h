#ifndef NETWORK_NET_VIRTUAL_H
#define NETWORK_NET_VIRTUAL_H
#include "network/net_hal_adapter.h"

/* Host driver replacement, never linked into a target build. Single-threaded
 * worker: enter/leave track ordering by depth, they are NOT an IRQ lock. */
typedef NetStatus (*NetVirtualWire)(void *, const NetUpperPacket *, uint64_t);
typedef void (*NetVirtualFact)(void *, NetTxKey, NetWireLink, unsigned,
                               NetStatus, uint64_t);
typedef struct {
    uint64_t admission_delay, wire_delay, final_delay, release_delay;
} NetVirtualTiming;
typedef struct {
    const NetHalTransfer *borrow;
    NetTxKey key;
    NetWireLink link;
    uint64_t epoch, submitted_at;
    unsigned phase;
    bool active, wire_sent;
} NetVirtualPending;
typedef struct {
    NetHalAdapter *adapter;
    uint64_t now_ms;
    uint64_t epoch[3];
    uint32_t enabled_links;
    unsigned critical_depth;
    bool initialized, ticking, connected[3];
    NetVirtualTiming timing;
    NetVirtualWire wire;
    NetVirtualFact fact;
    void *context;
    NetVirtualPending pending[NET_HAL_TX_CAPACITY];
    uint32_t submissions, wire_frames, releases, driver_rejections;
} NetVirtualBackend;

/* Fresh storage. Call before net_hal_adapter_init, then use returned config.
 * wire() transports bytes only. Its OK means physical bus enqueue, not peer
 * decode/Owner acceptance. All four TX phases occur in separate tick calls. */
NetStatus net_virtual_init(NetVirtualBackend *, NetHalAdapter *, uint32_t,
    uint64_t, uint64_t, NetVirtualWire, NetVirtualFact, void *);
NetHalConfig net_virtual_hal_config(NetVirtualBackend *);
NetStatus net_virtual_set_timing(NetVirtualBackend *, const NetVirtualTiming *);
NetStatus net_virtual_tick(NetVirtualBackend *, uint64_t);
/* Explicit fault clock setter permits regression; portable adapter must HOLD. */
NetStatus net_virtual_clock_fault(NetVirtualBackend *, uint64_t);
NetStatus net_virtual_link(NetVirtualBackend *, NetUpperEventKind, NetWireLink,
    uint64_t, uint32_t);
/* These are driver capture entrypoints: no decode or Owner callback. UART
 * chunks preserve capture order and may return NO_CAPACITY after consumed>0;
 * callers must inspect consumed, never replay an accepted prefix. */
NetStatus net_virtual_can(NetVirtualBackend *, const NetWireCanEnvelope *,
    uint64_t, uint64_t);
NetStatus net_virtual_uart(NetVirtualBackend *, NetWireLink, const uint8_t *,
    const uint64_t *, size_t, uint64_t, size_t *);
NetStatus net_virtual_tx_fact(NetVirtualBackend *, NetWireLink,
    const NetTxEvent *, uint64_t);
#endif
