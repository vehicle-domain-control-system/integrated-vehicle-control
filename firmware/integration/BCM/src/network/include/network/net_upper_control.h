#ifndef NETWORK_NET_UPPER_CONTROL_H
#define NETWORK_NET_UPPER_CONTROL_H
#include "network/net_upper.h"

/* Network-private copied observations, not a DomainIf/Owner public ABI.
 * All calls are serialized by the service owner; callback reentry is BUSY.
 * Non-OK leaves every caller OUT unchanged. No function infers peer success. */
typedef struct {
    NetUpperUartMessage message;
    NetWireUartFrame frame; /* Complete accepted wire; not per-byte history. */
    NetUpperDelivery delivery;
    NetSourceStamp source;
    bool has_wire_age; /* Request/Query/ACK/End have no original wire AGE. */
} NetUpperUartReceipt;

typedef struct {
    NetUpperTxSnapshot state;
    NetWireLink link;
    NetUpperCanMessage can;
    NetUpperUartMessage uart;
    NetSourceStamp source;
    NetAgePolicy policy;
    /* Last locally serialized packet. Only state.submitted identifies an
     * actual Port borrow; this copy never changes that borrow's lifetime. */
    NetUpperPacket packet;
    bool failure_notice_enabled;
    uint64_t failure_version, failure_ack_version;
} NetUpperTxDetail;

typedef struct { NetUpperTxDetail detail; uint64_t version; } NetUpperFailureNotice;

typedef struct {
    bool quiescing, failed, can_resume;
    uint64_t port_epoch;
    size_t tx_used, queued_tx, unresolved_tx, borrowed_tx, conflicts;
    size_t pending_failure_notices, pending_rx, partial_uart_bytes;
} NetUpperLinkSnapshot;

typedef struct {
    NetUpperObservation observation;
    NetUpperLinkSnapshot links[3];
    size_t tx_used, rx_used;
    bool context_current;
    NetWireMContext context; /* Zero when no current recipient is available. */
    uint64_t context_port_epoch;
    bool sequence_bound[2], sequence_exhausted[2];
    uint32_t next_uart_sequence[2];
} NetUpperDiagnostics;

NetStatus net_upper_received_uart(const NetUpper *, NetWireLink, uint8_t type,
    NetUpperUartReceipt *out);
/* Receipt access is current authority, not historical export: UART-M types
 * except context-independent Domain Alive require a current recipient context. */
NetStatus net_upper_tx_detail(const NetUpper *, NetTxKey, NetUpperTxDetail *out);
/* Explicit config.retain_failure_notices opt-in. Poll is non-consuming;
 * retirement waits for exact version acknowledgment in that mode. The copied
 * notice remains usable after retirement; no callbacks/retries are inferred. */
NetStatus net_upper_failure_poll(const NetUpper *, NetUpperFailureNotice *out);
NetStatus net_upper_failure_ack(NetUpper *, NetTxKey, uint64_t version);

/* Cooperative Network-local drain only, never a claim that a HAL is stopped.
 * Quiesce refuses new work, cancels queued copies and keeps observing real TX
 * facts. Resume cannot reclaim conflicts, unresolved borrows or pending
 * notices; caller must supply the actual newer Port epoch. Remote boot/order
 * evidence and UART TX allocation are retained. */
NetStatus net_upper_quiesce_link(NetUpper *, NetWireLink);
NetStatus net_upper_link_snapshot(const NetUpper *, NetWireLink,
    NetUpperLinkSnapshot *out);
NetStatus net_upper_resume_link(NetUpper *, NetWireLink, uint64_t new_port_epoch);
NetStatus net_upper_diagnostics(const NetUpper *, NetUpperDiagnostics *out);

/* Copy only a current accepted DOMAIN_PERMISSION, never caller permission.
 * The explicit use-limit is an Owner-selected candidate value; no default is
 * invented. This engine-level helper (the permission guard's relay option)
 * relays OK power quality only and copies the DOMAIN UPDATE_SEQUENCE; the CIS
 * node relay is the App Port facade (net_app_port_cis.c) with the M04 relay
 * revision and the quality table below.
 * Requires explicitly bound UART-C sequence allocation, never a hidden seed.
 * Original500ms retention still applies even if use_limit_ms is larger. */
NetStatus net_upper_cis_publish_pi_permission(NetUpper *, uint32_t cis_boot,
    uint16_t selected_use_limit_ms, const NetAgePolicy *, NetTxKey *out);

/* P_PERMISSION QUALITY_REASON for a relayed DOMAIN power quality (VALUE_QUALITY
 * code): OK -> NONE (0), STALE -> STALE (5), INVALID -> DATA_INVALID (9),
 * NO_DATA -> NO_DATA (6); 0xFF for a code outside VALUE_QUALITY. The service
 * accepts a P_PERMISSION relay only with this reason for its quality. */
uint8_t net_upper_pi_permission_reason(uint8_t power_quality);
#endif
