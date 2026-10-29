#include "bcm_diag.h"
#include "bcm_config.h"

typedef struct
{
    NetFaultCategory category;
    uint8_t          affected;
} FaultInfo;

static const FaultInfo k_faultInfo[BCM_FAULT_COUNT] =
{
    { NET_FAULT_CATEGORY_SENSOR,   NET_BCM_FUNCTION_DOOR },
    { NET_FAULT_CATEGORY_SENSOR,   NET_BCM_FUNCTION_CLIMATE },
    { NET_FAULT_CATEGORY_SENSOR,   NET_BCM_FUNCTION_CLIMATE },
    { NET_FAULT_CATEGORY_FUNCTION, NET_BCM_FUNCTION_CLIMATE },
    { NET_FAULT_CATEGORY_FUNCTION, NET_BCM_FUNCTION_CLIMATE },
    { NET_FAULT_CATEGORY_FUNCTION, NET_BCM_FUNCTION_DOOR },
    { NET_FAULT_CATEGORY_FUNCTION, NET_BCM_FUNCTION_LIGHT },
    { NET_FAULT_CATEGORY_COMM,     NET_BCM_FUNCTION_DOOR | NET_BCM_FUNCTION_CLIMATE | NET_BCM_FUNCTION_LIGHT }
};

typedef struct
{
    NetControllerState state;
    bool               startupDecided;
    uint32_t           startMs;
    bool               started;
    bool               faults[BCM_FAULT_COUNT];
    bool               hasActive;
    BcmFault           active;
    bool               recovering;
} DiagContext;

static DiagContext s_diag;

static uint16_t FaultMask(void)
{
    uint16_t mask = 0u;
    uint32_t bit;
    for (bit = 0u; bit < BCM_FAULT_COUNT; bit++)
    {
        if (s_diag.faults[bit])
        {
            mask |= (uint16_t)(1u << bit);
        }
    }
    return mask;
}

static void UpdateActive(const bool *faults)
{
    uint32_t bit;
    for (bit = 0u; bit < BCM_FAULT_COUNT; bit++)
    {
        if (faults[bit] && !s_diag.faults[bit])
        {
            s_diag.hasActive = true;
            s_diag.active = (BcmFault)bit;
        }
        s_diag.faults[bit] = faults[bit];
    }
    if (s_diag.hasActive && !s_diag.faults[s_diag.active])
    {
        s_diag.hasActive = false;
        for (bit = 0u; bit < BCM_FAULT_COUNT; bit++)
        {
            if (s_diag.faults[bit])
            {
                s_diag.hasActive = true;
                s_diag.active = (BcmFault)bit;
                break;
            }
        }
    }
}

void BcmDiag_Init(void)
{
    DiagContext zero = { 0 };
    s_diag = zero;
    s_diag.state = NET_CONTROLLER_INIT;
}

void BcmDiag_Update(const BcmDiagInputs *inputs, uint32_t nowMs)
{
    if (!s_diag.started)
    {
        s_diag.started = true;
        s_diag.startMs = nowMs;
    }
    UpdateActive(inputs->faults);
    s_diag.recovering = inputs->recovering;

    if (!s_diag.startupDecided)
    {
        if (inputs->initFailed || ((nowMs - s_diag.startMs) >= BCM_STARTUP_LIMIT_MS && !inputs->inputsSettled))
        {
            s_diag.state = NET_CONTROLLER_FAULT;
            s_diag.startupDecided = true;
        }
        else if (inputs->inputsSettled)
        {
            s_diag.startupDecided = true;
        }
        else
        {
            return;
        }
    }
    if (s_diag.state != NET_CONTROLLER_FAULT)
    {
        s_diag.state = (FaultMask() != 0u) ? NET_CONTROLLER_DEGRADED : NET_CONTROLLER_READY;
    }
}

NetControllerState BcmDiag_EcuState(void)
{
    return s_diag.state;
}

NetBcmStatus BcmDiag_Status(void)
{
    NetBcmStatus msg;
    uint32_t bit;

    msg.bcm_ecu_state = s_diag.state;
    msg.fault_mask = FaultMask();
    msg.active_fault_category = s_diag.hasActive ? k_faultInfo[s_diag.active].category : NET_FAULT_CATEGORY_NONE;
    msg.active_fault_code = s_diag.hasActive ? (uint16_t)((uint16_t)s_diag.active + 1u) : 0u;
    msg.affected_function = 0u;
    for (bit = 0u; bit < BCM_FAULT_COUNT; bit++)
    {
        if (s_diag.faults[bit])
        {
            msg.affected_function |= k_faultInfo[bit].affected;
        }
    }
    msg.recovering = s_diag.recovering ? NET_FLAG_YES : NET_FLAG_NO;
    msg.update_sequence = 0u;
    msg.source_age_ms = 0u;
    return msg;
}
