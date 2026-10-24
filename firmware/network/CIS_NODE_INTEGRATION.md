# CIS 노드 Network 통합 메모

CIS 노드(profile 1, CAN + UART-C) 펌웨어에 Network를 붙일 때 지켜야 할 사항만 모았다. 설계 근거는 `docs/network/interface/02_NODES/CIS/{10_DATA,20_API}.md`, `02_NODES/PI/10_DATA.md`, `03_COMMUNICATION/10_CAN_FD.md`, `30_UART_C.md`이다. 공개 진입, 시각, 보드 설정, ISR 링은 VSS와 같으므로 `VSS_NODE_INTEGRATION.md`를 따른다.

## App 경계 (정본 헤더)

- `include/network/app/net_if_cis.h`: CIS `10_DATA.md` 타입과 `20_API.md` 원형 8개. 진입 헤더(`net_if_entry.h`)도 include한다.
- `include/network/app/net_if_pi_data.h`: UART-C 기록(`NetVisionResult`, `NetVisionStatus`, `NetPiPermission`). Pi 문서에 한 번만 정의된 것을 그대로 옮겼다.
- `include/network/app/net_if_common.h`: 공통 타입(`NetUartRxMeta`, `NetValidity`, `NetSensorFunctionState` 포함).

| 함수 | 구현 | 메시지 | 비고 |
|---|---|---|---|
| `CisApp_OnDomainPermission` | CIS 앱 | CAN 0x080 수신 | `Net_MainFunction()` 안에서만 호출. 같은 입력이 Pi relay로도 간다 |
| `CisApp_OnPiVisionResult` | CIS 앱 | UART-C 0x60 수신 | `meta.link_sequence`, `meta.received_at_ms`(프레임 마지막 바이트 ISR 시각) |
| `CisApp_OnPiVisionStatus` | CIS 앱 | UART-C 0x61 수신 | 같음 |
| `CisNet_PublishRear` | Network | CAN 0x0A0 | 앱이 50 ms마다(또는 변화 시) 새 평가로 제출. Network는 반복하지 않는다 |
| `CisNet_PublishEnvironment` | Network | CAN 0x300 | 앱이 200 ms마다 제출. 반복하지 않는다 |
| `CisNet_PublishOccupant` | Network | CAN 0x310 | 아래 "Occupant relay" 참고. 반복하지 않는다 |
| `CisNet_PublishStatus` | Network | CAN 0x320 | 앱이 250 ms마다 제출. 반복하지 않는다 |
| `CisNetPriv_RelayPiPermission` | Network | UART-C 0x62 송신 | Network 내부. 앱은 부르지 않는다 |

- BOOT_ID와 CAN TX_SEQUENCE, UART LINK_SEQUENCE는 Network(M04 `net_upper_publish.c`)가 프레임마다 쓴다. 앱은 채우지 않는다.
- 콜백 안에서 `CisNet_Publish*`를 불러도 된다. 메시지마다 1건만 보관하고(최신 우선), 같은 `Net_MainFunction()` 안에서 보낸다.
- 반환값: 링크·문맥 미준비(`Net_Init` 전, CAN bus-off, Occupant의 Pi 판정 미수신) → `NET_SUBMIT_NOT_READY`, 인자 오류(NULL, 예약 코드, 다시 보내도 성립하지 않는 Occupant 조합) → `NET_SUBMIT_INVALID_ARGUMENT`. 보관 슬롯은 교체 방식이라 `NET_SUBMIT_NO_CAPACITY`는 나오지 않는다.

## Occupant relay

근거: `02_NODES/CIS/20_API.md` `CisNet_PublishOccupant`, `02_NODES/CIS/10_DATA.md` Pi 원 관측과 CIS 추가 평가.

