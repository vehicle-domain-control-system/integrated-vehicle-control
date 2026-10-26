# S32K144 보드 시험대 파일 (CIS 프로필, CAN + UART-C)

Network 제품 코드가 아니라 S32K144 보드 시험대 코드다. 144 디버그 노드 프로젝트(`network_144_can_debuger`)를 S32DS에서 복사한 복제본의 `src/net_bench/`에 `tools/sync_to_s32ds.py --board cis144`로 넣는다. 제품 소스 목록(`tools/s32ds_sources.txt`)에는 넣지 않는다.

CIS 노드의 MCU는 S32K144다(`feat/cis-rtd-network`의 `firmware/CIS/board/README.md`: CAN0 PTE4/PTE5, UART-C LPUART1 PTC6/PTC7). 이 폴더의 앱 대역이 실제 CIS 앱 대신 값을 제출하고 받은 것을 센다. Network는 S32K144 RTD로 CAN(FlexCAN0)과 UART-C(LPUART1)를 쓴다.

| 파일 | 내용 |
|---|---|
| `net_board_time144.c/.h` | SysTick 1 kHz 시계(시간원 방식 A의 tick, 항등 변환), µs 카운터, `Net_Init` 노드 설정 hook. `bench/s32k144_bcm/`의 같은 이름 파일과 BOOT_ID 상수만 다르다 |
| `cis_bench_app.c/.h` | CIS 앱 대역. 수신 콜백 수와 마지막 값, Pi 판정마다 CIS_OCCUPANT relay, 주기 제출 3종, DOMAIN_PERMISSION 감독 사유 추적, `NetNode_GetDiag` 주기 갱신. `NETWORK_UPPER_NODE_PROFILE`이 CIS(1)일 때만 내용이 컴파일된다 |

## 시계 (`net_board_time144.c`)

- 내용은 BCM 시험대(`bench/s32k144_bcm/README.md` "시계")와 같다. 코어 클럭 FIRC 48 MHz 상수, SysTick 1 kHz, `Board_Us32()` 경합 처리도 같다.
- `Board_NetNodeConfig()`: `boot_id` = 0x43495331("CIS1", 시험대 상수), `local_epoch` = 1, CAN epoch = 1. UART-C port epoch는 Network가 hook에 넘기는 값 1을 그대로 둔다.
  - **보드 출처 HOLD.** 리셋하면 같은 BOOT로 CAN TX_SEQUENCE와 UART-C LINK_SEQUENCE가 1부터 다시 시작한다(BOOT 수용 규칙의 전제 위반, 시험대 한정).
  - 상대(DOMAIN 역할 보드, Pi 역할 노트북)는 같은 BOOT 안에서 순번이 뒤로 간 프레임을 옛 프레임으로 버린다. 그래서 CIS를 리셋하면 상대도 처음부터 다시 시작한다(DOMAIN 역할 보드 리셋, Pi 도구는 새 Pi BOOT).

## CIS 앱 대역 (`cis_bench_app.c`)

- 수신 콜백은 받은 수와 마지막 값·meta를 `g_cis_bench`에 남긴다.

  | 콜백 | 메시지 | 남기는 것 |
  |---|---|---|
  | `CisApp_OnDomainPermission` | CAN 0x080 | `permission_count`(같은 평가의 반복 포함), `last_permission`, `last_permission_meta`(DOMAIN BOOT, TX_SEQUENCE, 수신 ms) |
  | `CisApp_OnPiVisionResult` | UART-C 0x60 | `vision_result_count`, `last_vision_result`, `last_vision_result_meta`(LINK_SEQUENCE, 수신 ms) |
  | `CisApp_OnPiVisionStatus` | UART-C 0x61 | `vision_status_count`, `last_vision_status`, `last_vision_status_meta` |

- **Occupant relay:** P_VISION_RESULT를 받을 때마다 콜백 안에서 `CisNet_PublishOccupant`를 부른다. Pi BOOT, VISION_SEQUENCE, AGE, 값, 필드별 validity·reason은 받은 그대로이고, CIS 기능 상태는 ACTIVE다. Network가 같은 `Net_MainFunction()`에서 0x310으로 보낸다.
- **주기 제출:** `CisBench_Service()`가 기동 직후 1회, 그 뒤 주기마다 새 평가를 제출한다. 다음 제출 시각은 제출한 시각 + 주기다.

  | 기록 | 함수 | CAN ID | 주기 | 내용 |
  |---|---|---|---|---|
  | REAR | `CisNet_PublishRear` | 0x0A0 | 50 ms | 거리 50.0~149.0 cm(순번에 따라 바뀜), VALID, 근접 상태 VALID_DISTANCE |
  | ENVIRONMENT | `CisNet_PublishEnvironment` | 0x300 | 200 ms | 23.50~23.59 °C, 45.00 %RH, 300~349 lx, 모두 VALID. 세 측정 번호는 같은 값 |
  | STATUS | `CisNet_PublishStatus` | 0x320 | 250 ms | ACTIVE, 기능 모두 ACTIVE, 두 인터페이스 READY, 고장 없음 |

  - 매번 새 평가다: 순번 + 1, AGE 0.
  - 거절된 제출은 다시 내지 않고 센다. 결과별 횟수는 `g_cis_bench.publish[i].result[]`(OK, INVALID_ARGUMENT, NOT_READY, NO_CAPACITY 순)에 남는다. `i`는 0 REAR, 1 ENVIRONMENT, 2 STATUS, 3 OCCUPANT다.
