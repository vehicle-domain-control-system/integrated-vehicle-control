# esp32_bench

DOMAIN 보드 시험에서 노트북이 ESP32를 맡는 Python 도구다. DOMAIN 보드의 UART-M(S32K344 평가 보드에서는 LPUART0, USB 가상 COM 포트)으로 M_CONTEXT를 주기적으로 보내고, 정한 시각에 M_REQUEST·M_QUERY·M_WARNING_ACK·세션 변경·망가뜨린 프레임을 보낸다. DOMAIN이 보낸 프레임은 모두 해석하고 검사해 끝에 판정을 낸다.

프레임 외형과 필드는 `docs/network/interface/03_COMMUNICATION/20_UART_M.md`와 저장소 코덱 구조체(`net_wire_uart_dto.h`, `net_wire_uart_m_core.h`, `net_wire_uart_m_projection.h`)를 따른다. host 시험 `bench_tool.esp32_*`(`tests/host/test_esp32_bench.py`)가 UART-M 메시지 23종 모두를 `tools/vss_bench`의 `uartm encode`·`uartm decode`(저장소 코덱)와 양방향으로 대조한다.

## 지금 DOMAIN 보드에서 볼 수 있는 것

- 보드 hook(`net_domain_c2_pinned_owner_api`)에는 M_CONTEXT를 받을 DOMAIN 함수(`DomainIf_RxDigitalKeyConnection`, `DomainIf_RxProximity`)가 연결되어 있지 않다. 그래서 DOMAIN Network는 M_CONTEXT를 받지 않고(HOLD), 문맥이 필요한 다른 상향 프레임도 HOLD다. 하향으로는 M_DOMAIN_ALIVE(250 ms)만 나간다. `alive_state`가 비어 있어 값은 INIT / NO_DATA / UPDATE_SEQUENCE 0 / AGE 65535다.
- 이 상태에서 판정되는 것은 선로(프레임 오류), LINK_SEQUENCE, M_DOMAIN_ALIVE다. 문맥 수용, 요청 결과, 조회 응답은 `HOLD`로 표시하고 실패로 보지 않는다.
- DOMAIN이 위 함수들을 연결하면 같은 명령으로 문맥 수용(`TAKEN`), M_RESULT(`ANSWERED`), M_QUERY_END(`ENDED`)를 본다. 상태 중계와 조회 응답이 나가려면 스냅숏 확인과 Query 계획 콜백도 연결되어야 한다(`nodes/domain/DOMAIN_NODE_INTEGRATION.md` "의미 콜백 5개").
- 상향 프레임이 DOMAIN 안에서 어떻게 처리됐는지(HOLD, 망가뜨린 프레임)는 도구가 볼 수 없다. 디버거로 `NetNodeDiag.upper.consumer_rejected`와 Network 서비스의 UART-M 수신 스트림 카운터(`NetUpperUartReceiver.stream`: `bad_crc`, `bad_headers`, `assembly_timeouts`, `discarded_bytes`)를 본다.

## 준비

- Python 3.8 이상. `frame`, `decode`는 표준 라이브러리만 쓴다.
- 직렬 포트를 쓰는 `ports`, `send`, `run`은 pyserial이 필요하다: `python -m pip install pyserial`
- 포트 하나는 프로그램 하나만 연다. 다른 터미널이 같은 COM 포트를 열고 있으면 닫는다.

## 명령

```
python firmware/network/tools/esp32_bench/esp32_bench.py ports
python firmware/network/tools/esp32_bench/esp32_bench.py frame context|request|query|ack [--device N] [--session N] [--id N] [--link N] [--bad KIND] [field=value ...]
python firmware/network/tools/esp32_bench/esp32_bench.py decode HEX ...
python firmware/network/tools/esp32_bench/esp32_bench.py send --port COM7 context|request|query|ack [--state FILE] [--new-session] [--bad KIND] [--listen S] [--log FILE] [field=value ...]
python firmware/network/tools/esp32_bench/esp32_bench.py run --port COM7 --duration S [--state FILE] [--new-session] [--context-every MS] [--context-mode new|repeat] [--session-change-at S ...] [--request KIND:OP[:ARG ...]@S ...] [--query SCOPE@S ...] [--ack TYPE:OCCURRENCE[:BOOT]@S ...] [--inject KIND@S ...] [--log FILE] [field=value ...]
```

