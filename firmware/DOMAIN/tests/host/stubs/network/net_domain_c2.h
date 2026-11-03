#ifndef TEST_STUB_NETWORK_NET_DOMAIN_C2_H
#define TEST_STUB_NETWORK_NET_DOMAIN_C2_H

#include <stdint.h>

typedef enum {
    NET_DOMAIN_C2_TRANSITION_NONE = 0,
    NET_DOMAIN_C2_TRANSITION_APPLIED,
    NET_DOMAIN_C2_TRANSITION_DUPLICATE_APPLIED,
    NET_DOMAIN_C2_TRANSITION_ID_CONFLICT,
    NET_DOMAIN_C2_TRANSITION_EPOCH_ORDER_UNAVAILABLE,
    NET_DOMAIN_C2_TRANSITION_BUSY,
    NET_DOMAIN_C2_TRANSITION_INVALID_ARGUMENT,
    NET_DOMAIN_C2_TRANSITION_NOT_INITIALIZED,
    NET_DOMAIN_C2_TRANSITION_INVALIDATION_FAILED_BLOCKED,
    NET_DOMAIN_C2_TRANSITION_STALE,
    NET_DOMAIN_C2_TRANSITION_ORDER_ERROR,
    NET_DOMAIN_C2_TRANSITION_UNAVAILABLE
} NetDomainC2TransitionStatus;

typedef struct {
    uint32_t id;
    uint64_t epoch;
    uint32_t revision;
} NetDomainC2TransitionReceipt;

typedef NetDomainC2TransitionStatus (*NetDomainC2TransitionApply)(
    void *context, uint32_t id, uint64_t epoch,
    NetDomainC2TransitionReceipt *out);

typedef struct {
    void *context;
    NetDomainC2TransitionApply environment;
    NetDomainC2TransitionApply occupant;
} NetDomainC2TransitionBinding;

#endif
