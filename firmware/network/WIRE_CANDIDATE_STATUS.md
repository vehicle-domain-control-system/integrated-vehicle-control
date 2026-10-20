# Wire Wave1 baseline candidate boundary

Current CAN20 extension and read-only verifier repair: see `WIRE_WAVE2A_STATUS.md`.
This document records the preserved Wave1 scope.

`network_wire_candidate` is a separate, SDK-free C11 library. It has no dependency
on `network_common`, `network_host_mock`, C2, adapters or MCU ports. CMake defaults
to OFF; calls require the explicit CANDIDATE_ONLY profile. Product adoption,
wire/ABI freeze, runtime consumption, scheduling, transmit and physical testing
remain HOLD. S32K144/S32K344 remain NOT_CONFIGURED.

| Layer/module | Added implementation | Tested candidate behavior |
|---|---|---|
| L04/M05 | `net_wire_can.c`, `net_wire_can.h` | CIS_REAR 0x0A0 and DOMAIN_PERMISSION 0x080, used19/frame20/DLC11 |
| L04/M06 | `net_wire_uart.c`, `net_wire_uart_dto.c`, `net_wire_stream.c`; public UART headers | UART-C v1 P_VISION_RESULT/P_PERMISSION; UART-M v2 M_CONTEXT/M_REQUEST/M_RESULT; CRC and bounded per-link stream |
| L04 shared M05/M06 | `net_wire_common.c`, `net_wire_internal.h`, `net_wire_registry.c`, `net_wire.h` | One LE implementation, range/profile helpers, CAN22/M23/C3 registry |

These shared utilities are not additional Layers or Modules. The existing five
Layers/nine Modules and all original 28 C/H files are preserved. M02 remains design
only and M03 product handoff remains unbound. Generic UART envelope validation of
a registry-only type is structural; it never creates an implemented typed DTO.

CAN explicitly validates standard 11-bit ID, no extended/RTR/driver error,
FDF/BRS true, DLC11, 20 captured bytes, and zero final padding. Raw123 means
123 mm. NO_OBJECT/65535/VALID/NONE is represented; VALID_DISTANCE/65535 is
malformed. Unsupported cross-field semantic combinations return CONTRACT_HOLD.
DOMAIN_PERMISSION preserves declared enum bytes and original AGE rather than
computing permission. Quality projection and downstream withdrawal stay HOLD.

UART uses A5 5A, exact per-type length, little-endian integers and CRC bytes,
CRC16-CCITT-FALSE (poly1021/initFFFF/no reflection/xorout0) over B2..B(P+7).
P<=128 and frame<=138 are hard bounds; known types still require their exact P.
Legacy M_CONTEXT P31/frame41 is rejected. DTO padding is never serialized.
Current downlink SESSION and original REQUEST_SESSION are independent fields.
M_REQUEST validates all eight argument categories (nine kind/op pairs with
door open/close), reserved zero bytes, booleans/Fan/RGB and signed i16 centi-C;
it does not clamp temperatures or execute requests. P_PERMISSION USE_LIMIT500
and1500, source AGE and all sequences remain raw values; no TTL/ALLOW decision
is implied. Unsupported semantic contradictions remain HOLD without repair.

All public encode/decode failures leave outputs unchanged and reject overlapping
input/output ranges. Both UART typed decode outputs must be disjoint. Caller
provides valid, sufficiently sized C objects and stable borrowed arrays. No heap,
vendor types or wire casts are used. APIs are serialized by the caller, outside
ISR; no interrupt/thread synchronization is claimed.

Stream instances have distinct M/C link and direction. Each call takes <=512
bytes and original per-byte monotonic uint64 millisecond timestamps. `OK` returns
one complete structural frame and consumed count; `NEED_MORE` commits consumed
input and diagnostics while leaving the output frame unchanged. Other failures
are transactional for state, consumed count and frame. Callers must re-present
any unconsumed suffix. Sync recovery advances one byte from the rejected sync;
suffix timestamps are retained. Fully framed payload SYNC remains opaque.

Time policy must be explicitly selected: 50ms is PROPOSED, and equality behavior
is caller-selected. `timer_check_permitted` means queued earlier RX has been
drained; current time can expire only incomplete data. Complete-frame assembly
uses original last-byte time, so worker delay does not create age. Accepted timer
watermarks are recorded only after the supplied batch has been fully consumed.
Backward/ambiguous capture or timer time returns TIME_HOLD. Observed link gaps
discard partial data but preserve monotonic watermarks. Explicit re-init requires
caller validation of a new clock epoch; uint64 wrap/cross-domain conversion is
not guessed. Parser counters saturate and are not vehicle Health or Fault events.

Seven independent literal goldens, strict GCC tests, signed-i16 exhaustive
conversion, 4,000 CAN randomized round trips and 3,000 candidate stream replay
frames are recorded in the result ZIP. Original synthetic-fixture stress remains
separate. Clang is unavailable locally; ASan/UBSan link libraries are missing;
GCC6.3 rejects -fanalyzer. These checks are NOT_RUN, not inferred from historical
review results. HAL/flash/physical tests are NOT_RUN.

G01..G08 and Round4 P1/P2 remain OPEN/HOLD. Source, offsets, source SHA, exact
test commands, remaining ambiguity and all48 progress are in the result ZIP.
