#ifndef BCM_APP_H
#define BCM_APP_H

#include <stdint.h>

typedef struct
{
    uint32_t command_delivered;
    uint32_t command_expired;
    uint32_t command_zero_id;
    uint32_t command_invalid;
    uint32_t command_handoff_full;
    uint32_t outbox_dropped;
    uint32_t climate_events_dropped;
    uint32_t publish_refused;
} BcmAppCounters;

void BcmApp_Init(uint32_t nowMs);

void BcmApp_Step(uint32_t nowMs);

void BcmApp_GetCounters(BcmAppCounters *out);

#endif
