#include "Gateway_PolicyConfig.h"

#include <string.h>

static bool s_initialized = false;
static bool s_proximity_configured = false;
static Gateway_ProximityPolicyConfig_t s_proximity_config;

Gateway_Status_t Gateway_PolicyConfig_Init(void)
{
    memset(&s_proximity_config, 0, sizeof(s_proximity_config));
    s_proximity_configured = false;
    s_initialized = true;

    return GATEWAY_STATUS_OK;
}

void Gateway_PolicyConfig_Reset(void)
{
    memset(&s_proximity_config, 0, sizeof(s_proximity_config));
    s_proximity_configured = false;
    s_initialized = false;
}

bool Gateway_PolicyConfig_IsInitialized(void)
{
    return s_initialized;
}

bool Gateway_PolicyConfig_IsValidProximityConfig(
    const Gateway_ProximityPolicyConfig_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if (config->near_enter_rssi_dbm <= config->far_exit_rssi_dbm)
    {
        return false;
    }

    if (config->stable_sample_count == 0U)
    {
        return false;
    }

    if (config->proximity_expiry_ms == 0U)
    {
        return false;
    }

    return true;
}

Gateway_Status_t Gateway_PolicyConfig_SetProximity(
    const Gateway_ProximityPolicyConfig_t *config)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (!Gateway_PolicyConfig_IsValidProximityConfig(config))
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    s_proximity_config = *config;
    s_proximity_configured = true;

    return GATEWAY_STATUS_OK;
}

Gateway_Status_t Gateway_PolicyConfig_GetProximity(
    Gateway_ProximityPolicyConfig_t *config)
{
    if (!s_initialized)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    if (config == NULL)
    {
        return GATEWAY_STATUS_INVALID_ARGUMENT;
    }

    if (!s_proximity_configured)
    {
        return GATEWAY_STATUS_NOT_READY;
    }

    *config = s_proximity_config;

    return GATEWAY_STATUS_OK;
}

bool Gateway_PolicyConfig_IsProximityConfigured(void)
{
    return s_initialized && s_proximity_configured;
}
