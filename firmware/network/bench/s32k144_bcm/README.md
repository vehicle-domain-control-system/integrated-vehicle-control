# S32K144 보드 시험대 파일 (BCM 프로필, CAN만)

Network 제품 코드가 아니라 S32K144 보드 시험대 코드다. 144 디버그 노드 프로젝트(`network_144_can_debuger`)를 S32DS에서 복사한 복제본의 `src/net_bench/`에 `tools/sync_to_s32ds.py --board bench144`로 넣는다. 제품 소스 목록(`tools/s32ds_sources.txt`)에는 넣지 않는다.

144에는 실제 BCM 앱이 없다. 이 폴더의 앱 대역이 BCM 상태 4종을 문서 주기로 제출하고, Network가 S32K144 RTD(FlexCAN0)로 내보낸다.

| 파일 | 내용 |
|---|---|
| `net_board_time144.c/.h` | SysTick 1 kHz 시계(시간원 방식 A의 tick, 항등 변환), µs 카운터, `Net_Init` 노드 설정 hook |
| `bcm_bench_app.c/.h` | BCM 앱 대역. 명령 콜백 수 세기, 상태 4종 주기 제출, `NetNode_GetDiag` 주기 갱신. `NETWORK_UPPER_NODE_PROFILE`이 BCM(3)일 때만 내용이 컴파일된다 |

## 시계 (`net_board_time144.c`)

- `Board_TimeInit()`: SysTick을 코어 클럭으로 1 kHz 인터럽트에 맞춘다(reload = 48 MHz / 1000 - 1 = 47,999).
  - 이 프로젝트의 생성 설정은 `CLOCK_IP_GET_FREQUENCY_API`가 `STD_OFF`라 `Clock_Ip_GetClockFrequency`를 쓸 수 없다.
  - 그래서 생성 클럭 설정 값을 상수로 쓴다: RUN 모드 시스템 클럭 FIRC 48 MHz, DIVCORE 1(`generate/src/Clock_Ip_Cfg.c`, `ClockYaml.txt`의 `CORE_CLK.outFreq` 48 MHz).
  - 클럭 설정을 바꾸면 `BOARD_CORE_CLK_HZ`도 바꾼다.
- SysTick 우선순위는 리셋값 그대로 둔다. 디버그 노드(`debug_time.c`)는 가장 낮게 내렸지만, 여기서는 `Board_Us32` 경합 구간을 짧게 하려고 내리지 않는다. 핸들러는 카운터만 올린다.
- `OSIF_USE_SYSTEM_TIMER`는 `STD_OFF`라 OsIf가 SysTick을 쓰지 않는다. 켜면 SysTick 소유가 겹친다.
- `SysTick_Handler`: 시작 코드의 weak 정의를 재정의해 `volatile` ms 카운터를 올린다. 디버그 노드의 `debug_time.c`도 이 함수를 정의하므로, Network 빌드에서는 그 정의가 빠져야 한다(아래 "복제본에 넣는 변경").
- `Board_TickMs32()`는 ISR에서 읽기만 하는 tick이고, `Board_TickToMs32()`는 항등 변환이다. `Board_NowMs32()`는 main 루프가 `Net_MainFunction`에 넘기는 값이다.
- `Board_Us32()`는 2^32에서 순환하는 µs 카운터다(`NET_BOARD_US_HOOK`, `NetNodeDiag.main_max_us`).
  - 계산은 ms × 1000 + 현재 ms 안의 경과 사이클 / 48이고, ms 카운터가 2^32에서 돌아도 끊기지 않는다.
  - 경합 처리는 344 시험대(`bench/s32k344/README.md`)와 같다: SysTick pending 비트(ICSR.PENDSTSET)가 켜져 있고 현재 값이 reload의 위쪽 절반이면 1 ms를 더한다.
  - 인터럽트를 1 ms 넘게 막으면 놓친 reload를 알 수 없다.
