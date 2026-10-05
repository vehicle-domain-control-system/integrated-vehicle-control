#ifndef PROXIMITY_MANAGER_H
#define PROXIMITY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "DeviceRegistrationManager.h"
#include "Gateway_Interface.h"
#include "Gateway_PolicyConfig.h"
#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * ProximityManager.h
 *
 * Bluetooth RSSI 기반 Digital Key 근접 상태 Provider.
 *
 * 현재 RSSI 방법은 PROVISIONAL이다.
 *
 * 책임:
 * - 현재 authenticated Bluetooth peer에 묶인 RSSI sample 수용
 * - hysteresis + stable sample count 기반 NEAR/FAR/UNKNOWN 판단
 * - invalid / no data / stale / disconnect를 FAR로 치환하지 않음
 * - 품질 / age / new update 근거 관리
 * - Domain에 typed Proximity Update 제공
 *
 * 하지 않는 것:
 * - 등록 단말 판정
 * - App Active 판정
 * - "새 접근" 판정
 * - Auto Unlock 명령 생성
 * - 실제 거리(m) 추정
 */

typedef struct
{
    bool initialized;

    Gateway_ProximityState_t state;
    Gateway_DataQuality_t quality;

    bool has_observation;
    int16_t last_rssi_dbm;
    Gateway_TimeMs_t last_observed_ms;

    Gateway_ProximityState_t pending_candidate;
    uint16_t pending_candidate_count;

    bool has_device_ref;
    DeviceRegistrationManager_DeviceRef_t device_ref;

    uint32_t observation_revision;
    uint32_t last_published_revision;
} ProximityManager_Snapshot_t;

Gateway_Status_t ProximityManager_Init(void);

void ProximityManager_Reset(void);

bool ProximityManager_IsInitialized(void);

Gateway_Status_t ProximityManager_OnRssiSampleAt(
    const Gateway_ByteView_t *device_context_ref,
    int16_t rssi_dbm,
    bool sample_valid,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t ProximityManager_OnRssiSample(
    const Gateway_ByteView_t *device_context_ref,
    int16_t rssi_dbm,
    bool sample_valid);

Gateway_Status_t ProximityManager_UpdateAt(
    Gateway_TimeMs_t now_ms);

Gateway_Status_t ProximityManager_UpdateNow(void);

Gateway_ProximityState_t ProximityManager_GetState(void);

Gateway_DataQuality_t ProximityManager_GetQuality(void);

Gateway_Status_t ProximityManager_GetSnapshot(
    ProximityManager_Snapshot_t *snapshot);

Gateway_Status_t ProximityManager_BuildDomainUpdateAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms,
    Gateway_ProximityUpdate_t *update);

Gateway_Status_t ProximityManager_BuildDomainUpdate(
    const Gateway_MessageContextView_t *base_context,
    Gateway_ProximityUpdate_t *update);

Gateway_Status_t ProximityManager_PublishToDomainAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms);

Gateway_Status_t ProximityManager_PublishToDomain(
    const Gateway_MessageContextView_t *base_context);

#endif /* PROXIMITY_MANAGER_H */
