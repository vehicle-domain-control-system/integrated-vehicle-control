# BCM 노드 통합 폴더 (S32K144, Network BCM 프로필)

BCM 노드 하나를 빌드하는 데 필요한 소스를 모은 폴더다. 구성은 Network 스택(CAN FD), BCM 앱(도어·공조·조명), 시험대 포트와 진입, 보드 시계·hook, `main()`이다. 실제 하드웨어는 실내등(앰비언트 RGB, FTM0 PWM)뿐이고, 도어 잠금 구동과 팬은 모사, 도어 접점은 "닫힘", 방열부 온도는 25.0 °C로 고정이다.

`src/` 구조는 보드에서 확인한 S32DS 프로젝트(`network_144_bcm_app`)의 `src/`와 같다(디버그 노드 파일과 Network의 다른 노드 전용 파일만 뺐다, 2절). 다른 IDE에서는 이 `src/`를 프로젝트 소스로 넣고 4절의 설정을 옮기면 된다.

- **보드 확인(2026-10-29):** S32K144 평가 보드에서 확인했다. 상대는 S32K344 디버그 노드(DOMAIN 역할)였다.
  - 기동: BCM_STATUS가 READY, 고장 없음으로 나왔다. BOOT_ID는 리셋할 때마다 바뀌었다.
  - 상태 송신: BCM_DOOR_STATE가 33.4초에 334개(100 ms)였다. UPDATE_SEQUENCE와 TX_SEQUENCE가 함께 올랐다.
  - 도어(모사): UNLOCK → DONE·UNLOCK_COMPLETED·발생 번호 1, 같은 명령 다시 → DONE·ALREADY_AT_TARGET·2, LOCK → DONE·LOCK_COMPLETED·3. BCM_DOOR_STATE의 잠금 상태가 명령대로 바뀌었다.
  - 공조(모사): 팬 2 → DONE, BCM_CLIMATE_STATE 지시·실측 모두 2.
  - 조명: 밝기 20 %·빨강에서 LED가 켜졌다. 밝기 100 %는 DONE으로 보고했지만 LED가 켜지지 않았다(ISSUES.md BCM-1, BCM-2).
  - 결과(BCM_RESULT)는 원 명령의 DOMAIN BOOT와 COMMAND_ID를 그대로 돌려주었고, TX_SEQUENCE가 결과 수와 맞았다.
- **빌드 확인:** 보드에 올린 S32DS 빌드는 text 136,852 B, data 368 B, bss 30,368 B다. 같은 컴파일러·플래그로 따로 빌드했을 때 컴파일 123개, 경고 0이었다. 이 폴더로 만든 이미지와 보드 이미지의 바이트 비교는 하지 않았다.

## 1. 구성

| 경로 | 내용 | 관리 | 출처 |
|---|---|---|---|
| `src/main.c` | Clock → Port → IntCtrl 초기화, `Board_TimeInit()` → `BcmNode_Init()`, 루프 `BcmNode_Service()` | 통합 | S32DS 프로젝트(144 디버그 노드 복제본)의 `src/main.c` |
| `src/bcm/bcm/` (17개) | BCM 기능 로직(도어·공조·조명·진단·결과, DOMAIN 생존 판정). 하드웨어를 직접 다루지 않는다 | BCM | BCM 원본 `src/bcm/` |
| `src/bcm/port/bcm_sim.c/.h` | 도어 잠금 구동·팬 모사 | BCM | BCM 원본 `src/port/` |
| `src/bcm/` (7개) | `bcm_node`(진입), `bcm_net_adapter`(Network 진입 감싸기, `g_net_diag`), `bcm_port_bench144`(원본 `bcm_port_rtd.c`에서 SysTick·WDOG·부팅 카운터를 뺀 포트), `bcm_port_config.h` | 통합 | BCM 통합 작업 폴더 `bcm_net/src/` |
| `src/net_bench/net_board_time144.c/.h` | SysTick 1 kHz 시계, `Board_*` hook, BOOT_ID | Network | `feat/vss-network-node` `firmware/network/nodes/bcm/bench/s32k144_bcm/` |
| `src/network/` (94개) | Network 스택의 BCM 프로필 생성 사본(`sync_to_s32ds.py --node BCM`). `README.md`는 Network의 BCM 통합 메모, `NETWORK_BUNDLE.txt`는 출처와 해시 기록 | Network | `feat/vss-network-node` `firmware/network/` |
| `s32ds/bcm144_board_tested.mex` | 보드에서 확인한 S32DS 프로젝트의 ConfigTools 설정(핀, 클럭, FlexCAN, LPUART, IntCtrl, Ftm_Pwm). 원래 이름은 `network_144_can_debuger.mex` | 통합 | S32DS 프로젝트 |
| `SOURCES.txt` | 출처와 파일별 SHA-256(CRLF를 LF로 읽은 값) | 통합 | |
| `ISSUES.md` | 보드 시험에서 찾은 결함 | 통합 | |

