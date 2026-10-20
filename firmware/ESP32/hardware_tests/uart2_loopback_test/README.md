# UART2 GPIO27/GPIO26 Loopback Test

메인 Gateway Firmware와 분리된 ESP-IDF 하드웨어 진단 프로젝트다. UART2 TX를 GPIO27,
UART2 RX를 GPIO26으로 명시하고 115200 8N1로 동작한다. UART0는 기존 Flash와 Serial Monitor
로그 전용으로 유지한다. BLE, UART-M Transport, Gateway는 초기화하지 않는다.

## 안전 경고

GPIO27은 이 테스트에서 push-pull UART TX가 된다. 저장소에는 교수님 제작 보드의 GPIO27 net과
외부 회로를 증명하는 회로도가 없으므로 PCB 내부의 다른 출력, LED, 트랜시버 또는 저임피던스
부하와의 충돌 위험은 완전히 배제되지 않았다. Loopback 성공도 GPIO27의 다른 하드웨어 연결이나
향후 S32K344 연결의 전기적 안전을 보장하지 않는다. 회로도 또는 제작자 확인 전에는 Flash하지
않는다.

## 테스트 패킷과 판정

1초마다 16바이트 자체 진단 패킷을 송신한다. 패킷은 magic, little-endian 32비트 sequence와
결정적 패턴으로 구성된다. 수신 전에 UART2 RX 버퍼를 비우고, 송신 완료 후 최대 250 ms만
수신을 기다린다. 길이와 모든 바이트가 같아야 `SUCCESS`다. 점퍼가 없으면 무한 대기하지 않고
`TIMEOUT`을 기록하며, timeout은 누적 실패에 포함한다.

UART-M v2 Frame은 사용하지 않는다. ESP32 TX용 UART-M 메시지를 자체 RX로 돌려보내면 기존
P2/P3 방향 검증에서 거부되며, 단순 전기적 loopback 시험이 P1/P2/P3와 결합되기 때문이다.

## Build

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass
. C:\esp\v5.5.5\esp-idf\export.ps1
cd C:\esp32-projects\esp32\hardware_tests\uart2_loopback_test
idf.py set-target esp32
idf.py build
```

## Flash 및 점퍼 이동

아래 작업은 GPIO27 출력 충돌 위험을 확인한 뒤 사용자가 직접 수행한다.

1. 기존 USB-TTL의 TX/RX/GND 3선을 사용해 Firmware를 Flash한다.
2. Flash가 끝나면 보드와 USB-TTL 전원을 완전히 끈다.
3. USB-TTL TX와 ESP32 SERIAL RX 사이의 점퍼 한 개를 양쪽에서 완전히 분리한다.
4. 분리한 점퍼를 GPIO27과 GPIO26 사이에 연결한다.
5. USB-TTL RX와 ESP32 SERIAL TX 연결 및 공통 GND는 유지한다.
6. USB-TTL VCC는 연결하지 않는다. 보드는 검증된 기존 전원 방식으로 공급한다.
7. 전원을 다시 공급하고 UART0 Serial Monitor에서 결과를 확인한다.

```text
ESP32 GPIO27 / UART2 TX  --------  ESP32 GPIO26 / UART2 RX

USB-TTL RX                 <----- ESP32 UART0 / SERIAL TX
USB-TTL GND                ------ ESP32 GND
USB-TTL TX                 --X--  ESP32 UART0 / SERIAL RX  (Flash 후 양쪽 분리)
```

Flash 명령:

```powershell
idf.py -p COMx flash monitor
```

`COMx`는 실제 USB-TTL 포트로 바꾼다. 전원 차단 전에 점퍼를 이동하지 않는다.

## 예상 로그와 해석

```text
I (...) UART2_LOOPBACK: UART2 loopback started: TX=GPIO27 RX=GPIO26 115200 8N1
W (...) UART2_LOOPBACK: GPIO27 is push-pull UART TX; board-level conflict is not ruled out
I (...) UART2_LOOPBACK: SUCCESS seq=0 length=16 success=1 failure=0
I (...) UART2_LOOPBACK: SUCCESS seq=1 length=16 success=2 failure=0
```

- `SUCCESS`: 해당 패킷의 길이와 모든 바이트가 일치했다.
- `TIMEOUT`: 250 ms 안에 바이트를 받지 못했다. 점퍼, GPIO 번호, GND를 확인한다.
- `FAIL RX length`: 일부 바이트만 수신됐다. 접촉, baud, 전원과 간섭을 확인한다.
- `FAIL byte[...]`: 길이는 같지만 내용이 달랐다. 배선 품질과 다른 회로 구동을 확인한다.
- TX/flush 실패: UART Driver 초기화 상태와 리소스를 확인한다.

## 기존 Firmware 복구

1. 보드와 USB-TTL 전원을 완전히 끈다.
2. GPIO27-GPIO26 loopback 점퍼를 제거한다.
3. USB-TTL TX를 원래 ESP32 UART0/SERIAL RX에 다시 연결한다.
4. UART0 RX/TX/GND 배선을 확인한 뒤 메인 Firmware를 다시 Build/Flash한다.

```powershell
cd C:\esp32-projects\esp32
idf.py build
idf.py -p COMx flash monitor
```
