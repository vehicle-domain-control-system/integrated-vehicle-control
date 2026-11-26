# UART-C 접수·순서·AGE·전송 수명

[개요](../06_UART_C_OVERVIEW.md) · [자료](../data/UART_C_DATA.md) · [Frame/Router](UART_C_FRAME_ROUTER.md) · [Pi→CIS](../flows/UART_C_PI_TO_CIS.md) · [CIS→Pi](../flows/UART_C_CIS_TO_PI_PERMISSION.md)

L03/M04/F11~F13은 원 의미 사본·AGE·순서를, L05/M07/F19~F21은 안정 저장소, M08/F22~F23은 장치 인계/관측, M09/F24~F25는 실제 통신 진단을 맡는다. M06/F16~F18의 Frame 조립·codec와 구분한다. 모든 `NetUartC_*` 함수는 문서의 `PROPOSED_PRIVATE_FUNCTION`이며 실제 Driver·SDK API가 아니다. 아래 산술/관측 의사코드는 B13의 검수된 공통 불변식을 UART-C 전용 자료로 풀어 쓴 것이고 UART-M 구현 공유를 요구하지 않는다.

<a id="returns"></a>
## 기존 로컬 API와 반환

| 기존 경계 | 호출자 → 수신 본체의 담당 | 반환이 증명하는 것 |
|---|---|---|
| PiNet_PublishVisionResult / PiNet_PublishVisionStatus | Pi 비전/상태 생산자 → Pi Network/Serial 연결 | 기존 NetSubmitResult4개로 로컬 완전 인수 또는 미접수 |
| CisNetPriv_RelayPiPermission | CIS CAN dispatcher → Network private relay | 같은 원 NetDomainPermission+NetCanRxMeta 전체인수; 변환 근거와 준비는 별도 전제 |
| CisApp_OnPiVisionResult / CisApp_OnPiVisionStatus | CIS Network → CIS 기능 소비자 | void. 호출 반환만 확인; CAN TX/DOMAIN commit 증거 아님 |
| PiApp_OnCisPermission | Pi Network → Pi 기능 소비자 | void. 현재 허용/물리 동작의 성공값이 아님 |

실제 경계는 공개5 + private relay1 = 6개 선언이다. 기존 원형은 [Result](uart_c/P_VISION_RESULT.md), [Status](uart_c/P_VISION_STATUS.md), [Permission](uart_c/P_PERMISSION.md)에 인용한다. C 표현은 Pi 언어/ABI를 확정하지 않는다. callback 인자는 같은 수신 사건에 속한 const DTO+meta이며 반환까지 빌린다. 비동기 소비자는 전체 자료와 기준시각을 함께 복사한다.

| NetSubmitResult 기존 값 | 로컬 접수 의미 |
|---|---|
| NET_SUBMIT_OK = 0 | caller 포인터와 독립된 완전한 DTO·원 identity·AGE기준·필요 meta 사본 확보 |
| NET_SUBMIT_INVALID_ARGUMENT = 1 | NULL·표현/확인된 코드 오류, 새 항목 미접수 |
| NET_SUBMIT_NOT_READY = 2 | 원 관측과 현재 기동의 결합·상대 profile·소비/실패 경계·실행문맥 등 필수근거 미준비 |
| NET_SUBMIT_NO_CAPACITY = 3 | 전체 사본을 보관할 공간 부족, 부분 접수 없음 |

정상 형식의 INVALID/FAULT/NO_DATA/UNKNOWN 품질을 인자 오류로 취급하지 않는다. 공개 반환에 새 실패값을 추가하거나 private 결과를 숫자 cast하지 않는다. 접수 후 실패는 과거 OK를 소급 변경하지 않는다. Pi/CIS에 DOMAIN C2 failure sink·TxPort를 복제하지 않는다. 실제 Pi/CIS 실패 전달 연결은 IF-O12/D-03/G06 HOLD다.

<a id="admission"></a>
## 생산자 입력에서 안정 pending 사본으로

