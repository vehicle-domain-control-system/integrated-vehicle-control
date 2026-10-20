# UART Wave2B Host raw candidates

Wave2B adds UART-M20 and UART-C P_VISION_STATUS to the isolated candidate
library. The existing CAN22, UART-M3 and UART-C2 are preserved. The current
typed-capability query covers CAN22/UART-M23/UART-C3; historical `typed_wave1`
still marks exactly seven records. Registry IDs, versions, routes and lengths
are unchanged. This is Host capability metadata; product adoption remains HOLD.

All new production files belong to L04 Frame Codec / M06 UART Codec. The five
Layers and nine Modules remain unchanged. `network_wire_candidate` defaults OFF
and has no runtime, Host Mock, C2, MCU, SDK, RTOS or heap dependency. Every call
requires explicit `NET_WIRE_CANDIDATE_ONLY`. MCU ports remain NOT_CONFIGURED.

| Public header / source | Raw candidate group |
|---|---|
| net_wire_uart_m_core.h / net_wire_uart_m_core.c | M_QUERY, M_WARNING_ACK, M_DOMAIN_ALIVE, M_WARNING, M_AVAILABILITY, M_DIGITAL_STATUS, M_DIGITAL_RESULT, M_USER_SETTINGS, M_QUERY_END |
| net_wire_uart_m_projection.h / net_wire_uart_m_projection.c | M_BCM_DOOR_STATE/CLIMATE_STATE/LIGHT_STATE/STATUS; M_CIS_ENVIRONMENT/OCCUPANT/REAR/STATUS; M_VSS_STATUS; M_WINDOW_STATE/FAULT |
| net_wire_uart_c_status.h / net_wire_uart_c_status.c | P_VISION_STATUS |
| net_wire_uart_typed_internal.h/.c (private M06) | Shared argument and payload checks for new DTOs |

The public umbrella is `net_wire_uart_dto.h`. Each new `NetWire...Raw` record has
an explicit encode/decode pair using the original frame encoder/decoder and CRC.
Decode returns the link sequence separately. All integers use explicit LE
operations; signed i16 uses the existing portable helper. Struct padding is not
wire data. Failures leave the entire output frame, DTO and link sequence intact.
NULL and exact/partial overlaps across input/output and both decode outputs
reject. Callers supply live, stable buffers and serialize calls outside ISR.

The current Device/Session/DomainBoot/Query, original CAN Boot/TX, Pi observation,
source evaluation sequences and AGE remain distinct. No clock read, identity
allocation, provenance join, source-age restart, runtime admission or vehicle
operation is implemented. UART-M version2 and UART-C version1 remain separate.
Direction is an API/registry constraint, not an invented payload address byte.

Raw boundaries and unresolved meaning:

- M_QUERY requires nonzero QueryID, reserved0 and cursor0. Scope0's request
  references must be0 as documented. Mixed request identity in scope1 and
  nonzero request filters for scope2/3 are conservative CONTRACT_HOLD; Session
  issuance and consumer admission remain OPEN.
- M_WARNING_ACK referenced DomainBoot0 and M_WARNING source Boot0 have no
  agreed absence form and HOLD. ACK does not CLEAR a warning or STOP sound.
  Known quality paired with a contradictory reason also HOLDs; fields are not
  repaired. Current/history and semantic relations without a truth table stay
  outside the raw codec.
- M_AVAILABILITY requires all nine entries in explicit function-ID order1..9;
  it does not cast C2 enum indices0..8 or invent reasons/availability.
- M_DIGITAL_RESULT keeps ORIGIN2 and distinguishes Result255 from Result0.
  Missing result with confirmation is malformed. DONE does not prove an actual
  Door Unlock transition.
- M_USER_SETTINGS preserves signed temperature and -32768, OFF/0 and UNKNOWN255,
  RGB byte order and raw quality. Frozen Fan codes are0..3/255; the kit test
  example's fan0..100 wording is not a new code range. Brightness is0..100.
  Known quality plus an unknown sentinel HOLDs. There is no representation for
  missing NORMAL brightness/RGB: callers must resolve composite export policy,
  not synthesize a value. The functional temperature setpoint range stays OPEN.
- M_QUERY_END serializes supplied scope/status/count only. It does not calculate
  final-TX count, prove empty success, close Query state, or acknowledge BLE/App
  receipt. G06 remains HOLD, including raw status/count combinations.
- Eleven projections preserve the exact CAN-used bytes after DOWN_CONTEXT20.
  The codec does not construct a projection from CAN/C2. Environment has three
  independent lanes; sentinel, quality and age are separate and TTL is not
  calculated. Rear raw123 means123mm without integer-cm truncation.
- M_VSS_STATUS keeps the u8 current FaultMask separate from last-fault history.
  Same-K state/fault commit is outside M06. M_WINDOW_STATE/FAULT remain raw Host
  candidates, with execution/TX/display/monitoring HOLD.
- P_VISION_STATUS preserves Pi Boot/status sequence, status, active/last fault,
  status age and affected bits0..1. READY with retained last-fault history is
  representable. No previous P_VISION_RESULT or Permission is updated.

The P0 baseline is source67 SHA, ON101/OFF41. All original CTest names remain;
only `wire.can22_typed_coverage` changes its obsolete UART5/P_STATUS-unimplemented
expectations to UART26/P_STATUS-implemented while retaining CAN22, historical
UART5/seven and unknown-ID checks. A new full48 capability test is separate.
All other baseline test files and original codec/CRC/stream/LE implementations
remain byte-identical. Exact execution results, new per-type positive Golden21,
structural-only references42, independent review, SHA and limitations are in
`_local_review/wire_wave2b_20261017_v1.0/` and the delivery ZIP. Structural zero
and pattern payloads are never counted as semantically valid typed Goldens.

```sh
cmake -S firmware/network -B firmware/network/_local_review/wave2b-build -DNETWORK_TARGET=host -DNETWORK_BUILD_WIRE_CANDIDATE=ON
cmake --build firmware/network/_local_review/wave2b-build --parallel
ctest --test-dir firmware/network/_local_review/wave2b-build --output-on-failure
```

Round4's11 finding IDs/32 locations, G01..G08, product ABI, producer/consumer
agreement, Permission propagation, Query finalTX, VSS, WINDOW and ESP32 ownership
remain OPEN/HOLD. Hardware/Flash/electrical/physical-link tests are NOT_RUN.
