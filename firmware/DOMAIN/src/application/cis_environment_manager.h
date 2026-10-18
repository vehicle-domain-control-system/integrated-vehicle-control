#ifndef CIS_ENVIRONMENT_MANAGER_H
#define CIS_ENVIRONMENT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "Vehicle_Types_v0.2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CIS_ENVIRONMENT_TEMPERATURE_TTL_MS UINT32_C(2500)
#define CIS_ENVIRONMENT_HUMIDITY_TTL_MS UINT32_C(2500)

/*
 * Domain-owned typed cache for one Network-admitted CIS environment aggregate.
 *
 * The caller owns this fixed-size context and serializes all calls. Network is
 * responsible for BOOT admission, old callback draining, and delayed old-BOOT
 * rejection before Store() is called. This Core deliberately does not infer a
 * BOOT transition from NO_DATA or from a sensor updateSequence, and it never
 * rewrites a producer sequence.
 */

typedef enum
{
    CIS_ENVIRONMENT_MANAGER_OK = 0,
    CIS_ENVIRONMENT_MANAGER_INVALID_ARGUMENT,
    CIS_ENVIRONMENT_MANAGER_NOT_INITIALIZED
} CisEnvironmentManager_Status_t;

typedef struct
{
    int16_t value;
    ValueQuality_t quality;
    uint32_t receivedAtMs;
    bool hasObservation;
} CisEnvironmentManager_TemperatureRecord_t;

typedef struct
{
    uint16_t value;
    ValueQuality_t quality;
    uint32_t receivedAtMs;
    bool hasObservation;
} CisEnvironmentManager_HumidityRecord_t;

typedef struct
{
    uint32_t value;
    ValueQuality_t quality;
    uint32_t receivedAtMs;
    bool hasObservation;
} CisEnvironmentManager_IlluminanceRecord_t;

typedef struct
{
    CisEnvironmentManager_TemperatureRecord_t temperature;
    CisEnvironmentManager_HumidityRecord_t humidity;
    CisEnvironmentManager_IlluminanceRecord_t illuminance;
    /*
     * Keeps the original diagnostic records while forcing every effective
     * projection to NO_DATA. Only a successful Network-admitted Store()
     * clears this latch; ordinary NO_DATA input does not imply a BOOT event.
     */
    bool invalidated;
    bool initialized;
} CisEnvironmentManager_t;

typedef struct
{
    bool hasObservation;
    uint32_t receivedAtMs;
    uint32_t localElapsedMs;
    uint32_t effectiveAgeMs;
} CisEnvironmentManager_SensorTiming_t;

typedef struct
{
    /* Exact last accepted values and producer metadata for diagnostics. */
    CisCabinEnvironmentState_t original;
    /* Read-time projection. Only AGE and an applicable TTL may be stricter. */
    CisCabinEnvironmentState_t effective;
    CisEnvironmentManager_SensorTiming_t temperatureTiming;
    CisEnvironmentManager_SensorTiming_t humidityTiming;
    CisEnvironmentManager_SensorTiming_t illuminanceTiming;
    bool invalidated;
} CisEnvironmentManager_Snapshot_t;

CisEnvironmentManager_Status_t CisEnvironmentManager_Init(
    CisEnvironmentManager_t *manager);

/*
 * Synchronously and atomically blocks all effective sensor values while
 * retaining the last accepted values and producer metadata in original.
 */
CisEnvironmentManager_Status_t CisEnvironmentManager_Invalidate(
    CisEnvironmentManager_t *manager);

/*
 * Atomically stores all three validated sensor records. The input is assumed
 * to have passed Network BOOT/ordering admission. A lower or wrapped producer
 * sequence is therefore preserved, not compared with a prior BOOT's value.
 * A successful store clears an earlier invalidation latch; a rejected store
 * leaves the latch and all records unchanged.
 */
CisEnvironmentManager_Status_t CisEnvironmentManager_Store(
    CisEnvironmentManager_t *manager,
    const CisCabinEnvironmentState_t *state,
    uint32_t receivedAtMs);

/* Read-only copy; failure leaves outSnapshot unchanged. */
CisEnvironmentManager_Status_t CisEnvironmentManager_GetSnapshot(
    const CisEnvironmentManager_t *manager,
    uint32_t nowMs,
    CisEnvironmentManager_Snapshot_t *outSnapshot);

#ifdef __cplusplus
}
#endif

#endif /* CIS_ENVIRONMENT_MANAGER_H */