Pi App은 원 평가의 값·두 품질·평가번호·현재 AGE를 제공한다. T25는 App에 pi_boot_id 초기화 의무를 두지 않고 runtime/Network가 로컬 인수 때 같은 관측 세대를 고정하도록 한다. 따라서 아직 결합되지 않은 caller의 boot 필드를 Wire 검사처럼 먼저 읽어 거부하거나 현재 Boot로 구 관측을 재표시하면 안 된다. 공개입력의 의미필드 확인 후, **그 관측을 실제로 만든 기동과 현재 runtime이 같은 근거**로 복사본의 boot를 결합하고 leaf Wire 검사를 수행한다. 그 연결 근거가 없으면 NOT_READY다.

아래는 API 본체가 지켜야 하는 C형 처리 순서다. 한글 지역 동작은 기존 M04/M07 소유 저장소의 원자적 인수 효과를 명시하며 새 SDK 호출 이름을 만들지 않는다. 실제 저장소/직렬화 primitive는 Owner 바인딩 OPEN이다.

```c
/* PiNet_PublishVisionResult 본체의 문서 처리 순서. 공개 원형은 그대로다. */
if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
NetVisionResult candidate;
// pi_boot_id는 아직 읽지 않는다. 나머지 10개 값은 호출 구간의 const 입력에서 복사한다.
candidate.vision_sequence = message->vision_sequence;
candidate.vision_age_ms = message->vision_age_ms;
candidate.occupant_presence = message->occupant_presence;
candidate.presence_validity = message->presence_validity;
candidate.presence_reason = message->presence_reason;
candidate.occupant_count = message->occupant_count;
candidate.count_validity = message->count_validity;
candidate.count_reason = message->count_reason;
candidate.vision_status = message->vision_status;
candidate.vision_fault_code = message->vision_fault_code;
if (원 평가에 결합된 runtime 기동의 동일성/비예약값 근거 없음) return NET_SUBMIT_NOT_READY;
candidate.pi_boot_id = 확인된 동일 원 평가의 Pi boot;
if (!NetUartC_ValidateVisionResult(&candidate)) return NET_SUBMIT_INVALID_ARGUMENT;
if (Pi/CIS 동일profile·실행문맥·Driver수명·실패전달 연결 미준비) return NET_SUBMIT_NOT_READY;
if (candidate와 원 AGE 기준점/세대 근거 전체를 담을 저장소 없음) return NET_SUBMIT_NO_CAPACITY;
// 성공 원자 구간: 전체 candidate + 원 평가 association + AGE 기준점 사본을 함께 게시.
// caller 포인터는 보관하지 않으며 기존 pending의 일부 필드를 먼저 덮지 않는다.
return NET_SUBMIT_OK;
```

```c
/* PiNet_PublishVisionStatus 본체의 대응 순서. Result 상태를 같이 갱신하지 않는다. */
if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
NetVisionStatus candidate;
candidate.status_sequence = message->status_sequence;
candidate.vision_status = message->vision_status;
candidate.active_fault = message->active_fault;
candidate.last_fault = message->last_fault;
candidate.status_age_ms = message->status_age_ms;
candidate.affected_function = message->affected_function;
if (원 상태평가와 runtime 기동 동일성/비예약값 근거 없음) return NET_SUBMIT_NOT_READY;
candidate.pi_boot_id = 확인된 동일 원 상태평가의 Pi boot;
if (!NetUartC_ValidateVisionStatus(&candidate)) return NET_SUBMIT_INVALID_ARGUMENT;
if (해당profile·문맥·Driver수명·실패전달 연결 미준비) return NET_SUBMIT_NOT_READY;
if (전체 candidate와 독립 상태평가/AGE 근거 저장소 없음) return NET_SUBMIT_NO_CAPACITY;
// 전체 원 상태평가 사본 게시. READY로 과거 Result나 last_fault를 재작성하지 않는다.
return NET_SUBMIT_OK;
```

위 기동 결합의 실제 atomic capture/API가 현재 제공되었다는 뜻이 아니다. 회신 전 실행은 HOLD이며 임의 nowBoot 상수 대입으로 통과시키지 않는다. 호출자가 복사 도중 입력을 바꾸지 않는 const/직렬화 계약도 필요하다.

