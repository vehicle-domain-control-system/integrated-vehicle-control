#ifndef AUDIO_CLOCK_H
#define AUDIO_CLOCK_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CS2100 + PIT reference-clock service.
 *
 * Vendor/RTD types and debugger-only state stay private to AudioClock.c.
 * Existing g_cs* / g_refToggleCount symbols are still emitted in the ELF,
 * so S32DS Expressions can continue to observe them.
 */

/*
 * Start the PIT-driven CS2100 reference clock.
 * The PIT interrupt table must already have been applied by IntCtrl_Ip_Init().
 */
void AudioClock_InitReference(void);

/* Busy-wait using the monotonically increasing reference counter. */
void AudioClock_WaitRefTicks(uint32 tickCount);

/* Current reference tick snapshot for main-context scheduling. */
uint32 AudioClock_GetRefTicks(void);

/*
 * Probe/configure the CS2100 and perform the existing PLL-lock check.
 * LPI2C instance 0 must already be initialized by the application.
 */
void AudioClock_InitCs2100(void);

boolean AudioClock_IsLocked(void);

/* Generated PIT callback name retained for the existing RTD configuration. */
void Cs2100Ref_Callback(uint8 channel);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CLOCK_H */