출처는 다음과 같다.
- Network: `feat/vss-network-node` `02051db`
- BCM 원본과 `bcm_net`: git 저장소가 아니다. 판은 `SOURCES.txt`의 해시로 가린다.

**이 폴더의 파일은 직접 고치지 않는다.** 원본에서 고친 뒤 다시 만든다(6절). 알려진 결함도 이 폴더에서 고치지 않고 `ISSUES.md`에 남긴다.

## 2. 정리해서 뺀 것

| 뺀 것 | 이유 |
|---|---|
| 디버그 노드 파일 `debug_*.c/.h` | `NET_NODE_BUILD`에서 내용이 모두 빠져 빈 오브젝트가 된다 |
| 시험대 앱 `bcm_bench_app.c/.h` | 실제 BCM 앱으로 바뀌었다. 함께 넣으면 `BcmApp_OnDomain*`, `g_net_diag` 정의가 겹친다 |
| BCM 원본 `main.c`, `port/bcm_port_rtd.c`, `port/bcm_runtime_rtd.c`, `port/bcm_net_board.*`, `port/bcm_port_config.h`, `port/bcm_boot_flash_ftfc.c`, `infra/*` | 원본 보드 프로젝트용이다. SysTick과 BOOT_ID는 시험대 시계가, 루프는 `bcm_node.c`가 맡는다. WDOG와 D-Flash 부팅 카운터는 쓰지 않는다 |
| Network의 CIS·VSS·DOMAIN 전용 파일 11개(`.c` 4개, 헤더 7개) | BCM 프로필 빌드에서 `.c` 4개가 0 B였다. `--node BCM` 사본에는 처음부터 들어가지 않는다 |
| NXP RTD 드라이버, 생성 설정(`RTD/`, `generate/`, `board/`), `Project_Settings/`(시작 코드, 링커 스크립트) | NXP 파일이다. 저장소에 올리지 않고 각 IDE에서 RTD 설치본과 ConfigTools로 만든다(4.1절) |
| 빌드 산출물(`Debug_FLASH/`), `.cproject`, `.project` | IDE 전용이다. 필요한 값은 4절에 적었다 |
| 복사 도구 `sync_bcm_to_s32ds.py` | S32DS 프로젝트 전용이다. `bcm_net/tools/`에 있다 |

## 3. 빌드 결과(보드 확인 빌드 기준)

- **Flash:** text 136,852 B. 전체 512 KB.
- **RAM:** `.sram_data` 368 B, `.sram_bss` 27,024 B. int_sram 57,088 B에서 벡터 사본 2,576 B와 heap 512 B까지 빼면 26,608 B가 남는다. 스택 4 KB는 따로 잡힌다.
  - BCM 쪽 RAM 약 7.3 KB 가운데 6 KB는 `NetNodeDiag`(2,056 B) 세 벌이다(`g_net_diag`, 어댑터 사본, `bcm_netlink.c` 사본).
- **스택:** 정적 호출 그래프(`-fstack-usage -fcallgraph-info=su`)로 main 경로 최대 1,960 B, `BcmApp_Step` 경로 1,108 B, CAN 메시지 버퍼 ISR 232 B였다. 함수 포인터로 부르는 앱 콜백과 예외 프레임을 더해 약 2.5 KB로 추정한다.

## 4. 다른 IDE·프로젝트에 적용할 때

### 4.1 RTD와 생성 설정

- **RTD 버전:** S32K1 RTD 3.0.0(`S32K1_RTD_3_0_0_QLP06_D2603`)을 쓴다.
- **빌드에 넣는 RTD 모듈:** `Clock_Ip*`, `Port_Ci_Port_Ip*`, `IntCtrl_Ip*`, `FlexCAN_Ip*`, `Ftm_Pwm_Ip`, `Ftm_Pwm_Ip_Irq`, `Lpuart_Uart_Ip*`(IntCtrl 설정이 LPUART 핸들러를 참조한다), `OsIf*`(Interrupts, Software_Semaphore, Timer, Timer_System, Timer_System_Internal_Systick), `Det`, `SchM_Can_43_FLEXCAN`, `SchM_Mcu`, `SchM_Port`, `SchM_Pwm`, `SchM_Uart`.
  - `Ftm_Pwm_Ip*`는 RTD 설치본의 `Pwm_TS_T40D2M30I0R0`, `SchM_Pwm`은 `Rte_TS_T40D2M30I0R0`에 있다. AUTOSAR Pwm 계층(`Pwm.c`, `Pwm_Ipw*`)과 `Flexio_Pwm_*`는 넣지 않는다.
