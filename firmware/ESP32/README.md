# ESP32 Gateway Firmware

ESP32-only firmware for the Android MOBILE BLE ↔ S32K344 UART-M vehicle gateway.

## Baseline

- ESP-IDF: v5.5.5
- Target: `esp32`
- Language: C
- RTOS: FreeRTOS
- BLE host: NimBLE (configured for future BLE implementation)

The current baseline contains the ESP-IDF application skeleton, UART-M v2 CRC/frame/parser,
message codec, UART transport, host tests, and isolated hardware test projects. BLE GATT,
Gateway Core, Session/Security, and real S32K344 end-to-end integration are not implemented by
this initial integration.

## ESP-IDF build

Use the ESP-IDF v5.5 PowerShell shortcut, or start a PowerShell process that permits the signed
environment profile for that process only:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass
. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
idf.py --version
idf.py set-target esp32
idf.py build
```

`sdkconfig` and `build/` are local generated outputs and must not be committed. The repository
keeps only `sdkconfig.defaults`.

Build success does not mean that the image was flashed or tested on a board.

## Host UART tests

Run each independent CMake test project from this directory:

```powershell
cmake -S tests/uart_frame -B build-host/uart_frame
cmake --build build-host/uart_frame
ctest --test-dir build-host/uart_frame --output-on-failure

cmake -S tests/uart_codec -B build-host/uart_codec
cmake --build build-host/uart_codec
ctest --test-dir build-host/uart_codec --output-on-failure

cmake -S tests/uart_transport -B build-host/uart_transport
cmake --build build-host/uart_transport
ctest --test-dir build-host/uart_transport --output-on-failure
```

## Hardware tests

`hardware_tests/` contains independent ESP-IDF projects. Read each test README before building or
flashing. The UART2 loopback test explicitly uses GPIO27 TX and GPIO26 RX and can conflict with an
unknown PCB circuit; its inclusion does not certify those pins as safe on every board.

The main firmware does not hardcode the UART application GPIOs. Keep UART0 for Flash/Monitor.

## Integration boundaries

- Do not copy or regenerate `docs/references/` from the personal repository into this directory.
- Do not modify team-owned Network, Domain, Mobile, or other ECU paths as part of ESP32 sync.
- Do not enable automatic vehicle Unlock until registration and continuous FAR evidence have an
  approved two-end contract.
- Report unexecuted board, BLE, UART electrical, and S32K344 checks as `NOT_TESTED`.
