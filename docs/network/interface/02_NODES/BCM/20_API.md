# BCM API — 목표를 받고 실제 결과·상태를 보고

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

**BCM/DOMAIN 확인 대기:** Door·Climate·Interior Light는 각각 원본 Command ID·명령 종류·기동 세대에 연결되는 새로운 실제 상태, 품질·Fault 및 원 관측 시각으로 확인 조건을 정해야 한다. 도어 증거를 공조·조명에 일괄 적용하지 않는다. DONE·CAN 수신·Network 접수만으로 CONFIRMED를 만들지 않으며, 현재 Wire로 충분한 근거가 없으면 해당 결과는 UNCONFIRMED로 제한한다. 새 confirmation 필드나 Wire 값은 추가하지 않는다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래는 각 노드 안의 로컬 Application/Network 경계다. 제공 DOMAIN C2·ESP32 Gateway 헤더와 달리 해당 노드의 실제 설치된 펌웨어의 선언은 확인되지 않았다. 기존 Port가 동일 책임을 충족하면 그 Port에 아래 계약을 매핑하고 공개 wrapper를 중복 설치하지 않는다.

## 송신·수신 함수 빠른 찾기

| 반환형·전체 원형 | 방향 / 메시지 | 호출자 → 구현 담당 후보 |
|---|---|---|
| [`void BcmApp_OnDomainDoorCommand(const NetBcmDoorCommand *message, const NetCanRxMeta *meta);`](#api-bcmapp_ondomaindoorcommand) | Network→App · `BCM_DOOR_COMMAND` | BCM Network 수신 Dispatcher → BCM 도어 Application 입력 연결 |
| [`void BcmApp_OnDomainClimateCommand(const NetBcmClimateCommand *message, const NetCanRxMeta *meta);`](#api-bcmapp_ondomainclimatecommand) | Network→App · `BCM_CLIMATE_COMMAND` | BCM Network 수신 Dispatcher → BCM 공조 Application 입력 연결 |
| [`void BcmApp_OnDomainLightCommand(const NetBcmLightCommand *message, const NetCanRxMeta *meta);`](#api-bcmapp_ondomainlightcommand) | Network→App · `BCM_LIGHT_COMMAND` | BCM Network 수신 Dispatcher → BCM 실내 조명 Application 입력 연결 |
| [`NetSubmitResult BcmNet_SendResult(const NetBcmResult *message);`](#api-bcmnet_sendresult) | App→Network · `BCM_RESULT` | BCM 명령 실행 모듈 → BCM Network |
| [`NetSubmitResult BcmNet_SendEvent(const NetBcmEvent *message);`](#api-bcmnet_sendevent) | App→Network · `BCM_EVENT` | BCM 보호·복구 평가 모듈 → BCM Network |
| [`NetSubmitResult BcmNet_PublishDoorState(const NetBcmDoorState *message);`](#api-bcmnet_publishdoorstate) | App→Network · `BCM_DOOR_STATE` | BCM 도어 상태 평가 모듈 → BCM Network |
| [`NetSubmitResult BcmNet_PublishClimateState(const NetBcmClimateState *message);`](#api-bcmnet_publishclimatestate) | App→Network · `BCM_CLIMATE_STATE` | BCM 공조 상태 평가 모듈 → BCM Network |
| [`NetSubmitResult BcmNet_PublishLightState(const NetBcmLightState *message);`](#api-bcmnet_publishlightstate) | App→Network · `BCM_LIGHT_STATE` | BCM 실내 조명 적용 모듈 → BCM Network |
| [`NetSubmitResult BcmNet_PublishStatus(const NetBcmStatus *message);`](#api-bcmnet_publishstatus) | App→Network · `BCM_STATUS` | BCM 상태·진단 모듈 → BCM Network |
| [`void BcmNetPriv_OnDomainPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);`](#api-bcmnetpriv_ondomainpermission) | NETWORK_PRIVATE · `DOMAIN_PERMISSION` | BCM Network 수신 Dispatcher → BCM Network 내부 감시 |

송신은 비차단 로컬 인계이며 [반환/실패·수명 공통 규칙](../../04_COMMON/20_API_USAGE_AND_FAILURE.md)을 적용한다. 수신은 담당 주체의 처리 문맥(Superloop 또는 태스크)에서 등록된 소비자에 전달하며 ISR에서 의미 API를 직접 호출하지 않는다. 실제 task/queue/driver/OS/SDK 배치는 확정하지 않는다. **`NET_SUBMIT_OK ≠ CAN/UART 송신 완료 ≠ 상대 ECU ACCEPTED ≠ 차량 DONE`**.

<a id="local-call-contract"></a>
## 로컬 호출·수명·실패 계약

<a id="local-tx-return"></a>
### 송신 반환

**반환·실패:** `NET_SUBMIT_OK`=필요한 필드의 로컬 복사/수용 완료, `NET_SUBMIT_INVALID_ARGUMENT`=NULL·예약값/범위·필드 조합 오류, `NET_SUBMIT_NOT_READY`=필요한 링크/현재 원 문맥 미준비, `NET_SUBMIT_NO_CAPACITY`=유한 보관 공간 없음. OK는 전송 완료·상대 수신·차량 DONE이 아니다. 정상 형식 INVALID/STALE 자료는 품질을 보존하여 수용할 수 있다.

<a id="local-rx-return"></a>
### 수신 전달

**반환·실패:** `void`는 의미를 전달하는 알림이며 ECU ACCEPTED/DONE을 반환하는 API가 아니다. 필수 소비자가 연결·준비된 owner 처리 문맥에서만 호출한다. CRC/length/예약 코드/수용되지 않은 세대/명령 Age 오류는 Network에서 격리하며 실제 기능 Result를 합성하지 않는다. 정상 형식의 INVALID/FAULT 상태 전달은 허용한다. 소비자 연결 실패의 실제 진단/무효화 경계는 BINDING_OPEN이다.

<a id="pending-current-value"></a>
### 현재값 보관

동일한 의미 식별자의 아직 송신을 시작하지 않은 현재값은 최신 평가로 교체할 수 있다. 이미 송신한 원 관측을 새 관측으로 만들거나 필요한 discrete 결과/발생을 덮어쓰지 않는다. 실제 scheduler/capacity/전송 후 실패 관측은 BINDING_OPEN이다.

<a id="discrete-result-event"></a>
### 개별 결과·발생 보관

개별 결과/발생은 동일한 원본 식별자를 보존하여 저장한다. 공간 부족을 기존 다른 결과/발생의 조용한 삭제로 해결하지 않는다. 실패 후 재시도는 동일 원 ID와 실제 증가한 Age를 사용하며 새 실행/새 발생을 만들지 않는다. 수용 후 terminal local failure의 기존 owner 연결은 필요성이 확인된 경우에만 적용하며 실제 sink는 BINDING_OPEN이다.

아래 상대편 `DomainIf_*`는 DOMAIN MCU 안의 제공 C2 선언이다. 이 노드가 함수 주소를 직접 호출하지 않는다. 양쪽 로컬 API 사이에는 CAN Codec/typed 변환이 있으며 실제 Port 바인딩은 BINDING_OPEN이다.

<a id="api-bcmapp_ondomaindoorcommand"></a>
## `BcmApp_OnDomainDoorCommand()`

```c
void BcmApp_OnDomainDoorCommand(
    const NetBcmDoorCommand *message,
    const NetCanRxMeta *meta
);
```

**호출 주체:** BCM Network 수신 Dispatcher. **구현 담당 후보:** BCM 도어 Application 입력 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmDoorCommand *` · IN/read-only · OUT 없음 | [`NetBcmDoorCommand`](10_DATA.md#type-netbcmdoorcommand) 전체 의미 record. NULL 불가. Network 검증 후 도어 실행 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetCanRxMeta *` · IN/read-only | [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** 신뢰 가능한 현재 DOMAIN/BCM 세대와 원 명령 문맥을 Network가 검증한 후 목표를 전달한다. LOCK/UNLOCK 범위 검사와 실제 도어 접점/상태에 따른 실행 허용·거부·보호는 BCM 책임이다. 동일 (DOMAIN Boot,command_id) 재전달은 추가 구동을 만들지 않고 기존 처리 상태/결과와 연결한다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** 실제 목표 확인·거부/실패는 BcmNet_SendResult의 BCM_RESULT로 보고한다.

**경로·상대 경계:** [`BCM_DOOR_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-command) · 원 송신 [`DomainIf_TxBcmDoorCommand()`](../DOMAIN/20_API.md).

<a id="api-bcmapp_ondomainclimatecommand"></a>
## `BcmApp_OnDomainClimateCommand()`

```c
void BcmApp_OnDomainClimateCommand(
    const NetBcmClimateCommand *message,
    const NetCanRxMeta *meta
);
```

**호출 주체:** BCM Network 수신 Dispatcher. **구현 담당 후보:** BCM 공조 Application 입력 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmClimateCommand *` · IN/read-only · OUT 없음 | [`NetBcmClimateCommand`](10_DATA.md#type-netbcmclimatecommand) 전체 의미 record. NULL 불가. Network 검증 후 공조 실행·로컬 보호 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetCanRxMeta *` · IN/read-only | [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** FAN UNKNOWN은 목표에 허용하지 않는다. 방향/출력 0~100% 형식을 확인하며 Fan 실제 피드백·방열 보호를 만족한 실행만 BCM이 선택한다. 수동/자동/사용자 설정의 최종 정책은 DOMAIN이 결정하며 BCM이 새 정책을 만들지 않는다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** BCM_CLIMATE_STATE의 지시 Fan/측정 Fan/열 출력·차단 상태를 각각 보고한다. 결과 DONE을 목표 온도 도달로 해석하지 않는다.

**경로·상대 경계:** [`BCM_CLIMATE_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-command) · 원 송신 [`DomainIf_TxBcmClimateCommand()`](../DOMAIN/20_API.md).

<a id="api-bcmapp_ondomainlightcommand"></a>
## `BcmApp_OnDomainLightCommand()`

```c
void BcmApp_OnDomainLightCommand(
    const NetBcmLightCommand *message,
    const NetCanRxMeta *meta
);
```

**호출 주체:** BCM Network 수신 Dispatcher. **구현 담당 후보:** BCM 실내 조명 Application 입력 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmLightCommand *` · IN/read-only · OUT 없음 | [`NetBcmLightCommand`](10_DATA.md#type-netbcmlightcommand) 전체 의미 record. NULL 불가. Network 검증 후 실내 조명 실행 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetCanRxMeta *` · IN/read-only | [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** 종류·밝기 0~100%·RGB를 적용한다. INTERIOR_LIGHT만 유지하며 EXTERIOR_LIGHT는 없다. 로컬 적용 성공과 실제 LED 물리 점등은 별도다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** BCM_LIGHT_STATE의 apply_result와 feedback_capability를 함께 보고한다. 현재 물리 확인 미지원(0)이다.

**경로·상대 경계:** [`BCM_LIGHT_COMMAND`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-command) · 원 송신 [`DomainIf_TxBcmInteriorLightCommand()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_sendresult"></a>
## `BcmNet_SendResult()`

```c
NetSubmitResult BcmNet_SendResult(
    const NetBcmResult *message
);
```

**호출 주체:** BCM 명령 실행 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmResult *` · IN/read-only · OUT 없음 | [`NetBcmResult`](10_DATA.md#type-netbcmresult) 전체 의미 record. NULL 불가. BCM 명령 실행 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 처리 단계가 실제로 변경되면 원 command_domain_boot/command_id를 유지해 결과를 제출한다. Network가 원 ID를 발급하지 않고 수신한 값을 참조한다. 선택 도어 event/occurrence/age/quality 근거는 같은 결과와 atomic하게 인계하며 별도 BCM_EVENT로 복제하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [개별 결과·발생 보관](#discrete-result-event)을 적용하며 원 명령/occurrence identity·증가한 Age를 보존한다.

**결과 사용:** 새 도어 완료 발생은 BCM이 확인한다. NET_SUBMIT_OK로 DONE/도어 완료/VSS 확인음을 만들지 않는다. `NetBcmResult`에는 C2의 `confirmation` 필드가 없으므로 실제 Result의 DONE만으로 CONFIRMED를 합성하지 않는다. 원 명령 echo·UNCONFIRMED 후보·실제 확인 근거는 [BCM_RESULT 계약](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result)의 PROPOSED/BINDING_OPEN을 따른다.

**경로·상대 경계:** [`BCM_RESULT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-result) · 반대편 수신 [`DomainIf_RxBcmCommandResult()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_sendevent"></a>
## `BcmNet_SendEvent()`

```c
NetSubmitResult BcmNet_SendEvent(
    const NetBcmEvent *message
);
```

**호출 주체:** BCM 보호·복구 평가 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmEvent *` · IN/read-only · OUT 없음 | [`NetBcmEvent`](10_DATA.md#type-netbcmevent) 전체 의미 record. NULL 불가. BCM 보호·복구 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 과열·Fan 불일치·복구의 실제 새 발생을 제출한다. occurrence는 BCM 발생 owner, 관련 DOMAIN 명령은 있으면 원 값을 echo하고 없으면 0이다. 같은 발생 반복은 occurrence 유지·Age 증가.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [개별 결과·발생 보관](#discrete-result-event)을 적용하며 원 명령/occurrence identity·증가한 Age를 보존한다.

**결과 사용:** 현재 fault 해제와 RECOVERY_CONFIRMED 발생은 다르며 이전 공조 목표 자동 재구동을 지시하지 않는다.

**경로·상대 경계:** [`BCM_EVENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-event) · 반대편 수신 [`DomainIf_RxBcmProtectionEvent()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_publishdoorstate"></a>
## `BcmNet_PublishDoorState()`

```c
NetSubmitResult BcmNet_PublishDoorState(
    const NetBcmDoorState *message
);
```

**호출 주체:** BCM 도어 상태 평가 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmDoorState *` · IN/read-only · OUT 없음 | [`NetBcmDoorState`](10_DATA.md#type-netbcmdoorstate) 전체 의미 record. NULL 불가. BCM 도어 평가 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 잠금/개폐/조합과 각 품질을 하나의 평가 기록으로 제출한다. 실제 새로운 평가를 수행한 경우에만 update_sequence를 갱신하고, 반복 전송은 원본 순번과 실제로 누적된 원본 AGE를 유지한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** 도어 Result와 현재 DoorState는 별도 수명이다. 현재 상태 제출 성공을 원 요청 DONE으로 읽지 않는다.

**경로·상대 경계:** [`BCM_DOOR_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-door-state) · 반대편 수신 [`DomainIf_RxBcmDoorState()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_publishclimatestate"></a>
## `BcmNet_PublishClimateState()`

```c
NetSubmitResult BcmNet_PublishClimateState(
    const NetBcmClimateState *message
);
```

**호출 주체:** BCM 공조 상태 평가 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmClimateState *` · IN/read-only · OUT 없음 | [`NetBcmClimateState`](10_DATA.md#type-netbcmclimatestate) 전체 의미 record. NULL 불가. BCM 공조 평가·보호 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** Fan 지시/실측·열 방향/출력·방열 상태의 독립 품질과 같은 평가 sequence/Age를 제출한다. 일부 실측 불가를 정상 지시값으로 대체하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** 정상 형식 INVALID/UNKNOWN 상태도 전달할 수 있다. Network 로컬 수용 여부와 실제 장치 상태를 분리한다.

**경로·상대 경계:** [`BCM_CLIMATE_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-climate-state) · 반대편 수신 [`DomainIf_RxBcmClimateState()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_publishlightstate"></a>
## `BcmNet_PublishLightState()`

```c
NetSubmitResult BcmNet_PublishLightState(
    const NetBcmLightState *message
);
```

**호출 주체:** BCM 실내 조명 적용 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmLightState *` · IN/read-only · OUT 없음 | [`NetBcmLightState`](10_DATA.md#type-netbcmlightstate) 전체 의미 record. NULL 불가. BCM 실내 조명 적용 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 조명 종류/밝기/RGB와 적용 결과/물리 확인 지원 여부·각 품질을 제출한다. 새 평가 sequence/Age를 보존한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** 현재 feedback_capability=0에서 APPLIED는 출력 지시 적용이며 실제 점등 확인 완료가 아니다.

**경로·상대 경계:** [`BCM_LIGHT_STATE`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-light-state) · 반대편 수신 [`DomainIf_RxBcmInteriorLightState()`](../DOMAIN/20_API.md).

<a id="api-bcmnet_publishstatus"></a>
## `BcmNet_PublishStatus()`

```c
NetSubmitResult BcmNet_PublishStatus(
    const NetBcmStatus *message
);
```

**호출 주체:** BCM 상태·진단 모듈. **구현 담당 후보:** BCM Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetBcmStatus *` · IN/read-only · OUT 없음 | [`NetBcmStatus`](10_DATA.md#type-netbcmstatus) 전체 의미 record. NULL 불가. BCM 상태·진단 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 현재 ECU/active fault mask·대표 code·영향 기능/복구와 평가 sequence/Age를 제출한다. 통신 fault의 실제 원천은 Network health이고 concrete 공급 경계는 BINDING_OPEN이다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** Network bus-off/무수신을 기능 FAILED로 자동 변환하지 않으며 필요한 영향 기능만 진단한다.

**경로·상대 경계:** [`BCM_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-bcm-status) · 반대편 수신 [`DomainIf_RxBcmStatus()`](../DOMAIN/20_API.md).

<a id="api-bcmnetpriv_ondomainpermission"></a>
## `BcmNetPriv_OnDomainPermission()` — NETWORK_PRIVATE

```c
void BcmNetPriv_OnDomainPermission(
    const NetDomainPermission *permission,
    const NetCanRxMeta *meta
);
```

**호출 주체:** BCM Network 수신 Dispatcher. **구현 담당 후보:** BCM Network 내부 감시. 실제 Port/consumer 연결은 BINDING_OPEN이다.

| 인자 | 정확한 타입 / IN·OUT | 책임·유효·수명 |
|---|---|---|
| `permission` | `const NetDomainPermission *` · IN · OUT 없음 | 수신한 [허용 record](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission). 원 품질·평가 sequence/Age 보존. callback 동안 borrowed. |
| `meta` | `const NetCanRxMeta *` · IN | CAN Network가 검증한 DOMAIN Boot/TX sequence·로컬 수신시각. callback 동안 borrowed. |

BCM 수신 Dispatcher가 형식/기동 검증 후 내부 supervision에 전달한다. 반환 `void`는 감시 record 전달이며 BCM App 허용/차량 실행 성공을 뜻하지 않는다. DOMAIN generation과 peer liveness는 정상 source 확인으로만 갱신하고 공유 CAN의 다른 ECU frame을 DOMAIN 생존으로 세지 않는다.

현재 BCM Application이 permission의 어느 항목을 추가 소비하는지 확인되지 않아 공개 `BcmApp_OnPermission`을 설치하지 않는다. 필요한 기존 보호/진단 owner에 health/허용 상실을 전달하는 실제 경계는 BINDING_OPEN이며 별도 공개 health API를 무더기로 추가하지 않는다. 로컬 보호는 Network 왕복과 독립이다. [DOMAIN_PERMISSION Wire](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)의 BCM 수신 의무는 유지한다. **PROPOSED_DESIGN · NETWORK_PRIVATE · BINDING_OPEN**.

## 사용 순서와 구현 한계

1. Network가 DOMAIN 세대/목표 Boot·명령 Age를 검사하고 해당 `BcmApp_OnDomain*Command`에 전체 record와 meta를 전달한다.
2. BCM 실행 owner는 원 `(DOMAIN Boot,command_id)`를 보존하고 로컬 보호/피드백으로 처리한다. callback `void`가 결과 성공을 뜻하지 않는다.
3. 같은 명령의 처리 단계는 `BcmNet_SendResult()`, 로컬 보호 새 발생은 `BcmNet_SendEvent()`로 보고한다. 현재값은 해당 `Publish*State/Status`에 제공한다.

```c
/* 설계 사용 예: 해당 owner가 평가한 전체 record를 인계한다. */
NetSubmitResult local = BcmNet_PublishDoorState(&evaluated_door_state);
/* local == NET_SUBMIT_OK는 평가 자료 복사/수용만 뜻한다. */
NetSubmitResult submitted = BcmNet_SendResult(&actual_command_result);
/* actual_command_result는 원 DOMAIN ID와 실제 실행 근거를 갖는다. */
```

예제는 설계 사용 흐름이며 실제 함수 구현/실행 시험이 아니다. Age 기준점·consumer 등록/무효화·통신 health 전달·capacity/재시도 연결은 BINDING_OPEN이다. MOBILE Query는 DOMAIN의 기존 snapshot getter를 사용하며 BCM/CIS/Pi에 새 원격 Query 메시지/API를 설치하지 않는다. 실제 SDK/Firmware/Codec/Board/무선 시험 **NOT_EXECUTED**.