- CIS_OCCUPANT는 **마지막으로 수용한 P_VISION_RESULT의 relay**다. Pi 원 식별(Pi BOOT, VISION_SEQUENCE)과 값(presence, count)은 그 Pi 판정과 같아야 한다.
- 필드별 validity·reason은 Pi와 같거나 **더 제한된 쪽만** 된다. INVALID에 CIS가 확인한 사유(예: 모순 → DATA_INVALID, 수신 중단 → COMMUNICATION_FAULT, 만료 → STALE)를 실을 수 있다. Pi가 INVALID인 필드를 VALID로 올릴 수는 없다.
- `vision_function_status`는 CIS 자기 기능 상태다. Pi `VISION_STATUS`와 비교하지 않는다.
- 반환값:
  - Pi 판정을 아직 받지 못했으면 `NET_SUBMIT_NOT_READY`다.
  - 다시 보내도 성립하지 않는 기록은 `NET_SUBMIT_INVALID_ARGUMENT`이고 아무것도 보내지 않는다. 다른 Pi 식별(이미 새 Pi 판정이 들어온 뒤의 옛 기록 포함), 값 변경, 품질 승격, 그 Pi 판정을 처음 받았을 때의 AGE보다 작은 `vision_age_ms`가 여기에 든다. NOT_READY로 돌려주면 앱이 영원히 재시도하기 때문이다.
- `vision_age_ms`의 하한은 그 Pi 판정을 처음 받았을 때의 AGE다. 같은 판정이 반복으로 다시 와도 하한은 그대로라, 앱은 첫 전달에서 복사해 둔 기록을 그대로 내도 된다(붙잡고 있던 시간을 더해도 된다). 보내는 AGE는 기록 값이 아니라 마지막으로 받은 그 판정(반복 포함)의 Pi AGE에 그 수신 뒤 경과를 Network가 한 번 더한 값이다.
- 시험: `cis_node.f10_publish_occupant_relay`, `f24_occupant_restricted_quality`.

## P_PERMISSION relay

근거: `03_COMMUNICATION/30_UART_C.md` P_PERMISSION, `02_NODES/CIS/20_API.md` `CisNetPriv_RelayPiPermission`, `04_COMMON/10_DATA_AND_ID_RULES.md` 번호 공간, `02_NODES/PI/10_DATA.md` `NetPiPermission`.

- **보내는 때:** 새 DOMAIN 평가, 수용한 DOMAIN 세대 변경(재부팅 뒤 새 BOOT의 첫 0x080), 실제 전원 품질 제한 변경이다. 받은 같은 `Net_MainFunction()`에서 바로 Pi로 보낸다.
- **내용:**
  - CIS_BOOT_ID: 이 노드의 Network BOOT_ID
  - USE_LIMIT: 1500 ms(기존 후보)
  - 전원 허용 값과 `VALUE_QUALITY`: 0x080 전원 필드를 받은 그대로. 비OK 품질도 명시 상태로 보낸다
  - `QUALITY_REASON`: 고정 대응표. OK→NONE(0), STALE→STALE(5), INVALID→DATA_INVALID(9), NO_DATA→NO_DATA(6)(`net_upper_pi_permission_reason()`). 서비스는 다른 사유의 relay를 거절한다
  - SOURCE_AGE: DOMAIN AGE + CIS 대기
- **UPDATE_SEQUENCE는 CIS가 소유하는 relay 번호다.** M04 P_PERMISSION 슬롯이 번호를 주며, 소유자는 이것 하나다. CIS 기동마다 1부터 시작하고 relay 기록마다 +1이다. DOMAIN UPDATE_SEQUENCE를 옮기지 않으므로 DOMAIN이 재부팅해 번호가 작아져도 relay 번호는 뒤로 가지 않는다. 한 바퀴 안에 relay 이벤트가 둘 오면(예: 새 평가와 그 품질 하향) 번호는 둘 쓰고, 아직 나가지 않은 앞 기록은 교체되어 프레임은 마지막 것 하나만 나간다. 진단은 `NetNodeDiag.pi_permission_tx.revision`(마지막으로 준 번호)이다.
- **같은 UPDATE의 품질 하향**(예: OK → INVALID, 새 TX)도 즉시 보낸다. 값은 그대로, 품질과 사유만 바뀐 relay이고 새 relay 번호를 받는다. 같은 UPDATE에서 값이 바뀌거나 품질이 OK로 돌아간 프레임은 수신에서 보류한다(`cis_node.f15_permission_quality_drop_same_update`, `f21_permission_quality_drop_relayed`).
- **보내지 않는 때:**
  - 같은 DOMAIN 평가를 새 TX로 반복한 0x080(약 50 ms마다)은 `CisApp_OnDomainPermission`에만 전달한다(`NetNodeDiag.app.permission_repeats`). 그래서 P_PERMISSION은 CAN 프레임마다 나가지 않는다(`cis_node.f19_permission_repeat_no_flood`).
  - relay가 싣는 것(DOMAIN BOOT, UPDATE, 전원 값, 전원 품질)이 마지막 relay와 같은 0x080(예: operation 품질만 바뀜)도 보내지 않는다(`NetNodeDiag.app.relay_unchanged`).
