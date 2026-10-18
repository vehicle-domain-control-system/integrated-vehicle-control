#include "permission_adapter.h"

#include <string.h>

static bool PermissionAdapter_IsQualityValid(DataQuality_t quality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
        case DATA_QUALITY_STALE:
        case DATA_QUALITY_INVALID:
        case DATA_QUALITY_NO_DATA:
            return true;
        default:
            return false;
    }
}

PermissionAdapter_Status_t PermissionAdapter_ConvertCisPowerPermission(
    const PermissionManager_Snapshot_t *snapshot,
    DomainIf_CisPowerPermission_t *outPermission)
{
    DomainIf_CisPowerPermission_t permission;

    if ((snapshot == NULL) || (outPermission == NULL))
    {
        return PERMISSION_ADAPTER_INVALID_ARGUMENT;
    }
    if (!PermissionAdapter_IsQualityValid(snapshot->effectiveQuality))
    {
        return PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT;
    }

    (void)memset(&permission, 0, sizeof(permission));

    switch (snapshot->effectivePermission)
    {
        case PERMISSION_MANAGER_PERMISSION_ALLOW:
            if ((snapshot->effectiveQuality != DATA_QUALITY_OK) ||
                !snapshot->activeAllowed)
            {
                return PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT;
            }
            permission.vehiclePowerAllowed = true;
            break;

        case PERMISSION_MANAGER_PERMISSION_DENY:
            if ((snapshot->effectiveQuality != DATA_QUALITY_OK) ||
                snapshot->activeAllowed)
            {
                return PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT;
            }
            permission.vehiclePowerAllowed = false;
            break;

        case PERMISSION_MANAGER_PERMISSION_UNKNOWN:
            if ((snapshot->effectiveQuality == DATA_QUALITY_OK) ||
                snapshot->activeAllowed)
            {
                return PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT;
            }
            permission.vehiclePowerAllowed = false;
            break;

        default:
            return PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT;
    }

    permission.quality = snapshot->effectiveQuality;
    permission.meta.quality = snapshot->effectiveQuality;
    permission.meta.ageMs = snapshot->effectiveAgeMs;
    permission.meta.updateSequence = snapshot->updateSequence;

    *outPermission = permission;
    return PERMISSION_ADAPTER_OK;
}
