/**
 * @file SettingsManager.c
 */

#include "SettingsManager.h"

#include <string.h>

typedef struct
{
    bool initialized;
    SettingsManager_Config_t config;
    SettingsManager_Snapshot_t snapshot;
} SettingsManager_Context_t;

static SettingsManager_Context_t g_settings;

static void Settings_ClearProvenance(
    SettingsProvenance_t *source)
{
    if (source != NULL)
    {
        (void)memset(source, 0, sizeof(*source));
    }
}

static void Settings_SetProvenance(
    SettingsProvenance_t *source,
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    uint32_t revision)
{
    source->valid = true;
    source->deviceContextId = request->deviceContextId;
    source->requestContext = request->context;
    source->sessionGeneration = sessionGeneration;
    source->updatedAtMs = nowMs;
    source->revision = revision;
}

static bool Settings_IsSupportedFanLevel(
    FanLevel_t level)
{
    return ((level == FAN_LEVEL_OFF) ||
            (level == FAN_LEVEL_LOW) ||
            (level == FAN_LEVEL_MEDIUM) ||
            (level == FAN_LEVEL_HIGH));
}

static bool Settings_RequestContextEqual(
    const RequestContext_t *a,
    const RequestContext_t *b)
{
    if ((a == NULL) || (b == NULL))
    {
        return false;
    }

    return ((a->sessionId == b->sessionId) &&
            (a->requestId == b->requestId));
}

static DigitalKeyUserSetting_t *Settings_FindDigitalKeyMutable(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U;
         i < SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS;
         ++i)
    {
        DigitalKeyUserSetting_t *record =
            &g_settings.snapshot.digitalKey[i];

        if (record->inUse &&
            (record->deviceContextId == deviceContextId))
        {
            return record;
        }
    }

    return NULL;
}

static const DigitalKeyUserSetting_t *Settings_FindDigitalKeyConst(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U;
         i < SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS;
         ++i)
    {
        const DigitalKeyUserSetting_t *record =
            &g_settings.snapshot.digitalKey[i];

        if (record->inUse &&
            (record->deviceContextId == deviceContextId))
        {
            return record;
        }
    }

    return NULL;
}

static DigitalKeyUserSetting_t *Settings_AllocateDigitalKey(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U;
         i < SETTINGS_MANAGER_MAX_DEVICE_CONTEXTS;
         ++i)
    {
        DigitalKeyUserSetting_t *record =
            &g_settings.snapshot.digitalKey[i];

        if (!record->inUse)
        {
            (void)memset(record, 0, sizeof(*record));
            record->inUse = true;
            record->deviceContextId = deviceContextId;
            return record;
        }
    }

    return NULL;
}

static uint32_t Settings_NextRevision(void)
{
    ++g_settings.snapshot.globalRevision;

    if (g_settings.snapshot.globalRevision == 0U)
    {
        /* 0은 "아직 갱신 없음"으로 사용하므로 wrap 시 1부터 재개. */
        g_settings.snapshot.globalRevision = 1U;
    }

    return g_settings.snapshot.globalRevision;
}

