#ifndef NETWORK_NET_DOMAIN_C2_H
#define NETWORK_NET_DOMAIN_C2_H
#include "network/net_upper_query.h"
#include "network/net_upper_control.h"
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

typedef struct {
    void *context;
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
/* Delivery gate of one CIS transition route (cis_transition_barriers). Only an
 * applied transition for a generation opens it; until then it stays closed. */
typedef struct {
    bool open;
    uint32_t id;   /* generation the barrier is open for */
    uint32_t held; /* frames held before C2 (diagnostic, saturating) */
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
typedef struct {
    unsigned char init_tag[8];
    NetUpper *service;
    NetUpperQuery *query;
    NetDomainC2Api api;
    NetDomainC2Config config;
    uint32_t next_can_sequence;
    bool initialized, registered, in_call;
    NetTxKey last_tx_key;
    DomainIf_Status_t last_c2_status;
    NetStatus last_status;
    uint32_t boundary_calls, rejected_mappings;
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
