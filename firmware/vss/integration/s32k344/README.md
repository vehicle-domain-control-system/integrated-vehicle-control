# S32K344 guarded source integration

This is a source adapter with an explicit physical HOLD. It is not a runnable
replacement target project and does not claim hardware validation.

`vss_audio_backend.c`, built with `VSS_S32K344_REFERENCE_BINDING`, reads the
existing `AudioClock_IsLocked`, `AudioCodec_IsConfigured`, and
`AudioDmaTransport_IsDmaReady/IsSaiReady/IsStarted` interfaces. The three headers
here are byte-identical reference copies, with original archive paths and SHA
values in `SOURCE_MANIFEST.json`. A target build should include the original
project headers and actual RTD `Std_Types.h`. The Host test uses the explicitly
guarded `audio_reference/host_shim`; it is not a replacement target SDK.

All device-ready values may be true while VSS preparation/start remains HOLD.
The original transport offers no per-operation physical activation correlation,
STOP fence, or proved cyclic CPU-write window. The adapter never invokes
`AudioDmaTransport_Start/Service`, `Sai_Ip_Send`, codec unmute, decoder refill,
or a fabricated STOP. It reports the missing physical capability and preserves
the original operation and Session-wide logical production fence. No actual,
NO_START, termination, PCM safe return or retirement is inferred from readiness.

The portable real PFlash asset/minimp3 path and exact valid tail lengths are
implemented and tested separately in `../../audio_reference`. Production
semantic-to-asset mapping remains explicit and unavailable until a real plan is
provided; the existing SoundAsset demo catalog is not a nine-signal catalog.
The original decoder must be selected/reset with an explicit asset and one-shot
mode by its owner. Its singleton state cannot be shared concurrently with the
old `AudioPlayer` or demo playlist.

## Target integration boundaries

Preserve original pin mux, clocks, CS2100/SGTL5000 configuration, generated SAI
and DMA configuration, linker/no-cache placement and the single continuous
vendor registration. Do not run the old demo entry alongside VSS: original
`AudioAppControl.c:70` falls back to Welcome, `:422` auto-starts Welcome, and
`VssAudioSession.c:273` overwrites its pending slot. Original `main.c:173,258,270`
are demo-owner call sites requiring replacement when the hardware gate is
eventually implemented. This package does not alter those files or activate
that path.

Original `AudioDmaTransport.c:41` allocates 8208 bytes: 16-byte startup FIFO
prime then A/B of 4096 bytes each. The allocation is aligned to32 bytes; A/B
start at offsets16/4112 and are not individually32-byte aligned. Keep this
geometry. Do not add a third transfer PCM array. The decoder's existing scratch
is a separate, bounded decoder resource. `SLAST=-8192`, `INTHALF=1`,
`INTMAJOR=1`, `DREQ=0` and one `Sai_Ip_Send` are original source facts.

Before enabling real effects, prove operation/Attempt/A-or-B/cycle attribution,
old-reference and old-content termination, a current write window with measured
decode/refill WCET plus interrupt delay, no-cache/barrier conditions, and the
future-output and residual FIFO/frame fences. HALF/MAJOR phase counters,
consumption, zero fill, idle, or abort acknowledgement do not meet those gates.
Original `AudioPlayer_IsOneShotAudiblyComplete/IsAudiblyStopped` must not be
converted into confirmed VSS output facts. Clock reference ticks also require
an actual domain/epoch/continuity and units binding; they are not milliseconds.

## Reproducible Host adapter check

Compile `src/vss_audio_backend.c` and `tests/test_audio_target_gate.c` with C99,
the normal strict warnings, `VSS_S32K344_REFERENCE_BINDING`,
`VSS_AUDIO_REFERENCE_HOST`, and include paths `src`, `integration/s32k344`,
`audio_reference/host_shim`. The test provides only read-only readiness stubs,
all true, and verifies that preparation/start remain HOLD and emit no output
facts. No `VSS_HOST_TEST` is used for this production-path check.

The archived project requests GCC Arm10.2/S32DS3.6.6 and external BaseNXP/Platform
SDK headers. The compiler, original workspace and C:/NXP installation are
absent here; the archive lacks `Std_Types.h`, `Platform_Types.h`, `Mcal.h` and
`S32K344.h`. Target compile/link and board timing/safety remain NOT_RUN/HOLD.
The historical ELF/MAP is original evidence, not a new-build result.