- **DOMAIN_PERMISSION 감독 사유:** 10 ms마다 `NetNodeDiag.permission.reason`을 읽어 바뀔 때마다 센다(`permission_reason`, `permission_reason_changes`, 사유별 `permission_reason_entries[]`, 마지막으로 들어간 시각 `permission_reason_entered_ms[]`).
  - 0x080이 끊기면 150 ms 뒤 RX_TIMEOUT(5)이 되고, 원 AGE가 500 ms에 이르면 SOURCE_EXPIRED(4)가 된다. 보드를 나중에 멈춰 읽어도 RX_TIMEOUT을 거쳤는지 이 기록으로 본다.
- `g_net_diag`: `Net_Init` 직후(`CisBench_Init`)와 이후 100 ms마다 `NetNode_GetDiag` 값으로 갱신한다.

## 관찰 변수 (S32DS Expressions)

| 식 | 볼 것 |
|---|---|
| `g_net_diag` | `entry_init_result`(1), `stage`(READY), `rtd.*`(`started`, `init_failed_step`, `tx_complete`, `tx_timeout_aborts`, `rx_frames`, `rx_overruns`, `isr_ring_max_depth`, `isr_ring_overflow`, `busoff_events`), `rtd.uart[1].*`(UART-C: `uart_rx_bytes`, `uart_rx_errors`, `uart_isr_ring_max_depth`, `uart_isr_ring_overflow`, `uart_tx_sent`, `uart_tx_complete`), `upper.rx_invalid`·`rx_dropped`·`generation_changes`, `hal.rx_dropped`, `app.*`(`permission_delivered`, `permission_repeats`, `relay_submitted`, `relay_unchanged`), 슬롯 `rear_tx`·`environment_tx`·`occupant_tx`·`status_tx`·`pi_permission_tx`, `permission.reason`, `main_max_us` |
| `g_cis_bench` | 위 표의 수신 기록, 제출 결과, 감독 사유 기록, `diag_refreshes` |
| `'net_node.c'::service.uart_rx[0].stream` | UART-C 수신 조립: `discarded_bytes`, `bad_headers`, `bad_crc`, `assembly_timeouts`(조립 한도 50 ms), `byte_losses` |
| `'net_node.c'::service.uart_rx[0]` | `typed_rejected`(형식은 맞고 값이 코덱 계약에 어긋난 프레임), `delivered_frames` |
| `net_hal_rtd_core.diag` | RTD 백엔드 진단 원본(`g_net_diag.rtd`와 같은 값, 100 ms 지연 없음) |

## Pi 역할 (노트북)

- S32K144 평가 보드의 LPUART1(PTC6/PTC7)은 OpenSDA USB 가상 COM 포트로 노트북에 이어진다. 디버그 노드에서는 PC 콘솔이었고, 이 시험대에서는 UART-C다. 그래서 Network 빌드에는 PC 콘솔이 없다.
- 노트북은 `tools/pi_bench/pi_bench.py`로 Pi를 맡는다. P_VISION_RESULT·P_VISION_STATUS를 보내고, CIS가 relay한 P_PERMISSION을 해석해 출력한다(`tools/pi_bench/README.md`). PuTTY로는 바이너리 프레임을 보낼 수 없다.
- 115200 8N1, 생성 설정 값 115942 bit/s(디버그 노드 콘솔 설정 그대로).

## 복제본에 넣는 변경

복제본의 디버그 노드 소스(`main.c`, `debug_*.c`)는 이 저장소에 복사하지 않는다. Network 빌드(`NET_NODE_BUILD`)에서 지켜야 할 조건만 적는다. 파일별 처리는 보드 단계 기록에 남긴다.

