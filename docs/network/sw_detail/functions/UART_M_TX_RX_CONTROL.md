# UART-M 접수·전송·수신·복구 제어

M04/F11~F13, M07/F19~F21, M08/F22~F23, M09/F24~F25의 UART-M 적용이다. L5/M9/F25/ST12 기준선 안에서 설명하며 새 전역 Manager·차량 cache·RTOS·Queue 크기·공개 API를 만들지 않는다. 함수에 표시한 NetUartM private 선언은 문서 제안이며 실제 Driver API가 아니다. 현재 Driver/시간원/직렬화/양단 profile·Session·세대·필수 sink 바인딩은 외부 HOLD다.

[개요](../05_UART_M_OVERVIEW.md) · [Data](../data/UART_FRAME_DATA.md) · [Frame/Stream](UART_FRAME_STREAM.md) · [Router](UART_M_ROUTER.md) · [Query](UART_M_QUERY_CONTROL.md) · [B12 전송 불변식](CAN_TRANSFER_CONTROL.md) · [B12 Port 수명](CAN_PORT_RECOVERY.md)

<a id="returns"></a>
## 접수 반환과 실제 보증

| 기존 값 | 접수 경계의 의미 | C2/Gateway 연결 후보와 한계 |
|---|---|---|
| NET_SUBMIT_OK0 | caller pointer 없이 원 식별·값·품질·AGE 기준·필요 순서를 포함한 완전 사본 소유 | DOMAIN_IF_OK0. 송신·상대수신·기능 완료 아님 |
| NET_SUBMIT_INVALID_ARGUMENT1 | NULL/enum/필드 조합/표현이 잘못됨, 새 입력 미접수 | DOMAIN_IF_INVALID_ARGUMENT1 |
| NET_SUBMIT_NOT_READY2 | 해당 profile/문맥/필수 sink/Owner 바인딩 미준비 | DOMAIN_IF_NOT_AVAILABLE3 |
| NET_SUBMIT_NO_CAPACITY3 | 완전 사본 공간 부족, 기존 discrete를 덮지 않음 | DOMAIN_IF_QUEUE_FULL4 |
| C2 NOT_INITIALIZED2/REJECTED5/UNSUPPORTED6 | 각각 C2 초기화/정책거부/지원 여부 | 위 Net 네 값에 숫자 cast 또는 임의 동일화하지 않음 |
| Gateway OK/INVALID_ARGUMENT/NOT_READY/UNSUPPORTED/INTERNAL_ERROR | [5개 값](UART_M_ROUTER.md#gateway-api)과 실제 해당 경계 | capacity→NOT_READY는 O14 후보. 차량 FAILED를 합성하지 않음 |

현재 증거로 미결인 계약은 NOT_READY/NOT_AVAILABLE 의미로 **영향 경로만 제한하는 제안**이며 실제 제품 반환을 확인했다는 뜻이 아니다. 안정 접수 후 caller에게 반환한 OK를 소급 변경하지 않는다. DomainIf_Tx* wrapper·Network callback·private submit는 호출 주체가 다르다. DOMAIN의 MarkDispatched와 Driver accepted도 다른 단계다. 이미 DISPATCHED인 명령을 송신 전 오류라고 AbortBeforeDispatch로 되돌리지 않는다.

<a id="admission"></a>
## 한 항목의 접수와 송신 준비

모든 TX 인자는 호출 동안 빌린 const IN이다. nested Gateway ByteView는 view 구조체와 실제 ID/payload bytes를 한 번에 확보해야 한다. callback table의 주소 복사는 user_context 수명을 연장하지 않는다. IRQ는 raw 수신/장치 오류 인계만 수행하며 C2/Parser/차량 의미 처리·blocking을 하지 않는다. Task/Superloop 선택·우선순위·lock은 실제 Owner 계약을 기다린다.

| 단계 | 정확한 수행·소유 | 실패와 보존 |
|---|---|---|
| 형식·계약 선행검사 | 해당23 typed DTO, source 의미·원본 연결, 수신대상·profile·기능 HOLD, 준비·필수 sink | INVALID_ARGUMENT 또는 NOT_READY, 부분 사본 공개 없음 |
| 안정 저장소 확보 | 원 Device/Session/Request/Query/Boot/발생·품질·AGE 기준을 포함해 전체 사본 인수 | 공간 부족이면 새 입력 전체 거부. 기존 discrete 사본 불변 |
| pending 의미 저장 | current state는 합의한 의미 key의 미송신 pending만 교체 가능 | key/동기화 미확인은 HOLD. Query 고정 사본·discrete 이력 silent overwrite 없음 |
| 송신 직전 | 동일 의미·원 식별을 유지하고 아직 미반영 대기 AGE 1회 누적, 기한/대상/순서 재검사 | 원 시간 미입증은 TIME_HOLD. before-send 만료/문맥 무효는 적용 가능한 F09로 귀속 |
| Codec | exact fields→고정 offset→선택된 type/P/version/현재 전송 LINK_SEQUENCE→CRC | 함수 실패면 OUT·기존 pending 불변. 의미를 정상값으로 채워 송신하지 않음 |
| Driver 호출 직전 | immutable frame 저장소와 localToken/localEpoch 원 항목 기록을 먼저 공개 | 즉시 callback이 와도 기록을 찾을 수 있어야 함. 실제 직렬화/등록 증거 없으면 제출하지 않음 |
| 호출 후 | Driver admission·최종 TX 결과·참조 해제를 각각 관측 | 반환 오류만으로 미접수·참조 해제·최종 실패를 추정하지 않음 |

LINK_SEQUENCE는 방향별 u16 송신 순번이다. 정확한 할당 시점·초기값·재제출·gap 허용은 실제 계약 확인 대상이다. 문서 Codec는 caller가 근거를 확인한 값을 받아 직렬화할 뿐 새 순번을 발급하지 않는다. Driver가 참조하기 시작한 뒤에는 AGE·LINK_SEQUENCE·CRC 어느 바이트도 수정하지 않는다. 그 뒤 지연을 알고 싶다는 이유로 같은 배열을 재encode하지 않는다.

<a id="age"></a>
## AGE 산술과 비교

각 메시지의 여러 AGE는 서로 다른 원 평가/결과/사건을 가리킨다. M_CONTEXT의 공동 등록·연결, App, 근접과 CIS Environment의 센서별 AGE는 독립이다. CAN→UART projection도 이미 계산된 앞 홉을 다시 더하지 않는다. 아래 함수는 확인된 미반영 구간만 caller가 제공하는 순수 산술이며 clock을 읽지 않는다.

```c
NetUartMResult NetUartM_AccumulateAge16(uint16_t originalAgeMs,
    uint32_t unaccountedLocalMs, uint32_t unaccountedHopMs,
    bool timeBasisVerified, uint16_t *outAgeMs)
{
    if (outAgeMs == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (!timeBasisVerified) return NET_UART_M_TIME_HOLD;
    uint64_t sum = (uint64_t)originalAgeMs + unaccountedLocalMs + unaccountedHopMs;
    *outAgeMs = sum > 65535u ? 65535u : (uint16_t)sum;
    return NET_UART_M_OK;
}
```

timeBasisVerified는 원 AGE 기준점·이미 누적된 구간·같은 로컬 시간원/연속성·wrap 해석을 입증한 내부 결과다. 숫자 true가 증거가 아니다. candidate UART hop50ms는 측정 상한이 아니며 Driver 미관측 지연을0으로 만드는 근거도 아니다. UINT32 경과 두 개를 더해도 uint64 중간값으로 overflow를 방지한다. 포화65535는 원 TTL 이내 보증이 아니고 미확인 AGE를65535로 채우라는 뜻도 아니다. 호출 실패는 OUT 불변이다.

동일 원 관측 A(AGE20@RX100), B(AGE15@RX130)를 로컬150에서 비교하면 각각70/35ms다. B를 새 LINK_SEQUENCE로 받았어도 같은 관측을35ms로 젊게 만들지 않는다. 어느 구간의 기준이 맞는지 확인하거나 보수적 동일 소비시각 AGE 근거를 유지한다. 다른 관측은 수용한 순서·세대 규칙으로 비교한다. u32 4294967290→5만으로 경과11을 확정할 수 없고 같은 runtime·한 wrap·간격 근거가 필요하다.

M_REQUEST24B에는 원 MOBILE SENT AGE가 없다. 로컬 접수시각+3s 검사로 과거 원 SENT3s 정책 준수를 주장하지 않는다. 해당 실행만 SAFE_RESTRICTED로 남기며 Query 전체를 차단하지 않는다. 조립50ms·전송 여유50ms·M_CONTEXT100/300ms·source TTL·Session TTL·Query deadline은 서로 다른 계약이다. 원문에 없는 timeout 수치·>/≥ 경계를 새로 정하지 않는다.

<a id="rx-order"></a>
## 수신 순서·중복의 보수적 분류

다음 함수는 검증된 same-context 관측 비교의 **분류만** 하고 상태를 쓰지 않는다. 이전/입력 Frame byte 비교, 원 기능 identity/단계, 두 AGE의 같은 소비시각 대응과 실제 기동/Session 수용 근거는 M04·원 Owner가 만든 입력이다. bool을 임의 true로 채우는 구현은 승인하지 않는다. hasAcceptedFrame=false 최초 수신도 실제 문맥 수용 근거가 필요하다.

```c
NetUartMResult NetUartM_ClassifyRx(const NetUartMRxComparison *c)
{
    if (c == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (!c->contextBound) return NET_UART_M_CONTEXT_HOLD;
    if (!c->sameContext || !c->hasAcceptedFrame) {
        if (!c->transitionAuthorized) return NET_UART_M_CONTEXT_HOLD;
        if (!c->ageRelationAccepted) return NET_UART_M_TIME_HOLD;
        return NET_UART_M_OK;
    }
    if (c->incomingLinkSequence == c->previousLinkSequence) {
        if (!c->rawExactlyEqual) return NET_UART_M_ORDER_HOLD;
        if (!c->ageRelationAccepted) return NET_UART_M_TIME_HOLD;
        return NET_UART_M_DUPLICATE;
    }
    if (!c->nonWrappingOrderKnown || c->incomingLinkSequence < c->previousLinkSequence)
        return NET_UART_M_ORDER_HOLD;
    if (!c->ageRelationAccepted) return NET_UART_M_TIME_HOLD;
    if (c->sameImmutableInput) {
        if (!c->semanticExactlyEqual) return NET_UART_M_ORDER_HOLD;
        return NET_UART_M_DUPLICATE;
    }
    if (!c->functionalTransitionKnown) return NET_UART_M_ORDER_HOLD;
    return NET_UART_M_OK;
}
```

숫자 증가 자체는 nonWrappingOrderKnown의 근거가 아니다. 65535→0은 표현상 modulo여도 이 문서는 임의 half-range 허용창을 정하지 않아 ORDER_HOLD다. 이후 합의한 wrap 수용이 생기면 별도 근거와 검수를 통해 통합한다. DUPLICATE는 원 기능 입력을 재실행·Query 응답수를 증가시키지 않는다는 뜻이다. 같은 원 관측의 새 Frame에 관한 transport 순서/유효 수신 관측을 따로 보존할 수 있으나 원 AGE·생성시각·source TTL을 갱신하지 않는다. terminal 결과 뒤 늦은 ACCEPTED는 같은 commandId여도 다른 결과 단계이며 Owner terminal 전이 규칙으로 차단한다.

ClassifyRx가 OK여도 실제 consumer는 실패할 수 있다. M04는 consumer 접수/commit·optional Observer 결과를 따로 기록하고 같은 입력을 중복 적용하지 않는다. 실패한 입력의 순서를 언제 소모할지는 실행 계약 미결이며 수신 성공 장부로 숨기지 않는다. 정상 품질저하 보고의 통신 유효성과 차량 기능 가용성을 분리한다.

<a id="port-observe"></a>
## Driver admission·최종 결과·해제 관측

기존 B12의 **참조 해제와 최종 결과는 독립**이라는 조건을 유지한다. record는 Driver를 호출하기 전에 원 항목에 바인딩한다. 초기 acceptedKnown=false, outcome=UNKNOWN, driverReferenceOutstanding=true(실제 소유권을 아직 몰라 보수적 보존), bufferReturned=false, identityRetained=true, queryTerminalAccounted=false다. localToken/epoch의 유일성·reuse·callback 경합 바인딩이 없으면 제출 HOLD다. 이는 공개 TxTicket/원 Wire 필드가 아니다.

```c
NetUartMResult NetUartM_ObservePort(NetUartMTxRecord *record,
    const NetUartMPortEvent *event)
{
    if (record == NULL || event == NULL) return NET_UART_M_INVALID_ARGUMENT;
    if (!record->identityRetained || record->localToken != event->localToken ||
        record->localEpoch != event->localEpoch) return NET_UART_M_CONTEXT_HOLD;
    if (event->outcome != NET_UART_M_TX_UNKNOWN && event->outcome != NET_UART_M_TX_SUCCEEDED &&
        event->outcome != NET_UART_M_TX_FAILED) return NET_UART_M_INVALID_ARGUMENT;
    NetUartMTxRecord tmp = *record;
    if (event->admissionObserved) {
        if (tmp.acceptedKnown && tmp.accepted != event->accepted) return NET_UART_M_CONTRACT_HOLD;
        tmp.acceptedKnown = true; tmp.accepted = event->accepted;
    }
    if (event->outcome != NET_UART_M_TX_UNKNOWN) {
        if (tmp.outcome != NET_UART_M_TX_UNKNOWN && tmp.outcome != event->outcome)
            return NET_UART_M_CONTRACT_HOLD;
        tmp.outcome = event->outcome;
    }
    if (tmp.acceptedKnown && !tmp.accepted && tmp.outcome == NET_UART_M_TX_SUCCEEDED)
        return NET_UART_M_CONTRACT_HOLD;
    if (event->releaseObserved) tmp.driverReferenceOutstanding = false;
    *record = tmp;
    if (!tmp.acceptedKnown) return NET_UART_M_DRIVER_PENDING;
    if (!tmp.accepted) return NET_UART_M_DRIVER_REJECTED;
    if (tmp.outcome == NET_UART_M_TX_UNKNOWN) return NET_UART_M_DRIVER_PENDING;
    return NET_UART_M_OK; // 결과 '관측' 완료; outcome FAILED도 OK일 수 있다.
}
```

record/event는 같은 Owner 문맥에서 안정화하고 겹치지 않는다. event는 실제 동일 제출의 관측만 담고 release는 DMA/FIFO/Driver의 실제 참조 종료 근거다. return OK는 송신 성공 flag가 아니므로 반드시 record.outcome을 별도로 읽는다. accepted=false의 반환만으로 release를 true로 만들지 않는다. callback이 동기 Driver return보다 먼저 와도 outcome/release 관측을 보존하고 admission 확인까지 pending으로 둔다. duplicate event는 같은 최종값을 유지하고 두 번의 완료·해제·count를 만들지 않는다. 상충 결과는 원 record 불변/HOLD다.

<a id="reclaim"></a>
```c
bool NetUartM_CanReturnBuffer(const NetUartMTxRecord *record)
{
    return record != NULL && record->identityRetained &&
        !record->driverReferenceOutstanding && !record->bufferReturned;
}
```

이 predicate는 allocator가 아니다. 실제 저장소 반환이 성공한 동일 직렬 구간에서만 bufferReturned=true를 기록한다. outcome이 아직 UNKNOWN이어도 **해제 근거와 별도 identity 기록 보존**이 충족되면 물리 버퍼 반환은 가능하다. callback이 버퍼 내용을 필요로 하면 필요한 값 사본을 record 측에 먼저 남겨야 한다. 해제 이후에도 outcome·Query/원 discrete 귀속이 미확정이면 identity를 지우거나 token을 재사용하지 않는다. 반대로 outcome이 SUCCEEDED여도 Driver 참조가 남으면 반환하지 않는다. timer·reset·link-down만으로 physical release를 추정하지 않는다.

<a id="query-observation"></a>
## Query 완료 관측을 전달하는 조건

M02는 [최종 TX 집계 전개](UART_M_QUERY_CONTROL.md)의 같은 Device/Session/DOMAIN Boot/QueryID/scope 항목을 읽는다. queryItem=true만으로 같은 Query가 되는 것은 아니며 원 항목의 완전 identity와 순서 연결이 필요하다. accepted=true, outcome=SUCCEEDED가 실제로 확인된 응답을 **한 번만** 포함한다. BUFFER RELEASE·enqueue·End 자체·QUERY_ID0 자발 갱신·중복 재제출은 수에 포함하지 않는다. queryTerminalAccounted는 M02의 동일 항목 반영이 실제 완료한 뒤 원자적으로 기록한다. 단순 ObservePort 호출 성공으로 미리 설정하지 않는다.

입력10건 중8개 최종성공·2개 확정실패이면 모든 결과와 End순서를 입증한 경우 status2/count8 후보다. 미확정2개를 확정실패로 바꾸어 End를 내지 않는다. release가 먼저 온 항목도 terminal 결과가 없으면 완료수에 들어가지 않는다. u16 count overflow·Getter 실패·association부재는 빈 조회0/0이 아니다. 추가 ACK·ordinal·Query failure kind·timeout 숫자는 만들지 않는다.

<a id="late-failure"></a>
## F09 필수 실패 sink와 정확한 세 reason

DOMAIN 소유 `DomainIf_RxNetworkLocalFailure`는 accepted discrete의 필수 Manager/Supervision 경계다. optional RxObserver11은 이 sink의 대체물이 아니다. 기존 [M03](../modules/M03_TYPED_HANDOFF.md)·[B12 F09](CAN_PORT_RECOVERY.md)의 원형·NetworkLocalFailure_t를 유지하며 다른 MCU에 C2 본체를 복제하지 않는다.

| 적용 조건 / 기존 reason | 연결·금지 |
|---|---|
| 로컬 완전 접수된 COMMAND/REQUEST_RESULT/OCCURRENCE | 원 itemKind·identityGeneration·존재 플래그·commandId 또는 원 RequestContext/Device 또는 occurrenceId 보존. 현재 recipient Session으로 과거 RequestSession을 교체하지 않음 |
| DRIVER_SUBMISSION_FINAL | 접수 후 **최종 Driver 제출 실패** 확정. temporary busy/결과미확인/이미in-flight오류를 이 reason으로 바꾸지 않음 |
| EXPIRED_BEFORE_SEND | 아직 송신 시작 전, 확인된 원 사용기한 만료. 임의 timeout·송신 중 오류에 적용 금지 |
| CONTEXT_INVALIDATED_BEFORE_SEND | 아직 송신 시작 전, 바로 이 항목 문맥 무효화가 확인됨. 다른peer변경/불명확상태와 구분 |
| elapsedSinceAdmissionMs | 로컬 접수 이후 경과이며 원 Command/Result/Occurrence AGE 또는 기능 Deadline 대체 아님 |

sink 준비/실제 반환/귀속 근거가 없으면 해당 discrete 성공 접수·후발 실패 지원을 약속하지 않는다. counter만 증가시켜 전달 완료라 하지 않는다. Generation0·BOOT 수명 미합의는 원 세대값을 현재 Boot로 만들어 메우지 않는다. Query 자체에는 해당 kind가 없어 Query 내부·transport 진단에 남기며 COMMAND로 위조하지 않는다. 주기 보고 손실·동기거부·CRC reject·observer오류도 이 failure대상으로 확대하지 않는다. 통신 실패를 차량 FAILED·UNCONFIRMED·기능 종료로 자동 합성하지 않는다.

<a id="recovery"></a>
## 손실·복구와 감독

| 실제 사건 | 처리와 회복 전제 |
|---|---|
| CRC/길이/조립 기한 오류 | [Stream](UART_FRAME_STREAM.md#stream-next)의 시작+1 suffix 재탐색. 기존 byte시각 유지 |
| CRC정상 enum/문맥/consumer 오류 | 신뢰한 전체 경계 소모, typed 정상 publish·기능 liveness 갱신 없음 |
| RXoverflow/byte 손실 | 실제 손실 단위를 기록하고 미완성 후보 분리. 재탐색은 잃은 byte 복원이 아님 |
| UART 부분 TX/취소 여부 불명확 | 제출 결과·참조 수명·양단 frame경계가 불명확하므로 이전 나머지byte를 새 frame으로 이어 보내거나 Request를 자동 재전송하지 않음. 실제 Port drain/cancel/동일profile 회복 근거 필요 |
| Queue full | 새 입력 전체 거부. 안정 저장소가 실제 반환된 후 현재 문맥·기한을 검증한 새 접수만 가능 |
| peer/Session/DOMAIN Boot 변경 | 구 Query/Request/association 완료 자격 분리. driver가 참조 중인 버퍼는 별도 보존 |
| link up/local reset | 이전 pending 자동 replay·권한·source TTL 복원 없음. 실제 local init/reset만 health counter reset허용 |
| 정상 통신·유효 열화 보고 | 원 품질/원 AGE 유지. peer reboot·Query완료·reconnect로 counters0 리셋 금지 |
| WINDOW2 | 정상 Tx/Rx·표시·timeout·liveness 모두 inactive HOLD. 미연결 상태를 필수 timeout fault로 만들지 않음 |

NetworkHealthSnapshot_t의 transportState는 로컬 링크 상태이고 hasValidRx/lastValidRxAgeMs는 검증한 정상 수신 관측이다. rxCrcRejectCount/rxFormatRejectCount/rxOverflowCount/txFinalFailureCount/busOffCount는 u32 포화이며 UART에 가상 CAN bus-off를 만들지 않는다. callback 부재를 txFinalFailure로 세거나 모든 format-valid 입력을 모든 peer 생존으로 확대하지 않는다. 실제 counter 사건 단위·clock·원 item 실패 전달은 별도 근거다.

전송 성공, 참조 해제, 상대 유효 수신, BLE 전달, App 목록/기능 결과는 각각 독립 증거를 요구한다. 제품 Driver/Host/보드 실행 없이 작성한 문서 명세이며 D-03/D-05, IF-O08/12/14/24/25, O-C08/O-C10, CF01, C2-H01·Generation/BOOT/CIS Environment 및 기존 모든 외부 HOLD를 유지한다.
