#include "DeviceRegistrationManager.h"

#include <string.h>

static DeviceRegistrationManager_Snapshot_t s_snapshot;


/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void DeviceRegistrationManager_ClearRef(
    DeviceRegistrationManager_DeviceRef_t *ref)
{
    if (ref == NULL)
    {
        return;
    }

    memset(ref->data, 0, sizeof(ref->data));
    ref->length = 0U;
}

static bool DeviceRegistrationManager_IsRefValid(
    const DeviceRegistrationManager_DeviceRef_t *ref)
{
    return (ref != NULL)
        && (ref->length <= DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES);
}

static Gateway_Status_t DeviceRegistrationManager_CopyViewToRef(
    const Gateway_ByteView_t *view,
    DeviceRegistrationManager_DeviceRef_t *ref)
{
    if ((view == NULL) || (ref == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!Gateway_Interface_IsValidByteView(view)
        || (view->length == 0U))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (view->length > DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    DeviceRegistrationManager_ClearRef(ref);

    memcpy(ref->data, view->data, view->length);
    ref->length = view->length;

    return GATEWAY_STATUS_OK;
}

static bool DeviceRegistrationManager_IsRefEqual(
    const DeviceRegistrationManager_DeviceRef_t *left,
    const DeviceRegistrationManager_DeviceRef_t *right)
{
    if (!DeviceRegistrationManager_IsRefValid(left)
        || !DeviceRegistrationManager_IsRefValid(right))
    {
        return false;
    }

    if (left->length != right->length)
    {
        return false;
    }

    if (left->length == 0U)
    {
        return true;
    }

    return memcmp(
        left->data,
        right->data,
        left->length) == 0;
}

static void DeviceRegistrationManager_NextObservation(
    Gateway_TimeMs_t now_ms)
{
    ++s_snapshot.observation_revision;

    if (s_snapshot.observation_revision == 0U)
    {
        ++s_snapshot.observation_revision;
    }

    s_snapshot.last_observed_ms = now_ms;
    s_snapshot.quality = GATEWAY_DATA_QUALITY_VALID;
}

static void DeviceRegistrationManager_RecalculateCurrentRegistration(void)
{
    if (s_snapshot.connection == GATEWAY_CONNECTED
        && s_snapshot.has_current_peer)
    {
        if (s_snapshot.has_registered_peer
            && DeviceRegistrationManager_IsRefEqual(
                &s_snapshot.current_peer,
                &s_snapshot.registered_peer))
        {
            s_snapshot.current_registration = GATEWAY_REGISTERED;
        }
        else
        {
            s_snapshot.current_registration = GATEWAY_NOT_REGISTERED;
        }

        return;
    }

    /*
     * 연결이 없더라도 차량 측에 등록 record가 남아 있는지는 알릴 수 있다.
     * 하지만 "authenticated current connection"은 아래 Query API가
     * REGISTERED + CONNECTED를 함께 확인한다.
     */
    s_snapshot.current_registration =
        s_snapshot.has_registered_peer
            ? GATEWAY_REGISTERED
            : GATEWAY_NOT_REGISTERED;
}

static Gateway_ByteView_t DeviceRegistrationManager_RefToView(
    const DeviceRegistrationManager_DeviceRef_t *ref)
{
    Gateway_ByteView_t view = {
        .data = NULL,
        .length = 0U
    };

    if ((ref != NULL) && (ref->length > 0U))
    {
        view.data = ref->data;
        view.length = ref->length;
    }

    return view;
}

static Gateway_ByteView_t DeviceRegistrationManager_GetReportDeviceView(void)
{
    if ((s_snapshot.connection == GATEWAY_CONNECTED)
        && s_snapshot.has_current_peer)
    {
        return DeviceRegistrationManager_RefToView(
            &s_snapshot.current_peer);
    }

    if (s_snapshot.has_registered_peer)
    {
        return DeviceRegistrationManager_RefToView(
            &s_snapshot.registered_peer);
    }

    return DeviceRegistrationManager_RefToView(NULL);
}

static Gateway_Status_t DeviceRegistrationManager_CheckReady(void)
{
    return s_snapshot.initialized
        ? GATEWAY_STATUS_OK
        : GATEWAY_STATUS_NOT_READY;
}


/* -------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_Init(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));

    s_snapshot.initialized = true;
    s_snapshot.has_registered_peer = false;
    s_snapshot.has_current_peer = false;

    s_snapshot.current_registration = GATEWAY_REGISTRATION_UNKNOWN;
    s_snapshot.connection = GATEWAY_CONNECTION_UNKNOWN;

    s_snapshot.quality = GATEWAY_DATA_QUALITY_UNKNOWN;
    s_snapshot.last_observed_ms = Gateway_Time_GetMs();

    s_snapshot.observation_revision = 0U;
    s_snapshot.last_published_revision = 0U;

    return GATEWAY_STATUS_OK;
}

void DeviceRegistrationManager_Reset(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
}

bool DeviceRegistrationManager_IsInitialized(void)
{
    return s_snapshot.initialized;
}


/* -------------------------------------------------------------------------- */
/* Registration events                                                        */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_OnRegistrationConfirmedAt(
    const Gateway_ByteView_t *device_context_ref,
    Gateway_TimeMs_t now_ms)
{
    DeviceRegistrationManager_DeviceRef_t candidate;
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    memset(&candidate, 0, sizeof(candidate));

    status = DeviceRegistrationManager_CopyViewToRef(
        device_context_ref,
        &candidate);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    s_snapshot.registered_peer = candidate;
    s_snapshot.has_registered_peer = true;

    DeviceRegistrationManager_RecalculateCurrentRegistration();
    DeviceRegistrationManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_OnRegistrationConfirmed(
    const Gateway_ByteView_t *device_context_ref)
{
    return DeviceRegistrationManager_OnRegistrationConfirmedAt(
        device_context_ref,
        Gateway_Time_GetMs());
}

Gateway_Status_t DeviceRegistrationManager_OnRegistrationRemovedAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    DeviceRegistrationManager_ClearRef(&s_snapshot.registered_peer);
    s_snapshot.has_registered_peer = false;

    DeviceRegistrationManager_RecalculateCurrentRegistration();
    DeviceRegistrationManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_OnRegistrationRemoved(void)
{
    return DeviceRegistrationManager_OnRegistrationRemovedAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Connection events                                                          */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_OnPeerConnectedAt(
    const Gateway_ByteView_t *device_context_ref,
    Gateway_TimeMs_t now_ms)
{
    DeviceRegistrationManager_DeviceRef_t candidate;
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    memset(&candidate, 0, sizeof(candidate));

    status = DeviceRegistrationManager_CopyViewToRef(
        device_context_ref,
        &candidate);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    s_snapshot.current_peer = candidate;
    s_snapshot.has_current_peer = true;
    s_snapshot.connection = GATEWAY_CONNECTED;

    DeviceRegistrationManager_RecalculateCurrentRegistration();
    DeviceRegistrationManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_OnPeerConnected(
    const Gateway_ByteView_t *device_context_ref)
{
    return DeviceRegistrationManager_OnPeerConnectedAt(
        device_context_ref,
        Gateway_Time_GetMs());
}

Gateway_Status_t DeviceRegistrationManager_OnPeerDisconnectedAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    DeviceRegistrationManager_ClearRef(&s_snapshot.current_peer);
    s_snapshot.has_current_peer = false;
    s_snapshot.connection = GATEWAY_DISCONNECTED;

    DeviceRegistrationManager_RecalculateCurrentRegistration();
    DeviceRegistrationManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_OnPeerDisconnected(void)
{
    return DeviceRegistrationManager_OnPeerDisconnectedAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Explicit observation refresh                                               */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_MarkCurrentStateObservedAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    DeviceRegistrationManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_MarkCurrentStateObserved(void)
{
    return DeviceRegistrationManager_MarkCurrentStateObservedAt(
        Gateway_Time_GetMs());
}


/* -------------------------------------------------------------------------- */
/* Query                                                                      */
/* -------------------------------------------------------------------------- */

bool DeviceRegistrationManager_IsCurrentPeerAuthenticated(void)
{
    if (!s_snapshot.initialized)
    {
        return false;
    }

    return (s_snapshot.current_registration == GATEWAY_REGISTERED)
        && (s_snapshot.connection == GATEWAY_CONNECTED);
}

Gateway_RegistrationState_t
DeviceRegistrationManager_GetCurrentRegistrationState(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_REGISTRATION_UNKNOWN;
    }

    return s_snapshot.current_registration;
}

Gateway_ConnectionState_t
DeviceRegistrationManager_GetConnectionState(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_CONNECTION_UNKNOWN;
    }

    return s_snapshot.connection;
}

Gateway_Status_t DeviceRegistrationManager_GetSnapshot(
    DeviceRegistrationManager_Snapshot_t *snapshot)
{
    Gateway_Status_t status;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (snapshot == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    *snapshot = s_snapshot;

    return GATEWAY_STATUS_OK;
}


/* -------------------------------------------------------------------------- */
/* Domain interface                                                           */
/* -------------------------------------------------------------------------- */

Gateway_Status_t DeviceRegistrationManager_BuildDomainUpdateAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms,
    Gateway_RegistrationConnectionUpdate_t *update)
{
    Gateway_Status_t status;
    const bool is_new_update =
        s_snapshot.observation_revision
        != s_snapshot.last_published_revision;

    status = DeviceRegistrationManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if ((base_context == NULL) || (update == NULL))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!Gateway_Interface_IsValidMessageContext(base_context))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    memset(update, 0, sizeof(*update));

    update->context = *base_context;
    update->context.device_context_id =
        DeviceRegistrationManager_GetReportDeviceView();

    update->registration = s_snapshot.current_registration;
    update->connection = s_snapshot.connection;

    update->update.quality = s_snapshot.quality;

    if (s_snapshot.quality == GATEWAY_DATA_QUALITY_UNKNOWN)
    {
        update->update.age_ms = 0U;
    }
    else
    {
        update->update.age_ms = Gateway_Time_ElapsedMs(
            s_snapshot.last_observed_ms,
            now_ms);
    }

    update->update.is_new_update = is_new_update;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t DeviceRegistrationManager_BuildDomainUpdate(
    const Gateway_MessageContextView_t *base_context,
    Gateway_RegistrationConnectionUpdate_t *update)
{
    return DeviceRegistrationManager_BuildDomainUpdateAt(
        base_context,
        Gateway_Time_GetMs(),
        update);
}

Gateway_Status_t DeviceRegistrationManager_PublishToDomainAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms)
{
    Gateway_RegistrationConnectionUpdate_t update;
    Gateway_Status_t status;

    status = DeviceRegistrationManager_BuildDomainUpdateAt(
        base_context,
        now_ms,
        &update);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = Gateway_Interface_PublishRegistrationConnection(
        &update);

    if (status == GATEWAY_STATUS_OK)
    {
        s_snapshot.last_published_revision =
            s_snapshot.observation_revision;
    }

    return status;
}

Gateway_Status_t DeviceRegistrationManager_PublishToDomain(
    const Gateway_MessageContextView_t *base_context)
{
    return DeviceRegistrationManager_PublishToDomainAt(
        base_context,
        Gateway_Time_GetMs());
}
