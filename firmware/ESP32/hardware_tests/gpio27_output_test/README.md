# GPIO27 Open-Drain Output Test

이 디렉터리는 메인 Gateway Firmware와 분리된 ESP-IDF 테스트 프로젝트다. GPIO27만 push-pull
출력이 아닌 `GPIO_MODE_OUTPUT_OD`로 설정하여 LOW 구동과 출력 해제를 3초마다 전환한다.
시작 상태는 level 1을 설정한 출력 해제 상태다. 로그는 기존 기본 콘솔 UART0 115200으로 출력하며
UART-M Driver, BLE, Gateway는 시작하지 않는다. GPIO26은 설정하거나 접근하지 않는다.

## 안전 조건

현재 저장소에는 교수님 제작 보드의 회로도나 GPIO27 net 정보가 없다. ESP32 자체에서 GPIO27이
입출력 가능하다는 사실만 확인됐으며, 보드에서 GPIO27에 연결된 외부 출력, LED, 풀업, 트랜시버
등은 배제되지 않았다. Open-drain은 HIGH를 직접 구동하지 않지만 LOW 상태에서는 외부 회로의
전류를 sink하므로 이 테스트 자체가 보드나 GPIO 출력의 안전을 보장하지 않는다. 다음 조건을
확인하기 전에는 이 이미지를 Flash하지 않는다.

- 보드 전원을 끈 상태에서 GPIO27 net과 3.3 V/GND 및 인접 핀의 저항·도통을 확인한다.
- GPIO27에 다른 push-pull 출력이나 저임피던스 부하가 연결되지 않았음을 회로도 또는 제작자에게
  확인한다.
- S32K344, USB-TTL 등 외부 장치의 TX/VCC는 분리한다.
- 보드가 `esp32` Target이며 GPIO27이 커넥터의 실제 측정 핀과 일치하는지 확인한다.

## Build only

ESP-IDF v5.5.5 PowerShell 환경에서 다음 명령을 실행한다.

```powershell
cd C:\esp32-projects\esp32\hardware_tests\gpio27_output_test
idf.py set-target esp32
idf.py build
```

빌드는 핀을 구동하지 않는다. Flash는 안전 확인 후 사용자가 직접 실행한다.

```powershell
idf.py -p COMx flash monitor
```

`COMx`는 실제 USB-UART 포트로 바꾼다. UART0가 Flash/Monitor에 계속 사용된다.

## 전압 측정

1. S32K344와 USB-TTL의 Application TX/RX/VCC 배선을 모두 분리한다.
2. 멀티미터 검정 리드를 `COM`, 빨강 리드를 `V/Ohm` 단자에 연결한다.
3. 수동 범위 미터는 DC 20 V 범위를 선택한다. 전류/저항/도통 모드를 사용하지 않는다.
4. 전원을 끈 상태에서 검정 클립을 보드 GND에 고정하고 단락이 없는지 확인한다.
5. 보드 전원을 켠 뒤 빨강 프로브를 GPIO27 측정점에 댄다.
6. LOW 구동 상태는 약 0 V가 예상된다. 출력 해제 상태의 전압은 외부 pull-up/연결 회로에 의해
   결정되므로 pull-up이 확인된 경우에만 약 3.3 V를 예상할 수 있다.

예상 로그:

```text
I (...) gpio27_test: GPIO27 open-drain=RELEASED (set level=1)
I (...) gpio27_test: GPIO27 open-drain=LOW
I (...) gpio27_test: GPIO27 open-drain=RELEASED (set level=1)
```

LOW 구동 로그인데 계속 3.3 V이거나, 예상 밖 전압·리셋·발열이 있으면 즉시 전원을 끄고 외부
회로를 다시 확인한다. 출력 해제 시 floating 전압이 측정될 수 있다. 전원이 켜진 상태에서는 저항
또는 도통 측정을 하지 않는다.

## 원래 Firmware 복구

테스트 후 메인 프로젝트를 다시 Build/Flash한다. 테스트 프로젝트는 메인 `main/`과
`components/uart/`를 참조하거나 변경하지 않는다.

```powershell
cd C:\esp32-projects\esp32
idf.py build
idf.py -p COMx flash monitor
```
