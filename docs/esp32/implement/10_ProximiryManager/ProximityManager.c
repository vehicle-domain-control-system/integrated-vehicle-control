#include "ProximityManager.h"

#include <limits.h>
#include <string.h>

#include "LinkStateManager.h"

static ProximityManager_Snapshot_t s_snapshot;

static Gateway_Status_t ProximityManager_CheckReady(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_PolicyConfig_IsInitialized()
        || !Gateway_PolicyConfig_IsProximityConfigured()
        || !DeviceRegistrationManager_IsInitialized()
        || !LinkStateManager_IsInitialized())
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    return GATEWAY_STATUS_OK;
}

static void ProximityManager_ClearCandidate(void)
{
    s_snapshot.pending_candidate = GATEWAY_PROXIMITY_UNKNOWN;
    s_snapshot.pending_candidate_count = 0U;
}

static void ProximityManager_NextObservation(
    Gateway_TimeMs_t now_ms)
{
    ++s_snapshot.observation_revision;

    if (s_snapshot.observation_revision == 0U)
    {
        ++s_snapshot.observation_revision;
    }

    s_snapshot.last_observed_ms = now_ms;
}

static void ProximityManager_ClearDeviceRef(void)
{
    memset(&s_snapshot.device_ref, 0, sizeof(s_snapshot.device_ref));
    s_snapshot.has_device_ref = false;
}

static Gateway_Status_t ProximityManager_CopyDeviceRef(
    const Gateway_ByteView_t *view)
{
    if ((view == NULL)
        || !Gateway_Interface_IsValidByteView(view)
        || (view->length == 0U))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (view->length > DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES)
    {
        return GATEWAY_STATUS_UNSUPPORTED;
    }

    memset(&s_snapshot.device_ref, 0, sizeof(s_snapshot.device_ref));

    memcpy(
        s_snapshot.device_ref.data,
        view->data,
        view->length);

    s_snapshot.device_ref.length = view->length;
    s_snapshot.has_device_ref = true;

    return GATEWAY_STATUS_OK;
}

static bool ProximityManager_IsDeviceRefEqualToStorage(
    const Gateway_ByteView_t *view,
    const DeviceRegistrationManager_DeviceRef_t *storage)
{
    if ((view == NULL) || (storage == NULL))
    {
        return false;
    }

    if (!Gateway_Interface_IsValidByteView(view))
    {
        return false;
    }

    if (view->length != storage->length)
    {
        return false;
    }

    if (view->length == 0U)
    {
        return true;
    }

    return memcmp(
        view->data,
        storage->data,
        view->length) == 0;
}

static bool ProximityManager_IsCurrentPeerRef(
    const Gateway_ByteView_t *device_context_ref)
{
    DeviceRegistrationManager_Snapshot_t registration;

    if (DeviceRegistrationManager_GetSnapshot(&registration)
        != GATEWAY_STATUS_OK)
    {
        return false;
    }

    if (!registration.has_current_peer)
    {
        return false;
    }

    return ProximityManager_IsDeviceRefEqualToStorage(
        device_context_ref,
        &registration.current_peer);
}

static void ProximityManager_BindCurrentPeerIfAvailable(void)
{
    DeviceRegistrationManager_Snapshot_t registration;

    if (DeviceRegistrationManager_GetSnapshot(&registration)
        != GATEWAY_STATUS_OK)
    {
        return;
    }

    if (!registration.has_current_peer)
    {
        return;
    }

    if (registration.current_peer.length
        > DEVICE_REGISTRATION_MANAGER_MAX_REF_BYTES)
    {
        return;
    }

    s_snapshot.device_ref = registration.current_peer;
    s_snapshot.has_device_ref = true;
}

static bool ProximityManager_IsBoundPeerStillCurrent(void)
{
    DeviceRegistrationManager_Snapshot_t registration;

    if (!s_snapshot.has_device_ref)
    {
        return false;
    }

    if (DeviceRegistrationManager_GetSnapshot(&registration)
        != GATEWAY_STATUS_OK)
    {
        return false;
    }

    if (!registration.has_current_peer)
    {
        return false;
    }

    if (registration.current_peer.length != s_snapshot.device_ref.length)
    {
        return false;
    }

    if (registration.current_peer.length == 0U)
    {
        return false;
    }

    return memcmp(
        registration.current_peer.data,
        s_snapshot.device_ref.data,
        registration.current_peer.length) == 0;
}

static void ProximityManager_SetUnavailable(
    Gateway_DataQuality_t quality,
    Gateway_TimeMs_t now_ms,
    bool refresh_device_ref)
{
    const bool changed =
        (s_snapshot.state != GATEWAY_PROXIMITY_UNKNOWN)
        || (s_snapshot.quality != quality)
        || (s_snapshot.pending_candidate_count != 0U);

    s_snapshot.state = GATEWAY_PROXIMITY_UNKNOWN;
    s_snapshot.quality = quality;
    ProximityManager_ClearCandidate();

    if (refresh_device_ref)
    {
        ProximityManager_ClearDeviceRef();
        ProximityManager_BindCurrentPeerIfAvailable();
    }

    if (changed)
    {
        ++s_snapshot.observation_revision;

        if (s_snapshot.observation_revision == 0U)
        {
            ++s_snapshot.observation_revision;
        }
    }

    (void)now_ms;
}

