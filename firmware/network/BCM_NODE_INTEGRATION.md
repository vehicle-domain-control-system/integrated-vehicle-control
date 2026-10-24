# BCM 노드 Network 통합 메모

BCM 노드(profile 3, CAN만) 펌웨어에 Network를 붙일 때 지켜야 할 사항만 모았다. 설계 근거는 `docs/network/interface/02_NODES/BCM/{10_DATA,20_API}.md`, `04_COMMON/10_DATA_AND_ID_RULES.md`, `03_COMMUNICATION/10_CAN_FD.md`이다. 공개 진입, 시각, 보드 설정, ISR 링은 VSS와 같으므로 `VSS_NODE_INTEGRATION.md`를 따른다.

## App 경계 (정본 헤더)

- `include/network/app/net_if_bcm.h`: BCM `10_DATA.md` 타입과 `20_API.md` 원형 10개. 진입 헤더(`net_if_entry.h`)도 include한다.
- `include/network/app/net_if_common.h`: 공통 타입(`NetCommandMeta`, `NetRgb`, `NetVehicleResult`, `NetReason`, `NetFanLevel`, `NetThermalDirection`, `NetLightType`, `NetOrigin`, `NetFaultCategory`, `NetControllerState` 포함).

| 함수 | 구현 | 메시지 | 비고 |
|---|---|---|---|
| `BcmApp_OnDomainDoorCommand` | BCM 앱 | CAN 0x100 수신 | `Net_MainFunction()` 안에서만 호출 |
| `BcmApp_OnDomainClimateCommand` | BCM 앱 | CAN 0x110 수신 | 같음 |
| `BcmApp_OnDomainLightCommand` | BCM 앱 | CAN 0x120 수신 | 같음 |
| `BcmNet_SendResult` | Network | CAN 0x140 | 개별 기록. 제출마다 프레임 1건, 반복 없음 |
| `BcmNet_SendEvent` | Network | CAN 0x141 | 같음 |
| `BcmNet_PublishDoorState` | Network | CAN 0x200 | 앱이 100 ms마다(또는 변화 시) 제출. Network는 반복하지 않는다 |
| `BcmNet_PublishClimateState` | Network | CAN 0x210 | 앱이 200 ms마다 제출. 반복하지 않는다 |
| `BcmNet_PublishLightState` | Network | CAN 0x220 | 앱이 200 ms마다 제출. 반복하지 않는다 |
| `BcmNet_PublishStatus` | Network | CAN 0x230 | 앱이 250 ms마다 제출. 반복하지 않는다 |
| `BcmNetPriv_OnDomainPermission` | Network | CAN 0x080 수신 | Network 내부 감시 기록. 공개 `BcmApp` permission 콜백은 없다 |

- BOOT_ID와 CAN TX_SEQUENCE는 Network(M04 `net_upper_publish.c`)가 프레임마다 쓴다. 앱은 채우지 않는다. AGE에는 Network 안에서 기다린 시간만 한 번 더한다.
- 반환값: `Net_Init` 전·CAN bus-off → `NET_SUBMIT_NOT_READY`, NULL·예약 코드·조합 오류 → `NET_SUBMIT_INVALID_ARGUMENT`, 개별 기록 저장소가 가득 참 → `NET_SUBMIT_NO_CAPACITY`.

## 명령 수신

- 명령은 원 명령(DOMAIN BOOT, TARGET_BOOT_ID, COMMAND_ID)마다 한 번 전달한다. 같은 명령을 새 TX로 재송신해도 다시 전달하지 않는다. 정확한 중복과 과거 TX도 버린다.
- `TARGET_BOOT_ID`가 이 노드의 Network BOOT_ID가 아닌 명령은 앱에 전달하지 않는다(`NetNodeDiag.app.command_wrong_target`).
- 명령 수용 상한(ACCEPT_LIMIT, 200 ms 후보와 1 s 잠정안 차이 미정)은 Network가 적용하지 않는다. `command_age_ms`와 `accept_limit_ms`를 그대로 앱에 넘긴다. Network는 공통 전달 상한 2000 ms만 적용한다.

