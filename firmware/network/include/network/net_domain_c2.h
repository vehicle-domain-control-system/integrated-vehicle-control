#ifndef NETWORK_NET_DOMAIN_C2_H
#define NETWORK_NET_DOMAIN_C2_H
#include "network/net_upper_query.h"
#include "network/net_upper_control.h"
#include "network/net_upper_publish.h"
/* Actual pinned Owner ABI, supplied by NETWORK_DOMAIN_C2_INCLUDE_DIR. */
#include "Domain_Interface_v0.5_C2_target.h"

typedef struct {
    bool (*IsInitialized)(void);
    DomainIf_Status_t (*SetRxObserver)(const DomainIf_RxObserver_t *observer);
    DomainIf_Status_t (*SetTxPort)(const DomainIf_TxPort_t *port);
    DomainIf_Status_t (*RxDigitalKeyConnection)(DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);
    DomainIf_Status_t (*RxProximity)(DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);
    DomainIf_Status_t (*RxMobileRequest)(const DomainIf_MobileRequest_t *request);
    DomainIf_Status_t (*RxWarningAck)(const DomainIf_WarningAck_t *ack);
    DomainIf_Status_t (*RxBcmDoorState)(const DoorState_t *state);
    DomainIf_Status_t (*RxBcmClimateState)(const ClimateState_t *state);
    DomainIf_Status_t (*RxBcmInteriorLightState)(const InteriorLightState_t *state);
    DomainIf_Status_t (*RxCisStatus)(const DomainIf_CisStatusReport_t *status);
    DomainIf_Status_t (*RxCisOccupant)(const CisOccupantState_t *state);
    DomainIf_Status_t (*RxCisCabinEnvironment)(const CisCabinEnvironmentState_t *state);
    DomainIf_Status_t (*RxCisRear)(const CisRearState_t *state);
    DomainIf_Status_t (*RxCisFault)(const FaultInfo_t *fault);
    DomainIf_Status_t (*RxWindowState)(const WindowState_t *state);
    DomainIf_Status_t (*RxWindowCommandResult)(const DomainIf_WindowCommandResult_t *result);
    DomainIf_Status_t (*RxWindowAntiPinch)(const DomainIf_AntiPinchReport_t *report);
    DomainIf_Status_t (*RxWindowFault)(const FaultInfo_t *fault);
    DomainIf_Status_t (*RxVssState)(const DomainIf_VssStateReport_t *state);
    DomainIf_Status_t (*RxVssFault)(const FaultInfo_t *fault);
    DomainIf_Status_t (*RxBcmStatus)(const DomainIf_BcmStatusReport_t *status);
    DomainIf_Status_t (*RxBcmCommandResult)(const DomainIf_BcmCommandResult_t *result);
    DomainIf_Status_t (*RxBcmProtectionEvent)(const DomainIf_BcmProtectionEvent_t *event);
    DomainIf_Status_t (*ValidatePassiveQueryContext)(DeviceContextId_t deviceContextId, SessionId_t sessionId);
    DomainIf_Status_t (*QueryGetBcmDoorState)(uint32_t nowMs, DoorState_t *outState);
    DomainIf_Status_t (*QueryGetBcmClimateState)(uint32_t nowMs, ClimateState_t *outState);
    DomainIf_Status_t (*QueryGetBcmInteriorLightState)(uint32_t nowMs, InteriorLightState_t *outState);
    DomainIf_Status_t (*QueryGetBcmStatus)(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);
    DomainIf_Status_t (*QueryGetCisEnvironment)(uint32_t nowMs, CisCabinEnvironmentState_t *outState);
    DomainIf_Status_t (*QueryGetCisOccupant)(uint32_t nowMs, CisOccupantState_t *outState);
    DomainIf_Status_t (*QueryGetCisRear)(uint32_t nowMs, CisRearState_t *outState);
    DomainIf_Status_t (*QueryGetCisStatus)(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);
    DomainIf_Status_t (*QueryGetVssStatus)(uint32_t nowMs, DomainIf_VssStateReport_t *outState);
    DomainIf_Status_t (*QueryGetWindowState)(uint32_t nowMs, WindowState_t *outState);
    DomainIf_Status_t (*QueryGetWindowFault)(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);
    DomainIf_Status_t (*QueryGetUserSettings)(uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);
    DomainIf_Status_t (*QueryGetFunctionAvailability)(uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);
    DomainIf_Status_t (*QueryGetDigitalKeyState)(uint32_t nowMs, DigitalKeyState_t *outState);
    DomainIf_Status_t (*QueryGetCurrentWarning)(DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);
    DomainIf_Status_t (*QueryGetRequestResult)(DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);
    DomainIf_Status_t (*QueryGetRecentRequestResults)(DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);
    DomainIf_Status_t (*QueryGetLastDigitalKeyResult)(DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);
    DomainIf_Status_t (*GetNetworkHealthSnapshot)(NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);
    DomainIf_Status_t (*RxNetworkLocalFailure)(const NetworkLocalFailure_t *failure);
} NetDomainC2Api;

