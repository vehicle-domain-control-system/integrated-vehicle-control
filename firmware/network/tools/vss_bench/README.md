# vss_bench

344 + 144 시험대용 Host CLI다. 저장소의 wire 코덱(`network_wire_candidate`)으로 CAN과 UART-C 프레임을 만들고, 그것을 보내는 S32K144 디버그 콘솔 명령을 출력하며, 144 콘솔에서 복사한 프레임을 해독한다. 판정은 모두 코덱의 판정이다. 도구가 정책을 더하지 않는다.

빌드(기본은 꺼짐):

```
cmake -S firmware/network -B firmware/network/.build-tools -G Ninja \
  -DBUILD_TESTING=ON -DNETWORK_BUILD_WIRE_CANDIDATE=ON -DNETWORK_BUILD_UPPER_STACK=ON \
  -DNETWORK_BUILD_HAL_PORT_ADAPTER=ON -DNETWORK_BUILD_BENCH_TOOL=ON
cmake --build firmware/network/.build-tools
ctest --test-dir firmware/network/.build-tools -R bench_tool
```

실행 파일은 `.build-tools/tools/vss_bench/vss_bench`다. `# `로 시작하는 줄은 설명이고, 나머지 줄은 모두 144 콘솔 명령이다. CAN 출력 줄은 모두 ASCII, 79자 이하이고 앞뒤 공백이 없어서 출력 전체를 144 콘솔에 붙여 넣을 수 있다. 144는 설명 줄을 실행하지 않는다(`NOT_SUPPORTED`로 센다).

종료 코드: 0 = 프레임을 만들었거나 해독한 프레임을 받아들임, 1 = 해독한 프레임을 거부함(CRC, 길이, 코덱), 2 = 명령줄 오류이거나 코덱이 값을 거부함(보낼 것을 출력하지 않음).

## CAN: VSS 입력 (DOMAIN -> VSS)

```
vss_bench can perm    --boot <hex> --tx <n> [field=value ...]
vss_bench can event   --boot <hex> --tx <n> [field=value ...]
vss_bench can warning --boot <hex> --tx <n> [field=value ...]
```

| 명령 | 144 메시지 이름 | 데이터 바이트(사용 길이) | CAN FD 프레임 |
|---|---|---|---|
| `can perm` | `DOMAIN_PERMISSION` (0x080) | 19 | 20 B |
| `can event` | `VSS_EVENT` (0x160) | 23 | 24 B |
| `can warning` | `VSS_WARNING_STATE` (0x0C0) | 60 | 64 B |

- `--boot`는 16진수다(`0x`를 붙여도 되고 안 붙여도 된다). `--tx`는 10진수 또는 `0x` 16진수다. BOOT_ID 0은 예약값이고 TX_SEQUENCE는 1부터 시작하므로 둘 다 0이 아니어야 한다.
- 주지 않은 필드는 문서상 유효한 기본값을 쓴다. 모르는 필드 이름을 주면 설정할 수 있는 목록을 출력한다. 값은 10진수 또는 `0x` 16진수다.
- 출력은 144의 이름 송신 한 건이다: `can tx begin <NAME>`, 사용 길이만큼 정확히 담은 `can tx data` 줄들(줄마다 최대 16바이트), 그다음 `can tx commit`과 `can tx status`. 144는 이름으로 ID, FD+BRS, DLC를 고르고 남는 칸은 0으로 채운다. 다른 데이터 길이는 거부한다.
- 앞 8바이트는 BOOT_ID와 TX_SEQUENCE이고 little-endian이다.

예:

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

144 디버그 노드 2026-10-20판을 쓴다. 이 판은 registry 메시지를 이름으로만 보낸다. 0x080, 0x160, 0x0C0, 0x340을 숫자 ID로 보내면 거부한다.

144에서 확인할 것:

- `can tx status`: 블록을 보내기 전에는 `lockout=0`이고 TEC/REC가 0이다. 보낸 뒤에는 송신이 끝난 상태다. `reason=FAULT`는 144 TX가 잠긴 것이니 144를 리셋한다. `reason=PROBE`는 `can ping`이 아직 TX를 쓰고 있는 것이니 끝날 때까지 기다린 뒤 `can rearm`한다.
- 0x340 응답: `can select VSS_STATUS`를 한 뒤, 다음 프레임이 오면 `can show`를 한다(프레임 하나의 모든 필드). `vss_bench can decode-status`로 대조한다.

### 시나리오

```
vss_bench can script [--boot <hex>] [--tx-base <n>] S1|S2|S3|S4|all
```

기본 base `n` = 1일 때:

