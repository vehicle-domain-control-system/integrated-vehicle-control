#ifndef VSS_NET_API_H
#define VSS_NET_API_H

/* Network interface of VSS. The type and function declarations below are
 * copied verbatim from the Network interface documents (common data and ID
 * rules, VSS Data, VSS API). When the Network module delivers its own header
 * it replaces this file; the declarations must stay identical.
 *
 * Net_Init and Net_MainFunction are the Network entry points that the VSS
 * super-loop calls. Their names and the nowMs argument are an assumption
 * pending agreement with the Network side: VSS calls Net_MainFunction once
 * per loop with its own millisecond clock, and the receive callbacks
 * VssApp_OnDomainEvent / VssApp_OnDomainWarningState run synchronously
 * inside it, never from an interrupt. */

#include <stdint.h>

/* ---- Common Net types ---------------------------------------------------- */

typedef enum NetSubmitResult
{
    NET_SUBMIT_OK = 0,             /**< 필요한 필드의 복사·로컬 보관을 완료했다. */
    NET_SUBMIT_INVALID_ARGUMENT = 1,    /**< NULL, 예약 코드, 필드 조합 또는 범위가 유효하지 않다. */
    NET_SUBMIT_NOT_READY = 2,           /**< 링크·현재 문맥이 이 제출을 수용할 준비가 되지 않았다. */
    NET_SUBMIT_NO_CAPACITY = 3      /**< 필요한 로컬 보관 공간이 없다. 기존 개별 항목을 덮어쓰지 않는다. */
} NetSubmitResult;

typedef struct NetCanSource
{
    uint32_t boot_id;       /**< 송신 노드 기동 번호. 0 예약. */
    uint32_t tx_sequence;   /**< 같은 메시지의 전송 순번. 새 관측 번호가 아니다. */
} NetCanSource;

typedef struct NetCanRxMeta
{
    NetCanSource source;
    uint32_t received_at_ms; /**< 로컬 단조 증가 ms. 노드 간 시계 비교에 사용하지 않는다. */
} NetCanRxMeta;

typedef uint8_t NetValueQuality;
enum NetValueQualityCode
{
    NET_QUALITY_OK = 0,
    NET_QUALITY_STALE = 1,
    NET_QUALITY_INVALID = 2,
    NET_QUALITY_NO_DATA = 3
};

typedef uint8_t NetQualityReason;
enum NetQualityReasonCode
{
    NET_QUALITY_REASON_NONE = 0,
    NET_QUALITY_REASON_NOT_READY = 1,
    NET_QUALITY_REASON_OUT_OF_RANGE = 2,
    NET_QUALITY_REASON_SENSOR_FAULT = 3,
    NET_QUALITY_REASON_VISION_FAULT = 4,
    NET_QUALITY_REASON_STALE = 5,
    NET_QUALITY_REASON_NO_DATA = 6,
    NET_QUALITY_REASON_COMMUNICATION_FAULT = 7,
    NET_QUALITY_REASON_RECOVERING = 8,
    NET_QUALITY_REASON_DATA_INVALID = 9
};

typedef uint8_t NetAvailability;
enum NetAvailabilityCode
{
    NET_AVAILABILITY_AVAILABLE = 0,
    NET_AVAILABILITY_LIMITED = 1,
    NET_AVAILABILITY_UNAVAILABLE = 2
};

typedef uint8_t NetFlag;
enum NetFlagCode
{
    NET_FLAG_NO = 0,
    NET_FLAG_YES = 1
};

#define NET_AGE_SATURATED_MS UINT16_MAX

/* ---- VSS types ----------------------------------------------------------- */

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
    uint16_t original_age_ms;
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
    uint8_t activation_meta_format;
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
    uint8_t fault_mask;
    uint16_t vss_last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    NetValueQuality last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetVssStatus;

/* ---- Calls between VSS and Network --------------------------------------- */

/* Network -> VSS (implemented by VSS). */
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);

/* VSS -> Network (implemented by Network). */
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);

/* Network entry points called by the VSS super-loop (assumed, see above). */
void Net_Init(void);
void Net_MainFunction(uint32_t nowMs);

#endif /* VSS_NET_API_H */
