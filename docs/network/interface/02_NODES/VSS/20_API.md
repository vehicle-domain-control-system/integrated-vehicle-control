# VSS API — 송신·수신 호출 계약

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

Event·Warning 입력은 Network→Application, Status 출력은 기존 Application 통신 연결→Network다. 원형은 **PROPOSED_DESIGN / BINDING_OPEN**이고 VSS R4/R6 실제 함수 원형을 대체하지 않는다.

[노드 개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공통 의미·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

## 반환형·전체 원형 먼저 보기

```c
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```

| 함수 | 방향·구현 owner | 메시지 |
|---|---|---|
| [VssApp_OnDomainWarningState](#api-vssapp_ondomainwarningstate) | Network→Application; Application 입력 연결 구현 | [VSS_WARNING_STATE](../../03_COMMUNICATION/10_CAN_FD.md#message-vss-warning-state) |
| [VssApp_OnDomainEvent](#api-vssapp_ondomainevent) | Network→Application; Application 입력 연결 구현 | [VSS_EVENT](../../03_COMMUNICATION/10_CAN_FD.md#message-vss-event) |
| [VssNet_PublishStatus](#api-vssnet_publishstatus) | Application→Network; Network 구현 | [VSS_STATUS](../../03_COMMUNICATION/10_CAN_FD.md#message-vss-status) |
| [VssNetPriv_OnDomainPermission](#api-vssnetpriv_ondomainpermission) | Network 내부 감독 | [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) |

앞의 세 함수가 Application과 Network 사이의 호출 후보이며 마지막 함수는 Network 내부다. Event는 원 발생 identity를 소비하고 Warning은 현재 위험 근거를 소비하므로 서로 합치지 않는다. Status는 VSS 자신의 관측 보고이며 수신 callback이나 개별 재생 완료 응답을 대신하지 않는다. 전체 인자의 타입은 [Data 색인](10_DATA.md)에서 찾는다.

## 공통 호출·반환 계약

송신은 기다리지 않는 로컬 제출이다. `NET_SUBMIT_OK`는 Network가 caller와 독립인 bounded handoff를 확보해 복사·접수한 상태다. 실제 CAN 송신·ACK·상대 RX·ECU 실행 DONE은 별도다. 포인터는 호출 동안만 빌려 읽으며 OK 반환 후 caller가 저장소를 재사용할 수 있다. `NO_CAPACITY`에서 discrete Event/Result를 조용히 버리지 않고 원 identity·Age를 유지한 처리/진단 방침을 적용한다.

| 반환 | 정확한 범위·처리 |
|---|---|
| `NET_SUBMIT_OK` | local 복사·접수. 차량 DONE으로 변환 금지 |
| `NET_SUBMIT_INVALID_ARGUMENT` | NULL/미지원 code/범위·정합 오류; 동일 잘못된 데이터의 무조건 재시도 금지 |
| `NET_SUBMIT_NOT_READY` | owner·Port·Network 미준비; 기능 실행 결과를 합성하지 않음 |
| `NET_SUBMIT_NO_CAPACITY` | bounded 제출 저장소 부족; 원 identity·Age·순서/유효기한을 지킨 처리 필요 |
| RX `void` | 검증된 typed 자료의 local 전달 callback이며 접수 성공·저장성공·물리 동작 결과를 반환하지 않음 |

RX callback은 Node owner 처리 문맥에서 호출하고 ISR 직접 호출을 하지 않는다. `message/meta`는 NULL이 아니고 호출 중 읽기 전용이며 반환 뒤 필요 값은 consumer가 복사한다. 실제 callback/poll/queue 바인딩·bounded 소비 보장·수용 후 실패 sink는 BINDING_OPEN이다. `void`를 사용할 때 실제 sink가 전달을 처리할 수 있어야 하며 누락을 성공으로 간주하지 않는다. [공통 호출·비동기 실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md).

<a id="api-vssapp_ondomainwarningstate"></a>
## `VssApp_OnDomainWarningState()`

```c
void VssApp_OnDomainWarningState(const NetVssWarningState *message, const NetCanRxMeta *meta);
```

**DOMAIN 확정 차량 경고를 기존 VSS INPUT 연결에 전달한다.** PROPOSED_DESIGN / BINDING_OPEN.

**호출 주체·시점:** VSS Network가 검증한 `VSS_WARNING_STATE`를 owner 처리 문맥에서 Application의 기존 INPUT 연결에 전달한다. Application은 필요한 입력을 반환 전에 복사한다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetVssWarningState *` · IN/borrowed; [전체 Data](10_DATA.md#type-netvsswarningstate) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; VSS 기능 입력 연결이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** 세 경고 evidence와 후방 활성 meta를 독립 보존한다. 후방 거리로 위험을 다시 판단하지 않는다. format0/미지원은 후방 활성 근거 없음이며 다른 유효 경고에 영향 주지 않는다.

**반환·실패:** `void`이며 local 전달 callback 호출은 기능적 수용·저장 수용·음향 시작·완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

**경로:** DOMAIN→공유 CAN `VSS_WARNING_STATE`→VSS Network→이 함수. [DOMAIN API](../DOMAIN/20_API.md)와 [메시지 색인](../../05_MESSAGE_INDEX.md).

```c
/* decoded는 검증된 NetVssWarningState, rx_meta는 Network 수신 문맥이다. */
VssApp_OnDomainWarningState(&decoded, &rx_meta);
/* consumer가 반환 이후 필요 값을 owner 저장소로 복사한다. */
```

<a id="api-vssapp_ondomainevent"></a>
## `VssApp_OnDomainEvent()`

```c
void VssApp_OnDomainEvent(const NetVssEvent *message, const NetCanRxMeta *meta);
```

**한 번 발생한 DOMAIN 음향 의미를 기존 VSS INPUT 연결에 전달한다.** PROPOSED_DESIGN / BINDING_OPEN.

**호출 주체·시점:** VSS Network가 검증한 `VSS_EVENT`를 owner 처리 문맥에서 Application의 기존 INPUT 연결에 전달한다. Application은 원 DOMAIN 기동·occurrence·AGE와 필요한 값을 반환 전에 복사한다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetVssEvent *` · IN/borrowed; [전체 Data](10_DATA.md#type-netvssevent) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; VSS 기능 입력 연결이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** DOMAIN 기동 + occurrence로 같은 발생을 구분한다. 기한·품질·가용성을 검사하며 same occurrence를 새 음향으로 재생하지 않는다. 무발생은 통신 고장이 아니다.

**반환·실패:** `void`이며 local 전달 callback 호출은 기능적 수용·저장 수용·음향 시작·완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

**경로:** DOMAIN→공유 CAN `VSS_EVENT`→VSS Network→이 함수. [DOMAIN API](../DOMAIN/20_API.md)와 [메시지 색인](../../05_MESSAGE_INDEX.md).

```c
/* decoded는 검증된 NetVssEvent, rx_meta는 Network 수신 문맥이다. */
VssApp_OnDomainEvent(&decoded, &rx_meta);
/* consumer가 반환 이후 필요 값을 owner 저장소로 복사한다. */
```

<a id="api-vssnet_publishstatus"></a>
## `VssNet_PublishStatus()`

```c
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);
```

**현재 VSS 서비스·현재/최근 고장 보고를 DOMAIN에 제출한다.** PROPOSED_DESIGN / BINDING_OPEN.

**호출 주체·시점:** Application의 기존 FLOW→INPUT 통신 연결이 실제 보고가 생성됐을 때 호출하는 후보다. HEALTH가 Network를 직접 호출하는 새 경계는 추가하지 않는다. 보호된 원 Snapshot의 변환·복사 위치는 [Status 보고 권장안](10_DATA.md#r2-status-proposal)의 **PROPOSED / BINDING_OPEN** 범위를 따른다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetVssStatus *` · IN/borrowed; [전체 Data](10_DATA.md#type-netvssstatus) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 일관된 반영 완료 관측에 근거한 서비스·고장 보고만 제출한다. currentFaults는 완전성이 확인된 현재 목록, last_fault는 원 발생·AGE를 가진 최근 기록이다. `VSS_STATUS_REEVALUATE`로 이전 보고의 평가 번호·AGE를 새로 만들지 않는다. Snapshot→DTO owner·인계 형식은 BINDING_OPEN이며 이 인자 후보가 App 선행 변환·Network 변환을 확정하지 않는다.

**반환·실패:** 위4개 NetSubmitResult만 local 수용 상태로 사용한다. 실패 뒤 재시도해도 원 상태평가/발생/명령 식별을 바꾸거나 원 Age를 초기화하지 않는다. State/Fault와 discrete Event/Result의 서로 다른 교체·수명 규칙은 Data를 따른다. Network는 송신시 Boot/TX_SEQUENCE/실제 대기 Age를 보완하고 기능 상태·고장을 재판정하지 않는다.

**경로:** VSS App→이 함수→Network Codec→`VSS_STATUS`→DOMAIN Network 내부 `DomainNetPriv_OnVssStatus`→기존 C2 또는 미정 의미 owner. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

```c
/* observed_message는 실제 기능 owner가 만든 NetVssStatus 객체다. */
NetSubmitResult local_result = VssNet_PublishStatus(&observed_message);
/* local_result == NET_SUBMIT_OK: 로컬 인계만 완료. */
```

<a id="api-vssnetpriv_ondomainpermission"></a>
## `VssNetPriv_OnDomainPermission()`

```c
void VssNetPriv_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```

**DOMAIN_PERMISSION의 정상 수신·원 근거를 VSS Network 감독에 반영한다.** NETWORK_PRIVATE · PROPOSED_DESIGN / BINDING_OPEN.

**호출 주체·시점:** VSS Network의 검증된 CAN 수신 경로가 내부 감시를 갱신한다. Application이 호출하거나 Network가 VSS 복구 허용으로 해석하는 함수가 아니다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetDomainPermission *` · IN/borrowed; [전체 Data](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; Network 감독이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** App의 필수 permission callback을 추가하지 않는다. VSS App 실제 소비 항목·R4 연결 필요성이 확인되면 해당 담당과 조건부 경계를 확정해야 한다. Backend 복구 허용 VssRecoveryPermission으로 자동 변환하지 않는다.

**반환·실패:** `void`이며 local 전달 callback 호출은 기능적 수용·저장 수용·음향 시작·완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

**경로:** DOMAIN→공유 CAN `DOMAIN_PERMISSION`→VSS Network→이 함수. [DOMAIN API](../DOMAIN/20_API.md)와 [메시지 색인](../../05_MESSAGE_INDEX.md).

```c
/* decoded는 검증된 NetDomainPermission, rx_meta는 Network 수신 문맥이다. */
VssNetPriv_OnDomainPermission(&decoded, &rx_meta);
/* consumer가 반환 이후 필요 값을 owner 저장소로 복사한다. */
```

## 구현 연동 경계

`VssNet_PublishStatus`는 정확한 NetVssStatus 보고가 제공됐을 때 사용할 로컬 handoff 후보다. R4 Snapshot을 직접 인자로 받는 구조를 확정하지 않는다. 형식·owner 조건 확정 전 normal0/READY/Fault없음으로 누락을 보완하지 않는다. `LAST_FAULT_QUALITY`의 이력 있음0/부재3은 current service·전체 Fault 품질을 표현하지 않으므로 미확인 품질을 정상 보고로 채우지 않는다. [변환·제한 정본](10_DATA.md#r2-status-proposal) · [입력 상실·복구](10_DATA.md#vss-input-recovery). VSS App permission ingress는 consumer 필요성이 미확인이라 필수 API에서 제외했다. 하나의 DOMAIN_PERMISSION은 VSS 포함4수신 의무를 유지하며 private handler가 담당한다. 별도 VSS_FAULT frame·VSS MOBILE Query RPC·재생제어 wrapper를 신설하지 않는다.