/* M_DOMAIN_ALIVE (UART-M 0x02) evaluation of the DOMAIN application
 * (docs/network/interface/03_COMMUNICATION/20_UART_M.md M_DOMAIN_ALIVE):
 * the actual DOMAIN state, never one Network makes up. Wire values. */
typedef struct {
    uint8_t domain_state;     /* 0 INIT, 1 READY, 2 DEGRADED, 3 FAULT */
    uint8_t value_quality;    /* VALUE_QUALITY 0 OK, 1 STALE, 2 INVALID, 3 NO_DATA */
    uint32_t update_sequence; /* DOMAIN alive evaluation number */
    uint16_t source_age_ms;   /* ms since that evaluation */
} NetDomainC2AliveState;

/* Result of one DOMAIN private CIS BOOT transition call, one to one with the
 * DOMAIN transition statuses (environment and Occupant private barriers of the
 * DOMAIN application; EPOCH_ORDER_UNAVAILABLE exists for the environment only
 * and its sequential contract never returns it). UNAVAILABLE: the binding found
 * no DOMAIN private context (Occupant disabled, runtime not ready) and called
 * nothing. NONE is the diagnostic "no call yet"; a binding never returns it. */
typedef enum {
    NET_DOMAIN_C2_TRANSITION_NONE = 0,
    NET_DOMAIN_C2_TRANSITION_APPLIED,
    NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED,
    NET_DOMAIN_C2_TRANSITION_ID_CONFLICT,
    NET_DOMAIN_C2_TRANSITION_EPOCH_ORDER_UNAVAILABLE,
    NET_DOMAIN_C2_TRANSITION_BUSY,
    NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT,
    NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED,
    NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED,
    NET_DOMAIN_C2_TRANSITION_STALE,
    NET_DOMAIN_C2_TRANSITION_ORDER_ERROR,
    NET_DOMAIN_C2_TRANSITION_UNAVAILABLE
} NetDomainC2TransitionStatus;
/* Completion receipt of a transition as DOMAIN returned it. */
typedef struct {
    uint32_t id;       /* admitted CIS BOOT_ID (environment) / Pi source generation (Occupant) */
    uint64_t epoch;    /* transition epoch */
    uint32_t revision; /* DOMAIN barrier revision, +1 per completed invalidation */
} NetDomainC2TransitionReceipt;
/* CIS transition binding (docs/network/interface/02_NODES/DOMAIN/
 * 25_PRIVATE_ROUTING.md, CIS environment BOOT change; DOMAIN private barrier
 * of the environment and of the Occupant). Each function hands (id, epoch) to
 * the synchronous DOMAIN private transition once and returns its result and
 * receipt. The DOMAIN side implements it (it needs DOMAIN application headers);
 * the adapter calls it only from net_domain_c2_step, outside every service
 * callback. NULL: no transition is bound and that route stays held. */
typedef NetDomainC2TransitionStatus (*NetDomainC2TransitionApply)(void *context, uint32_t id, uint64_t epoch,
    NetDomainC2TransitionReceipt *out);
typedef struct {
    void *context;
    NetDomainC2TransitionApply environment; /* id = CIS BOOT_ID of 0x300 */
    NetDomainC2TransitionApply occupant;    /* id = PI_BOOT_ID of 0x310 */
} NetDomainC2TransitionBinding;

