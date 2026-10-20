# Current Network source-to-responsibility mapping

This mapping describes the current isolated portable product source. It supersedes
the old scaffold-only mapping; immutable historical Wire reports remain evidence
for their original revisions. The design has **5 Layers, 9 Modules and 25
responsibility IDs**. `src/platform/` implements M08 bindings, not an extra layer.

| Layer / module | Product files | Responsibilities and actual behavior |
|---|---|---|
| L01 / M01 | `net_upper_source.c`, `net_upper_receipt.c`, `net_source.c` | F01-F03: immutable source age/receipt provenance, independent original-identity guards, accepted Pi/CAN relays and context ownership |
| L01 / M02 | `net_upper_query.c`, `net_upper_query.h` | F04-F05: bounded query lifecycle, 18 getter slots / 17 output schemas, provenance, final-TX accounting and End |
| L02 / M03 | `net_upper_adapter.c`, `net_ecu_adapter.c`, `net_domain_*.c` | F06-F10: typed ECU48 handoff, actual pinned C2 TxPort/RX/query/local-failure conversions; external Owner hooks remain explicit |
| L03 / M04 | `net_upper_service.c`, `net_upper_can.c`, `net_upper_uart.c`, `net_upper_control.c`, `net_upper_monitor.c` | F11-F13: immutable queue, dispatch, 48 routes / 51 receive destinations, original/transport identity, independent AGE lanes, context, expiry and recovery control |
| L04 / M05 | `net_wire_can*.c` | F14-F15: 22 CAN FD typed encode/decode functions, route envelopes, sizes/padding, payload ranges and errors |
| L04 / M06 | `net_wire_uart*.c`, `net_wire_stream.c` | F16-F18: 23 UART-M / 3 UART-C typed codecs, CRC, bounded stream resynchronization and original byte timestamps |
| L05 / M07 | `net_upper_outcome.c`, `net_upper_detail.c` | F19-F21: independent admission/final/release facts, immutable borrowed buffers, failure journal/ack and explicit retirement |
| L05 / M08 | `net_upper_port.c`, `net_hal_adapter.c`, `src/platform/*/net_*_upper_port.c`, `src/platform/host_virtual/net_virtual.c` | F22-F23: clock/backend injection, capture, loss/reset epochs, serialized lifecycle and executable virtual bus backend |
| L05 / M09 | `net_upper_observation.c`, `net_upper_diagnostics.c` | F24-F25: saturating raw counters, snapshots, link/queue observations; actual C2 raw Health getter is delegated without invented Owner severity |

File paths in the first five rows are within the named module. Shared wire LE
helpers and the immutable registry remain in `src/l04_frame_codec/`.
`include/network/` contains portable types; original public Owner headers are
provided read-only by an explicit CMake include path.

The preserved `network_common` library contains `net_runtime`, `net_buffer`,
`net_handoff`, `net_can_codec`, `net_uart_stream` and their original raw fixture.
Its Host tests remain regression evidence. These older synthetic fixture APIs
are not the product wire or the actual vehicle Owner API.

## Executable proof and detailed design trace

Product tests exercise service admission, dispatch, all 48 typed codecs, 51 RX
destinations, original/transport identity collisions, relays, 18 query getter
slots, failure/cancellation, node profiles and injected backend capture. New ECU
adapter tests traverse 22 CAN typed publishers / 25 receive destinations and
26 UART publishers / receive destinations, including 7 held wires / 8 held RX
through distinct observation sinks. Ordinary WINDOW callbacks remain untouched.

`tests/virtual_ecu/` replaces only physical transport and external Owners; separate
ECU workers run the product source. Four TX phases are observed separately.
The result package records exact test names, exit codes and traces. A source
reference is not test execution proof, and a passing wrapper test does not prove
an unbound vehicle policy.

The delivery's `DESIGN_TO_CODE_TRACE.csv` inventories every latest design document
with SHA. Detailed function/callback and typedef tables retain candidate names,
actual product equivalence, call-graph candidates, tests and semantic HOLDs.
Candidate private function spellings are not a requirement for duplicate wrappers
or permission to redefine public `DomainIf_*` declarations.

## Integration boundaries

`NETWORK_TARGET=host` runs Host tests. Native builds selecting s32k144/s32k344
compile portable libraries with an SDK-free injected Port adapter and no Host
executables. Physical HAL, ARM/SDK/toolchain, Flash and whole-ECU RAM fitness are
separate unexecuted gates. Node-profile storage excludes other nodes' states;
unknown non-Network application, RTD, driver, stack, heap and reserve RAM remain
UNKNOWN.

C2 Owner commit evidence, missing actual Owner getters, reason/quality choices,
boot/session issuance and wrap policy remain explicit integration hooks/HOLDs.
WINDOW has separate observation-only transport and no actuator approval. G01-G08
remain HOLD. Historical docs and original Git/firmware were not modified.
