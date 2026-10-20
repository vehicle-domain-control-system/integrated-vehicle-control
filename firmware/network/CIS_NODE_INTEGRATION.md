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
- 반환값: 링크·문맥 미준비(`Net_Init` 전, CAN bus-off, Occupant의 원 관측 불일치) → `NET_SUBMIT_NOT_READY`, 인자 오류(NULL, 예약 코드) → `NET_SUBMIT_INVALID_ARGUMENT`. 보관 슬롯은 교체 방식이라 `NET_SUBMIT_NO_CAPACITY`는 나오지 않는다.

## Occupant relay

- Network는 CIS_OCCUPANT를 **마지막으로 수용한 P_VISION_RESULT의 relay로만** 받는다. 같은 Pi BOOT, VISION_SEQUENCE, 값, validity, reason이어야 하고, `vision_function_status`는 Pi `vision_status` 코드와 같아야 한다.
- CIS가 validity를 더 제한한 기록, 다른 기능 상태, 이미 새 Pi 판정이 들어온 뒤의 옛 기록은 `NET_SUBMIT_NOT_READY`이고 아무것도 보내지 않는다.
- `vision_age_ms`에는 앱이 붙잡고 있던 시간을 더해서 넣어도 된다(Pi AGE보다 작으면 안 된다). Network는 수신 시각부터 송신까지의 경과를 직접 한 번 더한다.

## P_PERMISSION relay

- 새 DOMAIN 평가(새 UPDATE_SEQUENCE)를 받으면 같은 `Net_MainFunction()`에서 바로 Pi로 보낸다. CIS_BOOT_ID는 이 노드의 Network BOOT_ID, USE_LIMIT는 1500 ms(기존 후보), SOURCE_AGE는 DOMAIN AGE + CIS 대기다.
- DOMAIN이 재부팅한 뒤 새 BOOT의 첫 0x080도 바로 보낸다(수용한 세대 변경). 그 UPDATE_SEQUENCE가 이전 BOOT보다 작거나 같아도 보낸다.
- 같은 DOMAIN 평가를 새 TX로 반복한 0x080(약 50 ms마다)은 `CisApp_OnDomainPermission`에는 전달하지만 Pi로 다시 보내지 않는다(`NetNodeDiag.app.permission_repeats`). 그래서 P_PERMISSION은 CAN 프레임마다 나가지 않는다(`cis_node.f19_permission_repeat_no_flood`).
- 전원 품질이 OK가 아닌 허용은 보내지 않는다(Wire에 사유를 표현할 수 없음). 이전 ALLOW는 다시 나가지 않는다.
- 같은 DOMAIN 평가(같은 UPDATE_SEQUENCE)의 품질 하향을 새 TX로 받으면 `CisApp_OnDomainPermission`과 relay 경로 모두에 전달한다(relay는 위 규칙대로 보내지 않고 `relay_refused`를 올린다). 같은 UPDATE에서 값이 바뀌거나 품질이 OK로 돌아간 프레임은 보류한다(`cis_node.f15_permission_quality_drop_same_update`).
- **Network 반복 없음(결정 대기).** P_PERMISSION은 M04에 주기 0으로 등록되어 있고, 제출 1건이 프레임 1건이다. `30_UART_C.md`의 500 ms 반복/1500 ms 수신 중단은 아직 구현하지 않는다. 원 stamp로 다시 보내는 반복은 첫 프레임 500 ms 뒤 원 DOMAIN AGE가 500 ms에 이르러(`10_DATA_AND_ID_RULES.md` 소비 시점 경계) 서비스가 거절하고, DOMAIN이 같은 평가를 반복하면 지금 수용한 0x080과도 맞지 않게 되기 때문이다. 반복 기준(원 500 ms와 USE_LIMIT 1500 ms 중 무엇을 따를지, 반복 프레임의 AGE, 원 receipt 대조)은 문서 담당 결정을 기다린다. 그때까지 Pi 쪽 1500 ms 수신 중단을 피하는 것은 DOMAIN의 약 200 ms 재평가다(`cis_node.f03_permission_new_evaluations`, `f04_permission_same_evaluation`).
- DOMAIN 0x080이 150 ms 끊기면 `NetNodeDiag.permission.reason`이 `NET_PERMISSION_RX_TIMEOUT`이 된다.

## 재부팅과 같은 관측의 반복 (수신)