- **생성 설정:** S32 Config Tools로 `s32ds/bcm144_board_tested.mex`를 열고 코드를 생성한다.
  - 생성되는 파일: `Clock_Ip_Cfg`, `Port_Ci_Port_Ip_Cfg`, `FlexCAN_Ip_Sa_PBcfg`, `Lpuart_Uart_Ip_Sa_PBcfg`, `IntCtrl_Ip_Cfg`, `OsIf_Cfg`, `Ftm_Pwm_Ip_Cfg`·`Ftm_Pwm_Ip_PBcfg`.
- **이름을 맞춰야 하는 것:** 생성 설정에서 이름이 다르면 define 값만 맞춘다(Network 소스는 고치지 않는다).
  - **CAN 설정:** `FlexCAN_Config0`, 상태 `FlexCAN_State0`, 인스턴스 0, `max_num_mb` 7, FD payload 64 B. Network 기본값과 같아 define이 필요 없다.
  - **CAN 콜백 두 개:** `DebugCan_EventCallback`, `DebugCan_ErrorCallback`. 생성 설정이 부르는 이름을 define으로 넘기면 Network HAL이 그 이름의 함수를 정의한다. 이름은 디버그 노드에서 물려받은 것이다.
  - **PWM 설정:** `Ftm_Pwm_Ip_UserCfg0`(`bcm_node.c`가 `Ftm_Pwm_Ip_Init(0, &Ftm_Pwm_Ip_UserCfg0)`로 부른다). `.mex`의 `POST_BUILD_VARIANT_USED`가 true면 이름이 바뀐다.

### 4.2 컴파일러와 플래그

보드 확인 빌드와 같은 값을 쓴다. 컴파일러는 GCC 10.2 arm-none-eabi(S32DS 3.6.6 동봉)다.

```
-std=c99 -Os -mcpu=cortex-m4 -mthumb -mlittle-endian -mfloat-abi=hard -mfpu=fpv4-sp-d16
-funsigned-char -fno-short-enums -funsigned-bitfields -fno-common -fomit-frame-pointer
-Wall -Wextra -pedantic -Wstrict-prototypes -Wsign-compare -Wundef -Wdouble-promotion
-Werror=implicit-function-declaration
링크: -nostartfiles --entry=Reset_Handler -specs=nano.specs -specs=nosys.specs -lc -lm -lgcc
```

