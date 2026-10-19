#ifndef VSS_HEALTH_H
#define VSS_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

/* Fault kinds. The value is the bit index in VssStatusSnapshot.currentMask
 * and in the reported fault mask; the reported fault code is value + 1. */
typedef enum
{
    VSS_FAULT_ASSET = 0,
    VSS_FAULT_START,
    VSS_FAULT_OUTPUT,
    VSS_FAULT_STATE,
    VSS_FAULT_INIT,
    VSS_FAULT_KINDS
} VssFaultKind;

/* Service state and availability codes, in the order of the reported
 * status codes. */
typedef enum
{
    VSS_SERVICE_STARTUP = 0,
    VSS_SERVICE_READY,
    VSS_SERVICE_PLAYING,
    VSS_SERVICE_FAULT
} VssServiceState;

typedef enum
{
    VSS_SERVICE_FULL = 0,
    VSS_SERVICE_DEGRADED,
    VSS_SERVICE_UNAVAILABLE
} VssServiceAvailability;

/* One complete status evaluation. Times are on the VssTime millisecond
 * clock. */
typedef struct
{
    uint8_t state;            /* VssServiceState */
    uint8_t availability;     /* VssServiceAvailability */
    bool accepting;
    uint8_t currentMask;      /* bit n = VssFaultKind n currently present */
    bool lastFaultPresent;
    VssFaultKind lastFaultKind;
    uint32_t lastFaultOccurrence;
    uint64_t lastFaultAtMs;
    uint32_t reportSequence;  /* +1 per new evaluation, from 1 */
    uint64_t evaluatedAtMs;
} VssStatusSnapshot;

#endif /* VSS_HEALTH_H */
