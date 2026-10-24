#ifndef NETWORK_NET_HAL_ADAPTER_H
#define NETWORK_NET_HAL_ADAPTER_H
#include "network/net_upper.h"
#include "network/net_capacity.h"

/* Private Network implementation boundary, NOT an NXP/RTD or Owner ABI.
 * NET_HAL_TX_CAPACITY and NET_HAL_RX_CAPACITY are per node profile
 * (network/net_capacity.h); ISR binding is the backend's. */
#define NET_HAL_LINK_BIT(link) (1u << (unsigned)(link))
typedef enum { NET_HAL_S32K144 = 144, NET_HAL_S32K344 = 344 } NetHalPlatform;
typedef struct {
    NetHalPlatform platform;
    NetTxKey key;
    uint64_t port_epoch;
    NetUpperPacket packet; /* Owned, naturally >=32-bit aligned, immutable borrow. */
} NetHalTransfer;
typedef struct {
    NetStatus (*now_ms)(void *, uint64_t *);
    uintptr_t (*enter)(void *);
    void (*leave)(void *, uintptr_t);
    /* OK starts borrow only; callback facts provide admission/final/release.
     * Non-OK promises no retained pointer and no callbacks. The bridge detects
     * synchronous violations and pins storage instead of falsely releasing it.
     * Called outside the critical section; synchronous capture is supported. */
    NetStatus (*submit)(void *, const NetHalTransfer *);
} NetHalBackendOps;
typedef struct {
    bool binding_selected;
    uint32_t enabled_links;
    uint64_t port_epoch[3];
    const NetHalBackendOps *ops;
    void *context;
} NetHalConfig;
typedef struct {
    bool active, held, admission_known, accepted, released;
    bool sent_admission, sent_release, conflict_pending;
    NetTxOutcome outcome, sent_outcome;
    NetTxEvent conflict;
    /* Facts retain their own capture and callback order; polling never moves
     * a post-reset completion in front of the reset that preceded it. */
    NetUpperCapture capture, conflict_capture, fact_capture[3];
    uint64_t fact_order[3], conflict_order;
    NetHalTransfer transfer;
} NetHalAttempt;
typedef struct {
    uint32_t rx_captured, rx_dropped, tx_submitted, tx_rejected;
    uint32_t tx_facts, tx_duplicates, tx_stale, tx_conflicts;
    uint32_t errors, resets, backend_violations, clock_holds;
} NetHalObservation;
typedef struct {
    bool initialized, api_busy, clock_seen;
    NetHalPlatform platform;
    NetHalConfig config;
    uint64_t last_now_ms, next_order;
    NetUpperEvent rx[NET_HAL_RX_CAPACITY];
    uint64_t rx_order[NET_HAL_RX_CAPACITY];
    size_t rx_head, rx_count, tx_cursor;
    NetHalAttempt tx[NET_HAL_TX_CAPACITY];
    bool faulted[3], error_pending[3];
    NetUpperCapture error_capture[3];
    uint64_t error_order[3], last_fault_order[3];
    NetHalObservation observation;
} NetHalAdapter;

/* Fresh storage only, disjoint inputs/outputs; caller owns lifetime.
 * enter/leave must serialize ISR and foreground access for the real target.
 * These functions do not initialize any MCU peripheral or change its settings. */
NetStatus net_hal_adapter_init(NetHalAdapter *, const NetHalConfig *, NetHalPlatform);
NetStatus net_s32k144_upper_port_init(NetHalAdapter *, const NetHalConfig *);
NetStatus net_s32k344_upper_port_init(NetHalAdapter *, const NetHalConfig *);
NetUpperPort net_hal_adapter_port(NetHalAdapter *);
NetStatus net_hal_capture_can(NetHalAdapter *, const NetWireCanEnvelope *, const NetUpperCapture *);
NetStatus net_hal_capture_uart(NetHalAdapter *, NetWireLink, uint8_t, const NetUpperCapture *);
NetStatus net_hal_capture_tx(NetHalAdapter *, NetWireLink, const NetTxEvent *, const NetUpperCapture *);
NetStatus net_hal_capture_link(NetHalAdapter *, NetUpperEventKind, NetWireLink, const NetUpperCapture *);
NetStatus net_hal_adapter_observation(NetHalAdapter *, NetHalObservation *);
#endif
