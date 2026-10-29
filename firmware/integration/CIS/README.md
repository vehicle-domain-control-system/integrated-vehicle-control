# CIS 노드 통합 폴더 (S32K144, Network CIS 프로필)

CIS 노드 하나를 빌드하는 데 필요한 소스를 모은 폴더다. 구성은 Network 스택(CAN FD + UART-C), CIS 앱(센서 3종, Pi 판정, 후방 허가), 보드 시계·hook, `main()`이다.

`src/` 구조는 보드에서 확인한 S32DS 프로젝트(`network_144_cis_app`)의 `src/`와 같다. 그래서 다른 IDE에서는 이 `src/`를 프로젝트 소스로 넣고, 아래 4절의 설정을 옮기면 된다.

- **보드 확인(2026-10-29):** S32K144 평가 보드에서 확인했다. 상대는 S32K344 디버그 노드(DOMAIN 역할)였다.
  - CAN 송신: CIS_REAR 50 ms, CIS_ENVIRONMENT 200 ms, CIS_STATUS 250 ms. 받은 수와 TX_SEQUENCE가 같아 빠진 프레임이 없었다.
  - 0x080 허가: 받으면 후방 측정값이 VALID로 열리고, 끊기면 COMMUNICATION_FAULT로 닫혔다.
  - Pi(UART-C): 노트북 Pi 도구(pi_bench, COM3)로 UART-C 송수신과 CAN 중계를 확인했다. 실제 Raspberry Pi와 pi_vision 스크립트는 확인하지 않았다(5절).
- **빌드 확인:** 이 폴더의 소스를 보드 빌드와 같은 컴파일러·플래그로 빌드했다. 결과 flash 이미지가 보드에서 확인한 이미지와 바이트 단위로 같았다(컴파일 67개, 경고 0).

## 1. 구성

| 경로 | 내용 | 관리 | 출처 |
|---|---|---|---|
| `src/main.c` | Clock → Port → IntCtrl 초기화, `Board_TimeInit()` → `CisNode_Init()`, 루프 `CisNode_Service()` | CIS | `feat/cis-rtd-network` `firmware/CIS/s32ds/main.c` |
| `src/cis/` (37개) | CIS 앱과 센서 드라이버. 레지스터를 직접 다루고 RTD API는 쓰지 않는다 | CIS | `feat/cis-rtd-network` `firmware/CIS/src/` |
| `src/net_bench/net_board_time144.c/.h` | SysTick 1 kHz 시계, `Board_*` hook, BOOT_ID | Network | `feat/vss-network-node` `firmware/network/nodes/cis/bench/s32k144_cis/` |
| `src/network/` (95개) | Network 스택의 CIS 프로필 생성 사본(`sync_to_s32ds.py --node CIS`). `README.md`는 Network의 CIS 통합 메모, `NETWORK_BUNDLE.txt`는 출처와 해시 기록 | Network | `feat/vss-network-node` `firmware/network/` |
| `s32ds/cis144_board_tested.mex` | 보드에서 확인한 S32DS 프로젝트의 ConfigTools 설정(핀, 클럭, FlexCAN, LPUART, IntCtrl). 원래 이름은 `network_144_can_debuger.mex` | 통합 | S32DS 프로젝트 |
| `SOURCES.txt` | 출처 커밋과 파일별 SHA-256(CRLF를 LF로 읽은 값) | 통합 | |
| `ISSUES.md` | 보드 시험에서 찾은 결함 | 통합 | |

출처 커밋은 다음과 같다.
- CIS: `feat/cis-rtd-network` `129ef58`
- Network: `feat/vss-network-node` `02051db`

**이 폴더의 파일은 직접 고치지 않는다.** 원본에서 고친 뒤 다시 만든다(6절).

## 2. 정리해서 뺀 것