| 명령 | 하는 일 |
|---|---|
| `ports` | 직렬 포트 목록 |
| `frame` | 상향 프레임 하나를 16진수로 출력한다(포트 없음). 필드와 payload도 설명 줄로 나온다 |
| `decode` | 16진수 바이트를 해석한다(포트 없음). 23종 모두, 여러 프레임, 잡음, 잘린 프레임을 받는다 |
| `send` | 상향 프레임 하나를 보내고 `--listen` 초(기본 1) 동안 받은 것을 출력한다 |
| `run` | `--duration` 초 동안 M_CONTEXT를 `--context-every` ms(기본 100, 0이면 안 보냄)마다 보내고, 시나리오 시각에 나머지를 보낸다. 끝에 판정을 낸다 |

- 기본값(문서상 유효한 값): M_CONTEXT는 등록됨·BT 연결·앱 활성·근접 FAR(VALID)·품질 모두 OK·AGE 20 ms. M_REQUEST는 도어 LOCK(1:0). M_QUERY는 현재 상태(scope 0). M_WARNING_ACK는 경고 종류 1, 발생 1이다. `field=value`로 바꾼다. 이름은 코덱 구조체 멤버 이름이다(`args[0]`처럼).
- 장치·세션(`device_context_id`, `session_id`), M_CONTEXT 갱신 번호 셋, REQUEST_ID, QUERY_ID는 옵션과 상태 파일이 정한다. `field=value`로는 바꾸지 않는다.
- `--context-mode new`(기본)는 프레임마다 새 관측(갱신 번호 셋 + 1, AGE는 기본값)이다. `repeat`는 한 관측을 반복하며 AGE에 실제 경과를 더하고, 1000 ms마다 새 관측으로 바꾼다. DOMAIN은 같은 관측의 AGE가 이전 AGE + 자기가 잰 수신 간격보다 작으면 그 프레임을 HOLD하므로(`net_upper_source.c` `younger`), 실제 경과를 올림하고 반복마다 M_CONTEXT 주기의 1/10(최소 3 ms, 최대 10 ms)을 더 얹는다. 노트북과 DOMAIN의 시각 차이가 프레임마다 그 몫 안이면 HOLD되지 않는다. 그만큼 AGE가 실제보다 커진다(관측 하나에서 기본 주기 약 100 ms, 주기 10 ms면 약 300 ms). DOMAIN은 AGE가 2000 ms에 이르면 그 문맥을 쓰지 않는다.
- `--request KIND:OP[:ARG ...]@초`: 20_UART_M.md §6.1 표대로 쓴다. 1:0 도어 LOCK, 1:1 UNLOCK, 2:1:-150 목표 온도 -1.50 °C(i16 LE로 ARG0·1에 넣음, -32768~32767 밖이면 명령줄 오류), 2:2:1 자동 공조, 2:3:2 송풍 2단, 3:1:1 실내등 켬, 3:2:70 밝기 70 %, 3:3:255:128:0 색, 4:1:1 디지털 키 ON.
- `--query SCOPE@초`: 0 현재 상태, 1 요청 결과(요청 쌍 0/0), 2 경고, 3 전체.
- `--ack TYPE:OCCURRENCE[:BOOT]@초`: BOOT를 주지 않으면 마지막으로 받은 M_DOMAIN_ALIVE의 DOMAIN_BOOT_ID를 쓴다. 아직 받지 못했으면 보내지 않고 그 사실을 한 줄로 남긴다(요약 `events_skipped`).
- `--session-change-at 초`: 그 시각부터 SESSION_ID + 1로 M_CONTEXT를 바로 보낸다(앱 세션 교체).
- 코덱이 거절하는 값은 보내지 않고 종료 코드 2다. `run`은 시나리오의 요청·조회·확인 값을 포트를 열기 전에 모두 검사한다. `--allow-invalid`를 주면 그대로 보낸다(DOMAIN의 값 거절 확인용).

## 번호 이어 가기 (`--state`)

