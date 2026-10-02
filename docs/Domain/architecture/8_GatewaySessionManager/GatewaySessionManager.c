/**
 * @file GatewaySessionManager.c
 */

#include "GatewaySessionManager.h"

#include <string.h>

typedef struct
{
    bool initialized;
    GatewaySessionConfig_t config;
    GatewaySessionRecord_t records[GATEWAY_SESSION_MAX_DEVICES];
} GatewaySessionContext_t;

static GatewaySessionContext_t g_gsm;

static void Gsm_ClearTransition(
    GatewaySessionTransition_t *transition)
{
    if (transition != NULL)
    {
        (void)memset(transition, 0, sizeof(*transition));
        transition->type = GSM_TRANSITION_NONE;
    }
}

static GatewaySessionRecord_t *Gsm_FindMutable(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U; i < GATEWAY_SESSION_MAX_DEVICES; ++i)
    {
        if (g_gsm.records[i].inUse &&
            (g_gsm.records[i].deviceContextId == deviceContextId))
        {
            return &g_gsm.records[i];
        }
    }

    return NULL;
}

static const GatewaySessionRecord_t *Gsm_FindConst(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U; i < GATEWAY_SESSION_MAX_DEVICES; ++i)
    {
        if (g_gsm.records[i].inUse &&
            (g_gsm.records[i].deviceContextId == deviceContextId))
        {
            return &g_gsm.records[i];
        }
    }

    return NULL;
}

static GatewaySessionRecord_t *Gsm_Allocate(
    DeviceContextId_t deviceContextId)
{
    uint32_t i;

    for (i = 0U; i < GATEWAY_SESSION_MAX_DEVICES; ++i)
    {
        if (!g_gsm.records[i].inUse)
        {
            GatewaySessionRecord_t *record = &g_gsm.records[i];

            (void)memset(record, 0, sizeof(*record));
            record->inUse = true;
            record->deviceContextId = deviceContextId;
            record->sessionState = GSM_SESSION_STATE_UNKNOWN;

            record->connection.registration =
                DEVICE_REGISTRATION_UNKNOWN;
            record->connection.btConnection =
                BT_CONNECTION_UNKNOWN;
            record->connection.appActive =
                APP_ACTIVE_UNKNOWN;
            record->connection.meta.quality =
                DATA_QUALITY_NO_DATA;

            return record;
        }
    }

    return NULL;
}

static GatewaySessionRecord_t *Gsm_FindOrAllocate(
    DeviceContextId_t deviceContextId)
{
    GatewaySessionRecord_t *record =
        Gsm_FindMutable(deviceContextId);

    if (record != NULL)
    {
        return record;
    }

    return Gsm_Allocate(deviceContextId);
}

static bool Gsm_IsRetired(
    const GatewaySessionRecord_t *record,
    SessionId_t sessionId)
{
    uint32_t i;

    if (record == NULL)
    {
        return false;
    }

    for (i = 0U; i < record->retiredCount; ++i)
    {
        if (record->retiredSessions[i] == sessionId)
        {
            return true;
        }
    }

    return false;
}

static void Gsm_AddRetired(
    GatewaySessionRecord_t *record,
    SessionId_t sessionId)
{
    if (record == NULL)
    {
        return;
    }

    if (Gsm_IsRetired(record, sessionId))
    {
        return;
    }

    record->retiredSessions[record->retiredWriteIndex] = sessionId;

    record->retiredWriteIndex =
        (uint8_t)((record->retiredWriteIndex + 1U) %
                  GATEWAY_SESSION_RETIRED_PER_DEVICE);

    if (record->retiredCount < GATEWAY_SESSION_RETIRED_PER_DEVICE)
    {
        ++record->retiredCount;
    }
}

static bool Gsm_ConnectionAllowsSession(
    const DigitalKeyConnectionState_t *connection)
{
    if (connection == NULL)
    {
        return false;
    }

    return ((connection->meta.quality == DATA_QUALITY_OK) &&
            (connection->registration ==
             DEVICE_REGISTRATION_REGISTERED) &&
            (connection->btConnection ==
             BT_CONNECTION_CONNECTED));
}

