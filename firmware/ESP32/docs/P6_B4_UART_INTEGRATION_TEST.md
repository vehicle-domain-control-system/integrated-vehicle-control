# P6-B4-B UART-M 통합 사전 시험

이 절차는 `M_DOMAIN_ALIVE` RX, timeout 및 복구만 확인한다. Frame 수신 성공은
SessionReady, 차량 권한 또는 차량 제어 성공을 의미하지 않는다. Production UART TX,
`M_CONTEXT`, Query/Result routing과 Digital Key 자동 Unlock은 계속 비활성이다.

P6-B7-A의 최초 실보드 진단은 기본 비활성인
`CONFIG_P6_UART_RX_DIAGNOSTICS`를 사용한다. 활성화하면 5초마다 Frame/Codec 오류와
마지막 Alive를 읽기 전용으로 요약하며 raw frame, Session, Bond 및 Registration 값은
출력하지 않는다. `LINK_SEQUENCE`와 Alive payload의 `UPDATE_SEQUENCE`는 별도 항목이다.
UART-M Wire에는 Port Epoch가 없으며 ESP32는 이를 임의 필드로 추가하지 않는다.

## 배선 전 확인

| 신호 | ESP32 | S32K344 후보 | 방향 |
|---|---|---|---|
| RX | UART2 GPIO26 | LPUART11 TX PTC10 / J43-3 | S32K → ESP32 |
| TX | UART2 GPIO27 | LPUART11 RX PTC11 / J43-4 | ESP32 → S32K |
| GND | GND | GND | 공통 기준 |

J43 실제 net, S32K pin mux와 I/O 전압은 아직 검증되지 않았다. 양쪽 전원을 끈 뒤
배선하고, 두 UART 신호가 3.3V 호환임을 회로도/측정으로 확인하기 전에는 연결하지
않는다. 5V UART를 ESP32 GPIO에 직접 연결하지 않는다. 이 단계의 RX-only 시험에서는
외부 3.3V UART 송신기의 TX를 GPIO26에, GND를 공통 GND에 연결하고 GPIO27은 연결하지
않아도 된다.

Phase 5-M-17B의 후보 단방향 배선은 S32K344-WB LPUART11 TX PTC10(J43-3)에서
ESP32 GPIO26 RX로 연결하는 것이다. Network `336fd91`의 일반 보드 기본값은 UART-M
LPUART0 잠정값이므로, LPUART11 ConfigTools/PinMux/Callback 적용과 J43 net 및 3.3V I/O를
Domain UART-only Diagnostic Gate에서 별도로 확인하기 전에는 이 후보 배선을 사용하지
않는다. 최초 RX-only 시험에서는 PTC11/J43-4와 ESP32 GPIO27을 연결하지 않는다.

## P6-B7-A 진단 빌드와 Monitor

기본 빌드는 진단 Task를 포함하지 않는다. 별도 Build 디렉터리에서 다음 프로파일을
명시한다.

```powershell
idf.py -B build_p6b7_diag `
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.p6b7_uart_rx_diagnostics.defaults" build
idf.py -B build_p6b7_diag -p COMx monitor
```

`COMx`는 사용자가 실제 ESP32 UART0 Flash/Monitor 포트를 확인해 바꾼다. Flash는 배선과
전압 확인 및 사용자 승인 뒤 application-only 절차로 별도 수행한다. 예상 요약은 다음
세 판정을 분리한다.

- `frames`/`alive_decoded`: Frame/CRC/Type/Length/Codec을 통과한 구문 수신.
- `supervisor_ok`: Quality OK이고 Source Age가 750ms 이하인 Alive 수용.
- `link`: Gateway Alive supervisor의 현재 UP/DOWN 상태. 차량 READY나 SessionReady가 아니다.

마지막 Alive 요약은 LinkSequence, Boot ID, payload UpdateSequence, Age, Domain State,
Quality를 각각 출력한다. 오류 요약은 VERSION/LENGTH/CRC/Codec/Direction, parser/UART
overflow, frame assembly timeout, Transport/Gateway queue full을 구분한다.

## Host 검증과 Frame 생성

```powershell
cmake -S tests/uart_transport -B build-host/uart_transport -G Ninja
cmake --build build-host/uart_transport
ctest --test-dir build-host/uart_transport --output-on-failure

cmake -S tests/gateway_adapter -B build-host/gateway_adapter -G Ninja
cmake --build build-host/gateway_adapter
ctest --test-dir build-host/gateway_adapter --output-on-failure

