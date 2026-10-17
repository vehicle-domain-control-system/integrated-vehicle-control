# VSS_STATUS — 음향 서비스와 고장 이력

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [Data/전체 필드](../../data/CAN_NODE_DATA.md#VSS_STATUS) · [흐름](../../flows/CAN_CIS_WINDOW_VSS.md#VSS_STATUS)

`WIRE-P01-VSS_STATUS` / `RX-P01-VSS_STATUS-DOMAIN`. VSS→DOMAIN, 후보 ID 0x340 / used28 / frame32 / DLC13. Wire는 다른 MCU 사이의 프레임 전달이며 원격 C 호출이 아니다.

## 책임과 호출 경계

VSS 기능 Owner가 원 의미를 만들고 Network가 M03 typed 인계→M04 보관·시간→M05 Codec→M07/M08 Driver를 맡는다. 수신 측은 역방향으로 검사하여 DOMAIN 로컬 소비자에게 전달한다. ISR은 안정된 장치 관측 확보까지만 맡으며 의미 API는 Task/소유자 문맥에서 호출한다. 실제 Task·Lock·queue 용량·Driver HAL 이름은 미검증이다.

<a id="api-VssNet_PublishStatus"></a>
```c
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message);
```

출처: S4A api/vss.md#api-VssNet_PublishStatus.
<a id="api-DomainNetPriv_OnVssStatus"></a>
```c
DomainIf_Status_t DomainNetPriv_OnVssStatus(const NetVssStatus *message, const NetCanRxMeta *meta);
```

출처: S4A api/domain_private.md#api-DomainNetPriv_OnVssStatus.

송신 API 인자는 NULL 불가 IN 객체이며 호출 동안만 읽는다. 수신 callback의 message/meta 또는 state/status/result도 NULL 불가이고, 이후 필요하면 소비자가 반환 전에 복사한다. 공개 DomainIf 본체·Manager 상태는 DOMAIN 소유이고 Network가 중복 구현하지 않는다. NetSubmitResult/C2 반환의 전체 의미는 [기존 반환 정본](../../data/CIS_REAR_DATA.md#declared), 내부 함수 반환은 [공통 자료](../../data/CAN_COMMON_DATA.md)에 있다.

**내부 설계 선택 / 실제 Binding 대기:** A2는 완전 HEALTH Snapshot→기존 INPUT 단일 변환→`NetVssStatus` 생산과 같은 K의 State/Fault 독립 수용을 선택했다. IF-O06/15/17/09/24의 실제 producer·설치 C2·품질/세대·같은 입력 association·후발 관측은 E-S01~05 확인 대기다. [A2 수신 계약](#a2-domain-contract)을 적용하며 문서 선택을 구현 완료로 처리하지 않는다.

<a id="submit"></a>
## 송신 인계

`VssNet_PublishStatus` 본체는 송신 노드 Network M03/M04 소유다. 센서·실행·진단 Owner가 자기 Task/Superloop에서 제출한다. message는 NULL 불가 IN 사본이며 호출 중 불변이다. 다음 절차는 기존 선언에 대한 설계이며 제품 코드를 변경하지 않는다.

```c
NetSubmitResult VssNet_PublishStatus(const NetVssStatus *message)
{
    if (message == NULL || 단일 필드표의 코드·범위·명시 조합이 잘못되면)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (초기화·원 기동·시간·profile·필요한 소비 경계가 준비되지 않았으면)
        return NET_SUBMIT_NOT_READY;
    // VSS 생산 세대+update_sequence는 서비스 평가, last_fault_occurrence는 최근 고장 이력이다.
    // source_age_ms와 last_fault_age_ms는 독립이다. 최근 고장 AGE65535는 오래된 이력의 포화 하한이다.
    if (원 관측·사건 순서 또는 같은 소비시점 AGE를 입증하지 못하면)
        return NET_SUBMIT_NOT_READY;
    if (전체 DTO와 원 근거를 함께 둘 안전한 저장공간이 없으면)
        return NET_SUBMIT_NO_CAPACITY;
    // 한 항목의 전체 값과 시각 기준을 준비한 뒤 대기 저장소에 함께 반영한다.
    // 기존 개별 명령·사건·결과 또는 Driver 참조 중 프레임을 덮어쓰지 않는다.
    return NET_SUBMIT_OK; // caller 포인터를 이후 참조하지 않는 로컬 접수다.
}
```

형식이 정상인 INVALID/STALE/NO_DATA/FAULT 보고는 허용된 각 필드 의미로 보존한다. last_fault_quality는 별도 이력형식0/3이며1/2를 허용하지 않는다. 호출 실패 시 새 자료의 부분 접수는 없고 이전 대기는 유지한다. 후발 Driver 실패는 과거 반환을 변경하지 않는 별도 관측이다. Network는 반환 전에 DTO 11필드·원 generation 대응·각 AGE 기준·진단 K를 전체 복사한다. terminal failure는 RX나 새 CREATED와 무관하게 기존 M09→INPUT 진단→FLOW sink로 원 K/BOOT/TX/raw/release를 인계하며 실제 E-S04 연결은 미검증이다.

<a id="codec"></a>
## 메시지 Codec: 내부 함수 후보

두 원형은 `PROPOSED_PRIVATE_FUNCTION`이다. 호출자는 VSS의 M04 송신 선택과 DOMAIN의 M07/M04 수신 처리다. 전체 공통 검사 순서는 [공통 Codec](../CAN_COMMON_CODEC.md#check-frame), 전체 필드 변환은 [단일 필드표](../../data/CAN_NODE_DATA.md#wire-VSS_STATUS)에 있다. 이 함수는 시간원·Driver·기능 API를 호출하지 않는다. 실행은 Network 소유자 Task/Superloop이며 ISR에서 프레임 해석을 하지 않는다.

| 인자 | 방향·범위·소유·수명 |
|---|---|
| message | IN `NetVssStatus` 전체, NULL 불가. 송신 시점까지 AGE를 반영한 호출 내 불변 사본 |
| source | IN `NetCanSource`, NULL 불가. 확인한 로컬 Boot/TX; Codec는 새 번호 발급/소비를 하지 않음 |
| frame | IN `NetCanFrame`, NULL 불가. 실제 Driver flags/길이/수신시각과 바이트가 동일 입력인 안정 사본 |
| profile | IN 값. 후보 종류를 표시할 뿐 양단 채택 증거를 대신하지 않음 |
| out, meta | OUT caller 저장소, NULL 불가. 입력 및 서로와 겹치지 않음. 모든 검사 성공 뒤 함께 반영; 실패 시 불변 |

Encode `NET_CAN_OK`는 완전 프레임 작성, Decode `NET_CAN_OK`는 형식 변환만 뜻한다. INVALID_ARGUMENT은 NULL/alias 또는 송신 값 오류, MALFORMED는 수신 ID/flags/DLC/길이/padding/코드/명시 조합 오류, CONTRACT_HOLD는 형식·실행 계약 미확인, ORDER_HOLD는 송신 Boot/TX 전제 미확인이다. Decode 성공은 admission·신선도·C2 성공이 아니다.

```c
NetCanResult NetCan_EncodeVssStatus(const NetVssStatus *message,
    const NetCanSource *source, NetCanProfile profile, NetCanFrame *out)
{
    if (message == NULL || source == NULL || out == NULL || 입출력이 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (profile에 대한 실제 양단 선택·지원 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD;
    if (source의 Boot가0이거나 송신 순번 수용 전제를 확인하지 못하면)
        return NET_CAN_ORDER_HOLD;
    if (단일 필드표의 모든 코드·범위·sentinel·명시 조합을 만족하지 못하면)
        return NET_CAN_INVALID_ARGUMENT;
    // vss_state0..3, availability0..2, 두flag0/1, fault_mask bit0..4, last_fault0..5, last_fault_quality는0 또는3만 허용한다. 일반 ValueQuality1/2는 이 필드에서 거부한다.
    NetCanFrame tmp = {0};
    tmp.canId = 0x340; tmp.dlc = 13;
    tmp.length = 32;
    tmp.extended = false; tmp.fdf = true; tmp.brs = true;
    tmp.rtr = false; tmp.driverError = false;
    // 수신시각은 송신의 의미 필드가 아니다. 송신용 receivedAtMs는 사용하지 않는다.
    // B0/4에 source.boot_id/tx_sequence를 u32LE로 기록한다.
    for (단일 필드표의 BOOT/TX를 포함한 각 필드) {
        // offset/width와 source 또는 message 멤버를 표에서 하나씩 선택한다.
        // enum은 검증한 고정폭 raw, 음수 i16은 uint16의 2의 보수로 변환한다.
        uint32_t raw = 표에서 선택한 멤버의 검증된 unsigned 비트열;
        for (uint8_t k = 0; k < 표의 width; ++k)
            tmp.data[표의 offset + k] = (uint8_t)(raw >> (8u * k));
    }
    // padding 영역(28..31)이 있으면 초기값0을 유지한다.
    // 전체 byte/flags/필드표 충족을 재확인한 뒤 한 번에 반영한다.
    *out = tmp; // Driver 호출·TX 순번 발급은 없다.
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeVssStatus(const NetCanFrame *frame,
    NetCanProfile profile, NetVssStatus *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    NetCanResult rc = NetCan_CheckFrame(frame, NET_CAN_VSS_STATUS,
        NET_CAN_NODE_DOMAIN, profile);
    if (rc != NET_CAN_OK) return rc;
    NetVssStatus tmp;
    NetCanRxMeta tmpMeta;
    for (단일 필드표의 각 필드) {
        uint32_t raw = 0;
        for (uint8_t k = 0; k < 표의 width; ++k)
            raw |= (uint32_t)frame->data[표의 offset + k] << (8u * k);
        // unsigned 필드는 해당 고정폭 멤버에 범위 확인 뒤 담는다.
        // i16은 raw<32768이면 raw, 아니면 (int32_t)raw-65536으로 복원한다.
        // 같은 표의 member를 tmp 또는 tmpMeta.source에 저장한다. enum cast는 없다.
    }
    if (단일 필드표의 코드·예약bit·범위·sentinel 조합 또는 Boot0이 발견되면)
        return NET_CAN_MALFORMED;
    // vss_state0..3, availability0..2, 두flag0/1, fault_mask bit0..4, last_fault0..5, last_fault_quality는0 또는3만 허용한다. 일반 ValueQuality1/2는 이 필드에서 거부한다.
    tmpMeta.received_at_ms = frame->receivedAtMs;
    // 이 지점에서 tmp의 모든 멤버와 tmpMeta.source가 채워졌는지 확인한다.
    *out = tmp;
    *meta = tmpMeta; // caller 소유 임시 출력에 함께 반영; 외부 공개는 한 번이다.
    // 순번 장부·liveness·기능 상태는 여기서 갱신하지 않는다.
    return NET_CAN_OK;
}
```

<a id="receive"></a>
## 기존 private 수신의 적용 경계

`DomainNetPriv_OnVssStatus`는 S4A에 있는 DOMAIN Network 소유 선언이다. 새로운 공개 API나 차량 상태 cache를 추가하지 않는다. admission에 성공한 같은 DTO/meta가 caller이며 실행은 DOMAIN Task 문맥이다. 둘 다 NULL 불가 IN, 호출 중만 유효하다. 이후 보관할 자료는 반환 전에 전체 사본을 확보한다.

<a id="api-DomainIf_RxVssState"></a>
```c
DomainIf_Status_t DomainIf_RxVssState( const DomainIf_VssStateReport_t *state);
```

출처: S4A api/domain_rx.md#api-DomainIf_RxVssState.
<a id="api-DomainIf_RxVssFault"></a>
```c
DomainIf_Status_t DomainIf_RxVssFault( const FaultInfo_t *fault);
```

출처: S4A api/domain_rx.md#api-DomainIf_RxVssFault.

아래 단계 이름은 기존 M03/M09의 원 K와 각 호출 결과를 설명하는 의사 표기다. 새 enum·struct·API 선언이 아니다. `ACCEPTED_CONFIRMED`는 해당 경계의 실제 수용 근거, `CALLING/UNKNOWN_FAILED`는 호출 중 또는 미수용이 입증되지 않은 상태다. raw OK 자체로 Core commit을 만들지 않는다.

```c
DomainIf_Status_t DomainNetPriv_OnVssStatus(const NetVssStatus *message, const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || 같은 검증 수신 사건의 쌍이 아니면)
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (원 enum·범위·present/absent 조합이 잘못되면) return DOMAIN_IF_INVALID_ARGUMENT;
    if (완전 producer·설치 profile·원 세대·순서·freshness·동일 입력 근거가 없으면)
        return DOMAIN_IF_NOT_AVAILABLE;
    // 기존 직렬 owner 경계에서 K=(VSS 생산 generation, update_sequence)를 조회한다.
    if (K가 high-water보다 오래되거나 회수·fence된 K이면)
        return DOMAIN_IF_NOT_AVAILABLE; // 새 평가·새 K를 발급하지 않는다.
    bool newKey = 기존 M09에 같은 K가 보호되어 있지 않음;
    if (!newKey) {
        if (보호한 원 평가의 의미·본문·두 AGE 계보와 모순이면)
            return DOMAIN_IF_INVALID_ARGUMENT;
        if (기존 owner가 식별한 부분 후속 처리 기회가 아니면)
            goto aggregate; // 같은 K 재수신/높은 TX: State와 Fault 호출 0.
    } else {
        if (원 K 없는 후속 처리 || 엄격히 새 K 아님 || old 비동기 후속 쓰기 차단 미확인)
            return DOMAIN_IF_NOT_AVAILABLE;
        if (전체 원 DTO/meta·두 결과·이전 결과 이전·후발 sink의 보호 공간 없음)
            return DOMAIN_IF_NOT_AVAILABLE;
        [기존 결과를 bounded 진단/손실 관측에 보존하고 old 미수용 재검토를 종료];
        [전체 원 K 사본과 high-water를 함께 보호; State/Fault NOT_CALLED, raw 없음];
    }
    DomainIf_VssStateReport_t state;
    FaultInfo_t fault;
    [보호된 원 K에서 설치 profile의 승인된 State와 현재 대표 Fault를 준비];

    if (State 단계 == ACCEPTED_CONFIRMED) {
        /* State 재호출 없음; 실제 수용·원 raw·commit 근거 보존. */
    } else if (State 단계 == CALLING || State 단계 == UNKNOWN_FAILED || old State 쓰기 미확인) {
        goto aggregate;
    } else if (State 단계 == NOT_CALLED || State 단계 == EXPLICIT_NOT_ACCEPTED) {
        if (EXPLICIT_NOT_ACCEPTED인데 무부작용 미접수 실증이 없으면) goto aggregate;
        if (!newKey && (owner 부분 후속·준비 변화·DOMAIN 무부작용 보장 중 하나 없음 ||
                        State의 최대1회 재검토 기회를 이미 소모)) goto aggregate;
        if (State 자체 준비 또는 기존 raw 보존·이번 결과/sink 공간이 없으면)
            goto aggregate; // 기존 raw 보존. Fault 준비 여부로 State를 전역 차단하지 않는다.
        if (!newKey) [State 재검토 기회를 실제 호출 전에 소모];
        [기존 raw를 보호하고 State 단계 CALLING으로 표시];
        DomainIf_Status_t stateRc = DomainIf_RxVssState(&state);
        [실제 stateRc/raw 존재와 Owner 근거로 State 수용·명시 미접수·불명을 각각 기록];
    } else return DOMAIN_IF_NOT_AVAILABLE;
    if (State 단계 != ACCEPTED_CONFIRMED || State raw 없음 || State raw != DOMAIN_IF_OK)
        goto aggregate; // State가 미호출/실패/불명일 때 Fault를 선행하지 않는다.

    if (Fault 단계 == ACCEPTED_CONFIRMED) {
        /* Fault 재호출 없음; State 수용도 그대로 보존. */
    } else if (Fault 단계 == CALLING || Fault 단계 == UNKNOWN_FAILED || old Fault 쓰기 미확인) {
        goto aggregate;
    } else if (Fault 단계 == NOT_CALLED || Fault 단계 == EXPLICIT_NOT_ACCEPTED) {
        if (EXPLICIT_NOT_ACCEPTED인데 무부작용 미접수 실증이 없으면) goto aggregate;
        if (!newKey && (owner 부분 후속·준비 변화·DOMAIN 무부작용 보장 중 하나 없음 ||
                        Fault의 최대1회 재검토 기회를 이미 소모)) goto aggregate;
        if (Fault 자체 준비 또는 기존 raw 보존·이번 결과/sink 공간이 없으면)
            goto aggregate; // Fault 미호출, 기존 State/Fault raw와 수용 결과 유지.
        if (!newKey) [Fault 재검토 기회를 실제 호출 전에 소모];
        [기존 raw를 보호하고 Fault 단계 CALLING으로 표시];
        DomainIf_Status_t faultRc = DomainIf_RxVssFault(&fault);
        [실제 faultRc/raw 존재와 callback/consumer 근거로 Fault 단계를 각각 기록];
        // v0.5 observer-only Fault의 Core commit은 해당 없음. 가짜 APPLIED 금지.
    } else return DOMAIN_IF_NOT_AVAILABLE;
aggregate:
    // 미호출 raw 생성 금지. 실패/중복/준비 회복이 재검토 기회를 충전하지 않는다.
    if (State raw 존재 && State raw != DOMAIN_IF_OK) return State raw;
    if (State raw 존재 && State raw == DOMAIN_IF_OK && Fault raw 존재) return Fault raw;
    return DOMAIN_IF_NOT_AVAILABLE; // aggregate 보류이며 호출하지 않은 C2의 raw 반환이 아니다.
}
```

<a id="a2-domain-contract"></a>
## A2 생산·설치 profile·독립 수용

HEALTH가 같은 원 평가의 서비스/current/history와 시각·generation·sequence를 완전하게 보호하고 기존 INPUT 한 곳에서 11필드로 변환한다. 불완전 Snapshot은 REEVALUATE이며 READY/정상품질/mask0을 합성하지 않는다. INPUT은 정책을 재판정하지 않고 명시 대응·범위·원 시각을 검증한다. 실제 새 완료 평가만 `update_sequence`를 증가시키고 tick/재인계는 번호·기산점을 바꾸지 않는다. `last_fault_quality=3`은 code/occurrence/historyAGE 모두0; quality0은 code1..5와 원 occurrence1..UINT32_MAX를 가진다. `source_age_ms`는 원 평가, 이력 AGE는 최초 발생 기준으로 독립 누적하며 부재 이력0은 증가시키지 않는다.

| 대상 | 명시 투영·손실 방지 |
|---|---|
| State service | state0..3/availability0..2/flag0..1을 이름별 대응. meta는 원 update/누적 source AGE이며 완전 producer·동일 입력·freshness 근거 없이 OK 합성 금지 |
| State current/history | mask/code/occurrence/이력 AGE와 승인된 원 generation을 보존. history quality0/3을 전체 faultQuality와 동치로 쓰지 않음;65535는 포화 하한 |
| FaultInfo | 완전 current mask의 가장 작은 정의 code1..5를 현재 대표로 선택; category FUNCTION/state ACTIVE/quality OK. 공집합이면 code0/state INACTIVE. 위험 우선순위가 아니며 lastFaultCode를 현재 고장으로 대입하지 않음 |

FaultInfo에 없는 전체 mask/occurrence/AGE/generation은 보호한 원 State/M09에 남긴다. 같은 kind 여러 instance의 원 기록은 HEALTH 소유다. 반복 현재 요약을 새 고장 발생으로 소비하지 않는다.

문서의 기본 대상은 확장 C2 StateReport다. 참고 v0.5는 StateReport가 service/lastFaultCode/faultQuality 3필드이고 RxVssState는 service만 VSM에 전달한다. RxVssFault는 onVssFault 부재 시 NOT_AVAILABLE, 등록 시 callback 원 반환이며 선행 Core commit이 없는 observer-only 경계다. 설치 profile/header/source를 E-S05로 확인하기 전 참고 source를 실제 제품 지원으로 간주하지 않는다. 확인된 v0.5 service 한정 수용을 확장 이력/generation 전체 소비 성공으로 기록하지 않는다. 후보 C2 7값과 v0.5 6값(UNSUPPORTED 없음)을 혼용하지 않는다.

같은 K 재수신은 두 Rx 추가 호출0이다. 실제 수용된 State는 Fault 미등록/실패/불명으로 롤백하거나 재호출하지 않는다. CALLING/UNKNOWN_FAILED는 자동 재호출0이며 raw REJECTED/QUEUE_FULL/NOT_AVAILABLE만으로 무부작용 미접수를 추정하지 않는다. 미호출/명시 미접수만 기존 owner의 부분 후속·준비 변화·원 freshness/세대·무부작용 증거·미소모 최대1회 기회가 모두 있을 때 해당 경계를 재검토한다. 현재 처리/부분 미결 원 K는 기존 M09 1건과 bounded 진단에 보존하고 후속 보고 수용 전에 old 결과를 이전한다. 회수된 K는 high-water/fence로 다시 등록하지 않는다. old 비동기 작업의 후속 쓰기 차단이 미확인인 해당 VSS 소비만 HOLD하며 늦은 old 결과를 새 State/결과에 귀속하지 않는다.

State의 raw·수용·실제 VSM commit과 Fault의 raw·callback·consumer 결과는 독립이다. 미호출 Fault raw는 없음이고 aggregate NOT_AVAILABLE과 구별한다. v0.5 Fault를 [공통 Core→Observer classifier](../DOMAIN_RX_ADAPTER.md#outcome)에 넣기 위해 Core APPLIED를 만들지 않는다. Getter/TxPort의 같은 accepted 원 DTO/meta association은 값 일치로 추정하지 않으며 E-S05 미입증 투영만 제한한다.

E-S01 원기록 완전성/게시·용량, E-S02 reportGeneration↔CAN BOOT/수명, E-S03 clock/원 AGE·TTL/포화, E-S04 전체 복사와 terminal sink→INPUT/FLOW, E-S05 설치 C2/부분 결과/old K fence/Getter 연결은 실제 확인 대기다. Host·Board·실제 Binding은 NOT_RUN/BINDING_NOT_VERIFIED다.

## 원본·시간·보관·후발 실패

VSS 생산 세대+update_sequence는 서비스 평가, last_fault_occurrence는 최근 고장 이력이다. source_age_ms와 last_fault_age_ms는 독립이다. 이력 있음0의 AGE65535는 오래된 이력의 포화 하한이고, 확인된 부재3의 code/occurrence/이력 AGE0은 누적하지 않는다. 새 reportGeneration은 VSS 실제 기동이며 DOMAIN Boot로 대체하거나 CAN 단독 재시작으로 old 보고를 재라벨하지 않는다(E-S02).

통신 주기/수신 중단: 250 / 750ms 후보. VSS_EVENT의 무발생과 링크 상실을 구별한다. 이 수치는 채택·실측되지 않은 후보이며 `>`/`>=` 경계나 원 TTL을 임의 추가하지 않는다. 형식 오류·중복을 정상 heartbeat나 semantic fresh로 쓰지 않는다.

현재 상태는 동일 의미 key의 아직 송신을 시작하지 않은 항목만 교체 후보가 된다. 채널별 원 순서와 품질을 모두 보존해야 한다. Driver가 참조 중인 프레임은 AGE나TX를 수정하지 않는다. 저장 공간 부족·사본 준비 실패는 전체 새 접수 실패이며 순번 장부부터 진전시키지 않는다.

접수 OK, Driver accepted, 최종 TX, 참조 해제, 특정 상대 수신, 기능 완료는 각각 별도 사실이다. bus-off·옛 callback·중복 callback·미확인 결과 처리와 AGE 계산의 단일 절차는 [전송 공통](../CAN_TRANSFER_CONTROL.md), [전체 경로](../../flows/CAN_NETWORK_PATHS.md)를 따른다. 현재 품질 보고를 명령 실패 kind로 위조하지 않는다. identity 있는 discrete의 후발 실패는 원 항목에 연결하며 DOMAIN 송신은 D-03/D-05/O-C10 필수 sink가 준비되기 전 안정 접수 성공을 약속하지 않는다. 다른 ECU에 DomainIf 실패 API를 복제하지 않는다.

## 정상·경계·거부 사례

| 사례 | 기대 처리 |
|---|---|
| 정상 또는 조건부 경로 | fault_active0·fault_mask0이어도 최근 fault3/occurrence5/AGE65535/quality0은 이력으로 남는다. VSS PLAYING은 개별 Event 완료가 아니다. |
| 고유 경계 | fault_mask0x1F는 정의 비트,0x20은 예약이다. last_fault_quality1/2는 malformed다. NO_DATA3은 실제 기록없음과 함께 해석한다. |
| 의미 반례 | CAN link lost를 FAULT_MASK의 AUDIO_OUTPUT_FAILURE로 만들거나 기록품질2를0/3으로 축약하지 않는다. |
| ID/FDF/BRS/DLC/길이/padding 또는 raw 예약값 오류 | 임시 변환 전체 폐기, typed 호출·정상 수신 시각·순번 갱신 없음 |
| 같은 원 identity 재송신·AGE 감소 | 같은 소비시점으로 환산해 원본을 젊게 만들지 않음. 순서/clock/wrap 미확인이면 HOLD |
| Queue full·C2 미준비·소비 거부 | 호출 전 미접수는 해당 경계만 제한하고 기존 상태·Driver 참조 버퍼 불변. State 수용 뒤 Fault 실패/불명은 원 부분 결과를 보존하며 State 재호출0; 실제 각 raw와 aggregate를 구분 |
| accepted 뒤 Driver 실패·옛 callback·bus-off | 실제 관측을 원 제출과 연결하고 참조 해제 전 재사용 금지. 자동 재송신·기능 완료 생성 없음 |

근거: B07 can/VSS_STATUS.md 전문, S4A의 위 API/Data 절, 최신 S4C TIME_IDENTITY_CONTRACT/FRESHNESS_AGE_TIMEOUT/RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 출처 채택·팀 변경은 별도이며 본문에서 기존 공개 선언이나 Wire를 변경하지 않는다.
