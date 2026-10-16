/* Link the real S32K344 public header adapter against explicit host stubs.
 * This is source/interface integration, never a target or physical test. */
#include "vss_audio_backend.h"
#include "AudioClock.h"
#include "AudioCodec.h"
#include "AudioDmaTransport.h"
#include <stdio.h>

boolean AudioClock_IsLocked(void) { return TRUE; }
boolean AudioCodec_IsConfigured(void) { return TRUE; }
boolean AudioDmaTransport_IsDmaReady(void) { return TRUE; }
boolean AudioDmaTransport_IsSaiReady(void) { return TRUE; }
boolean AudioDmaTransport_IsStarted(void) { return TRUE; }
int main(void)
{
    const VssAudioKey key = { 1u, 1u, 1u, 1u, VSS_PRODUCT };
    const VssTimeEvidence time = { VSS_PRODUCT, 1u, 10u, 10u, VSS_CONTINUITY_COMPARABLE };
    VssAudioObservation observation;
    VssAudioOutput output;
    size_t written = 1u;
    if (VssAudio_Prepare(&key, 3u, &time) != VSS_AUDIO_HOLD ||
        VssAudio_Start(&key, &time) != VSS_AUDIO_HOLD ||
        VssAudio_Stop(&key, &time) != VSS_AUDIO_HOLD ||
        !VssAudio_Read(&key, &observation) || !observation.clockReady ||
        !observation.codecReady || !observation.transportReady ||
        !observation.transportRunning || !observation.physicalHold ||
        observation.prepared || observation.startSubmitted || observation.outputClosed)
        return 1;
    VssAudio_Poll(&output, 1u, &written);
    if (written != 0u) return 2;
    puts("PASS S32K344 public header adapter; ready stubs still HOLD; target/board NOT_RUN");
    return 0;
}