typedef struct {
    void *context;
    /* bcm_boot: command TARGET_BOOT_ID unless bcm_target_from_rx is set. */
    uint32_t domain_boot, bcm_boot, window_boot, first_can_sequence;
    uint16_t command_accept_limit_ms, event_use_limit_ms;
    NetAgePolicy transmit_policy;
    /* These unresolved semantic choices must be supplied by integration owner.
     * Missing callbacks restrict only the affected mapping; no guessed OK. */
    NetStatus (*value_quality)(void *, uint8_t validity, uint8_t reason, DataQuality_t *out);
    NetStatus (*status_quality)(void *, uint16_t id, const NetUpperCanMessage *, DataQuality_t *out);
    NetStatus (*permission_bundle)(void *, const DomainIf_CisPowerPermission_t *, NetWireDomainPermissionRaw *out);
    NetStatus (*window_permission_bundle)(void *, const DomainIf_WindowOperationPermission_t *, NetWireDomainPermissionRaw *out);
    NetStatus (*window_alive_bundle)(void *, const DomainIf_WindowDomainAlive_t *, NetWireDomainPermissionRaw *out);
    NetStatus (*window_event_projection)(void *, const NetUpperCanMessage *, DomainIf_AntiPinchReport_t *out);
    NetStatus (*local_failure_reason)(void *, const NetUpperTxSnapshot *, NetworkLocalFailureReason_t *out);
    NetStatus (*select_query_plan)(void *, const NetWireMQueryRaw *, NetUpperQueryPlan *);
    /* Confirms same original Owner commit, not merely optional observer OK. */
    bool (*confirm_snapshot)(void *, uint16_t can_id, uint64_t receipt_id, uint32_t boot, uint32_t tx_sequence);
    bool failure_sink_ready;
    bool window_transport_enabled; /* never authorizes actuator execution */
    bool window_channel_bound;
    uint8_t window_channel;
    /* Network owns the CIS environment / Occupant BOOT transitions
     * (docs/network/interface/04_COMMON/10_DATA_AND_ID_RULES.md, environment
     * values and their invalidation; DOMAIN private transition boundary):
     * 0x300 reaches C2 only while the environment barrier is open for the
     * frame's CIS BOOT_ID, 0x310 only while the Occupant barrier is open for
     * its PI_BOOT_ID (never for PI_BOOT_ID 0). A held frame makes no C2 call
     * and no ingress record. Off (zero) delivers both routes without a
     * transition; the DOMAIN Owner then refuses them until its transition is
     * applied, which latches the route blocked. */
    bool cis_transition_barriers;
    /* The transition calls that open the barriers (cis_transition_barriers). */
    NetDomainC2TransitionBinding transition;
    /* M04 publisher of the DOMAIN node (net_upper_publish.h) with entries for
     * DOMAIN_PERMISSION 0x080 and VSS_WARNING_STATE 0x0C0: their TxPort
     * submissions go to its latest-value slots, which own BOOT_ID /
     * TX_SEQUENCE, the 50 ms repetition and the replacement of a frame no
     * device took yet. NULL: one frame per call through the service queue. */
    NetUpperPublisher *publisher;
    /* Command TARGET_BOOT_ID from the BOOT_ID of the last BCM frame the
     * service accepted, BCM_STATUS first (docs/network/interface/
     * 03_COMMUNICATION/10_CAN_FD.md common header: TARGET_BOOT_ID is checked by
     * DOMAIN Network on the last normal target state). None received yet: no
     * command is sent. Off (zero): bcm_boot. */
    bool bcm_target_from_rx;
    /* M_DOMAIN_ALIVE source of the DOMAIN node (net_node_domain.c); NULL or a
     * non-OK return: INIT / NO_DATA / 0 / 0xFFFF. */
    NetStatus (*alive_state)(void *, NetDomainC2AliveState *out);
} NetDomainC2Config;
typedef struct {
    bool used, failure_delivered;
    NetTxKey key;
    NetworkLocalFailure_t identity;
    uint64_t admitted_at;
    DomainIf_Status_t failure_return;
} NetDomainC2TxRecord;
typedef struct {
    bool used, blocked;
    NetWireLink link;
    uint16_t id;
    uint32_t boot;
    uint64_t epoch, sequence;
} NetDomainC2IngressRecord;
typedef enum {
    NET_DOMAIN_C2_ENVIRONMENT_BARRIER, /* 0x300, keyed by the CIS BOOT_ID */
    NET_DOMAIN_C2_OCCUPANT_BARRIER,    /* 0x310, keyed by PI_BOOT_ID */
    NET_DOMAIN_C2_BARRIERS
} NetDomainC2BarrierKind;
/* Delivery gate of one CIS transition route (cis_transition_barriers),
 * net_domain_c2.c:
 *   CLOSED -> PENDING(id) -> OPEN(id) | BLOCKED, and DISABLED.
 * A frame of another nonzero generation than the open one, not one the barrier
 * left, makes it PENDING(id); the next adapter step requests the transition
 * (id, next_epoch). Only a confirmed completion opens it. BLOCKED and DISABLED
 * hold for the rest of this run. */
