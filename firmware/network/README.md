# Portable Network product stack

The current product entrypoint is `NetUpper` in `include/network/net_upper.h`.
It executes the 48 CAN FD / UART-M / UART-C typed codecs through a bounded product
queue, deferred receive service, typed ECU adapters, query controller and an
injected Port. The preserved `network_common` / `NetRuntime` scaffold is a
separate historical test target; it is not the product service.

The five Layers and nine Modules remain the ownership structure. There are 25
design responsibilities, not a requirement to invent 25 wrapper functions.
`SOURCE_MAPPING.md` and `IMPLEMENTATION_MATRIX.csv` describe the current source.
Exact executed test results, source SHA, detailed design declarations and HOLD
status belong to the result package; this file does not infer PASS from a build.

## Build the portable product on Host

```sh
cmake -S firmware/network -B build-network -DNETWORK_TARGET=host -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON
cmake --build build-network --parallel
ctest --test-dir build-network --output-on-failure
```

Select a C11 compiler/generator appropriate to the host. The wire selection
`NET_WIRE_CANDIDATE_ONLY` remains an explicit local profile; passing Host tests
is not approval of external contracts. `NETWORK_NODE_PROFILE` supports HOST,
CIS, BCM, WINDOW, DOMAIN and VSS with profile-owned storage. Unknown vehicle
application/RTD/stack/heap RAM is UNKNOWN, never inferred to fit from Network size.

`NETWORK_TARGET=s32k144` or `s32k344` with `BUILD_TESTING=OFF` builds the same
portable C libraries and SDK-free adapter bridge. It does not configure RTD,
pins, clocks, IRQ, DMA, a linker script or an ARM compiler. Explicit
`NETWORK_BUILD_BOARD_HAL=ON` fails because no board backend is supplied here.
The older `net_s32k144_port.c` / `net_s32k344_port.c` scaffold functions remain
fail-closed; actual product injection uses `net_*_upper_port.c` and `NetHalAdapter`.

## Select the RTD node MCU

`NETWORK_NODE_PROFILE` selects the ECU role; it does not select its MCU.
The shared `NetNode_Init` uses `NETWORK_NODE_PLATFORM=144` or `344`
(`src/platform/rtd/net_board_cfg.h`) to initialize the same `NetHalAdapter`.
In S32DS, an existing `CPU_S32K144HFT0VLLT` or `CPU_S32K344` compiler define
supplies that selection if no explicit value is given. Missing, unknown or
conflicting selections are compile errors. Other CPU part defines require an
explicit `NETWORK_NODE_PLATFORM` value.

Host RTD mocks require the explicit CMake option, for example:

```sh
cmake -S firmware/network -B build-network-cis -DNETWORK_TARGET=host -DBUILD_TESTING=ON -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON -DNETWORK_NODE_PROFILE=CIS -DNETWORK_BUILD_RTD_MOCK=ON -DNETWORK_NODE_PLATFORM=144
```

Use 344 for the existing VSS board, 144 for the CIS S32K144 RTD board integration,
and choose BCM from its actual board project (the current BCM design specifies
144; its board project is not present here). The mock SDK keeps its synthetic
peripheral configuration; selecting 144 verifies adapter identity and product
paths, not physical S32K144 pins, RTD headers or resources. `NETWORK_TARGET` alone
still selects only the portable libraries described above.

`tools/s32ds_sources.txt` is shared by both MCU choices and the mock build.
The node calls `net_hal_adapter_init` directly with the selected identity, so
this list no longer needs a fixed S32K344 wrapper. Standalone 144/344 upper-port
wrapper APIs remain available in the portable adapter library. The copy tool
continues to exclude legacy S32K144 scaffold sources.

## Product integration

- `net_upper_init`, `net_upper_capture`, `net_upper_step`: fresh caller-owned
  storage, stable event copies and one serialized superloop.
- `NetEcuAdapter`: 48 typed publish functions and typed receive callbacks; a
  missing callback fails closed. No vehicle decision algorithm is synthesized.
- `net_upper_cis_publish_rear`, `net_upper_cis_publish_occupant`, `net_upper_cis_publish_pi_permission`:
  product relays retain accepted receipt, original identity and source age.
- `NetUpperQuery`: context validation, bounded getter progression, raw provenance,
  real local TX success counting, Query End and cancellation.
- `NetPermissionGuard` and `NetRouteWatch`: explicitly selected original-age and
  receive-silence monitoring. The view retains the original raw DTO and reason;
  timeout never fabricates a DENY/ALLOW DTO. Schedule their step/read functions
  in the same serialized superloop. Event/Result silence is not periodic liveness.
