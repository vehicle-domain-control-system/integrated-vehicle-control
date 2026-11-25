# Phase 5-M-33F — Mobile Production integration contract

This document records only contracts demonstrated by the inspected code. It
does not enable Production routing, change UART-M v2, or select unresolved
timeouts.

## Inspected baselines

- ESP32 `main`: `ecb1f493d86c615250ad24373489977d7da6c1f3`.
- ESP32 33E worktree: `feature/phase5m33c-mobile-interface`, based on
  `6e1ccf0eb3aa8b2ecb5d6169fa63c169d1a959d4`, with the uncommitted 33E UART
  queue integration. The later `main` Query-runtime changes are not in that
  worktree and must be reconciled before a Production merge.
- Network: `team/feat/vss-network-node@0cc4ae071ccb0562fd488e0c159703cb19a61bec`.
- Domain Production: the inspected S32DS project contains the 27B admission
  inbox and 33D private bridge, but neither is installed by the default
  bootstrap.

## Confirmed contract

### P6 and UART-M

- `p6_session_manager` is the ESP32 P6 Snapshot producer. GatewayTask is its
  serialized consumer/owner and requires registered, bonded, encrypted,
  reconciled identity, current connection generation, nonzero Session ID,
  current Domain Boot ID, and no reset/storage fault.
- `P6_PRODUCTION_BINDING_ENABLED` and
  `GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED` are both `0`.
- `M_CONTEXT` carries Device Context ID, Session ID, state values, and
  independent Quality/Age/UpdateSequence metadata. It does **not** carry the
  P6 Snapshot or cryptographic proof. Registration/connection values on the
  wire therefore cannot by themselves set Domain
  `p6AuthorizationVerified=true`.
- ESP32 33E copies a request into a four-entry admission queue, rechecks P6,
  BLE connection generation/epoch, Domain link/Boot ID, and request TTL
  immediately before UART queue admission, and submits `M_CONTEXT` before
  `M_REQUEST`. UART transport assigns LinkSequence and copies the typed DTO.
- UART queue `OK` is local admission only. It is not Domain acceptance or
  vehicle completion.

### Time and identity ownership

| Value | Owner and permitted meaning |
|---|---|
| Device Context / Session / Request ID | ESP32/MOBILE values preserved by UART-M. Request ID remains the application request identity. |
| LinkSequence | UART transport framing order, modulo 16 bit. Network may use it for same-link duplicate/order checks; it is not SourceSequence. |
| RX timestamp | Network HAL capture time (`captured_at_ms`, `timestamp_known`) on the Network clock. It is transport admission evidence, not MOBILE creation time. |
| PortEpoch | Network per-link 64-bit epoch. Nonzero; changes only on a deliberate link reset. It is not synthesized from LinkSequence. |
| Network Boot ID | Network node/board identity returned by `Net_GetBootId()`. Existing policy is retained. Its current board seed has a documented power-cycle reuse risk. |
| ContextRevision | Domain private bridge counter. It increments, skips zero, and changes when the authoritative security tuple changes. It is not serialized. |

### Domain admission

- Network decodes `M_REQUEST` with unavailable source metadata:
  `NO_DATA`, `UINT32_MAX`, sequence `0`. LinkSequence and RX timestamp must not
  be substituted into `SignalMeta_t`.
- The private Domain bridge copies the decoded DTO and transport evidence in
  one serialized Owner call. Runtime Poll performs the authoritative admission
  check later.
- Admission requires matching Device/Session, PortEpoch, ContextRevision,
  Network Boot ID, current P6 authorization, known mapped timestamps, fresh
  context/evidence, and a newer LinkSequence.
- Link down, PortEpoch change, Network Boot change, security-context change,
  or authorization revocation invalidates the context and discards pending
  inbox entries. Rejected entries are not replayed automatically.

## TTL and refresh decision

The code exposes three injected values and keeps all zero values fail-closed:

- registration/connection source TTL;
- App source TTL;
- request candidate TTL.

No numeric Production value is currently justified:

- Registration/connection observation is refreshed by an accepted BLE
  security event; App observation is refreshed by an accepted App-state event.
  GatewayTask's 100 ms supervisor tick only recalculates age and does not
  create a new source observation.
- The interface reference lists a 100 ms `M_CONTEXT` period and 300 ms receive
  timeout, but 33E currently emits context only while a request is pending and
  suppresses another context when only Age/UpdateSequence changed.
- `M_REQUEST` has no MOBILE creation timestamp. Network RX time can bound local
  residence only, not end-to-end request age.

Consequently the safe Production values remain `0` until the team chooses one
of these existing-semantics-compatible policies:

1. define authoritative periodic source observations and a bounded
   `M_CONTEXT` cadence, then choose each TTL greater than the measured maximum
   observation-to-transmit interval plus scheduling/transport margin; or
2. explicitly define security/App state as event-latched validity and specify
   which lifecycle events revoke it, without representing periodic freshness
   that the producer does not supply.

The request TTL also needs a measured MOBILE-to-Gateway service bound. It must
not be inferred from Network RX time or the historical 3 s requirement.

## Production connection order

1. GatewayTask obtains a current authoritative P6 Snapshot and current
   registration/App source observations.
2. Nonzero, approved TTL policy is injected; otherwise configuration remains
   disabled.
3. After reconciling 33E with current ESP32 `main`, the lifecycle owner binds
   the UART transport to GatewayTask. No second UART sender is permitted.
4. GatewayTask queues current `M_CONTEXT`, then revalidates and queues the
   matching `M_REQUEST`. Queue failure consumes the request fail-closed.
5. Network validates UART-M framing/CRC/order, owns LinkSequence, PortEpoch and
   RX capture time, and invokes the DOMAIN Owner serially.
6. The Domain private bridge processes the accepted `M_CONTEXT` before the
   matching request, copies DTO/evidence, and queues it to the bounded inbox.
7. Domain Runtime Poll revalidates context and evidence. Only then may the
   existing Request Coordinator evaluate policy. Local handoff remains
   distinct from ECU completion.
8. Existing `M_RESULT` encoding and ESP32 BLE Notify path returns the correlated
   result; it must not convert UART/Network acceptance into vehicle success.

## Decisions still required

Only these decisions block Production connection:

1. **P6 trust boundary:** define an authoritative cross-ECU source for
   Domain `p6AuthorizationVerified`. No such proof exists in UART-M v2 or the
   current Network delivery metadata. Without it, the private context provider
   must return false.
2. **Freshness policy:** choose periodic observations versus event-latched
   validity, then approve the three TTLs and `M_CONTEXT` refresh rule.
3. **Lifecycle hooks:** designate the Network owner that calls Domain bridge
   invalidation for link down, PortEpoch change and Network Boot change.
4. **Boot identity risk:** accept or resolve the documented Network Boot ID
   reuse risk before using Boot ID as replay separation across power cycles.
5. **Integration baseline:** reconcile the 33E worktree with ESP32 current
   `main` Query-runtime changes before enabling its binding.

## Current executable scope

Host E2E may exercise P6 Snapshot injection, context/request queue ordering,
transport evidence copy, invalidation, duplicate/replay rejection, Runtime
Poll, `M_RESULT` encoding and BLE Notify. Production Owner binding, physical
UART, BCM command transmission, and vehicle completion remain disabled and
require board-level verification.
