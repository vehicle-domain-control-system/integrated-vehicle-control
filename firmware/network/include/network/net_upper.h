#ifndef NETWORK_NET_UPPER_H
#define NETWORK_NET_UPPER_H
#include "network/net_source.h"
#include "network/net_port.h"
#include "network/net_upper_can.h"
#include "network/net_upper_uart.h"
#include "network/net_upper_profile.h"

/* Network-private product service. No MCU, RTOS or external Owner ABI.
 * These are bounded implementation capacities, not approved ECU sizing. */
#define NET_UPPER_TX_CAPACITY 16u
#define NET_UPPER_RX_CAPACITY 32u
#define NET_UPPER_ROUTE_COUNT 48u
#define NET_UPPER_POLL_BUDGET 32u
typedef struct {
    NetWireLink link;
    NetWireDirection direction;
    NetWireCanEnvelope can;
    NetWireUartFrame uart;
    /* Local monotonic deadline, never serialized onto the wire. Driver must
     * check immediately before first-byte/frame transmission. Encoded AGE is
     * measured at age_encoded_at_ms; physical residence/propagation uncertainty
     * still needs the external timing budget, not mutation of borrowed bytes. */
    uint64_t age_encoded_at_ms, transmit_deadline_ms;
    bool deadline_bound, expires_at_equality;
} NetUpperPacket;
NetStatus net_upper_packet_eligible(const NetUpperPacket *, uint64_t now_ms);
typedef struct {
    uint64_t port_epoch, captured_at_ms;
    bool timestamp_known;
    uint32_t errors, overflow;
} NetUpperCapture;
typedef enum {
    NET_UPPER_CAN_RX, NET_UPPER_UART_BYTE, NET_UPPER_TX_EVENT,
    NET_UPPER_LINK_LOSS, NET_UPPER_LINK_RESET, NET_UPPER_PORT_ERROR
} NetUpperEventKind;
typedef struct {
    NetUpperEventKind kind;
    NetWireLink link;
    NetUpperCapture capture;
    NetWireCanEnvelope can;
    uint8_t byte;
    NetTxEvent tx;
} NetUpperEvent;
typedef struct {
    NetStatus (*now_ms)(void *context, uint64_t *out);
    /* OK begins an immutable borrow only. Non-OK promises no borrow/events.
     * Submit, admission, final TX and reference release are independent. */
    NetStatus (*submit)(void *context, const NetUpperPacket *packet, NetTxKey key);
    NetStatus (*poll)(void *context, NetUpperEvent *out);
} NetUpperPortOps;
typedef struct { const NetUpperPortOps *ops; void *context; } NetUpperPort;
typedef struct {
    NetUpperCapture capture;
    uint64_t delivered_at_ms, local_elapsed_ms, age_ms, receipt_id;
    uint32_t source_boot, source_sequence; /* Sender boot and transport TX/LINK sequence, not observation ID. */
    bool freshness_known; /* Continuous local clock and unsaturated wire AGE; no original AGE exists for M_REQUEST. */
    bool observation_only; /* Held route transport inspection; never Owner execution authority. */
    bool per_field_age_required; /* age_ms is conservative max; inspect each typed field. */
} NetUpperDelivery;
/* Raw Network observation independent of Owner ABI. Current pinned DOMAIN C2
 * stores rearDistanceMm; net_domain_c2 preserves mm without rounding. External
 * ABI adoption and legacy cm display conversion remain explicitly separate. */
