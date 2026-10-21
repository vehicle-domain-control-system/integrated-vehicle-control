#include <assert.h>

#include "Gateway_PolicyConfig.h"

int main(void)
{
    Gateway_ProximityPolicyConfig_t config;
    const Gateway_ProximityPolicyConfig_t valid = {
        .near_enter_rssi_dbm = -60,
        .far_exit_rssi_dbm = -70,
        .stable_sample_count = 2U,
        .proximity_expiry_ms = 500U
    };

    Gateway_PolicyConfig_Reset();

    assert(!Gateway_PolicyConfig_IsInitialized());
    assert(Gateway_PolicyConfig_Init() == GATEWAY_STATUS_OK);
    assert(!Gateway_PolicyConfig_IsProximityConfigured());

    assert(
        Gateway_PolicyConfig_GetProximity(&config)
        == GATEWAY_STATUS_NOT_READY);

    assert(
        Gateway_PolicyConfig_SetProximity(&valid)
        == GATEWAY_STATUS_OK);

    assert(Gateway_PolicyConfig_IsProximityConfigured());

    assert(
        Gateway_PolicyConfig_GetProximity(&config)
        == GATEWAY_STATUS_OK);

    assert(config.near_enter_rssi_dbm == -60);
    assert(config.far_exit_rssi_dbm == -70);
    assert(config.stable_sample_count == 2U);
    assert(config.proximity_expiry_ms == 500U);

    {
        Gateway_ProximityPolicyConfig_t invalid = valid;
        invalid.near_enter_rssi_dbm = -80;
        invalid.far_exit_rssi_dbm = -70;

        assert(
            Gateway_PolicyConfig_SetProximity(&invalid)
            == GATEWAY_STATUS_INVALID_ARGUMENT);
    }

    return 0;
}
