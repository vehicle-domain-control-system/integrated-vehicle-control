#ifndef P6_AUTHORIZATION_PROVIDER_H
#define P6_AUTHORIZATION_PROVIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "p6_session_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const p6_session_manager_t *manager;
} p6_authorization_provider_t;

/*
 * Provider calls are read-only and do not advance time.  The manager's single
 * owner must call p6_session_manager_tick() before exposing a snapshot.
 */

void p6_authorization_provider_init(p6_authorization_provider_t *provider,
                                    const p6_session_manager_t *manager);
p6_session_status_t p6_authorization_provider_read(
    const p6_authorization_provider_t *provider,
    p6_authorization_snapshot_t *out_snapshot);
bool p6_authorization_provider_matches(
    const p6_authorization_provider_t *provider,
    uint32_t device_context_id,
    uint64_t session_id,
    uint32_t connection_generation);
bool p6_authorization_provider_is_production_bound(void);

#ifdef __cplusplus
}
#endif

#endif
