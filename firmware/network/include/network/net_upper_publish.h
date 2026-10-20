#ifndef NETWORK_NET_UPPER_PUBLISH_H
#define NETWORK_NET_UPPER_PUBLISH_H
#include "network/net_upper.h"

/* L03/M04 Message Service: Network-owned transmit identity for current-state
 * messages, one latest-value slot per registered message and a periodic
 * repetition driver that does not depend on the link.
 *
 * Basis (docs/network):
 *  - network_sw_architecture.md §4, §5.2, §5.3: the Message Service owns TX
 *    pending/scheduling, sequence management and periodic/change/event
 *    handling, and chooses TX_SEQUENCE / LINK_SEQUENCE.
 *  - interface/03_COMMUNICATION/10_CAN_FD.md, common header: BOOT_ID belongs to
 *    the sending Network; TX_SEQUENCE is kept per message, starts at 1 and
 *    increases for every frame, a retransmission of the same original included.
 *  - interface/04_COMMON/10_DATA_AND_ID_RULES.md (sequence table, receive
 *    timeout): a repetition is not a new observation; the original time and
 *    observation number stay and the current AGE keeps growing.
 *  - sw_detail/functions/CIS_REAR_TX_DISPATCH.md: a sequence number is used
 *    up only when the frame is offered to a device and is never reused; after
 *    0xFFFFFFFF the sequence is HOLD (CAN TX_SEQUENCE; a UART LINK_SEQUENCE is
 *    "frame order modulo 65536", 03_COMMUNICATION/20_UART_M.md shared framing,
 *    and wraps in the service); the wait not yet counted is added to AGE
 *    once, before encoding; a frame a device owns is never rewritten.
 *  - interface/02_NODES/VSS/20_API.md (VssNet_PublishStatus): Network only adds
 *    the sending BOOT/TX and the waiting AGE.
 *
 * A slot holds replaceable current state (architecture §6 A): a newer
 * submission replaces an older one no device has taken yet. Discrete
 * Command/Result/Event traffic must not be registered here (§6 B forbids a
 * silent overwrite). At most one frame per message is unsettled at a time, so
 * frames of one message reach the bus in TX_SEQUENCE order; a submission that
 * arrives meanwhile waits and its AGE keeps growing from its original stamp.
 *
 * Serialized owner context only (the superloop that runs net_upper_step),
 * never an ISR. The service must not retain private failure notices
 * (config.retain_failure_notices), otherwise retirement would wait forever.
 * Capacities are bounded implementation choices, not approved ECU sizing. */
#ifndef NET_UPPER_PUBLISH_CAPACITY
#define NET_UPPER_PUBLISH_CAPACITY 8u
#endif

typedef struct {
    NetWireLink link;  /* NET_WIRE_CAN, NET_WIRE_UART_M or NET_WIRE_UART_C */
    uint16_t id;       /* CAN standard identifier or UART MESSAGE_TYPE */
    /* 0: one frame per submission, never repeated. Otherwise the last
     * submission is sent again every period_ms until a newer one arrives. */
    uint32_t period_ms;
    /* Freshness bound of a submission on its way to the bus (original AGE plus
     * local wait). Repetitions use the M04 repetition policy instead (see
     * net_upper_publish.c), so this bound never stops a schedule. */
    NetAgePolicy age;
} NetUpperPublishEntry;

typedef struct {
    /* Network-owned CAN BOOT_ID of this power cycle, written into every CAN
     * frame of the table. Nonzero when the table has a CAN entry. UART
     * payload identities stay the caller's; LINK_SEQUENCE is the service's
     * per-direction counter (net_upper_submit_uart_auto). */
    uint32_t can_boot_id;
    const NetUpperPublishEntry *entries; /* copied by net_upper_publish_init */
    size_t entry_count;
} NetUpperPublishConfig;

