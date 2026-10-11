# CIS API — 센싱·Pi 관측 취합과 허용 중계

[전체 시작](../../00_START_HERE.md) · [개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md)

**PROPOSED_DESIGN · BINDING_OPEN.** 아래는 각 노드 안의 로컬 Application/Network 경계다. 제공 DOMAIN C2·ESP32 Gateway 헤더와 달리 해당 노드의 실제 설치/firmware 선언은 확인되지 않았다. 기존 Port가 동일 책임을 충족하면 그 Port에 아래 계약을 매핑하고 공개 wrapper를 중복 설치하지 않는다.

## 송신·수신 함수 빠른 찾기

| 반환형·전체 원형 | 방향 / 메시지 | 호출자 → 구현 담당 후보 |
|---|---|---|
| [`void CisApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);`](#api-cisapp_ondomainpermission) | Network→App · `DOMAIN_PERMISSION` | CIS Network 수신 Dispatcher → CIS 전원 허용 소비 Application 연결 |
| [`NetSubmitResult CisNet_PublishRear(const NetCisRear *message);`](#api-cisnet_publishrear) | App→Network · `CIS_REAR` | CIS 초음파 관측 담당자 → CIS Network |
| [`NetSubmitResult CisNet_PublishEnvironment(const NetCisEnvironment *message);`](#api-cisnet_publishenvironment) | App→Network · `CIS_ENVIRONMENT` | CIS 환경 측정 담당자 → CIS Network |
| [`NetSubmitResult CisNet_PublishOccupant(const NetCisOccupant *message);`](#api-cisnet_publishoccupant) | App→Network · `CIS_OCCUPANT` | CIS Pi 관측 취합 담당자 → CIS Network |
| [`NetSubmitResult CisNet_PublishStatus(const NetCisStatus *message);`](#api-cisnet_publishstatus) | App→Network · `CIS_STATUS` | CIS 기능·진단 담당자 → CIS Network |
| [`void CisApp_OnPiVisionResult(const NetVisionResult *message, const NetUartRxMeta *meta);`](#api-cisapp_onpivisionresult) | Network→App · `P_VISION_RESULT` | CIS UART-C 수신 Dispatcher → CIS Pi 관측 취합 Application 연결 |
| [`void CisApp_OnPiVisionStatus(const NetVisionStatus *message, const NetUartRxMeta *meta);`](#api-cisapp_onpivisionstatus) | Network→App · `P_VISION_STATUS` | CIS UART-C 수신 Dispatcher → CIS 비전 기능·고장 Application 연결 |
| [`NetSubmitResult CisNetPriv_RelayPiPermission(const NetDomainPermission *permission, const NetCanRxMeta *meta);`](#api-cisnetpriv_relaypipermission) | NETWORK_PRIVATE · `P_PERMISSION` | CIS permission Dispatcher → CIS Network 내부 relay |

송신은 비차단 로컬 인계이며 [반환/실패·수명 공통 규칙](../../04_COMMON/20_API_USAGE_AND_FAILURE.md)을 적용한다. 수신은 owner 처리 문맥(superloop 또는 task)에서 등록된 소비자에 전달하며 ISR에서 의미 API를 직접 호출하지 않는다. 실제 task/queue/driver/OS/SDK 배치는 확정하지 않는다. **`NET_SUBMIT_OK ≠ CAN/UART 송신 완료 ≠ 상대 ECU ACCEPTED ≠ 차량 DONE`**.

<a id="local-call-contract"></a>
## 로컬 호출·수명·실패 계약

<a id="local-tx-return"></a>
### 송신 반환

**반환·실패:** `NET_SUBMIT_OK`=필요한 필드의 로컬 복사/수용 완료, `NET_SUBMIT_INVALID_ARGUMENT`=NULL·예약값/범위·필드 조합 오류, `NET_SUBMIT_NOT_READY`=필요한 링크/현재 원 문맥 미준비, `NET_SUBMIT_NO_CAPACITY`=유한 보관 공간 없음. OK는 전송 완료·상대 수신·차량 DONE이 아니다. 정상 형식 INVALID/STALE 자료는 품질을 보존하여 수용할 수 있다.

<a id="local-rx-return"></a>
### 수신 전달

**반환·실패:** `void`는 의미 전달 notification이며 ECU ACCEPTED/DONE을 반환하는 API가 아니다. 필수 소비자가 연결·준비된 owner 처리 문맥에서만 호출한다. CRC/length/예약 코드/수용되지 않은 세대/명령 Age 오류는 Network에서 격리하며 실제 기능 Result를 합성하지 않는다. 정상 형식의 INVALID/FAULT 상태 전달은 허용한다. 소비자 연결 실패의 실제 진단/무효화 경계는 BINDING_OPEN이다.

<a id="pending-current-value"></a>
### 현재값 보관

같은 의미 key의 아직 송신을 시작하지 않은 현재값은 최신 평가로 교체할 수 있다. 이미 송신한 원 관측을 새 관측으로 만들거나 필요한 discrete 결과/발생을 덮어쓰지 않는다. 실제 scheduler/capacity/전송 후 실패 관측은 BINDING_OPEN이다.

아래 상대편 `DomainIf_*`는 DOMAIN MCU 안의 제공 C2 선언이다. 이 노드가 함수 주소를 직접 호출하지 않는다. 양쪽 로컬 API 사이에는 CAN Codec/typed 변환이 있으며 실제 Port 바인딩은 BINDING_OPEN이다.

<a id="api-cisapp_ondomainpermission"></a>
## `CisApp_OnDomainPermission()`

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

**결과 사용:** 같은 수신 의무에서 Pi relay는 private CisNetPriv_RelayPiPermission()이 담당한다. App callback이 허용을 재발급하지 않는다.

**경로·상대 경계:** [`DOMAIN_PERMISSION`](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) · 원 송신 [`DomainIf_TxCisPowerPermission()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishrear"></a>
## `CisNet_PublishRear()`

```c
NetSubmitResult CisNet_PublishRear(
    const NetCisRear *message
);
```

**호출 주체:** CIS 초음파 관측 담당자. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisRear *` · IN/read-only · OUT 없음 | [`NetCisRear`](10_DATA.md#type-netcisrear) 전체 의미 record. NULL 불가. CIS 초음파 관측 담당자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 거리/상태·품질/사유·새 관측 sequence/원 Age를 제출한다. NO_OBJECT와 UNAVAILABLE/FAULT/RECOVERING/INACTIVE를 구분하며 65535를 실제 거리로 해석하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** CAUTION/EMERGENCY/CLEAR는 DOMAIN 판단이다. 통신 재전송은 새 거리 관측이 아니다.

**경로·상대 경계:** [`CIS_REAR`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-rear) · 반대편 수신 [`DomainIf_RxCisRear()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishenvironment"></a>
## `CisNet_PublishEnvironment()`

```c
NetSubmitResult CisNet_PublishEnvironment(
    const NetCisEnvironment *message
);
```

**호출 주체:** CIS 환경 측정 담당자. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisEnvironment *` · IN/read-only · OUT 없음 | [`NetCisEnvironment`](10_DATA.md#type-netcisenvironment) 전체 의미 record. NULL 불가. CIS 환경 측정 담당자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 온도·습도·조도의 값/sequence/Age/validity/reason을 독립적으로 제출한다. 해당 값을 갱신하지 않은 센서의 Age·순번을 다른 센서 갱신으로 초기화하지 않는다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** 습도 실패가 온도/조도 정상 관측을 무효화하는 근거는 아니다. 제안 스케일과 실제 물리 센서 범위는 구분한다.

**경로·상대 경계:** [`CIS_ENVIRONMENT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-environment) · 반대편 수신 [`DomainIf_RxCisCabinEnvironment()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishoccupant"></a>
## `CisNet_PublishOccupant()`

```c
NetSubmitResult CisNet_PublishOccupant(
    const NetCisOccupant *message
);
```

**호출 주체:** CIS Pi 관측 취합 담당자. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisOccupant *` · IN/read-only · OUT 없음 | [`NetCisOccupant`](10_DATA.md#type-netcisoccupant) 전체 의미 record. NULL 불가. CIS Pi 관측 취합 담당자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

**호출·사용:** 같은 occupant update에 연결한 Pi 원 Boot/vision_sequence/원 Age·presence/count와 CIS 최종 품질·vision_function_status를 제출한다. 원 Pi record는 cache에 보존한다.

**반환·실패:** [송신 반환](#local-tx-return)을 적용한다. 품질이 INVALID/STALE인 정상 형식의 자료는 그 품질을 보존해 제출할 수 있다.

**보관·재시도:** [현재값 보관](#pending-current-value)을 적용하며 원 평가 순번·Age를 보존한다.

**결과 사용:** [Pi 원 관측과 CIS 제한 평가](10_DATA.md#pi-원-관측과-cis-추가-평가)의 PROPOSED 권장안을 따른다. 사유 우선순위·상태 mapping은 TEAM_DECISION_REQUIRED이며 미합의 조합을 VALID로 승격하거나 원 Age를 새 평가 시각으로 바꾸지 않는다.

**경로·상대 경계:** [`CIS_OCCUPANT`](../../03_COMMUNICATION/10_CAN_FD.md#message-cis-occupant) · 반대편 수신 [`DomainIf_RxCisOccupant()`](../DOMAIN/20_API.md).

<a id="api-cisnet_publishstatus"></a>
## `CisNet_PublishStatus()`

```c
NetSubmitResult CisNet_PublishStatus(
    const NetCisStatus *message
);
```

**호출 주체:** CIS 기능·진단 담당자. **구현 담당 후보:** CIS Network. 실제 Port/consumer 연결은 BINDING_OPEN이다.

**Application → Network(TX) · PROPOSED_DESIGN · BINDING_OPEN**

| 인자 | 정확한 타입 / IN·OUT | 생성·소비·유효 조건 / 수명 |
|---|---|---|
| `message` | `const NetCisStatus *` · IN/read-only · OUT 없음 | [`NetCisStatus`](10_DATA.md#type-netcisstatus) 전체 의미 record. NULL 불가. CIS 기능·고장 평가 담당자가 준비; Network는 OK 반환 전 caller와 무관하게 복사/보관. 실패는 부분 수용 없음. |

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

현재 1500ms use_limit, 원 품질+relay 검사 reason 우선순위, relay revision의 실제 발생 조건/rollover는 **TEAM_DECISION_REQUIRED**다. SOURCE_AGE는 DOMAIN 원 판단부터 누적하며 CIS 수신 후 보관→실제 UART 송신 경과를 한 번만 더한다. 실제 전송 시간 여유는 측정/합의 전 정확한 보장으로 표시하지 않는다.

[DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)에서 파생하여 [P_PERMISSION](../../03_COMMUNICATION/30_UART_C.md#message-p-permission)으로 전달하고, 반대편은 [PiApp_OnCisPermission()](../PI/20_API.md#api-piapp_oncispermission)이다. 같은 원 허용의 App 소비와 내부 Pi relay를 나누며 공개 Pi 허용 생성 API를 중복 설치하지 않는다. 원 기한·150/500/1500ms 후보의 사용 제한은 [Pi 허용 소비](../PI/10_DATA.md#허용-소비와-원본-age)와 [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission)을 함께 확인한다. **PROPOSED_DESIGN · BINDING_OPEN**.

## 사용 순서와 구현 한계

1. Pi 결과/상태는 각각 `CisApp_OnPiVisionResult/Status()`에서 독립 수명으로 받는다. 원 record를 cache에 복사하고 원 Age/기동·관측 순번을 보존한다.
2. CIS 추가 평가를 같은 occupant update와 연결해 `CisNet_PublishOccupant()`로 제공한다. 환경/후방/진단은 각각 독립 생성·품질로 보고한다.
3. DOMAIN 허용은 CIS App이 소비하고 private relay가 Pi로 전달한다. 공개 별도 Pi 허용 생성/조회 API는 없다.

```c
/* 설계 사용 예: Pi 원 identity와 CIS 평가를 연결한 record다. */
NetSubmitResult local = CisNet_PublishOccupant(&evaluated_occupant);
NetSubmitResult sensing = CisNet_PublishRear(&measured_rear);
/* 두 성공은 CAN peer 수신·DOMAIN 위험 판단·VSS 경고 성공이 아니다. */
```

예제는 설계 사용 흐름이며 실제 함수 구현/실행 시험이 아니다. Age 기준점·consumer 등록/무효화·통신 health 전달·capacity/재시도 연결은 BINDING_OPEN이다. MOBILE Query는 DOMAIN의 기존 snapshot getter를 사용하며 BCM/CIS/Pi에 새 원격 Query 메시지/API를 설치하지 않는다. 실제 SDK/Firmware/Codec/Board/무선 시험 **NOT_EXECUTED**.