| 뺀 것 | 이유 |
|---|---|
| 디버그 노드 파일 `debug_*.c/.h` | `NET_NODE_BUILD`에서 내용이 모두 빠져 빈 오브젝트가 된다 |
| 시험대 앱 `cis_bench_app.c/.h` | 실제 CIS 앱으로 바뀌었다. 함께 넣으면 `CisApp_On*` 정의가 겹친다 |
| CIS 단독 빌드 파일 `src/main.c`, `pi_uart.c`, `net_api_shim.h` | `CIS_USE_NETWORK_API` 빌드에서는 UART-C를 Network가 맡는다. `main.c`는 위의 `s32ds/main.c`로 바뀐다 |
| Network의 BCM·VSS·DOMAIN 전용 파일 10개 | CIS 프로필 빌드에서 0 B였다. `--node CIS` 사본에는 처음부터 들어가지 않는다 |
| NXP RTD 드라이버, 생성 설정(`RTD/`, `generate/`, `board/`), `Project_Settings/`(시작 코드, 링커 스크립트) | NXP 파일이다. 저장소에 올리지 않고 각 IDE에서 RTD 설치본과 ConfigTools로 만든다(4.1절) |
| 빌드 산출물(`Debug_FLASH/`), `.cproject`, `.project` | IDE 전용이다. 필요한 값은 4절에 적었다 |
| CIS 도구(`sync_cis_to_s32ds.py`, `add_network_to_*.py`) | S32DS 프로젝트 전용이다. CIS 브랜치에 그대로 있다 |

## 3. 빌드 결과(보드 확인 빌드 기준)

- **Flash:** 약 125 KB(`.flash` 127,356 B, 벡터 784 B, 설정 16 B). 전체 512 KB.
- **RAM(.data + .bss):** 26,732 B. int_sram 57,088 B의 약 47%다. 스택 4 KB와 heap 512 B는 따로 잡힌다.

  | 부분 | 크기 |
  |---|---|
  | Network 코어 | 18,976 B |
  | Network RTD 백엔드 | 4,984 B |
  | RTD 드라이버 | 1,042 B |
  | 생성 설정 | 784 B |
  | CIS 앱 | 458 B |

## 4. 다른 IDE·프로젝트에 적용할 때

### 4.1 RTD와 생성 설정

- **RTD 버전:** S32K1 RTD 3.0.0(`S32K1_RTD_3_0_0_QLP06_D2603`)을 쓴다.
- **빌드에 넣는 RTD 모듈:** `Clock_Ip*`, `Port_Ci_Port_Ip*`, `IntCtrl_Ip*`, `FlexCAN_Ip*`, `Lpuart_Uart_Ip*`, `OsIf*`(Interrupts, Software_Semaphore, Timer, Timer_System, Timer_System_Internal_Systick), `Det`, `SchM_Can_43_FLEXCAN`, `SchM_Mcu`, `SchM_Port`, `SchM_Uart`.
- **생성 설정:** S32 Config Tools로 `s32ds/cis144_board_tested.mex`를 열고 코드를 생성한다.
  - 생성되는 파일: `Clock_Ip_Cfg`, `Port_Ci_Port_Ip_Cfg`, `FlexCAN_Ip_Sa_PBcfg`, `Lpuart_Uart_Ip_Sa_PBcfg`, `IntCtrl_Ip_Cfg`, `OsIf_Cfg`.
  - 새 프로젝트에서 처음 만들 때는 CIS 브랜치의 `NETWORK_INTEGRATION.md` 1-B절을 따른다.
- **이름을 맞춰야 하는 것:** Network가 생성 설정의 이름을 그대로 쓰므로, 생성 설정에서 이름이 다르면 define 값만 맞춘다(Network 소스는 고치지 않는다).
  - **CAN 설정:** `FlexCAN_Config0`, 상태 `FlexCAN_State0`, 인스턴스 0, `max_num_mb` 7, FD payload 64 B. Network 기본값과 같아 define이 필요 없다.
  - **UART 설정:** `Lpuart_Uart_Ip_xHwConfigPB_1`(LPUART1). define `NET_BOARD_UART_C_CONFIG`로 넘긴다.
  - **콜백 세 개:** `DebugCan_EventCallback`, `DebugCan_ErrorCallback`, `DebugConsole_UartCallback`. 생성 설정이 부르는 이름을 define으로 넘기면 Network HAL이 그 이름의 함수를 정의한다. 그래서 다른 파일에 같은 이름의 함수가 있으면 중복 정의가 된다. 이름은 디버그 노드에서 물려받은 것이다. 바꾸려면 `.mex`와 define을 함께 바꾼다.

### 4.2 컴파일러와 플래그