Permission relay는 원 DOMAIN power/operation/alive 값·독립 품질·source AGE와 원 CAN meta를 같은 사건으로 복사한 뒤 [중계 함수](../flows/UART_C_CIS_TO_PI_PERMISSION.md)에서 변환한다. NetDomainPermission에 없는 quality_reason, CIS 새 relay revision, use_limit을 근거 없이 NONE/0/1500으로 채우지 않는다. App이 만든 NetPiPermission을 private relay 원 인자로 바꾸지 않는다. CAN dispatcher가 같은 사건을 CisApp_OnDomainPermission과 private relay로 인계하며 App이 relay를 중복 호출할 의무를 만들지 않는다.

## pending에서 장치로

| 단계 | 구체 수행·소유 | 거부/미확인 처리 |
|---|---|---|
| pending 현재값 | 아직 Driver 제출 전이며 같은 의미 key·순서 보존이 입증된 항목만 교체 후보 | 정확한 key/동시성/저장소 깊이 미확정. 원 품질저하를 숨기거나 참조중 frame 교체 금지 |
| 송신 직전 | 원 Boot/원 평가의 동일성 재확인, 아직 더하지 않은 로컬 대기/hop AGE만 누적 | 시간 불확실·permission 원 기한 미입증이면 해당 경로 제한 |
| 의미→payload | Result11/Status7/Permission7 필드의 정확 EncodePayload | 실패면 기존 의미사본/Frame OUT 불변 |
| payload→Frame | 방향별 LINK_SEQUENCE, VERSION1/Type/exactP와 CRC 생성 | 초기/할당/wrap 정책 미확정; source sequence를 전송마다 증가시키지 않음 |
| Driver 직전 | 원 사본·Frame·localToken/epoch 기록을 먼저 준비/게시 | 즉시 callback에서도 원 항목을 찾을 수 없으면 제출 HOLD |
| Driver 이후 | Frame 불변, admission/final outcome/release 독립 관측 | 반환 오류만으로 미접수/참조해제를 추정하지 않음 |

Frame 준비 후 아직 미관측인 Driver 대기를0으로 간주하지 않는다. 실제 인계지연 상한/관측 증거 없는 permission은 신선도나 연속 허용을 보증할 수 없다. 새 retry 횟수·Queue 깊이·timeout·RTOS를 정하지 않는다.

Driver 제출 시도에 선택한 원 의미 항목은 결과 미확인 동안 그 시도에 예약한다. 다른 Frame 공간이 남거나 release가 먼저 와도 같은 관측·relay를 새 token/LinkSequence로 다시 선택하지 않는다. 실제 미접수와 참조 없음이 함께 확인되거나 해당 시도의 결과·참조 상태가 확인된 뒤에만 예약 해제를 검토하고, 이후 원 AGE·기동·기한을 다시 검사한다. 저장소 반환과 재제출 권한을 분리한다.

<a id="age"></a>
## AGE 산술과 기한

```c
NetUartCResult NetUartC_AccumulateAge16(uint16_t originalAgeMs,
    uint32_t unaccountedLocalMs, uint32_t unaccountedHopMs,
    bool timeBasisVerified, uint16_t *outAgeMs)
{
    if (outAgeMs == NULL) return NET_UART_C_INVALID_ARGUMENT;
    if (!timeBasisVerified) return NET_UART_C_TIME_HOLD;
    uint64_t sum = (uint64_t)originalAgeMs + unaccountedLocalMs + unaccountedHopMs;
    *outAgeMs = sum > 65535u ? 65535u : (uint16_t)sum;
    return NET_UART_C_OK;
}
```

timeBasisVerified는 원 AGE 기준점·이미 누적된 구간·같은 로컬 시간원/연속성·wrap 해석을 입증한 내부 결과다. 숫자 true가 증거가 아니다. candidate UART hop50ms는 측정 상한이 아니며 Driver 미관측 지연을0으로 만드는 근거도 아니다. UINT32 경과 두 개를 더해도 uint64 중간값으로 overflow를 방지한다. 포화65535는 원 TTL 이내 보증이 아니고 미확인 AGE를65535로 채우라는 뜻도 아니다. 호출 실패는 OUT 불변이다.


