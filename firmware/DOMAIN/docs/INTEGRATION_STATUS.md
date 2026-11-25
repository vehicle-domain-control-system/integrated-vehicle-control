# Domain integration snapshot — 2026-11-25

## Source and evidence

The imported product files are the local S32DS Production implementation.
`production_source_manifest.csv` records its original SHA-256 and an LF-normalized
text SHA-256 (Git checkout may change line endings). Production itself is not a
Git repository. The source-copy audit also preserves the original dirty Git
worktree separately; its metadata and unrelated edits are not part of this commit.

Recorded Production ARM GCC 10.2 Debug_FLASH full rebuild: 0 errors, 5 existing
enum ABI warnings. ELF SHA-256:
`CC66C2D7EFE0F7ABF1B94ACC0EA42C5DAEA722048AF6B859B353B9D71CA23888`.
No ARM rebuild is claimed for a different layout or for the team PR. ELF/MAP,
RTD, executables, caches and backups are not distributed by this branch.

## Completed software

- Runtime/ApplicationEntry and one shared Request/Command/Result ownership path.
- Bounded Deferred Inbox, Admission Evidence and Context/lifecycle invalidation.
- CIS Environment/Rear, read-only Occupant storage, CIS/VSS status bindings.
- Rear Warning and VSS One-shot management (TX acceptance is not sound playback).
- Exact receipt/Snapshot Commit bridge, Runtime state Publishers, Session isolation
  and final TX dispatch validation, including same-step revocation.
- Read-only Monitoring Provider bound in DOMAIN node composition. It becomes active
  only when the currently blocked Network initialization is explicitly enabled later.
- Door result Queue Full retry, using current original Device/Session authorization,
  the stored ResultManager outcome and increasing AGE; ECU commands are not retried.

Recorded focused Host evidence before import: Monitoring 10/10, Publisher/Commit
7/7, Door connection 7/7. The ESP32 workstream separately reported a combined
Monitoring Host E2E PASS. Neither is an Android UI or physical communication test.

## Monitoring/control separation

The approved low-voltage demo assumes an exclusive trusted ESP32 Gateway UART-M
producer with real P6 verification and guarded queue cancellation/AGE handling.
It is not cryptographic UART authentication. Monitoring uses private
`monitoringSessionReady`; it leaves P6/control verification fields false.

| Policy | Value |
|---|---|
| ESP32 M_CONTEXT target refresh | 200 ms (ESP32-owned configuration) |
| Registration/Connection source TTL | 600 ms |
| App source TTL | 1000 ms |
| Domain Context lease | 750 ms |
| Proximity | UNKNOWN/NO_DATA permitted, never promoted to NEAR/FAR |
| Source observation targets | BLE/registration 200 ms; actual APP_STATE 500 ms |

Repeated transmission does not renew source observations. Context, required
source expiry, Link/Boot/Epoch/Session change and revocation fail closed. Already
driver-borrowed frames cannot be recalled. Query/protected Result authorization
is unchanged; the Monitoring-only binding does not authorize those operations.

## Deliberately not enabled

Hardware Gate=false, CAN0 STB HIGH, Request Coordinator=false, Permission=false,
Occupant Warning=false, UART diagnostic approval=false, no Auto Unlock.
BCM firmware/0x100 command execution/0x140 completion evidence has not been found
in the checked workspaces. Door/BCM/CIS/VSS Status SOURCE_AGE TTLs are unresolved;
100/300 ms and 250/750 ms send/RX supervision are not substitute source TTLs.
Fan/climate/interior-light orchestration, Occupant Warning and Welcome/Goodbye
Providers remain deferred. WINDOW/EXTERIOR_LIGHT/Find My Car are out of scope.

CIS/VSS supported Domain software is SW_INTEGRATED. Pi Vision supply,
VSS_CFG_NET_STUB selection, physical CAN/UART/BLE, Android display and sound are
BOARD_NOT_TESTED. ESP32 and Android product code was not modified here.

## Reproduction and retention

The standalone branch `feature/domain-monitoring-integration` in
`kyu-choi/S32K344-Domain-controller` preserves the S32DS bootstrap/configuration.
This team branch excludes NXP-template-derived main, generated settings, Startup,
Linker, project metadata and RTD according to the team import policy. Its source
manifest is restricted to the product files actually present here. It is not an
independently ARM-built S32DS project. Use the licensed Production project for
target validation; do not regenerate settings or enable hardware as a shortcut.

Host commands are documented in `../tests/host/README.md`. Historical 33F text in
`history/` is provenance, not the current monitoring policy. Production, verified
ELF backups and the fixture path still used by ESP32 must not be deleted merely
because this source branch exists.

Packaging verification: the recovered Monitoring runner's normal trusted-Gateway
case passed with the copied product, and the native Runtime CMake target compiled
and linked after its missing VSS target dependencies were restored. This is not
a new full regression or ARM build. Use a short external CMake build directory on
Windows to avoid long object paths.

`git diff --check` reports inherited formatting in the unchanged upstream BCM
codec, a private header's final blank line, and unified-patch context lines.
Those product bytes were kept equal to Production rather than reformatted.
No private-key/token pattern or excluded binary was found in the staged import.
