# CIS 앱을 Network 시험 프로젝트(S32K144)에 붙이기

Network 담당자가 만든 S32K144 CIS 시험대(`nodes/cis/bench/s32k144_cis`, 보드 시험 통과: `docs/BOARD_TEST_RESULTS.md`)를
그대로 쓰고, 시험대 앱(`cis_bench_app.c`) 자리에 이 CIS 앱을 넣는다. 담당자 파일은 고치지 않는다.

## 구성

| 맡는 쪽 | 내용 |
|---|---|
| Network (담당자) | CAN FD(FlexCAN0)·UART-C(LPUART1)·시계(SysTick 1 kHz, 코어 FIRC 48 MHz)·`Board_*` hook·`BOOT_ID`. 복제본 프로젝트 `network_144_net_cis`의 RTD 설정 |
| CIS (우리) | 센서 3종(HC-SR04, DHT22, VEML7700), Pi 판정(`vision_logic`), 후방 허용(`power_gate`), `CisNet_Publish*` 제출, `CisApp_On*` 수신 |

우리 센서 드라이버는 레지스터를 직접 만진다(RTD API 없음). 핀 먹스는 RTD Port 초기화가 끝난 뒤 드라이버가 다시 설정한다.

## 1. 프로젝트 만들기와 소스 복사

1. 담당자의 144 디버그 노드 프로젝트를 복제한다(예: `network_144_net_cis`). 원본 `network_144_can_debuger`는 읽기 전용이다.
2. 담당자 브랜치(`feat/vss-network-node`)를 별도 폴더로 체크아웃한다(읽기 전용).
3. 복사:
   ```
   python firmware/CIS/tools/sync_cis_to_s32ds.py --network <NET_git>/firmware/network --project <network_144_net_cis>          # dry run
   python firmware/CIS/tools/sync_cis_to_s32ds.py --network <NET_git>/firmware/network --project <network_144_net_cis> --apply
   ```
   - 담당자 도구(`--board cis144`)가 Network 소스를 `src/network/`, 시계 파일을 `src/net_bench/`에 넣는다.
   - 시험대 앱 `cis_bench_app.c/.h`는 이 스크립트가 지운다(우리 `CisApp_On*`와 이름이 겹친다).
   - 우리 소스는 `src/cis/`로 들어간다(`main.c`, `pi_uart.c`, `net_api_shim.h` 제외).
   - 도구는 Python 3로 돈다. Windows의 `python`이 Microsoft Store 안내로 이어지면 Python을 먼저 설치한다.

### 1-B. 디버그 노드 프로젝트가 없을 때: 새 RTD 프로젝트에 직접 설정

담당자 프로젝트 대신 S32DS 새 프로젝트(S32K144, RTD 3.0.0, 기본 Clock/Port/OsIf)에 아래를 넣는다. 2026-10-29 `newcis`에서 이렇게 만들었다.

저장소에 있는 것:

| 파일 | 내용 |
|---|---|
| `tools/add_network_to_mex.py` | 새 프로젝트 `.mex`에 아래 핀·FlexCAN·Lpuart_Uart·IntCtrl_Ip를 넣는다. 코드 생성 명령도 이 파일 머리말에 있다 |
| `tools/add_network_to_cproject.py` | 2절의 include 경로와 define을 Debug_FLASH에 넣는다 |
| `s32ds/cis_node.mex` | 위 도구를 적용한 `newcis`의 `.mex`(결과 확인용, ConfigTools로 열어 볼 수 있다) |
| `s32ds/main.c` | 3절의 `main()`. 프로젝트 `src/main.c`로 복사한다 |

순서: 새 프로젝트 → `add_network_to_mex.py --apply` → 코드 생성(Update Code) → 아래 RTD 드라이버 복사 → 1절 동기화 → `add_network_to_cproject.py --apply` → `s32ds/main.c` 복사 → 빌드.
OpenSDA(PEMicro)로 디버그하려면 디버그 설정을 PEMicro로 만든다(새 프로젝트의 기본은 S32 Debug Probe다).

- 클럭: 기본 설정 그대로(코어 48 MHz, FLEXCAN0 48 MHz, LPUART1 8 MHz, SOSCDIV1/2 8 MHz).
- Pins: CAN0 RX PTE4(9번 핀), TX PTE5(8), LPUART1 RX PTC6(81), TX PTC7(80). LQFP100 번호다.
- Peripherals:

  | 컴포넌트 | 설정 |
  |---|---|
  | FlexCAN | `FLEXCAN_0`, `FlexCAN_Config0`, `max_num_mb` 7, RX FIFO 없음, 일반 모드, FD·ISO·BRS 켬, payload 64 B, 자동 bus-off 복구 켬, PE 48 MHz. 공칭 500 kbit/s: 분주 6, prop 7, seg1 5, seg2 3(16 tq, 81.25 %). 데이터 2 Mbit/s: 분주 1, prop 14, seg1 4, seg2 5(24 tq, 79 %). 콜백 `DebugCan_EventCallback` / `DebugCan_ErrorCallback` |
  | Lpuart_Uart | `LPUART_1`, 115200(실제 115942), 인터럽트, 콜백 사용 `DebugConsole_UartCallback` → `Lpuart_Uart_Ip_xHwConfigPB_1` |
  | IntCtrl_Ip | `CAN0_ORed_0_15_MB_IRQn`, `CAN0_ORed_IRQn`, `CAN0_Error_IRQn`, `LPUART1_RxTx_IRQn` (핸들러는 RTD 이름: `CAN0_ORED_0_15_MB_IRQHandler`, `CAN0_ORED_IRQHandler`, `CAN0_Error_IRQHandler`, `LPUART_UART_IP_1_IRQHandler`) |

  콜백 이름을 디버그 노드와 같게 두었으므로 2절 define 11개를 그대로 쓴다.