- **Network는 P_PERMISSION을 반복하지 않는다(주기 0).** 새 평가·세대 변경·품질 제한 때만 보낸다. Pi의 1500 ms 수신 감시는 DOMAIN의 약 200 ms 재평가(`10_CAN_FD.md` DOMAIN_PERMISSION)가 채운다. DOMAIN이 멈추면 Pi는 1500 ms 뒤 수신 중단으로 판단한다. 이전 ALLOW를 연장하지 않으며, 원 stamp로 다시 보내는 반복은 원 Power 사용 기한 500 ms(`10_DATA_AND_ID_RULES.md` 소비 시점 경계)도 넘길 수 없다(`cis_node.f03_permission_new_evaluations`, `f04_permission_same_evaluation`, `f22_relay_sequence_cis_owned`).
- DOMAIN 0x080이 150 ms 끊기면 `NetNodeDiag.permission.reason`이 `NET_PERMISSION_RX_TIMEOUT`이 된다.

## 재부팅과 같은 관측의 반복 (수신)

- **DOMAIN 재부팅:** 새 BOOT_ID로 TX 1부터 오는 0x080을 첫 프레임에서 새 세대로 받아 콜백하고 relay한다. 구조 검증을 통과한 프레임만 세대를 바꾼다. TX_SEQUENCE 0은 예약값이다(`10_CAN_FD.md` TX_SEQUENCE "최초 1"). 그래서 구조 오류로 버린다(`rx_invalid`, 콜백·relay 없음, `cis_node.f20_permission_tx_zero`).
- **Pi 재부팅:** Pi가 새 BOOT로 LINK_SEQUENCE를 처음부터 다시 보내면, LINK_SEQUENCE 창 비교보다 먼저 새 세대로 받는다. CRC·길이·형식을 통과한 프레임만 바꾼다. P_VISION_RESULT와 P_VISION_STATUS는 같은 링크(Pi→CIS)를 쓰므로, 둘 중 먼저 온 새 BOOT 프레임이 링크를 바꾸고 다른 메시지는 자기 첫 새 BOOT 프레임에서 기준을 다시 잡는다.
- **이전 BOOT로는 돌아갈 수 없다.** 끝난 BOOT를 키(CAN ID, UART-C 수신 방향)마다 최근 4개 기억하고, 그 BOOT의 늦은 프레임은 콜백 없이 버린다(`NetNodeDiag.upper.old_generation`). 같은 BOOT_ID로 재기동한 송신 노드는 이 규칙으로 복구되지 않는다.
- **같은 관측의 반복:** 수신 중단 감시가 있는 주기 메시지(0x080, P_VISION_RESULT, P_VISION_STATUS)는 같은 원 관측을 새 TX/LINK_SEQUENCE로 반복해도 기존 콜백으로 전달한다(`20_API.md` 수신 전달). 원 관측 번호는 그대로이고 AGE는 늘어난 값이다. 정확한 중복(같은 순번, 같은 바이트)은 버린다. 반복은 `NetNodeDiag.upper.repeat_delivered`로 센다.
- **늦게 도착한 반복:** AGE가 "직전 AGE + 수신 간격"보다 CAN 5 ms, UART 50 ms 이내로 모자라면 그 하한으로 올려 전달한다(`repeat_age_raised`). 문서의 전달 여유 후보(PROPOSED)다. 더 모자라면 보류한다.
- **오래된 Pi 반복:** P_VISION_RESULT/STATUS는 AGE가 2000 ms를 넘은 반복도 콜백으로 전달한다(수신 AGE 상한은 포화 65535 + 1000 ms뿐). 같은 상태·판정이 몇 초씩 이어질 수 있고, 오래된 관측을 쓸지는 CIS 앱이 정한다(`cis_node.f25_vision_repeat_long_age`).

