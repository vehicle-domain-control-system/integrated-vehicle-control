# pi_bench

CIS 보드 시험에서 노트북이 Pi를 맡는 Python 도구다. CIS 보드의 UART-C(S32K144 평가 보드에서는 OpenSDA USB 가상 COM 포트)로 P_VISION_RESULT(0x60)·P_VISION_STATUS(0x61)를 보내고, CIS가 relay한 P_PERMISSION(0x62)을 해석해 출력한다. 일부러 망가뜨린 프레임도 보낼 수 있다.

프레임 외형·CRC·필드 위치는 `docs/network/interface/03_COMMUNICATION/30_UART_C.md`와 `20_UART_M.md` 공통 외형을 따른다. 값 검사는 저장소 코덱(`net_wire_uart_dto.c`, `net_wire_uart_c_status.c`)과 같다. 바이트는 host 시험 `bench_tool.pi_*`(`tests/host/test_pi_bench.py`)가 `tools/vss_bench`(저장소 코덱)와 양방향으로 대조한다.

## 준비

- Python 3.8 이상. `frame`, `decode`는 표준 라이브러리만 쓴다.
- 직렬 포트를 쓰는 `ports`, `send`, `run`은 pyserial이 필요하다: `python -m pip install pyserial`
- 포트 하나는 프로그램 하나만 연다. PuTTY 등 다른 터미널이 같은 COM 포트를 열고 있으면 닫는다.

## 명령

```
python firmware/network/tools/pi_bench/pi_bench.py ports
python firmware/network/tools/pi_bench/pi_bench.py frame result|status|permission [--boot HEX] [--seq N] [--link N] [--bad KIND] [field=value ...]
python firmware/network/tools/pi_bench/pi_bench.py decode HEX ...
python firmware/network/tools/pi_bench/pi_bench.py send --port COM5 result|status [--state FILE] [--new-boot] [--seq N] [--bad KIND] [--listen S] [--log FILE] [field=value ...]
python firmware/network/tools/pi_bench/pi_bench.py run --port COM5 --duration S [--state FILE] [--new-boot] [--result-every MS] [--status-every MS] [--result-mode new|repeat] [--inject KIND@S ...] [--log FILE] [field=value ...]
```

| 명령 | 하는 일 |
|---|---|
| `ports` | 직렬 포트 목록 |
| `frame` | 프레임 하나를 16진수로 출력한다(포트 없음). 필드와 payload도 설명 줄로 나온다 |
| `decode` | 16진수 바이트를 해석한다(포트 없음). 여러 프레임, 잡음, 잘린 프레임도 받는다 |
| `send` | 프레임 하나를 보내고 `--listen` 초(기본 1) 동안 받은 것을 출력한다 |
| `run` | `--duration` 초 동안 P_VISION_RESULT를 `--result-every` ms(기본 100, 0이면 안 보냄), P_VISION_STATUS를 `--status-every` ms(기본 1000, 0이면 안 보냄)마다 보내고 받은 것을 출력한다 |

- 값: 주지 않은 필드는 `vss_bench`와 같은 문서상 유효한 기본값이다. P_VISION_RESULT는 존재 1·인원 1(둘 다 VALID), 기능 상태 ACTIVE(2), AGE 20 ms. P_VISION_STATUS는 READY(1), AGE 20 ms. `field=value`로 바꾼다(이름은 `vss_bench`와 같다). `run`에서는 두 기록 중 그 이름이 있는 쪽에 들어간다.
- 코덱이 거절하는 값(예: 인원 6, VALID인데 사유가 0이 아님)은 보내지 않고 종료 코드 2다. `--allow-invalid`를 주면 그대로 보낸다(CIS의 값 거절 확인용).
- `--result-mode new`(기본)는 프레임마다 새 판정(VISION_SEQUENCE + 1, AGE는 기본값)이다. `repeat`는 한 판정을 반복하고 AGE에 실제 경과를 더한다.

## 번호 이어 가기 (`--state`)

