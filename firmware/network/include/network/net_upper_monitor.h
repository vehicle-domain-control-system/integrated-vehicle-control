#ifndef NETWORK_NET_UPPER_MONITOR_H
#define NETWORK_NET_UPPER_MONITOR_H
#include "network/net_upper_control.h"

/* Observed permission eligibility, not a replacement vehicle Permission DTO.
 * Unusable never changes the received raw permission to DENY/OK or emits ALLOW.
 * Call from the same serialized superloop after net_upper_step, never an ISR. */
typedef enum {
    NET_PERMISSION_NO_DATA, NET_PERMISSION_CURRENT, NET_PERMISSION_RESTRICTED,
    NET_PERMISSION_BAD_QUALITY, NET_PERMISSION_SOURCE_EXPIRED,
    NET_PERMISSION_RX_TIMEOUT, NET_PERMISSION_LINK_UNAVAILABLE,
    NET_PERMISSION_CLOCK_HOLD, NET_PERMISSION_OWNER_FAILED,
    NET_PERMISSION_OWNER_NO_DATA
} NetPermissionReason;
typedef struct {
    bool has_original, power_usable, operation_usable, alive_usable;
    NetPermissionReason reason;
    NetWireDomainPermissionRaw original;
    NetUpperDelivery delivery;
    uint64_t evaluated_at_ms, original_age_ms, receive_silence_ms;
} NetPermissionView;
typedef struct {
    /* Explicit selected receiver timeout, independent of the fixed original
     * Power 500ms rule. No implicit CAN-period or command acceptance policy. */
    NetAgePolicy receive_timeout;
    void *context;
    void (*changed)(void *, const NetPermissionView *); /* synchronous copied-consumption contract */
    bool relay_to_pi;
    uint32_t cis_boot;
    uint16_t pi_use_limit_ms;
    NetAgePolicy relay_age;
} NetPermissionPolicy;
typedef struct {
    bool initialized, in_call, view_seen, invalidated, relay_active;
    NetUpper *service;
    NetPermissionPolicy policy;
    NetPermissionView view;
    NetPermissionReason invalid_reason;
    uint64_t invalid_receipt, relayed_receipt;
    NetTxKey relay_key;
} NetPermissionGuard;
NetStatus net_permission_init(NetPermissionGuard *, NetUpper *, const NetPermissionPolicy *);
NetStatus net_permission_step(NetPermissionGuard *);
NetStatus net_permission_snapshot(const NetPermissionGuard *, NetPermissionView *);
/* Actual Owner FAILED/NO_DATA evidence immediately invalidates local usability
 * and cancels still-queued permissions. Driver borrows cannot be revoked.
 * A later, newly accepted receipt is required to restore eligibility. */
NetStatus net_permission_invalidate(NetPermissionGuard *, NetPermissionReason);

/* Per-route communication silence is explicitly caller-selected. Discrete
 * Event/Result are excluded from periodic supervision. No bus-wide RX implies
 * a particular peer alive. Caller owns one small object for each needed route. */
typedef struct {
    bool initialized, seen;
    NetWireLink link;
    uint16_t id_type;
    NetAgePolicy silence;
    uint64_t port_epoch, captured_at_ms, receipt_id;
    uint32_t source_boot, transport_sequence;
} NetRouteWatch;
typedef struct {
    bool has_valid_rx, usable;
    NetStatus status;
    uint64_t age_since_capture_ms, receipt_id, port_epoch;
} NetRouteHealth;
NetStatus net_route_watch_init(NetRouteWatch *, NetWireLink, uint16_t, const NetAgePolicy *);
NetStatus net_route_watch_observe(NetRouteWatch *, const NetUpperDelivery *);
NetStatus net_route_watch_refresh(NetRouteWatch *, const NetUpper *);
NetStatus net_route_watch_read(const NetRouteWatch *, uint64_t now, uint64_t epoch,
    bool link_available, NetRouteHealth *);
#endif
