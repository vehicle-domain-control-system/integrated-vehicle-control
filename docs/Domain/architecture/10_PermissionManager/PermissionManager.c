/**
 * @file PermissionManager.c
 */

#include "PermissionManager.h"

#include <stddef.h>

static bool g_permissionInitialized = false;

static void Permission_Deny(
    PermissionResult_t *result,
    PermissionReason_t reason,
    PermissionRequirementMask_t failedRequirement)
{
    result->decision = PERMISSION_DECISION_DENY;
    result->reason = reason;
    result->failedRequirement = failedRequirement;
}

static bool Permission_IsValidRequirementMask(
    PermissionRequirementMask_t mask)
{
    return ((mask & ~PERMISSION_REQUIRE_ALL_DEFINED) == 0U);
}

static PermissionReason_t Permission_InputQualityReason(
    DataQuality_t quality)
{
    switch (quality)
    {
        case DATA_QUALITY_OK:
            return PERMISSION_REASON_NONE;

        case DATA_QUALITY_STALE:
            return PERMISSION_REASON_REQUIRED_INPUT_STALE;

        case DATA_QUALITY_INVALID:
            return PERMISSION_REASON_REQUIRED_INPUT_INVALID;

        case DATA_QUALITY_NO_DATA:
        default:
            return PERMISSION_REASON_REQUIRED_INPUT_NO_DATA;
    }
}

PermissionManager_Status_t PermissionManager_Init(void)
{
    g_permissionInitialized = true;
    return PERMISSION_STATUS_OK;
}

bool PermissionManager_IsInitialized(void)
{
    return g_permissionInitialized;
}