- FlexCAN0과 그 콜백은 Network HAL(`net_hal_rtd_can.c`)만 쓴다. 디버그 노드에서 FlexCAN 콜백(`DebugCan_EventCallback`, `DebugCan_ErrorCallback`)을 정의하거나 CAN0을 직접 쓰는 코드는 빌드에서 빠져야 한다.
- LPUART1과 그 콜백은 Network HAL(`net_hal_rtd_uart.c`)만 쓴다. 생성 LPUART 설정이 참조하는 콜백 이름(디버그 노드에서는 `DebugConsole_UartCallback`)을 `NET_BOARD_UART_CALLBACK`으로 주면 HAL이 그 이름의 함수를 정의한다. 그래서 디버그 노드 쪽 같은 이름의 정의(빈 함수 포함)는 빌드에서 빠져야 한다. 디버그 노드 생성 설정에서는 LPUART0(상대 보드와 잇는 UART0)과 LPUART1이 이 콜백을 공유한다. LPUART0은 Network 빌드에서 초기화하지 않으므로 사건이 오지 않는다. 그래도 오면 HAL이 세기만 한다(`rtd.uart_unexpected_instance`).
- `SysTick_Handler`는 `net_board_time144.c`만 정의한다.
- `main()`의 `NET_NODE_BUILD` 경로:
  1. Clock, Port, IntCtrl 초기화
  2. `Board_TimeInit()` → `Net_Init()` → `CisBench_Init()`
  3. 루프: `Net_MainFunction(Board_NowMs32())` → `CisBench_Service()`
- `.mex`, `generate/`, `board/`, `RTD/`, `Project_Settings/`는 바꾸지 않는다. ConfigTools 재생성도 하지 않는다.

### `.cproject` (Debug_FLASH C 컴파일러만)

include 경로 7개(BCM 시험대와 같은 목록):

```
${ProjDirPath}/src/network/include
${ProjDirPath}/src/network/src/l04_frame_codec
${ProjDirPath}/src/network/src/l02_typed_handoff/m03_typed_handoff
${ProjDirPath}/src/network/src/l05_transport_port/m07_buffer_outcome
${ProjDirPath}/src/network/src/l05_transport_port/m09_observation
${ProjDirPath}/src/network/src/l03_message_service/m04_message_service
${ProjDirPath}/src/network/src/platform/rtd
```

define 11개:

```
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
```

- MCU는 프로젝트의 `CPU_S32K144HFT0VLLT`가 144를 고른다. `NETWORK_NODE_PLATFORM`은 넣지 않는다.
- 콜백 이름 세 개와 LPUART 설정 이름은 `network_144_can_debuger`(S32K1 RTD 3.0.0) 생성 설정에서 읽은 이름이다. 복제본에서 다시 확인하고, 다르면 define 값을 생성 설정에 맞춘다. Network 소스는 고치지 않는다.
- CAN 설정 `FlexCAN_Config0`, 상태 `FlexCAN_State0`, 인스턴스 0, `max_num_mb` 7, FD payload 64 B는 `net_board_cfg.h` 144 기본값(RX MB 0..1, TX MB 2..5, MB 6 안 씀)과 같아 define을 넣지 않는다.
- 생성 설정 `ctrlOptions`에 `FLEXCAN_IP_BUSOFF_RECOVERY_U32`(하드웨어 bus-off 자동 복구)가 켜져 있다. Network는 bus-off를 링크 실패로 보고 리셋 전까지 CAN 링크를 잠근다. 설정은 바꾸지 않는다.
- 실제 RTD 헤더로는 `tools/sdk_swap_check.py`가 디버그 노드 원본(DS144)의 생성 설정으로 CIS 프로필과 LPUART1 콜백의 HAL 정의 모드(생성 설정 .c와 HAL을 한 단위로 묶은 probe)를 컴파일해 본다. 복제본의 생성 설정은 원본과 같다.

## 동기화

```
python firmware/network/tools/sync_to_s32ds.py --project <144 CIS 복제본> --board cis144          # dry run
python firmware/network/tools/sync_to_s32ds.py --project <144 CIS 복제본> --board cis144 --apply
```

- 도구가 거부하는 경우는 `--board bench144`와 같다: 브링업 원본(`network_144_can_debuger`), VSS 프로젝트(`src/app/vss_main.c` 있음), `.cproject`에 `CPU_S32K144` define이 없는 프로젝트.
- `bench144`(BCM)와 `cis144`(CIS)는 둘 다 `src/net_bench/`로 복사한다. 한 복제본에는 하나만 쓴다. BCM 복제본(`network_144_net_bench`)에 `cis144`를 쓰지 않는다.
- 복제본의 `src/network/`, `src/net_bench/`는 직접 고치지 않는다. 이 저장소에서 고치고 다시 동기화한다.
