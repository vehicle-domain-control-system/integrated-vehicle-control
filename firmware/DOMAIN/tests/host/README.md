# Recovered Host tests

The current product is at the repository root; the Admission fixture is now
`tests/host/test_domain_network_admission_bridge.c` with its original Host bridge
helper/header. It can also be supplied to ESP32 monitoring_interop as
`DOMAIN_FIXTURE_DIR`. Keep the old referenced fixture until the ESP32 owner changes
its configured path. `DOMAIN_PROJECT` may point to this standalone project.

Runners reuse the validated harnesses. They accept tool/dependency paths rather
than containing a developer's absolute paths. For example:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/run_monitoring.ps1 -Esp32Project <esp32-root> -Cc <gcc> -Cases dispatch_state_gateway_ready
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/run_publishers.ps1 -Esp32Project <esp32-root> -Cc <gcc>
powershell -NoProfile -ExecutionPolicy Bypass -File tests/host/run_door.ps1 -Esp32Project <esp32-root> -Cc <gcc>
```

Omit `-Cases` to run the runner's focused scenario list. Executables are ignored.
The CMake module tests and Network reference tests are retained, but the packaging
step does not claim to have rerun every historical scenario against the final API.
The `tests/network/sdk_mock/rtd` files are independently written Host stand-ins,
not installed NXP RTD code. They must never enter an S32DS target source list.

Other preserved tests include VSS Warning/Event, Network Adapter E2E and UART
rollback/diagnostic cases. Use their matching pinned Network build/profile when
rerunning them; do not mix Host mocks with a real board build.
Their original RTD-mock target matrix/source-list is preserved under
`tests/network/build_reference/` for reconstruction against the canonical pinned
Network layout. It is reference material, not an executable CMake entry point
for the flattened bundle and not a claim of a newly run RTD-mock regression.
