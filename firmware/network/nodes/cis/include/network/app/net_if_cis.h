#ifndef NETWORK_APP_NET_IF_CIS_H
#define NETWORK_APP_NET_IF_CIS_H

/* CIS Application Port, transcribed from the interface contract:
 *   docs/network/interface/02_NODES/CIS/10_DATA.md, 20_API.md
 * UART-C records (NetVisionResult, NetVisionStatus) are the Pi's
 * (network/app/net_if_pi_data.h).
 *
 * Direction / owner:
 *   CisApp_OnDomainPermission / CisApp_OnPiVisionResult / CisApp_OnPiVisionStatus
 *       Network -> Application. IMPLEMENTED BY THE CIS APPLICATION.
 *       Called inside Net_MainFunction() (serialized superloop), never from an ISR.
 *       message/meta are borrowed for the call only: copy what you need.
 *       void return: delivery is not acceptance or use.
 *   CisNet_PublishRear / Environment / Occupant / Status
 *       Application -> Network. IMPLEMENTED BY THE NETWORK.
 *       NET_SUBMIT_OK = bounded local copy only (not TX / DOMAIN acceptance).
 *       The application never fills CAN BOOT_ID / TX_SEQUENCE; Network owns
 *       them and adds only the local wait to the submitted AGE. A call made
 *       inside a CisApp_* callback is kept (latest one per message) and sent
 *       in the same Net_MainFunction() pass.
 *       CisNet_PublishOccupant relays the Pi observation the Network accepted
 *       last (see net_app_port_cis.h for what is accepted).
 *   CisNetPriv_RelayPiPermission
 *       NETWORK_PRIVATE: implemented AND called by the Network (the
 *       DOMAIN_PERMISSION dispatcher, same input as CisApp_OnDomainPermission).
 *       Declared here because 20_API.md lists it; the application never calls it.
 *   Net_Init / Net_MainFunction
 *       Application -> Network entry points (network/app/net_if_entry.h). */
#include "network/app/net_if_common.h"
#include "network/app/net_if_pi_data.h"
#include "network/app/net_if_entry.h"

typedef uint8_t NetCisState;
enum NetCisStateCode
{
    NET_CIS_STATE_STARTUP = 0,
    NET_CIS_STATE_READY = 1,
    NET_CIS_STATE_ACTIVE = 2,
    NET_CIS_STATE_FAULT = 3
};

typedef uint8_t NetInterfaceState;
enum NetInterfaceStateCode
{
    NET_INTERFACE_READY = 0,
    NET_INTERFACE_UNAVAILABLE = 1,
    NET_INTERFACE_RECOVERING = 2
};

typedef uint8_t NetProximityStatus;
enum NetProximityStatusCode
{
    NET_PROXIMITY_VALID_DISTANCE = 0,
    NET_PROXIMITY_NO_OBJECT = 1,
    NET_PROXIMITY_UNAVAILABLE = 2,
    NET_PROXIMITY_FAULT = 3,
    NET_PROXIMITY_RECOVERING = 4,
    NET_PROXIMITY_INACTIVE = 5
};

enum NetCisFaultMask
{
    NET_CIS_FAULT_INITIALIZATION_FAILURE = (1u << 0),
    NET_CIS_FAULT_VISION_FAULT = (1u << 1),
    NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT = (1u << 2),
    NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT = (1u << 3),
    NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT = (1u << 4),
    NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT = (1u << 5),
    NET_CIS_FAULT_COMMUNICATION_FAULT = (1u << 6),
    NET_CIS_FAULT_DATA_INVALID = (1u << 7),
    NET_CIS_FAULT_OUT_OF_RANGE = (1u << 8)
};

enum NetCisFaultCode
{
    NET_CIS_FAULT_CODE_NONE = 0,
    NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE = 1,
    NET_CIS_FAULT_CODE_VISION_FAULT = 2,
    NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT = 3,
    NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT = 4,
    NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT = 5,
    NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT = 6,
    NET_CIS_FAULT_CODE_COMMUNICATION_FAULT = 7,
    NET_CIS_FAULT_CODE_DATA_INVALID = 8,
    NET_CIS_FAULT_CODE_OUT_OF_RANGE = 9
};

typedef struct NetCisEnvironment
{
    int16_t temperature_centi_c;        /* 0.01 degC; -4000..12500; -32768 invalid */
    uint32_t temperature_sequence;      /* new measurement number of this value */
    uint16_t temperature_age_ms;        /* since the original measurement */
    NetValidity temperature_validity;
    NetQualityReason temperature_reason;
    uint16_t humidity_centi_percent;    /* 0.01 %RH; 0..10000; 65535 invalid */
    uint32_t humidity_sequence;
    uint16_t humidity_age_ms;
    NetValidity humidity_validity;
    NetQualityReason humidity_reason;
    uint32_t illuminance_lux;           /* 1 lx; 0..200000; 0xFFFFFFFF invalid */
    uint32_t illuminance_sequence;
    uint16_t illuminance_age_ms;
    NetValidity illuminance_validity;
    NetQualityReason illuminance_reason;
} NetCisEnvironment;

typedef struct NetCisOccupant
{
    uint32_t pi_boot_id;                /* Pi original boot, kept on relay */
    uint32_t vision_sequence;           /* Pi judgement number, kept on relay */
    uint16_t vision_age_ms;             /* Pi judgement age, accumulated */
    NetTriState occupant_presence;      /* 0 none, 1 present, 255 unknown */
    NetValidity presence_validity;
    NetQualityReason presence_reason;
    uint8_t occupant_count;             /* 0..5, 255 unknown */
    NetValidity count_validity;
    NetQualityReason count_reason;
    NetSensorFunctionState vision_function_status; /* CIS function state of this update */
} NetCisOccupant;

typedef struct NetCisRear
{
    uint32_t update_sequence;           /* new distance observation number */
    uint16_t source_age_ms;             /* since the original observation */
    uint16_t rear_distance_deci_cm;     /* 0.1 cm; 65535 no distance (see proximity_status) */
    NetValidity validity;
    NetQualityReason quality_reason;
    NetProximityStatus proximity_status;
} NetCisRear;

typedef struct NetCisStatus
{
    NetCisState cis_state;
    NetSensorFunctionState vision_status;
    NetSensorFunctionState temperature_status;
    NetSensorFunctionState humidity_status;
    NetSensorFunctionState illuminance_status;
    NetSensorFunctionState rear_status;
    NetInterfaceState pi_uart_status;
    NetInterfaceState can_interface_status;
    uint16_t fault_mask;                /* NetCisFaultMask bits 0..8 */
    uint16_t active_fault;              /* NetCisFaultCode */
    NetFlag recovering;
    uint16_t last_fault;                /* most recent fault since this boot */
    uint32_t last_fault_occurrence;
    uint8_t affected_function;          /* bit0 vision .. bit4 rear, bit5 provision path */
    uint32_t update_sequence;           /* status evaluation number, not CAN TX_SEQUENCE */
    uint16_t source_age_ms;             /* since the evaluation */
} NetCisStatus;

/* Implemented by the CIS application. */
void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta);
void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta);

/* Implemented by the Network. Serialized superloop context only, never an ISR. */
NetSubmitResult CisNet_PublishRear(const NetCisRear *message);
NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);
NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);
NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message);

/* NETWORK_PRIVATE (see above). */
NetSubmitResult CisNetPriv_RelayPiPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);

#endif