| 시나리오 | 프레임 | 344에서 기대하는 것 |
|---|---|---|
| S1 | VSS_EVENT TX=n | `event_count`=1, 필드 일치 |
| S2 | S2a: S1을 다시(정확한 중복, TX=n), S2b: TX=n+1, `occurrence_id=2` | S2a: 변화 없음, S2b: `event_count`=2 |
| S3 | VSS_WARNING_STATE TX=n | `warning_count` +1 |
| S4 | DOMAIN_PERMISSION TX=n | `app.permission_received` +1, 150 ms 뒤 수신 timeout |

- **세션 하나에 BOOT 하나.** 모든 시나리오가 같은 BOOT_ID를 쓴다. 기본값은 `0x42454E43`(144 시험대 BOOT)이다. `--boot`는 모든 블록의 값을 바꾼다. BOOT를 바꾸면 344는 옛 BOOT를 거부한다. 처음부터 다시 하려면 344를 리셋한다.
- **TX는 메시지마다 이어진다.** TX_SEQUENCE는 메시지마다 `--tx-base`(기본 1)에서 시작한다. 같은 세션에서 다시 돌리려면 더 큰 base를 준다. 출력 머리에 다음에 써도 되는 base(`n + 2`)가 나온다.
- S2b는 새 occurrence를 쓴다. 같은 occurrence의 반복은 더 오래된 원 AGE를 담아야 받아들여지기 때문이다.
- 출력 전체를 붙여 넣는다. 블록마다 `can tx begin` ... `can tx commit`, 그다음 `can tx status`다.

### VSS_STATUS(0x340) 해독

```
vss_bench can decode-status <hex>
```

`can log hex`가 출력하는 형태를 받는다: `data=0403...`, 공백으로 띄운 바이트, `00:` 오프셋 표시가 붙은 단순 콘솔 형식. 사용 바이트 28개나 32바이트 프레임을 받는다. BOOT/TX 머리를 출력한 뒤, 필드와 함께 `codec: OK`를 출력하거나 `codec: INVALID (<status>)`를 출력한다.

```
$ vss_bench can decode-status data=04030201443322110201010000030088776655FFFF00CCBBAA99341200000000
# VSS_STATUS id=0x340 input=32 B frame=32 B
# header: boot_id=0x01020304 tx_sequence=287454020
codec: OK
fields: boot_id=0x01020304 tx_sequence=287454020 vss_state=2 vss_availability=1 ...
```

## UART-C: CIS로 가는 Pi 입력 (144 UART0 -> 344 LPUART11)

```
vss_bench uart vision-status --boot <hex> --seq <n> [field=value ...]   # P_VISION_STATUS 0x61, 16 B
vss_bench uart vision-result --boot <hex> --seq <n> [field=value ...]   # P_VISION_RESULT 0x60, 19 B
```

- `--boot`는 PI_BOOT_ID, `--seq`는 STATUS_SEQUENCE 또는 VISION_SEQUENCE다. 144는 BOOT나 SEQ가 0이면 거부하므로 도구도 거부한다.
- 출력 첫 줄은 먼저 `uart profile c1`을 실행하라는 안내다. 144는 프로필이 c1이 아니면 HEX 송신을 거부한다.
- `uart send` 줄은 사용 payload를 띄어 쓰지 않은 HEX 한 단어로 담는다(STATUS 32자, RESULT 38자). 144가 SYNC, VERSION, TYPE, LEN, 자기 LINK_SEQ, CRC16을 붙인다. LINK_SEQ=1인 참고 프레임을 설명 줄로 보여 준다.

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

### 해독

```
vss_bench uart decode <hex>
```

- **입력:** 144 로그에서 복사한 프레임 전체(`A5 5A ...`, CRC 포함, `hex=A5 5A ...`, 띄어 쓰거나 붙여 씀) 또는 payload만. payload만 주면 길이로 TYPE을 정한다: 19 = P_VISION_RESULT, 16 = P_VISION_STATUS, 15 = P_PERMISSION.
- **프레임 출력:** 머리, `crc: OK` 또는 `crc: BAD stored=.. computed=..`, 그다음 필드와 함께 `codec: OK <name>` 또는 `codec: INVALID <name> (<status>)`.
- **CRC:** CCITT-FALSE(poly 0x1021, init 0xFFFF, 반사 없음). VERSION부터 payload 끝까지 계산하고 SYNC는 뺀다. ASCII `123456789`의 검사값은 0x29B1이다(`docs/network/interface/03_COMMUNICATION/20_UART_M.md`). 144 `DebugFrame_Crc16`과 같은 계산이다.