보드 확인 빌드와 같은 값을 쓴다. 컴파일러는 GCC 10.2 arm-none-eabi(S32DS 3.6.6 동봉)다.

```
-std=c99 -Os -mcpu=cortex-m4 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv4-sp-d16
-funsigned-char -fno-short-enums -funsigned-bitfields -fno-common -fomit-frame-pointer
-Wall -Wextra -pedantic -Wstrict-prototypes -Wsign-compare -Wundef -Wdouble-promotion
-Werror=implicit-function-declaration
링크: -nostartfiles --entry=Reset_Handler -specs=nano.specs -specs=nosys.specs -lc -lm -lgcc
```

- `-fno-short-enums`와 `-funsigned-char`를 바꾸지 않는다. 값이 바뀌면 구조체 크기와 부호가 달라질 수 있다.
- 링크할 때 `libc_nano.a ... uses variable-size enums` 경고가 네 줄 나온다. 툴체인 라이브러리 쪽 경고로, 보드 빌드에서도 같다.
- **링커 스크립트:** RTD 섹션(`.mcal_text`, `.mcal_bss` 등)을 배치하는 S32K144 flash 스크립트가 필요하다. S32DS 프로젝트의 `linker_flash_s32k144.ld`를 그대로 쓰거나, 그 섹션 배치를 옮긴다.
- **GCC 전용 문법:** IAR처럼 GCC 문법을 받지 않는 컴파일러에서는 아래 줄을 그 컴파일러의 문법으로 바꿔야 한다. Network 줄은 `feat/vss-network-node` 브랜치의 `firmware/network`에서, CIS 줄은 `feat/cis-rtd-network` 브랜치의 `firmware/CIS`에서 고친 뒤 다시 만든다.
  - `src/network/src/platform/rtd/net_board_cfg.h:314-319`: `__asm volatile`로 PRIMASK 저장·복원
  - `src/network/src/l04_frame_codec/net_wire_common.c:21`: `__attribute__((may_alias))`
  - `src/cis/dht22.c:29-36`: `cpsid i`·`cpsie i`

### 4.3 include 경로와 define

include 경로 9개(경로 앞부분 `src/`는 이 폴더 기준):

```
src/network/include
src/network/src/l04_frame_codec
src/network/src/l02_typed_handoff/m03_typed_handoff
src/network/src/l05_transport_port/m07_buffer_outcome
src/network/src/l05_transport_port/m09_observation
src/network/src/l03_message_service/m04_message_service
src/network/src/platform/rtd
src/net_bench
src/cis
```

여기에 RTD include와 생성 설정 include를 더한다. S32DS 기준으로는 `generate/include`, `RTD/include`, `board`, PlatformSDK의 `BaseNXP .../header`·`include`, `Platform .../include`·`startup/include`다.

define 17개:

```
CPU_S32K144HFT0VLLT  I_CACHE_ENABLE  ENABLE_FPU  GCC  CPU_CORTEX_M4F       (S32DS 기본 5개)
NETWORK_UPPER_NODE_PROFILE=1
NET_NODE_BUILD
NET_BOARD_ISR_TICK_HOOK=Board_TickMs32
NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32
NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig
NET_BOARD_US_HOOK=Board_Us32
NET_BOARD_CAN_CALLBACK=DebugCan_EventCallback
NET_BOARD_CAN_ERROR_CALLBACK=DebugCan_ErrorCallback
NET_BOARD_UART_C_INSTANCE=1U
NET_BOARD_UART_C_CONFIG=Lpuart_Uart_Ip_xHwConfigPB_1
NET_BOARD_UART_CALLBACK=DebugConsole_UartCallback
CIS_USE_NETWORK_API
```

`NETWORK_NODE_PLATFORM`은 넣지 않는다. MCU는 `CPU_S32K144HFT0VLLT`로 정해진다.

### 4.4 클럭