## DOMAIN 재부팅과 같은 평가의 반복 (수신)

- **재부팅:** DOMAIN이 새 BOOT_ID로 TX 1부터 보내면, CAN ID(0x080, 0x100, 0x110, 0x120)마다 그 ID의 첫 새 BOOT 프레임에서 새 세대로 바꾸고 전달한다. 구조 검증을 통과하고 facade가 받은 프레임만 바꾼다. 그래서 다른 BCM BOOT를 대상으로 한 명령(`command_wrong_target`)은 세대를 바꾸지 않고, 명령의 TARGET_BOOT_ID 규칙은 그대로다. TX_SEQUENCE 0은 예약값이다(`10_CAN_FD.md` TX_SEQUENCE "최초 1"). 그래서 구조 오류로 버리고(`rx_invalid`) 세대도 바꾸지 않는다.
- **이전 BOOT로는 돌아갈 수 없다.** 끝난 BOOT를 CAN ID마다 최근 4개 기억하고 그 늦은 프레임은 버린다(`NetNodeDiag.upper.old_generation`). 같은 BOOT_ID로 재기동한 DOMAIN은 복구되지 않는다.
- **같은 평가의 반복:** 0x080(50 ms / 150 ms)은 같은 평가를 새 TX로 반복해도 `BcmNetPriv_OnDomainPermission` 감독 기록에 전달한다(`upper.repeat_delivered`). 명령은 발생 메시지라 수신 중단 감시가 없고, 같은 명령의 재송신을 다시 전달하지 않는다.
- 시험: `bcm_node.b13_domain_reboot`.

## 현재값과 개별 기록

- 상태 4종은 현재값이다. 아직 장치가 가져가지 않은 기록은 새 제출로 교체된다(메시지별 1건, 최신 우선).
- Result·Event는 개별 기록이다. 메시지 슬롯이 비어 있으면 바로 M04로 가고, 아니면 슬롯 앞의 저장소(`NET_APP_PORT_DISCRETE_DEPTH`, 기본 4)에 호출 순서대로 기다린다. 각 기록은 자기 식별(원 명령 ID, occurrence)을 유지하고 한 번만 나간다. 기다린 시간은 AGE에 한 번 더한다. 저장소가 가득 차면 새 기록을 `NET_SUBMIT_NO_CAPACITY`로 거절하고, 보관 중인 기록은 덮어쓰지 않는다.
- 콜백 안에서 `BcmNet_*`를 불러도 된다. 첫 기록은 같은 `Net_MainFunction()` 안에서 나가고, 같은 메시지의 다음 기록은 앞 프레임이 끝난 뒤 순서대로 나간다.

## 진단

- `NetNodeDiag.app`: 명령 전달 수, 대상 BOOT 불일치, 마지막 permission과 meta, 함수별 미수용·콜백 안 제출 수, Result·Event 저장소(대기 수, 최대 깊이, 거절 수).
- `NetNodeDiag.result_tx`, `event_tx`, `door_state_tx`, `climate_state_tx`, `light_state_tx`, `status_tx`: M04 슬롯(BOOT/TX, 교체, 대기 Age).
- 큐 최대 깊이(줄지 않는다): `NetNodeDiag.upper.tx_max_depth`(서비스 송신 칸에 동시에 있던 최대 수), `upper.rx_max_depth`(서비스 수신 큐), `hal.rx_max_depth`(어댑터 수신 큐), `rtd.isr_ring_max_depth`(CAN ISR 링, TX 완료 사실 포함).
- DOMAIN 0x080이 150 ms 끊기면 `NetNodeDiag.permission.reason`이 `NET_PERMISSION_RX_TIMEOUT`이 된다.

## 용량