python tools/uart_m_alive_generator.py --link-sequence 65535
python tools/uart_m_alive_generator.py --crc-error --output alive_bad_crc.bin
python tools/uart_m_alive_generator.py --boot-id 1 --next-boot-id 2 --output alive_boot_change.bin
python tests/tools/test_uart_m_alive_generator.py
```

스크립트의 dry-run/Hex 및 파일 출력은 Python 표준 라이브러리만 사용한다. `--port`를
지정한 COM 송신에는 `pyserial`이 필요하다. GPIO26에 주입하려면 별도의 3.3V USB-UART
송신 장치와 115200 8N1/no-flow 설정이 필요하다.

### PC USB-UART에서 250ms 주기 송신

`--port`를 지정하면 유한한 개수의 Frame만 전송한 뒤 포트를 닫는다. ESP32 UART0
Monitor용 COM 포트와 외부 USB-UART 송신 COM 포트는 서로 다르다.

```powershell
# 정상 READY/OK Alive 20회(5초)
python tools/uart_m_alive_generator.py --port COMx --count 20 --period-ms 250 `
  --boot-id 1 --link-sequence 0 --domain-state 1 --quality 0 --source-age-ms 0

# INIT/NO_DATA: valid Frame이지만 supervisor/link 수용 대상이 아님
python tools/uart_m_alive_generator.py --port COMx --count 8 --period-ms 250 `
  --boot-id 1 --link-sequence 20 --domain-state 0 --quality 3 --source-age-ms 65535

# 전송 중간에 Boot ID를 한 번 변경
python tools/uart_m_alive_generator.py --port COMx --count 12 --period-ms 250 `
  --boot-id 1 --next-boot-id 2 --link-sequence 30 --domain-state 1 --quality 0

# CRC 오류 Frame
python tools/uart_m_alive_generator.py --port COMx --count 8 --period-ms 250 `
  --boot-id 2 --link-sequence 50 --domain-state 1 --quality 0 --crc-error
```

유한 송신이 끝난 뒤 750ms를 초과해 Link DOWN을 확인하고, 정상 READY/OK 명령을 다시
실행해 Link RECOVERED를 확인한다. USB-UART가 없으면 `--port`를 생략해 Hex dry-run을
사용하거나 `--output <file>`로 한 Frame의 raw binary를 저장할 수 있다.

초기 단방향 시험은 USB-UART TX → ESP32 GPIO26(RX), GND ↔ GND만 연결한다. USB-UART
RX와 ESP32 GPIO27(TX)는 연결하지 않는다. 어댑터가 3.3V UART logic인지 먼저 확인하고,
별도 전원 계획이 검증되지 않았다면 USB-UART VCC를 ESP32 전원 핀에 연결하지 않는다.

## 실보드 절차와 판정

1. UART0 monitor에서 부팅과 기존 BLE 동작을 확인한다.
2. 정상 Alive를 750ms timeout보다 짧은 주기(문서 제안 250ms)로 주입한다.
   Quality OK 및 Source Age 750ms 이하이면 `Domain UART link UP`과 Gateway Status의
   Domain link=1을 확인한다. 현재 Network fallback인 INIT/NO_DATA/AGE 65535도 유효한
   Frame이면 `alive_decoded`는 증가하지만 `supervisor_ok`는 증가하지 않고 link는 DOWN을
   유지해야 한다. 어떤 경우에도 INIT을 차량 READY로 해석하지 않는다.
3. CRC 오류 Frame은 Alive accepted 수를 증가시키거나 link를 복구시키면 FAIL이다.
4. 송신을 750ms 초과 중단해 `Domain UART link DOWN`과 link=0을 확인한다.
5. 정상 Frame을 재개해 `Domain UART link RECOVERED`를 확인한다.
6. 다른 nonzero Boot ID를 주입해 Boot 변경 통계가 1회 증가하는지 확인한다.

시험용 Boot ID `0x00000001`은 관측 데이터일 뿐 제품 Boot ID 정책으로 고정하지 않는다.
Boot 변경은 기존 Gateway Core에서 SessionReady와 active Query를 무효화하지만, 그 자체가
새 Session이나 차량 권한을 만들지 않는다.

PASS는 valid/invalid 분리, timeout, 정상 복구와 Boot 변경 관측이 모두 일치할 때다.
S32K344 실제 송수신, J43 전기 특성, Link Sequence 중복·역순 정책은 아직 미검증이다.
현재 ESP32 수신기는 Link Sequence wrap/중복/역순을 보존해 상위에 전달하며, 합의되지
않은 거부 규칙을 적용하지 않는다.