- `-fno-short-enums`와 `-funsigned-char`를 바꾸지 않는다.
- 링크할 때 `libc_nano.a ... uses variable-size enums` 경고가 네 줄 나온다. 툴체인 라이브러리 쪽 경고다.
- **링커 스크립트:** RTD 섹션을 배치하는 S32K144 flash 스크립트가 필요하다. S32DS 프로젝트의 `linker_flash_s32k144.ld`를 그대로 쓰거나 그 섹션 배치를 옮긴다.
- **GCC 전용 문법:** Network의 `src/network/src/platform/rtd/net_board_cfg.h:314-319`(`__asm volatile` PRIMASK), `src/network/src/l04_frame_codec/net_wire_common.c:21`(`__attribute__((may_alias))`). BCM 쪽 코드에는 없다.

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
src/bcm
```

여기에 RTD include와 생성 설정 include를 더한다. S32DS 기준으로는 `generate/include`, `RTD/include`, `board`, PlatformSDK의 `BaseNXP .../header`·`include`, `Platform .../include`·`startup/include`다.

define 13개:

```
CPU_S32K144HFT0VLLT  I_CACHE_ENABLE  ENABLE_FPU  GCC  CPU_CORTEX_M4F       (S32DS 기본 5개)
NETWORK_UPPER_NODE_PROFILE=3
NET_NODE_BUILD
NET_BOARD_ISR_TICK_HOOK=Board_TickMs32
NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32
NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig
NET_BOARD_US_HOOK=Board_Us32
NET_BOARD_CAN_CALLBACK=DebugCan_EventCallback
NET_BOARD_CAN_ERROR_CALLBACK=DebugCan_ErrorCallback
```

`NETWORK_NODE_PLATFORM`은 넣지 않는다. MCU는 `CPU_S32K144HFT0VLLT`로 정해진다.

### 4.4 클럭

| 항목 | 값 | 바꿀 때 같이 바꿀 것 |
|---|---|---|
| 코어 | FIRC 48 MHz | `src/net_bench/net_board_time144.h:28` `BOARD_CORE_CLK_HZ`(SysTick 1 ms와 µs 계산) |
| FlexCAN0 PE | 48 MHz | CAN 비트 타이밍(4.6절) |
| FTM0(실내등) | 시스템 클럭 48 MHz, 1분주, 주기 8000 tick(6 kHz) | `src/bcm/bcm_port_config.h`의 duty 기준(ISSUES.md BCM-1) |

SOSC는 `.mex`에서 켜져 있지만 BCM은 쓰지 않는다.

### 4.5 핀과 주변장치

- **RTD Port 초기화로 잡는 핀:** CAN0 RX PTE4·TX PTE5(ALT5), 실내등 R PTD15·G PTD16·B PTD0 → FTM0 CH0/1/2(ALT2). 세 채널은 High_true_pulse, 극성 Channel_active_LOW, 초기 출력 HIGH, 초기 duty 0이다.
- `.mex`에 LPUART1 PTC6·PTC7과 LPUART0 PTB0·PTB1이 남아 있다. 디버그 노드에서 물려받은 설정이고, BCM 빌드에서는 초기화하지 않는다.
- 도어 접점(PTC12)과 방열부 ADC(PTC14)는 쓰지 않는다(`bcm_port_config.h`의 `BCM_PORT_HAS_DOOR_CONTACT 0`, `BCM_PORT_HAS_HEATSINK_ADC 0`).

### 4.6 CAN

- **보드 확인 값:** CAN FD ISO, BRS, 500 kbit/s / 2 Mbit/s, 자동 bus-off 복구 켬.
  - 공칭: 분주 6, prop 7, seg1 6, seg2 2, SJW 2(16 tq, 샘플 포인트 87.5%)
  - 데이터: 분주 2, prop 4, seg1 4, seg2 3, SJW 2(12 tq, 75%)
  - `firmware/integration/CIS/s32ds/cis144_board_tested.mex`와 FlexCAN 설정 47개 항목이 모두 같다. 이 값으로 S32K344 디버그 노드와 확인했다.
- **두 보드만 이은 짧은 버스 기준이다.** 데이터 단계 TDC 설정이 없다.

### 4.7 인터럽트와 시간

- **IntCtrl에 등록하는 핸들러:** `CAN0_ORed_0_15_MB_IRQn`, `CAN0_ORed_IRQn`, `CAN0_Error_IRQn` → RTD 핸들러(우선순위 6). `LPUART0/1_RxTx_IRQn`(우선순위 5)도 등록돼 있지만 쓰지 않는다. FTM 인터럽트는 쓰지 않는다.
- **`SysTick_Handler`는 `net_board_time144.c`만 정의한다.** 시작 코드의 weak 정의는 덮인다. `OsIf_Init()`을 부르지 않는다.
- **WDOG:** 시작 코드가 끈 상태 그대로 둔다.
- **루프:** `BcmNode_Service()`는 ms가 바뀐 바퀴에만 `Net_MainFunction` → `BcmApp_Step` → `Net_TxFlush`를 돈다. `Net_MainFunction` 1회는 15 ms 미만이어야 한다.

### 4.8 BOOT_ID와 상대 노드

- **BOOT_ID는 부팅마다 바뀐다.** 부팅 때 코어 사이클 카운터(DWT_CYCCNT)로 만든다. 리셋 버튼을 누를 때마다 바뀌지만, 전원을 뺐다 꽂으면 같은 값이 나올 수 있다. 고정값이 필요하면 `NET_BOARD_BOOT_ID_FIXED`를 정의한다(0x42434D31 "BCM1").
- **DOMAIN은 명령의 TARGET_BOOT_ID에 BCM의 현재 BOOT_ID를 넣어야 한다.** 다르면 Network가 명령을 앱에 넘기지 않는다. BCM을 리셋한 뒤에는 BCM_STATUS의 BOOT_ID로 다시 맞춘다.
- **상대(DOMAIN)를 리셋해 같은 BOOT로 순번이 1부터 다시 오면 BCM은 그 프레임을 옛 실행으로 버린다.** S32K344 디버그 노드에서는 `tx boot new`를 먼저 친다.

### 4.9 디버거에서 볼 값

| 식 | 내용 |
|---|---|
| `g_bcm_node` | 루프 실행 수, `loop_us_last/max`(루프 1회), `diag_us_last/max`(`NetNode_GetDiag` 1회), `boot_id`, `app`(명령 전달·만료·무효·인계 가득·outbox 버림·제출 거절 수) |
| `g_net_diag` | 100 ms마다 갱신한 `NetNode_GetDiag()`. `entry_init_result`, `stage`, `permission.reason`, `app.command_wrong_target`, 슬롯 `result_tx`·`door_state_tx` 등, `rtd.*`, `main_max_us` |

## 5. 확인하지 않은 것과 남은 점

- **결함:** `ISSUES.md`(BCM-1 실내등 duty 단위, BCM-2 실내등 적용 결과).
- **DOMAIN_PERMISSION 끊김:** 시험 내내 상대가 0x080을 50 ms마다 보내고 있었다. 끊겼을 때의 STALE 거절, COMM_TIMEOUT 고장(DEGRADED), 조명 꺼짐은 보드에서 확인하지 않았다.
- **밝기·색:** 밝기별 LED 관찰과 빨강 외의 색은 기록하지 않았다(BCM-1).
- **시간 예산:** `g_bcm_node.loop_us_max`, `diag_us_max`, `g_net_diag.main_max_us`를 보드에서 읽지 않았다.
- **BCM_EVENT:** 모사 값이 고정이라 과열·팬 불일치 발생이 생기지 않는다. 보드에서 본 적이 없다.
- **실제 도어·온도 하드웨어:** 쓰지 않는다. 쓰려면 `bcm_port_config.h`의 `BCM_PORT_HAS_*`와 `.mex`(핀, ADC)를 함께 바꾼다.

## 6. 다시 만들기와 확인

이 브랜치에는 원본이 없다. 원본을 따로 체크아웃하고, 도구는 원본 체크아웃의 것으로 실행하며, 대상만 이 브랜치의 체크아웃(`<통합 체크아웃>`)을 가리킨다.

- **Network 부분:** Network 원본은 `feat/vss-network-node` 브랜치의 `firmware/network`(`02051db`)다. 그 체크아웃(`<Network 체크아웃>`)의 도구로 실행한다. 출력 폴더 이름은 `network`여야 한다.

  ```
  python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node BCM --check <통합 체크아웃>/firmware/integration/BCM/src/network
  python <Network 체크아웃>/firmware/network/tools/sync_to_s32ds.py --node BCM --out <통합 체크아웃>/firmware/integration/BCM/src/network --apply
  ```

  - `--check`의 마지막 줄 결과:
    - `CHECK BCM OK`: 같다
    - `CHECK BCM STALE`: 원본이 바뀌었다. 다시 만든다
    - `CHECK BCM EDITED`: 누가 손으로 고쳤다. 원본에서 고치고 다시 만든다
  - `--apply`는 Network 체크아웃이 커밋된 상태(clean)에서만 되고, 그 커밋이 `NETWORK_BUNDLE.txt`에 기록된다.
- **BCM 부분:** BCM 앱 원본은 받은 BCM 폴더(`<BCM 폴더>`, git 저장소가 아님)와 BCM 통합 작업 폴더(`<bcm_net>`, git 저장소가 아님)다. `<BCM 폴더>/src/bcm/*.c|h`를 `<통합 체크아웃>/firmware/integration/BCM/src/bcm/bcm/`로, `<BCM 폴더>/src/port/bcm_sim.c|h`를 `.../BCM/src/bcm/port/`로, `<bcm_net>/src/*.c|h`를 `.../BCM/src/bcm/`로 복사한다. 판은 `SOURCES.txt`의 해시로 가린다.
- **시계 파일:** `<Network 체크아웃>/firmware/network/nodes/bcm/bench/s32k144_bcm/net_board_time144.c/.h`를 `<통합 체크아웃>/firmware/integration/BCM/src/net_bench/`로 복사한다.
- **main.c:** 144 디버그 노드 복제본의 `src/main.c`에서 `NET_NODE_BUILD` 경로의 시험대 호출 세 곳을 바꾼 것이다(`bcm_bench_app.h` → `bcm_node.h`, `Net_Init(); BcmBench_Init();` → `BcmNode_Init();`, `Net_MainFunction(Board_NowMs32()); BcmBench_Service();` → `BcmNode_Service();`).
- **다시 만든 뒤:** `SOURCES.txt`의 해시를 갱신한다.