- 프로젝트 `RTD/`에 드라이버를 넣는다(RTD 설치 폴더의 플러그인에서): `FlexCAN_Ip*.c/.h`, `Lpuart_Uart_Ip*.c/.h`, `IntCtrl_Ip*.c/.h`, `SchM_Can_43_FLEXCAN`, `SchM_Uart`, `SchM_Platform`.
- 데이터 단계 2 Mbit/s는 두 보드만 이은 짧은 버스 기준이다(TDC 설정 없음). 다른 노드와 샘플 포인트가 다르면 이 값을 맞춘다.

## 2. `.cproject` (Debug_FLASH C 컴파일러)

담당자 시험대와 같은 include 경로 7개와 define 11개에, 우리 폴더만 더한다.

include 경로에 추가(`src/net_bench`는 `cis_node.c`와 `main.c`가 `net_board_time144.h`를 찾는 데 필요하다):
```
${ProjDirPath}/src/net_bench
${ProjDirPath}/src/cis
```

define에 추가:
```
CIS_USE_NETWORK_API
```
(나머지 11개는 `nodes/cis/bench/s32k144_cis/README.md` 그대로: `NETWORK_UPPER_NODE_PROFILE=1`, `NET_NODE_BUILD`, `NET_BOARD_*_HOOK`,
FlexCAN·LPUART 콜백 이름, `NET_BOARD_UART_C_INSTANCE=1U` 등. 생성 설정과 이름이 다르면 그 define 값만 맞춘다.)

`CIS_USE_NETWORK_API`를 켜면 우리 쪽 UART 구동과 `P_PERMISSION` 송신, 벤치 보고(0x70)가 꺼지고 Network 스택이 LPUART1을 쓴다.

## 3. `main()` 수정 (프로젝트 쪽)

담당자 시험대의 `NET_NODE_BUILD` 경로에서 시험대 앱 호출만 바꾼다(담당자 `main.c`는 저장소에 복사하지 않는다).
1-B처럼 새 프로젝트로 만들면 `s32ds/main.c`를 그대로 쓴다.

```c
#include "net_board_time144.h"
#include "cis_node.h"

(void)Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);
(void)Port_Ci_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
                           g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);
(void)IntCtrl_Ip_Init(&IntCtrlConfig_0);   /* CAN0·LPUART1 인터럽트 핸들러 설치 */
Board_TimeInit();
(void)CisNode_Init();          /* 센서 시작 + Net_Init() ; CisBench_Init() 자리 */
for (;;) {
    CisNode_Service();         /* Net_MainFunction + CIS 기능 + CisNet_Publish* ; CisBench_Service() 자리 */
}
```

`Net_Init()`은 `CisNode_Init()` 안에서 부른다(담당자 순서와 같다: `Board_TimeInit` 다음).

## 4. 확인할 것 (S32DS)

- 컴파일/링크: `src/cis`의 17개 `.c`와 `src/network`가 같이 빌드되는지. 이름이 겹치면 로그를 보고 맞춘다.
  - 우리 `regs_s32k144.h`의 매크로 이름이 RTD/디바이스 헤더와 겹치면 에러가 난다.
  - `SysTick_Handler`는 `net_board_time144.c`에만 있어야 한다(우리 코드에는 없음).
- SOSC: 클럭 설정이 이미 SOSC를 켜 두었으면 `timebase_init()`은 꺼진 분주기만 켠다(SOSCDIV1·SOSCDIV2가 0이면 1로). 켜져 있는 분주기는 다른 주변장치가 쓸 수 있어 바꾸지 않는다.
  - SOSCDIV1은 FTM0(센서 1 MHz 시각), SOSCDIV2는 LPI2C0(VEML7700)이 쓴다.
  - `CisNode_Init()`이 `false`면 FTM0 1 MHz가 없다는 뜻이다: SOSC가 안 켜졌거나 SOSCDIV1이 1분주가 아니다. 클럭 설정의 SOSCDIV1을 1분주로 맞춘다.
  - SOSCDIV2가 1분주가 아니면 I2C가 100 kHz보다 느려질 뿐 동작은 한다(8 MHz 기준 설정).
- 핀: HC-SR04 TRIG PTE7 / ECHO PTE8, DHT22 PTA14, VEML7700 SDA PTA2 / SCL PTA3. 디버그 노드 Port 설정이 이 핀을 다른 용도로 쓰고 있지 않은지 확인한다.
- 시간 예산: `g_net_diag.main_max_us`(Net 1회 최대)가 15 ms 미만이고, DHT22 읽기(약 7 ms: 시작 2 ms + 40비트)와 겹쳐도 UART 링(256 B, 약 22 ms) 안인지.
  HC-SR04는 기다리지 않는다(상태 기계). VEML7700은 정상이면 짧지만, 센서가 빠지면 I2C 대기 한도(5 ms)가 거래마다 걸린다. 센서를 뺀 채로도 `main_max_us`와 `uart_isr_ring_overflow`를 한 번 본다.
  DHT22 읽기 중 인터럽트는 막지 않는다. 비트 시각이 흐트러지면 체크섬이 틀려 그 회차만 실패로 세고(연속 3회 실패해야 고장), 2초 뒤 다시 읽는다.

## 5. 달라지는 점

- `BOOT_ID`, 시계, 복사 도구는 담당자 것을 쓴다. 이 저장소의 `firmware/CIS/board/`와 옛 복사 스크립트는 없앴다.
- Pi 쪽(`tools/pi_vision`)은 그대로다. 담당자의 `tools/pi_bench`는 노트북 시험 도구이고, 실제 Pi는 우리 스크립트가 맡는다. 프레임 형식(29/26/25 B)은 같다.