| 항목 | 값 | 바꿀 때 같이 바꿀 것 |
|---|---|---|
| 코어 | FIRC 48 MHz | `src/net_bench/net_board_time144.h:28` `BOARD_CORE_CLK_HZ`(SysTick 1 ms와 µs 계산) |
| FlexCAN0 PE | 48 MHz | CAN 비트 타이밍(4.6절) |
| LPUART1 | 8 MHz, 115200 설정(실제 115,942 bit/s) | LPUART 분주 |
| SOSC | 8 MHz 켬, SOSCDIV1 = SOSCDIV2 = 1분주 | CIS 센서 시각이 FTM0 1 MHz(SOSCDIV1)이고 VEML7700 I2C가 LPI2C0(SOSCDIV2)이라 이 값이 필요하다. SOSCDIV1이 1분주가 아니면 `CisNode_Init()`이 false를 낸다 |

### 4.5 핀과 주변장치

- **RTD Port 초기화로 잡는 핀:** CAN0 RX PTE4·TX PTE5(ALT5), LPUART1 RX PTC6·TX PTC7(ALT2, 평가 보드에서는 OpenSDA 가상 COM).
- **CIS 드라이버가 직접 먹스하는 핀:** HC-SR04 TRIG PTE7·ECHO PTE8, DHT22 PTA14, VEML7700 SDA PTA2·SCL PTA3(LPI2C0). 드라이버가 Port 초기화 뒤에 설정한다.
- **피할 것:** CIS가 FTM0과 LPI2C0을 레지스터로 직접 쓴다. 이 두 주변장치와 위 핀을 다른 RTD 설정이나 코드가 쓰지 않게 한다.
- `.mex`에 LPUART0 PTB0·PTB1이 남아 있다. 디버그 노드에서 물려받은 설정이고, CIS 빌드에서는 초기화하지 않아 영향이 없다.

### 4.6 CAN

- **보드 확인 값:** CAN FD ISO, BRS, 500 kbit/s / 2 Mbit/s, 자동 bus-off 복구 켬.
  - 공칭: 분주 6, prop 7, seg1 6, seg2 2(16 tq, 샘플 포인트 87.5%)
  - 데이터: 분주 2, prop 4, seg1 4, seg2 3(12 tq, 75%)
  - 이 값으로 S32K344 디버그 노드와 확인했다.
- **CIS 브랜치의 `s32ds/cis_node.mex`(새 프로젝트용)는 값이 다르다:** 공칭 81.25%, 데이터 79%. **버스의 모든 노드가 같은 비트 타이밍을 쓰게 하나로 정한다.**
- **두 보드만 이은 짧은 버스 기준이다.** 데이터 단계 TDC 설정이 없다. 노드가 늘거나 버스가 길어지면 다시 본다.

### 4.7 인터럽트와 시간

- **IntCtrl에 등록할 핸들러:**
  - `LPUART1_RxTx_IRQn` → `LPUART_UART_IP_1_IRQHandler`(우선순위 5)
  - `CAN0_ORed_0_15_MB_IRQn`, `CAN0_ORed_IRQn`, `CAN0_Error_IRQn` → RTD 핸들러(우선순위 6)
- **`SysTick_Handler`는 `net_board_time144.c`만 정의한다.** 시작 코드의 weak 정의는 덮인다.
  - `OsIf_Init()`을 부르지 않는다. OsIf가 SysTick을 다시 설정한다.
  - RTOS처럼 SysTick을 따로 쓰는 환경이면 시간원(`Board_*` hook)을 다시 정해야 한다.
- **한 루프 시간 예산:** DHT22 읽기는 인터럽트를 약 7 ms 끈다(`src/cis/dht22.c`). 그동안 CAN과 UART 수신은 Network 링이 버틴다(UART 256 B ≈ 22 ms). `Net_MainFunction` 1회는 15 ms 미만이어야 한다. 루프에 오래 걸리는 작업을 더하지 않는다.

### 4.8 BOOT_ID와 상대 노드

- **BOOT_ID는 부팅마다 바뀐다.** 부팅 때 코어 사이클 카운터(DWT_CYCCNT)로 만든다. 리셋 버튼을 누를 때마다 바뀌지만, 전원을 뺐다 꽂으면 같은 값이 나올 수 있다.
  - 고정값이 필요하면 `NET_BOARD_BOOT_ID_FIXED`를 정의한다(0x43495331 "CIS1").
  - 제품용 출처는 아직 정하지 않았다.
