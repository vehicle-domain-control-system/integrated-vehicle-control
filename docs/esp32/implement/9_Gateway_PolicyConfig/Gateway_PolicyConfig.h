#ifndef GATEWAY_POLICY_CONFIG_H
#define GATEWAY_POLICY_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "Gateway_Time.h"
#include "Gateway_Types.h"

/*
 * Gateway_PolicyConfig.h
 *
 * ESP32 Gateway의 PROVISIONAL calibration/policy 값을 Core Logic과 분리한다.
 *
 * 현재 첫 사용 대상:
 * - Bluetooth RSSI 기반 Proximity 판단
 *
 * 중요:
 * - 실제 값은 아직 프로젝트에서 확정되지 않았다.
 * - 이 파일의 struct field는 설정 항목을 정의할 뿐, 특정 수치를 확정하지 않는다.
 */

typedef struct
{
    /*
     * RSSI는 값이 클수록(예: -55 > -75) 일반적으로 더 강한 신호다.
     */
    int16_t near_enter_rssi_dbm;
    int16_t far_exit_rssi_dbm;

    /*
     * 상태 전환 전에 같은 후보 판정이 연속으로 필요한 sample 수.
     */
    uint16_t stable_sample_count;

    /*
     * 마지막 실제 RSSI 관측이 이 시간에 도달하면 STALE로 처리한다.
     */
    Gateway_TimeMs_t proximity_expiry_ms;
} Gateway_ProximityPolicyConfig_t;

Gateway_Status_t Gateway_PolicyConfig_Init(void);

void Gateway_PolicyConfig_Reset(void);

bool Gateway_PolicyConfig_IsInitialized(void);

Gateway_Status_t Gateway_PolicyConfig_SetProximity(
    const Gateway_ProximityPolicyConfig_t *config);

Gateway_Status_t Gateway_PolicyConfig_GetProximity(
    Gateway_ProximityPolicyConfig_t *config);

bool Gateway_PolicyConfig_IsProximityConfigured(void);

bool Gateway_PolicyConfig_IsValidProximityConfig(
    const Gateway_ProximityPolicyConfig_t *config);

#endif /* GATEWAY_POLICY_CONFIG_H */