- CIS는 Pi BOOT마다 마지막으로 받은 LINK_SEQUENCE와 판정 번호를 기억하고 그보다 옛 프레임을 버린다. 같은 BOOT로 다시 돌리면 번호를 이어야 한다.
- `--state FILE`은 실행 전에 BOOT, 다음 LINK_SEQUENCE, 마지막 VISION_SEQUENCE·STATUS_SEQUENCE를 읽고 실행 뒤 다시 쓴다. `--boot`, `--link`, `--result-seq`, `--status-seq`, `--seq`를 주면 그 값이 우선한다.
- `--new-boot`는 Pi 재부팅이다. 다음 BOOT(파일의 BOOT + 1, 또는 `--boot`)로 LINK_SEQUENCE 1, 판정 번호 1부터 시작한다. CIS를 리셋한 뒤에도 Pi 쪽 번호는 그대로 이어도 된다(CIS는 Pi BOOT 기준으로 기억한다).
- 기본 BOOT는 0x50493031이다.

## 망가뜨린 프레임 (`--bad KIND`, `--inject KIND@초`)

| KIND | 바이트 | CIS 쪽 기대 |
|---|---|---|
| `crc` | 마지막 바이트를 바꾼다 | `stream.bad_crc` 증가, 콜백 없음 |
| `trunc` | 앞 12바이트만 | 조립 한도 50 ms 뒤 `stream.assembly_timeouts` 증가, 콜백 없음 |
| `len` | PAYLOAD_LENGTH + 1, CRC 다시 계산 | `stream.bad_headers` 증가 |
| `version` | VERSION 2, CRC 다시 계산 | `stream.bad_headers` 증가 |
| `type` | TYPE 0x6f, CRC 다시 계산 | `stream.bad_headers` 증가 |
| `noise` | SYNC가 없는 8바이트 | `stream.discarded_bytes` 증가 |

- 망가뜨린 프레임은 LINK_SEQUENCE 번호를 쓰지 않는다. 다음 정상 프레임이 같은 번호를 쓴다.
- `run --inject crc@5 --inject trunc@10`처럼 실행 중 그 시각에 P_VISION_RESULT를 망가뜨려 한 번 보낸다. 그 뒤 80 ms는 보내지 않는다(CIS 조립 한도 50 ms보다 길게 쉬어 잘린 프레임이 다음 프레임과 섞이지 않게 한다).

## 출력

- 받은 프레임마다 한 줄: `  12.345 rx P_PERMISSION link=3 codec=OK cis_boot_id=0x43495331 update_sequence=2 ...`. 받은 잡음·CRC 오류·잘린 프레임도 한 줄씩 나온다.
- 끝에 `SUMMARY {json}` 한 줄: 보낸 수, 받은 프레임 수, 받은 오류(`rx_damaged`), P_PERMISSION 요약(수, CIS BOOT, UPDATE_SEQUENCE 처음·끝과 +1이 아닌 걸음 수, LINK_SEQUENCE +1이 아닌 걸음 수, USE_LIMIT, 간격 최소·최대와 1500 ms 넘은 간격 수, 마지막 기록), 다음 실행에 쓸 번호.
- `--log FILE`은 보낸 것과 받은 것을 모두 JSON 한 줄씩 덧붙인다(시각, 방향, 종류, LINK_SEQUENCE, 필드, 16진수).

종료 코드: 0 = 끝남(회선에서 일어난 일은 요약에 있다), 1 = `decode`가 망가졌거나 코덱이 거절하는 프레임을 찾음, 2 = 명령줄 오류, 열 수 없는 포트, 코덱이 거절하는 값.

## 예

```
$ python firmware/network/tools/pi_bench/pi_bench.py frame status --boot 50493031 --seq 42
# P_VISION_STATUS type=0x61 payload=16 B frame=26 B link=1 codec=OK
# fields: pi_boot_id=0x50493031 status_sequence=42 vision_status=1 active_fault=0 last_fault=0 status_age_ms=20 affected_function=0
# payload: 31 30 49 50 2a 00 00 00 01 00 00 00 00 14 00 00
a5 5a 01 61 10 00 01 00 31 30 49 50 2a 00 00 00 01 00 00 00 00 14 00 00 d2 e8
```

이 프레임은 `tools/vss_bench/README.md`의 참고 프레임(LINK_SEQ 1)과 같다.

```
python firmware/network/tools/pi_bench/pi_bench.py run --port COM5 --duration 60 --state pi_state.json --log pi_run.jsonl
```