- DOMAIN은 같은 장치·세션의 상향 프레임마다 마지막 LINK_SEQUENCE를 기억하고 그보다 옛 번호를 버린다. 같은 세션으로 다시 돌리면 번호를 이어야 한다.
- `--state FILE`은 실행 전에 장치, 세션, 다음 LINK_SEQUENCE, M_CONTEXT 갱신 번호 셋, 마지막 REQUEST_ID·QUERY_ID를 읽고 실행 뒤 다시 쓴다. `--device`, `--session`, `--link`를 주면 그 값이 우선한다.
- `--new-session`은 SESSION_ID + 1로 시작한다. 나머지 번호는 이어 간다. 상향 프레임에는 BOOT가 없어서, DOMAIN이 새 ESP32 실행을 알아보는 것은 새 장치·세션뿐이다.
- 기본 장치는 7, 세션은 1이다.

## 망가뜨린 프레임 (`--bad KIND`, `--inject KIND@초`)

| KIND | 바이트 | DOMAIN 쪽 기대(스트림 카운터) |
|---|---|---|
| `crc` | 마지막 바이트를 바꾼다 | `bad_crc` 증가 |
| `trunc` | 앞 12바이트만 | 조립 한도 50 ms 뒤 `assembly_timeouts` 증가 |
| `len` | PAYLOAD_LENGTH + 1, CRC 다시 계산 | `bad_headers` 증가 |
| `version` | VERSION 1, CRC 다시 계산 | `bad_headers` 증가 |
| `type` | TYPE 0x6f, CRC 다시 계산 | `bad_headers` 증가 |
| `noise` | SYNC가 없는 8바이트 | `discarded_bytes` 증가 |

- `discarded_bytes`는 다시 맞추며 버린 바이트마다 는다(`net_wire_stream.c` `discard`). 그래서 `noise`만이 아니라 모든 종류에서 는다.

- 망가뜨린 프레임은 언제나 M_CONTEXT이고 LINK_SEQUENCE 번호를 쓰지 않는다. 그 뒤 80 ms는 보내지 않는다(조립 한도 50 ms보다 길게).

## 받은 프레임 검사와 판정

| 항목 | 기준 | 판정 |
|---|---|---|
| `line` | SYNC, VERSION 2, 하향 type, 정확한 길이, CRC, 50 ms 안에 다 옴. 포트를 열 때 드라이버 버퍼를 비우고 200 ms 동안 들어오는 것을 읽어 버린다(USB 브리지가 열기 전 프레임을 지니고 있을 수 있다, 요약 `rx_drained_bytes`). 그 뒤 첫 정상 프레임 전에 버린 바이트가 가장 긴 하향 프레임(72바이트)보다 적으면, 그리고 읽기를 멈출 때 들어오던 프레임은 따로 센다(`rx_lead_bytes`, `rx_tail_bytes`) | 그 밖의 오류가 하나라도 있으면 FAIL |
| `codec` | M_DOMAIN_ALIVE 값 검사(코덱과 같음). 다른 하향 type은 프레임 검사만 | 거절 값이 있으면 FAIL |
| `link_sequence` | 하향 번호 하나. +1 정상, 2~32767 앞은 건너뜀(수만 셈), 같거나 옛 번호. 0이 아닌 DOMAIN_BOOT_ID가 바뀌면(DOMAIN 재시작) 처음부터 다시 센다(`restarts`) | 같거나 옛 번호가 있으면 FAIL |
| `alive` | M_DOMAIN_ALIVE가 한 번 이상, 간격 최대 750 ms 이하(ESP32 수신 기한). 포트를 연 뒤 첫 ALIVE까지와 마지막 ALIVE 뒤 읽기를 멈출 때까지도 같은 기한 | 아니면 FAIL. INIT/NO_DATA/65535는 HOLD 경로로 적고, DOMAIN_BOOT_ID가 바뀌면 DOMAIN 재시작으로 적는다 |
| `context` | DEVICE_CONTEXT_ID·SESSION_ID가 있는 프레임(ALIVE 빼고 모두)은 도구가 보낸 문맥이어야 한다 | DOMAIN이 이번 실행의 문맥을 실은 뒤에 모르는 문맥이 오면 FAIL. 이번 실행의 앞 세션(바꾸는 순간 선로에 있던 것)과 `--state` 파일의 문맥(지난 실행. DOMAIN은 링크나 DOMAIN이 다시 시작할 때까지 문맥을 지닌다)은 따로 센다. DOMAIN이 이번 실행의 문맥을 싣기 전의 모르는 문맥도 따로 센다(`unknown_before_taken`, 상태 파일 없이 돌린 지난 실행의 문맥일 수 있다). 따로 세는 것은 언제 와도 실패로 보지 않는다(수로만 본다) |
| `context_taken` | 지금 문맥을 실은 프레임이 하나라도 옴. 문맥마다 처음 실린 시각을 적는다 | TAKEN 또는 HOLD(실패 아님) |
| `age` | 같은 관측을 다시 실을 때 AGE가 줄지 않음. 관측은 저장소 수신 규칙(`net_upper_source.c` `net_upper_uart_original_age`)과 같은 필드로 가린다(도구의 `AGE_KEYS`·`AGE_GUARDS`, host 시험이 대조). 65535는 모름 | 줄면 FAIL |
| `requests` | M_REQUEST마다 REQUEST_SESSION·REQUEST_ID가 같은 M_RESULT | ANSWERED, 없으면 HOLD(그 요청을 보낸 문맥이 실리지 않음) 또는 OPEN(문서에 기한 없음) |
| `queries` | M_QUERY마다 그 QUERY_ID 프레임 수와 M_QUERY_END ITEM_COUNT | ENDED, 수가 다르면 FAIL, 없으면 HOLD 또는 OPEN |