PermissionManager_Status_t PermissionManager_Evaluate(
    const PermissionEvaluation_t *evaluation,
    PermissionResult_t *outResult)
{
    PermissionReason_t qualityReason;

    if ((evaluation == NULL) || (outResult == NULL))
    {
        return PERMISSION_STATUS_INVALID_ARGUMENT;
    }

    if (!g_permissionInitialized)
    {
        return PERMISSION_STATUS_NOT_INITIALIZED;
    }

    if (!Permission_IsValidRequirementMask(
            evaluation->requirements))
    {
        return PERMISSION_STATUS_INVALID_ARGUMENT;
    }

    outResult->decision = PERMISSION_DECISION_ALLOW;
    outResult->reason = PERMISSION_REASON_NONE;
    outResult->failedRequirement = 0U;

    /* 1. Domain operational */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_DOMAIN_OPERATIONAL) != 0U)
    {
        if ((evaluation->facts.lifecycleState !=
             DOMAIN_LIFECYCLE_READY) &&
            (evaluation->facts.lifecycleState !=
             DOMAIN_LIFECYCLE_DEGRADED))
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_DOMAIN_NOT_OPERATIONAL,
                PERMISSION_REQUIRE_DOMAIN_OPERATIONAL);
            return PERMISSION_STATUS_OK;
        }
    }

    /* 2. Current Session */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_SESSION_VALID) != 0U)
    {
        if (!evaluation->facts.sessionValid)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_SESSION_INVALID,
                PERMISSION_REQUIRE_SESSION_VALID);
            return PERMISSION_STATUS_OK;
        }
    }

    /* 3. Function support */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_FUNCTION_SUPPORTED) != 0U)
    {
        if (!evaluation->facts.functionSupported)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_FUNCTION_UNSUPPORTED,
                PERMISSION_REQUIRE_FUNCTION_SUPPORTED);
            return PERMISSION_STATUS_OK;
        }
    }

    /* 4. Function availability */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_FUNCTION_AVAILABLE) != 0U)
    {
        switch (evaluation->facts.functionAvailability)
        {
            case FUNCTION_AVAILABILITY_AVAILABLE:
                break;

            case FUNCTION_AVAILABILITY_DEGRADED:
                if (evaluation->facts.degradedPolicy ==
                    PERMISSION_DEGRADED_DENY)
                {
                    Permission_Deny(
                        outResult,
                        PERMISSION_REASON_FUNCTION_DEGRADED_NOT_ALLOWED,
                        PERMISSION_REQUIRE_FUNCTION_AVAILABLE);
                    return PERMISSION_STATUS_OK;
                }
                break;

            case FUNCTION_AVAILABILITY_UNAVAILABLE:
            case FUNCTION_AVAILABILITY_UNSUPPORTED:
            default:
                Permission_Deny(
                    outResult,
                    PERMISSION_REASON_FUNCTION_UNAVAILABLE,
                    PERMISSION_REQUIRE_FUNCTION_AVAILABLE);
                return PERMISSION_STATUS_OK;
        }
    }

    /* 5. Only the input quality actually required by this feature/action. */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_INPUT_QUALITY) != 0U)
    {
        qualityReason = Permission_InputQualityReason(
            evaluation->facts.requiredInputQuality);

        if (qualityReason != PERMISSION_REASON_NONE)
        {
            Permission_Deny(
                outResult,
                qualityReason,
                PERMISSION_REQUIRE_INPUT_QUALITY);
            return PERMISSION_STATUS_OK;
        }
    }

    /* 6. Power permission */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_POWER_PERMISSION) != 0U)
    {
        if (evaluation->facts.powerPermission ==
            PERMISSION_SIGNAL_UNKNOWN)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_POWER_PERMISSION_UNKNOWN,
                PERMISSION_REQUIRE_POWER_PERMISSION);
            return PERMISSION_STATUS_OK;
        }

        if (evaluation->facts.powerPermission ==
            PERMISSION_SIGNAL_NOT_ALLOWED)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_POWER_NOT_ALLOWED,
                PERMISSION_REQUIRE_POWER_PERMISSION);
            return PERMISSION_STATUS_OK;
        }
    }

    /* 7. Operation/keep-alive permission, only when action requires it. */
    if ((evaluation->requirements &
         PERMISSION_REQUIRE_OPERATION_PERMISSION) != 0U)
    {
        if (evaluation->facts.operationPermission ==
            PERMISSION_SIGNAL_UNKNOWN)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_OPERATION_PERMISSION_UNKNOWN,
                PERMISSION_REQUIRE_OPERATION_PERMISSION);
            return PERMISSION_STATUS_OK;
        }

        if (evaluation->facts.operationPermission ==
            PERMISSION_SIGNAL_NOT_ALLOWED)
        {
            Permission_Deny(
                outResult,
                PERMISSION_REASON_OPERATION_NOT_ALLOWED,
                PERMISSION_REQUIRE_OPERATION_PERMISSION);
            return PERMISSION_STATUS_OK;
        }
    }

    return PERMISSION_STATUS_OK;
}

ResultReason_t PermissionManager_ToResultReason(
    PermissionReason_t reason)
{
    switch (reason)
    {
        case PERMISSION_REASON_NONE:
            return RESULT_REASON_NONE;

        case PERMISSION_REASON_REQUIRED_INPUT_STALE:
        case PERMISSION_REASON_REQUIRED_INPUT_INVALID:
        case PERMISSION_REASON_REQUIRED_INPUT_NO_DATA:
            return RESULT_REASON_STATE_UNTRUSTED;

        case PERMISSION_REASON_DOMAIN_NOT_OPERATIONAL:
        case PERMISSION_REASON_SESSION_INVALID:
        case PERMISSION_REASON_FUNCTION_UNSUPPORTED:
        case PERMISSION_REASON_FUNCTION_UNAVAILABLE:
        case PERMISSION_REASON_FUNCTION_DEGRADED_NOT_ALLOWED:
        case PERMISSION_REASON_POWER_PERMISSION_UNKNOWN:
        case PERMISSION_REASON_POWER_NOT_ALLOWED:
        case PERMISSION_REASON_OPERATION_PERMISSION_UNKNOWN:
        case PERMISSION_REASON_OPERATION_NOT_ALLOWED:
        default:
            return RESULT_REASON_NOT_ALLOWED;
    }
}