static Gateway_ProximityState_t ProximityManager_ClassifyCandidate(
    int16_t rssi_dbm,
    const Gateway_ProximityPolicyConfig_t *config)
{
    switch (s_snapshot.state)
    {
        case GATEWAY_PROXIMITY_NEAR:
            if (rssi_dbm <= config->far_exit_rssi_dbm)
            {
                return GATEWAY_PROXIMITY_FAR;
            }

            return GATEWAY_PROXIMITY_NEAR;

        case GATEWAY_PROXIMITY_FAR:
            if (rssi_dbm >= config->near_enter_rssi_dbm)
            {
                return GATEWAY_PROXIMITY_NEAR;
            }

            return GATEWAY_PROXIMITY_FAR;

        case GATEWAY_PROXIMITY_UNKNOWN:
        default:
            if (rssi_dbm >= config->near_enter_rssi_dbm)
            {
                return GATEWAY_PROXIMITY_NEAR;
            }

            if (rssi_dbm <= config->far_exit_rssi_dbm)
            {
                return GATEWAY_PROXIMITY_FAR;
            }

            return GATEWAY_PROXIMITY_UNKNOWN;
    }
}

static void ProximityManager_ApplyCandidate(
    Gateway_ProximityState_t candidate,
    uint16_t stable_sample_count)
{
    if (candidate == s_snapshot.state)
    {
        ProximityManager_ClearCandidate();
        return;
    }

    if (candidate == GATEWAY_PROXIMITY_UNKNOWN)
    {
        ProximityManager_ClearCandidate();
        return;
    }

    if (s_snapshot.pending_candidate == candidate)
    {
        if (s_snapshot.pending_candidate_count < UINT16_MAX)
        {
            ++s_snapshot.pending_candidate_count;
        }
    }
    else
    {
        s_snapshot.pending_candidate = candidate;
        s_snapshot.pending_candidate_count = 1U;
    }

    if (s_snapshot.pending_candidate_count >= stable_sample_count)
    {
        s_snapshot.state = candidate;
        ProximityManager_ClearCandidate();
    }
}

static Gateway_ByteView_t ProximityManager_DeviceRefView(void)
{
    Gateway_ByteView_t view = {
        .data = NULL,
        .length = 0U
    };

    if (s_snapshot.has_device_ref
        && (s_snapshot.device_ref.length > 0U))
    {
        view.data = s_snapshot.device_ref.data;
        view.length = s_snapshot.device_ref.length;
    }

    return view;
}

Gateway_Status_t ProximityManager_Init(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));

    s_snapshot.initialized = true;
    s_snapshot.state = GATEWAY_PROXIMITY_UNKNOWN;
    s_snapshot.quality = GATEWAY_DATA_QUALITY_UNKNOWN;
    s_snapshot.pending_candidate = GATEWAY_PROXIMITY_UNKNOWN;

    return GATEWAY_STATUS_OK;
}

void ProximityManager_Reset(void)
{
    memset(&s_snapshot, 0, sizeof(s_snapshot));
}

bool ProximityManager_IsInitialized(void)
{
    return s_snapshot.initialized;
}

Gateway_Status_t ProximityManager_OnRssiSampleAt(
    const Gateway_ByteView_t *device_context_ref,
    int16_t rssi_dbm,
    bool sample_valid,
    Gateway_TimeMs_t now_ms)
{
    Gateway_ProximityPolicyConfig_t config;
    Gateway_Status_t status;

    status = ProximityManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = Gateway_PolicyConfig_GetProximity(&config);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if ((device_context_ref == NULL)
        || !Gateway_Interface_IsValidByteView(device_context_ref)
        || (device_context_ref->length == 0U))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!DeviceRegistrationManager_IsCurrentPeerAuthenticated()
        || !LinkStateManager_IsAvailable(GATEWAY_LINK_BLUETOOTH)
        || !ProximityManager_IsCurrentPeerRef(device_context_ref))
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    status = ProximityManager_CopyDeviceRef(device_context_ref);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    s_snapshot.has_observation = true;
    s_snapshot.last_rssi_dbm = rssi_dbm;
    s_snapshot.quality = sample_valid
        ? GATEWAY_DATA_QUALITY_VALID
        : GATEWAY_DATA_QUALITY_INVALID;

    if (!sample_valid)
    {
        s_snapshot.state = GATEWAY_PROXIMITY_UNKNOWN;
        ProximityManager_ClearCandidate();
        ProximityManager_NextObservation(now_ms);
        return GATEWAY_STATUS_OK;
    }

    ProximityManager_ApplyCandidate(
        ProximityManager_ClassifyCandidate(rssi_dbm, &config),
        config.stable_sample_count);

    ProximityManager_NextObservation(now_ms);

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t ProximityManager_OnRssiSample(
    const Gateway_ByteView_t *device_context_ref,
    int16_t rssi_dbm,
    bool sample_valid)
{
    return ProximityManager_OnRssiSampleAt(
        device_context_ref,
        rssi_dbm,
        sample_valid,
        Gateway_Time_GetMs());
}

