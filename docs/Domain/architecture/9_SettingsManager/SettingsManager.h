/**
 * @file SettingsManager.h
 * @brief Domain이 확정한 사용자 설정을 차량 상태/실측/ECU 적용 상태와 분리해 관리
 *
 * ============================================================================
 * 책임
 * ============================================================================
 *
 * SettingsManager는 다음 "확정 설정"을 관리한다.
 *
 * Vehicle-global:
 *   - Target Temperature
 *   - Auto Climate Enable
 *   - Manual Fan/Circulation Selection
 *   - Interior Light User Enable
 *   - Interior Light Brightness
 *   - Interior Light RGB Color
 *
 * Device/Connection-specific:
 *   - Digital Key Auto Unlock Enable
 *
 * 하지 않는 것:
 *   - BCM 실제 Fan/Light 적용 상태 저장       -> VehicleStateManager
 *   - Cabin Temperature 실측 저장             -> VehicleStateManager
 *   - Climate 최종 Fan/Thermal 목표 계산       -> ClimateManager
 *   - 기능 실행 가능 여부 판단                 -> PermissionManager
 *   - Request 중복/결과 이력                    -> RequestManager
 *   - 설정 영구 저장                            -> 현재 요구 미확정
 *
 * ============================================================================
 * 핵심 의미 분리
 * ============================================================================
 *
 * Requested Setting
 *      !=
 * Confirmed Setting
 *      !=
 * Commanded Target
 *      !=
 * Applied ECU State
 *      !=
 * Measured State
 *
 * 설정 Request의 DONE은 "Domain이 반영값을 확정"한 의미다.
 * 실제 BCM 적용/목표 온도 도달/물리 점등 확인과 동일하지 않다.
 */

#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Domain_Interface.h"
#include "Vehicle_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS
#define SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS (4U)
#endif

typedef enum
{
    SETTINGS_STATUS_OK = 0,
    SETTINGS_STATUS_INVALID_ARGUMENT,
    SETTINGS_STATUS_NOT_INITIALIZED,
    SETTINGS_STATUS_UNSUPPORTED,
    SETTINGS_STATUS_OUT_OF_RANGE,
    SETTINGS_STATUS_CONFIG_REQUIRED,
    SETTINGS_STATUS_BUSY,
    SETTINGS_STATUS_NOT_FOUND,
    SETTINGS_STATUS_CONTEXT_MISMATCH
} SettingsManager_Status_t;

typedef enum
{
    SETTINGS_APPLY_CONFIRMED = 0,
    SETTINGS_APPLY_REQUIRES_EXECUTION,
    SETTINGS_APPLY_NOT_A_SETTING,
    SETTINGS_APPLY_UNSUPPORTED,
    SETTINGS_APPLY_INVALID_VALUE,
    SETTINGS_APPLY_CONTEXT_MISMATCH,
    SETTINGS_APPLY_BUSY
} SettingsApplyResult_t;

/**
 * @brief 현재 일반 공조에서 마지막으로 명시적으로 선택된 모드.
 *
 * TARGET temperature 변경만으로 mode를 시작하지 않는다.
 */
typedef enum
{
    SETTINGS_CLIMATE_MODE_NONE = 0,
    SETTINGS_CLIMATE_MODE_MANUAL,
    SETTINGS_CLIMATE_MODE_AUTO
} SettingsClimateMode_t;

/**
 * @brief 실제 프로젝트 지원 범위/구성을 연결하는 설정.
 *
 * SysRS에는 "지원 범위 확인" 요구가 있지만 목표 온도의 숫자 범위는
 * 현재 기준 문서에서 확정되지 않았으므로 코드가 임의 범위를 만들지 않는다.
 */
typedef struct
{
    bool targetTemperatureRangeConfigured;
    int16_t targetTemperatureMin;
    int16_t targetTemperatureMax;

    bool supportAutoClimate;
    bool supportManualFan;
    bool supportInteriorLight;
    bool supportDigitalKey;
} SettingsManager_Config_t;

typedef struct
{
    bool valid;

    DeviceContextId_t deviceContextId;
    RequestContext_t requestContext;
    uint32_t sessionGeneration;

    uint32_t updatedAtMs;
    uint32_t revision;
} SettingsProvenance_t;

typedef struct
{
    bool hasTargetTemperature;
    int16_t targetTemperature;
    SettingsProvenance_t targetTemperatureSource;

    bool hasAutoClimateSetting;
    bool autoClimateEnabled;
    SettingsProvenance_t autoClimateSource;

    bool hasManualFanSetting;
    FanLevel_t manualFanLevel;
    SettingsProvenance_t manualFanSource;

    SettingsClimateMode_t activeMode;
    uint32_t revision;
} ClimateUserSettings_t;

typedef struct
{
    bool hasUserEnableSetting;
    bool userEnabled;
    SettingsProvenance_t userEnableSource;

    bool hasBrightnessSetting;
    uint8_t brightnessPercent;
    SettingsProvenance_t brightnessSource;

    bool hasColorSetting;
    RgbColor_t color;
    SettingsProvenance_t colorSource;

    uint32_t revision;
} InteriorLightUserSettings_t;

