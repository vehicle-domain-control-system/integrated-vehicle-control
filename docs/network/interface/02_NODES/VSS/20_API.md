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

송신은 기다리지 않는 로컬 제출이다. `NET_SUBMIT_OK`는 Network가 호출자 저장소와 독립된 크기 제한형 인계 사본를 확보해 복사·접수한 상태다. 실제 CAN 송신·ACK·상대 RX·ECU 실행 DONE은 별도다. 포인터는 호출 동안만 빌려 읽으며 OK 반환 후 호출자가 원본 저장소를 재사용할 수 있다. `NO_CAPACITY`에서 discrete Event/Result를 조용히 버리지 않고 원 identity·Age를 유지한 처리/진단 방침을 적용한다.

| 반환 | 정확한 범위·처리 |
|---|---|
| `NET_SUBMIT_OK` | local 복사·접수. 차량 DONE으로 변환 금지 |
| `NET_SUBMIT_INVALID_ARGUMENT` | NULL/미지원 code/범위·정합 오류; 동일 잘못된 데이터의 무조건 재시도 금지 |
| `NET_SUBMIT_NOT_READY` | owner·Port·Network 미준비; 기능 실행 결과를 합성하지 않음 |
| `NET_SUBMIT_NO_CAPACITY` | bounded 제출 저장소 부족; 원 identity·Age·순서/유효기한을 지킨 처리 필요 |
| RX `void` | 검증된 typed 자료의 local 전달 callback이며 접수 성공·저장성공·물리 동작 결과를 반환하지 않음 |

RX callback은 기존 직렬 Node owner 문맥에서 호출하고 ISR 직접 호출을 하지 않는다. `message/meta`는 NULL 아닌 읽기 전용 borrow이며 Event1/Warning최대4 bundle을 기존 INPUT에 전달한다. STORE가 원 meta·기한·상실 정보를 자기 수명에 복사한 뒤 반환하고 이후 원 포인터를 참조하지 않는다. payload Age는 RX 기준으로 보존하며 INPUT이 RX→소비 경과를 한 번 누적한다. 실제 callback/직렬화·bounded 소비·수용 후 sink는 BINDING_OPEN이다. `void`는 소비/저장/음향 성공을 증명하지 않는다. [입력 정본](10_DATA.md#a1-binding) · [공통 호출·비동기 실패](../../04_COMMON/20_API_USAGE_AND_FAILURE.md).

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

**언제·어떻게 사용:** A1 WARNING60의 세 경고 판단 근거와 후방 활성 meta를 독립 보존하고 거리를 다시 판단하지 않는다. format0은 B50/B52..59=0의 정규 활성 근거 부재이며 미지원 marker와 구별한다. 부재0을 정상 DISABLED/OK로 소비하지 않고 다른 유효 경고는 독립 처리한다. [A1 시간·품질](10_DATA.md#a1-binding)을 따른다.

**반환·실패:** `void`이며 로컬 전달 콜백 호출은 기능적 수용·저장 수용·음향 시작·완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

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

**호출 주체·시점:** VSS Network가 검증한 `VSS_EVENT`를 owner 처리 문맥에서 Application의 기존 INPUT 연결에 전달한다. Application은 원본 DOMAIN 기동·occurrence·AGE와 필요한 값을 반환 전에 복사한다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetVssEvent *` · IN/borrowed; [전체 Data](10_DATA.md#type-netvssevent) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; VSS 기능 입력 연결이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** 수용한 DOMAIN 기동 + occurrence(출처 PRODUCT/TEST 포함)가 key이며 event_kind 변경은 같은 key의 내용 충돌이다. 같은 occurrence를 새 음향으로 재생하지 않는다. 최초 actual에는 원 `L=min(양의 USE_LIMIT,500ms,2000ms)`를 적용하고 L 도달은 만료다. 무발생은 통신 고장이 아니다.

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

**호출 주체·시점:** HEALTH의 완전 CREATED 보고를 FLOW가 기존 INPUT으로 전달하고 INPUT 한 곳에서 보호된 Snapshot을 기존 NetVssStatus 자료형으로 변환해 호출한다. HEALTH→Network 직접 호출이나 새 공개 변환기를 추가하지 않는다. [A2 생산·수용](10_DATA.md#r2-status-proposal)의 선택과 실제 E-S01~05 Binding 대기를 구분한다.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetVssStatus *` · IN/borrowed; [전체 Data](10_DATA.md#type-netvssstatus) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 완전한 current와 원 최근 이력·state/availability/accepting·평가 근거가 함께 있는 보고만 제출한다. `VSS_STATUS_REEVALUATE`로 이전 번호·AGE를 새로 만들지 않는다. INPUT이 한 번 명시 변환하고 Network는 반환 전 11필드·원 key·두 AGE 기준의 전체 사본을 확보한다. 호출자의 Snapshot 포인터를 반환 후에도 보관하지 않는다.

**반환·실패:** 위4개 NetSubmitResult는 local 수용 결과다. 원 세대/평가/발생 identity와 AGE를 보존한다. sink 미준비는 NOT_READY, 전체 미접수·용량 부족은 NO_CAPACITY이며 bounded 재검토만 허용한다. 접수 후 terminal 실패는 기존 M09→INPUT 진단16/loss latch→FLOW 후속으로 RX/새 CREATED와 독립 전달한다. actual sink는 E-S04 미검증이며 local OK를 TX/DOMAIN/음향 성공으로 만들지 않는다. Network는 송신 Boot/TX와 대기 AGE만 보완하고 기능 사실을 재판정하지 않는다.

**경로:** VSS HEALTH→FLOW→기존 INPUT→이 함수→Network Codec→`VSS_STATUS`→DOMAIN `DomainNetPriv_OnVssStatus`→설치 profile별 State/Fault 독립 수용. 같은 원 K의 중복 호출·old K 늦은 완료를 제한하며 실제 C2/Getter association은 E-S05 대기다. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

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

`VssNet_PublishStatus`는 INPUT이 만든 정확한 기존 NetVssStatus 전체 보고의 local handoff다. R4 Snapshot을 직접 인자로 받는 공개 구조를 추가하지 않는다. A2에서 변환 owner를 선택했어도 실제 원 생산·세대·시간·sink·consumer 근거는 E-S01~05 확인 대기다. normal0/READY/Fault없음으로 누락을 보완하지 않는다. `LAST_FAULT_QUALITY`의 이력0/부재3은 current service·전체 Fault 품질이 아니다. [생산·제한 정본](10_DATA.md#r2-status-proposal) · [입력 상실·복구](10_DATA.md#vss-input-recovery). VSS App permission ingress는 필수 API에 추가하지 않고 DOMAIN_PERMISSION의4수신 의무는 private handler로 유지한다. 별도 VSS_FAULT frame·VSS MOBILE Query RPC·재생제어 wrapper를 신설하지 않는다.
