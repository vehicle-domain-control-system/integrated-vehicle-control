# VSS 노드 Network 통합 메모

VSS 노드(profile 6) 펌웨어에 Network를 붙일 때 지켜야 할 사항만 모았다. 설계 근거는 `docs/network/`이다.

## App 경계 (정본 헤더)

- `include/network/app/net_if_common.h`: `04_COMMON` 공통 타입을 그대로 옮긴 것.
- `include/network/app/net_if_vss.h`: `02_NODES/VSS/20_API.md` 원형을 그대로 옮긴 것.
- VSS 앱도 이 두 헤더를 그대로 쓴다. 별도 사본을 만들면 struct 배치가 어긋날 수 있다.

| 함수 | 구현 | 호출 문맥 |
|---|---|---|
| `VssApp_OnDomainEvent` | VSS 앱 | `NetNode_Step()` 안. ISR 아님 |
| `VssApp_OnDomainWarningState` | VSS 앱 | `NetNode_Step()` 안. ISR 아님 |
| `VssNet_PublishStatus` | Network | 슈퍼루프 문맥. 콜백 안에서 불러도 됨(아직 나가지 않은 제출은 최신 것으로 교체) |

BOOT_ID와 TX_SEQUENCE는 Network(M04 `net_upper_publish.c`)가 프레임마다 쓴다. 앱은 채우지 않는다.

## VSS_STATUS(0x340) 송신 규칙

- **0x340은 앱이 주기적으로 제출하고, Network는 반복하지 않는다.** VSS 앱은 상태가 바뀌면 즉시, 아니면 250 ms마다 새 평가(새 `update_sequence`)로 제출한다. Network는 제출마다 프레임 1건을 보낸다. M04 주기 표에 0x340을 등록하지 않는다. 등록하면 같은 주기에 프레임이 두 번 나간다.
- TX_SEQUENCE는 1부터 프레임마다 +1이다. `0xFFFFFFFF`를 쓴 뒤에는 송신을 멈추고(HOLD) `sequence_hold`를 올리며, 제출은 `NET_SUBMIT_NOT_READY`를 받는다.
- **대기 Age:** 제출 시각부터 실제로 장치에 넘기는 시각까지 기다린 시간을 SOURCE_AGE에 더한다(65535 포화). 보통은 다음 `NetNode_Step()`에서 바로 나가므로 더할 것이 거의 없다. 직전 0x340 프레임이 아직 TX MB에 있으면 새 제출은 그 프레임이 끝날 때까지 기다리고, 기다린 시간이 더해진다.
- 진단: `NetNodeDiag.status_tx`(M04 슬롯). `sent`, `replaced`, `pending_age_added_ms_max`, `sequence_hold`를 본다. `periodic_sent`는 항상 0이다.

## 슈퍼루프

```c
/* Clock/Port/IntCtrl 초기화 후 */
Board_TimeInit();                 /* NET_BOARD_NOW_MS_HOOK 를 쓰는 경우 */
NetNode_Init(&cfg);               /* FlexCAN 시작 */
for (;;) { NetNode_Step(); App_Service(); }
```

## 보드 프로젝트에 넣을 소스

- 제품 C 소스 목록의 정본은 `tools/s32ds_sources.txt`다. host 빌드(`NETWORK_BUILD_RTD_MOCK`)도 같은 목록을 읽어 링크까지 확인한다.
- RTD backend는 두 파일이다. `src/platform/rtd/net_hal_rtd.c`(시간원, 임계구역, 시작 순서, backend ops 등록)와 `src/platform/rtd/net_hal_rtd_can.c`(FlexCAN RX/TX/abort/error 콜백). 둘은 비공개 헤더 `net_hal_rtd_internal.h`로만 연결된다.
- RTD 헤더를 include하는 Network 파일은 `src/platform/rtd/` 안에만 있다.

## 보드 설정 요구 (`src/platform/rtd/net_board_cfg.h`)

- ConfigTools의 FlexCAN 콜백 이름(`NET_BOARD_CAN_CALLBACK`, `NET_BOARD_CAN_ERROR_CALLBACK`)은 HAL이 정의한다. 같은 이름을 정의하는 다른 파일은 빌드에서 빼야 한다.
- MB 배치 기본값: RX 0..3(마스크 0, 모든 표준 ID 수신 후 registry로 걸러냄), TX 8..11. `max_num_mb`(344=16) 안에 들어가야 한다.
- 시간원 둘 중 하나를 고른다.
  - OsIf "Use System Timer" ON
  - `NET_BOARD_NOW_MS_HOOK=<함수>`: 단조 증가 ms를 `uint64_t`로 반환하는 함수. ISR과 루프 양쪽에서 안전해야 한다.
  - 둘 다 없으면 컴파일 오류(`OSIF_COUNTER_SYSTEM` 미정의)가 난다.
- `NetNodeConfig.boot_id`는 0이 아니어야 하고 재부팅마다 바뀌어야 한다. 값을 어디서 얻을지는 아직 HOLD다(시험대에서는 상수 사용).

## 제한

- 버스오프 자동 복구는 없다. bus-off가 나면 CAN 링크가 잠기고, 보드를 리셋해야 다시 송수신한다.
- VSS 노드는 UART 링크와 다른 노드의 facade를 쓰지 않는다.