/* Saturating raw counters (M09 convention), per message. */
typedef struct {
    uint32_t submitted;                /* accepted publish calls */
    uint32_t replaced;                 /* submissions replaced before any device took them */
    uint32_t sent;                     /* submissions offered to the service queue (once each) */
    uint32_t periodic_sent;            /* repetitions offered to the service queue */
    uint32_t periodic_skipped_pending; /* due periods skipped: previous frame still unsettled */
    uint32_t sequence_hold;            /* publish/offer refused: CAN TX_SEQUENCE exhausted
                                          (a UART LINK_SEQUENCE wraps modulo 65536) */
    uint32_t queue_full;               /* offers deferred: service queue or device had no room */
    uint32_t not_sent;                 /* offers refused, or frames that ended before a device took them */
    uint32_t tx_failed;                /* frames a device took and reported not sent */
    /* Largest wait from the submission (or the relayed original receipt) to
     * the offer, i.e. what the service adds to AGE when it dispatches the
     * frame in the same step. */
    uint32_t pending_age_added_ms_max;
} NetUpperPublishCounters;

typedef struct {
    NetUpperPublishEntry entry;
    bool has_source;      /* a submission exists (nothing is sent before the first) */
    bool fresh;           /* the latest submission has not been offered yet */
    bool fresh_counted;   /* that submission is already counted in counters.sent */
    bool in_flight;       /* an offered frame is not settled and retired yet */
    bool in_flight_fresh; /* that frame carried a submission, not a repetition */
    bool sequence_held;   /* CAN: TX_SEQUENCE 0xFFFFFFFF used, no further frame */
    union { NetUpperCanMessage can; NetUpperUartMessage uart; } content;
    NetSourceStamp source; /* original stamp of the latest submission */
    uint32_t next_sequence, in_flight_sequence; /* CAN TX_SEQUENCE, first 1 */
    NetTxKey key;
    uint64_t next_due_ms;
    NetUpperPublishCounters counters;
} NetUpperPublishSlot;

typedef struct {
    bool initialized;
    NetUpper *service;
    uint32_t can_boot_id;
    uint64_t next_observation;
    size_t count;
    NetUpperPublishSlot slot[NET_UPPER_PUBLISH_CAPACITY];
} NetUpperPublisher;

typedef struct {
    bool replaced; /* an earlier submission not taken by any device was replaced */
    bool in_step;  /* called back from inside net_upper_step (an RX callback) */
} NetUpperPublishNote;

typedef struct {
    bool has_source, fresh, in_flight, sequence_held;
    uint32_t period_ms;
    uint32_t boot_id;       /* CAN BOOT_ID; 0 for UART entries */
    uint32_t next_sequence; /* CAN: next TX_SEQUENCE; UART: next LINK_SEQUENCE of the direction */
    NetUpperPublishCounters counters;
} NetUpperPublishView;

/* Fresh storage only. Every entry must be a registry route this node sends on
 * a link of the compiled profile, with a bound age policy, and not listed
 * twice. UART entries need the direction's LINK_SEQUENCE allocation bound in
 * the service configuration. The table is copied. */
NetStatus net_upper_publish_init(NetUpperPublisher *, NetUpper *, const NetUpperPublishConfig *);

/* Copy the latest content of a registered message; returns at once
 * (OK = copied, not TX success). BOOT_ID/TX_SEQUENCE (CAN) and the link
 * sequence (UART) in the copy are ignored: M04 assigns them per frame.
 * source NULL: a local evaluation, stamped now. Otherwise the original stamp
 * of a relayed receipt; its original_age_ms must equal the content's AGE.
 * May be called from inside an RX callback of the same service; the step's
 * clock reading is then the submission time (never later than the call, so
 * the counted wait is never shorter than the real one).
 * NET_HOLD: link failed/quiescing or the message's sequence is exhausted;
 * NET_UNSUPPORTED: not registered; codec and age results otherwise. */
NetStatus net_upper_publish_can(NetUpperPublisher *, const NetUpperCanMessage *,
    const NetSourceStamp *source, NetUpperPublishNote *note);
NetStatus net_upper_publish_uart(NetUpperPublisher *, const NetUpperUartMessage *,
    const NetSourceStamp *source, NetUpperPublishNote *note);

/* Once per superloop pass, outside the step and right before net_upper_step(),
 * which then dispatches what was offered here. Settles and retires finished
 * frames, offers a waiting submission once the message's previous frame is
 * settled, and offers due repetitions. */
NetStatus net_upper_publish_service(NetUpperPublisher *);

NetStatus net_upper_publish_view(const NetUpperPublisher *, NetWireLink, uint16_t id,
    NetUpperPublishView *out);
#endif
