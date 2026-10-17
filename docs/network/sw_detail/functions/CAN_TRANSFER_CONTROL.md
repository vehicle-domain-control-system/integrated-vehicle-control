# CAN 전송 제어 — 접수·시간·순서·Driver 관측

M01/M03/M04/M07/M08/M09는 메시지별 Codec 바깥에서 원 identity와 저장소 수명을 보존한다. 이 문서는 CAN 공통 제어의 정본이며 API별 의미 변환은 메시지 함수 문서가 맡는다. DOMAIN 공개 API는 ISR 밖의 단일 담당 실행 문맥에서 호출한다. ISR은 실제 frame/장치 시각/오류와 짧은 인계만 담당하고 parsing·C2 호출·대기·동적 정책 판정을 하지 않는다. 실제 Worker/Superloop·동기화 primitive·Queue 수/깊이는 확인되지 않았다.

[CAN 흐름](../flows/CAN_NETWORK_PATHS.md) · [공통 Data](../data/CAN_COMMON_DATA.md) · [M04](../modules/M04_MESSAGE_SERVICE.md) · [M07](../modules/M07_BUFFER_OUTCOME.md) · [M08](../modules/M08_DEVICE_PORT.md) · [M09](../modules/M09_OBSERVATION.md)

<a id="registration"></a>
## F06 — DOMAIN 포트와 필수 consumer 준비

기존 선언: `DomainIf_Status_t DomainIf_SetTxPort(const DomainIf_TxPort_t *port);`. 공개 본체/보관 정책은 DOMAIN, 콜백 본체와 등록 요청은 Network 소유다. 기존 TxPort26 중 CAN9 입력(BCM 명령3, Permission 입력3, WINDOW 명령1, VSS 사건/경고2)을 이 묶음에서 연결한다. UART17을 NULL 또는 임의 함수로 대체하는 새 테이블을 만들지 않는다. 등록 자료 전체 선언과 공개 API는 기존 [DOMAIN Data](../../interface/02_NODES/DOMAIN/10_DATA.md)·[API](../../interface/02_NODES/DOMAIN/20_API.md), S4A `T13_Domain_Interface_v05_C2_target.md`를 따른다. 실제 지원표/등록 수명·반환 대응은 양단 확인 대상이다.

M03은 팀 기준선의 콜백 테이블이 수명 동안 유효하고, 각 채택된 입력의 안정 복사와 필수 후발 실패 consumer가 준비되었는지 확인한다. 그 후에만 DOMAIN 소유 등록 API를 호출한다. 등록 OK를 곧바로 링크 준비/Driver TX/차량 기능 준비로 해석하지 않는다. observer11은 선택적 관측 경계이며 Manager 필수 sink를 대신하지 않는다. 등록 실패·필수 sink 부재이면 해당 의미 입력에 성공을 반환하지 않는다. Generation0/BOOT 수명/새 후보 타입을 채택한 것처럼 포트를 연결하지 않는다.

테이블 전체 `port=NULL`의 등록해제 의미는 미정이므로 HOLD다. 테이블 안 개별 callback=NULL과 구별하며 없는 callback 포인터를 호출하지 않는다. 개별 callback 지원표를 전체 등록 성공으로 추정하지 않는다.

<a id="admission"></a>
## F07/F10/F11 — typed 입력의 안정 접수