VISION_AGE·STATUS_AGE·SOURCE_AGE의 기준은 각각 원 카메라 판정·원 기능상태 평가·원 DOMAIN 전원허용 판단이다. callback received_at_ms는 실제 로컬 수신 근거이며 원 AGE0을 발급하지 않는다. 원 DOMAIN/CIS/Pi 시계를 서로 빼지 않는다. PiVision 재전송/Status READY/CIS reboot/새 LINK_SEQUENCE가 원 AGE를 낮출 수 없다.

같은 평가 A=AGE20@RX100과 B=AGE15@RX130을 로컬150에서 비교하면 A70/B35ms다. B가 새 Frame이어도 같은 평가를35ms로 젊게 만들지 않고 실제 구간/누락 근거를 다시 확인한다. 이미 포함한 CAN/CIS/UART 구간은 다시 더하지 않는다. 양단이 승인한 보수적 전달 여유가 없는 상황을 0ms로 처리하지 않는다. u32 경과의 wrap/장기중단은 숫자 뺄셈만으로 증명하지 않는다.

| 시간과 상태 | 원 의미·분리 |
|---|---|
| 조립50ms | SYNC부터 실제 끝byte 조립 기한. 비교 > 또는 ≥ 미확정 |
| UART 전달 여유50ms | 미반영 hop 산정 후보, 실측 상한 아님 |
| Result100/300ms | 송신 주기 / 수신 중단 후보, 관측의 유효성 보증 아님 |
| Status1000/3000ms | 상태 송신 주기 / 수신 중단 후보, Result freshness와 독립 |
| Permission500/1500ms | relay 보고 주기 / 수신 중단 후보 |
| Permission USE_LIMIT1500ms | 기존 Wire 후보. 동일 후방 원 판단 전달을 양끝에서 검증할 때만500ms 후보 변경; 자동 대입 없음. raw0 의미 미정 |
| 후방 활성 Power 원500ms / 실제 재평가 약200ms | 확정 합의. effective AGE>=500ms 비사용;499ms도 품질·현재 Boot·원 association 필요. Operation/Alive·후방 거리 TTL과 별개 |

65535는 포화된 최소 경과이며 fresh가 아니다. 후방 원500ms/도달 시 비사용은 합의로 고정하고 원 association과 relay 한도의 실제 근거·경계조건을 확인한 뒤 소비 가능 여부를 평가한다. 하나라도 확인 불가하면 **새 허용의 근거로 사용하지 않음**을 기록하며 0/OK 불허와 255/NO_DATA 미확인을 구분한다. 물리 전원 차단/프로세스 종료로 확대하지 않는다.

<a id="order"></a>
## 원본·순서·반복 분류

M04가 정상 frame/typed검사 후 같은 실제 수신의 원 sourceBoot와 source sequence, 현재 수용 문맥, 전체 raw 및 의미/AGE 대응을 확인한다. VisionSequence·StatusSequence·CIS relay UpdateSequence는 각각 다른 평가 공간이다. 원 DOMAIN lineage가 payload에 없다는 한계는 permission 소비에서 별도로 남는다.

```c
NetUartCResult NetUartC_ClassifyRx(const NetUartCRxComparison *c)
{
    if (c == NULL) return NET_UART_C_INVALID_ARGUMENT;
    if (!c->contextBound) return NET_UART_C_CONTEXT_HOLD;
    if (!c->sameContext || !c->hasAcceptedFrame) {
        if (!c->transitionAuthorized) return NET_UART_C_CONTEXT_HOLD;
        if (!c->ageRelationAccepted) return NET_UART_C_TIME_HOLD;
        return NET_UART_C_OK;
    }
    if (c->incomingLinkSequence == c->previousLinkSequence) {
        if (!c->rawExactlyEqual) return NET_UART_C_ORDER_HOLD;
        if (!c->ageRelationAccepted) return NET_UART_C_TIME_HOLD;
        return NET_UART_C_DUPLICATE;
    }
    if (!c->nonWrappingOrderKnown || c->incomingLinkSequence < c->previousLinkSequence)
        return NET_UART_C_ORDER_HOLD;
    if (!c->ageRelationAccepted) return NET_UART_C_TIME_HOLD;
    if (c->sameImmutableInput) {
        if (!c->semanticExactlyEqual) return NET_UART_C_ORDER_HOLD;
        return NET_UART_C_DUPLICATE;
    }
    if (!c->functionalTransitionKnown) return NET_UART_C_ORDER_HOLD;
    return NET_UART_C_OK;
}
```


