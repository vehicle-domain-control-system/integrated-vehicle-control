#include "domain_network_bringup.h"

#include <stddef.h>

#include "domain_can0_transceiver.h"
#include "network/app/net_if_entry.h"

static bool DomainNetworkBringup_TimeReached(uint32_t nowMs, uint32_t deadlineMs)
{
    return ((int32_t)(nowMs - deadlineMs) >= 0);
}

static bool DomainNetworkBringup_TimeAfter(uint32_t laterMs, uint32_t earlierMs)
{
    return ((int32_t)(laterMs - earlierMs) > 0);
}

static void DomainNetworkBringup_RequestSafeStandby(
    DomainNetworkBringup_t *context)
{
    DomainCan0Transceiver_Status_t status;

    if (context->standbyRecoveryAttempted)
    {
        return;
    }

    context->standbyRecoveryAttempted = true;
    status = DomainCan0Transceiver_RequestStandby();
    context->standbyRecoveryFailed =
        (status != DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED) ||
        (DomainCan0Transceiver_GetLastRequestedMode() !=
         DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY);
}

static DomainNetworkBringup_Status_t DomainNetworkBringup_LatchFault(
    DomainNetworkBringup_t *context,
    DomainNetworkBringup_Fault_t fault)
{
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }

    context->fault = fault;
    context->state = DOMAIN_NETWORK_BRINGUP_FAULT;
    DomainNetworkBringup_RequestSafeStandby(context);
    return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_Init(
    DomainNetworkBringup_t *context,
    bool hardwareActivationApproved)
{
    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_ALREADY;
    }

    context->hardwareActivationApproved = hardwareActivationApproved;
    context->standbyRecoveryAttempted = false;
    context->standbyRecoveryFailed = false;
    context->state = DOMAIN_NETWORK_BRINGUP_BOOTSTRAP;
    context->fault = DOMAIN_NETWORK_BRINGUP_FAULT_NONE;
    context->expectedNetworkBootId = 0U;
    context->expectedPortEpoch = 0U;
    context->normalRequestedAtMs = 0U;
    context->activeEvidenceDeadlineMs = 0U;
    context->initialized = true;
    return DOMAIN_NETWORK_BRINGUP_OK;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_EnterStandby(
    DomainNetworkBringup_t *context)
{
    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_STANDBY)
    {
        return DOMAIN_NETWORK_BRINGUP_ALREADY;
    }
    if (context->state != DOMAIN_NETWORK_BRINGUP_BOOTSTRAP)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (!DomainCan0Transceiver_IsControlAvailable())
    {
        return DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_UNAVAILABLE;
    }
    if (DomainCan0Transceiver_GetLastRequestedMode() !=
        DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY)
    {
        return DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NOT_STANDBY;
    }

    context->state = DOMAIN_NETWORK_BRINGUP_STANDBY;
    return DOMAIN_NETWORK_BRINGUP_OK;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_BeginNetworkInitialization(
    DomainNetworkBringup_t *context)
{
    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_NETWORK_INITIALIZING)
    {
        return DOMAIN_NETWORK_BRINGUP_ALREADY;
    }
    if (context->state != DOMAIN_NETWORK_BRINGUP_STANDBY)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (!context->hardwareActivationApproved)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_APPROVED;
    }

    context->state = DOMAIN_NETWORK_BRINGUP_NETWORK_INITIALIZING;
    return DOMAIN_NETWORK_BRINGUP_OK;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_ObserveNetworkStatus(
    DomainNetworkBringup_t *context)
{
    NetHardwareInitStatus networkStatus;

    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if ((context->state != DOMAIN_NETWORK_BRINGUP_NETWORK_INITIALIZING) &&
        (context->state != DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED))
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }

    networkStatus = Net_GetHardwareInitStatus();
    switch (networkStatus)
    {
        case NET_HARDWARE_INIT_NOT_INITIALIZED:
        case NET_HARDWARE_INIT_INITIALIZING:
            if (context->state == DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED)
            {
                return DomainNetworkBringup_LatchFault(
                    context,
                    DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
            }
            return DOMAIN_NETWORK_BRINGUP_PENDING;

        case NET_HARDWARE_INIT_PREPARED:
            if (context->state == DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED)
            {
                return DOMAIN_NETWORK_BRINGUP_ALREADY;
            }
            context->state = DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED;
            return DOMAIN_NETWORK_BRINGUP_OK;

        case NET_HARDWARE_INIT_ROLLBACK_FAILED:
            return DomainNetworkBringup_LatchFault(
                context,
                DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_ROLLBACK);

        case NET_HARDWARE_INIT_FAILED:
        default:
            return DomainNetworkBringup_LatchFault(
                context,
                DOMAIN_NETWORK_BRINGUP_FAULT_NETWORK_INITIALIZATION);
    }
}

