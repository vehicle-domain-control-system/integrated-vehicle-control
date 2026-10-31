# Network 후발 실패 인계와 통신 Health

[개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [수명](NETWORK_SERVICE_LIFETIME.md) · [자료 정본](../data/NETWORK_SERVICE_DATA.md) · [DOMAIN Rx22](DOMAIN_RX_ADAPTER.md)

M03/F09는 실제 성공 접수된 개별 항목의 로컬 실패를 원 Owner에 인계한다. M09/F24·F25는 통신 관측과 조회를 연결한다. 두 책임은 서로를 대신하지 않으며 DOMAIN Manager·CommunicationMonitor·공개 API 본체는 DOMAIN 소유다. 아래 `NetSvc*`는 내부 문서 제안이고 제품 API가 아니다.

<a id="failure-scope"></a>
## 어떤 실패를 어떤 자료로 인계하는가

기존 선언은 다음과 같으며 본체를 재작성하지 않는다.

```c
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);
DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot(
    NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);
```

선언 근거는 [DOMAIN 공개 원형](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxnetworklocalfailure), [Health 원형](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_getnetworkhealthsnapshot)이다. C2 v0.5의 일곱 반환 의미와 실제 함수별 지원·부분 반영 바인딩을 구분한다. 후보 v0.6을 공식 Header나 실제 제품 지원으로 취급하지 않는다.

| 적용 조건 | 생성·대조 근거 | 적용 제외·제한 |
|---|---|---|
| 로컬 성공 접수 | M04의 완전 사본·원자 Queue 게시·원 token/epoch/시각 기록, 실제 OK 반환 | 동기 INVALID_ARGUMENT/NOT_READY/QUEUE_FULL을 후발 실패로 다시 집계하지 않음 |
| 원 개별 identity | COMMAND / REQUEST_RESULT / OCCURRENCE와 원 producer Generation·존재 플래그·원 ID | 일반 주기 상태·Query 자체·malformed·optional Observer 오류 제외 |
| 실제 실패 원인 | 아래 세 reason에 정확히 대응하는 원 사건·송신 전후 관측 | terminal FAILED 하나로 모든 오류를 DRIVER_SUBMISSION_FINAL로 바꾸지 않음 |
| 지역 경과 | 같은 검증된 로컬 clock 수명에서 접수→원 실패 관측 경과 | wire/source AGE·기능 Deadline으로 대체하지 않음 |
| 필수 귀속 | M03→DOMAIN 공개 RxNetworkLocalFailure→원 필수 Manager/Supervision sink | Observer11·운영 counter를 유일 sink로 사용하지 않음 |

| 기존 reason | 허용 근거 | 허용하지 않는 추론 |
|---|---|---|
| `NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL` | 해당 로컬 접수 항목의 **최종 Driver 제출 실패**가 실제 확정 | transient busy·callback 부재·분류되지 않은 in-flight 오류를 동일 reason으로 확대 |
| `NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND` | 실제 송신 시작 전이고 원 사용 기한 위반이 확인 | 미합의 timeout 수치 선택, 이미 송신 중인 항목에 소급 적용 |
| `NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND` | 실제 송신 전 원 항목 문맥의 무효화 근거 | 현재 App Session 변화만으로 다른 원 요청/결과를 무효화·재라벨 |

SOURCE_GENERATION의 0·수명, 명령 DISPATCHED와 Network 단계, 정확 reason→Driver event 대응, 필수 sink 지원·Core 반영 결과는 IF-O12/D-03/D-05/G06·기존 O-C10 등의 HOLD다. 실제 바인딩이 없으면 영향 항목의 성공 접수나 후발 귀속 보증을 하지 않는다. 다른 정상 링크의 전달까지 막지 않는다.

<a id="make-failure"></a>
## 내부 함수 — 원 접수 사건으로부터 failure 구성

**PROPOSED_PRIVATE_FUNCTION.**

```c
NetSvcResult NetSvc_MakeLocalFailure(const NetSvcFailureEvidence *evidence,
    NetworkLocalFailure_t *outFailure);
```

| 요소 | 상세 |
|---|---|
| 호출자·문맥 | M03/F09의 Network Owner. 같은 M04 admission과 M08/수명 사건의 token/epoch 결합을 먼저 확인한 후 호출. ISR 직접 호출 금지 |
| IN | [NetSvcFailureEvidence](../data/NETWORK_SERVICE_DATA.md#type-NetSvcFailureEvidence) 전체; 성공 접수된 원 identity와 실제 원인/관측시각의 불변 사본 |
| OUT | 기존 [NetworkLocalFailure_t](../data/CAN_COMMON_DATA.md#type-NetworkLocalFailure_t)의 caller-owned 저장소. NULL·IN과 중첩 금지. 전 검사 성공 뒤 전체 갱신, 실패 불변 |
| 선행 근거 | 원 kind의 ID/Generation·Request 유효성은 접수 때 검증. 이 함수는 없는 generation·Session을 발급하지 않고 그 검사를 생략하는 공개 진입점이 아님 |
| 반환 | OK는 로컬 DTO 생성만. TIME_HOLD는 clock/범위 불명확, EVIDENCE_HOLD는 미분류 원인·미확인 송신시점·미지원 DOMAIN 링크. 실제 필수 sink 인계와 별개 |

```c
NetSvcResult NetSvc_MakeLocalFailure(const NetSvcFailureEvidence *evidence,
    NetworkLocalFailure_t *outFailure)
{
    if (evidence == NULL || outFailure == NULL) return NET_SVC_INVALID_ARGUMENT;
    const NetSvcCorrelation *id = &evidence->original;
    if (id->link != NETWORK_LINK_CAN && id->link != NETWORK_LINK_UART_M)
        return NET_SVC_EVIDENCE_HOLD; // DOMAIN에 없는 UART-C sink를 복제하지 않는다.
    switch (id->itemKind) {
    case NETWORK_LOCAL_ITEM_COMMAND:
        if (!id->hasCommandId) return NET_SVC_INVALID_ARGUMENT;
        break;
    case NETWORK_LOCAL_ITEM_REQUEST_RESULT:
        if (!id->hasRequestContext) return NET_SVC_INVALID_ARGUMENT;
        break;
    case NETWORK_LOCAL_ITEM_OCCURRENCE:
        if (!id->hasOccurrenceId) return NET_SVC_INVALID_ARGUMENT;
        break;
    default: return NET_SVC_INVALID_ARGUMENT;
    }
    if (evidence->sendPhase < NET_SVC_SEND_NOT_STARTED ||
        evidence->sendPhase > NET_SVC_SEND_START_UNKNOWN)
        return NET_SVC_INVALID_ARGUMENT;
    NetworkLocalFailureReason_t reason;
    switch (evidence->trigger) {
    case NET_SVC_FINAL_SUBMISSION_FAILURE:
        // 이 분류 자체가 실제 최종 제출 실패 근거여야 한다.
        // 이미 송신 시작이 확인됐거나 시작 여부가 모순/불명확하면 합성하지 않는다.
        if (evidence->sendPhase != NET_SVC_SEND_NOT_STARTED)
            return NET_SVC_EVIDENCE_HOLD;
        reason = NETWORK_LOCAL_FAILURE_DRIVER_SUBMISSION_FINAL;
        break;
    case NET_SVC_EXPIRED_BEFORE_SEND:
        if (evidence->sendPhase != NET_SVC_SEND_NOT_STARTED)
            return NET_SVC_EVIDENCE_HOLD;
        reason = NETWORK_LOCAL_FAILURE_EXPIRED_BEFORE_SEND;
        break;
    case NET_SVC_CONTEXT_INVALID_BEFORE_SEND:
        if (evidence->sendPhase != NET_SVC_SEND_NOT_STARTED)
            return NET_SVC_EVIDENCE_HOLD;
        reason = NETWORK_LOCAL_FAILURE_CONTEXT_INVALIDATED_BEFORE_SEND;
        break;
    case NET_SVC_OTHER_OR_UNRESOLVED_ERROR: return NET_SVC_EVIDENCE_HOLD;
    default: return NET_SVC_INVALID_ARGUMENT;
    }
    if (evidence->admittedAt.localClockEpoch != evidence->observedAt.localClockEpoch ||
        evidence->observedAt.monotonicMs < evidence->admittedAt.monotonicMs)
        return NET_SVC_TIME_HOLD;
    uint64_t elapsed = evidence->observedAt.monotonicMs - evidence->admittedAt.monotonicMs;
    if (elapsed > UINT32_MAX) return NET_SVC_TIME_HOLD;

    NetworkLocalFailure_t next = {0};
    next.link = id->link;
    next.itemKind = id->itemKind;
    next.reason = reason;
    next.identityGeneration = id->identityGeneration;
    next.elapsedSinceAdmissionMs = (uint32_t)elapsed;
    next.hasCommandId = id->hasCommandId;
    if (id->hasCommandId) next.commandId = id->commandId;
    next.hasRequestContext = id->hasRequestContext;
    if (id->hasRequestContext) {
        next.requestDeviceContextId = id->requestDeviceContextId;
        next.requestContext = id->requestContext; // 원 Session/Request의 값 전체
    }
    next.hasOccurrenceId = id->hasOccurrenceId;
    if (id->hasOccurrenceId) next.occurrenceId = id->occurrenceId;
    *outFailure = next;
    return NET_SVC_OK;
}
```

0 초기화는 **flag=false인 비활성 저장 필드**를 결정적으로 초기화하려는 것이며, 없는 Generation·Request를 정상 값으로 생성하는 방식이 아니다. 활성 ID/Generation/Request의 유효성·ABI는 접수 경계가 검증한 원 값을 따른다. `elapsed > UINT32_MAX`를 0이나 임의 포화 값으로 의미 변경하지 않고 전달 근거를 HOLD로 보존한다. 현재 clockEpoch나 now로 원 failure 관측시각을 다시 찍지 않는다.

r1 [접수 후 실패의 닫힌 범위](../../interface/04_COMMON/20_API_USAGE_AND_FAILURE.md#r1-local-failure-reasons)를 직접 대조했다. 과거 v2의 포괄적 표현에 대한 문구 정렬 필요는 이전 검수 이력이며, 현재 r1은 아래 기존 세 사건과 일치한다. 알고리즘·공개 reason·OUT 계약은 변경하지 않는다.

위 세 reason은 성공 접수된 원 항목에 대해 합의된 후발 실패 의미를 구분한다. 각 분기는 기존 NOT_STARTED 전제를 유지하며, 포괄적인 “최종 TX 실패”라는 요약만으로 busy·callback 부재·미분류 in-flight 오류를 DRIVER_SUBMISSION_FINAL로 치환하지 않는다. 실제 Driver 사건과 reason의 대응이 확인되지 않으면 그 사건을 EVIDENCE_HOLD로 보존한다. 합의에 포함된 최종 Driver 제출 실패·송신 전 만료·송신 전 문맥 무효화를 제외하거나 새로운 reason을 추가하는 의미가 아니다. `sendPhase=NOT_STARTED`도 Driver 참조 해제를 뜻하지 않는다. DTO 생성 성공 뒤에도 Frame 수명은 [별도 판정](NETWORK_SERVICE_LIFETIME.md#retirement)을 따른다.

<a id="delivery"></a>
## 내부 함수 — 필수 DOMAIN 경계 한 번 호출하고 원 반환 보존

```c
NetSvcResult NetSvc_DeliverLocalFailure(NetSvcFailureDelivery *delivery,
    DomainIf_Status_t *outC2Status);
```

**PROPOSED_PRIVATE_FUNCTION.** M03 단일 Owner가 검증된 delivery를 소유하며 outC2Status는 NULL 불가·비중첩이다. `UNSENT`만 첫 호출 가능하다. 등록/필수 Manager sink/비차단 실행·재진입과 기록 원자성의 실제 바인딩이 전제이며 IsInitialized=true가 이 근거를 대신하지 않는다. 접수 당시 준비했던 sink가 현재 유효하지 않으면 호출 전 해당 경로를 제한한다. 코드 밖 임의 ready bool이 아니라 [등록의 Owner·지원표](DOMAIN_REGISTRATION.md)와 실제 lifecycle을 대조해야 한다.

```c
NetSvcResult NetSvc_DeliverLocalFailure(NetSvcFailureDelivery *delivery,
    DomainIf_Status_t *outC2Status)
{
    if (delivery == NULL || outC2Status == NULL) return NET_SVC_INVALID_ARGUMENT;
    if (delivery->state != NET_SVC_DELIVERY_UNSENT) return NET_SVC_EVIDENCE_HOLD;
    if (!DomainIf_IsInitialized()) return NET_SVC_NOT_READY;
    // 이 기록 공개와 호출은 준비된 동일 Owner 처리 문맥에서 직렬화한다.
    delivery->state = NET_SVC_DELIVERY_CALL_IN_PROGRESS;
    DomainIf_Status_t status = DomainIf_RxNetworkLocalFailure(&delivery->message);
    delivery->lastC2Return = status;
    delivery->hasC2Return = true;
    switch (status) {
    case DOMAIN_IF_OK:
        delivery->state = NET_SVC_DELIVERY_BOUNDARY_OK;
        break;
    case DOMAIN_IF_INVALID_ARGUMENT:
    case DOMAIN_IF_NOT_INITIALIZED:
    case DOMAIN_IF_NOT_AVAILABLE:
    case DOMAIN_IF_QUEUE_FULL:
    case DOMAIN_IF_REJECTED:
    case DOMAIN_IF_UNSUPPORTED:
    default:
        delivery->state = NET_SVC_DELIVERY_UNRESOLVED;
        break;
    }
    *outC2Status = status;
    return NET_SVC_OK; // 호출과 원 반환 기록 완료. 필수 Core 반영 완료는 별도다.
}
```

이 함수의 OK는 C2 호출이 반환됐다는 뜻이다. `outC2Status`가 C2 오류일 수 있으며 원 값을 그대로 기록한다. C2 반환 실패 시 그 API의 부분 반영을 추정하지 않는다. BOUNDARY_OK도 원 identity·Core/필수 Owner 수용의 연결 증거 없이 `consumer=CONFIRMED`로 올리지 않는다. 실제 Owner 계약으로 core가 반영됐고 optional Observer만 실패했다면 CORE_CONFIRMED로 기록할 수 있지만 이미 반영된 Core를 재호출하지 않는다.

C2가 호출 중 재진입하면 CALL_IN_PROGRESS가 추가 동일 호출을 막는다. 반환하지 못했거나 lifecycle이 바뀌었다면 해당 기록을 불명확으로 보존한다. 다시 UNSENT로 바꿔 자동 retry하거나 새 Queue를 만들어 무한 재통지하지 않는다. 실제로 Core 미반영이 확인된 뒤 동일 입력 재인계를 허용하는 절차는 C2/Owner 바인딩과 정책이 필요하다. Network가 `Vehicle FAILED`, 새 RequestResult 또는 `AbortBeforeDispatch`를 호출해 보상하지 않는다.

<a id="counters"></a>
## 내부 함수 — 이미 식별한 단일 관측의 포화 집계

```c
NetSvcResult NetSvc_CountObservedFault(NetworkLinkId_t link,
    NetSvcCounterEvent event, uint32_t increment,
    bool *eventAlreadyCounted, NetworkOperationalCounters_t *counters);
```

**PROPOSED_PRIVATE_FUNCTION.** M09가 기존 원 사건 journal과 counters를 같은 직렬 처리에서 소유한다. eventAlreadyCounted는 callback 호출자가 임의로 새로 만든 bool이 아니라 **동일 원 사건의 보관된 집계 여부 필드**다. 그 키는 실제 parser 사건/Driver token·epoch/bus-off 관측에서 연결한다. 같은 사건을 새 record로 재생성해 반복 집계하지 않는다. increment는 실제 합의된 관측 단위의 양수다. 잃은 byte 수와 overflow interrupt 횟수를 임의로 같은 counter 단위로 더하지 않는다.

두 IN/OUT 포인터는 NULL 불가이며 저장소가 서로 겹치지 않아야 한다. 비OK 반환 시 두 대상은 모두 불변이다. 성공 시 counter와 사건 원장을 같은 Owner의 원자 처리에서 함께 공개한다.

```c
NetSvcResult NetSvc_CountObservedFault(NetworkLinkId_t link,
    NetSvcCounterEvent event, uint32_t increment,
    bool *eventAlreadyCounted, NetworkOperationalCounters_t *counters)
{
    if (eventAlreadyCounted == NULL || counters == NULL || increment == 0)
        return NET_SVC_INVALID_ARGUMENT;
    if (link != NETWORK_LINK_CAN && link != NETWORK_LINK_UART_M)
        return NET_SVC_EVIDENCE_HOLD;
    if (event < NET_SVC_COUNT_CRC_REJECT || event > NET_SVC_COUNT_CAN_BUS_OFF)
        return NET_SVC_INVALID_ARGUMENT;
    if (event == NET_SVC_COUNT_CAN_BUS_OFF && link != NETWORK_LINK_CAN)
        return NET_SVC_EVIDENCE_HOLD;
    if (*eventAlreadyCounted) return NET_SVC_DUPLICATE;
    NetworkOperationalCounters_t next = *counters;
    uint32_t *value;
    switch (event) {
    case NET_SVC_COUNT_CRC_REJECT: value = &next.rxCrcRejectCount; break;
    case NET_SVC_COUNT_FORMAT_REJECT: value = &next.rxFormatRejectCount; break;
    case NET_SVC_COUNT_RX_OVERFLOW: value = &next.rxOverflowCount; break;
    case NET_SVC_COUNT_TX_FINAL_FAILURE: value = &next.txFinalFailureCount; break;
    case NET_SVC_COUNT_CAN_BUS_OFF: value = &next.busOffCount; break;
    default: return NET_SVC_INVALID_ARGUMENT;
    }
    *value = increment >= UINT32_MAX - *value ? UINT32_MAX : *value + increment;
    *counters = next;
    *eventAlreadyCounted = true; // counter와 사건 원장을 같은 Owner 처리에서 함께 공개한다.
    return NET_SVC_OK;
}
```

위 두 OUT 기록은 같은 원자 처리로 공개해야 한다. 실제 동기화 계약이 없으면 연결하지 않는다. 별도 함수 포인터·신규 전역 counter store를 만들지 않는다. CAN CRC에 대해 소프트웨어가 수신한 field가 없는데 UART의 CRC 검사처럼 집계하지 않으며 실제 Driver의 오류→counter mapping이 필요하다. 형식상 유효한 INVALID/NO_DATA 보고는 format reject가 아니다. 동기 접수 거부·terminal UNKNOWN을 txFinalFailureCount로 세지 않는다.

**명명된 설계 호출자:** [COMMON_FAILURE_RECOVERY / SC18 M09 F24](../flows/e2e/COMMON_FAILURE_RECOVERY.md#observed-health-callsite)가 실제 parser/Driver 원 관측을 이 함수에 연결하고, M09 원장이 두 IN/OUT 결과를 소비한다. 런타임 호출 함수·동기화 바인딩은 OPEN이며 집계와 필수 failure 인계는 독립이다.

Counter 증가와 failure sink 전달은 별도다. counter 포화는 정확한 누적값 상실의 하한 기록이며 wrap하지 않는다. 실제 local Network init/reset 외에 peer reboot·정상 수신·Query/링크 복구로 카운터를 초기화하지 않는다.

<a id="health"></a>
## 내부 함수 — 일관된 M09 관측을 기존 Snapshot 형식으로 복사

```c
NetSvcResult NetSvc_BuildHealthView(const NetSvcHealthView *view,
    const NetSvcClockStamp *now, NetworkHealthSnapshot_t *outSnapshot);
```

**PROPOSED_PRIVATE_FUNCTION.** M09 Owner가 생성한 원자 view, 같은 로컬 시간 바인딩의 now, caller-owned OUT을 사용한다. IN/OUT 중첩·NULL 금지, 실패 OUT 불변이다. 이 함수는 C2 Getter 본체가 아니며, M09 raw→DOMAIN CommunicationMonitor의 실제 공급 함수/스레드/원자성은 BINDING_OPEN이다. 공개 setter를 발명하지 않는다. DOMAIN 소유 Getter가 실패하면 OUT은 원 계약상 undefined이므로 이 내부 함수의 실패 불변 보증을 공개 Getter에 확대하지 않는다.

```c
NetSvcResult NetSvc_BuildHealthView(const NetSvcHealthView *view,
    const NetSvcClockStamp *now, NetworkHealthSnapshot_t *outSnapshot)
{
    if (view == NULL || now == NULL || outSnapshot == NULL)
        return NET_SVC_INVALID_ARGUMENT;
    if (view->link != NETWORK_LINK_CAN && view->link != NETWORK_LINK_UART_M)
        return NET_SVC_EVIDENCE_HOLD;
    if (view->transportState < NETWORK_TRANSPORT_UNKNOWN ||
        view->transportState > NETWORK_TRANSPORT_RECOVERING)
        return NET_SVC_INVALID_ARGUMENT;
    uint32_t age = 0;
    if (view->hasValidRx) {
        if (now->localClockEpoch != view->lastValidRx.localClockEpoch ||
            now->monotonicMs < view->lastValidRx.monotonicMs)
            return NET_SVC_TIME_HOLD;
        uint64_t elapsed = now->monotonicMs - view->lastValidRx.monotonicMs;
        if (elapsed > UINT32_MAX) return NET_SVC_TIME_HOLD;
        age = (uint32_t)elapsed;
    }
    NetworkHealthSnapshot_t next;
    next.link = view->link;
    next.transportState = view->transportState;
    next.hasValidRx = view->hasValidRx;
    next.lastValidRxAgeMs = age; // hasValidRx=false일 때 소비하지 않는 비활성 필드
    next.counters = view->counters;
    *outSnapshot = next;
    return NET_SVC_OK;
}
```

**명명된 설계 호출자:** [COMMON_FAILURE_RECOVERY / M09 F25 Health 공급](../flows/e2e/COMMON_FAILURE_RECOVERY.md#observed-health-callsite)이 원자 view/now를 제공하고 성공 OUT만 기존 DOMAIN CommunicationMonitor 공급 경계에 복사한다. 실제 공급 함수·Task·소비자 바인딩은 OPEN이다.

UNKNOWN/UNAVAILABLE/RECOVERING도 알려진 통신 관측이므로 Snapshot 복사 자체는 성공할 수 있다. hasValidRx=false의 age0을 fresh로 읽지 않는다. 단순 bus-wide 정상 CAN Rx로 특정 BCM/CIS/WINDOW/VSS liveness를 만들지 않는다. Health는 링크 로컬 사실이고 차량 가용성·명령 완료·Permission 복구는 각 기능 Owner가 판정한다. DOMAIN에 없는 UART-C 조회는 기존 C2 Owner의 UNSUPPORTED 경계이며 이 제안은 Pi/CIS 감독 C2를 복제하지 않는다.

## 문서 반례

| ID | 사건 | 기대·제한 |
|---|---|---|
| SVC-F01 | TxPort OK 후 확정 Driver 제출 실패 | 원 generation/ID와 접수→실패 경과로 failure 생성, 과거 OK·차량 FAILED를 변경하지 않음 |
| SVC-F02 | transient busy 또는 원인 미분류 terminal FAILED | 정확 기존 reason 근거 전 EVIDENCE_HOLD, 임의 final failure 귀속 없음 |
| SVC-F03 | 송신 시작 뒤 만료 | BEFORE_SEND 생성 거부. Driver reference는 별도 유지 |
| SVC-F04 | command hasCommandId=false | INVALID_ARGUMENT, OUT 불변 |
| SVC-F05 | 과거 Request Session7, 현재 recipient Session9 | 원 Request7·identityGeneration 그대로. 현재 수신 대상은 별도 transport 문맥 |
| SVC-F06 | 접수clockEpoch1·실패epoch2 또는 역행 | TIME_HOLD. elapsed0·원 AGE 대용 금지 |
| SVC-F07 | 경과 UINT32_MAX+1 | 표현 근거 HOLD. wrap/0/임의 포화 보고 없음 |
| SVC-F08 | C2 반환 OK, 같은 Core 반영 미확인 | BOUNDARY_OK 보존, consumer/identity 정리 자동 승격 없음 |
| SVC-F09 | Core commit 확인, optional Observer만 실패 | Core 재호출 금지. Owner 증거로만 완료 기록 |
| SVC-F10 | 실패 sink 반환이 QUEUE_FULL/REJECTED | 원 C2 값과 identity 보존, 부분 반영 불명확 시 재호출 HOLD |
| SVC-F11 | Query 응답 실패 또는 일반 periodic 손실 | Command/RequestResult kind로 위조하지 않음. M02/기존 transport 진단 사용 |
| SVC-F12 | counter=UINT32_MAX, 새 사건1 | UINT32_MAX 유지, 사건집계flag만 함께확정. wrap 없음 |
| SVC-F13 | 동일 Driver failure callback 두 번 | 동일journal 두번째 DUPLICATE, counter와 실패 통지의 중복 방지 별도 |
| SVC-F14 | UART에서 bus-off event | EVIDENCE_HOLD, counter 불변 |
| SVC-F15 | INVALID 품질을 정상 schema로 수신 | raw validity와 기능 품질 분리, format reject 증가 없음 |
| SVC-F16 | Health link UNAVAILABLE, 유효 원시 관측 있음 | Snapshot 복사는 OK 가능. 차량 기능 정상 주장 없음 |
| SVC-F17 | hasValidRx=false, lastValidRx 슬롯 미초기화 | 그 시각 읽지 않음. age0 비활성, fresh 주장 없음 |
| SVC-F18 | 같은clock 관측 now100/last80 | age20. 다른epoch이면 TIME_HOLD·OUT 불변 |
| SVC-F19 | UART-C를 DOMAIN Health로 요청 | 기존 Owner UNSUPPORTED/해당바인딩부재. 다른노드 C2복제없음 |
| SVC-F20 | peer reboot/Query 완료 후 counter reset 요구 | local Network reset 근거가 아니므로 보존 |

근거: S4A api/common·domain_management·domain_rx, 기존 CAN_TRANSFER_CONTROL의 failure/recovery-health, S4C RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 기존 Owner35·G4 조건·G5-0 게시 HOLD·C2 후보·S6 이후 통합 방침을 유지한다. 실제 Driver/제품/Host/실보드 검증 증거는 별도로 필요하다.
