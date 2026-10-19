#ifndef VSS_INPUT_NET_H
#define VSS_INPUT_NET_H

/* Conversion between the Network messages and the INPUT path.
 *
 * The receive side turns a NetVssEvent / NetVssWarningState plus its
 * NetCanRxMeta into the INPUT evidence and hands it to INPUT. The Network
 * callbacks VssApp_OnDomainEvent / VssApp_OnDomainWarningState are
 * implemented here with the PRODUCT origin; the built-in test sequence uses
 * the same functions with the TEST origin, so the two paths differ only in
 * origin.
 *
 * DOMAIN generation: the first message of an origin opens INPUT for its
 * boot_id; a message with another boot_id is a DOMAIN restart that the
 * Network already accepted, and INPUT takes the new generation over before
 * the message is processed.
 *
 * Time: received_at_ms is on the VssTime_NowMs32 clock. The receive time is
 * extended to 64 bits against VssTime_NowMs, which is also the processing
 * time handed to INPUT. A receive time in the future is unusable time.
 *
 * All functions run in the super-loop owner context, never from an ISR, and
 * do not keep the caller's pointers. */

#include <stdbool.h>
#include <stdint.h>

#include "vss_input_store.h"
#include "../health/vss_health.h"
#include "../../net_if/vss_net_api.h"

void VssInputNet_OnEvent(VssOrigin origin, const NetVssEvent *m, const NetCanRxMeta *rx);
void VssInputNet_OnWarningState(VssOrigin origin, const NetVssWarningState *m, const NetCanRxMeta *rx);

/* Status report of one HEALTH evaluation, ages measured to now and saturated
 * at 65535 ms. False (out untouched) for a report that cannot be converted:
 * a code out of range, report number 0, unusable time, or an evaluation or
 * fault time after now. */
bool VssInputNet_ToNetStatus(const VssStatusSnapshot *s, const VssTimeEvidence *now, NetVssStatus *out);

/* Pure message conversions used by the receive functions (no INPUT call).
 * receivedAt is the receive time already placed on the VssTime clock.
 * ConvertEvent returns false for a message that is discarded: NULL argument,
 * event_kind outside 1..5, or a quality or availability code out of range.
 * On success out->occurrence points to *key.
 * ConvertWarning returns the frame's structureVerified: every code in range
 * and the activation metadata format rule held (format 1, or format 0 with
 * activation, its quality, reason, sequence and age all 0). */
bool VssInputNet_ConvertEvent(VssOrigin origin, const NetVssEvent *m, const NetCanRxMeta *rx,
                              const VssFactTime *receivedAt, VssOccurrenceKey *key,
                              VssInputEvidence *out);
bool VssInputNet_ConvertWarning(VssOrigin origin, const NetVssWarningState *m,
                                const NetCanRxMeta *rx, const VssFactTime *receivedAt,
                                VssWarningFrame *out);

/* Per-origin adapter observation (counters saturate). */
typedef struct
{
    bool generationOpen;            /* INPUT opened for acceptedBoot */
    uint32_t acceptedBoot;
    uint32_t generationChanges;     /* DOMAIN restarts taken over */
    uint32_t eventsForwarded;
    uint32_t eventsDiscarded;
    uint32_t warningFramesForwarded;
    uint32_t warningFramesUnverified; /* forwarded with structureVerified false */
    VssInputApplication lastEvent;
    VssWarningBundleResult lastWarning;
} VssInputNetObservation;

bool VssInputNet_Read(VssOrigin origin, VssInputNetObservation *out);

#if VSS_CFG_HOST_TEST
void VssInputNet_ResetForTest(void);
#endif

#endif /* VSS_INPUT_NET_H */
