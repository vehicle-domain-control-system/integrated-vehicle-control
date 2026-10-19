#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../asset/audio_asset.h"
#include "../../infra/vss_config.h"

/* One playback session at a time: Prepare selects the asset, START begins
 * decoding into the output ring, and the session ends with exactly one of
 * COMPLETED, STOPPED or FAULT.
 *
 * Fact times are output half-buffer boundaries, in ms of the system clock:
 *   STARTED   the boundary at which the half holding the session's first PCM
 *             began to play
 *   COMPLETED the boundary at which the half holding a one-shot asset's last
 *             PCM finished playing
 *   STOPPED   after STOP, the first boundary at which the playing half holds
 *             no session PCM (at most two boundaries after the request); a
 *             STOP before any PCM was handed to the output reports STOPPED at
 *             once, with the latest boundary time and everStarted false
 *   FAULT     the boundary at which the failure was seen (decode failure:
 *             like STOPPED, once the session's PCM has left the output)
 * everStarted tells whether STARTED was reported for the session. */

typedef enum {
    STREAM_FACT_STARTED = 0,
    STREAM_FACT_COMPLETED,
    STREAM_FACT_STOPPED,
    STREAM_FACT_FAULT
} StreamFactKind;

typedef struct {
    StreamFactKind kind;
    uint32_t token;
    uint64_t atMs;
    bool     everStarted;
    uint32_t faultMask;      /* FAULT only: transmit fault bits and the bits below */
} StreamFact;

typedef enum {
    STREAM_OK = 0,
    STREAM_BUSY,
    STREAM_REJECTED
} StreamResult;

/* Fault bits of the stream itself. The low bits of faultMask carry the
 * transmit driver's latched fault bits (late refill, overlap, interface
 * error, stalled boundaries). */
#define AUDIO_STREAM_FAULT_DECODE      (1UL << 16)  /* the asset could not be decoded */
#define AUDIO_STREAM_FAULT_REQUEST     (1UL << 17)  /* the driver refused a refill without a fault of its own */
#define AUDIO_STREAM_FAULT_FACT_QUEUE  (1UL << 18)  /* facts were not collected in time */

/* Facts held until AudioStream_Advance collects them. The last slot is kept
 * for the FAULT that reports an overflow. */
#define AUDIO_STREAM_FACT_CAPACITY 8u

/* Idle, no facts, decoder reset. Called once at boot, before the system
 * clock starts counting. */
void AudioStream_Init(void);

/* Selects the asset of a new session. STREAM_BUSY while a session exists;
 * STREAM_REJECTED for an unknown or unreadable asset, or once the output
 * has failed (the output stays failed until reset). */
StreamResult AudioStream_Prepare(uint32_t token, AudioAsset_IdType asset, bool loop);

/* start == true begins the prepared session; false requests STOP.
 * STREAM_REJECTED when token is not the current session. */
StreamResult AudioStream_RequestControl(uint32_t token, bool start);

/* Called first in every main-loop pass: handles at most one output event,
 * reports facts and refills the free half (decoded PCM or silence). */
void AudioStream_Service(void);

/* Moves up to capacity facts, oldest first, into out. Returns the count. */
size_t AudioStream_Advance(StreamFact *out, size_t capacity);

bool AudioStream_IsIdle(void);

/* True once the output has failed: the stream's own latched failure, or a
 * fault the transmit driver latched that the stream has not handled yet.
 * Both last until reset; Prepare is refused from the next service on. */
bool AudioStream_OutputFailed(void);

#endif /* AUDIO_STREAM_H */
