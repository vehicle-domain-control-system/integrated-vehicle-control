# Integrated Domain source map

This team review contains the validated Domain modules and DOMAIN Network bundle.
The licensed NXP-template-derived `main.c`, project metadata, generated settings,
Startup and Linker remain in the separate standalone Production preservation
branch; they are not redistributed by this team change. That validated bootstrap
keeps Network hardware and UART diagnostic approvals false and CAN0 in Standby.

| Area | Responsibility |
|---|---|
| `src/application/domain_runtime.*` | Owns Managers, RX/Result/Query bindings and bounded Poll. |
| `src/interface/` | Existing C2 DTO/API and local Core; wire meanings unchanged. |
| `src/application/domain_network_admission_bridge_private.*` | Private Admission seam and distinct read-only trusted-Gateway mode. Monitoring creates no P6/control evidence. |
| `network/` | Validated DOMAIN bundle, including local integration changes listed in `docs/NETWORK_LOCAL_CHANGES.md`. |
| `src/board/` | STB, fail-closed Network bootstrap, STM0 and UART diagnostic adapters. |
| `tests/host/` | Portable tests, original Admission fixture and recovered integration harnesses. |
| `tests/network/` | Preserved Network/RTD-mock regression sources; no vendor RTD implementation. |

Occupant storage is enabled independently of Occupant Warning. Permission,
Request Coordinator, Smart Access and Auto Unlock remain disabled. Status/Door
SOURCE_AGE TTLs without a production contract are not inferred from RX timeouts.

See `docs/INTEGRATION_STATUS.md` for current policy and validation boundaries.
