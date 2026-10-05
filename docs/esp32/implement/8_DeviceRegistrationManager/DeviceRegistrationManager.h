#ifndef DEVICE_REGISTRATION_MANAGER_H
#define DEVICE_REGISTRATION_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Gateway_Interface.h"
#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * DeviceRegistrationManager.h
 *
 * ESP32 차량 측 Bluetooth 등록 / 현재 연결 Context 관리자.
 *
 * 책임:
 * - 등록된 Bluetooth peer의 논리 Device Context Reference 보관
 * - 현재 연결된 Bluetooth peer의 논리 Device Context Reference 보관
 * - 현재 peer가 등록된 peer인지 비교
 * - Registration / Connection 상태 생성
 * - 상태의 품질 / 갱신 근거 관리
 * - Gateway_Interface를 통한 Domain Publish
 *
 * 하지 않는 것:
 * - Bluetooth 이름 / 주소만으로 등록 성공 생성
 * - Pairing/Bonding 내부 key 자체 저장 또는 노출
 * - App Active 추론
 * - Proximity NEAR/FAR 추론
 * - 차량 Request 허용 판단
 * - Session ID 생성
 *
 * 아래 device context reference는 Bluetooth 계층이 등록 절차를 통해
 * 확인한 "논리 식별 참조"여야 한다.
 * Raw bonding key / secret을 이 Manager에 전달하면 안 된다.
 */

#ifndef DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES
#define DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES (64U)
#endif

typedef struct
{
    uint8_t data[DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES];
    size_t length;
} DeviceRegistrationManager_DeviceRef_t;

typedef struct
{
    bool initialized;

    bool has_registered_peer;
    DeviceRegistrationManager_DeviceRef_t registered_peer;

    bool has_current_peer;
    DeviceRegistrationManager_DeviceRef_t current_peer;

    Gateway_RegistrationState_t current_registration;
    Gateway_ConnectionState_t connection;

    Gateway_DataQuality_t quality;
    Gateway_TimeMs_t last_observed_ms;

    uint32_t observation_revision;
    uint32_t last_published_revision;
} DeviceRegistrationManager_Snapshot_t;


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_Init(void);

void DeviceRegistrationManager_Reset(void);

bool DeviceRegistrationManager_IsInitialized(void);


/* -------------------------------------------------------------------------- */
/* Registration events                                                        */
/* -------------------------------------------------------------------------- */

/*
 * Bluetooth connection layer가 등록 절차 완료를 확인한 뒤 호출한다.
 *
 * ref는 raw MAC address/name만을 의미해서는 안 되며,
 * 등록된 peer를 재식별할 수 있는 논리 reference여야 한다.
 */
Gateway_Status_t DeviceRegistrationManager_OnRegistrationConfirmedAt(
    const Gateway_ByteView_t *device_context_ref,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_OnRegistrationConfirmed(
    const Gateway_ByteView_t *device_context_ref);

/*
 * 차량 측 등록 기록이 제거되었음을 반영한다.
 *
 * 현재 Bluetooth 물리 연결 자체를 강제로 끊는 함수는 아니다.
 * 현재 peer가 연결되어 있다면 registration은 NOT_REGISTERED로 갱신된다.
 */
Gateway_Status_t DeviceRegistrationManager_OnRegistrationRemovedAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_OnRegistrationRemoved(void);


/* -------------------------------------------------------------------------- */
/* Connection events                                                          */
/* -------------------------------------------------------------------------- */

/*
 * 현재 Bluetooth peer 연결이 확인됐을 때 호출한다.
 *
 * 현재 peer ref가 registered peer와 동일하면:
 *   REGISTERED + CONNECTED
 *
 * 다르면:
 *   NOT_REGISTERED + CONNECTED
 *
 * "연결됨" 자체로 등록 성공을 만들지 않는다.
 */
Gateway_Status_t DeviceRegistrationManager_OnPeerConnectedAt(
    const Gateway_ByteView_t *device_context_ref,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_OnPeerConnected(
    const Gateway_ByteView_t *device_context_ref);

Gateway_Status_t DeviceRegistrationManager_OnPeerDisconnectedAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_OnPeerDisconnected(void);


/* -------------------------------------------------------------------------- */
/* Explicit observation refresh                                               */
/* -------------------------------------------------------------------------- */

/*
 * Bluetooth 계층이 현재 등록/연결 상태를 다시 확인했지만 값은 동일한 경우,
 * "새 관측" 근거를 갱신하기 위해 사용한다.
 */
Gateway_Status_t DeviceRegistrationManager_MarkCurrentStateObservedAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_MarkCurrentStateObserved(void);


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

bool DeviceRegistrationManager_IsCurrentPeerAuthenticated(void);

Gateway_RegistrationState_t
DeviceRegistrationManager_GetCurrentRegistrationState(void);

Gateway_ConnectionState_t
DeviceRegistrationManager_GetConnectionState(void);

Gateway_Status_t DeviceRegistrationManager_GetSnapshot(
    DeviceRegistrationManager_Snapshot_t *snapshot);


/* -------------------------------------------------------------------------- */
/* Domain interface                                                           */
/* -------------------------------------------------------------------------- */

/*
 * 현재 Registration/Connection 정보를 typed logical update로 만든다.
 *
 * base_context의 vehicle_id / session_id / request_id는 그대로 유지하고,
 * device_context_id는 Manager가 현재 보고하는 peer reference로 덮어쓴다.
 *
 * device reference 선택:
 * - CONNECTED + current peer 존재: current peer
 * - disconnected + registered peer 존재: registered peer
 * - 둘 다 없음: empty
 *
 * 실제 UART byte mapping은 NETWORK-TBD다.
 */
Gateway_Status_t DeviceRegistrationManager_BuildDomainUpdateAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms,
    Gateway_RegistrationConnectionUpdate_t *update);

Gateway_Status_t DeviceRegistrationManager_BuildDomainUpdate(
    const Gateway_MessageContextView_t *base_context,
    Gateway_RegistrationConnectionUpdate_t *update);

/*
 * Build + Gateway_Interface_PublishRegistrationConnection().
 *
 * Publish 성공 시 현재 observation revision을 published로 기록한다.
 * 실패하면 새 관측 근거는 소비하지 않는다.
 */
Gateway_Status_t DeviceRegistrationManager_PublishToDomainAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t DeviceRegistrationManager_PublishToDomain(
    const Gateway_MessageContextView_t *base_context);

#endif /* DEVICE_REGISTRATION_MANAGER_H */