Gateway_Status_t ProximityManager_UpdateAt(
    Gateway_TimeMs_t now_ms)
{
    Gateway_ProximityPolicyConfig_t config;
    Gateway_Status_t status;

    status = ProximityManager_CheckReady();
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = Gateway_PolicyConfig_GetProximity(&config);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    if (!LinkStateManager_IsAvailable(GATEWAY_LINK_BLUETOOTH)
        || !DeviceRegistrationManager_IsCurrentPeerAuthenticated())
    {
        ProximityManager_SetUnavailable(
            GATEWAY_DATA_QUALITY_NO_DATA,
            now_ms,
            true);
        return GATEWAY_STATUS_OK;
    }

    if (s_snapshot.has_device_ref
        && !ProximityManager_IsBoundPeerStillCurrent())
    {
        s_snapshot.has_observation = false;

        ProximityManager_SetUnavailable(
            GATEWAY_DATA_QUALITY_NO_DATA,
            now_ms,
            true);

        return GATEWAY_STATUS_OK;
    }

    if (!s_snapshot.has_device_ref)
    {
        ProximityManager_BindCurrentPeerIfAvailable();
    }

    if (!s_snapshot.has_observation)
    {
        ProximityManager_SetUnavailable(
            GATEWAY_DATA_QUALITY_NO_DATA,
            now_ms,
            false);
        return GATEWAY_STATUS_OK;
    }

    if (s_snapshot.quality == GATEWAY_DATA_QUALITY_INVALID)
    {
        return GATEWAY_STATUS_OK;
    }

    if (Gateway_Time_HasElapsed(
            s_snapshot.last_observed_ms,
            config.proximity_expiry_ms,
            now_ms))
    {
        ProximityManager_SetUnavailable(
            GATEWAY_DATA_QUALITY_STALE,
            now_ms,
            false);
    }

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t ProximityManager_UpdateNow(void)
{
    return ProximityManager_UpdateAt(
        Gateway_Time_GetMs());
}

Gateway_ProximityState_t ProximityManager_GetState(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_PROXIMITY_UNKNOWN;
    }

    return s_snapshot.state;
}

Gateway_DataQuality_t ProximityManager_GetQuality(void)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_DATA_QUALITY_UNKNOWN;
    }

    return s_snapshot.quality;
}

Gateway_Status_t ProximityManager_GetSnapshot(
    ProximityManager_Snapshot_t *snapshot)
{
    if (!s_snapshot.initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (snapshot == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    *snapshot = s_snapshot;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t ProximityManager_BuildDomainUpdateAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms,
    Gateway_ProximityUpdate_t *update)
{
    Gateway_Status_t status;
    const bool is_new_update =
        s_snapshot.observation_revision
        != s_snapshot.last_published_revision;

    status = ProximityManager_CheckReady();
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
        ProximityManager_DeviceRefView();

    update->proximity.state = s_snapshot.state;
    update->proximity.update.quality = s_snapshot.quality;

    if (!s_snapshot.has_observation)
    {
        update->proximity.update.age_ms = 0U;
    }
    else
    {
        update->proximity.update.age_ms =
            Gateway_Time_ElapsedMs(
                s_snapshot.last_observed_ms,
                now_ms);
    }

    update->proximity.update.is_new_update = is_new_update;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t ProximityManager_BuildDomainUpdate(
    const Gateway_MessageContextView_t *base_context,
    Gateway_ProximityUpdate_t *update)
{
    return ProximityManager_BuildDomainUpdateAt(
        base_context,
        Gateway_Time_GetMs(),
        update);
}

Gateway_Status_t ProximityManager_PublishToDomainAt(
    const Gateway_MessageContextView_t *base_context,
    Gateway_TimeMs_t now_ms)
{
    Gateway_ProximityUpdate_t update;
    Gateway_Status_t status;

    status = ProximityManager_BuildDomainUpdateAt(
        base_context,
        now_ms,
        &update);
    if (status != GATEWAY_STATUS_OK)
    {
        return status;
    }

    status = Gateway_Interface_PublishProximity(&update);

    if (status == GATEWAY_STATUS_OK)
    {
        s_snapshot.last_published_revision =
            s_snapshot.observation_revision;
    }

    return status;
}

Gateway_Status_t ProximityManager_PublishToDomain(
    const Gateway_MessageContextView_t *base_context)
{
    return ProximityManager_PublishToDomainAt(
        base_context,
        Gateway_Time_GetMs());
}