static void Gsm_InvalidateRecord(
    GatewaySessionRecord_t *record,
    uint32_t nowMs,
    GatewaySessionTransition_t *outTransition)
{
    if ((record == NULL) || (!record->hasActiveSession))
    {
        if (record != NULL)
        {
            record->sessionState = GSM_SESSION_STATE_INACTIVE;
            record->lastUpdatedAtMs = nowMs;
        }
        return;
    }

    if (outTransition != NULL)
    {
        outTransition->type =
            GSM_TRANSITION_SESSION_INVALIDATED;
        outTransition->deviceContextId =
            record->deviceContextId;
        outTransition->hadPreviousSession = true;
        outTransition->previousSessionId =
            record->activeSessionId;
        outTransition->hasCurrentSession = false;
        outTransition->sessionGeneration =
            record->sessionGeneration;
    }

    Gsm_AddRetired(record, record->activeSessionId);

    record->hasActiveSession = false;
    record->sessionState = GSM_SESSION_STATE_INACTIVE;
    record->lastUpdatedAtMs = nowMs;
    ++record->sessionGeneration;
}

GatewaySessionStatus_t GatewaySessionManager_Init(
    const GatewaySessionConfig_t *config)
{
    (void)memset(&g_gsm, 0, sizeof(g_gsm));

    if (config != NULL)
    {
        g_gsm.config = *config;
    }
    else
    {
        /*
         * 현재 Interface에 별도 Session Generation 정보가 확정되지 않았으므로
         * 과거 Session ID 재사용을 기본적으로 차단하는 것이 안전하다.
         */
        g_gsm.config.reusePolicy = GSM_SESSION_REUSE_STRICT;
    }

    g_gsm.initialized = true;
    return GSM_STATUS_OK;
}

bool GatewaySessionManager_IsInitialized(void)
{
    return g_gsm.initialized;
}

