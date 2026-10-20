# vss_bench

Host CLI for the 344 + 144 bench. It builds CAN and UART-C frames with the
repository wire codecs (`network_wire_candidate`), prints the S32K144 debug
console commands that send them, and decodes frames copied from the 144
console. Every verdict is the codec verdict; the tool adds no policy.

Build (off by default):

```
cmake -S firmware/network -B firmware/network/.build-tools -G Ninja \
  -DBUILD_TESTING=ON -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON \
  -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON -DNETWORK_BUILD_BENCH_TOOL=ON
cmake --build firmware/network/.build-tools
ctest --test-dir firmware/network/.build-tools -R bench_tool
```

The executable is `.build-tools/tools/vss_bench/vss_bench`.
Lines starting with `#` are notes. Every other line is a 144 console command
(each one is shorter than the 80-character console line).

Exit codes: 0 = frame built or decoded frame accepted, 1 = decoded frame
rejected (CRC, length or codec), 2 = bad command line or the codec refused
the values (nothing is printed to send).

## CAN: VSS inputs (DOMAIN -> VSS)

```
vss_bench can perm    --boot <hex> --tx <n> [field=value ...]   # DOMAIN_PERMISSION 0x080, 20 B
vss_bench can event   --boot <hex> --tx <n> [field=value ...]   # VSS_EVENT 0x160, 24 B
vss_bench can warning --boot <hex> --tx <n> [field=value ...]   # VSS_WARNING_STATE 0x0C0, 64 B
```

- `--boot` is hex (with or without `0x`). `--tx` is decimal or `0x` hex.
  BOOT_ID 0 is reserved and TX_SEQUENCE starts at 1, so both must be non-zero.
- Fields not given keep documented-valid defaults. An unknown field name prints
  the settable list. Values are decimal or `0x` hex.
- The frame is the full CAN FD length (20, 24 or 64 B). Bytes after the used
  length are zero.

Example:

```
$ vss_bench can event --boot 0x1A2B3C4D --tx 1
# VSS_EVENT id=0x160 payload=23 B frame=24 B (zero padded)
# NETBENCH: the 144 accepts this arm only after its NETBENCH send extension is approved and flashed; stock firmware rejects registry IDs
# payload: 4D 3C 2B 1A 01 00 00 00 01 01 00 00 00 0A 00 F4 01 00 00 00 00 00 00
# fields: boot_id=0x1A2B3C4D tx_sequence=1 event_kind=1 occurrence_id=1 ...
can tx arm 0x160 NETBENCH
can tx begin 0x160 fd brs 24
can tx data 4D 3C 2B 1A 01 00 00 00 01 01 00 00 00 0A 00 F4
can tx data 01 00 00 00 00 00 00 00
can tx commit
```

The stock 144 debugger refuses manual sends on registry IDs (0x080, 0x160,
0x0C0). `can tx arm <id> NETBENCH` works only on a 144 build that carries the
approved NETBENCH send extension. One arm covers one request, so each bundle
arms again.

### Scenarios

```
vss_bench can script [--boot <hex>] S1|S2|S3|S4|all
```

| Scenario | Frames | Expected on the 344 |
|---|---|---|
| S1 | VSS_EVENT TX=1 | `event_count`=1, fields equal |
| S2 | S1 again (exact duplicate), then TX=2 with `occurrence_id=2` | duplicate: no change; TX=2: `event_count`=2 |
| S3 | VSS_WARNING_STATE TX=1 | `warning_count` +1 |
| S4 | DOMAIN_PERMISSION TX=1 | `app.permission_received` +1, then a receive timeout 150 ms later |

The default script BOOT_ID is `0x444F4D31`. S2b uses a new occurrence because
a repeat of the same occurrence must carry an older original age to be
accepted.

### Decode VSS_STATUS (0x340)

```
vss_bench can decode-status <hex>
```

The command accepts what `can log hex` prints: `data=0403...`, spaced bytes,
or the simple-console form with `00:` offset labels. It takes 28 used bytes or
the 32-byte frame. It prints the BOOT/TX header, then `codec: OK` with the
fields or `codec: INVALID (<status>)`.

```
$ vss_bench can decode-status data=04030201443322110201010000030088776655FFFF00CCBBAA99341200000000
# VSS_STATUS id=0x340 input=32 B frame=32 B
# header: boot_id=0x01020304 tx_sequence=287454020
codec: OK
fields: boot_id=0x01020304 tx_sequence=287454020 vss_state=2 vss_availability=1 ...
```

## UART-C: Pi inputs to the CIS (144 UART0 -> 344 LPUART11)

```
vss_bench uart vision-status --boot <hex> --seq <n> [field=value ...]   # P_VISION_STATUS 0x61, 16 B
vss_bench uart vision-result --boot <hex> --seq <n> [field=value ...]   # P_VISION_RESULT 0x60, 19 B
```

- `--boot` is PI_BOOT_ID. `--seq` is STATUS_SEQUENCE or VISION_SEQUENCE. The
  144 rejects a zero BOOT or a zero SEQ, so the tool refuses them.
- The first output line is a reminder to run `uart profile c1` first. The 144
  rejects a HEX send unless the profile is c1.
- The `uart send` line carries the used payload as one unspaced HEX word
  (32 characters for STATUS, 38 for RESULT). The 144 adds SYNC, VERSION,
  TYPE, LEN, its own LINK_SEQ and the CRC16. A reference frame with LINK_SEQ=1
  is shown as a note.

```
$ vss_bench uart vision-status --boot 50493031 --seq 42
# FIRST on the 144: uart profile c1  (a HEX 'uart send' is rejected unless the profile is c1)
# P_VISION_STATUS type=0x61 payload=16 B frame=26 B
# payload: 31 30 49 50 2A 00 00 00 01 00 00 00 00 14 00 00
# fields: pi_boot_id=0x50493031 status_sequence=42 vision_status=1 ...
# the 144 adds SYNC, VERSION, TYPE, LEN, its own LINK_SEQ and CRC16; reference frame with LINK_SEQ=1:
# A5 5A 01 61 10 00 01 00 31 30 49 50 2A 00 00 00 01 00 00 00 00 14 00 00 D2 E8
uart send P_VISION_STATUS 313049502A0000000100000000140000
```

### Decode

```
vss_bench uart decode <hex>
```

- **Input:** a whole frame (`A5 5A ...` including the CRC) copied from the 144
  log (`hex=A5 5A ...`, spaced or packed), or a bare payload. A bare payload's
  TYPE comes from its length: 19 = P_VISION_RESULT, 16 = P_VISION_STATUS,
  15 = P_PERMISSION.
- **Frame output:** the header, `crc: OK` or `crc: BAD stored=.. computed=..`,
  then `codec: OK <name>` with the fields or `codec: INVALID <name> (<status>)`.
- **CRC:** CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection). It covers
  VERSION through the payload end and excludes SYNC. The check value of ASCII
  `123456789` is 0x29B1 (`docs/network/interface/03_COMMUNICATION/20_UART_M.md`).
  This is the same computation as the 144 `DebugFrame_Crc16`.