typedef struct {
    uint32_t boot_id, tx_sequence, update_sequence;
    uint16_t rear_distance_mm;
    uint8_t validity, quality_reason, proximity_status;
    uint64_t source_age_ms;
    bool distance_available;
} NetUpperRear;
typedef struct {
    void *context;
    NetStatus (*rear)(void *, const NetUpperRear *, const NetUpperDelivery *);
    NetStatus (*can)(void *, const NetUpperCanMessage *, const NetUpperDelivery *);
    NetStatus (*uart)(void *, const NetUpperUartDelivery *, const NetUpperDelivery *);
    /* Held WINDOW routes are observable only through these distinct sinks.
     * No actuator, C2 state commit, liveness or ordinary Owner call is implied. */
    NetStatus (*held_can)(void *, const NetUpperCanMessage *, const NetUpperDelivery *);
    NetStatus (*held_uart)(void *, const NetUpperUartDelivery *, const NetUpperDelivery *);
} NetUpperOwner;
typedef struct {
    uint32_t node;
    uint64_t local_epoch, port_epoch[3];
    NetWireProfile profile;
    NetUpperPort port;
    NetUpperOwner owner;
    /* Indexed by the immutable registry, explicit owner-selected bounds.
     * No implicit command acceptance limit, permission or freshness policy. */
    NetAgePolicy rx_policy[NET_UPPER_ROUTE_COUNT];
    /* Explicit Owner opt-in for composite observations/history: bound local
     * backlog only, retain every raw field AGE/quality, and make no whole-
     * message freshness assertion. Rear/permission cannot opt out of TTL. */
    bool rx_per_field_age[NET_UPPER_ROUTE_COUNT];
    NetWireStreamPolicy uart_policy[2];
    bool uart_sequence_bound[2];
    uint16_t uart_initial_sequence[2]; /* Explicit owner-selected starting value. Wrap HOLD. */
    bool inspection_transport; /* Default OFF; isolated held-route observation only. */
    bool retain_failure_notices; /* Explicit opt-in: pending private failure notice prevents retirement. */
} NetUpperConfig;
#if NETWORK_UPPER_NODE_PROFILE == 0
typedef NetUpperConfig NetUpperStoredConfig;
#else
typedef struct {
    uint32_t node;
    uint64_t local_epoch, port_epoch[3];
    NetWireProfile profile;
    NetUpperPort port;
    NetUpperOwner owner;
    /* Indexed by the profile-owned RX rank, explicit owner-selected bounds.
     * No implicit command acceptance limit, permission or freshness policy. */
    NetAgePolicy rx_policy[NET_UPPER_ADMISSION_CAPACITY];
    /* Explicit Owner opt-in for composite observations/history: bound local
     * backlog only, retain every raw field AGE/quality, and make no whole-
     * message freshness assertion. Rear/permission cannot opt out of TTL. */
    bool rx_per_field_age[NET_UPPER_ADMISSION_CAPACITY];
    NetWireStreamPolicy uart_policy[2];
    bool uart_sequence_bound[2];
    uint16_t uart_initial_sequence[2]; /* Explicit owner-selected starting value. Wrap HOLD. */
    bool inspection_transport; /* Default OFF; isolated held-route observation only. */
    bool retain_failure_notices; /* Explicit opt-in: pending private failure notice prevents retirement. */
} NetUpperStoredConfig;
#endif

