# Board binding status ? Network scaffold

As of 2026-10-17 (Asia/Seoul), **S32K144 and S32K344 Network HAL bindings remain `NOT_CONFIGURED` / `BINDING_OPEN`**. Host Mock is a separate injected test implementation. A Host pass establishes common C behavior only. Actual target build, flash, CAN/UART communication, interrupt concurrency and electrical measurements are **NOT_RUN**.

## Platform boundary

| Area | S32K144 | S32K344 | Host Mock |
|---|---|---|---|
| Driver and SDK | No applicable local Network driver project established | RTD/PlatformSDK 7.0.1 audio evidence found; Network driver binding not established | Standard C; no SDK required |
| Pin / transceiver / bus instances | BINDING_OPEN | Audio SAI/LPI2C/GPIO pin setup exists; CAN/UART assignment and transceiver remain BINDING_OPEN | Injected bytes and transport kind |
| Clock / timestamp | BINDING_OPEN | Audio PIT reference clock exists; Network tick units, wrap, byte timestamp and STM policy BINDING_OPEN | Test-controlled fake clock |
| CAN nominal/data bitrate / UART baud | BINDING_OPEN | BINDING_OPEN | No physical rates asserted |
| IRQ / ISR / synchronization | BINDING_OPEN | BINDING_OPEN | Deterministic single owner; no real ISR proof |
| Admission / terminal / release | Actual device contract unverified | Actual device contract unverified | Independently injected/observed events |
| Target activation | NOT_CONFIGURED | NOT_CONFIGURED | Host build/test target; see `_local_review/HOST_TEST_RESULTS.txt` |
| Hardware verification | NOT_RUN | NOT_RUN | Not applicable |

MCU adapter skeletons must return `NOT_CONFIGURED`/`UNSUPPORTED`, and unsupported target configuration must fail explicitly. No pin, frequency, baud rate, transceiver-enable sequence, IRQ registration, SDK API, or successful hardware outcome is invented. The common Layer/Module sources use only portable C and the common Port interface; platform internals stay under `src/platform/`. M08 owns the boundary; platform folders do not create a sixth Layer.

## Concrete local evidence

Repository HEAD is `00a43414807e7a5b9c087d589a1183d1989e5b91`. `hardware/README.md` is the only hardware file, and HEAD firmware contains only `firmware/README.md`. Their hashes and relevant read-only design sources are in `_local_review/BOARD_EVIDENCE.json`.

The inspected external archive is `C:/Users/leejh/Downloads/S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4.zip`. Its project prefix is `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/`. Members were read in memory; no SDK files were copied into this repository.

- `.settings/com.freescale.s32ds.cross.sdk.support.prefs` declares `PlatformSDK_S32K3_S32K344_M7_7.0.1_PATH` for Debug_FLASH/Debug_RAM. `RTD/include/Clock_Ip.h` reports software version **7.0.1**; pin and generated clock files agree.
- `.cproject` references S32DS K3 ARM GNU 10.2 toolchain variables and external PlatformSDK include roots. These are project metadata, not proof that the corresponding toolchain/SDK is installed or linked into this Network build.
- `board/Siul2_Port_Ip_Cfg.c:108` onward lists SAI0 (PTB2/PTC12/PTB3/PTC13), LPI2C0 (PTD13/PTD14), GPIO and ADC assignments. This is audio-board evidence; it does not establish Network CAN/UART pins or transceiver wiring.
- `src/AudioClock.c` uses PIT0/channel0 plus LPI2C0 for a CS2100 audio reference clock. It does not establish Network millisecond timestamps, timestamp wrap handling, per-byte UART timing, or Network STM0 ownership.
- No archive member name contains FlexCAN, LPUART, Can_Ip, or Uart_Ip. The audio project's existence is insufficient to implement/validate Network CAN/UART drivers.
- `docs/vss/design/90_BINDING/90_C_FILE_API_MAPPING.md` and `91_IMPLEMENTATION_TBD.md` explicitly keep actual lower mapping, generated source, IRQ/DMA completion/lifetime and Node Communication evidence open. Their historical audio evidence does not close Network G-08.

| Archive / member | SHA256 |
|---|---|
| `C:/Users/leejh/Downloads/S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4.zip` | `27a5cae866bc3a3a187db570b2d18501bcada57fb0b41d1fe7086068a3849836` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/.settings/com.freescale.s32ds.cross.sdk.support.prefs` | `83a09b67e14020bde7fc64d60978c011cf12e85bd3470d0f1dd389834ddfe283` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/RTD/include/Clock_Ip.h` | `a7fa41dc1d9ebfe0947c8eb67918f6e1d87eabcb3d0a7d6681ebe5f33a388e89` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/generate/src/Clock_Ip_Cfg.c` | `209c571b393c6eeaab76807b0e1dc278527f0bec1c5c0431d9ff3f914cc11fcc` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/board/Siul2_Port_Ip_Cfg.c` | `8f4649a9c33593aaa9c85a873c809f9115f8e3c0e2608988af1ae77a4d9fc933` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/src/AudioClock.c` | `d57afb181633ef628f1b66887fd93974f95fff69bc0e5c8d5ecf640001df91ed` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/src/AudioClock.h` | `d9c1c066e6403904152d6951af2b99d97722cab5808a2887a85ca81c56ab087c` |
| `S32K344_AUDIO_BRINGUP_STUDY_TRACKER_v1.4/S32K344_WB_Audio_Bringup/.cproject` | `6641231574dbe31fbe78e27949cfbf072e1dd64e17efcf98906174e2088a831e` |

The scoped filename search found no accessible S32K144-specific Network SDK/pin project. This is a scoped discovery result, not a claim that no SDK exists anywhere on the computer. Read-only discovery limitations are recorded in `_local_review/EVIDENCE_REVIEW.md`.

## Required evidence before real binding

For each board, identify the approved board revision and schematic, CAN/UART instances and pin mux, transceiver controls, generated peripheral configuration, exact SDK/toolchain, clock units/rates and wrap rules, ISR-safe ownership/synchronization, frame/byte timestamp source, and bounded queue sizing. Demonstrate callback identity across restart and the independent meanings of driver admission, final transmission result and reference release. Then perform target compile/link, physical loopback/bus tests and error/recovery tests. None of these steps is inferred from Host tests.

Round4 v1.4 keeps G-08 `NOT_RUN` and the broader product gates OPEN/HOLD. See `_local_review/EVIDENCE_REVIEW.md` for the 11 IDs / 32 positions and source distinctions. Existing DOMAIN v0.5/Vehicle v0.2 documentation headers and separate v0.6/v0.3 candidate headers are not board bindings or approved Network ABI integration.