- **상대(DOMAIN)를 리셋해 같은 BOOT로 순번이 1부터 다시 오면 CIS는 그 0x080을 옛 프레임으로 버린다.** 이때 후방은 INACTIVE와 COMMUNICATION_FAULT에 머문다. S32K344 디버그 노드에서는 `tx boot new`를 먼저 친다.

### 4.9 디버거에서 볼 값

- **`g_net_diag`는 이 빌드에 없다.** 시험대 앱의 변수였다. CIS 브랜치 `s32ds/main.c` 주석과 `NETWORK_INTEGRATION.md`에는 아직 남아 있다.
- 대신 아래 식으로 본다.

| 식 | 내용 |
|---|---|
| `cis_node_time_ok` | FTM0 1 MHz 시작 여부 |
| `'net_entry.c'::entry.main_max_us` | `Net_MainFunction` 1회 최대 시간 |
| `net_hal_rtd_core.diag` | CAN·UART 백엔드 진단 |
| `'net_node.c'::service` | UART-C 수신 조립 |
| `g_dht_raw`, `g_dht_ok`, `g_dht_checksum`, `g_dht_timeout` | DHT22 원시 바이트와 성공·실패 수 |

## 5. 확인하지 않은 것과 남은 점

- **결함:** `ISSUES.md`(CIS-1 DHT22 76 °C).
- **Pi 연동:** 노트북 Pi 도구(pi_bench, COM3)로 UART-C 송수신과 CAN 중계 확인, 실제 Raspberry Pi와 pi_vision 스크립트는 미확인.
- **DHT22 값:** 보드 확인 때 76.30 °C, 7.30 %RH가 VALID로 나왔다. 체크섬은 통과했으므로 센서 발열(배선)이나 센서 종류부터 확인한다(ISSUES.md CIS-1).
- **후방 유효 범위:** 10~100 cm는 후보값이다(`src/cis/rear_logic.h:20-21`). 10 cm보다 가까우면 OUT_OF_RANGE / UNAVAILABLE이 나온다.
- **새 프로젝트로 만들 때(1-B):** CIS 브랜치 방식으로 만든 프로젝트는 보드에서 확인하지 않았다. 확인한 것은 디버그 노드 복제본 기반의 이 `.mex`다.

## 6. 다시 만들기와 확인

이 브랜치에는 원본이 없다. 원본을 따로 체크아웃하고, 도구는 원본 체크아웃의 것으로 실행하며, 대상만 이 브랜치의 체크아웃(`<통합 체크아웃>`)을 가리킨다.

- **Network 부분:** Network 원본은 `feat/vss-network-node` 브랜치의 `firmware/network`(`02051db`)다. 그 체크아웃(`<Network 체크아웃>`)의 도구로 실행한다. 출력 폴더 이름은 `network`여야 한다.

  ```
  python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node CIS --check <통합 체크아웃>/firmware/integration/CIS/src/network
  python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node CIS --out <통합 체크아웃>/firmware/integration/CIS/src/network --apply
  ```

  - `--check`의 마지막 줄 결과:
    - `CHECK CIS OK`: 같다
    - `CHECK CIS STALE`: 원본이 바뀌었다. 다시 만든다
    - `CHECK CIS EDITED`: 누가 손으로 고쳤다. 원본에서 고치고 다시 만든다
  - `--apply`는 Network 체크아웃이 커밋된 상태(clean)에서만 되고, 그 커밋이 `NETWORK_BUNDLE.txt`에 기록된다.
- **CIS 부분:** CIS 앱 원본은 `feat/cis-rtd-network` 브랜치의 `firmware/CIS`(`129ef58`)다. 그 체크아웃(`<CIS 체크아웃>`)에서 `<CIS 체크아웃>/firmware/CIS/src/`(`main.c`, `pi_uart.c`, `net_api_shim.h` 제외)를 `<통합 체크아웃>/firmware/integration/CIS/src/cis/`로, `<CIS 체크아웃>/firmware/CIS/s32ds/main.c`를 `.../CIS/src/main.c`로 복사한다.
- **시계 파일:** `<Network 체크아웃>/firmware/network/nodes/cis/bench/s32k144_cis/net_board_time144.c/.h`를 `<통합 체크아웃>/firmware/integration/CIS/src/net_bench/`로 복사한다.
- **다시 만든 뒤:** `SOURCES.txt`의 커밋과 해시를 갱신한다.