typedef enum {
    NET_DOMAIN_C2_BARRIER_CLOSED = 0, /* no generation seen yet */
    NET_DOMAIN_C2_BARRIER_PENDING,    /* transition for id due at the next adapter step */
    NET_DOMAIN_C2_BARRIER_OPEN,       /* transition for id confirmed: its frames pass */
    NET_DOMAIN_C2_BARRIER_BLOCKED,    /* latched; never called again in this run */
    NET_DOMAIN_C2_BARRIER_DISABLED    /* binding UNAVAILABLE; never called again */
} NetDomainC2BarrierState;
/* Why a barrier is BLOCKED or DISABLED. */
typedef enum {
    NET_DOMAIN_C2_BARRIER_REASON_NONE = 0,
    NET_DOMAIN_C2_BARRIER_REASON_RESULT,           /* last_result */
    NET_DOMAIN_C2_BARRIER_REASON_RECEIPT_MISMATCH, /* APPLIED / DUPLICATE_APPLIED, last_receipt not the request */
    NET_DOMAIN_C2_BARRIER_REASON_EPOCH_EXHAUSTED   /* the next epoch would be UINT64_MAX */
} NetDomainC2BarrierReason;
#define NET_DOMAIN_C2_BARRIER_LEFT_IDS 4u
typedef struct {
    bool open;     /* state OPEN; the delivery check reads it */
    uint32_t id;   /* generation the barrier is open (or pending) for */
    uint32_t held; /* frames held before C2 (diagnostic, saturating) */
    NetDomainC2BarrierState state;
    NetDomainC2BarrierReason reason;
    NetDomainC2TransitionStatus last_result;
    NetDomainC2TransitionReceipt last_receipt;
    uint64_t next_epoch; /* epoch of the next request: 1 in every run, +1 per confirmed transition */
    uint32_t revision;   /* DOMAIN barrier revision of the last confirmed transition */
    /* Generations the barrier left, latest last (0 = empty): their late
     * frames stay held and never start a transition back. */
    uint32_t left[NET_DOMAIN_C2_BARRIER_LEFT_IDS];
    uint32_t calls, applied; /* binding calls, confirmed transitions (saturating) */
} NetDomainC2Barrier;
/* CAN UPDATE_SEQUENCE of DOMAIN_PERMISSION (0x080), synthesized by Network
 * in a number space of its own (net_domain_tx.c): the tuple of the last
 * submission the service accepted. Zero (init) = nothing sent yet. */
typedef struct {
    bool sent;
    uint32_t sequence;        /* CAN UPDATE_SEQUENCE of that frame */
    uint32_t domain_sequence; /* its DOMAIN meta.updateSequence */
    uint32_t age_ms;          /* its DOMAIN meta.ageMs */
    uint8_t power, power_quality, operation_state, operation_allowed, operation_quality; /* wire values */
    /* Saturating: +1, the same number again, and HOLD for an older DOMAIN
     * sequence, a lifted restriction under the same DOMAIN sequence and a
     * smaller AGE of the same tuple. */
    uint32_t advanced, kept, held_older, held_relax, held_age;
} NetDomainC2PermissionSequence;
/* CAN messages whose TX_SEQUENCE the adapter numbers itself (one counter per
 * message, docs/network/interface/03_COMMUNICATION/10_CAN_FD.md common header:
 * per message, first 1): BCM door / climate / light command, DOMAIN_PERMISSION
 * and VSS_WARNING_STATE without a publisher, VSS_EVENT, WINDOW command. */
#define NET_DOMAIN_C2_CAN_MESSAGES 7u
typedef struct {
    unsigned char init_tag[8];
    NetUpper *service;
    NetUpperQuery *query;
    NetDomainC2Api api;
    NetDomainC2Config config;
    /* Next TX_SEQUENCE per message (net_domain_c2.c), from first_can_sequence. */
    uint32_t can_sequence[NET_DOMAIN_C2_CAN_MESSAGES];
    /* BOOT_ID of the last accepted BCM_STATUS and of the last other accepted
     * BCM frame; 0 = none yet (bcm_target_from_rx). */
    uint32_t bcm_status_boot, bcm_frame_boot;
    bool initialized, registered, in_call;
    NetTxKey last_tx_key;
    DomainIf_Status_t last_c2_status;
    NetStatus last_status;
    uint32_t boundary_calls, rejected_mappings;
    /* BCM result / event frames (0x140, 0x141) the Owner returned
     * DOMAIN_IF_REJECTED for: dropped one by one, route kept (nd_rx_call,
     * saturating). */
    uint32_t rejected_frames;
    NetDomainC2TxRecord tx[NET_UPPER_TX_CAPACITY];
    /* Once-invoked boundary journal; Observer failure cannot replay Core. */
    NetDomainC2IngressRecord ingress[19];
    NetDomainC2IngressRecord *current_ingress;
    NetDomainC2Barrier barrier[NET_DOMAIN_C2_BARRIERS];
    NetDomainC2PermissionSequence permission;
} NetDomainC2;
typedef struct {
    uint16_t id;
    NetUpperCanMessage original;
    NetUpperDelivery delivery;
    union {
        WindowState_t state;
        DomainIf_WindowFaultState_t fault;
        DomainIf_WindowCommandResult_t result;
        DomainIf_AntiPinchReport_t antipinch;
    } value;
} NetDomainC2WindowView;