typedef struct {
    bool used, queued, submitted, admission_known, accepted;
    bool reference_released, conflict;
    NetTxKey key;
    NetTxOutcome outcome;
    NetStatus local_status;
} NetUpperTxSnapshot;
typedef struct {
    NetUpperTxSnapshot state;
    NetWireLink link;
    NetUpperCanMessage can;
    NetUpperUartMessage uart;
    NetSourceStamp source;
    NetAgePolicy policy;
    NetUpperPacket packet;
    bool projection_bound;
    NetWireLink source_link;
    uint64_t source_port_epoch;
    bool failure_notice_enabled;
    uint64_t failure_version, failure_ack_version;
} NetUpperTx;
typedef struct {
    bool seen;
    uint32_t boot, sequence;
    NetUpperCanMessage can;
    NetUpperUartMessage uart;
    NetUpperDelivery delivery;
    /* Exact accepted wire identity. Arrival time and storage padding are not wire. */
    union { NetWireCanEnvelope can; NetWireUartFrame uart; } raw;
    NetWireDirection raw_direction;
} NetUpperAdmission;
typedef struct {
    uint32_t rx_captured, rx_dropped, rx_invalid, rx_held, rx_delivered;
    uint32_t duplicates, stale, consumer_rejected, errors, resets, rx_conflicts;
    uint32_t tx_queued, tx_submitted, tx_admitted, tx_succeeded, tx_failed;
    uint32_t tx_released, tx_conflicts, tx_stale, tx_duplicates, time_holds;
} NetUpperObservation;
typedef struct NetUpper {
    unsigned char init_tag[8]; /* Byte-readable live-instance guard; fresh storage may be poisoned. */
    NetUpperStoredConfig config;
    bool initialized, in_call, clock_seen;
    uint64_t last_now_ms, next_token, next_receipt;
    NetUpperEvent rx[NET_UPPER_RX_CAPACITY];
    size_t rx_head, rx_count;
    NetUpperTx tx[NET_UPPER_TX_CAPACITY];
    NetUpperAdmission admission[NET_UPPER_ADMISSION_CAPACITY];
#if NET_UPPER_UART_RECEIVER_CAPACITY > 0
    NetUpperUartReceiver uart_rx[NET_UPPER_UART_RECEIVER_CAPACITY];
#endif
    bool uart_enabled[2];
    bool link_failed[3], context_seen;
    bool quiescing[3];
    NetWireMContext context;
    uint64_t context_port_epoch;
    uint32_t next_uart_sequence[2];
    NetUpperObservation observation;
} NetUpper;

/* Fresh storage only. No reinitialization of borrowed or live service state.
 * All APIs serialized by one owner; capture performs a stable value copy only.
 * Actual ISR synchronization and clock/epoch binding are S5 obligations. */
NetStatus net_upper_init(NetUpper *, const NetUpperConfig *);
NetStatus net_upper_now(NetUpper *, uint64_t *out);
NetStatus net_upper_capture(NetUpper *, const NetUpperEvent *);
NetStatus net_upper_step(NetUpper *, size_t receive_budget);
NetStatus net_upper_submit_can(NetUpper *, const NetUpperCanMessage *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
NetStatus net_upper_submit_uart(NetUpper *, const NetUpperUartMessage *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
NetStatus net_upper_submit_uart_auto(NetUpper *, const NetUpperUartMessage *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
NetStatus CisNet_PublishRear(NetUpper *, const NetWireCisRearRaw *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
/* Relay the latest actual Owner-accepted Pi observation, preserving capture
 * and every raw Vision identity/quality. Caller supplies only CIS transport identity. */
NetStatus CisNet_PublishOccupant(NetUpper *, uint32_t cis_boot, uint32_t tx_sequence,
    const NetAgePolicy *, NetTxKey *out);
NetStatus net_upper_tx_snapshot(const NetUpper *, NetTxKey, NetUpperTxSnapshot *out);
NetStatus net_upper_retire(NetUpper *, NetTxKey);
NetStatus net_upper_cancel(NetUpper *, NetTxKey); /* Queued only; never revoke a borrow. */
NetStatus net_upper_received_can(const NetUpper *, uint16_t id,
    NetUpperCanMessage *out, NetUpperDelivery *delivery, NetSourceStamp *source);
NetStatus net_upper_observation(const NetUpper *, NetUpperObservation *out);
/* Explicit observation-only held transport. Ordinary submit remains HOLD.
 * The caller must select config.inspection_transport in an isolated inspection
 * image. This does not authorize WINDOW operation or ordinary Owner delivery. */
NetStatus net_upper_submit_inspection_can(NetUpper *, const NetUpperCanMessage *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
NetStatus net_upper_submit_inspection_uart(NetUpper *, const NetUpperUartMessage *,
    const NetSourceStamp *, const NetAgePolicy *, NetTxKey *out);
/* Helpers shared by real modules; no success is synthesized by an adapter. */
NetStatus net_upper_wire_status(NetWireStatus);
NetStatus net_upper_deliver_can(const NetUpperOwner *, const NetUpperCanMessage *, const NetUpperDelivery *);
void net_upper_observe_increment(uint32_t *);
#endif
