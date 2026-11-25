# Network dependency and local integration delta

Upstream source baseline: `vehicle-domain-control-system/integrated-vehicle-control`
`feat/vss-network-node@0cc4ae0`. Its canonical source tree is `firmware/network`.
Production uses the DOMAIN bundle produced by `tools/sync_to_s32ds.py`: a
canonical `nodes/domain/<path>` becomes `<path>` inside `network/` here.
`NETWORK_BUNDLE.txt` is historical generator provenance, not a claim that the
locally modified bundle currently passes an upstream generator equality check.

The common product files not listed below match that baseline after CRLF/LF
normalization. Existing 388269e CIS invalid encoding, independent warning lanes,
5ms late AGE handling and 0cc4ae0 AGE saturation are retained.

| Canonical source | Local integration responsibility |
|---|---|
| `nodes/domain/board/s32k344_domain/net_domain_board.c` | Clock/IRQ callbacks, LPUART11, status quality, query plan and identity hooks |
| `nodes/domain/src/node/net_node_domain.c` | Private Monitoring Owner composition and final submit guard |
| `nodes/domain/src/l02_typed_handoff/m03_typed_handoff/net_domain_owner_binding.c` | Exact receipt/commit evidence, Runtime publishers and dispatch authority |
| `nodes/domain/src/l02_typed_handoff/m03_typed_handoff/net_domain_rx.c` | Private commit capture around real C2 RX |
| `nodes/domain/src/l02_typed_handoff/m03_typed_handoff/net_domain_snapshot_private.h` | Private receipt/dispatch declarations |
| `nodes/domain/src/l02_typed_handoff/m03_typed_handoff/net_domain_tx.c` | Domain boundary integration; original C2/wire retained |
| `nodes/domain/include/network/app/net_uart_alive_diagnostic.h` (new) | Diagnostic-only API |
| `nodes/domain/src/platform/rtd/net_uart_alive_diagnostic.c` (new) | UART-only diagnostic Init/Step/Stop |
| `include/network/app/net_if_entry.h` | Additive Hardware Init Status query; shared review required |
| `src/node/net_entry.c` | Ready status integration; shared review required |
| `src/platform/rtd/net_hal_rtd.c` | Rollback and Production/Diagnostic Owner arbitration; shared review required |
| `src/platform/rtd/net_hal_rtd_internal.h` | Internal ownership/cleanup state; shared review required |
| `src/platform/rtd/net_hal_rtd_can.c` | Existing approved Stop/Deinit rollback; shared review required |
| `src/platform/rtd/net_hal_rtd_uart.c` | LPUART11 callbacks, TX-only diagnostic cleanup and completion handling; shared review required |

Do not overwrite a newer Network branch with this bundle. A team PR must apply
only these deltas to the canonical paths and retain unrelated upstream changes.
The current team develop snapshot has no `firmware/network` tree, so its Network
dependency integration must be explicit. No team develop merge is authorized by
this source-preservation operation.

`integration/network/production-local-delta.patch` and `files.csv` preserve the
14-file canonical review delta separately. They are not an automatic shared
Network update: upstream build/source-list integration and cross-profile checks
remain Network review work. The standalone DOMAIN bundle is the validated build
input; the patch is review/porting material only.