값은 `include/network/net_capacity.h`의 BCM 블록이 정한다(빌드의 `NETWORK_UPPER_NODE_PROFILE=3`). 모두 `#ifndef`라 보드 프로젝트가 `-D`로 바꿀 수 있지만, 여러 파일이 함께 쓰는 구조체의 칸 수이므로 한 빌드의 모든 파일에 같은 값을 준다. 통신량 근거는 `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md` 메시지 목록이다(버스 주기 프레임 약 107개/s, WINDOW 제외, 1초마다 11개가 함께 시작. RX MB는 모든 ID를 받는다). BCM은 CAN만 쓰므로 UART 링(`NET_BOARD_UART_ISR_RING`)은 쓰지 않는다.

| 상수 | 값 | 기준 |
|---|---|---|
| `NET_UPPER_TX_CAPACITY` | 6 | publish 항목 수(BCM_RESULT, BCM_EVENT, BCM_DOOR_STATE, BCM_CLIMATE_STATE, BCM_LIGHT_STATE, BCM_STATUS). 항목마다 retire 전까지 프레임 1건만 둔다 |
| `NET_UPPER_PUBLISH_CAPACITY` | 6 | publish 항목 수 |
| `NET_UPPER_RX_CAPACITY` | 8 | 서비스 step 1회의 수신 사건(CAN 프레임 1개가 1건) |
| `NET_UPPER_POLL_BUDGET` | 8 | step 1회에 Port에서 꺼내는 수, RX 용량과 같게. 바퀴당 step 2회라 16건 |
| `NET_HAL_RX_CAPACITY` | 8 | 바퀴당 비우기 상한과 같게 |
| `NET_BOARD_ISR_DRAIN_BUDGET` | 8 | 바퀴당 ISR 링 → 어댑터 이동 수. 동시 시작 11개를 두 바퀴에 나눠 꺼낸다 |
| `NET_HAL_TX_CAPACITY` | 4 | CAN TX MB 수. 축소 전 공통 값 그대로 |
| `NET_BOARD_CAN_ISR_RING` | 16 | 두 바퀴 사이의 수신 프레임. 버스 전체 주기 프레임과 동시 시작 11개를 한 바퀴 약 45 ms까지 담는다 |

**연결 조건(어기는 값은 컴파일 오류):**

- C1: `NET_UPPER_POLL_BUDGET` ≤ `NET_UPPER_RX_CAPACITY`(8 ≤ 8). `net_capacity.h` 정적 검사. 예산이 더 크면 `net_upper_step()`이 `NET_INVALID_ARGUMENT`로 아무것도 받지 않는다.
- C2: `NET_UPPER_TX_CAPACITY` ≥ 노드 publish 표 항목 수(6 ≥ 6). `src/node/net_node_profile.h`의 `NET_NODE_PUBLISH_TABLE_FITS`. publish 표가 `NET_UPPER_PUBLISH_CAPACITY`보다 크면 `NetNode_Init`이 실패한다.
- C3: `NET_BOARD_ISR_DRAIN_BUDGET` ≤ `NET_HAL_RX_CAPACITY`(8 ≤ 8). `src/platform/rtd/net_hal_rtd.c` 정적 검사.

**보드 확인 항목(C4, 실행 중 진단만 있고 정적 검사가 없다):** 실제 보드에서 문서 통신량이 흐르는 버스로 수 분 이상 돌린 뒤 `NetNode_GetDiag`로 기록한다. 링 최대 깊이는 **용량의 75 % 이하**여야 한다.

