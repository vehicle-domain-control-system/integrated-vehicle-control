#include "p6_authorization_provider.h"

#include <stddef.h>

void p6_authorization_provider_init(p6_authorization_provider_t *provider,
                                    const p6_session_manager_t *manager)
{
    if (provider != NULL) {
        provider->manager = manager;
    }
}

p6_session_status_t p6_authorization_provider_read(
    const p6_authorization_provider_t *provider,
    p6_authorization_snapshot_t *out_snapshot)
{
    if ((provider == NULL) || (provider->manager == NULL)) {
        return P6_SESSION_INVALID_ARGUMENT;
    }
    return p6_session_manager_get_snapshot(provider->manager, out_snapshot);
}

bool p6_authorization_provider_matches(
    const p6_authorization_provider_t *provider,
    uint32_t device_context_id,
    uint64_t session_id,
    uint32_t connection_generation)
{
    return (provider != NULL) && (provider->manager != NULL) &&
           p6_session_manager_context_is_current(provider->manager,
                                                 device_context_id,
                                                 session_id,
                                                 connection_generation);
}

bool p6_authorization_provider_is_production_bound(void)
{
    return P6_PRODUCTION_BINDING_ENABLED != 0;
}
