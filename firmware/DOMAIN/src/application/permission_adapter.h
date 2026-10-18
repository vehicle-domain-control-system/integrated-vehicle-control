#ifndef PERMISSION_ADAPTER_H
#define PERMISSION_ADAPTER_H

#include "Domain_Interface_v0.5_C2_target.h"
#include "permission_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure value conversion from a Domain-owned Permission snapshot to the
 * Production C2 power-permission type. This module never calls DomainIf_Tx*,
 * an Observer, or a Network port, and it never mutates PermissionManager.
 */
typedef enum
{
    PERMISSION_ADAPTER_OK = 0,
    PERMISSION_ADAPTER_INVALID_ARGUMENT,
    PERMISSION_ADAPTER_UNSUPPORTED_SNAPSHOT
} PermissionAdapter_Status_t;

PermissionAdapter_Status_t PermissionAdapter_ConvertCisPowerPermission(
    const PermissionManager_Snapshot_t *snapshot,
    DomainIf_CisPowerPermission_t *outPermission);

#ifdef __cplusplus
}
#endif

#endif /* PERMISSION_ADAPTER_H */