## 진단

- `NetNodeDiag.app`: 수신 콜백 수, 마지막 permission, relay 제출·거절·변화 없음(`relay_unchanged`), 함수별 미수용·콜백 안 제출 수, `occupant_not_current`(Occupant 거절: Pi 판정 전이거나 그 relay가 아님).
- `NetNodeDiag.rear_tx`, `environment_tx`, `occupant_tx`, `status_tx`, `pi_permission_tx`: M04 슬롯(BOOT/TX 또는 LINK_SEQUENCE, 교체, 대기 Age, 반복). `pi_permission_tx.revision`은 마지막 relay 번호다.
- 큐 최대 깊이(줄지 않는다): `NetNodeDiag.upper.tx_max_depth`(서비스 송신 칸에 동시에 있던 최대 수), `upper.rx_max_depth`(서비스 수신 큐), `hal.rx_max_depth`(어댑터 수신 큐), `rtd.isr_ring_max_depth`(CAN ISR 링, TX 완료 사실 포함), `rtd.uart[1].uart_isr_ring_max_depth`(UART-C 바이트 링).

## 보드 설정

- MCU는 역할(profile 1)과 별도로 선택한다. `feat/cis-rtd-network`의 `firmware/CIS/board/README.md`와 `net_cis_board.h`는 S32K144 / S32K1 RTD 3.0.0을 대상으로 한다. 보드의 `CPU_S32K144HFT0VLLT` 또는 명시적 `NETWORK_NODE_PLATFORM=144`를 사용한다. 알려진 CPU define과 선택이 충돌하거나 둘 다 없으면 컴파일 오류다. 실제 생성 프로젝트/RTD 검증 여부는 별도다.
- UART-C는 LPUART11(`NET_BOARD_UART_C_INSTANCE`), 생성 설정 `Lpuart_Uart_Ip_xHwConfigPB_11`이 기본값이다. DS344는 콘솔 LPUART0과 콜백을 공유하므로 라우팅 모드(`NetHalRtdUart_OnEvent`)를 쓴다.
- 위 기본값과 Host mock은 DS344 벤치 설정이다. CIS S32K144 프로젝트에서는 실제 생성 설정에 맞춰 UART 인스턴스/심볼, CAN 메시지 버퍼 등 `NET_BOARD_*`를 지정해야 한다. MCU identity 선택은 이 주변장치 설정을 바꾸지 않는다.
- `NET_BOARD_NODE_CONFIG_HOOK`이 받는 `NetNodeConfig`의 `uart_c_port_epoch`는 1로 들어온다. 0으로 바꾸면 초기화가 실패한다.
- main 주기는 UART 링 시간(256 B, 115200 bit/s 가득 찬 선로에서 약 22 ms)보다 짧고, 선로 평균 부하가 바퀴당 48 B(서비스 step 2회 × poll 24, 같은 바퀴의 CAN 사건 포함) 이하가 되게 둔다. 바퀴당 비우기 상한(16 B)을 넘는 바이트는 서비스 Port가 시각 검사 전에 넘긴다(`VSS_NODE_INTEGRATION.md` 보드 설정 절, 시험 `cis_node.f16_vision_rx_main_3ms`).

## 용량

값은 `include/network/net_capacity.h`의 CIS 블록이 정한다(빌드의 `NETWORK_UPPER_NODE_PROFILE=1`). 모두 `#ifndef`라 보드 프로젝트가 `-D`로 바꿀 수 있지만, 여러 파일이 함께 쓰는 구조체의 칸 수이므로 한 빌드의 모든 파일에 같은 값을 준다. 통신량 근거는 `docs/network/interface/03_COMMUNICATION/10_CAN_FD.md`와 `30_UART_C.md`의 메시지 목록이다(버스 주기 프레임 약 107개/s, WINDOW 제외, 1초마다 11개가 함께 시작. RX MB는 모든 ID를 받는다).

