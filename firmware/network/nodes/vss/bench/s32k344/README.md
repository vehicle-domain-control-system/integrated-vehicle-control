# S32K344 보드 시험대 파일 (VSS 프로필)

Network 제품 코드가 아니라 S32K344 보드 시험대 코드다. S32DS 벤치 프로젝트(브링업 프로젝트를 복사한 것)의 `src/net_bench/`에 `tools/sync_to_s32ds.py`로 넣는다. 제품 소스 목록(`tools/s32ds_sources.txt`)에는 넣지 않는다.

| 파일 | 내용 |
|---|---|
| `net_board_time.c/.h` | SysTick 1 kHz 시계(시간원 방식 A의 tick, 항등 변환), µs 카운터, `Net_Init` 노드 설정 hook |
| `vss_bench_app.c/.h` | VSS 앱 대역. 수신 콜백 기록, VSS_STATUS 제출, `NetNode_GetDiag` 주기 갱신. `NETWORK_UPPER_NODE_PROFILE`이 VSS(6)일 때만 내용이 컴파일된다 |

## 시계 (`net_board_time.c`)

- `Board_TimeInit()`: SysTick을 코어 클럭으로 1 kHz 인터럽트에 맞춘다(reload = 160 MHz / 1000 - 1). 이 프로젝트의 생성 설정은 `CLOCK_IP_GET_FREQUENCY_API`가 `STD_OFF`라 `Clock_Ip_GetClockFrequency`를 쓸 수 없다. 그래서 생성 클럭 설정의 CORE_CLK 160 MHz를 상수로 쓴다.
- `SysTick_Handler`: 시작 코드의 weak 정의를 재정의해 `volatile` ms 카운터를 올린다.
- `Board_TickMs32()`는 ISR에서 읽기만 하는 tick이고, `Board_TickToMs32()`는 항등 변환이다. `Board_NowMs32()`는 main 루프가 `Net_MainFunction`에 넘기는 값이다.
- `Board_Us32()`는 2^32에서 순환하는 µs 카운터다. `NetNodeDiag.main_max_us`(`Net_MainFunction` 1회 최대 실행 시간, VSS 계약 C8: 15 ms 초과 금지)를 보는 데 쓴다.
  - ms × 1000 + 현재 ms 안의 경과 사이클 / 160을 32비트로 계산한다. ms 카운터가 2^32에서 돌아도 µs 값은 끊기지 않는다. 사이클 카운터를 160으로 나눈 값은 약 26.8초마다 끊기므로 쓰지 않는다.
  - SysTick이 reload했지만 핸들러가 아직 ms를 올리지 않은 순간을 처리한다. COUNTFLAG는 읽으면 지워지므로 대신 SysTick pending 비트(ICSR.PENDSTSET)를 본다. pending이고 현재 값이 reload의 위쪽 절반이면 1 ms를 더한다. 인터럽트를 1 ms 넘게 막으면 놓친 reload를 알 수 없다.
- `Board_NetNodeConfig()`: `local_epoch` = 1, CAN epoch = 1.
  - `boot_id`: 부팅마다 코어 사이클 카운터(DWT_CYCCNT)를 seed로 `net_boot_id_next`가 만든 값(임시, 제품 출처는 HOLD). 리셋 버튼(warm·functional reset)은 이 카운터를 지우지 않으므로 누를 때마다 값이 바뀐다. 소프트웨어·디버거 리셋은 S32K344에서 destructive reset일 수 있어 보드에서 확인하지 않았다. 전원을 뺐다 꽂으면 카운터가 0부터라 값이 늘 같다. 상대가 이번 실행 중에 그 값을 본 적이 있으면 이 보드의 프레임을 옛 실행으로 버리므로, 리셋 버튼으로 한 번 더 리셋한다. `NET_BOARD_BOOT_ID_FIXED`를 정의하면 시험대 상수 0x00000344다.
  - BOOT가 바뀌면 수신 노드는 TX가 1부터 다시 시작한 0x340을 새 실행으로 받으므로, 344만 리셋해도 된다. 제품 BOOT_ID는 기동마다 달라야 한다(VSS_NODE_INTEGRATION.md 계약 C4). 제품 출처(NVM, noinit 카운터)는 아직 정하지 않았다.

## VSS 앱 대역 (`vss_bench_app.c`)

