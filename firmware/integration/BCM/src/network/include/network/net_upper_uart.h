#ifndef NETWORK_NET_UPPER_UART_H
#define NETWORK_NET_UPPER_UART_H
#include "network/net_wire_uart_dto.h"
#include "network/net_upper_can.h"

/* Private upper-stack binding API. Not a new DOMAIN/Gateway/Pi production ABI.
 * Codec profile remains CANDIDATE_ONLY; external protocol adoption is HOLD. */
typedef struct {
    NetWireLink link;
    uint8_t type;
    uint16_t sequence; /* Link sequence, never an original observation number. */
    union {
        NetWireMContext m_context;
        NetWireMRequest m_request;
        NetWireMQueryRaw m_query;
        NetWireMWarningAckRaw m_warning_ack;
        NetWireMDomainAliveRaw m_domain_alive;
        NetWireMResult m_result;
        NetWireMWarningRaw m_warning;
        NetWireMAvailabilityRaw m_availability;
        NetWireMDigitalStatusRaw m_digital_status;
        NetWireMDigitalResultRaw m_digital_result;
        NetWireMUserSettingsRaw m_user_settings;
        NetWireMQueryEndRaw m_query_end;
        NetWireMBcmDoorStateRaw m_bcm_door_state;
        NetWireMBcmClimateStateRaw m_bcm_climate_state;
        NetWireMBcmLightStateRaw m_bcm_light_state;
        NetWireMBcmStatusRaw m_bcm_status;
        NetWireMCisEnvironmentRaw m_cis_environment;
        NetWireMCisOccupantRaw m_cis_occupant;
        NetWireMCisRearRaw m_cis_rear;
        NetWireMCisStatusRaw m_cis_status;
        NetWireMVssStatusRaw m_vss_status;
        NetWireMWindowStateRaw m_window_state;
        NetWireMWindowFaultRaw m_window_fault;
        NetWirePVisionResult p_vision_result;
        NetWirePVisionStatusRaw p_vision_status;
        NetWirePPermission p_permission;
    } data;
} NetUpperUartMessage;

/* Pure dispatch: exact typed codec validation; failure leaves all OUT unchanged.
 * WINDOW codec representation is available; product service must disable routes. */
NetWireStatus net_upper_uart_encode(NetWireProfile profile,
    const NetUpperUartMessage *message, NetWireUartFrame *out);
NetWireStatus net_upper_uart_decode(NetWireProfile profile, NetWireLink link,
    NetWireDirection direction, const NetWireUartFrame *frame,
    NetUpperUartMessage *out);
/* Only confirmed unaccounted local elapsed. Does not claim freshness/TTL.
 * UINT16_MAX remains saturated, zero elapsed preserves every raw AGE. */
NetWireStatus net_upper_uart_add_age(NetUpperUartMessage *message,
    uint64_t elapsed_ms);
/* Maximum of all independently preserved AGE fields; no-AGE messages return
 * zero only as a structural property, never evidence of freshness or a TTL. */
uint16_t net_upper_uart_max_age(const NetUpperUartMessage *message);
/* Exactly 17 response schemas; never edits historical request_session or CAN
 * original identity. Failure leaves the complete message unchanged. */
NetWireStatus net_upper_uart_set_query_context(NetUpperUartMessage *message,
    uint32_t device, uint64_t session, uint32_t domain_boot, uint32_t query_id);

/* Pure same-observation projection only. The service separately requires an
 * actual accepted CAN receipt and current recipient binding before transmit.
 * WINDOW representation remains available with product routes disabled. */
NetWireStatus net_upper_uart_project_can(NetWireProfile profile,
    const NetUpperCanMessage *can, uint32_t device, uint64_t session,
    uint32_t domain_boot, uint32_t query_id, uint16_t link_sequence,
    NetUpperUartMessage *out);

typedef struct {
    NetUpperUartMessage message;
    NetWireUartFrame frame;
    uint64_t port_epoch;
    /* Only [0, frame.length) are meaningful, exactly as captured by the Port. */
    uint64_t byte_at_ms[NET_WIRE_UART_FRAME_MAX];
} NetUpperUartDelivery;

typedef struct {
    NetWireUartStream stream;
    uint64_t port_epoch;
    /* The last history_count captured bytes with their times, oldest at
     * history_head: a ring, so a byte is added without moving the others. */
    size_t history_count, history_head;
    uint8_t history_bytes[NET_WIRE_UART_FRAME_MAX];
    uint64_t history_at_ms[NET_WIRE_UART_FRAME_MAX];
    uint32_t typed_rejected, delivered_frames;
    bool initialized;
} NetUpperUartReceiver;

NetWireStatus net_upper_uart_receiver_init(NetUpperUartReceiver *receiver,
    NetWireProfile profile, NetWireLink link, NetWireDirection direction,
    const NetWireStreamPolicy *policy, uint64_t port_epoch);
/* Single-owner worker only. No queue/IRQ/clock fabricated here. A suffix frame
 * may be returned before this byte is consumed; caller retains that event when
 * consumed=false. Typed failure consumes a structural frame without reparsing
 * its payload. TIME_HOLD events must be quarantined by the capture owner and
 * an observed loss/reset signalled; never block unrelated queued traffic.
 * Delivery OUT is unchanged on every non-OK result. */
NetWireStatus net_upper_uart_receiver_feed(NetUpperUartReceiver *receiver,
    uint8_t byte, uint64_t captured_at_ms, uint64_t now_ms,
    bool timestamp_known, uint64_t port_epoch, bool *consumed,
    NetUpperUartDelivery *out);
/* May be called only after the capture queue is drained AND actual Port poll
 * reported no work, using real clock now. Exhausting a poll budget is not drain. */
NetWireStatus net_upper_uart_receiver_poll(NetUpperUartReceiver *receiver,
    uint64_t now_ms, uint64_t port_epoch, NetUpperUartDelivery *out);
/* Observed byte loss clears partial data but preserves time watermarks.
 * Explicit new nonzero port epoch is needed to reset those watermarks. This is
 * not authorization of a peer BOOT/Session transition. */
NetWireStatus net_upper_uart_receiver_signal(NetUpperUartReceiver *receiver,
    uint64_t port_epoch, bool byte_loss, bool epoch_reset);
#endif