DomainNetworkBringup_Status_t DomainNetworkBringup_RequestNormal(
    DomainNetworkBringup_t *context,
    uint32_t networkBootId,
    uint32_t portEpoch,
    uint32_t nowMs,
    uint32_t activeEvidenceDeadlineMs)
{
    DomainCan0Transceiver_Status_t transceiverStatus;

    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED)
    {
        return DOMAIN_NETWORK_BRINGUP_ALREADY;
    }
    if (context->state != DOMAIN_NETWORK_BRINGUP_NETWORK_PREPARED)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (!context->hardwareActivationApproved)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_APPROVED;
    }
    if ((networkBootId == 0U) || (networkBootId != Net_GetBootId()) ||
        (portEpoch == 0U) ||
        !DomainNetworkBringup_TimeAfter(activeEvidenceDeadlineMs, nowMs))
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (Net_GetHardwareInitStatus() != NET_HARDWARE_INIT_PREPARED)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (!DomainCan0Transceiver_IsControlAvailable())
    {
        return DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_UNAVAILABLE;
    }
    if (DomainCan0Transceiver_GetLastRequestedMode() !=
        DOMAIN_CAN0_TRANSCEIVER_MODE_STANDBY)
    {
        return DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NOT_STANDBY;
    }

    transceiverStatus = DomainCan0Transceiver_RequestNormal();
    if ((transceiverStatus != DOMAIN_CAN0_TRANSCEIVER_REQUEST_ACCEPTED) ||
        (DomainCan0Transceiver_GetLastRequestedMode() !=
         DOMAIN_CAN0_TRANSCEIVER_MODE_NORMAL))
    {
        (void)DomainNetworkBringup_LatchFault(
            context,
            DOMAIN_NETWORK_BRINGUP_FAULT_NORMAL_REQUEST);
        return DOMAIN_NETWORK_BRINGUP_TRANSCEIVER_NORMAL_REQUEST_FAILED;
    }

    context->expectedNetworkBootId = networkBootId;
    context->expectedPortEpoch = portEpoch;
    context->normalRequestedAtMs = nowMs;
    context->activeEvidenceDeadlineMs = activeEvidenceDeadlineMs;
    context->state = DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED;
    return DOMAIN_NETWORK_BRINGUP_OK;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_ConfirmActive(
    DomainNetworkBringup_t *context,
    const DomainNetworkBringup_ActiveEvidence_t *evidence,
    uint32_t nowMs)
{
    if ((context == NULL) || (evidence == NULL))
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_ACTIVE)
    {
        return DOMAIN_NETWORK_BRINGUP_ALREADY;
    }
    if (context->state != DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (DomainNetworkBringup_TimeReached(
            nowMs, context->activeEvidenceDeadlineMs))
    {
        return DomainNetworkBringup_LatchFault(
            context,
            DOMAIN_NETWORK_BRINGUP_FAULT_ACTIVE_EVIDENCE_TIMEOUT);
    }
    if ((evidence->source == DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_NONE) ||
        (evidence->source >
         DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CAN_TX_CONFIRMED) ||
        (evidence->networkBootId != context->expectedNetworkBootId) ||
        (evidence->portEpoch != context->expectedPortEpoch) ||
        !DomainNetworkBringup_TimeAfter(
            evidence->observedAtMs, context->normalRequestedAtMs) ||
        DomainNetworkBringup_TimeAfter(evidence->observedAtMs, nowMs) ||
        !DomainNetworkBringup_TimeAfter(
            evidence->expiresAtMs, evidence->observedAtMs))
    {
        return DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_INVALID;
    }
    if (DomainNetworkBringup_TimeReached(nowMs, evidence->expiresAtMs))
    {
        return DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_EXPIRED;
    }

    /*
     * TEAM_DECISION_REQUIRED: Network/Board owners have not approved which
     * event proves physical activity, nor its epoch source and lifetime.
     * Do not set ACTIVE until that contract is implemented and verified.
     */
    return DOMAIN_NETWORK_BRINGUP_ACTIVE_EVIDENCE_CONTRACT_OPEN;
}

DomainNetworkBringup_Status_t DomainNetworkBringup_CheckActiveEvidenceDeadline(
    DomainNetworkBringup_t *context,
    uint32_t nowMs)
{
    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if (context->state != DOMAIN_NETWORK_BRINGUP_NORMAL_REQUESTED)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_STATE;
    }
    if (!DomainNetworkBringup_TimeReached(
            nowMs, context->activeEvidenceDeadlineMs))
    {
        return DOMAIN_NETWORK_BRINGUP_PENDING;
    }
    return DomainNetworkBringup_LatchFault(
        context,
        DOMAIN_NETWORK_BRINGUP_FAULT_ACTIVE_EVIDENCE_TIMEOUT);
}

DomainNetworkBringup_Status_t DomainNetworkBringup_ReportFault(
    DomainNetworkBringup_t *context,
    DomainNetworkBringup_Fault_t fault)
{
    if (context == NULL)
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }
    if (!context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_NOT_INITIALIZED;
    }
    if (context->state == DOMAIN_NETWORK_BRINGUP_FAULT)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_LATCHED;
    }
    if ((fault == DOMAIN_NETWORK_BRINGUP_FAULT_NONE) ||
        (fault >= DOMAIN_NETWORK_BRINGUP_FAULT_INVALID_CONTEXT))
    {
        return DOMAIN_NETWORK_BRINGUP_INVALID_ARGUMENT;
    }

    return DomainNetworkBringup_LatchFault(context, fault);
}

DomainNetworkBringup_State_t DomainNetworkBringup_GetState(
    const DomainNetworkBringup_t *context)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT;
    }
    return context->state;
}

DomainNetworkBringup_Fault_t DomainNetworkBringup_GetFault(
    const DomainNetworkBringup_t *context)
{
    if ((context == NULL) || !context->initialized)
    {
        return DOMAIN_NETWORK_BRINGUP_FAULT_INVALID_CONTEXT;
    }
    return context->fault;
}

bool DomainNetworkBringup_WasStandbyRecoveryAttempted(
    const DomainNetworkBringup_t *context)
{
    return (context != NULL) && context->initialized &&
           context->standbyRecoveryAttempted;
}

bool DomainNetworkBringup_DidStandbyRecoveryFail(
    const DomainNetworkBringup_t *context)
{
    return (context != NULL) && context->initialized &&
           context->standbyRecoveryFailed;
}