- `overall`은 FAIL 항목이 하나라도 있으면 FAIL이다. HOLD와 OPEN은 실패로 세지 않는다.
- 문서상 주기(M_CONTEXT 100/300 ms, 상태 200/600·1000/3000 ms 등)와 Query 기한은 PROPOSED라 판정에 쓰지 않는다. 종류별 수와 간격 최소·최대는 요약에 있다.

## 출력

- 받은 프레임마다 한 줄(M_DOMAIN_ALIVE는 처음 3개만, `--verbose`면 전부): `  12.345 rx M_DOMAIN_ALIVE link=3 codec=OK domain_boot_id=0x444F4D31 ...`. 받은 잡음·CRC 오류·잘린 프레임도 한 줄씩 나온다. 보낸 M_REQUEST·M_QUERY·M_WARNING_ACK도 한 줄씩 나온다.
- 끝에 `SUMMARY {json}`(보낸 수, 받은 종류별 수와 간격, 받은 오류, LINK_SEQUENCE, 문맥, AGE, ALIVE 요약(값 종류는 16개까지, 넘은 프레임 수는 `frames_beyond_values`), 다음 실행에 쓸 번호)와 `VERDICT {json}`(위 표) 한 줄씩.
- `--log FILE`은 보낸 것과 받은 것을 모두 JSON 한 줄씩 덧붙이고, 끝에 요약과 판정을 덧붙인다. 파일 이름은 `esp32_bench_<시각>.jsonl`처럼 시각을 넣어 실행마다 따로 둔다.

종료 코드: 0 = 끝남(판정은 출력에 있다), 1 = `decode`가 망가졌거나 코덱이 거절하는 프레임을 찾음, 2 = 명령줄 오류, 열 수 없는 포트, 코덱이 거절하는 값.

## 예

```
$ python firmware/network/tools/esp32_bench/esp32_bench.py frame query
# M_QUERY type=0x11 payload=32 B frame=42 B link=1 codec=OK
# fields: device_context_id=7 session_id=0x1 query_id=1 query_scope=0 reserved=0 request_session=0x0 request_id=0 cursor=0
# payload: 07 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
a5 5a 02 11 20 00 01 00 07 00 00 00 01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 28 bd
```

```
python firmware/network/tools/esp32_bench/esp32_bench.py run --port COM7 --duration 30 --state esp32_state.json --request 1:0@5 --query 0@10 --session-change-at 15 --inject crc@20 --log esp32_bench_20261102_1000.jsonl
```
