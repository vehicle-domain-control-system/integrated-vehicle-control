# CIS API — 센싱·Pi 관측 취합과 허용 중계

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

권장 프로필과 실제 연동 검증 전 조건을 구분한다. 각 API의 호출·품질·시간 조건을 적용한다.

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래는 각 노드 안의 로컬 Application/Network 경계다. 제공 DOMAIN C2·ESP32 Gateway 헤더와 달리 해당 노드의 실제 설치/firmware 선언은 확인되지 않았다. 기존 Port가 동일 책임을 충족하면 그 Port에 아래 계약을 매핑하고 공개 wrapper를 중복 설치하지 않는다.

## 송신·수신 함수 빠른 찾기

| 반환형·전체 원형 | 방향 / 메시지 | 호출자 → 구현 담당 후보 |
|---|---|---|
| [`void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);`](#api-cisapp_ondomainpermission) | Network→App · `DOMAIN_PERMISSION` | CIS Network 수신 Dispatcher → CIS 전원 허용 소비 Application 연결 |
| [`NetSubmitResult CisNet_PublishRear(const NetCisRear *message);`](#api-cisnet_publishrear) | App→Network · `CIS_REAR` | CIS 초음파 관측 모듈 → CIS Network |
| [`NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);`](#api-cisnet_publishenvironment) | App→Network · `CIS_ENVIRONMENT` | CIS 환경 측정 모듈 → CIS Network |
| [`NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);`](#api-cisnet_publishoccupant) | App→Network · `CIS_OCCUPANT` | CIS Pi 관측 취합 모듈 → CIS Network |
| [`NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message);`](#api-cisnet_publishstatus) | App→Network · `CIS_STATUS` | CIS 기능·진단 모듈 → CIS Network |
| [`void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta);`](#api-cisapp_onpivisionresult) | Network→App · `P_VISION_RESULT` | CIS UART-C 수신 Dispatcher → CIS Pi 관측 취합 Application 연결 |
| [`void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta);`](#api-cisapp_onpivisionstatus) | Network→App · `P_VISION_STATUS` | CIS UART-C 수신 Dispatcher → CIS 비전 기능·고장 Application 연결 |
| [`NetSubmitResult CisNetPriv_RelayPiPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);`](#api-cisnetpriv_relaypipermission) | NETWORK_PRIVATE · `P_PERMISSION` | CIS permission Dispatcher → CIS Network 내부 relay |

송신은 비차단 로컬 인계이며 [반환/실패·수명 공통 규칙](../../04_COMMON/20_API_USAGE_AND_FAILURE.md)을 적용한다. 수신은 owner 처리 문맥(superloop 또는 task)에서 등록된 소비자에 전달하며 ISR에서 의미 API를 직접 호출하지 않는다. 실제 task/queue/driver/OS/SDK 배치는 확정하지 않는다. **`NET_SUBMIT_OK ≠ CAN/UART 송신 완료 ≠ 상대 ECU ACCEPTED ≠ 차량 DONE`**.

<a id="local-call-contract"></a>
## 로컬 호출·수명·실패 계약

<a id="local-tx-return"></a>
### 송신 반환

접수 전 INVALID_ARGUMENT/NOT_READY/NO_CAPACITY는 그 항목이 접수되지 않았다는 반환이다. 이를 접수 후 최종 전송 실패 사건으로 중복 생성하지 않는다. DOMAIN의 기존 `DomainIf_RxNetworkLocalFailure()`는 DOMAIN 측 계약이며 CIS나 Pi에 같은 공개 함수를 설치하지 않는다. 정상 접수 뒤 실패·만료를 관측하는 통신 처리와 주기 상태·Query 실패 경로는 각각 분리한다.

**반환·실패:** `NET_SUBMIT_OK`=필요한 필드의 로컬 복사/수용 완료, `NET_SUBMIT_INVALID_ARGUMENT`=NULL·예약값/범위·필드 조합 오류, `NET_SUBMIT_NOT_READY`=필요한 링크/현재 원 문맥 미준비, `NET_SUBMIT_NO_CAPACITY`=유한 보관 공간 없음. OK는 전송 완료·상대 수신·차량 DONE이 아니다. 정상 형식 INVALID/STALE 자료는 품질을 보존하여 수용할 수 있다.

<a id="local-rx-return"></a>
### 수신 전달

통신 처리 규칙 — 같은 수용 BOOT에서 같은 전송 순번·같은 raw인 정확한 중복은 수신감시를 갱신하지 않는다. 새 TX_SEQUENCE/LINK_SEQUENCE로 수용한 정상 반복은 같은 원 관측이라도 기존 수신 콜백으로 전달하여 기능 수신감시를 갱신한다. 원 생성시각·관측번호는 유지하고 현재 AGE는 경과에 따라 증가한다. 콜백 생략은 별도 감시 경로가 같은 수용 사실을 전달한다는 연결 증거가 있을 때만 허용한다. CRC/길이/코드 오류와 구세대는 감시 근거가 아니다. UART-C Pi Result/Status는 기존 두 CisApp 수신 콜백의 감시 연결을 따른다. CAN에서 같은 원 관측·같은 값·같은 품질인 정상 반복은 감시/AGE에 반영하되 C2의 새 관측으로 재발행하지 않는다. 확인된 품질 저하는 품질 저하 인계 분기로 별도 처리한다. [공통 수용 규칙](../../04_COMMON/10_DATA_AND_ID_RULES.md#section-repeat-and-quality)을 호출 경계에 적용한다.

정상 형식의 INVALID/NO_DATA/STALE 보고와 CRC·길이·예약 코드 오류를 구분한다. 전자는 현재 품질을 전달할 자료이며, 후자를 버렸다고 마지막 정상 관측의 원 AGE가 다시 시작되지는 않는다. 후속 처리에 필요한 message·meta·수신 시각은 콜백 반환 전에 복사한다.

**반환·실패:** `void`는 의미 전달 notification이며 ECU ACCEPTED/DONE을 반환하는 API가 아니다. 필수 소비자가 연결·준비된 owner 처리 문맥에서만 호출한다. CRC/length/예약 코드/수용되지 않은 세대/명령 Age 오류는 Network에서 격리하며 실제 기능 Result를 합성하지 않는다. 정상 형식의 INVALID/FAULT 상태 전달은 허용한다. 소비자 연결 실패의 실제 진단/무효화 경계는 BINDING_OPEN이다.

<a id="pending-current-value"></a>
### 현재값 보관

같은 의미 key의 아직 송신을 시작하지 않은 현재값은 최신 평가로 교체할 수 있다. 이미 송신한 원 관측을 새 관측으로 만들거나 필요한 discrete 결과/발생을 덮어쓰지 않는다. 실제 scheduler/capacity/전송 후 실패 관측은 BINDING_OPEN이다.

아래 상대편 `DomainIf_*`는 DOMAIN MCU 안의 제공 C2 선언이다. 이 노드가 함수 주소를 직접 호출하지 않는다. 양쪽 로컬 API 사이에는 CAN Codec/typed 변환이 있으며 실제 Port 바인딩은 BINDING_OPEN이다.

<a id="api-cisapp_ondomainpermission"></a>
## `CisApp_OnDomainPermission()`

후방 활성용 원 Permission은 약 200ms 재평가·500ms 사용 기한을 적용한다. CAN 정상 프레임 수신 중단 150ms와 원 평가 만료 500ms를 별도 검사한다. 미수신·만료를 실제 기능에 반영할 호출/주기 확인 지점은 구현 연결에서 확인하며 새 통지 API를 임의로 추가하지 않는다. 소유권은 Network의 검증·전달과 CIS의 후방 power_gate로 나눈다.

```c
void CisApp_OnDomainPermission(
    const NetDomainPermission *message,
    const NetCanRxMeta *meta
);
```

**호출 주체:** CIS Network 수신 Dispatcher. **구현 담당 후보:** CIS 전원 허용 소비 Application 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetDomainPermission *` · IN/read-only · OUT 없음 | [`NetDomainPermission`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission) 전체 의미 record. NULL 불가. Network 검증 후 CIS 전원 허용 소비 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetCanRxMeta *` · IN/read-only | [`NetCanRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** DOMAIN_PERMISSION 수신의 vehicle_power_permission/power 품질·원 Age/새 평가를 소비한다. operation/window 정보가 같은 frame에 존재해도 CIS 전원 정책의 새 원천이 아니다. 원 DOMAIN generation은 meta.source에서 검증한다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** 같은 수신 의무에서 Pi relay는 private CisNetPriv_RelayPiPermission이 담당한다. App callback이 허용을 재발급하지 않는다.

**추가 조건:** FAILED/NO_DATA 시 기존 ALLOW 즉시 철회는 DOMAIN Core 담당자 구현 완료 보고다. 원인별 UNKNOWN의 C2 Quality 투영과 Network→CIS 전달은 수용·연동 검증 대기다. 검증된 무효 품질이 오면 원 성공 평가 Sequence가 같아도 품질 하향을 누락하지 않고 과거 ALLOW 사용을 제한한다. 정상 DENY/OK와 UNKNOWN/비정상 품질을 구분하고, CAN 합성 UPDATE_SEQUENCE를 DOMAIN 원 성공 번호로 보지 않는다. [실패·순번 분리](../../07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)

**경로·상대 경계:** [`DOMAIN_PERMISSION`](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · 원 송신 [`DomainIf_TxCisPowerPermission()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishrear"></a>
## `CisNet_PublishRear()`

```c
NetSubmitResult CisNet_PublishRear(
    const NetCisRear *message
);
```

**호출 주체:** CIS 초음파 관측 모듈. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisRear *` · IN/read-only · OUT 없음 | [`NetCisRear`](10_DATA.md#type-netcisrear) 전체 의미 record. NULL 불가. CIS 초음파 관측 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 거리/상태·품질/사유·새 관측 sequence/원 Age를 제출한다. NO_OBJECT와 UNAVAILABLE/FAULT/RECOVERING/INACTIVE를 구분하며 65535를 실제 거리로 해석하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** CAUTION/EMERGENCY/CLEAR는 DOMAIN 판단이다. 통신 재전송은 새 거리 관측이 아니다.

**경로·상대 경계:** [`CIS_REAR`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) · 반대편 수신 [`DomainIf_RxCisRear()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishenvironment"></a>
## `CisNet_PublishEnvironment()`

각 값의 원 품질·Sequence·AGE를 독립 제출한다. DOMAIN의 온도·습도 소비 기한 2500ms는 적용값이고, 조도 1000ms는 미결이다. 기존 C2 환경 구조를 우선 유지하는 범위와 DOMAIN 저장값 무효화 확인은 [환경 원본 최신성](10_DATA.md#환경-원본-최신성--o20)을 따른다.

```c
NetSubmitResult CisNet_PublishEnvironment(
    const NetCisEnvironment *message
);
```

**호출 주체:** CIS 환경 측정 모듈. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisEnvironment *` · IN/read-only · OUT 없음 | [`NetCisEnvironment`](10_DATA.md#type-netcisenvironment) 전체 의미 record. NULL 불가. CIS 환경 측정 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 온도·습도·조도의 값/sequence/Age/validity/reason을 독립적으로 제출한다. 해당 값을 갱신하지 않은 센서의 Age·순번을 다른 센서 갱신으로 초기화하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

환경 원 Sequence는 새 BOOT의0을 구900에 이어901로 바꾸지 않는다. 일반 NO_DATA는 재부팅/순번 리셋 명령이 아니다. DOMAIN 측의 구 쓰기 격리 → DOMAIN 소유 private BOOT 통지 후보 → 무효화 저장 완료 확인 → 새 원 관측 전달은 [수용 대기인 저장 순서](../../07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)다. private 통지의 정확한 API·동기/비동기 완료 방식은 미확정이며 CIS 공개 함수나 Wire를 추가하지 않는다. 이 송신 API의 local OK로 DOMAIN VSM 완료를 입증하지 않는다.

**결과 사용:** 습도 실패가 온도/조도 정상 관측을 무효화하는 근거는 아니다. 제안 스케일과 실제 물리 센서 범위는 구분한다.

**경로·상대 경계:** [`CIS_ENVIRONMENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · 반대편 수신 [`DomainIf_RxCisCabinEnvironment()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishoccupant"></a>
## `CisNet_PublishOccupant()`

같은 원 관측의 품질이 저하되면 원 Pi 값·BOOT/VISION_SEQUENCE·생성 기준점을 유지한 채 현재 품질과 실제 제한 근거를 제출한다. 현재 AGE는 경과에 따라 증가하며 순번 증가·AGE=0으로 새 관측을 꾸미지 않는다. 높은 CAN TX_SEQUENCE만으로 원값 변경이나 INVALID→VALID를 허용하지 않는다. copy/인계가 실패하면 원 raw·Meta·품질·순서 중 일부만 먼저 commit하지 않는다. 기존 C2 경로의 DOMAIN 저장·Getter까지 같은 수용 결과로 연결하는 실제 구현은 검증 대기다.

```c
NetSubmitResult CisNet_PublishOccupant(
    const NetCisOccupant *message
);
```

**호출 주체:** CIS Pi 관측 취합 모듈. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisOccupant *` · IN/read-only · OUT 없음 | [`NetCisOccupant`](10_DATA.md#type-netcisoccupant) 전체 의미 record. NULL 불가. CIS Pi 관측 취합 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 같은 occupant update에 연결한 Pi 원 Boot/vision_sequence/원 Age·presence/count와 CIS 최종 품질·vision_function_status를 제출한다. 원 Pi record는 cache에 보존한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** [Pi 원 관측과 CIS 제한 평가](10_DATA.md#pi-원-관측과-cis-추가-평가)의 권장 프로필을 따른다. 같은 판정의 두 필드가 각각 사용 가능할 때만 상호 모순을 검사한다. 이후 실제 수신 중단·만료·확인된 고장은 현재 품질에 반영하되, 이전 Pi 원 사유와 원 AGE를 보존한다. 기능 상태 변환과 최종 C2 연결은 구현 검증이 남아 있다.

**경로·상대 경계:** [`CIS_OCCUPANT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · 반대편 수신 [`DomainIf_RxCisOccupant()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishstatus"></a>
## `CisNet_PublishStatus()`

```c
NetSubmitResult CisNet_PublishStatus(
    const NetCisStatus *message
);
```

**호출 주체:** CIS 기능·진단 모듈. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisStatus *` · IN/read-only · OUT 없음 | [`NetCisStatus`](10_DATA.md#type-netcisstatus) 전체 의미 record. NULL 불가. CIS 기능·고장 평가 모듈이 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 기능별 상태·UART/CAN 경로 상태·현재 fault/이번 기동 최근 fault/발생 번호/영향 기능·새 상태 sequence/Age를 제출한다. 실제 인터페이스 상태는 Network 관측에서 제공한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** 이 상태 평가만으로 과거 Pi/환경 관측을 새 VALID로 갱신하지 않는다. 별도 CIS_FAULT CAN frame을 생성하지 않는다.

**경로·상대 경계:** [`CIS_STATUS`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-status) · 반대편 수신 [`DomainIf_RxCisStatus()`](../DOMAIN/20_API.md).

<a id="api-cisapp_onpivisionresult"></a>
## `CisApp_OnPiVisionResult()`

```c
void CisApp_OnPiVisionResult(
    const NetVisionResult *message,
    const NetUartRxMeta *meta
);
```

**호출 주체:** CIS UART-C 수신 Dispatcher. **구현 담당 후보:** CIS Pi 관측 취합 Application 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetVisionResult *` · IN/read-only · OUT 없음 | [`NetVisionResult`](../PI/10_DATA.md#type-netvisionresult) 전체 의미 record. NULL 불가. Network 검증 후 CIS Pi 관측 취합 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetUartRxMeta *` · IN/read-only | [`NetUartRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netuartrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** Pi 원 Boot/vision sequence/관측 Age, presence/count와 각 원 validity/reason, 동반 Pi 기능/고장을 받는다. CRC/길이/세대/순서 검증 후 전달하되 정상 형식 INVALID도 보존한다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** 후속 CAN 취합 시 원 identity/값과 CIS 추가 평가를 구분한다. RX callback 수신 시각을 새 Pi 관측 생성시각으로 쓰지 않는다.

**경로·상대 경계:** [`P_VISION_RESULT`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-result) · 원 송신 [`PiNet_PublishVisionResult()`](../PI/20_API.md). 양쪽 로컬 API 사이에는 UART-C Codec/프레임이 존재한다. 실제 serial/consumer 바인딩은 BINDING_OPEN이다.

<a id="api-cisapp_onpivisionstatus"></a>
## `CisApp_OnPiVisionStatus()`

```c
void CisApp_OnPiVisionStatus(
    const NetVisionStatus *message,
    const NetUartRxMeta *meta
);
```

**호출 주체:** CIS UART-C 수신 Dispatcher. **구현 담당 후보:** CIS 비전 기능·고장 Application 연결. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Network → Application(RX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetVisionStatus *` · IN/read-only · OUT 없음 | [`NetVisionStatus`](../PI/10_DATA.md#type-netvisionstatus) 전체 의미 record. NULL 불가. Network 검증 후 CIS 비전 기능·고장 평가 담당에게 전달, 반환까지 borrowed; 보관할 의미와 시간근거는 App이 복사. |
| `meta` | `const NetUartRxMeta *` · IN/read-only | [`NetUartRxMeta`](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netuartrxmeta)는 수신 Network 생산. 로컬 수신 시각/그 홉 전송 순번; 원 관측 sequence/현재 Session/command ID로 대체하지 않음. callback 반환까지 borrowed. |

**호출·사용:** Pi 독립 status_sequence/status Age·기능 상태/현재·최근 fault와 영향 범위를 받는다. Pi 상태를 CIS 평가에 연결하는 변환은 명시 mapping 후보이며 raw cast가 아니다.

**반환·실패:** [수신 전달](#local-rx-return)을 적용한다. `void`는 실제 실행·소비 성공이 아니며 오류를 기능 Result로 합성하지 않는다.

**결과 사용:** READY/복구 수신은 새 카메라 판정이 아니므로 이전 presence/count 품질을 승격하지 않는다.

**경로·상대 경계:** [`P_VISION_STATUS`](../../03_COMMUNICATION/30_UART_C.md#message-p-vision-status) · 원 송신 [`PiNet_PublishVisionStatus()`](../PI/20_API.md). 양쪽 로컬 API 사이에는 UART-C Codec/프레임이 존재한다. 실제 serial/consumer 바인딩은 BINDING_OPEN이다.

<a id="api-cisnetpriv_relaypipermission"></a>
## `CisNetPriv_RelayPiPermission()` — NETWORK_PRIVATE

새 DOMAIN 평가·수용한 원 세대 변경·품질 제한 발생 시 중계 record를 갱신해 가능한 즉시 제출한다. 같은 원 평가의 500ms 반복은 relay 순번을 유지하고 AGE만 누적한다. DOMAIN 후방 원 기한 500ms와 UART-C 수신 중단 1500ms는 별개다. `USE_LIMIT`의 기존 1500ms 후보를 500ms로 바꾸는 것은 조건부 권장 프로필이며 최종 송신값은 미확정이다.

```c
NetSubmitResult CisNetPriv_RelayPiPermission(
    const NetDomainPermission *permission,
    const NetCanRxMeta *meta
);
```

**호출 주체:** CIS permission Dispatcher. **구현 담당 후보:** CIS Network 내부 relay. 실제 Port/consumer 연결은 BINDING_OPEN이다.

| 인자 | 정확한 타입 / IN·OUT | 책임·유효·수명 |
|---|---|---|
| `permission` | `const NetDomainPermission *` · IN/read-only · OUT 없음 | 현재 검증된 [DOMAIN 원 허용](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission). power 품질·원 source Age/revision을 복사한다. App이 생성한 별도 Pi 허용 객체를 입력받지 않는다. |
| `meta` | `const NetCanRxMeta *` · IN/read-only | 수신 DOMAIN generation·수신시각. relay 수용 전에 필요한 근거를 복사; 반환 후 외부 포인터 보관 금지. |

CIS permission Dispatcher의 owner 처리 문맥에서 `CisApp_OnDomainPermission()`과 이 내부 relay에 같은 검증된 원 입력을 분배한다. Network는 [NetPiPermission](../PI/10_DATA.md#type-netpipermission)의 `cis_boot_id`를 runtime에서 채우고, 새 relay 의미 평가에서만 update_sequence를 바꾼다. 반복 전송은 순번 유지·Age 증가다. **DOMAIN 원 허용을 제한하는 유효성 검사는 수행할 수 있으나 새 허용 판단을 생성하지 않는다.**

반환은 `NET_SUBMIT_OK/NET_SUBMIT_INVALID_ARGUMENT/NET_SUBMIT_NOT_READY/NET_SUBMIT_NO_CAPACITY`의 로컬 수용 결과다. NULL/형식 오류, 수용된 DOMAIN 문맥 부재, 유한 보관 미확보를 구분한다. 정상 형식의 불허/STALE/INVALID도 명시 상태로 보관·전달할 수 있다. 이전 허용을 새 OK/age0으로 재사용하지 않고, 원 DOMAIN 세대 변경/만료 시 기존 relay 허용을 무효화한다. 수신 중단 시 Pi consumer가 만료를 확인하는 supervision 연결은 BINDING_OPEN이다.

`USE_LIMIT=1500ms`는 기존 후보, `500ms`는 같은 후방 Power 원본을 중계한다는 근거와 양단 바이트·만료 검증을 전제로 한 조건부 권장 프로필이다. 원 DOMAIN 재평가·원 기한은 약 200ms·500ms다. relay 사유의 구체적인 변환, 순번 rollover, 실제 실패/무수신 관측 연결은 검증이 남아 있다. SOURCE_AGE에는 원 판단 이후 아직 반영하지 않은 구간의 경과만 한 번 누적한다.

[DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)에서 파생하여 [P_PERMISSION](../../03_COMMUNICATION/30_UART_C.md#message-p-permission)으로 전달하고, 반대편은 [PiApp_OnCisPermission](../PI/20_API.md#api-piapp_oncispermission)이다. 같은 원 허용의 App 소비와 내부 Pi relay를 나누며 공개 Pi 허용 생성 API를 중복 설치하지 않는다. 후방 원 기한 500ms와 CAN 수신 중단 150ms·UART-C 반복 500ms/수신 중단 1500ms, 별도 USE_LIMIT 후보의 차이는 [Pi 허용 소비](../PI/10_DATA.md#허용-소비와-원본-age)와 [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)을 함께 확인한다. **PROPOSED_DESIGN · BINDING_OPEN**.

** relay 확인 대기:** 실패 무효화 권장안을 DOMAIN이 수용해 품질 하향을 전달하면 같은 원 성공 Sequence여도 이 private 경로가 누락시키지 않아야 한다. 기존 relay 갱신·즉시 제출 요청을 사용하고 원 시각/AGE를 유지한다. 500ms 반복·다음 성공 평가까지 품질 하향을 유예하지 않는 안이며 Queue 적체·송신 실패 뒤 과거 ALLOW가 되살아나지 않는지 검증한다. Pi 실제 수신·적용은 local OK로 보장하지 않는다. [번호 공간과 상태](../../04_COMMON/10_DATA_AND_ID_RULES.md#delta-permission-sequences)

## 사용 순서와 구현 한계

1. Pi 결과/상태는 각각 `CisApp_OnPiVisionResult/Status`에서 독립 수명으로 받는다. 원 record를 cache에 복사하고 원 Age/기동·관측 순번을 보존한다.
2. CIS 추가 평가를 같은 occupant update와 연결해 `CisNet_PublishOccupant()`로 제공한다. 환경/후방/진단은 각각 독립 생성·품질로 보고한다.
3. DOMAIN 허용은 CIS App이 소비하고 private relay가 Pi로 전달한다. 공개 별도 Pi 허용 생성/조회 API는 없다.

```c
/* 설계 사용 예: Pi 원 identity와 CIS 평가를 연결한 record다. */
NetSubmitResult local = CisNet_PublishOccupant(&evaluated_occupant);
NetSubmitResult sensing = CisNet_PublishRear(&measured_rear);
/* 두 성공은 CAN peer 수신·DOMAIN 위험 판단·VSS 경고 성공이 아니다. */
```

예제는 설계 사용 흐름이며 실제 함수 구현/실행 시험이 아니다. Age 기준점·consumer 등록/무효화·통신 health 전달·capacity/재시도 연결은 BINDING_OPEN이다. MOBILE Query는 DOMAIN의 기존 snapshot getter를 사용하며 BCM/CIS/Pi에 새 원격 Query 메시지/API를 설치하지 않는다. 실제 SDK/Firmware/Codec/Board/무선 시험 **NOT_EXECUTED**.