nonWrappingOrderKnown은 숫자 증가만으로 참이 되지 않는다. 65535→0의 modulo 표기는 비교창/최초 수용의 승인과 다르므로 미합의 wrap는 ORDER_HOLD다. 새 Boot나 최초 frame도 transitionAuthorized 근거가 필요하다. 중복은 새 관측/현재 허용/AGE기준을 만들지 않으며 transport 수신 사실과 의미의 반복을 구별한다. 정상 CRC·enum을 통과해도 sourceBoot 수용 실패 자료를 정상 callback/원 관측으로 승격하지 않는다.

이 분류는 상태를 쓰지 않으며 반환 enum은 유지한다. `DUPLICATE` 안에서 같은 Link/raw의 정확한 frame 중복과 수용된 새 Link의 동일 관측 반복을 호출자가 구분한다. 전자는 정상 감시·callback을 갱신하지 않는다. 후자는 Vision Result/Status 각각의 수신 감시를 갱신하고 기존 callback으로 전달한다. 이 Vision 규칙을 Pi Permission callback에 일괄 적용하지 않는다. sameImmutableInput=true는 원 평가 association을 확인한 결과여야 한다. 같은 LINK_SEQUENCE의 다른 raw는 충돌, 새 LINK_SEQUENCE의 같은 원 평가·같은 의미는 반복이다. 반복의 AGE 비교는 같은 소비시각으로 맞춰야 하며 한 값의 AGE 감소를 무시해 새 값으로 받아들이지 않는다. callback void의 실제 commit 여부는 반환값으로 확인할 수 없고, caller가 별도 성공을 만들어 순서장부/기능cache를 갱신하지 않는다. 실제 수용장부 시점·회복 후 소비 연결은 Owner 계약 OPEN이다.

반복을 받아들인 경우에도 최신 transport LinkSequence와 해당 원 raw 사본·실제 수신 메타를 보수적 AGE 근거와 함께 원자적으로 갱신할 준비가 필요하다. 예를 들어 같은 평가의 Link10→20을 수용한 뒤15가 도착하면 확인된 비wrap 전제에서 최신20을 기준으로 제한한다. 공간 확보/동시 반영이 실패하면 순서장부만 먼저 진전시키지 않는다. raw AGE15인 Link20이 다시 왔을 때 비교 대상은 원 raw15이며, 별도로 누적한 보수 AGE70과 raw15를 비교해 충돌로 만들지 않는다. 이 transport 장부 갱신은 새 원 관측이나 현재 허용의 발급이 아니다. Vision의 유효 반복에서는 아래 Flow처럼 callback을 호출하여 해당 기능의 수신 감시에 연결한다. 분류 당시 comparison·raw frame·typed/meta·AGE 근거의 안정 사본을 같은 직렬 처리 구간에서 유지한다. 저장소/consumer 바인딩 미준비이면 장부 일부만 먼저 게시하지 않는다. void callback의 반환을 App 저장 ACK로 바꾸거나 외부 App 반영을 Network 원자 갱신과 같은 transaction으로 가정하지 않는다.

<a id="port-observe"></a>
## Driver 관측과 참조 해제

record는 호출 전 원 의미사본/Frame과 함께 준비한다. 초기 acceptedKnown=false, outcome=UNKNOWN, driverReferenceOutstanding=true, bufferReturned=false, identityRetained=true다. localToken/epoch는 지역 제안 식별이며 재사용/재진입 근거가 없으면 제출하지 않는다. 아래 event는 실제 SDK 관측에 결합된 경우만 입력한다.