/**
 * @brief Digital Key 설정은 새 연결마다 OFF로 시작한다.
 *
 * 따라서 차량 전역 설정이 아니라 Device + Session Generation Context에 묶는다.
 */
typedef struct
{
    bool inUse;
    bool contextActive;

    DeviceContextId_t deviceContextId;
    SessionId_t sessionId;
    uint32_t sessionGeneration;

    DigitalKeySetting_t setting;
    SettingsProvenance_t source;

    uint32_t revision;
    uint32_t contextStartedAtMs;
} DigitalKeyUserSetting_t;

typedef struct
{
    bool active;

    DeviceContextId_t deviceContextId;
    RequestContext_t requestContext;
    uint32_t sessionGeneration;

    FanLevel_t requestedLevel;
    uint32_t stagedAtMs;
} PendingManualFanSetting_t;

typedef struct
{
    ClimateUserSettings_t climate;
    InteriorLightUserSettings_t interiorLight;
    DigitalKeyUserSetting_t
        digitalKey[SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS];

    PendingManualFanSetting_t pendingManualFan;

    uint32_t globalRevision;
} SettingsManager_Snapshot_t;


/* ============================================================================
 * Init
 * ============================================================================
 */

/**
 * @brief 현재 프로젝트의 지원 기능 기본값 로드.
 *
 * - Auto Climate / Manual Fan / Interior Light / Digital Key: 지원
 * - Target Temperature 숫자 범위: 미확정 -> configured=false
 */
void SettingsManager_LoadCurrentProjectDefaults(
    SettingsManager_Config_t *outConfig);

SettingsManager_Status_t SettingsManager_Init(
    const SettingsManager_Config_t *config);

bool SettingsManager_IsInitialized(void);


/* ============================================================================
 * Gateway Session 연계
 * ============================================================================
 */

/**
 * @brief 새 유효 Gateway Session 활성화.
 *
 * Digital Key setting은 새 연결에서 OFF로 초기화한다.
 */
SettingsManager_Status_t SettingsManager_OnSessionActivated(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t sessionGeneration,
    uint32_t nowMs);

/**
 * @brief 현재 Connection-specific setting context 비활성화.
 *
 * 과거 Request History와는 무관하다.
 */
SettingsManager_Status_t SettingsManager_OnSessionInvalidated(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t sessionGeneration,
    uint32_t nowMs);


/* ============================================================================
 * MOBILE Setting Request
 * ============================================================================
 */

/**
 * @brief MOBILE Request 중 설정 성격의 요청을 반영/Stage한다.
 *
 * 즉시 Confirm되는 항목:
 * - Target Temperature
 * - Auto Climate Enable
 * - Interior Light Enable
 * - Interior Light Brightness
 * - Interior Light Color
 * - Digital Key Setting
 *
 * Manual Fan:
 * - BCM 실행 결과가 필요하므로 Pending으로 Stage
 * - outResult = SETTINGS_APPLY_REQUIRES_EXECUTION
 * - BCM 결과 확인 후 CommitManualFanSelection()
 *
 * Door Lock/Unlock:
 * - Setting이 아니므로 SETTINGS_APPLY_NOT_A_SETTING
 */
SettingsManager_Status_t SettingsManager_ApplyMobileSettingRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    SettingsApplyResult_t *outResult);

/**
 * @brief Pending Manual Fan 실행이 정상 확인된 뒤 확정.
 *
 * 확정 시:
 * - Manual Fan setting 저장
 * - activeMode = MANUAL
 * - Auto Climate Enabled = false
 *
 * 자동 모드가 수동 선택으로 대체된다는 SysRS 정책을 반영한다.
 */
SettingsManager_Status_t SettingsManager_CommitManualFanSelection(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext,
    uint32_t sessionGeneration,
    uint32_t nowMs);

/**
 * @brief Manual Fan 실행 실패/취소/대체 시 Pending 제거.
 *
 * 기존 confirmed setting을 실패한 새 요청으로 덮어쓰지 않는다.
 */
SettingsManager_Status_t SettingsManager_CancelPendingManualFan(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext,
    uint32_t sessionGeneration);


/* ============================================================================
 * Query
 * ============================================================================
 */

SettingsManager_Status_t SettingsManager_GetSnapshot(
    SettingsManager_Snapshot_t *outSnapshot);

SettingsManager_Status_t SettingsManager_GetClimate(
    ClimateUserSettings_t *outSettings);

SettingsManager_Status_t SettingsManager_GetInteriorLight(
    InteriorLightUserSettings_t *outSettings);

SettingsManager_Status_t SettingsManager_GetDigitalKey(
    DeviceContextId_t deviceContextId,
    DigitalKeyUserSetting_t *outSetting);

bool SettingsManager_HasPendingManualFan(void);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_MANAGER_H */
