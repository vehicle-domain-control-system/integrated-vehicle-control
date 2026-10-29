#ifndef NETWORK_NET_ECU_ADAPTER_H
#define NETWORK_NET_ECU_ADAPTER_H
#include "network/net_upper.h"
/* Network-private instance-scoped typed adapter. Owner callbacks consume or
 * copy during the call; missing functions fail closed. No actuator algorithm,
 * identity, quality or Permission is invented. Table may live in read-only ROM. */
typedef struct {
    NetStatus (*domain_permission)(void *, const NetWireDomainPermissionRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_door_command)(void *, const NetWireBcmDoorCommandRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_climate_command)(void *, const NetWireBcmClimateCommandRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_light_command)(void *, const NetWireBcmLightCommandRaw *, const NetUpperDelivery *);
    NetStatus (*window_command)(void *, const NetWireWindowCommandRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_door_state)(void *, const NetWireBcmDoorStateRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_climate_state)(void *, const NetWireBcmClimateStateRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_light_state)(void *, const NetWireBcmLightStateRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_result)(void *, const NetWireBcmResultRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_event)(void *, const NetWireBcmEventRaw *, const NetUpperDelivery *);
    NetStatus (*bcm_status)(void *, const NetWireBcmStatusRaw *, const NetUpperDelivery *);
    NetStatus (*cis_environment)(void *, const NetWireCisEnvironmentRaw *, const NetUpperDelivery *);
    NetStatus (*cis_occupant)(void *, const NetWireCisOccupantRaw *, const NetUpperDelivery *);
    NetStatus (*cis_rear)(void *, const NetWireCisRearRaw *, const NetUpperDelivery *);
    NetStatus (*cis_status)(void *, const NetWireCisStatusRaw *, const NetUpperDelivery *);
    NetStatus (*window_state)(void *, const NetWireWindowStateRaw *, const NetUpperDelivery *);
    NetStatus (*window_event)(void *, const NetWireWindowEventRaw *, const NetUpperDelivery *);
    NetStatus (*window_result)(void *, const NetWireWindowResultRaw *, const NetUpperDelivery *);
    NetStatus (*window_fault)(void *, const NetWireWindowFaultRaw *, const NetUpperDelivery *);
    NetStatus (*vss_event)(void *, const NetWireVssEventRaw *, const NetUpperDelivery *);
    NetStatus (*vss_warning_state)(void *, const NetWireVssWarningStateRaw *, const NetUpperDelivery *);
    NetStatus (*vss_status)(void *, const NetWireVssStatusRaw *, const NetUpperDelivery *);
    NetStatus (*m_context)(void *, const NetWireMContext *, const NetUpperDelivery *);
    NetStatus (*m_request)(void *, const NetWireMRequest *, const NetUpperDelivery *);
    NetStatus (*m_query)(void *, const NetWireMQueryRaw *, const NetUpperDelivery *);
    NetStatus (*m_warning_ack)(void *, const NetWireMWarningAckRaw *, const NetUpperDelivery *);
    NetStatus (*m_domain_alive)(void *, const NetWireMDomainAliveRaw *, const NetUpperDelivery *);
    NetStatus (*m_result)(void *, const NetWireMResult *, const NetUpperDelivery *);
    NetStatus (*m_warning)(void *, const NetWireMWarningRaw *, const NetUpperDelivery *);
    NetStatus (*m_availability)(void *, const NetWireMAvailabilityRaw *, const NetUpperDelivery *);
    NetStatus (*m_digital_status)(void *, const NetWireMDigitalStatusRaw *, const NetUpperDelivery *);
    NetStatus (*m_digital_result)(void *, const NetWireMDigitalResultRaw *, const NetUpperDelivery *);
    NetStatus (*m_user_settings)(void *, const NetWireMUserSettingsRaw *, const NetUpperDelivery *);
    NetStatus (*m_query_end)(void *, const NetWireMQueryEndRaw *, const NetUpperDelivery *);
    NetStatus (*m_bcm_door_state)(void *, const NetWireMBcmDoorStateRaw *, const NetUpperDelivery *);
    NetStatus (*m_bcm_climate_state)(void *, const NetWireMBcmClimateStateRaw *, const NetUpperDelivery *);
    NetStatus (*m_bcm_light_state)(void *, const NetWireMBcmLightStateRaw *, const NetUpperDelivery *);
    NetStatus (*m_bcm_status)(void *, const NetWireMBcmStatusRaw *, const NetUpperDelivery *);
    NetStatus (*m_cis_environment)(void *, const NetWireMCisEnvironmentRaw *, const NetUpperDelivery *);
    NetStatus (*m_cis_occupant)(void *, const NetWireMCisOccupantRaw *, const NetUpperDelivery *);
    NetStatus (*m_cis_rear)(void *, const NetWireMCisRearRaw *, const NetUpperDelivery *);
    NetStatus (*m_cis_status)(void *, const NetWireMCisStatusRaw *, const NetUpperDelivery *);
    NetStatus (*m_vss_status)(void *, const NetWireMVssStatusRaw *, const NetUpperDelivery *);
    NetStatus (*m_window_state)(void *, const NetWireMWindowStateRaw *, const NetUpperDelivery *);
    NetStatus (*m_window_fault)(void *, const NetWireMWindowFaultRaw *, const NetUpperDelivery *);
    NetStatus (*p_vision_result)(void *, const NetWirePVisionResult *, const NetUpperDelivery *);
    NetStatus (*p_vision_status)(void *, const NetWirePVisionStatusRaw *, const NetUpperDelivery *);
    NetStatus (*p_permission)(void *, const NetWirePPermission *, const NetUpperDelivery *);
} NetEcuCallbacks;
typedef struct {
    NetUpper *service;
    const NetEcuCallbacks *callbacks;
    void *context;
    uint32_t node;
    bool initialized;
} NetEcuAdapter;
NetStatus net_ecu_adapter_init(NetEcuAdapter *, NetUpper *, uint32_t node, const NetEcuCallbacks *, void *);
NetUpperOwner net_ecu_adapter_owner(NetEcuAdapter *);
/* Input raw DTO and explicit source stamp/age policy are copied by product
 * service. No C struct is serialized with memcpy; real field codecs run. */