- `Board_NetNodeConfig()`: `boot_id` = 0x42434D31("BCM1", 시험대 상수), `local_epoch` = 1, CAN epoch = 1.
  - **보드 출처 HOLD.** 리셋하면 같은 BOOT로 TX가 1부터 다시 시작한다(BOOT 수용 규칙의 전제 위반, 시험대 한정).
  - 상대 노드는 같은 BOOT 안에서 TX가 뒤로 간 프레임을 옛 프레임으로 버린다. 상대 344(VSS)도 BOOT가 보드 상수다.
  - 그래서 처음부터 다시 할 때는 **두 보드를 함께 리셋**한다.

## BCM 앱 대역 (`bcm_bench_app.c`)

- `BcmApp_OnDomainDoorCommand`, `BcmApp_OnDomainClimateCommand`, `BcmApp_OnDomainLightCommand`: 받은 수만 센다(`g_bcm_bench.*_command_count`).
- `BcmBench_Service()`: 기동 직후 1회 제출하고, 그 뒤 주기마다 새 평가를 제출한다.

  | 상태 | 함수 | CAN ID | 주기 | 내용 |
  |---|---|---|---|---|
  | DOOR | `BcmNet_PublishDoorState` | 0x200 | 100 ms | LOCKED / CLOSED / NORMAL |
  | CLIMATE | `BcmNet_PublishClimateState` | 0x210 | 200 ms | 명령·측정 OFF / IDLE / 0 % / 열 제거 정상 |
  | LIGHT | `BcmNet_PublishLightState` | 0x220 | 200 ms | NORMAL / 0 % / RGB 0 / APPLIED / 물리 피드백 미지원 |
  | STATUS | `BcmNet_PublishStatus` | 0x230 | 250 ms | READY / fault 없음 |

  - 매번 새 평가다: UPDATE_SEQUENCE + 1(상태별), SOURCE_AGE 0, 품질 모두 OK.
  - 다음 제출 시각은 제출한 시각 + 주기다.
  - 거절된 제출은 다시 내지 않고 센다. 결과별 횟수는 `g_bcm_bench.state[i].result[]`(OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY 순)에 남는다. `i`는 0 DOOR, 1 CLIMATE, 2 LIGHT, 3 STATUS다.
- `g_net_diag`: `Net_Init` 직후(`BcmBench_Init`)와 이후 100 ms마다 `NetNode_GetDiag` 값으로 갱신한다.
- Result(0x140)·Event(0x141)는 보내지 않는다. 명령을 보낼 상대가 없어 결과가 생기지 않는다.

## 관찰 변수 (S32DS Expressions)

| 식 | 볼 것 |
|---|---|
| `g_net_diag` | `entry_init_result`(1), `stage`(READY), `rtd.*`(`started`, `init_failed_step`, `tx_complete`, `tx_timeout_aborts`, `rx_frames`, `rx_overruns`, `isr_ring_max_depth`, `isr_ring_overflow`, `busoff_events`), `upper.rx_invalid`·`rx_dropped`, `hal.rx_dropped`, 슬롯 `door_state_tx`·`climate_state_tx`·`light_state_tx`·`status_tx`, `main_max_us` |
| `g_bcm_bench` | 상태별 `submitted`, `result[]`, `last_result`, `last_update_sequence`, 합계 `refused`, 명령 콜백 수, `diag_refreshes` |
| `net_hal_rtd_core.diag` | RTD 백엔드 진단 원본(`g_net_diag.rtd`와 같은 값, 100 ms 지연 없음) |

## 복제본에 넣는 변경

복제본의 디버그 노드 소스(`main.c`, `debug_*.c`)는 이 저장소에 복사하지 않는다. Network 빌드(`NET_NODE_BUILD`)에서 지켜야 할 조건만 적는다. 파일별 처리(빌드 제외 또는 `#ifndef NET_NODE_BUILD`)는 보드 단계 기록에 남긴다.

- FlexCAN0과 그 콜백은 Network HAL(`net_hal_rtd_can.c`)만 쓴다. 디버그 노드에서 FlexCAN 콜백(`DebugCan_EventCallback`, `DebugCan_ErrorCallback`)을 정의하거나 CAN0을 직접 쓰는 코드는 빌드에서 빠져야 한다.
- `SysTick_Handler`는 `net_board_time144.c`만 정의한다.
- `main()`의 `NET_NODE_BUILD` 경로:
  1. Clock, Port, IntCtrl 초기화
  2. `Board_TimeInit()` → `Net_Init()` → `BcmBench_Init()`
  3. 루프: `Net_MainFunction(Board_NowMs32())` → `BcmBench_Service()`