static SettingsManager_Status_t Settings_ApplyTargetTemperature(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    ClimateUserSettings_t *climate =
        &g_settings.snapshot.climate;
    uint32_t revision;

    if (!g_settings.config.targetTemperatureRangeConfigured)
    {
        return SETTINGS_STATUS_CONFIG_REQUIRED;
    }

    if ((request->payload.targetTemperature <
         g_settings.config.targetTemperatureMin) ||
        (request->payload.targetTemperature >
         g_settings.config.targetTemperatureMax))
    {
        return SETTINGS_STATUS_OUT_OF_RANGE;
    }

    revision = Settings_NextRevision();

    climate->hasTargetTemperature = true;
    climate->targetTemperature =
        request->payload.targetTemperature;
    climate->revision = revision;

    Settings_SetProvenance(
        &climate->targetTemperatureSource,
        request,
        sessionGeneration,
        nowMs,
        revision);

    /*
     * SysRS:
     * Target temperature 변경만으로 꺼진 공조를 새로 시작하지 않는다.
     * 따라서 activeMode는 여기서 변경하지 않는다.
     */

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_ApplyAutoClimate(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    ClimateUserSettings_t *climate =
        &g_settings.snapshot.climate;
    uint32_t revision;

    if (!g_settings.config.supportAutoClimate)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    revision = Settings_NextRevision();

    climate->hasAutoClimateSetting = true;
    climate->autoClimateEnabled = request->payload.enable;
    climate->revision = revision;

    Settings_SetProvenance(
        &climate->autoClimateSource,
        request,
        sessionGeneration,
        nowMs,
        revision);

    if (request->payload.enable)
    {
        /*
         * Auto 선택은 기존 manual 동작을 대체한다.
         * 마지막 manual setting 값 자체는 정보로 남길 수 있지만
         * active mode는 AUTO가 된다.
         */
        climate->activeMode = SETTINGS_CLIMATE_MODE_AUTO;
    }
    else if (climate->activeMode == SETTINGS_CLIMATE_MODE_AUTO)
    {
        /*
         * Auto 사용 해제는 해당 Auto mode만 종료.
         * 과거 Manual mode를 자동 복원하지 않는다.
         */
        climate->activeMode = SETTINGS_CLIMATE_MODE_NONE;
    }

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_ApplyInteriorLightEnable(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    InteriorLightUserSettings_t *light =
        &g_settings.snapshot.interiorLight;
    uint32_t revision;

    if (!g_settings.config.supportInteriorLight)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    revision = Settings_NextRevision();

    light->hasUserEnableSetting = true;
    light->userEnabled = request->payload.enable;
    light->revision = revision;

    Settings_SetProvenance(
        &light->userEnableSource,
        request,
        sessionGeneration,
        nowMs,
        revision);

    /*
     * User OFF는 NORMAL 표현에만 적용한다.
     * WARNING/FAULT/GOODBYE 같은 최종 출력 중재는
     * InteriorLightManager 책임이다.
     */

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_ApplyInteriorLightLevel(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    InteriorLightUserSettings_t *light =
        &g_settings.snapshot.interiorLight;
    uint32_t revision;

    if (!g_settings.config.supportInteriorLight)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    if (request->payload.levelPercent > 100U)
    {
        return SETTINGS_STATUS_OUT_OF_RANGE;
    }

    revision = Settings_NextRevision();

    light->hasBrightnessSetting = true;
    light->brightnessPercent = request->payload.levelPercent;
    light->revision = revision;

    Settings_SetProvenance(
        &light->brightnessSource,
        request,
        sessionGeneration,
        nowMs,
        revision);

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_ApplyInteriorLightColor(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    InteriorLightUserSettings_t *light =
        &g_settings.snapshot.interiorLight;
    uint32_t revision;

    if (!g_settings.config.supportInteriorLight)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    /*
     * RgbColor_t channel의 실제 Wire scaling/representation은 Network TBD.
     * 현재 C type 범위 내 값 자체는 구조적으로 유효하다.
     */
    revision = Settings_NextRevision();

    light->hasColorSetting = true;
    light->color = request->payload.color;
    light->revision = revision;

    Settings_SetProvenance(
        &light->colorSource,
        request,
        sessionGeneration,
        nowMs,
        revision);

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_ApplyDigitalKey(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    DigitalKeyUserSetting_t *record;
    uint32_t revision;

    if (!g_settings.config.supportDigitalKey)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    if ((request->payload.digitalKeySetting !=
         DIGITAL_KEY_SETTING_OFF) &&
        (request->payload.digitalKeySetting !=
         DIGITAL_KEY_SETTING_ON))
    {
        return SETTINGS_STATUS_OUT_OF_RANGE;
    }

    record = Settings_FindDigitalKeyMutable(
        request->deviceContextId);

    if ((record == NULL) ||
        (!record->contextActive) ||
        (record->sessionId != request->context.sessionId) ||
        (record->sessionGeneration != sessionGeneration))
    {
        return SETTINGS_STATUS_CONTEXT_MISMATCH;
    }

    revision = Settings_NextRevision();

    record->setting = request->payload.digitalKeySetting;
    record->revision = revision;

    Settings_SetProvenance(
        &record->source,
        request,
        sessionGeneration,
        nowMs,
        revision);

    return SETTINGS_STATUS_OK;
}

static SettingsManager_Status_t Settings_StageManualFan(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    PendingManualFanSetting_t *pending =
        &g_settings.snapshot.pendingManualFan;

    if (!g_settings.config.supportManualFan)
    {
        return SETTINGS_STATUS_UNSUPPORTED;
    }

    if (!Settings_IsSupportedFanLevel(
            request->payload.fanLevel))
    {
        return SETTINGS_STATUS_OUT_OF_RANGE;
    }

    if (pending->active)
    {
        if ((pending->deviceContextId ==
             request->deviceContextId) &&
            (pending->sessionGeneration ==
             sessionGeneration) &&
            Settings_RequestContextEqual(
                &pending->requestContext,
                &request->context) &&
            (pending->requestedLevel ==
             request->payload.fanLevel))
        {
            /* 동일 Request 재전달은 idempotent stage. */
            return SETTINGS_STATUS_OK;
        }

        /*
         * 다른 pending request의 supersede/cancel 정책은
         * Request/Result orchestration에서 명시적으로 처리한 뒤
         * 새 request를 stage한다.
         */
        return SETTINGS_STATUS_BUSY;
    }

    pending->active = true;
    pending->deviceContextId =
        request->deviceContextId;
    pending->requestContext =
        request->context;
    pending->sessionGeneration =
        sessionGeneration;
    pending->requestedLevel =
        request->payload.fanLevel;
    pending->stagedAtMs = nowMs;

    return SETTINGS_STATUS_OK;
}

void SettingsManager_LoadCurrentProjectDefaults(
    SettingsManager_Config_t *outConfig)
{
    if (outConfig == NULL)
    {
        return;
    }

    (void)memset(outConfig, 0, sizeof(*outConfig));

    /*
     * 기능 범위는 현재 SysRS에 존재한다.
     * 단, 목표 온도 숫자 범위는 현재 문서에서 찾을 수 없어 미설정.
     */
    outConfig->targetTemperatureRangeConfigured = false;
    outConfig->supportAutoClimate = true;
    outConfig->supportManualFan = true;
    outConfig->supportInteriorLight = true;
    outConfig->supportDigitalKey = true;
}

SettingsManager_Status_t SettingsManager_Init(
    const SettingsManager_Config_t *config)
{
    SettingsManager_Config_t defaults;

    (void)memset(&g_settings, 0, sizeof(g_settings));

    if (config == NULL)
    {
        SettingsManager_LoadCurrentProjectDefaults(&defaults);
        config = &defaults;
    }

    if (config->targetTemperatureRangeConfigured &&
        (config->targetTemperatureMin >
         config->targetTemperatureMax))
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    g_settings.config = *config;

    /*
     * Climate/Light 초기 사용자 설정 기본값은 SysRS에서 확정되지 않았으므로
     * hasXXX=false로 시작한다.
     *
     * Digital Key는 session activation 시 OFF로 명시 초기화한다.
     */

    g_settings.initialized = true;
    return SETTINGS_STATUS_OK;
}

bool SettingsManager_IsInitialized(void)
{
    return g_settings.initialized;
}

SettingsManager_Status_t SettingsManager_OnSessionActivated(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    DigitalKeyUserSetting_t *record;
    uint32_t revision;

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    record = Settings_FindDigitalKeyMutable(deviceContextId);

    if (record == NULL)
    {
        record = Settings_AllocateDigitalKey(
            deviceContextId);

        if (record == NULL)
        {
            return SETTINGS_STATUS_BUSY;
        }
    }

    revision = Settings_NextRevision();

    record->contextActive = true;
    record->sessionId = sessionId;
    record->sessionGeneration = sessionGeneration;
    record->setting = DIGITAL_KEY_SETTING_OFF;
    record->revision = revision;
    record->contextStartedAtMs = nowMs;

    /*
     * Initial/new connection starts OFF.
     * 이는 MOBILE request로 발생한 변경이 아니므로 source는 invalid.
     */
    Settings_ClearProvenance(&record->source);

    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_OnSessionInvalidated(
    DeviceContextId_t deviceContextId,
    SessionId_t sessionId,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    DigitalKeyUserSetting_t *record;

    (void)nowMs;

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    record = Settings_FindDigitalKeyMutable(
        deviceContextId);

    if (record == NULL)
    {
        return SETTINGS_STATUS_NOT_FOUND;
    }

    if ((!record->contextActive) ||
        (record->sessionId != sessionId) ||
        (record->sessionGeneration != sessionGeneration))
    {
        return SETTINGS_STATUS_CONTEXT_MISMATCH;
    }

    record->contextActive = false;

    /*
     * old context의 setting을 새 연결에 이월하지 않는다.
     * 다음 SessionActivated에서 OFF로 시작한다.
     */

    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_ApplyMobileSettingRequest(
    const DomainIf_MobileRequest_t *request,
    uint32_t sessionGeneration,
    uint32_t nowMs,
    SettingsApplyResult_t *outResult)
{
    SettingsManager_Status_t status;

    if ((request == NULL) || (outResult == NULL))
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    *outResult = SETTINGS_APPLY_INVALID_VALUE;

    switch (request->type)
    {
        case MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE:
            status = Settings_ApplyTargetTemperature(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_CLIMATE_AUTO_ENABLE:
            status = Settings_ApplyAutoClimate(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_FAN_LEVEL:
            status = Settings_StageManualFan(
                request, sessionGeneration, nowMs);

            if (status == SETTINGS_STATUS_OK)
            {
                *outResult =
                    SETTINGS_APPLY_REQUIRES_EXECUTION;
                return SETTINGS_STATUS_OK;
            }
            break;

        case MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE:
            status = Settings_ApplyInteriorLightEnable(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL:
            status = Settings_ApplyInteriorLightLevel(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_INTERIOR_LIGHT_COLOR:
            status = Settings_ApplyInteriorLightColor(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_DIGITAL_KEY_SETTING:
            status = Settings_ApplyDigitalKey(
                request, sessionGeneration, nowMs);
            break;

        case MOBILE_REQUEST_DOOR_LOCK:
        default:
            *outResult = SETTINGS_APPLY_NOT_A_SETTING;
            return SETTINGS_STATUS_UNSUPPORTED;
    }

    if (status == SETTINGS_STATUS_OK)
    {
        *outResult = SETTINGS_APPLY_CONFIRMED;
    }
    else if (status == SETTINGS_STATUS_UNSUPPORTED)
    {
        *outResult = SETTINGS_APPLY_UNSUPPORTED;
    }
    else if (status == SETTINGS_STATUS_CONTEXT_MISMATCH)
    {
        *outResult = SETTINGS_APPLY_CONTEXT_MISMATCH;
    }
    else if (status == SETTINGS_STATUS_BUSY)
    {
        *outResult = SETTINGS_APPLY_BUSY;
    }
    else
    {
        *outResult = SETTINGS_APPLY_INVALID_VALUE;
    }

    return status;
}

SettingsManager_Status_t SettingsManager_CommitManualFanSelection(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext,
    uint32_t sessionGeneration,
    uint32_t nowMs)
{
    PendingManualFanSetting_t *pending =
        &g_settings.snapshot.pendingManualFan;
    ClimateUserSettings_t *climate =
        &g_settings.snapshot.climate;
    DomainIf_MobileRequest_t syntheticSource;
    uint32_t revision;

    if (requestContext == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    if ((!pending->active) ||
        (pending->deviceContextId != deviceContextId) ||
        (pending->sessionGeneration != sessionGeneration) ||
        (!Settings_RequestContextEqual(
            &pending->requestContext,
            requestContext)))
    {
        return SETTINGS_STATUS_CONTEXT_MISMATCH;
    }

    revision = Settings_NextRevision();

    climate->hasManualFanSetting = true;
    climate->manualFanLevel =
        pending->requestedLevel;
    climate->activeMode =
        SETTINGS_CLIMATE_MODE_MANUAL;

    /*
     * Manual fan selection ends Auto temperature control.
     */
    climate->hasAutoClimateSetting = true;
    climate->autoClimateEnabled = false;
    climate->revision = revision;

    (void)memset(
        &syntheticSource,
        0,
        sizeof(syntheticSource));

    syntheticSource.deviceContextId =
        deviceContextId;
    syntheticSource.context =
        *requestContext;
    syntheticSource.type =
        MOBILE_REQUEST_FAN_LEVEL;
    syntheticSource.payload.fanLevel =
        pending->requestedLevel;

    Settings_SetProvenance(
        &climate->manualFanSource,
        &syntheticSource,
        sessionGeneration,
        nowMs,
        revision);

    /*
     * Auto-off는 manual selection에 의해 파생된 변경이다.
     * 같은 원 요청을 source로 연결한다.
     */
    Settings_SetProvenance(
        &climate->autoClimateSource,
        &syntheticSource,
        sessionGeneration,
        nowMs,
        revision);

    (void)memset(pending, 0, sizeof(*pending));

    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_CancelPendingManualFan(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext,
    uint32_t sessionGeneration)
{
    PendingManualFanSetting_t *pending =
        &g_settings.snapshot.pendingManualFan;

    if (requestContext == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    if ((!pending->active) ||
        (pending->deviceContextId != deviceContextId) ||
        (pending->sessionGeneration != sessionGeneration) ||
        (!Settings_RequestContextEqual(
            &pending->requestContext,
            requestContext)))
    {
        return SETTINGS_STATUS_CONTEXT_MISMATCH;
    }

    (void)memset(pending, 0, sizeof(*pending));
    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_GetSnapshot(
    SettingsManager_Snapshot_t *outSnapshot)
{
    if (outSnapshot == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    *outSnapshot = g_settings.snapshot;
    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_GetClimate(
    ClimateUserSettings_t *outSettings)
{
    if (outSettings == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    *outSettings = g_settings.snapshot.climate;
    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_GetInteriorLight(
    InteriorLightUserSettings_t *outSettings)
{
    if (outSettings == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    *outSettings =
        g_settings.snapshot.interiorLight;
    return SETTINGS_STATUS_OK;
}

SettingsManager_Status_t SettingsManager_GetDigitalKey(
    DeviceContextId_t deviceContextId,
    DigitalKeyUserSetting_t *outSetting)
{
    const DigitalKeyUserSetting_t *record;

    if (outSetting == NULL)
    {
        return SETTINGS_STATUS_INVALID_ARGUMENT;
    }

    if (!g_settings.initialized)
    {
        return SETTINGS_STATUS_NOT_INITIALIZED;
    }

    record = Settings_FindDigitalKeyConst(
        deviceContextId);

    if (record == NULL)
    {
        return SETTINGS_STATUS_NOT_FOUND;
    }

    *outSetting = *record;
    return SETTINGS_STATUS_OK;
}

bool SettingsManager_HasPendingManualFan(void)
{
    return (g_settings.initialized &&
            g_settings.snapshot.pendingManualFan.active);
}