- **DOMAIN 재부팅:** 새 BOOT_ID로 TX 1부터 오는 0x080을 첫 프레임에서 새 세대로 받아 콜백하고 relay한다. 구조 검증을 통과한 프레임만 세대를 바꾼다. TX_SEQUENCE 0은 `10_CAN_FD.md`의 "최초 1"에 따라 구조 오류로 버린다(`rx_invalid`, 콜백·relay 없음, `cis_node.f20_permission_tx_zero`).
- **Pi 재부팅:** Pi가 새 BOOT로 LINK_SEQUENCE를 처음부터 다시 보내면, LINK_SEQUENCE 창 비교보다 먼저 새 세대로 받는다. CRC·길이·형식을 통과한 프레임만 바꾼다. P_VISION_RESULT와 P_VISION_STATUS는 같은 링크(Pi→CIS)를 쓰므로, 둘 중 먼저 온 새 BOOT 프레임이 링크를 바꾸고 다른 메시지는 자기 첫 새 BOOT 프레임에서 기준을 다시 잡는다.
- **이전 BOOT로는 돌아갈 수 없다.** 끝난 BOOT를 키(CAN ID, UART-C 수신 방향)마다 최근 4개 기억하고, 그 BOOT의 늦은 프레임은 콜백 없이 버린다(`NetNodeDiag.upper.old_generation`). 같은 BOOT_ID로 재기동한 송신 노드는 이 규칙으로 복구되지 않는다.
- **같은 관측의 반복:** 수신 중단 감시가 있는 주기 메시지(0x080, P_VISION_RESULT, P_VISION_STATUS)는 같은 원 관측을 새 TX/LINK_SEQUENCE로 반복해도 기존 콜백으로 전달한다(`20_API.md` 수신 전달). 원 관측 번호는 그대로이고 AGE는 늘어난 값이다. 정확한 중복(같은 순번, 같은 바이트)은 버린다. 반복은 `NetNodeDiag.upper.repeat_delivered`로 센다.
- **늦게 도착한 반복:** AGE가 "직전 AGE + 수신 간격"보다 CAN 5 ms, UART 50 ms 이내로 모자라면 그 하한으로 올려 전달한다(`repeat_age_raised`). 문서의 전달 여유 후보(PROPOSED)다. 더 모자라면 보류한다.

## 진단

- `NetNodeDiag.app`: 수신 콜백 수, 마지막 permission, relay 제출·거절, 함수별 미수용·콜백 안 제출 수, `occupant_not_current`.
- `NetNodeDiag.rear_tx`, `environment_tx`, `occupant_tx`, `status_tx`, `pi_permission_tx`: M04 슬롯(BOOT/TX 또는 LINK_SEQUENCE, 교체, 대기 Age, 반복).

## 보드 설정

- UART-C는 LPUART11(`NET_BOARD_UART_C_INSTANCE`), 생성 설정 `Lpuart_Uart_Ip_xHwConfigPB_11`이 기본값이다. DS344는 콘솔 LPUART0과 콜백을 공유하므로 라우팅 모드(`NetHalRtdUart_OnEvent`)를 쓴다.
- `NET_BOARD_NODE_CONFIG_HOOK`이 받는 `NetNodeConfig`의 `uart_c_port_epoch`는 1로 들어온다. 0으로 바꾸면 초기화가 실패한다.
- main 주기는 UART 링 시간(256 B, 115200 bit/s 가득 찬 선로에서 약 22 ms)보다 짧고, 선로 평균 부하가 바퀴당 64 B 이하가 되게 둔다. 바퀴당 비우기 상한(16 B)을 넘는 바이트는 서비스 Port가 시각 검사 전에 넘긴다(`VSS_NODE_INTEGRATION.md` 보드 설정 절, 시험 `cis_node.f16_vision_rx_main_3ms`).

## 시험

- `.build-cis`(`-DNETWORK_NODE_PROFILE=CIS -DNETWORK_BUILD_RTD_MOCK=ON`)의 `cis_node.*`: `tests/cis_node/test_cis_node_e2e.c`. CAN(DOMAIN)과 UART-C(Pi)를 mock SDK로 함께 구동한다. `cis_node.f13_entry`와 `*_entry`는 `Net_Init`/`Net_MainFunction` 경로다.
- 반복 전달과 재부팅: `cis_node.f14_permission_repeat_delivered`, `f17_domain_reboot`, `f18_pi_reboot`, `f19_permission_repeat_no_flood`, `f20_permission_tx_zero`.