NetStatus net_domain_c2_init(NetDomainC2 *, NetUpper *, NetUpperQuery *, const NetDomainC2Api *, const NetDomainC2Config *);
/* Links only currently implemented pinned Owner symbols; absent public bodies
 * remain NULL and may be filled by the external Owner before init. */
void net_domain_c2_pinned_owner_api(NetDomainC2Api *);
NetUpperOwner net_domain_c2_owner(NetDomainC2 *);
NetUpperQueryOwner net_domain_c2_query_owner(NetDomainC2 *);
NetStatus net_domain_c2_register(NetDomainC2 *);
NetStatus net_domain_c2_unregister(NetDomainC2 *);
NetStatus net_domain_c2_set_observer(NetDomainC2 *, const DomainIf_RxObserver_t *);
const DomainIf_TxPort_t *net_domain_c2_tx_port(void);
NetStatus net_domain_c2_step(NetDomainC2 *);
NetStatus net_domain_c2_health(NetDomainC2 *, NetworkLinkId_t, uint32_t, NetworkHealthSnapshot_t *);
/* Pure typed inspection only, no C2 invocation and no actuator authority.
 * A normal (observation_only=false) delivery is rejected. */
NetStatus net_domain_c2_window_view(NetDomainC2 *,const NetUpperCanMessage *,
    const NetUpperDelivery *,NetDomainC2WindowView *);
/* Serialized integration-owner acknowledgment after actual lifecycle/commit
 * evidence. A blocked C2 partial commit cannot be reset by link recovery. */
NetStatus net_domain_c2_accept_ingress_epoch(NetDomainC2 *, NetWireLink, uint16_t,
    uint64_t actual_new_port_epoch, uint32_t actual_new_source_boot,
    bool owner_confirmed_prior_commit_disposition);
NetStatus net_domain_c2_from_status(DomainIf_Status_t);
DomainIf_Status_t net_domain_c2_to_status(NetStatus);
/* PROVISIONAL explicit policy helpers: integration may select or replace them. */
NetStatus net_domain_c2_value_quality_provisional(void *, uint8_t, uint8_t, DataQuality_t *);
/* PROVISIONAL status report quality: OK for an admitted, decoded BCM_STATUS
 * (0x230), CIS_STATUS (0x320) or VSS_STATUS (0x340), whose wire has no
 * quality field; DOMAIN judges freshness on meta.ageMs. Any other id is HOLD.
 * Not selected by default: the integration (board hook) or a test binds it. */
NetStatus net_domain_c2_status_quality_provisional(void *, uint16_t, const NetUpperCanMessage *, DataQuality_t *);
/* PROVISIONAL Power/Operation bundle of DOMAIN_PERMISSION. Power: a confirmed
 * ALLOW / DENY (OK quality) is 1 / 0, any non-OK quality is UNKNOWN 255 with
 * that quality, true with a non-OK quality (or a non-OK meta.quality) is HOLD.
 * Operation has no source yet: 255 / 0 / NO_DATA. SOURCE_AGE is meta.ageMs
 * (above 65535 is HOLD); the adapter writes UPDATE_SEQUENCE. Not selected by
 * default: the integration (board hook) or a test binds it. */
NetStatus net_domain_c2_permission_bundle_provisional(void *, const DomainIf_CisPowerPermission_t *,
    NetWireDomainPermissionRaw *);
/* Legacy presentation only. Actual C2 rearDistanceMm stays exact. */
typedef enum { NET_DOMAIN_CM_EXACT_ONLY, NET_DOMAIN_CM_FLOOR, NET_DOMAIN_CM_NEAREST } NetDomainCmPolicy;
NetStatus net_domain_c2_mm_to_cm(uint16_t mm, NetDomainCmPolicy, uint16_t *cm);
#endif
