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
Lines starting with `# ` are notes. Every other line is a 144 console command.
Every CAN output line is ASCII, at most 79 characters long and has no leading
or trailing spaces, so the whole output can be pasted into the 144 console.
The 144 does not run note lines (it counts them as `NOT_SUPPORTED`).

Exit codes: 0 = frame built or decoded frame accepted, 1 = decoded frame
rejected (CRC, length or codec), 2 = bad command line or the codec refused
the values (nothing is printed to send).

## CAN: VSS inputs (DOMAIN -> VSS)

```
vss_bench can perm    --boot <hex> --tx <n> [field=value ...]
vss_bench can event   --boot <hex> --tx <n> [field=value ...]
vss_bench can warning --boot <hex> --tx <n> [field=value ...]
```

| Command | 144 message name | Data bytes (used length) | CAN FD frame |
|---|---|---|---|
| `can perm` | `DOMAIN_PERMISSION` (0x080) | 19 | 20 B |
| `can event` | `VSS_EVENT` (0x160) | 23 | 24 B |
| `can warning` | `VSS_WARNING_STATE` (0x0C0) | 60 | 64 B |

- `--boot` is hex (with or without `0x`). `--tx` is decimal or `0x` hex.
  BOOT_ID 0 is reserved and TX_SEQUENCE starts at 1, so both must be non-zero.
- Fields not given keep documented-valid defaults. An unknown field name prints
  the settable list. Values are decimal or `0x` hex.
- The output is one 144 named send: `can tx begin <NAME>`, `can tx data`
  lines (at most 16 bytes each) carrying exactly the used length, then
  `can tx commit` and `can tx status`. The 144 picks the ID, FD+BRS and DLC
  from the name and fills the padding with zeros. It rejects any other data
  length.
- The first 8 data bytes are BOOT_ID and TX_SEQUENCE, little-endian.

Example:

```
$ vss_bench can event --boot 0x42454E43 --tx 1
# VSS_EVENT id=0x160: 23 data bytes; the 144 sends 24 B FD+BRS
# fields: boot_id=0x42454E43 tx_sequence=1 event_kind=1 occurrence_id=1
#   original_age_ms=10 use_limit_ms=500 value_quality=0 input_availability=0
#   related_command_id=0x00000000
can tx begin VSS_EVENT
can tx data 43 4E 45 42 01 00 00 00 01 01 00 00 00 0A 00 F4
can tx data 01 00 00 00 00 00 00
can tx commit
can tx status
```

Use the 144 debug node 2026-10-20 build. It sends registry messages by name
only; a numeric-ID send of 0x080, 0x160, 0x0C0 or 0x340 is rejected.

Checks on the 144:

- `can tx status`: before a block, `lockout=0` and TEC/REC 0; after it, the
  send is complete. `reason=FAULT` means the 144 TX is locked: reset the 144.
  `reason=PROBE` means a `can ping` still owns TX: wait for its end, then
  `can rearm`.
- 0x340 reply: `can select VSS_STATUS`, then `can show` after the next frame
  (all fields of one frame). `vss_bench can decode-status` cross-checks it.

### Scenarios

```
vss_bench can script [--boot <hex>] [--tx-base <n>] S1|S2|S3|S4|all
```

With the default base `n` = 1:

| Scenario | Frames | Expected on the 344 |
|---|---|---|
| S1 | VSS_EVENT TX=n | `event_count`=1, fields equal |
| S2 | S2a: S1 again (exact duplicate, TX=n); S2b: TX=n+1 with `occurrence_id=2` | S2a: no change; S2b: `event_count`=2 |
| S3 | VSS_WARNING_STATE TX=n | `warning_count` +1 |
| S4 | DOMAIN_PERMISSION TX=n | `app.permission_received` +1, then a receive timeout 150 ms later |

- **One BOOT per session.** Every scenario uses the same BOOT_ID, by default
  `0x42454E43` (the 144 bench BOOT). `--boot` changes it for all blocks. After
  a BOOT switch the 344 rejects the old BOOT; to start over, reset the 344.
- **TX continues per message.** TX_SEQUENCE starts at `--tx-base` (default 1)
  for each message. To rerun in the same session, pass a larger base; the
  output header prints the next safe base (`n + 2`).
- S2b uses a new occurrence because a repeat of the same occurrence must
  carry an older original age to be accepted.
- Paste the whole output. Each block is `can tx begin` ... `can tx commit`,
  then `can tx status`.

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