- `NetDomainC2`: optional actual pinned C2 header binding, TxPort registration,
  typed conversions, Owner RX/getters, local failures and commit-correlation checks.
  Enable `NETWORK_BUILD_DOMAIN_C2=ON` and provide
  `NETWORK_DOMAIN_C2_INCLUDE_DIR`. Missing actual Owner bodies or semantic policy
  hooks hold the affected mapping; no replacement public DOMAIN API is declared.
- `NetVirtualBackend`: executable Host driver replacement. Enable
  `NETWORK_BUILD_VIRTUAL_ECU=ON` with the Port adapter to run the selected ECU
  worker. `tests/virtual_ecu/network_virtual_demo.py` connects prepared profile
  workers through actual CAN envelopes / UART bytes. Owner fakes are outside the
  product service; the product queue, codecs, routers and adapters execute.
  `NETWORK_BUILD_VIRTUAL_NODE=ON` builds the HAL-less node worker instead: the
  node profile table (`src/node/net_node_profile_<node>.c`) and App Port facade
  of VSS / CIS / BCM on this backend, plus a HOST peer (DOMAIN on CAN,
  the Pi on UART-C). `tools/virtual_nodes_run.py --build-dir <dir>` builds all
  four, checks that no include path reaches the mock SDK, `src/platform/rtd` or
  an NXP SDK, and runs facade round trips (`RESULT passed=N failed=M`).
- SDK swap: `tools/sdk_swap_check.py` compiles `src/platform/rtd` (and
  `net_node.c` / `net_entry.c`) with ARM GCC against the real RTD headers of an S32K344 and an
  S32K144 S32DS project, reading the board names from their generated
  configuration (only differing names become `net_board_cfg.h` -D overrides).
  The projects are only read; the report goes to `--out/summary.md`.
- Bench helper: `NETWORK_BUILD_BENCH_TOOL=ON` builds the host `vss_bench` CLI
  (`tools/vss_bench/README.md`). It builds CAN / UART-C frames with the wire
  codecs, prints the S32K144 debug-console commands that send them and decodes
  frames copied from that console.

Ordinary WINDOW routes remain HOLD. Explicit `inspection_transport` plus the
separate inspection-submit API may carry held wire data only to `held_can` /
`held_uart` observation callbacks. It never authorizes ordinary Owner delivery,
C2 vehicle-state commitment, WINDOW liveness or actuator execution.

## Ownership, timing and recovery

All APIs require serialized access by one owner. `in_call` rejects synchronous
reentry; it is not an ISR/thread lock. Do not copy/reinitialize a live instance,
mutate service internals or alias output storage with live internal objects.
The Port has independent submit, admission, final-result and reference-release
facts. Success at any one boundary is not peer reception or vehicle execution.
No reset, timeout or cancellation frees an outstanding driver borrow.

Wire transport identity and original observation identity are separate. Same
transport identity with different raw bytes is a conflict. A newer transport
sequence with the same original per-field identity may increase AGE but cannot
change that field's value/quality. Valid repeated observations refresh receipt
metadata without repeated Owner delivery, except the explicitly designed new
UART-C LINK observation of an unchanged Vision evaluation and the periodic routes
a node profile opts in (`rx_repeat_delivery`: VSS/CIS/BCM receive callbacks that
feed a receive supervision), where the repetition reaches the Owner with the
original receipt (`NetUpperDelivery.repeat`). Result progression and independent lanes
remain independent. A sender BOOT other than the accepted one starts a new
generation per CAN ID (UART-C: per receive direction) unless it is one of the
4 BOOTs that key ended; a late frame of an ended BOOT is refused.

Source ages use an explicit monotonic clock and immutable capture provenance.
Backward/unknown time, arithmetic overflow, unresolved identity wrap or missing
policy holds the affected operation. Permission original TTL, RX silence and
command/event use limits are separate. Query may count only actual final local
TX success; this is not an acknowledgement from a remote application.

## Historical evidence and unresolved integration

`WIRE_CANDIDATE_STATUS.md`, `WIRE_WAVE2A_STATUS.md`, `WIRE_WAVE2B_STATUS.md`, and
`BOARD_BINDING_STATUS.md` are historical records of their named waves. Their
7-codec / 27-codec / 48-codec counts, scaffold-only statements and old test totals
must not be read as the current implementation inventory.

G01-G08, WINDOW execution, external Owner policy/ABI adoption, BOOT_ID source,
CAN TX_SEQUENCE wrap, real driver/RTD timing and release semantics, target whole-ECU RAM/stack/heap,
ARM builds, Flash and physical bus tests remain explicitly HOLD/UNKNOWN/NOT_RUN
as applicable. The HAL-independent code runs with Host backends; no Host success
is promoted to a physical-board or whole-ECU fitness claim.