- `VssApp_OnDomainEvent`, `VssApp_OnDomainWarningState`: 받은 횟수, 마지막 메시지, 마지막 meta를 `g_vss_bench`에 저장한다.
- `VssBench_Service()`: 실제 VSS 앱의 VSS_STATUS 제출 규칙을 따른다.
  - 기동 직후 1회, 그 뒤 250 ms마다 새 평가(UPDATE_SEQUENCE + 1)를 `VssNet_PublishStatus`로 제출한다.
  - 내용: READY / FULL / accepting = YES / fault 없음 / `last_fault_quality` = NO_DATA. SOURCE_AGE는 평가 뒤 지난 시간이다(새 평가는 0).
  - NOT_READY·NO_CAPACITY면 다음 바퀴에 같은 번호로 다시 제출한다. 그사이 새 평가 시각이 되면 새 평가가 대신한다. INVALID_ARGUMENT는 버린다.
  - 결과별 횟수는 `g_vss_bench.status_result[]`(OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY 순)에 남는다.
- `g_net_diag`: `Net_Init` 직후(`VssBench_Init`)와 이후 100 ms마다 `NetNode_GetDiag` 값으로 갱신한다.

## 벤치 프로젝트에 넣은 변경

벤치 프로젝트의 `main.c`, `bringup_can0.c`는 RTD 예제에서 시작한 파일이라 이 저장소에 복사하지 않는다. 바꾼 내용만 적는다. 모두 `NET_NODE_BUILD`로 나뉘며, 정의하지 않으면 원래 브링업 동작 그대로다.

### `src/main.c`

- include: `network/app/net_if_entry.h`, `net_bench/net_board_time.h`, `net_bench/vss_bench_app.h`(`main.c` 기준 상대 경로라 include 경로를 더하지 않는다).
- 초기화: 공통 초기화(Clock, Port, IntCtrl)와 `Bringup_Uart_Init`/`Bringup_Uart_Service`는 그대로 둔다. `Bringup_Can0_Init()` 대신 `Board_TimeInit()` → `Net_Init()` → `VssBench_Init()`.
- 루프: `Bringup_Uart_Service()` → `Net_MainFunction(Board_NowMs32())` → `VssBench_Service()`. `Bringup_Can0_Service()`는 부르지 않는다.

### `src/bringup_can0.c`

- CAN0 Echo 구현(정적 데이터, `Can0_BringupCallback`, `Can0_BringupErrorCallback`, `Bringup_Can0_Init`, `Bringup_Can0_Service` 등)을 `#ifndef NET_NODE_BUILD`로 감쌌다. Network 빌드에서는 Network HAL(`net_hal_rtd_can.c`)이 FlexCAN0을 쓰고 두 콜백을 정의한다.
- 항상 빌드되는 것: 전역 `g_can0_peer_diag`(`bringup_uart.c`가 출력하고 CAN1 stub이 센다), `Can1_BringupCallback`, `Can1_BringupErrorCallback`(생성 CAN1 설정이 참조한다).
- `Bringup_Can0_SetWideEcho`, `Bringup_Can0_Rearm`: UART0 콘솔 명령이 부르므로 Network 빌드에서는 FALSE를 반환하는 stub이다.

### `.cproject` (Debug_FLASH C 컴파일러만)

include 경로 7개:

```
${ProjDirPath}/src/network/include
${ProjDirPath}/src/network/src/l04_frame_codec
${ProjDirPath}/src/network/src/l02_typed_handoff/m03_typed_handoff
${ProjDirPath}/src/network/src/l05_transport_port/m07_buffer_outcome
${ProjDirPath}/src/network/src/l05_transport_port/m09_observation
${ProjDirPath}/src/network/src/l03_message_service/m04_message_service
${ProjDirPath}/src/network/src/platform/rtd
```

define 6개:

```
NETWORK_UPPER_NODE_PROFILE=6
NET_NODE_BUILD
NET_BOARD_ISR_TICK_HOOK=Board_TickMs32
NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32
NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig
NET_BOARD_US_HOOK=Board_Us32
```

- 소스 항목은 `src` 폴더 전체라 `src/network/`, `src/net_bench/`는 자동으로 빌드에 들어간다. 소스 항목은 바꾸지 않는다.
- 소스 항목은 모든 빌드 구성이 공유하고 include·define은 Debug_FLASH에만 넣었다. 그래서 Release_FLASH와 RAM 구성은 빌드되지 않는다(예상된 결과).
- `.mex`, `generate/`, `board/`, `RTD/`, `Project_Settings/`는 바꾸지 않는다. ConfigTools 재생성도 하지 않는다.

## 동기화

```
python firmware/network/tools/sync_to_s32ds.py --project <벤치 프로젝트> --board bench          # dry run
python firmware/network/tools/sync_to_s32ds.py --project <벤치 프로젝트> --board bench --apply
```

`--board bench`가 기본값이다. VSS 프로젝트(`src/app/vss_main.c`가 있음)에는 이 폴더를 넣지 않는다(`VssApp_On*`, `SysTick_Handler`, `Board_*`가 겹친다). 도구가 거부한다.

벤치의 `src/network/`, `src/net_bench/`는 직접 고치지 않는다. 이 저장소에서 고치고 다시 동기화한다.