- PC 콘솔(LPUART1)은 남겨도 되지만, CAN0을 쓰는 명령 경로는 빠져야 한다.
- `.mex`, `generate/`, `board/`, `RTD/`, `Project_Settings/`는 바꾸지 않는다. ConfigTools 재생성도 하지 않는다.

### `.cproject` (Debug_FLASH C 컴파일러만)

include 경로 7개(344 시험대·VSS 프로젝트와 같은 목록):

```
${ProjDirPath}/src/network/include
${ProjDirPath}/src/network/src/l04_frame_codec
${ProjDirPath}/src/network/src/l02_typed_handoff/m03_typed_handoff
${ProjDirPath}/src/network/src/l05_transport_port/m07_buffer_outcome
${ProjDirPath}/src/network/src/l05_transport_port/m09_observation
${ProjDirPath}/src/network/src/l03_message_service/m04_message_service
${ProjDirPath}/src/network/src/platform/rtd
```

define 8개:

```
NETWORK_UPPER_NODE_PROFILE=3
NET_NODE_BUILD
NET_BOARD_ISR_TICK_HOOK=Board_TickMs32
NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32
NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig
NET_BOARD_US_HOOK=Board_Us32
NET_BOARD_CAN_CALLBACK=DebugCan_EventCallback
NET_BOARD_CAN_ERROR_CALLBACK=DebugCan_ErrorCallback
```

- MCU는 프로젝트의 `CPU_S32K144HFT0VLLT`가 144를 고른다. `NETWORK_NODE_PLATFORM`은 넣지 않는다.
- 생성 설정에서 읽은 이름(`network_144_can_debuger`, S32K1 RTD 3.0.0):
  - 설정 `FlexCAN_Config0`, 상태 `FlexCAN_State0`, 인스턴스 0: `net_board_cfg.h` 기본값과 같아 define을 넣지 않는다.
  - 콜백 `DebugCan_EventCallback` / `DebugCan_ErrorCallback`: 기본값(`Can0_Bringup*`)과 달라 위 define 두 줄로 맞춘다. 이름은 ConfigTools에 적힌 것이고 생성 파일이 extern으로 참조한다.
  - `max_num_mb` 7, FD payload 64 B: 144 기본 배치(RX MB 0..1, TX MB 2..5, MB 6 안 씀)와 같아 MB 매크로를 넣지 않는다.
- 생성 설정 `ctrlOptions`에 `FLEXCAN_IP_BUSOFF_RECOVERY_U32`(하드웨어 bus-off 자동 복구)가 켜져 있다. VSS 프로젝트(`S32K344_WB_Audio_Bringup`)의 생성 설정도 같다.
  - Network는 bus-off가 나면 CAN 링크를 닫고, 남은 송신을 정산한 뒤 100 ms(다섯 번 넘게 이어지면 1 s) 지나 다시 연다(`VSS_NODE_INTEGRATION.md` "제한"). 설정은 바꾸지 않는다.

## 동기화

```
python firmware/network/tools/sync_to_s32ds.py --project <144 복제본> --board bench144          # dry run
python firmware/network/tools/sync_to_s32ds.py --project <144 복제본> --board bench144 --apply
```

- 도구가 거부하는 경우:
  - 브링업 원본(`network_144_can_debuger`)
  - VSS 프로젝트(`src/app/vss_main.c` 있음)
  - `.cproject`에 `CPU_S32K144` define이 없는 프로젝트. `bench`와 `bench144`는 둘 다 `src/net_bench/`로 복사하므로, 보드를 잘못 고르면 344와 144 시험대 파일이 섞인다.
- 복제본의 `src/network/`, `src/net_bench/`는 직접 고치지 않는다. 이 저장소에서 고치고 다시 동기화한다.
