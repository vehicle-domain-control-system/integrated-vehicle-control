/**
 * @file Domain_PolicyConfig.c
 */

#include "Domain_PolicyConfig.h"

#include <string.h>

typedef struct
{
    bool initialized;
    DomainPolicyConfig_t active;
} DomainPolicyContext_t;

static DomainPolicyContext_t g_policy;

static DomainPolicyU32_t PolicyU32(
    uint32_t value,
    DomainPolicyMaturity_t maturity)
{
    DomainPolicyU32_t out;
    out.value = value;
    out.maturity = maturity;
    return out;
}

static DomainPolicyS32_t PolicyS32(
    int32_t value,
    DomainPolicyMaturity_t maturity)
{
    DomainPolicyS32_t out;
    out.value = value;
    out.maturity = maturity;
    return out;
}

static DomainPolicyU8_t PolicyU8(
    uint8_t value,
    DomainPolicyMaturity_t maturity)
{
    DomainPolicyU8_t out;
    out.value = value;
    out.maturity = maturity;
    return out;
}

void DomainPolicyConfig_LoadDocumentCandidates(
    DomainPolicyConfig_t *outConfig)
{
    if (outConfig == NULL)
    {
        return;
    }

    (void)memset(outConfig, 0, sizeof(*outConfig));

    /* Rear-distance boundaries: current [CANDIDATE]. */
    outConfig->rearValidMinCm =
        PolicyU32(10U, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->rearEmergencyMaxCm =
        PolicyU32(40U, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->rearCautionMaxCm =
        PolicyU32(70U, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->rearValidMaxCm =
        PolicyU32(100U, DOMAIN_POLICY_MATURITY_CANDIDATE);

    /* Auto ventilation demo values: temporary/demo candidates. */
    outConfig->autoVentStartTempDegC =
        PolicyS32(30, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->autoVentStopTempDegC =
        PolicyS32(28, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->autoVentMaxDurationMs =
        PolicyU32(300000U, DOMAIN_POLICY_MATURITY_CANDIDATE);
    outConfig->autoVentTargetClosedPct =
        PolicyU8(80U, DOMAIN_POLICY_MATURITY_CANDIDATE);

    /* Result wait deadlines: current provisional/candidate project values. */
    outConfig->settingResultWaitMs =
        PolicyU32(2000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);
    outConfig->doorResultWaitMs =
        PolicyU32(3000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);
    outConfig->climateResultWaitMs =
        PolicyU32(5000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);
    outConfig->lightApplyResultWaitMs =
        PolicyU32(1000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);
    outConfig->windowMoveResultWaitMs =
        PolicyU32(10000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);
    outConfig->windowStopResultWaitMs =
        PolicyU32(1000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);

    /*
     * 150ms duplicate suppression is auxiliary only.
     * Occurrence identity remains the primary duplicate basis.
     */
    outConfig->eventDuplicateAssistMs =
        PolicyU32(150U, DOMAIN_POLICY_MATURITY_CANDIDATE);

    /* Startup/Wake one-shot event max age: current provisional value. */
    outConfig->oneShotEventMaxAgeMs =
        PolicyU32(2000U, DOMAIN_POLICY_MATURITY_PROVISIONAL);

    /* Recent control-result display history: current project selection. */
    outConfig->recentControlHistoryCount =
        PolicyU8(5U, DOMAIN_POLICY_MATURITY_PROJECT_SELECTED);
}

bool DomainPolicyConfig_Validate(
    const DomainPolicyConfig_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if (config->rearValidMinCm.value >
        config->rearEmergencyMaxCm.value)
    {
        return false;
    }

    if (config->rearEmergencyMaxCm.value >
        config->rearCautionMaxCm.value)
    {
        return false;
    }

    if (config->rearCautionMaxCm.value >
        config->rearValidMaxCm.value)
    {
        return false;
    }

    if (config->autoVentStopTempDegC.value >
        config->autoVentStartTempDegC.value)
    {
        return false;
    }

    if (config->autoVentTargetClosedPct.value > 100U)
    {
        return false;
    }

    if (config->autoVentMaxDurationMs.value == 0U)
    {
        return false;
    }

    if ((config->settingResultWaitMs.value == 0U) ||
        (config->doorResultWaitMs.value == 0U) ||
        (config->climateResultWaitMs.value == 0U) ||
        (config->lightApplyResultWaitMs.value == 0U) ||
        (config->windowMoveResultWaitMs.value == 0U) ||
        (config->windowStopResultWaitMs.value == 0U))
    {
        return false;
    }

    if (config->oneShotEventMaxAgeMs.value == 0U)
    {
        return false;
    }

    if (config->recentControlHistoryCount.value == 0U)
    {
        return false;
    }

    return true;
}

DomainPolicyStatus_t DomainPolicyConfig_Init(
    const DomainPolicyConfig_t *config)
{
    DomainPolicyConfig_t candidate;

    if (config == NULL)
    {
        DomainPolicyConfig_LoadDocumentCandidates(&candidate);
        config = &candidate;
    }

    if (!DomainPolicyConfig_Validate(config))
    {
        return DOMAIN_POLICY_STATUS_INVALID_CONFIGURATION;
    }

    g_policy.active = *config;
    g_policy.initialized = true;

    return DOMAIN_POLICY_STATUS_OK;
}

bool DomainPolicyConfig_IsInitialized(void)
{
    return g_policy.initialized;
}

const DomainPolicyConfig_t *DomainPolicyConfig_Get(void)
{
    if (!g_policy.initialized)
    {
        return NULL;
    }

    return &g_policy.active;
}