NetStatus net_ecu_publish_domain_permission(NetEcuAdapter *, const NetWireDomainPermissionRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_door_command(NetEcuAdapter *, const NetWireBcmDoorCommandRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_climate_command(NetEcuAdapter *, const NetWireBcmClimateCommandRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_light_command(NetEcuAdapter *, const NetWireBcmLightCommandRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_window_command(NetEcuAdapter *, const NetWireWindowCommandRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_door_state(NetEcuAdapter *, const NetWireBcmDoorStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_climate_state(NetEcuAdapter *, const NetWireBcmClimateStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_light_state(NetEcuAdapter *, const NetWireBcmLightStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_result(NetEcuAdapter *, const NetWireBcmResultRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_event(NetEcuAdapter *, const NetWireBcmEventRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_bcm_status(NetEcuAdapter *, const NetWireBcmStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_cis_environment(NetEcuAdapter *, const NetWireCisEnvironmentRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_cis_occupant(NetEcuAdapter *, const NetWireCisOccupantRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_cis_rear(NetEcuAdapter *, const NetWireCisRearRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_cis_status(NetEcuAdapter *, const NetWireCisStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_window_state(NetEcuAdapter *, const NetWireWindowStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_window_event(NetEcuAdapter *, const NetWireWindowEventRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_window_result(NetEcuAdapter *, const NetWireWindowResultRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_window_fault(NetEcuAdapter *, const NetWireWindowFaultRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_vss_event(NetEcuAdapter *, const NetWireVssEventRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_vss_warning_state(NetEcuAdapter *, const NetWireVssWarningStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_vss_status(NetEcuAdapter *, const NetWireVssStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_context(NetEcuAdapter *, const NetWireMContext *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_request(NetEcuAdapter *, const NetWireMRequest *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_query(NetEcuAdapter *, const NetWireMQueryRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_warning_ack(NetEcuAdapter *, const NetWireMWarningAckRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_domain_alive(NetEcuAdapter *, const NetWireMDomainAliveRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_result(NetEcuAdapter *, const NetWireMResult *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_warning(NetEcuAdapter *, const NetWireMWarningRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_availability(NetEcuAdapter *, const NetWireMAvailabilityRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_digital_status(NetEcuAdapter *, const NetWireMDigitalStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_digital_result(NetEcuAdapter *, const NetWireMDigitalResultRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_user_settings(NetEcuAdapter *, const NetWireMUserSettingsRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_query_end(NetEcuAdapter *, const NetWireMQueryEndRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_bcm_door_state(NetEcuAdapter *, const NetWireMBcmDoorStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_bcm_climate_state(NetEcuAdapter *, const NetWireMBcmClimateStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_bcm_light_state(NetEcuAdapter *, const NetWireMBcmLightStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_bcm_status(NetEcuAdapter *, const NetWireMBcmStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_cis_environment(NetEcuAdapter *, const NetWireMCisEnvironmentRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_cis_occupant(NetEcuAdapter *, const NetWireMCisOccupantRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_cis_rear(NetEcuAdapter *, const NetWireMCisRearRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_cis_status(NetEcuAdapter *, const NetWireMCisStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_vss_status(NetEcuAdapter *, const NetWireMVssStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_window_state(NetEcuAdapter *, const NetWireMWindowStateRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_m_window_fault(NetEcuAdapter *, const NetWireMWindowFaultRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_p_vision_result(NetEcuAdapter *, const NetWirePVisionResult *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_p_vision_status(NetEcuAdapter *, const NetWirePVisionStatusRaw *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
NetStatus net_ecu_publish_p_permission(NetEcuAdapter *, const NetWirePPermission *, const NetSourceStamp *, const NetAgePolicy *, NetTxKey *);
#endif