```c
NetUartCResult NetUartC_ObservePort(NetUartCTxRecord *record,
    const NetUartCPortEvent *event)
{
    if (record == NULL || event == NULL) return NET_UART_C_INVALID_ARGUMENT;
    if (!record->identityRetained || record->localToken != event->localToken ||
        record->localEpoch != event->localEpoch) return NET_UART_C_CONTEXT_HOLD;
    if (event->outcome != NET_UART_C_TX_UNKNOWN && event->outcome != NET_UART_C_TX_SUCCEEDED &&
        event->outcome != NET_UART_C_TX_FAILED) return NET_UART_C_INVALID_ARGUMENT;
    NetUartCTxRecord tmp = *record;
    if (event->admissionObserved) {
        if (tmp.acceptedKnown && tmp.accepted != event->accepted) return NET_UART_C_CONTRACT_HOLD;
        tmp.acceptedKnown = true; tmp.accepted = event->accepted;
    }
    if (event->outcome != NET_UART_C_TX_UNKNOWN) {
        if (tmp.outcome != NET_UART_C_TX_UNKNOWN && tmp.outcome != event->outcome)
            return NET_UART_C_CONTRACT_HOLD;
        tmp.outcome = event->outcome;
    }
    if (tmp.acceptedKnown && !tmp.accepted && tmp.outcome == NET_UART_C_TX_SUCCEEDED)
        return NET_UART_C_CONTRACT_HOLD;
    if (event->releaseObserved) tmp.driverReferenceOutstanding = false;
    *record = tmp;
    if (!tmp.acceptedKnown) return NET_UART_C_DRIVER_PENDING;
    if (!tmp.accepted) return NET_UART_C_DRIVER_REJECTED;
    if (tmp.outcome == NET_UART_C_TX_UNKNOWN) return NET_UART_C_DRIVER_PENDING;
    return NET_UART_C_OK; // 결과 '관측' 완료; outcome FAILED도 OK일 수 있다.
}
```

record/event는 같은 Owner 문맥에서 안정화하고 겹치지 않는다. event는 실제 동일 제출의 관측만 담고 release는 DMA/FIFO/Driver의 실제 참조 종료 근거다. return OK는 송신 성공 flag가 아니므로 반드시 record.outcome을 별도로 읽는다. accepted=false의 반환만으로 release를 true로 만들지 않는다. callback이 동기 Driver return보다 먼저 와도 outcome/release 관측을 보존하고 admission 확인까지 pending으로 둔다. duplicate event는 같은 최종값을 유지하고 두 번의 완료·해제를 만들지 않는다. 상충 결과는 원 record 불변/HOLD다.

<a id="reclaim"></a>
```c
bool NetUartC_CanReturnBuffer(const NetUartCTxRecord *record)
{
    return record != NULL && record->identityRetained &&
        !record->driverReferenceOutstanding && !record->bufferReturned;
}
```

이 predicate는 allocator가 아니다. 실제 저장소 반환이 성공한 동일 직렬 구간에서만 bufferReturned=true를 기록한다. outcome이 아직 UNKNOWN이어도 **해제 근거와 별도 identity 기록 보존**이 충족되면 물리 버퍼 반환은 가능하다. callback이 버퍼 내용을 필요로 하면 필요한 값 사본을 record 측에 먼저 남겨야 한다. 해제 이후에도 outcome·원 관측/relay 귀속이 미확정이면 identity를 지우거나 token을 재사용하지 않는다. 반대로 outcome이 SUCCEEDED여도 Driver 참조가 남으면 반환하지 않는다. timer·reset·link-down만으로 physical release를 추정하지 않는다.


<a id="recovery"></a>
## 오류·복구·후발 실패

| 실제 사건 | 처분과 보존할 근거 |
|---|---|
| header/CRC/조립기한 오류 | 후보시작+1 suffix 재탐색, byte와 원 timestamp 함께 유지 |
| CRC정상 enum/Boot/consumer 미준비 | 전체 Frame 경계 소비. 잘못된 형식을 정상 생존으로 세지 않음 |
| 실제 overflow/reset | 잃은 구간·old half-frame 분리. reset은 새 sourceBoot 발급 아님 |
| 부분 TX / 취소·drain 불명 | 남은 바이트를 새 Frame에 이어 붙이지 않음. 실제 참조 종료/같은profile 회복 근거 필요 |
| 로컬 접수 후 만료/문맥상실/확정 TX 실패 | 해당 Pi/CIS 원 관측·relay에 귀속하고 기존 진단/소비자 실패 연결 확인. 과거 OK 반환 변경 없음 |
| release만 관측 | Frame 회수 가능 여부만 평가, 원 identity/미확정 outcome 보존 |
| link up / Pi 또는 CIS 재시작 | 구 평가·구 permission 자동 replay/AGE0/과거 허용 복원 금지 |
| DOMAIN만 재시작 | CISBoot가 같아도 원 DOMAIN association 무효화/새평가 수용 근거를 따로 확인 |
| 정상 열화 보고 | 통신 형식과 기능 품질을 각각 기록. Presence/Count/Status 독립 보존 |

