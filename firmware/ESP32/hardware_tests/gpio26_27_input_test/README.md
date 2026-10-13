# GPIO26/27 Input Test

메인 Gateway Firmware 및 GPIO27 출력 테스트와 분리된 ESP-IDF 테스트 프로젝트다.
GPIO26과 GPIO27을 내부 pull-up/pull-down이 없는 입력으로 설정하고 1초마다 논리 값을
UART0 Serial Monitor에 출력한다. BLE, UART-M Driver, Gateway는 초기화하지 않는다.

## Build

ESP-IDF v5.5.5 PowerShell 환경에서 실행한다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass
. C:\esp\v5.5.5\esp-idf\export.ps1
cd C:\esp32-projects\esp32\hardware_tests\gpio26_27_input_test
idf.py set-target esp32
idf.py build
```

## Flash — 사용자가 별도로 실행

```powershell
idf.py -p COMx flash monitor
```

`COMx`는 실제 ESP32 USB-UART 포트로 바꾼다. UART0/115200이 Flash와 로그에 사용된다.

예상 로그:

```text
I (...) GPIO_TEST: GPIO26/27 input test started; internal pulls disabled
I (...) GPIO_TEST: GPIO26=1 GPIO27=1
I (...) GPIO_TEST: GPIO26=1 GPIO27=1
```

두 핀이 floating 상태이면 값이 불안정하게 바뀔 수 있다. 입력 결과와 멀티미터 전압만으로
교수님 보드의 외부 연결, 출력 충돌 가능성 또는 push-pull 출력 안전성을 판단하지 않는다.
출력 시험 전에는 회로도, net 연결, 전압 및 다른 출력 회로 존재 여부를 별도로 확인한다.

## 문제 확인

- 로그가 없으면 실제 COM 포트, UART0 115200, USB-UART 연결과 보드 전원을 확인한다.
- 값이 흔들리면 내부 pull이 꺼진 floating 입력 또는 외부 신호일 수 있다.
- 예상과 반대 값이면 GPIO26/27 측정점과 GND 기준, 핀 라벨을 다시 확인한다.
- 항상 HIGH 또는 LOW여도 외부 회로가 없다고 단정하지 않는다.
- S32K344나 USB-TTL을 연결할 때는 양쪽 전압과 TX/RX 방향을 먼저 확인한다.