| 상수 | 값 | 기준 |
|---|---|---|
| `NET_UPPER_TX_CAPACITY` | 5 | publish 항목 수(CIS_REAR, CIS_ENVIRONMENT, CIS_OCCUPANT, CIS_STATUS, P_PERMISSION). 항목마다 retire 전까지 프레임 1건만 둔다 |
| `NET_UPPER_PUBLISH_CAPACITY` | 5 | publish 항목 수 |
| `NET_UPPER_RX_CAPACITY` | 24 | 서비스 step 1회의 수신 사건(UART 1바이트가 1건). 부하 목표 시험 `cis_node.f16_vision_rx_main_3ms`(Pi 프레임 12개를 3 ms 바퀴로 연달아)가 24 이상을 요구한다 |
| `NET_UPPER_POLL_BUDGET` | 24 | step 1회에 Port에서 꺼내는 수, RX 용량과 같게. 바퀴당 step 2회라 48건 |
| `NET_HAL_RX_CAPACITY` | 16 | 바퀴당 비우기 상한과 같게 |
| `NET_BOARD_ISR_DRAIN_BUDGET` | 16 | 바퀴당 ISR 링 → 어댑터 이동 수(CAN 먼저, 남는 몫은 UART 바이트) |
| `NET_HAL_TX_CAPACITY` | 4 | 장치에 동시에 넘기는 전송 수(CAN TX MB 수, UART 송신 칸). 축소 전 공통 값 그대로 |
| `NET_BOARD_CAN_ISR_RING` | 16 | 두 바퀴 사이의 수신 프레임. 버스 전체 주기 프레임과 동시 시작 11개를 한 바퀴 약 45 ms까지 담는다 |
| `NET_BOARD_UART_ISR_RING` | 256 | UART-C 바이트 링. 115200 bit/s 가득 찬 선로에서 약 22 ms. Pi는 1초 경계에서 한 번에 최대 55 B(P_VISION_RESULT 29 B + P_VISION_STATUS 26 B), 평균 316 B/s |

**연결 조건(어기는 값은 컴파일 오류):**

- C1: `NET_UPPER_POLL_BUDGET` ≤ `NET_UPPER_RX_CAPACITY`(24 ≤ 24). `net_capacity.h` 정적 검사. 예산이 더 크면 `net_upper_step()`이 `NET_INVALID_ARGUMENT`로 아무것도 받지 않는다.
- C2: `NET_UPPER_TX_CAPACITY` ≥ 노드 publish 표 항목 수(5 ≥ 5). `src/node/net_node_profile.h`의 `NET_NODE_PUBLISH_TABLE_FITS`. publish 표가 `NET_UPPER_PUBLISH_CAPACITY`보다 크면 `NetNode_Init`이 실패한다.
- C3: `NET_BOARD_ISR_DRAIN_BUDGET` ≤ `NET_HAL_RX_CAPACITY`(16 ≤ 16). `src/platform/rtd/net_hal_rtd.c` 정적 검사.

**보드 확인 항목(C4, 실행 중 진단만 있고 정적 검사가 없다):** 실제 보드에서 문서 통신량이 흐르는 버스와 Pi 링크로 수 분 이상 돌린 뒤 `NetNode_GetDiag`로 기록한다. 링 최대 깊이는 **용량의 75 % 이하**여야 한다.

