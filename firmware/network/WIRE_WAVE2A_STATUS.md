# CAN Wave2A Host candidate (historical baseline)

This file records Wave2A's source67 / ON101 / OFF41 baseline. Current UART21
additions and CAN22/UART-M23/UART-C3 raw capability are in `WIRE_WAVE2B_STATUS.md`.

Scope: CAN22 raw typed candidates, UART5 typed candidates and UART21 registry-only.
All are unadopted candidates. The isolated `network_wire_candidate` target remains
default OFF and requires explicit `NET_WIRE_CANDIDATE_ONLY` calls. No Runtime,
C2, M03 handoff, device Port, HAL, scheduler or vehicle consumer is connected.
The existing five Layers/nine Modules are unchanged.

| L04/M05 source | Raw candidate messages |
|---|---|
| Preserved `net_wire_can.c` | CIS_REAR, DOMAIN_PERMISSION (Wave1) |
| `net_wire_can_bcm_raw.c` | BCM_DOOR_COMMAND, BCM_CLIMATE_COMMAND, BCM_LIGHT_COMMAND, BCM_DOOR_STATE, BCM_CLIMATE_STATE, BCM_LIGHT_STATE, BCM_RESULT, BCM_EVENT, BCM_STATUS |
| `net_wire_can_nodes_raw.c` | WINDOW_COMMAND, WINDOW_STATE, WINDOW_EVENT, WINDOW_RESULT, WINDOW_FAULT, CIS_ENVIRONMENT, CIS_OCCUPANT, CIS_STATUS, VSS_EVENT, VSS_WARNING_STATE, VSS_STATUS |
| `net_wire_can_common.c` | Argument/alias checks and exact envelope/padding using the single Registry48 |

Public declarations are available through `net_wire_can.h`; the BCM/node headers
split the raw types for readability. Each type has an explicit encode/decode pair.
Struct padding is never wire data. No application CRC, heap or vendor header is
introduced. Shared LE helpers include portable signed-i16 conversion.

`NetWireRegistryEntry.typed_wave1` deliberately retains its historical seven
entries. Use `net_wire_registry_has_typed_codec(link,id)` for current capability:
CAN22 plus UART5. This capability is metadata and does not authorize execution.
Existing Wave1 fixture source and all69 original CTest cases remain intact.

The exact standard ID, FDF/BRS/RTR/extended/driver_error flags, DLC, captured
length and every zero padding byte are checked before payload decoding. BOOT_ID0
is reserved. Original counter, command identity, related identity, source AGE and
limit fields are preserved, including0/FFFF where defined; no freshness, ordering,
epoch transition, matching, identity allocation or permission is inferred.
Every failure preserves the entire output object; NULL and any exact/partial
input-output overlap are rejected. Caller buffers must be live and stable and
calls serialized outside ISR. No ISR/thread-safety binding is claimed.

CIS_ENVIRONMENT contains three independent temperature/humidity/illuminance
lanes with their own validity/reason/sequence/AGE. Signed temperature is raw
centi-C; invalid sentinels remain distinct from measured values. The kit's example
wording about four signals does not add a fourth lane. TTL2500/1000 proposals
remain unadopted. BCM RGB order is R/G/B; measured UNKNOWN and command ranges are
distinct. Results/events preserve evidence without inventing execution success,
confirmation or fault transitions. Unsupported semantic combinations return HOLD
where no safe candidate interpretation is documented; reserved scalars reject.

WARNING60 is used60/frame64/DLC15 with padding B60..63. Format0 requires canonical
absence (B50=0 and B52..59=0); format1 preserves supplied metadata. Invalid format,
wrong length and nonzero padding reject. A legacy51-byte capture rejects. A full
64-byte legacy producer frame whose bytes exactly match canonical format0 cannot
be distinguished by bytes alone: candidate profile selection is explicit, and
actual producer/profile adoption remains HOLD. No new discriminator is invented.
VSS_STATUS current FaultMask is u8 and remains separate from last-fault history;
A2 same-K State/Fault commit is not implemented here.

WINDOW's five raw DTOs are Host-only design candidates. They do not enable command
TX, monitoring, application execution or display. VENT70/80 policy is not selected.
Round4 11 finding IDs/32 locations, G01..G08, C2 ABI, Permission propagation,
Query/finalTX, VSS projection and actual MCU bindings remain OPEN/HOLD.
S32K144/S32K344 remain NOT_CONFIGURED; hardware/flash/physical tests NOT_RUN.

## Golden verifier P2 repair

`tools/fixtures/wave1_golden.json` is a byte-copy of the supplied Wave1 fixture
(SHA256 `4c979747887b76a0aed9a295309cb050650ea83da145b9d321569d8f475bd07e`).
`tools/verify_wave1_golden.py` defaults to read-only checking, as does `--check`.
It verifies exact fixture bytes, all parsed fields, independent Python struct/CRC
references, nine C literals and original test source SHA. It writes no fixture,
source, report or cache. Generation/output switches are unsupported and rejected.
The original released Wave1 ZIP, JSON and verifier remain untouched.

```sh
python firmware/network/tools/verify_wave1_golden.py --check
python firmware/network/tests/host/test_wave1_verifier.py
cmake -S firmware/network -B firmware/network/.build-wave2a -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_TARGET=host
cmake --build firmware/network/.build-wave2a --parallel
ctest --test-dir firmware/network/.build-wave2a --output-on-failure
```

Candidate Host tests require Python3.8+ in addition to CMake and a C11 compiler;
the libraries do not require Python. Verifier regressions create isolated scratch
copies under a guarded network build directory, corrupt only those copies and
check SHA before/after successful and failing tool invocations. CRLF original
fixture is accepted on any host; LF-normalized replacement is detected as byte
corruption and is never auto-repaired. Python `-O` cannot bypass verification.
Exact current commands, independent CAN20 references, per-type results, limitations
and immutable-source proof are in `_local_review/wire_wave2a_20261017_v1.0/`.
