#ifndef NETWORK_NET_SOURCE_H
#define NETWORK_NET_SOURCE_H
#include "network/net_types.h"

/* Host/local observation metadata. Not a wire AGE or BOOT definition. */
typedef struct {
    uint64_t observation_id;
    uint64_t observed_at_ms;
    uint64_t original_age_ms;
} NetSourceStamp;
typedef struct {
    bool bound;
    uint64_t maximum_age_ms;
    bool expires_at_equality;
} NetAgePolicy;
/* Same monotonic clock domain required. No output on ambiguous/overflow time. */
NetStatus net_source_age(const NetSourceStamp *source, uint64_t now_ms,
                         uint64_t *age_ms);
#endif
