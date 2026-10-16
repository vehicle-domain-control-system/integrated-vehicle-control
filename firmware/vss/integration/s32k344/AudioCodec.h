#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CODEC_VOLUME_PERCENT_MAX 100U

/*
 * AudioCodec public contract intentionally contains no RTD/vendor types.
 * Existing g_sgtl* / g_volume* debug symbols remain global in AudioCodec.c
 * and therefore remain available in S32DS Expressions.
 */
typedef void (*AudioCodec_WaitTicksCallbackType)(uint32 tickCount);

/*
 * Probe and validate SGTL5000 CHIP_ID.
 * The LPI2C peripheral itself must already be initialized by the application.
 */
boolean AudioCodec_Probe(void);

/*
 * Configure the SGTL5000 headphone path.
 * waitTicks provides the already-running monotonic reference timebase.
 * The existing Stage 20 configuration delay is preserved exactly.
 */
boolean AudioCodec_Configure(
    AudioCodec_WaitTicksCallbackType waitTicks
);

/*
 * Enable the DAC path and apply the initial headphone volume.
 * Returns TRUE only when both operations complete successfully.
 */
boolean AudioCodec_StartAudio(
    uint8 initialVolumePercent
);

/*
 * Apply a 0..100 % headphone target when the existing deadband policy says
 * an I2C update is required. 0 % is represented as headphone mute.
 */
void AudioCodec_ApplyVolumeIfNeeded(
    uint8 targetPercent
);

boolean AudioCodec_IsConfigured(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CODEC_H */
