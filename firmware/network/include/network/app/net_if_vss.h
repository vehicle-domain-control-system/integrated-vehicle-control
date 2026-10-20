#ifndef NETWORK_APP_NET_IF_VSS_H
#define NETWORK_APP_NET_IF_VSS_H

/* VSS Application Port, transcribed from the interface contract:
 *   docs/network/interface/02_NODES/VSS/10_DATA.md, 20_API.md
 *
 * Direction / owner:
 *   VssApp_OnDomainEvent / VssApp_OnDomainWarningState
 *       Network -> Application. IMPLEMENTED BY THE VSS APPLICATION.
 *       Called from NetNode_Step() (serialized superloop), never from an ISR.
 *       message/meta are borrowed for the call only: copy what you need.
 *       void return: delivery is not acceptance, storage or sound evidence.
 *   VssNet_PublishStatus
 *       Application -> Network. IMPLEMENTED BY THE NETWORK.
 *       NET_SUBMIT_OK = bounded local copy only (not TX/ACK/DOMAIN acceptance).
 *       The application never fills CAN BOOT_ID / TX_SEQUENCE; Network owns them.
 *   VssNetPriv_OnDomainPermission
 *       Network-private supervision (see net_app_port_vss.h). Not an App API. */
#include "network/app/net_if_common.h"

typedef uint8_t NetVssEventKind;
enum NetVssEventKindCode
{
    NET_VSS_EVENT_WELCOME = 1,
    NET_VSS_EVENT_GOODBYE = 2,
    NET_VSS_EVENT_LOCK_COMPLETE = 3,
    NET_VSS_EVENT_UNLOCK_COMPLETE = 4,
    NET_VSS_EVENT_LOCK_ERROR = 5
};

typedef uint8_t NetVssState;
enum NetVssStateCode
{
    NET_VSS_STATE_STARTUP = 0,
    NET_VSS_STATE_READY = 1,
    NET_VSS_STATE_PLAYING = 2,
    NET_VSS_STATE_FAULT = 3
};

typedef uint8_t NetVssAvailability;
enum NetVssAvailabilityCode
{
    NET_VSS_AVAILABILITY_FULL = 0,
    NET_VSS_AVAILABILITY_DEGRADED = 1,
    NET_VSS_AVAILABILITY_UNAVAILABLE = 2
};

typedef uint8_t NetRearWarningState;
enum NetRearWarningStateCode
{
    NET_REAR_WARNING_CLEAR = 0,
    NET_REAR_WARNING_CAUTION = 1,
    NET_REAR_WARNING_EMERGENCY = 2
};

enum NetVssFaultMask
{
    NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE = (1u << 0),
    NET_VSS_FAULT_PLAYBACK_START_FAILURE = (1u << 1),
    NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE = (1u << 2),
    NET_VSS_FAULT_PLAYBACK_STATE_FAILURE = (1u << 3),
    NET_VSS_FAULT_INITIALIZATION_FAILURE = (1u << 4)
};

enum NetVssFaultCode
{
    NET_VSS_FAULT_CODE_NONE = 0,
    NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE = 1,
    NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE = 2,
    NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE = 3,
    NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE = 4,
    NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE = 5
};

typedef struct NetVssEvent
{
    NetVssEventKind event_kind;
    uint32_t occurrence_id;
    uint16_t original_age_ms;   /* RX basis: age at meta.received_at_ms */
    uint16_t use_limit_ms;
    NetValueQuality value_quality;
    NetAvailability input_availability;
    uint32_t related_command_id;
} NetVssEvent;

typedef struct NetVssWarningState
{
    uint32_t antipinch_source_boot;
    uint32_t antipinch_source_sequence;
    uint16_t antipinch_original_age_ms;
    NetFlag antipinch_state;
    NetValueQuality antipinch_quality;
    NetQualityReason antipinch_reason;
    NetAvailability antipinch_availability;
    uint32_t occupant_hazard_source_boot;
    uint32_t occupant_hazard_source_sequence;
    uint16_t occupant_hazard_original_age_ms;
    NetFlag occupant_hazard_state;
    NetValueQuality occupant_hazard_quality;
    NetQualityReason occupant_hazard_reason;
    NetAvailability occupant_hazard_availability;
    uint32_t rear_obstacle_source_boot;
    uint32_t rear_obstacle_source_sequence;
    uint16_t rear_obstacle_original_age_ms;
    NetRearWarningState rear_obstacle_state;
    NetValueQuality rear_obstacle_quality;
    NetQualityReason rear_obstacle_reason;
    NetAvailability rear_obstacle_availability;
    NetFlag rear_detection_activation;
    uint8_t activation_meta_format;  /* 0 = no activation evidence, 1 = evidence present */
    NetValueQuality rear_activation_quality;
    NetQualityReason rear_activation_reason;
    uint32_t rear_activation_sequence;
    uint16_t rear_activation_age_ms;
} NetVssWarningState;

typedef struct NetVssStatus
{
    NetVssState vss_state;
    NetVssAvailability vss_availability;
    NetFlag vss_accepting_events;
    NetFlag vss_fault_active;
    uint8_t fault_mask;             /* current faults only, bits 0..4 */
    uint16_t vss_last_fault;        /* history code 0..5 */
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    NetValueQuality last_fault_quality; /* history present = OK(0), none = NO_DATA(3) */
    uint32_t update_sequence;       /* VSS evaluation number, not CAN TX_SEQUENCE */
    uint16_t source_age_ms;         /* age of the current service evaluation */
} NetVssStatus;

/* Implemented by the VSS application. */
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);

/* Implemented by the Network. Serialized superloop context only, never an ISR. */
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);

#endif
