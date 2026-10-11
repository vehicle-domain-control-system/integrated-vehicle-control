# WINDOW API — 송신·수신 호출 계약

**[구현 보류 — 설계 유지]**. 아래 원형은 DEFERRED / PROPOSED_DESIGN이며 현재 WINDOW firmware 구현 요구 또는 통합 동작 승인이 아니다.

[노드 개요](00_OVERVIEW.md) · [Data](10_DATA.md) · [API](20_API.md) · [전체 시작](../../00_START_HERE.md) · [공통 의미·수명](../../04_COMMON/10_DATA_AND_ID_RULES.md) · [CAN FD](../../03_COMMUNICATION/10_CAN_FD.md)

## 반환형·전체 원형 먼저 보기

```c
void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
void WindowApp_OnDomainCommand(const NetWindowCommand *message, const NetCanRxMeta *meta);
NetSubmitResult WindowNet_SendEvent(const NetWindowEvent *message);
NetSubmitResult WindowNet_SendResult(const NetWindowResult *message);
NetSubmitResult WindowNet_PublishState(const NetWindowState *message);
NetSubmitResult WindowNet_PublishFault(const NetWindowFault *message);
```

| 함수 | 방향·구현 owner | 메시지 |
|---|---|---|
| [WindowApp_OnDomainPermission](#api-windowapp_ondomainpermission) | Network→Application; Application 입력 연결 구현 | [DOMAIN_PERMISSION](../../03_COMMUNICATION/10_CAN_FD.md#message-domain-permission) |
| [WindowApp_OnDomainCommand](#api-windowapp_ondomaincommand) | Network→Application; Application 입력 연결 구현 | [WINDOW_COMMAND](../../03_COMMUNICATION/10_CAN_FD.md#message-window-command) |
| [WindowNet_SendEvent](#api-windownet_sendevent) | Application→Network; Network 구현 | [WINDOW_EVENT](../../03_COMMUNICATION/10_CAN_FD.md#message-window-event) |
| [WindowNet_SendResult](#api-windownet_sendresult) | Application→Network; Network 구현 | [WINDOW_RESULT](../../03_COMMUNICATION/10_CAN_FD.md#message-window-result) |
| [WindowNet_PublishState](#api-windownet_publishstate) | Application→Network; Network 구현 | [WINDOW_STATE](../../03_COMMUNICATION/10_CAN_FD.md#message-window-state) |
| [WindowNet_PublishFault](#api-windownet_publishfault) | Application→Network; Network 구현 | [WINDOW_FAULT](../../03_COMMUNICATION/10_CAN_FD.md#message-window-fault) |

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

<a id="api-windowapp_ondomainpermission"></a>
## `WindowApp_OnDomainPermission()`

```c
void WindowApp_OnDomainPermission(const NetDomainPermission *message, const NetCanRxMeta *meta);
```

**WINDOW 상위 동작허용과 원 품질·수명을 기존 동작 담당자에 전달한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetDomainPermission *` · IN/borrowed; [전체 Data](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netdomainpermission) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; WINDOW 기능 입력 연결이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** 원 허용과 수신 생존을 분리한다. 최초 NO_DATA/INVALID/STALE을 새 이동허용으로 만들지 않으며 permission 복구로 과거 이동 재개 금지.

**반환·실패:** `void`이며 local 전달 callback 호출은 기능적 수용·실제 이동·정지·보호 완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

**경로:** DOMAIN→공유 CAN `DOMAIN_PERMISSION`→WINDOW Network→이 함수. [DOMAIN API](../DOMAIN/20_API.md)와 [메시지 색인](../../05_MESSAGE_INDEX.md).

```c
/* decoded는 검증된 NetDomainPermission, rx_meta는 Network 수신 문맥이다. */
WindowApp_OnDomainPermission(&decoded, &rx_meta);
/* consumer가 반환 이후 필요 값을 owner 저장소로 복사한다. */
```

<a id="api-windowapp_ondomaincommand"></a>
## `WindowApp_OnDomainCommand()`

```c
void WindowApp_OnDomainCommand(const NetWindowCommand *message, const NetCanRxMeta *meta);
```

**수용 조건을 확인한 DOMAIN 이동·STOP 명령을 WINDOW 동작 담당자에 전달한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetWindowCommand *` · IN/borrowed; [전체 Data](10_DATA.md#type-netwindowcommand) | 수신 Network가 길이/enum/기동/순서 검증 후 제공; WINDOW 기능 입력 연결이 소비; NULL 금지, 호출 반환까지 유효 |
| `meta` | `const NetCanRxMeta *` · IN/borrowed; [전체 메타](../../04_COMMON/10_DATA_AND_ID_RULES.md#type-netcanrxmeta) | Network 기동/프레임 전송순번·로컬 RX 시간. 원 관측 source sequence/Age와 별개; NULL 금지, 호출 동안 읽기 전용 |

**언제·어떻게 사용:** 새 이동은 유효한 permission이 필요하다. 정당한 DOMAIN STOP은 이동허용 상실만으로 전달 거부하지 않는다. 출처·대상기동·ID·Age·채널·형식 검증은 유지한다. 로컬 STOP·끼임 보호는 이 호출·DOMAIN 응답과 독립.

**반환·실패:** `void`이며 local 전달 callback 호출은 기능적 수용·실제 이동·정지·보호 완료를 증명하지 않는다. 의미상 무효/만료 근거를 typed 상태로 전달할 수 있지만 CRC·length·unsupported code 등 구조 오류는 Network가 구분한다. 실패를 CLEAR/DONE으로 합성하지 않는다.

**경로:** DOMAIN→공유 CAN `WINDOW_COMMAND`→WINDOW Network→이 함수. [DOMAIN API](../DOMAIN/20_API.md)와 [메시지 색인](../../05_MESSAGE_INDEX.md).

```c
/* decoded는 검증된 NetWindowCommand, rx_meta는 Network 수신 문맥이다. */
WindowApp_OnDomainCommand(&decoded, &rx_meta);
/* consumer가 반환 이후 필요 값을 owner 저장소로 복사한다. */
```

<a id="api-windownet_sendevent"></a>
## `WindowNet_SendEvent()`

```c
NetSubmitResult WindowNet_SendEvent(const NetWindowEvent *message);
```

**새 끝단 전이 또는 끼임 발생 근거를 DOMAIN에 제출한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetWindowEvent *` · IN/borrowed; [전체 Data](10_DATA.md#type-netwindowevent) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 끝단·끼임 kind, WINDOW 원 occurrence, 선택적 DOMAIN 연결, origin, Age/quality를 보존한다. AntiPinchReport 하나로 끝단까지 대체하지 않는다. Network 실패가 로컬 보호를 지연시키지 않는다.

**반환·실패:** 위4개 NetSubmitResult만 local 수용 상태로 사용한다. 실패 뒤 재시도해도 원 상태평가/발생/명령 식별을 바꾸거나 원 Age를 초기화하지 않는다. State/Fault와 discrete Event/Result의 서로 다른 교체·수명 규칙은 Data를 따른다. Network는 송신시 Boot/TX_SEQUENCE/실제 대기 Age를 보완하고 기능 상태·고장을 재판정하지 않는다.

**경로:** WINDOW App→이 함수→Network Codec→`WINDOW_EVENT`→DOMAIN Network 내부 `DomainNetPriv_OnWindowEvent`→기존 C2 또는 미정 의미 owner. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

```c
/* observed_message는 실제 기능 owner가 만든 NetWindowEvent 객체다. */
NetSubmitResult local_result = WindowNet_SendEvent(&observed_message);
/* local_result == NET_SUBMIT_OK: 로컬 인계만 완료. */
```

<a id="api-windownet_sendresult"></a>
## `WindowNet_SendResult()`

```c
NetSubmitResult WindowNet_SendResult(const NetWindowResult *message);
```

**특정 원 DOMAIN 명령의 실제 처리 Result·Reason을 제출한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetWindowResult *` · IN/borrowed; [전체 Data](10_DATA.md#type-netwindowresult) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 원 DOMAIN 기동/식별/채널 echo이며 commandId 대응 OPEN. 실제 동작 담당자만 결과를 생성한다. DONE은 목표 확인 근거, local OK는 결과 아님. terminal 뒤 이전 단계 회신으로 후퇴 금지.

**반환·실패:** 위4개 NetSubmitResult만 local 수용 상태로 사용한다. 실패 뒤 재시도해도 원 상태평가/발생/명령 식별을 바꾸거나 원 Age를 초기화하지 않는다. State/Fault와 discrete Event/Result의 서로 다른 교체·수명 규칙은 Data를 따른다. Network는 송신시 Boot/TX_SEQUENCE/실제 대기 Age를 보완하고 기능 상태·고장을 재판정하지 않는다.

**경로:** WINDOW App→이 함수→Network Codec→`WINDOW_RESULT`→DOMAIN Network의 typed 변환→기존 C2 `DomainIf_RxWindowCommandResult`. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

```c
/* observed_message는 실제 기능 owner가 만든 NetWindowResult 객체다. */
NetSubmitResult local_result = WindowNet_SendResult(&observed_message);
/* local_result == NET_SUBMIT_OK: 로컬 인계만 완료. */
```

<a id="api-windownet_publishstate"></a>
## `WindowNet_PublishState()`

```c
NetSubmitResult WindowNet_PublishState(const NetWindowState *message);
```

**현재 위치·움직임·끝단·보호·역전·품질 평가를 제출한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetWindowState *` · IN/borrowed; [전체 Data](10_DATA.md#type-netwindowstate) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 실제 새 평가에서 update_sequence 갱신. 현재 fully_open_state=true 반복은 새 FULLY_OPENED 발생 아님. UNKNOWN 위치·끝단을0/false로 만들지 않는다.

**반환·실패:** 위4개 NetSubmitResult만 local 수용 상태로 사용한다. 실패 뒤 재시도해도 원 상태평가/발생/명령 식별을 바꾸거나 원 Age를 초기화하지 않는다. State/Fault와 discrete Event/Result의 서로 다른 교체·수명 규칙은 Data를 따른다. Network는 송신시 Boot/TX_SEQUENCE/실제 대기 Age를 보완하고 기능 상태·고장을 재판정하지 않는다.

**경로:** WINDOW App→이 함수→Network Codec→`WINDOW_STATE`→DOMAIN Network의 typed 변환→기존 C2 `DomainIf_RxWindowState`. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

```c
/* observed_message는 실제 기능 owner가 만든 NetWindowState 객체다. */
NetSubmitResult local_result = WindowNet_PublishState(&observed_message);
/* local_result == NET_SUBMIT_OK: 로컬 인계만 완료. */
```

<a id="api-windownet_publishfault"></a>
## `WindowNet_PublishFault()`

```c
NetSubmitResult WindowNet_PublishFault(const NetWindowFault *message);
```

**현재·복구중 고장과 원 발생·평가 근거를 제출한다.** DEFERRED · PROPOSED_DESIGN / BINDING_OPEN.

| 인자 | 정확한 타입·방향 | 생성·소비·수명 |
|---|---|---|
| `message` | `const NetWindowFault *` · IN/borrowed; [전체 Data](10_DATA.md#type-netwindowfault) | 실제 기능 owner가 생성한 보고; Network가 접수 전 검사·복사; NULL 금지, 호출 반환까지 유효 |

**언제·어떻게 사용:** 현재 고장 상태와 occurrence 기록을 분리한다. COMM 근거는 WINDOW Network가 관측해 기능 진단 담당자가 반영하며 fault 원 근거 생성자로 위장하지 않는다. FaultInfo_t ingress의 추가 provenance owner OPEN.

**반환·실패:** 위4개 NetSubmitResult만 local 수용 상태로 사용한다. 실패 뒤 재시도해도 원 상태평가/발생/명령 식별을 바꾸거나 원 Age를 초기화하지 않는다. State/Fault와 discrete Event/Result의 서로 다른 교체·수명 규칙은 Data를 따른다. Network는 송신시 Boot/TX_SEQUENCE/실제 대기 Age를 보완하고 기능 상태·고장을 재판정하지 않는다.

**경로:** WINDOW App→이 함수→Network Codec→`WINDOW_FAULT`→DOMAIN Network 내부 `DomainNetPriv_OnWindowFault`→기존 C2 또는 미정 의미 owner. [DOMAIN API](../DOMAIN/20_API.md) · [DOMAIN 내부 Routing](../DOMAIN/25_PRIVATE_ROUTING.md).

```c
/* observed_message는 실제 기능 owner가 만든 NetWindowFault 객체다. */
NetSubmitResult local_result = WindowNet_PublishFault(&observed_message);
/* local_result == NET_SUBMIT_OK: 로컬 인계만 완료. */
```

## 구현 재개 조건

여섯 경계는 현재 설계 검토용이다. commandId 연계·끝단 ingress·실제 보호 확인·Fault provenance/quality·C2 enum mapping을 해결한 뒤 실제 WINDOW 바인딩을 검토한다. 기존 WINDOW local Port가 같은 책임을 충족하면 우선 재사용하고 wrapper를 동시에 설치하지 않는다. 로컬 STOP·AntiPinch 우선 경계에 Network 왕복을 추가하지 않는다. SDK/Firmware/Codec/Board 시험은 **NOT_EXECUTED**다.
