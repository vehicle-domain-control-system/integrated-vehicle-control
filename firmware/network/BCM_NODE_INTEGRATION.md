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

## 현재값과 개별 기록

- 상태 4종은 현재값이다. 아직 장치가 가져가지 않은 기록은 새 제출로 교체된다(메시지별 1건, 최신 우선).
- Result·Event는 개별 기록이다. 메시지 슬롯이 비어 있으면 바로 M04로 가고, 아니면 슬롯 앞의 저장소(`NET_APP_PORT_DISCRETE_DEPTH`, 기본 4)에 호출 순서대로 기다린다. 각 기록은 자기 식별(원 명령 ID, occurrence)을 유지하고 한 번만 나간다. 기다린 시간은 AGE에 한 번 더한다. 저장소가 가득 차면 새 기록을 `NET_SUBMIT_NO_CAPACITY`로 거절하고, 보관 중인 기록은 덮어쓰지 않는다.
- 콜백 안에서 `BcmNet_*`를 불러도 된다. 첫 기록은 같은 `Net_MainFunction()` 안에서 나가고, 같은 메시지의 다음 기록은 앞 프레임이 끝난 뒤 순서대로 나간다.

## 진단

- `NetNodeDiag.app`: 명령 전달 수, 대상 BOOT 불일치, 마지막 permission과 meta, 함수별 미수용·콜백 안 제출 수, Result·Event 저장소(대기 수, 최대 깊이, 거절 수).
- `NetNodeDiag.result_tx`, `event_tx`, `door_state_tx`, `climate_state_tx`, `light_state_tx`, `status_tx`: M04 슬롯(BOOT/TX, 교체, 대기 Age).
- DOMAIN 0x080이 150 ms 끊기면 `NetNodeDiag.permission.reason`이 `NET_PERMISSION_RX_TIMEOUT`이 된다.

## 시험

- `.build-bcm`(`-DNETWORK_NODE_PROFILE=BCM -DNETWORK_BUILD_RTD_MOCK=ON`)의 `bcm_node.*`: `tests/bcm_node/test_bcm_node_e2e.c`. DOMAIN 쪽 CAN을 mock SDK로 구동한다. `bcm_node.b14_entry`와 `*_entry`는 `Net_Init`/`Net_MainFunction` 경로다.
