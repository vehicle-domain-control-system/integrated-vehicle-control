#include "network/net_source.h"

NetStatus net_source_age(const NetSourceStamp *source, uint64_t now_ms,
                         uint64_t *age_ms)
{
    uint64_t elapsed;
    if (source == NULL || age_ms == NULL || source->observation_id == 0u)
        return NET_INVALID_ARGUMENT;
    if (now_ms < source->observed_at_ms) return NET_HOLD;
    elapsed = now_ms - source->observed_at_ms;
    if (UINT64_MAX - source->original_age_ms < elapsed) return NET_HOLD;
    *age_ms = source->original_age_ms + elapsed;
    return NET_OK;
}