GatewaySessionStatus_t GatewaySessionManager_UpdateConnection(
    DeviceContextId_t deviceContextId,
    const DigitalKeyConnectionState_t *connection,
    uint32_t nowMs,
    GatewaySessionTransition_t *outTransition)
{
    GatewaySessionRecord_t *record;
    bool sessionAllowed;

    if (connection == NULL)
    {
        return GSM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    Gsm_ClearTransition(outTransition);

    record = Gsm_FindOrAllocate(deviceContextId);

    if (record == NULL)
    {
        return GSM_STATUS_CAPACITY_FULL;
    }

    sessionAllowed = Gsm_ConnectionAllowsSession(connection);

    /*
     * Connection snapshot은 세션 활성 여부와 별개로 저장한다.
     * APP_ACTIVE=UNKNOWN/INACTIVE도 일반 Session 자체를 자동 폐기하지 않는다.
     */
    record->connection = *connection;
    record->lastUpdatedAtMs = nowMs;

    if (!sessionAllowed)
    {
        Gsm_InvalidateRecord(record, nowMs, outTransition);
        return GSM_STATUS_OK;
    }

    if (record->hasActiveSession)
    {
        if (record->activeSessionId == connection->sessionId)
        {
            record->sessionState = GSM_SESSION_STATE_ACTIVE;
            return GSM_STATUS_OK;
        }

        /* Current session changed while connection remains valid. */
        if (outTransition != NULL)
        {
            outTransition->type =
                GSM_TRANSITION_SESSION_CHANGED;
            outTransition->deviceContextId =
                deviceContextId;
            outTransition->hadPreviousSession = true;
            outTransition->previousSessionId =
                record->activeSessionId;
            outTransition->hasCurrentSession = true;
            outTransition->currentSessionId =
                connection->sessionId;
        }

        Gsm_AddRetired(record, record->activeSessionId);

        if ((g_gsm.config.reusePolicy ==
             GSM_SESSION_REUSE_STRICT) &&
            Gsm_IsRetired(record, connection->sessionId))
        {
            record->hasActiveSession = false;
            record->sessionState =
                GSM_SESSION_STATE_REJECTED_REUSE;
            ++record->sessionGeneration;

            if (outTransition != NULL)
            {
                outTransition->type =
                    GSM_TRANSITION_SESSION_REUSE_REJECTED;
                outTransition->hasCurrentSession = false;
                outTransition->sessionGeneration =
                    record->sessionGeneration;
            }

            return GSM_STATUS_SESSION_REUSE_BLOCKED;
        }

        record->activeSessionId = connection->sessionId;
        record->hasActiveSession = true;
        record->sessionState = GSM_SESSION_STATE_ACTIVE;
        ++record->sessionGeneration;

        if (outTransition != NULL)
        {
            outTransition->sessionGeneration =
                record->sessionGeneration;
        }

        return GSM_STATUS_OK;
    }

    /*
     * No active session currently.
     * Strict policy prevents a retired runtime session from becoming active
     * again when there is no separate boot/session generation evidence.
     */
    if ((g_gsm.config.reusePolicy ==
         GSM_SESSION_REUSE_STRICT) &&
        Gsm_IsRetired(record, connection->sessionId))
    {
        record->sessionState =
            GSM_SESSION_STATE_REJECTED_REUSE;
        ++record->sessionGeneration;

        if (outTransition != NULL)
        {
            outTransition->type =
                GSM_TRANSITION_SESSION_REUSE_REJECTED;
            outTransition->deviceContextId =
                deviceContextId;
            outTransition->hasCurrentSession = false;
            outTransition->sessionGeneration =
                record->sessionGeneration;
        }

        return GSM_STATUS_SESSION_REUSE_BLOCKED;
    }

    record->activeSessionId = connection->sessionId;
    record->hasActiveSession = true;
    record->sessionState = GSM_SESSION_STATE_ACTIVE;
    ++record->sessionGeneration;

    if (outTransition != NULL)
    {
        outTransition->type =
            GSM_TRANSITION_SESSION_ACTIVATED;
        outTransition->deviceContextId =
            deviceContextId;
        outTransition->hasCurrentSession = true;
        outTransition->currentSessionId =
            record->activeSessionId;
        outTransition->sessionGeneration =
            record->sessionGeneration;
    }

    return GSM_STATUS_OK;
}

GatewaySessionStatus_t GatewaySessionManager_InvalidateDevice(
    DeviceContextId_t deviceContextId,
    uint32_t nowMs,
    GatewaySessionTransition_t *outTransition)
{
    GatewaySessionRecord_t *record;

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    Gsm_ClearTransition(outTransition);

    record = Gsm_FindMutable(deviceContextId);

    if (record == NULL)
    {
        return GSM_STATUS_NOT_FOUND;
    }

    Gsm_InvalidateRecord(record, nowMs, outTransition);

    record->connection.registration =
        DEVICE_REGISTRATION_UNKNOWN;
    record->connection.btConnection =
        BT_CONNECTION_UNKNOWN;
    record->connection.appActive =
        APP_ACTIVE_UNKNOWN;
    record->connection.meta.quality =
        DATA_QUALITY_NO_DATA;

    return GSM_STATUS_OK;
}

GatewaySessionStatus_t GatewaySessionManager_InvalidateAll(
    uint32_t nowMs)
{
    uint32_t i;

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    for (i = 0U; i < GATEWAY_SESSION_MAX_DEVICES; ++i)
    {
        GatewaySessionRecord_t *record = &g_gsm.records[i];

        if (!record->inUse)
        {
            continue;
        }

        Gsm_InvalidateRecord(record, nowMs, NULL);

        record->connection.registration =
            DEVICE_REGISTRATION_UNKNOWN;
        record->connection.btConnection =
            BT_CONNECTION_UNKNOWN;
        record->connection.appActive =
            APP_ACTIVE_UNKNOWN;
        record->connection.meta.quality =
            DATA_QUALITY_NO_DATA;
    }

    return GSM_STATUS_OK;
}

GatewayRequestValidation_t GatewaySessionManager_ValidateRequest(
    DeviceContextId_t deviceContextId,
    const RequestContext_t *requestContext)
{
    const GatewaySessionRecord_t *record;

    if ((!g_gsm.initialized) || (requestContext == NULL))
    {
        return GSM_REQUEST_NO_DEVICE_CONTEXT;
    }

    record = Gsm_FindConst(deviceContextId);

    if (record == NULL)
    {
        return GSM_REQUEST_NO_DEVICE_CONTEXT;
    }

    if (record->connection.meta.quality != DATA_QUALITY_OK)
    {
        return GSM_REQUEST_CONNECTION_QUALITY_INVALID;
    }

    if (record->connection.registration !=
        DEVICE_REGISTRATION_REGISTERED)
    {
        return GSM_REQUEST_DEVICE_NOT_REGISTERED;
    }

    if (record->connection.btConnection !=
        BT_CONNECTION_CONNECTED)
    {
        return GSM_REQUEST_BT_NOT_CONNECTED;
    }

    if (Gsm_IsRetired(record, requestContext->sessionId))
    {
        return GSM_REQUEST_RETIRED_SESSION;
    }

    if (record->sessionState ==
        GSM_SESSION_STATE_REJECTED_REUSE)
    {
        return GSM_REQUEST_SESSION_REUSE_BLOCKED;
    }

    if (!record->hasActiveSession)
    {
        return GSM_REQUEST_NO_ACTIVE_SESSION;
    }

    if (record->activeSessionId !=
        requestContext->sessionId)
    {
        return GSM_REQUEST_SESSION_MISMATCH;
    }

    return GSM_REQUEST_VALID;
}

bool GatewaySessionManager_HasActiveSession(
    DeviceContextId_t deviceContextId)
{
    const GatewaySessionRecord_t *record;

    if (!g_gsm.initialized)
    {
        return false;
    }

    record = Gsm_FindConst(deviceContextId);

    return ((record != NULL) &&
            record->hasActiveSession &&
            (record->sessionState ==
             GSM_SESSION_STATE_ACTIVE));
}

GatewaySessionStatus_t GatewaySessionManager_GetAppActive(
    DeviceContextId_t deviceContextId,
    AppActiveState_t *outState)
{
    const GatewaySessionRecord_t *record;

    if (outState == NULL)
    {
        return GSM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    record = Gsm_FindConst(deviceContextId);

    if (record == NULL)
    {
        return GSM_STATUS_NOT_FOUND;
    }

    *outState = record->connection.appActive;
    return GSM_STATUS_OK;
}

GatewaySessionStatus_t GatewaySessionManager_GetRecord(
    DeviceContextId_t deviceContextId,
    GatewaySessionRecord_t *outRecord)
{
    const GatewaySessionRecord_t *record;

    if (outRecord == NULL)
    {
        return GSM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    record = Gsm_FindConst(deviceContextId);

    if (record == NULL)
    {
        return GSM_STATUS_NOT_FOUND;
    }

    *outRecord = *record;
    return GSM_STATUS_OK;
}

GatewaySessionStatus_t GatewaySessionManager_GetActiveSession(
    DeviceContextId_t deviceContextId,
    SessionId_t *outSessionId,
    uint32_t *outGeneration)
{
    const GatewaySessionRecord_t *record;

    if ((outSessionId == NULL) ||
        (outGeneration == NULL))
    {
        return GSM_STATUS_INVALID_ARGUMENT;
    }

    if (!g_gsm.initialized)
    {
        return GSM_STATUS_NOT_INITIALIZED;
    }

    record = Gsm_FindConst(deviceContextId);

    if ((record == NULL) ||
        (!record->hasActiveSession) ||
        (record->sessionState !=
         GSM_SESSION_STATE_ACTIVE))
    {
        return GSM_STATUS_NOT_FOUND;
    }

    *outSessionId = record->activeSessionId;
    *outGeneration = record->sessionGeneration;

    return GSM_STATUS_OK;
}