실제 후발 실패 전달/소비 무효화 경로는 IF-O12/D-03/G06 HOLD다. DOMAIN의 NetworkLocalFailure kind/reason을 Pi/CIS에 가정하지 않으며 새 공개 실패 API/차량 FAILED를 만들지 않는다. callback 부재는 확정 실패가 아니며 timeout만으로 Driver 참조를 회수하지 않는다. 로컬 진단 counter는 실제 사건 단위·u32포화로 기록하고 정상 frame/peer reboot에서0으로 만들지 않는다. UART에 CAN bus-off를 합성하지 않는다.

원 Owner35·C2 Generation0/BOOT/CIS Environment·IF-O05/O09/O12/O13/O19/G02/G06 및 S6Q-01~14를 유지한다. 실제 Driver ABI/핀/전압/clock/Queue/Task·제품/Host/실보드 시험은 확인하지 않았다. 정적 설계 검토는 UART-C 양단 채택이나 실제 바인딩의 증거가 아니다.

<a id="named-caller-lifecycle"></a>
## 기존 송신 Flow의 관측·회수 호출 연결

설계상 호출자는 [Pi 생산→Driver Flow](../flows/UART_C_PI_TO_CIS.md#tx)와 [CIS Permission relay Flow](../flows/UART_C_CIS_TO_PI_PERMISSION.md#named-private-paths)의 M08 장치 관측 담당 및 M07 저장소 담당이다. 두 책임은 확인된 같은 Owner 문맥에서 직렬화한다. 아래는 기존 Flow의 호출 순서이며 새 Task·공개 API·함수 선언이 아니다. 실제 Task/Driver callback 심볼·실행 바인딩·해제 근거는 IF-O12/D-03/D-05/G06 HOLD다.

```c
// M08: 실제 사건과 동일 token/epoch의 보존 record만 선택한다.
observed = NetUartC_ObservePort(record, event);
// INVALID_ARGUMENT / CONTEXT_HOLD / CONTRACT_HOLD이면 사건을 적용하지 않고 중단한다.
// OK / DRIVER_PENDING / DRIVER_REJECTED는 record의 서로 다른 관측 상태다.
// 성공/실패 outcome과 접수·release를 각각 읽으며 OK만으로 TX 성공을 만들지 않는다.
// M07: 위 관측이 적용된 동일 record를 같은 직렬 구간에서 검사한다.
if ((observed == NET_UART_C_OK || observed == NET_UART_C_DRIVER_PENDING ||
     observed == NET_UART_C_DRIVER_REJECTED) && NetUartC_CanReturnBuffer(record)) {
    // 실제 buffer 반환이 성공한 경우에만 record->bufferReturned = true;
    // 실패하면 false 유지. callback에 필요한 사본은 반환 전에 별도로 보존한다.
}
// outcome 미확정이면 원 identity/예약을 보존한다. 반환은 재송신 허가가 아니다.
```

`NetUartC_ObservePort`는 record를 소비·갱신하며 별도 OUT이 없다. 오류/HOLD에서는 기존 record가 불변이고, PENDING/REJECTED도 해제 관측을 담을 수 있어 조건을 충족하면 `NetUartC_CanReturnBuffer`를 평가한다. 후자는 record를 읽는 순수 predicate로 상태를 쓰지 않는다. 참조가 살아 있거나 이미 반환한 버퍼는 반환하지 않는다. 이 표기만으로 allocator 반환 성공·실제 callback 도착·최종 TX 결과를 확인한 것은 아니다.