| 기록 | 판정 |
|---|---|
| `NetNodeDiag.main_max_us` | `Net_MainFunction` 1회 최대 시간(`NET_BOARD_US_HOOK`이 있을 때, 없으면 0). 수 ms(15 ms 미만)인지 보고, main 주기 + 이 값이 UART 링 시간 약 22 ms와 CAN 링 한 바퀴 약 45 ms보다 짧은지 본다 |
| `rtd.isr_ring_max_depth` | 75 % 이하: `NET_BOARD_CAN_ISR_RING` 16의 12 이하. TX 완료 사실까지 센 링 전체 깊이라 보수적이다. `rtd.isr_ring_overflow` 0 |
| `rtd.uart[1].uart_isr_ring_max_depth` | 75 % 이하: `NET_BOARD_UART_ISR_RING` 256의 192 이하. `rtd.uart[1].uart_isr_ring_overflow` 0 |
| `upper.rx_max_depth`, `hal.rx_max_depth`, `upper.tx_max_depth` | 기록만 한다. host 부하 시험(`cis_node.load_doc_traffic`)에서 24/24, 16/16, 5/5로 용량과 같았다. step당 poll·바퀴당 비우기·publish 항목 수만큼 한 번에 채우는 설계 값이라 75 % 판정 대상이 아니다. 대신 `upper.rx_dropped`, `hal.rx_dropped`가 0인지 본다 |
| 슬롯 `counters.queue_full`(`rear_tx` … `pi_permission_tx`) | 기록한다. 장치 칸이 없어 한 바퀴 미룬 수다. 아래 알려진 제한 때문에 0이 아닐 수 있다 |

**알려진 제한:** HAL TX 4칸을 CAN 송신 4종(0x0A0, 0x300, 0x310, 0x320)과 UART-C P_PERMISSION relay가 함께 쓴다. 앱 제출과 새 DOMAIN 평가가 같은 바퀴에 겹치면 relay가 한 바퀴 미뤄질 수 있다(`pi_permission_tx.counters.queue_full`). 그때 CIS→Pi LINK_SEQUENCE가 한 번호 건너뛰고, 프레임은 다음 바퀴에 나간다. host 부하 시험은 앱을 Network 한 바퀴 뒤에 두는 순서에서 손실 0이며, 이 결과는 바퀴 위상에 기댄다.

## 시험

- `.build-cis`(`-DNETWORK_NODE_PROFILE=CIS -DNETWORK_BUILD_RTD_MOCK=ON -DNETWORK_NODE_PLATFORM=144`)의 `cis_node.*`: `tests/cis_node/test_cis_node_e2e.c`. CAN(DOMAIN)과 UART-C(Pi)를 mock SDK로 함께 구동한다. `cis_node.f13_entry`와 `*_entry`는 `Net_Init`/`Net_MainFunction` 경로다. 344도 명시하여 같은 API를 검증할 수 있으며 실제 보드 시험과 구분한다.
- 반복 전달과 재부팅: `cis_node.f14_permission_repeat_delivered`, `f17_domain_reboot`, `f18_pi_reboot`, `f19_permission_repeat_no_flood`, `f20_permission_tx_zero`, `f23_vision_repeat_delivered`(Pi 판정 반복·늦은 반복 50 ms 여유·정확한 중복), `f25_vision_repeat_long_age`(2초 넘는 Pi 반복).
- P_PERMISSION relay와 Occupant 품질: `cis_node.f21_permission_quality_drop_relayed`, `f22_relay_sequence_cis_owned`, `f24_occupant_restricted_quality`.
- 문서 통신량 부하: `cis_node.load_doc_traffic`. 10 s 동안 `10_CAN_FD.md` 주기 표의 버스 프레임(WINDOW 제외, 약 107개/s, 위상 0이라 1초마다 11개가 함께 시작한다. CIS가 받는 것은 다른 노드 몫 7개)과 Pi UART-C(P_VISION_RESULT 29 B / 100 ms, P_VISION_STATUS 26 B / 1 s, 바이트 간격 87 µs)를 넣는다. 앱 역할은 CIS_REAR·ENVIRONMENT·STATUS를 문서 주기마다, CIS_OCCUPANT를 Pi 판정마다 Network 한 바퀴(`NetNode_Step`) 뒤에 제출한다. main은 10 ms 주기이고 10바퀴마다 한 번 7 ms 늦는다(17 ms). 손실 0(링·큐·M04 슬롯, CIS 몫 수신 전부 전달), 자기 프레임 간격 ≤ 주기 + 17 ms, P_PERMISSION 간격 ≤ 500 ms + 17 ms, 최대 깊이 ≤ 용량을 확인하고 `LOAD CIS ...` 한 줄로 남긴다.