| 기록 | 판정 |
|---|---|
| `NetNodeDiag.main_max_us` | `Net_MainFunction` 1회 최대 시간(`NET_BOARD_US_HOOK`이 있을 때, 없으면 0). 수 ms(15 ms 미만)인지 보고, main 주기 + 이 값이 CAN 링 한 바퀴 약 45 ms보다 짧은지 본다 |
| `rtd.isr_ring_max_depth` | 75 % 이하: `NET_BOARD_CAN_ISR_RING` 16의 12 이하. TX 완료 사실까지 센 링 전체 깊이라 보수적이다. `rtd.isr_ring_overflow` 0 |
| `upper.rx_max_depth`, `hal.rx_max_depth`, `upper.tx_max_depth` | 기록만 한다. host 부하 시험(`bcm_node.load_doc_traffic`)에서 8/8, 7/8, 4/6이었다. step당 poll·바퀴당 비우기만큼 한 번에 채우는 설계 값이라 75 % 판정 대상이 아니다. 대신 `upper.rx_dropped`, `hal.rx_dropped`가 0인지 본다 |
| 슬롯 `counters.queue_full`(`result_tx` … `status_tx`) | 기록한다. 장치 칸이 없어 한 바퀴 미룬 수다 |

## MCU 선택과 시험

- BCM SysRS는 전용 S32K144를 지정하지만 이 저장소에 BCM 실제 보드 프로젝트는 없다. 실제 프로젝트의 `CPU_S32K144HFT0VLLT` 또는 확인한 MCU의 `NETWORK_NODE_PLATFORM=144/344`를 명시한다. 역할(profile 3)만으로 MCU를 정하지 않으며 선택 누락/충돌은 컴파일 오류다.
- CAN 메시지 버퍼 배치는 MCU 선택(`NETWORK_NODE_PLATFORM`, S32DS에서는 CPU define)에 따라 `src/platform/rtd/net_board_cfg.h`가 기본값을 고른다. 144: `NET_BOARD_CAN_MAX_MB` 7(`network_144_can_debuger` CAN0의 `max_num_mb`, CAN FD 64 B), RX MB 0~1(`NET_BOARD_CAN_RX_MB_FIRST` 0, `NET_BOARD_CAN_RX_MB_COUNT` 2), TX MB 2~5(`NET_BOARD_CAN_TX_MB_FIRST` 2부터 `NET_HAL_TX_CAPACITY` 4개), MB 6은 쓰지 않는다. 344는 16, RX 0~3, TX 8부터다. RX MB는 모든 표준 ID를 받는다(mask 0). 실제 생성 설정의 `max_num_mb`가 다르면 네 값을 `-D`로 준다. RX와 TX가 겹치거나 `NET_BOARD_CAN_MAX_MB`를 넘는 배치(예: 144에서 `-DNET_BOARD_CAN_TX_MB_FIRST=4`, TX 4~7)는 `net_hal_rtd_can.c`의 정적 검사가 컴파일 오류로 막는다(시험 `tools.board_mb_layout`). `.build-bcm`의 mock 빌드도 이 144 배치로 돈다.
- `.build-bcm`(`-DNETWORK_NODE_PROFILE=BCM -DNETWORK_BUILD_RTD_MOCK=ON -DNETWORK_NODE_PLATFORM=144`)의 `bcm_node.*`: `tests/bcm_node/test_bcm_node_e2e.c`. DOMAIN 쪽 CAN을 mock SDK로 구동한다. `bcm_node.b14_entry`와 `*_entry`는 `Net_Init`/`Net_MainFunction` 경로다. 344도 명시하여 같은 API를 검증할 수 있으며 실제 보드 시험과 구분한다.
- 문서 통신량 부하: `bcm_node.load_doc_traffic`. 10 s 동안 `10_CAN_FD.md` 주기 표의 버스 프레임(WINDOW 제외, 약 107개/s, 위상 0이라 1초마다 11개가 함께 시작한다. BCM이 받는 것은 다른 노드 몫 7개)을 넣고, 앱 역할로 BCM_DOOR_STATE·CLIMATE_STATE·LIGHT_STATE·STATUS를 문서 주기마다 Network 한 바퀴(`NetNode_Step`) 뒤에 제출한다. main은 10 ms 주기다. 손실 0(링·큐·M04 슬롯, DOMAIN_PERMISSION 전부 감시에 전달), 자기 프레임 간격 ≤ 주기 + 10 ms, 최대 깊이 ≤ 용량을 확인하고 `LOAD BCM ...` 한 줄로 남긴다.
