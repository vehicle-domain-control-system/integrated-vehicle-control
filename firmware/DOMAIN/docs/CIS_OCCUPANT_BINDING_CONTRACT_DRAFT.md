# CIS Occupant Binding Contract Draft

## Scope

This draft documents the Production C2 v0.5 CIS occupant state owner, Rx/Query binding, and private BOOT transition boundary. It does not change the public C2 headers or the CAN/UART wire contract.

## Domain-owned state

- `CisOccupantManager` owns a bounded, typed copy of `CisOccupantState_t`.
- Presence and count quality, source age, and local receive time are evaluated independently.
- TTL values are supplied through `DomainRuntime_Config_t`; zero is invalid and does not mean unlimited lifetime.
- Query is read-only. It returns an effective snapshot and never refreshes source age or sequence.

## Same observation rules

- An entirely identical Generation/Sequence observation is an idempotent no-op.
- With unchanged values and `visionFunctionStatus`, `OK` may be withdrawn independently to `INVALID`, `STALE`, or `NO_DATA`.
- A withdrawal preserves the original observation timeline; it cannot rejuvenate source AGE.
- Recovery from a non-usable quality to `OK` with the same Sequence is rejected.
- A value or `visionFunctionStatus` change with the same Sequence is rejected as a conflict.
- No total ordering is invented among `INVALID`, `STALE`, and `NO_DATA`.

## Private BOOT transition

`DomainCisOccupantPriv_ApplyBootTransition()` is a Domain-private synchronous invalidation barrier. It accepts only a Network-validated producer `sourceGeneration` and transition Epoch. Epoch 1 is the initial transition; later successful transitions must use the immediately following Epoch. Duplicate Epoch/Generation requests are idempotent only after completion.

The public `DomainIf_RxCisOccupant()` gate remains closed until a successful private transition. A transition invalidates the effective state atomically before admitting the declared generation. Previous diagnostic payloads may remain internally retained, but Query cannot expose them as usable state.

## BINDING_OPEN — Network responsibilities

The following are not completed by this pure Domain change:

- owner and call point of the private BOOT transition;
- mapping between CIS transport `BOOT_ID` and Pi producer `sourceGeneration`;
- drain/cancellation of callbacks and queued frames from the previous BOOT;
- admission-gate control and checking the Domain receipt before reopening ingress;
- Epoch recovery when Network restarts;
- delayed-callback rejection when a Generation value is reused;
- keeping Occupant Rx ingress closed before initial admission.

The Domain does not infer BOOT from a public `NO_DATA` sample and does not order generations numerically. Network must not deliver the new BOOT until the private barrier succeeds, and must keep ingress closed after a failed transition.

## Runtime and BoardRuntime compatibility

This change adds Occupant policy and storage to `DomainRuntime_Config_t` and `DomainRuntime_t`. All sources embedding these structures must be rebuilt together; mixing old BoardRuntime objects with the new Runtime object is unsupported.

Team PR #87 currently does not provide nonzero Presence/Count TTL values. Its integration is therefore **BLOCKED** until the TTL policy and explicit configuration are agreed. This change does not modify PR #87.

## Verification boundary

Host tests cover quality withdrawal, conflict rejection, independent timing, Epoch transitions, fail-closed behavior, TTL validation, and Runtime readiness. ARM compilation/linking verifies source and symbol consistency. S32DS and physical-board behavior remain separate gates.