기존 `NetSubmitResult`와 `DomainIf_Status_t`를 메시지별 원형 그대로 반환한다. 새 private `NetCanResult`의 [반환 의미](../data/CAN_COMMON_DATA.md#type-NetCanResult)는 내부 결과이며 외부 enum으로 숫자 캐스팅하지 않는다. C2 반환값으로 명시 대응할 근거가 없는 경우 바인딩 HOLD다.

1. NULL·값/품질·원 identity·표현 가능성, 링크/profile/대상 기동, 필요한 Owner 정책·실패 sink 준비를 확인한다.
2. 호출자의 전체 DTO와 원 출처·원 시각·귀속 식별을 Network 소유 저장소에 완전히 복사한다. 포인터만 저장하거나 borrowed ByteView를 나중에 읽지 않는다.
3. 현재상태는 동일 메시지/원 source key의 아직 Driver가 참조하지 않는 pending 자료만 교체할 수 있다. 새 자료의 품질·AGE가 더 나쁘거나 서로 비교할 수 없으면 무조건 latest로 덮지 않는다. 원 관측 관계를 확인한다.
4. 명령·결과·사건은 독립 identity별 보관이다. 기존 항목을 덮거나 capacity를 가정하지 않는다. 필요한 저장소가 없으면 접수 실패이고 이전 항목은 불변이다.
5. 복사와 접수 원장 공개를 원자적으로 완료한 뒤 로컬 OK를 반환한다. 그 전에 생산자는 기존 메모리를 유지하고, 반환 이후에는 수정해도 Network 사본에 영향이 없어야 한다.

한국어 절차를 포함한 기존 원형별 C 의사코드는 각 [22종 함수](../04_CAN_FD_OVERVIEW.md#messages)에 있다. 위 공통 절차를 호출하는 실재하지 않는 `GenericSubmit(void*)` 같은 공개 API를 만들지 않는다.

<a id="age"></a>
## F12 — AGE를 한 번씩 누적

`PROPOSED_PRIVATE_FUNCTION`:

```c
NetCanResult NetCan_AccumulateAge(uint16_t sourceAgeMs,
    uint32_t elapsedMs, bool elapsedKnown,
    uint16_t *outAgeMs, bool *saturated);
```

이 함수는 이미 검증한 로컬 elapsed를 u16 Wire AGE에 포화 합산할 뿐, clock wrap/시계 동기/센서 TTL을 판정하지 않는다. `elapsedKnown`은 동일 로컬 단조 시간원, 불연속·wrap 처리, 포함/미포함 구간이 확인되었다는 뜻이다. 미측정 Driver 대기를 0으로 넘기지 않는다. 성공 시 out/saturated를 함께 쓰고 실패 시 둘 다 불변이다. saturated=true는 하한이며 fresh/기한 이내를 증명하지 않는다.

```c
NetCanResult NetCan_AccumulateAge(uint16_t sourceAgeMs,
    uint32_t elapsedMs, bool elapsedKnown,
    uint16_t *outAgeMs, bool *saturated)
{
    if (outAgeMs == NULL || saturated == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (!elapsedKnown) return NET_CAN_TIME_HOLD;
    // 더하기 전에 남은 범위를 검사하므로 u32 덧셈도 넘치지 않는다.
    bool sat = sourceAgeMs == 65535 || elapsedMs >= (uint32_t)(65535 - sourceAgeMs);
    uint16_t next = sat ? 65535 : (uint16_t)(sourceAgeMs + elapsedMs);
    *outAgeMs = next;
    *saturated = sat;
    return NET_CAN_OK;
}
```

원 생산 AGE + 실제로 아직 누적하지 않은 local pending/queue/hop 지연만 더한다. C2 u32 AGE로 옮길 때도 먼저 UINT32_MAX까지 남은 범위를 검사한다. u16로 내려갈 때 포화한다. Codec는 clock을 읽지 않고 dispatch가 확정한 값을 사용한다. Driver 참조 이후 frame의 AGE는 수정하지 않는다. 미측정 driver delay와 CAN 전달여유5ms 후보를 검증된 상한으로 대체하지 않는다.

같은 관측의 보수적 AGE는 **같은 소비 시각**에 환산한다. A=20@100, B=15@130, now=150이면 A70/B35다. B의 raw15가 작다는 이유로 A를 젊게 만들지 않는다. 보수적 max를 보관해도 서로 모순된 계보/시각 근거를 합법화하지 않는다. raw frame 비교용 AGE와 현재 소비 시각의 보수적 AGE는 별도 보관한다.

COMMAND AGE는 명령 birth 기준이며 센서 AGE와 다르다. 200ms와 구1s ACCEPT_LIMIT 충돌, `>`/`>=` 경계 미합의는 Owner 판단 전 HOLD다. report 주기·Rx 중단 시간·source TTL은 다른 수치다. CIS Environment 온도·습도 원 사용기한은 각각2500ms(도달 시 비사용)로 합의되었다. 조도1000/로컬5000과 report200/Rx600 후보는 별개다. Event/Result/Command가 발생하지 않았다는 사실로 주기 timeout을 만들지 않는다.

<a id="dispatch"></a>
## F12/F19/F22 — 전송 선택과 Driver 인계

M04는 아직 대기 중인 항목의 원 생성 identity, 현재 대상 BOOT/Session 등 필요한 문맥, AGE/수용기한, 계약/HOLD를 다시 확인한다. 실패한 항목을 새 Generation·현재 Session·새 commandId로 바꾸지 않는다. 인계 전 폐기가 후발 실패 통지 대상이면 F09에 원 접수 identity를 남긴다.

M07은 안정된 의미 사본과 별도로 encoded frame 저장소를 확보한다. 실제 송신 선택 직전 메시지별 TX_SEQUENCE를 소비하는 것은 내부 제안이며 gap/wrap/기동 초기 정책은 미확정이다. 아직 관측 번호가 같아도 전송 번호만 바뀔 수 있다. Codec에 원 BOOT/TX와 확정 AGE를 넘긴 뒤 검증된 불변 frame을 Driver에 인계한다.

Driver accepted, 최종 TX, 참조 released는 서로 독립 관측이다. 인계 여부가 불명확하면 같은 항목을 pending으로 남기고 추가 buffer가 있더라도 중복 전송 시도하지 않는다. transient busy를 terminal failure로 바꾸지 않는다. 실제 retry 수·간격·취소 방법은 확인 전 채택하지 않는다. DOMAIN `DISPATCHED`와 Driver 실제 TX 시작도 다르므로 DOMAIN이 이미 DISPATCHED라면 Driver 미송신만으로 AbortBeforeDispatch를 수행하지 않는다.

Driver 호출 **전** M07은 재사용되지 않는 token/epoch와 원 identity 저장소를 확보하고 callback이 조회할 record를 먼저 공개한다. 초기값은 `acceptedKnown=false`, `accepted=false`(미관측이므로 의미 없음), `outcome=UNKNOWN`, `driverReferenceOutstanding=true`(참조 가능성이 아직 해소되지 않음), `bufferReturned=false`, `identityRetained=true`다. Driver 호출 중 즉시 callback이 오거나 반환과 callback 순서가 뒤바뀌어도 같은 record에 직렬화한다. 이 동기화·callback 수명 바인딩이 확인되지 않으면 인계를 시작하지 않는다. 호출 전 실패는 아직 외부 참조가 전혀 없음을 입증한 경우에만 로컬 할당을 회수한다. 호출 후에는 accepted=false만으로 reference를 해제하지 않으며, 실제 참조 없음/release의 Driver 계약 근거를 기다린다.

<a id="rx-order"></a>
## F13/F20 — 수신 순서 분류와 원자적 인계

`PROPOSED_PRIVATE_FUNCTION`:

```c
NetCanResult NetCan_ClassifyRx(const NetCanRxComparison *c,
    NetCanRxAction *action);
```

[비교 입력과 결과](../data/CAN_COMMON_DATA.md#type-NetCanRxComparison)는 raw/의미 임시값을 이미 확보한 M04가 만든다. 함수는 원장을 변경하지 않는 분류기다. OK는 전달 가능성 분류만 끝났다는 뜻이며 실제 copy/consumer 성공은 별도다. DUPLICATE는 action에 EXACT_DUPLICATE를 쓰고 다른 거부는 action 불변이다. 동일 BOOT/TX이나 raw가 다르면 충돌이다. BOOT 변경은 unsigned 크기 비교로 최신 여부를 결정하지 않는다.

비교 입력의 `hasImmutableInputIdentity/sameImmutableInputIdentity`는 단순 commandId 일치가 아니다. 관측은 같은 UPDATE의 불변 의미, 명령은 같은 commandId와 불변 명령 본문, 사건은 같은 occurrence와 불변 내용이다. 결과는 같은 commandId 안에서도 **결과 단계 전이**를 별도 비교한다. 승인된 ACCEPTED→DONE 전이라면 새 입력으로 분류하고 `functionalIdentityOrderKnown=true`에 전이 근거를 연결한다. 동일 결과 단계/내용의 반복은 SAME_INPUT, terminal 뒤 ACCEPTED/IN_PROGRESS 퇴행이나 동일 단계의 모순은 HOLD다. 결과 전이 정책을 Network가 창작하지 않으며 근거가 없으면 functionalIdentityOrderKnown은 false다. 최초/새 generation의 transitionAuthorized도 동일 의미 검증을 포함한다.

```c
NetCanResult NetCan_ClassifyRx(const NetCanRxComparison *c, NetCanRxAction *action)
{
    if (c == NULL || action == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (!c->generationBindingKnown) return NET_CAN_ORDER_HOLD;
    if (!c->hasAcceptedFrame || !c->sameGeneration) {
        // 첫 수신도 현재 source context가 수용 가능한지 확인되어야 한다.
        if (!c->transitionAuthorized) return NET_CAN_ORDER_HOLD;
        *action = NET_CAN_RX_NEW_INPUT; return NET_CAN_OK;
    }
    if (c->incomingTx == c->previousTx) {
        if (!c->rawExactlyEqual) return NET_CAN_ORDER_HOLD;
        *action = NET_CAN_RX_EXACT_DUPLICATE; return NET_CAN_DUPLICATE;
    }
    if (!c->nonWrappingOrderKnown || c->incomingTx < c->previousTx)
        return NET_CAN_ORDER_HOLD;
    if (c->hasImmutableInputIdentity && c->sameImmutableInputIdentity) {
        if (!c->semanticExactlyEqual) {
            if (c->messageKind != NET_CAN_CIS_OCCUPANT || c->occupantRestriction == NULL)
                return NET_CAN_ORDER_HOLD;
            if (!c->ageRelationAccepted) return NET_CAN_TIME_HOLD;
            NetCanResult restricted = NetCan_CheckOccupantRestriction(c->occupantRestriction);
            if (restricted != NET_CAN_OK) return restricted;
            // 증거의 실제 old/new TX가 이번 비교와 일치해야 한다.
            if (c->occupantRestriction->previousMeta.source.tx_sequence != c->previousTx ||
                c->occupantRestriction->incomingMeta.source.tx_sequence != c->incomingTx)
                return NET_CAN_ORDER_HOLD;
            *action = NET_CAN_RX_CURRENT_RESTRICTION; return NET_CAN_OK;
        }
        if (!c->ageRelationAccepted) return NET_CAN_TIME_HOLD;
        *action = NET_CAN_RX_SAME_INPUT; return NET_CAN_OK;
    }
    // 다른 UPDATE/command/event identity도 Owner 순서 규칙이 확인되어야 한다.
    if (!c->functionalIdentityOrderKnown) return NET_CAN_ORDER_HOLD;
    // 단순히 높은 TX만으로 새 기능 identity를 수용하지 않는다.
    *action = NET_CAN_RX_NEW_INPUT; return NET_CAN_OK;
}
```

NEW_INPUT 및 CIS_OCCUPANT의 CURRENT_RESTRICTION 인계는 typed 자료/meta/보수적 AGE와 실제 제한 사건의 Network 안정 사본, M09 결과 공간 및 consumer 준비를 먼저 확보한다. 실패하면 이전 frame순서·기능관측 상태는 유지하여 같은 프레임 재시도를 막지 않는다. Manager 소유 상태를 Network가 먼저 수정하지 않는다. C2 거부 이후의 복구는 원 반환을 보존하고 consumer의 부분 반영 여부가 확인되지 않으면 재호출 HOLD다. VSS_STATUS는 [A2 per-K 독립 수용](can/VSS_STATUS.md#a2-domain-contract)을 적용한다. 준비된 State를 Fault 미등록으로 전역 차단하지 않고, 수용한 State는 Fault 실패로 재실행하지 않는다. 실제 profile·같은 입력·부분 결과·old K 후속 쓰기 차단이 미입증인 해당 경계만 HOLD한다.

SAME_INPUT은 높은 TX의 raw frame 기준선과 같은 소비시각의 보수적 AGE를 함께 원자적으로 갱신하며 C2 관측을 재발행하지 않는다. CURRENT_RESTRICTION은 별도이며 [CIS_OCCUPANT 검사·호출](can/CIS_OCCUPANT.md#restriction-check)을 거쳐 같은 원 관측의 제한을 기존 C2로 전달한다. allocation/AGE검사 실패면 둘 다 이전 값이다. 수신 TX10→20(동일UPDATE) 뒤15는 adopt된 nonwrap 조건에서 ORDER_HOLD다. TX20을 원장에 반영하지 않고 AGE만 바꾸는 부분 갱신은 금지다. exact duplicate·malformed는 정상수신 시각/heartbeat·의미 freshness를 갱신하지 않는다.

실제 통신 건강의 유효 frame 관측과 새 semantic 관측은 구분한다. 동일 UPDATE의 더 높은 유효 TX라도 값의 freshness를 새로 만들지 않는다. 상태값·품질·순서 ledger를 쪼개서 독자가 서로 다른 관측을 읽게 하지 않는다. 동기화 구현은 실제 runtime 확정 후 선택한다.

<a id="port"></a>
## F21/F22 — Driver 관측과 참조 해제

`PROPOSED_PRIVATE_FUNCTION`:

```c
NetCanResult NetCan_ObservePort(NetCanTxRecord *r,
    const NetCanPortEvent *e);
```

M08이 관측한 [event](../data/CAN_COMMON_DATA.md#type-NetCanPortEvent)를 M07의 [record](../data/CAN_COMMON_DATA.md#type-NetCanTxRecord)에 반영한다. 호출 중 event는 불변이고 record는 같은 제출의 단일 작성 경계에서 갱신한다. callback 식별/참조 해제 의미를 SDK에서 확인하기 전 이 제안은 연결하지 않는다. accepted=false는 제출 최종 거절이 확인된 이벤트만 표시하며 temporary busy와 구분한다.

```c
NetCanResult NetCan_ObservePort(NetCanTxRecord *r, const NetCanPortEvent *e)
{
    if (r == NULL || e == NULL) return NET_CAN_INVALID_ARGUMENT;
    if (r->localToken != e->localToken || r->localEpoch != e->localEpoch)
        return NET_CAN_ORDER_HOLD; // 오래된 callback을 새 슬롯에 적용하지 않는다.
    if (e->outcome < NET_CAN_PORT_OUTCOME_UNKNOWN || e->outcome > NET_CAN_PORT_TX_FAILED)
        return NET_CAN_INVALID_ARGUMENT;
    if (e->admissionObserved && r->acceptedKnown && e->accepted != r->accepted)
        return NET_CAN_ORDER_HOLD;
    if (e->outcome != NET_CAN_PORT_OUTCOME_UNKNOWN &&
        r->outcome != NET_CAN_PORT_OUTCOME_UNKNOWN && e->outcome != r->outcome)
        return NET_CAN_ORDER_HOLD; // 두 충돌 관측은 별도 진단에 보존한다.
    if (((e->admissionObserved && !e->accepted) || (r->acceptedKnown && !r->accepted)) &&
        (e->outcome == NET_CAN_PORT_TX_SUCCEEDED || r->outcome == NET_CAN_PORT_TX_SUCCEEDED))
        return NET_CAN_ORDER_HOLD;
    bool admissionNew = e->admissionObserved && !r->acceptedKnown;
    bool outcomeNew = e->outcome != NET_CAN_PORT_OUTCOME_UNKNOWN &&
                      r->outcome == NET_CAN_PORT_OUTCOME_UNKNOWN;
    bool releaseNew = e->releaseObserved && r->driverReferenceOutstanding;
    if (!admissionNew && !outcomeNew && !releaseNew) return NET_CAN_DUPLICATE;
    NetCanTxRecord next = *r;
    if (admissionNew) { next.acceptedKnown = true; next.accepted = e->accepted; }
    if (outcomeNew) next.outcome = e->outcome;
    if (releaseNew) {
        if (!next.identityRetained) return NET_CAN_ORDER_HOLD;
        next.driverReferenceOutstanding = false;
        // bufferReturned가 false인 경우만 물리 회수를 원자 처리에 한 번 연결한다.
        // 실제 회수를 완료하기 전에는 bufferReturned를 true로 기록하지 않는다.
        if (!next.bufferReturned) {
            // 이 원자 처리 안에서 저장소 회수를 완료할 수 없으면 CONTRACT_HOLD로 종료한다.
            // 그러면 r은 이전 상태이고, 실제 release 증거는 별도 보존해 재처리할 수 있다.
            // 회수를 완료한 경로만 다음 대입을 수행한다. 외부 allocator 이름은 미정이다.
            next.bufferReturned = true;
        }
    }
    // 결과 UNKNOWN이어도 실제 release가 증명되면 물리 저장소는 회수할 수 있다.
    // 단, 원 identity/결과 pending은 별도 공간에 남아 있어야 한다.
    // source identity의 후속 통지/보관까지 완료 전 identityRetained는 유지한다.
    *r = next;
    return NET_CAN_OK; // 관측 반영만 완료; 원격 기능 완료가 아니다.
}
```

위 의사코드의 저장소 회수와 record 공개는 일관된 단일 처리 경계에서 수행해야 한다. 처리 도중 실패할 수 있는 실제 allocator/Driver callback에 맞춘 원자성은 아직 확인되지 않았다. 이를 확인하지 못하면 회수를 보류하며 이중 해제·reuse를 하지 않는다. 실제 참조 release 뒤 identity를 별도 보관하는 B11 규칙을 유지한다. 보수적으로 결과와 release 둘 다 기다릴 수도 있으나 release만으로 원 identity를 잊거나 결과만으로 참조를 해제하면 안 된다.

<a id="failure"></a>
## F09 — 접수 후 로컬 실패 통지

기존 선언: `DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);`. [완전 자료 선언](../data/CAN_COMMON_DATA.md#type-NetworkLocalFailure_t)과 `NetworkLocalItemKind_t/NetworkLocalFailureReason_t`를 따른다. 본체는 DOMAIN, 호출·안정 사본·재통지 관측은 Network M03 책임이다. 선택 observer나 단순 health counter로 대체하지 않는다.

이 경계는 이미 로컬 접수한 COMMAND/REQUEST_RESULT/OCCURRENCE 가운데 결과 귀속이 필요한 항목에 사용한다. CAN 정기 상태, Query, malformed/CRC, observer오류를 이 계약으로 끌어오지 않는다. F09에 필요한 sink가 준비되지 않으면 의존 항목을 처음부터 성공 접수하지 않는다.

다음은 S4A `api/common.md#c2-return`의 공통 최소 의미이며 모든 함수가 일곱 상태를 모두 지원한다는 표가 아니다. F09·등록·Rx별 실제 반환/부분 반영 지원표는 Owner 확인 전 HOLD다.

| 원 C2 반환 | 호출 후 처리 제한 |
|---|---|
| DOMAIN_IF_OK | 로컬 경계 성공만 기록한다. 실제 Core 반영과 동일 입력의 연결 증거는 별도로 보존한다. |
| DOMAIN_IF_INVALID_ARGUMENT | 같은 잘못된 입력을 반복하지 않는다. 원 identity와 실패 원인을 보존한다. |
| DOMAIN_IF_NOT_INITIALIZED | C2 초기화 준비를 기다리며 해당 인계를 성공 처리하지 않는다. |
| DOMAIN_IF_NOT_AVAILABLE | Port·문맥·필수 sink 등 준비 부족을 보존한다. 무한 retry를 만들지 않는다. |
| DOMAIN_IF_QUEUE_FULL | 다른 개별 결과/사건을 덮지 않고 현재 미전달 identity를 유지한다. |
| DOMAIN_IF_REJECTED | DOMAIN 의미 거부를 Network 인자 오류나 차량 FAILED로 바꾸지 않는다. |
| DOMAIN_IF_UNSUPPORTED | 선언의 존재로 지원을 추정하지 않는다. 해당 경로와 WINDOW 보류를 유지한다. |

Core commit이 완료된 뒤 **선택 observer만 실패한 경우 Core를 다시 적용하지 않는다**. Core 반영 여부/observer 단계가 구분되지 않는 실패는 성공도 무조건 재통지도 아니며 해당 identity의 추가 호출을 HOLD한다. 이는 필수 Manager sink 미전달을 선택 observer 실패로 바꾸는 규칙이 아니다.

| reason | 허용 조건 |
|---|---|
| DRIVER_SUBMISSION_FINAL | Driver 제출이 최종 실패한 사실. transient busy 또는 임의의 인계 후 오류 전체를 뜻하지 않음 |
| EXPIRED_BEFORE_SEND | 실제 송신 전 만료. 이미 in-flight인 항목을 소급해 before-send로 분류하지 않음 |
| CONTEXT_INVALIDATED_BEFORE_SEND | 실제 송신 전 접수 당시 문맥 무효화. 단순 현재 Session 교체로 옛 항목을 새 문맥으로 재발급하지 않음 |

```c
/* 기존 DOMAIN 소유 공개 경계의 호출 절차. Network가 본체를 구현하지 않는다. */
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);
// Network Task:
// 1. 접수 원장으로부터 failure를 안정 사본으로 만든다.
// 2. link/itemKind/reason/원 identityGeneration/존재 플래그를 검증한다.
// 3. commandId 또는 원 requestDeviceContextId+RequestContext 또는 occurrenceId를 유지한다.
// 4. elapsedSinceAdmissionMs는 검증한 로컬 경과이며 Wire AGE로 덮지 않는다.
// 5. 필수 Manager sink에 호출하고 원 DomainIf_Status_t를 기록한다.
// 6. 미수용/반영 불명확이면 identity를 유지한다. 무제한 재시도 Queue를 만들지 않는다.
// 7. 수용 증거 전에는 전달 완료로 세지 않으며 차량 FAILED를 합성하지 않는다.
```

다른 ECU에 DOMAIN의 동일 C2 failure API를 복제하지 않는다. 실제 non-DOMAIN supervision 경계가 미정이면 로컬 관측과 Owner HOLD를 남긴다.

<a id="recovery-health"></a>
## F23/F24/F25 — 기동·Bus-off·통신 건강

Boot 변경/Bus-off는 현재 Driver reference를 자동 해제하거나 원 관측을 새 것으로 만드는 사건이 아니다. 새 제출을 멈추고 in-flight·불명확·released-but-pending 항목을 각각 보존한다. 실제 abort/release 증거와 clock/profile/현재 peer BOOT/필수 consumer 준비를 다시 확인한 뒤 해당 경로만 재개한다. 한 ECU 복구로 다른 ECU의 생존 시각·원장을 초기화하지 않는다. WINDOW 비연결은 필수 통신 고장을 만들지 않는다.

Health raw 관측은 M09가 보유하고 기능 상태판정은 DOMAIN/각 노드 Owner에게 남긴다. 기존 `DomainIf_Status_t DomainIf_GetNetworkHealthSnapshot(NetworkLinkId_t link, uint32_t nowMs, NetworkHealthSnapshot_t *outSnapshot);`의 공개 본체는 DOMAIN이다. [Snapshot/counters](../data/CAN_COMMON_DATA.md#type-NetworkHealthSnapshot_t)는 실제 선언을 따른다. Network raw 입력을 연결하는 실제 API·원자성·지원 범위는 미결이므로 새 공개 setter를 만들지 않는다. hasValidRx=false이면 lastValidRxAgeMs를 0ms fresh로 사용하지 않는다.

각 u32 counter는 UINT32_MAX에서 포화하고 실제 로컬 초기화/승인된 reset에서만 초기화한다. 상대 reboot·정상 frame 한 건·bus recovery로 손실·후발 실패 기록을 지우지 않는다. Health read는 동일 snapshot을 복사하며 읽기 자체가 count를 초기화하지 않는다. counter 증가가 Manager 실패 sink 전달 성공을 증명하지 않는다.

Health Getter의 `link`는 DOMAIN이 실제 보유한 CAN/UART-M 지원 범위를 확인하고 UART-C 지원을 추정하지 않는다. `nowMs`는 로컬 단조 시각이며 원격 노드 시계가 아니다. `outSnapshot`은 NULL 불가 OUT 저장소이고 `DOMAIN_IF_OK`일 때만 읽는다. 실패 후 OUT 내용은 미정이다. 링크 transportState가 UNAVAILABLE이어도 조회 자체는 OK일 수 있으므로 둘을 같은 상태로 합치지 않는다.

| 정상·경계·반례 | 정적 기대 |
|---|---|
| AGE10+20, elapsedKnown=true | OK, AGE30, saturated=false |
| AGE65534+1 또는 AGE1+UINT32_MAX | 포화65535; 정확 fresh 주장 없음 |
| clock 불연속/Driver 대기 불명 | TIME_HOLD, 출력 불변 |
| 로컬 접수 뒤 caller DTO 변경 | Network 사본은 불변 |
| 제출 결과 불명인데 여분 buffer 존재 | 동일 항목 추가 시도 없음 |
| released=true, outcome unknown | 참조 증거에 따라 물리 회수 가능; identity pending 유지 |
| late callback의 token/epoch 불일치 | 현 record/새 buffer 불변 |
| 동일 terminal 두 번/반대 terminal | 전자는 idempotent, 후자는 충돌 보존·HOLD |
| 높은 TX, 같은 UPDATE/의미, AGE 실패 | TX·AGE 원장 모두 이전 값 |
| 최종 결과 뒤 늦은 ACCEPTED/IN_PROGRESS | 기능 결과를 뒤로 돌리지 않음; 상태 Owner 규칙 미정 시 HOLD |

근거: S4C `TIME_IDENTITY_CONTRACT`, `FRESHNESS_AGE_TIMEOUT`, `RESOURCE_QUEUE_DRIVER_FLOW`, `FAILURE_RECOVERY_CONTRACT`; S4A API/Data; 기존 B11 후방 원자성·Driver 수명 정본. 이는 정적 명세이며 실제 SDK/제품 실행 시험 결과가 아니다.

### PG6-SW01 caller 초기화와 재진입

모든 기존 M04 caller는 `NetCanRxComparison` 전체를 초기화하고 실제 messageKind와 `occupantRestriction=NULL`을 명시한다. BCM/명령/결과/VSS/WINDOW에는 새 예외를 전달하지 않는다. CIS_OCCUPANT M04만 동일 처리 구간의 old/new 원자료로 제한 증거를 구성한다. 직접 생성·검증·인계 순서는 [CIS_OCCUPANT](can/CIS_OCCUPANT.md#restriction-check)와 [수직 호출부](../flows/NETWORK_TO_DOMAIN.md#cis-current-quality)에 있다.

제한 증거를 관측했으나 M09/인계 공간이 부족한 경우 원 관측과 제한 사건을 보존하고 해당 원 관측의 정상 소비를 HOLD한다. C2 호출 전 안정 보관이 불가하면 호출하지 않는다. 호출 이후 raw 제한·원 rc·Core 단계는 보존하며 성공 여부 불명확을 이전 VALID로 rollback하지 않는다. 동일 원 관측의 낮은 TX는 공통 ORDER_HOLD이고, 더 높은 TX라도 INVALID→VALID는 제한 예외가 아니므로 HOLD다. 새 실제 관측의 회복은 원 순서와 기능 회복 근거를 함께 검증하는 기존 NEW_INPUT 경로에서만 가능하다.

<a id="vss-warning-admission"></a>
## VSS A1의 Event·Warning 전용 admission

아래 분기는 VSS Event1건/Warning최대4 lane의 기존 직렬 owner 전달을 명시하며 공통 `NetCan_ClassifyRx` 코드·다른 메시지의 immutable-input·CIS_CURRENT_RESTRICTION 예외를 바꾸지 않는다. WARNING60 bundle은 세 위험과 독립 rearActivation을 각 원 source/sequence/quality/reason/availability/AGE로 판정한다. 한 lane의 제한을 다른 유효 lane의 손실이나 정상 CLEAR로 바꾸지 않는다. 같은 source key라도 승인된 제한 품질 변화는 전용 lane에서 전달하고, 같은 원 근거의 정상 판단 변경·승격은 E01 DOMAIN 실제 평가 증거 전 제한한다. exact duplicate는 재소비/정상 liveness 갱신 없음, 적법한 높은 TX의 동일 의미 반복은 전달 건강만 관측하며 원 freshness를 늘리지 않는다.

Decode DTO의 AGE 기산점은 실제 `received_at_ms`다. callback 전 현재 AGE는 admission용 임시값으로만 계산하고 DTO를 현재 AGE로 덮은 뒤 RX시각과 함께 넘기지 않는다. INPUT이 RX→자기 owner 경과를 한 번 더하는 계약과 겹치지 않아야 한다. Event 원 Key는 수용된 DOMAIN Boot+occurrence(생산 문맥 포함), event_kind는 내용 충돌이며 새 재생 key가 아니다. 최초 actual의 원 L과 Warning 각 lane TTL/Hold는 [VSS Data](../../interface/02_NODES/VSS/10_DATA.md)에서 그대로 추적한다. 실제 clock·owner bounded bundle·void callback 복사/누락 관측은 E01~05 Binding 대기다.
