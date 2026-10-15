# CIS_ENVIRONMENT — 세 센서의 독립 관측

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [Data/전체 필드](../../data/CAN_NODE_DATA.md#CIS_ENVIRONMENT) · [흐름](../../flows/CAN_CIS_WINDOW_VSS.md#CIS_ENVIRONMENT)

`WIRE-P01-CIS_ENVIRONMENT` / `RX-P01-CIS_ENVIRONMENT-DOMAIN`. CIS→DOMAIN, 후보 ID 0x300 / used40 / frame48 / DLC14. Wire는 다른 MCU 사이의 프레임 전달이며 원격 C 호출이 아니다.

## 책임과 호출 경계

CIS 기능 Owner가 원 의미를 만들고 Network가 M03 typed 인계→M04 보관·시간→M05 Codec→M07/M08 Driver를 맡는다. 수신 측은 역방향으로 검사하여 DOMAIN 로컬 소비자에게 전달한다. ISR은 안정된 장치 관측 확보까지만 맡으며 의미 API는 Task/소유자 문맥에서 호출한다. 실제 Task·Lock·queue 용량·Driver HAL 이름은 미검증이다.

<a id="api-CisNet_PublishEnvironment"></a>
```c
NetSubmitResult CisNet_PublishEnvironment(
    const NetCisEnvironment *message
);
```

출처: S4A api/cis.md#api-CisNet_PublishEnvironment.
<a id="api-DomainIf_RxCisCabinEnvironment"></a>
```c
DomainIf_Status_t DomainIf_RxCisCabinEnvironment( const CisCabinEnvironmentState_t *state);
```

출처: S4A api/domain_rx.md#api-DomainIf_RxCisCabinEnvironment.

송신 API 인자는 NULL 불가 IN 객체이며 호출 동안만 읽는다. 수신 callback의 message/meta 또는 state/status/result도 NULL 불가이고, 이후 필요하면 소비자가 반환 전에 복사한다. 공개 DomainIf 본체·Manager 상태는 DOMAIN 소유이고 Network가 중복 구현하지 않는다. NetSubmitResult/C2 반환의 전체 의미는 [기존 반환 정본](../../data/CIS_REAR_DATA.md#declared), 내부 함수 반환은 [공통 자료](../../data/CAN_COMMON_DATA.md)에 있다.

**실행에 필요한 미결:** CIS_QUALITY_PROJECTION, IF-O09, IF-O19, IF-O20, IF-O24: 온도·습도 원 TTL은 각각2500ms(>=비사용)로 합의되었다. 나머지 품질 대응·조도 TTL·VSM/독립 Meta의 실제 연결은 검증 대기다. 원 S4A CisCabinEnvironmentState_t에는 sourceGeneration 멤버가 없으므로 추가하지 않는다.

<a id="submit"></a>
## 송신 인계

`CisNet_PublishEnvironment` 본체는 송신 노드 Network M03/M04 소유다. 센서·실행·진단 Owner가 자기 Task/Superloop에서 제출한다. message는 NULL 불가 IN 사본이며 호출 중 불변이다. 다음 절차는 기존 선언에 대한 설계이며 제품 코드를 변경하지 않는다.

```c
NetSubmitResult CisNet_PublishEnvironment(     const NetCisEnvironment *message )
{
    if (message == NULL || 단일 필드표의 코드·범위·명시 조합이 잘못되면)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (초기화·원 기동·시간·profile·필요한 소비 경계가 준비되지 않았으면)
        return NET_SUBMIT_NOT_READY;
    // 온도·습도·조도 각각의 원 생산 세대, temperature/humidity/illuminance_sequence. CAN BOOT/TX는 외피 식별이다.
    // temperature_age_ms, humidity_age_ms, illuminance_age_ms를 독립 누적한다. 다른 채널 갱신이 원 시각을 재시작하지 않는다.
    if (원 관측·사건 순서 또는 같은 소비시점 AGE를 입증하지 못하면)
        return NET_SUBMIT_NOT_READY;
    if (전체 DTO와 원 근거를 함께 둘 안전한 저장공간이 없으면)
        return NET_SUBMIT_NO_CAPACITY;
    // 한 항목의 전체 값과 시각 기준을 준비한 뒤 대기 저장소에 함께 반영한다.
    // 기존 개별 명령·사건·결과 또는 Driver 참조 중 프레임을 덮어쓰지 않는다.
    return NET_SUBMIT_OK; // caller 포인터를 이후 참조하지 않는 로컬 접수다.
}
```

형식이 정상인 INVALID/STALE/NO_DATA/FAULT 보고는 그대로 보존한다. 호출 실패 시 새 자료의 부분 접수는 없고 이전 대기는 유지한다. 후발 Driver 실패는 과거 반환을 변경하지 않는 별도 관측이다.

<a id="codec"></a>
## 메시지 Codec: 내부 함수 후보

두 원형은 `PROPOSED_PRIVATE_FUNCTION`이다. 호출자는 CIS의 M04 송신 선택과 DOMAIN의 M07/M04 수신 처리다. 전체 공통 검사 순서는 [공통 Codec](../CAN_COMMON_CODEC.md#check-frame), 전체 필드 변환은 [단일 필드표](../../data/CAN_NODE_DATA.md#wire-CIS_ENVIRONMENT)에 있다. 이 함수는 시간원·Driver·기능 API를 호출하지 않는다. 실행은 Network 소유자 Task/Superloop이며 ISR에서 프레임 해석을 하지 않는다.

| 인자 | 방향·범위·소유·수명 |
|---|---|
| message | IN `NetCisEnvironment` 전체, NULL 불가. 송신 시점까지 AGE를 반영한 호출 내 불변 사본 |
| source | IN `NetCanSource`, NULL 불가. 확인한 로컬 Boot/TX; Codec는 새 번호 발급/소비를 하지 않음 |
| frame | IN `NetCanFrame`, NULL 불가. 실제 Driver flags/길이/수신시각과 바이트가 동일 입력인 안정 사본 |
| profile | IN 값. 후보 종류를 표시할 뿐 양단 채택 증거를 대신하지 않음 |
| out, meta | OUT caller 저장소, NULL 불가. 입력 및 서로와 겹치지 않음. 모든 검사 성공 뒤 함께 반영; 실패 시 불변 |

Encode `NET_CAN_OK`는 완전 프레임 작성, Decode `NET_CAN_OK`는 형식 변환만 뜻한다. INVALID_ARGUMENT은 NULL/alias 또는 송신 값 오류, MALFORMED는 수신 ID/flags/DLC/길이/padding/코드/명시 조합 오류, CONTRACT_HOLD는 형식·실행 계약 미확인, ORDER_HOLD는 송신 Boot/TX 전제 미확인이다. Decode 성공은 admission·신선도·C2 성공이 아니다.

```c
NetCanResult NetCan_EncodeCisEnvironment(const NetCisEnvironment *message,
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
    // 온도는 -4000..12500 또는 -32768, 습도는 0..10000 또는 65535, 조도는 0..200000 또는 0xFFFFFFFF다. sentinel+VALID는 모순이며 INVALID+해당 sentinel의 정상 열화 보고는 유지한다. 세 validity는 0/1, reason은 0..9다.
    NetCanFrame tmp = {0};
    tmp.canId = 0x300; tmp.dlc = 14;
    tmp.length = 48;
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
    // padding 영역(40..47)이 있으면 초기값0을 유지한다.
    // 전체 byte/flags/필드표 충족을 재확인한 뒤 한 번에 반영한다.
    *out = tmp; // Driver 호출·TX 순번 발급은 없다.
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeCisEnvironment(const NetCanFrame *frame,
    NetCanProfile profile, NetCisEnvironment *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    NetCanResult rc = NetCan_CheckFrame(frame, NET_CAN_CIS_ENVIRONMENT,
        NET_CAN_NODE_DOMAIN, profile);
    if (rc != NET_CAN_OK) return rc;
    NetCisEnvironment tmp;
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
    // 온도는 -4000..12500 또는 -32768, 습도는 0..10000 또는 65535, 조도는 0..200000 또는 0xFFFFFFFF다. sentinel+VALID는 모순이며 INVALID+해당 sentinel의 정상 열화 보고는 유지한다. 세 validity는 0/1, reason은 0..9다.
    tmpMeta.received_at_ms = frame->receivedAtMs;
    // 이 지점에서 tmp의 모든 멤버와 tmpMeta.source가 채워졌는지 확인한다.
    *out = tmp;
    *meta = tmpMeta; // caller 소유 임시 출력에 함께 반영; 외부 공개는 한 번이다.
    // 순번 장부·liveness·기능 상태는 여기서 갱신하지 않는다.
    return NET_CAN_OK;
}
```

<a id="receive"></a>
## typed 수신과 소비

아래 `PROPOSED_PRIVATE_FUNCTION`은 수신 Network M03의 투영·호출 절차다. 원 public `DomainIf_RxCisCabinEnvironment` 본체는 DOMAIN Owner 소유다. message/meta는 admission에 성공한 동일 입력의 IN 읽기 전용 쌍이고 NULL 불가다. caller는 원 수신시각·원 AGE를 보존하며 [공통 수신/시간 절차](../CAN_TRANSFER_CONTROL.md)로 소비시점의 미반영 경과를 정확히 한 번 계산한 소유자 문맥이다. 이 함수는 별도 현재 시각을 발급하지 않는다.

cabinTemperature/cabinHumidity/cabinIlluminance에 원 raw를 그대로 옮긴다. 각 ValueQuality_t.validity/reason은 이름으로 변환하고 meta.updateSequence와 meta.ageMs에는 같은 채널의 원 순번과 소비시점 AGE를 넣는다. meta.quality는 validity/reason 숫자 비교로 만들지 않는다. 현재 수용 CIS Boot·채널별 순서와 품질 대응을 확인한 뒤 전체 C2 객체를 전달한다. 새 Boot의 C2 순서 재기준 연결은 아래 별도 미완료 항목이다.

```c
NetCanResult NetCan_DeliverCisEnvironment(const NetCisEnvironment *message,
    const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || 같은 수신 사건의 쌍이 아니면)
        return NET_CAN_INVALID_ARGUMENT;
    if (입력 코드·명시 조합이 잘못되면) return NET_CAN_INVALID_ARGUMENT;
    if (수신 소유자 문맥·실제 소비자 지원·초기화가 준비되지 않았으면)
        return NET_CAN_NOT_READY;
    if (원 생산 세대·순서·identity 대응이 입증되지 않았으면)
        return NET_CAN_ORDER_HOLD;
    if (소비시점의 미반영 홉·수신 대기 또는 필요한 원 사용 한도를 입증하지 못하면)
        return NET_CAN_TIME_HOLD;
    if (본문의 필수 품질·enum·원본 투영 계약이 미합의이거나 정보가 손실되면)
        return NET_CAN_CONTRACT_HOLD;
    // cabinTemperature/cabinHumidity/cabinIlluminance에 원 raw를 그대로 옮긴다. 각 ValueQuality_t.validity/reason은 이름으로 변환하고 meta.updateSequence와 meta.ageMs에는 같은 채널의 원 순번과 소비시점 AGE를 넣는다. meta.quality는 validity/reason 숫자 비교로 만들지 않는다. 현재 수용 CIS Boot·채널별 순서와 품질 대응을 확인한 뒤 전체 C2 객체를 전달한다. 새 Boot의 C2 순서 재기준 연결은 아래 별도 미완료 항목이다.
    CisCabinEnvironmentState_t input;
    // 같은 실제 DTO/meta에서 원값·채널별 seq·소비 AGE와 원 품질을 전체 구성한다.
    for (temperatureQuality와 humidityQuality를 각각 검사한다) {
        if (해당 원 AGE와 미반영 경과를 더한 meta.ageMs >= 2500) {
            // 원값/원seq/원기준시각은 보존한다. 이미 INVALID인 원 사유를 지우지 않는다.
            if (현재 해당 값이 VALID이면) {
                해당 validity = VALUE_VALIDITY_INVALID;
                해당 reason = QUALITY_REASON_STALE;
                해당 meta.quality = DATA_QUALITY_STALE;
            }
        }
    }
    // 조도는 자기 품질/AGE를 보존하며 미합의1000ms를 여기서 확정하지 않는다.
    // 새 Boot의 낮은 seq를 이전 Boot의 큰 seq와 비교하거나 901로 재발급하지 않는다.
    if (meta의 Boot가 M01에 기억된 종료 Boot이면) return NET_CAN_ORDER_HOLD;
    if (수용 B의 첫 정상 인계인데 아래 r1 저장 순서의 기존 DOMAIN 연결 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD; // BG-R05-BOOT: 조건부 절차와 실제 근거는 다르다.
    if (수용 B가 구 쓰기 종료 또는 같은 전이 NO_DATA의 VSM·독립 Meta 저장을 기다리면)
        return NET_CAN_NOT_READY; // owner를 막지 않고 B 원사본/원시각을 보존한다.
    // 위 기다림의 실제 해소는 M09의 원 사건별 기존 DOMAIN 관측과 대조한다.
    // 이 입력에 미반영인 대기 AGE는 위 채널 검사에 한 번 포함한다. 2500 이상은 비사용.
    if (원 반환과 입력 식별·반영 단계를 보존할 기존 M09 관측을 사전 확보하지 못하면)
        return NET_CAN_NOT_READY;
    DomainIf_Status_t rc = DomainIf_RxCisCabinEnvironment(&input);
    M09의 확보한 기존 관측에 원 rc, 입력의 BOOT/TX와 본문에 정의한 의미 식별자,
        호출 대상 DomainIf_RxCisCabinEnvironment, 실제로 확인한 Core/선택 Observer 반영 단계를 보존한다;
    if (Core와 Observer 중 어느 단계까지 반영했는지 미확인이면)
        같은 입력의 재호출을 HOLD로 남긴다; // rc만으로 미접수 또는 rollback을 추정하지 않는다.
    if (rc == DOMAIN_IF_OK) return NET_CAN_OK;
    if (rc == DOMAIN_IF_QUEUE_FULL) return NET_CAN_NO_CAPACITY;
    if (rc == DOMAIN_IF_NOT_INITIALIZED || rc == DOMAIN_IF_NOT_AVAILABLE)
        return NET_CAN_NOT_READY;
    // 원 C2 반환을 보존하며 INVALID_ARGUMENT/REJECTED/UNSUPPORTED를 성공으로 바꾸지 않는다.
    return NET_CAN_CONSUMER_REJECTED;
}
```

OK 뒤 해당 인계를 다시 호출하지 않는다. 실패 시 원 사본은 보존하되 API의 미접수·부분 변경 여부가 확인되기 전 자동 재호출을 하지 않는다. 같은 원본의 순서·AGE를 다음 시도에서 다시 확인한다. 새로운 관측·권한·차량 결과를 합성하지 않는다.

## 원본·시간·보관·후발 실패

온도·습도·조도 각각의 원 생산 세대, temperature/humidity/illuminance_sequence. CAN BOOT/TX는 외피 식별이다. temperature_age_ms, humidity_age_ms, illuminance_age_ms를 독립 누적한다. 다른 채널 갱신이 원 시각을 재시작하지 않는다.

통신 주기/수신 중단은 200 / 600ms 후보를 유지한다. 온도·습도는 각각 원 AGE2500ms부터 비사용하며2499ms는 시간 필요조건만 충족한다. 조도1000ms와 CIS local5000ms는 별도 후보/미합의이며 DOMAIN의2500ms를 연장하지 않는다. 형식 오류·중복을 정상 heartbeat나 semantic fresh로 쓰지 않는다.

현재 상태는 동일 의미 key의 아직 송신을 시작하지 않은 항목만 교체 후보가 된다. 채널별 원 순서와 품질을 모두 보존해야 한다. Driver가 참조 중인 프레임은 AGE나TX를 수정하지 않는다. 저장 공간 부족·사본 준비 실패는 전체 새 접수 실패이며 순번 장부부터 진전시키지 않는다.

접수 OK, Driver accepted, 최종 TX, 참조 해제, 특정 상대 수신, 기능 완료는 각각 별도 사실이다. bus-off·옛 callback·중복 callback·미확인 결과 처리와 AGE 계산의 단일 절차는 [전송 공통](../CAN_TRANSFER_CONTROL.md), [전체 경로](../../flows/CAN_NETWORK_PATHS.md)를 따른다. 현재 품질 보고를 명령 실패 kind로 위조하지 않는다. identity 있는 discrete의 후발 실패는 원 항목에 연결하며 DOMAIN 송신은 D-03/D-05/O-C10 필수 sink가 준비되기 전 안정 접수 성공을 약속하지 않는다. 다른 ECU에 DomainIf 실패 API를 복제하지 않는다.

## 정상·경계·거부 사례

| 사례 | 기대 처리 |
|---|---|
| 정상 또는 조건부 경로 | 온도 -123raw는 -1.23°C, 습도65535+INVALID/SENSOR_FAULT와 조도70000lx를 함께 보존한다. 유효하지 않은 습도만으로 정상 온도·조도를 버리지 않는다. |
| 고유 경계 | 온도 -32768+VALID, 습도10001, 조도200001은 거부한다. 올바른 sentinel+INVALID는 malformed가 아니다. |
| 의미 반례 | 조도70000을 uint16으로 절삭하거나 습도 고장 때문에 온도를0°C로 바꾸면 거부 대상이다. |
| ID/FDF/BRS/DLC/길이/padding 또는 raw 예약값 오류 | 임시 변환 전체 폐기, typed 호출·정상 수신 시각·순번 갱신 없음 |
| 같은 원 identity 재송신·AGE 감소 | 같은 소비시점으로 환산해 원본을 젊게 만들지 않음. 순서/clock/wrap 미확인이면 HOLD |
| Queue full·C2 미준비·소비 거부 | 새 항목 전체 인계 실패. 기존 상태·Driver 참조 버퍼 불변, 반환 원인 보존 |
| accepted 뒤 Driver 실패·옛 callback·bus-off | 실제 관측을 원 제출과 연결하고 참조 해제 전 재사용 금지. 자동 재송신·기능 완료 생성 없음 |

근거: B07 can/CIS_ENVIRONMENT.md 전문, S4A의 위 API/Data 절, 최신 S4C TIME_IDENTITY_CONTRACT/FRESHNESS_AGE_TIMEOUT/RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 출처 채택·팀 변경은 별도이며 본문에서 기존 공개 선언이나 Wire를 변경하지 않는다.

<a id="local-invalidation"></a>
## PG6-SW01 — 새 프레임 없는 환경 무효화

```c
NetCanResult NetCan_InvalidateCisEnvironment(const NetCisEnvironmentLocalEvent *event);
```

이 `PROPOSED_PRIVATE_FUNCTION`은 M09의 기존 관측 단계가 M03/F08의 직렬 문맥으로 전달한 [사건 사본](../../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)을 읽는다. event는 NULL 불가·호출 중 불변이며 raw/meta를 새 수신으로 제출하지 않는다. M09가 만료 채널/수신중단/수용 Boot 전이의 실제 원자료와 원 기록을 함께 확보한다. Boot 전이는 아래 r1 순서의 구 쓰기 종료 조건을 실제로 충족한 뒤 한 번 호출한다. 호출 전 조건 대기와 호출 후 저장 확인을 구분하며, 실제 Task 공급·C2→VSM Binding은 미검증이다.

```c
NetCanResult NetCan_InvalidateCisEnvironment(const NetCisEnvironmentLocalEvent *event)
{
    if (event == NULL || event->affectedChannels == 0 ||
        (event->affectedChannels & ~7u) != 0) return NET_CAN_INVALID_ARGUMENT;
    if (!event->elapsedKnown) return NET_CAN_TIME_HOLD;
    if (event->elapsedSinceRxMs가 retainedMeta.received_at_ms에서 observedAtMs까지
        검증된 같은 로컬 clock의 실제 미반영 경과와 다르면)
        return NET_CAN_TIME_HOLD;
    if (recordRevision이 M01의 보존 원자료와 다르거나 같은 eventRevision이
        CALL_IN_PROGRESS/처리됨/결과불명 상태이면)
        return NET_CAN_ORDER_HOLD; // 늦은 timer로 새 측정을 무효화하지 않는다.
    if (원 raw/meta와 retainedC2의 값·각seq·AGE 기준 연결이 없으면)
        return NET_CAN_CONTRACT_HOLD;
    switch (event->kind) {
    case NET_ENV_ORIGIN_EXPIRED:
        if (mask에 조도가 있거나, 온습도 실제 계산 AGE>=2500인 채널 집합과 mask가 다르면)
            return NET_CAN_CONTRACT_HOLD;
        break;
    case NET_ENV_RX_STOPPED:
        if (!event->rxStopPolicyAdopted || event->rxStopLimitMs == 0 ||
            lastValidRxAtMs가 해당 CIS_ENVIRONMENT의 실제 정상 수신 원장과 다르거나,
            observedAtMs-lastValidRxAtMs가 채택된 rxStopLimitMs·경계에 미달하면)
            return NET_CAN_CONTRACT_HOLD;
        if (같은 Boot 일반 수신중단의 STALE/NO_DATA 선택·채널별 대응이 채택되지 않았으면)
            return NET_CAN_CONTRACT_HOLD; // Boot NO_DATA 제안으로 이 정책을 확정하지 않는다.
        break;
    case NET_ENV_BOOT_CHANGED:
        if (acceptedBootId가 기존 M01의 실제 수용 Boot와 다르거나
            acceptedBootId == retainedMeta.source.boot_id)
            return NET_CAN_ORDER_HOLD;
        if (이전 Boot에 속한 채널 집합과 mask가 다르면)
            return NET_CAN_ORDER_HOLD;
        if (M01의 같은 전이 기록에서 구 Boot 신규 전달·미실행 패킷/콜백 차단이 확인되지 않으면)
            return NET_CAN_ORDER_HOLD;
        if (기존 DOMAIN owner의 구 실행·후속 비동기 쓰기 전체를 연결한 근거가 없으면)
            return NET_CAN_CONTRACT_HOLD; // callback 반환/활성 수0만으로 대체하지 않는다.
        if (그 구 쓰기가 남았거나 이후 commit 불가의 실제 관측이 아직 없으면)
            return NET_CAN_NOT_READY; // 동기대기/임의취소 없이 원 사건과 B를 보존한다.
        break;
    default: return NET_CAN_INVALID_ARGUMENT;
    }
    if (M09의 원 사건·전체 인계 결과 공간 또는 기존 C2 소비 문맥이 준비되지 않았으면)
        return NET_CAN_NOT_READY; // 원 제한 사실은 보존하고 정상 사용을 HOLD한다.
    if (원 사건 이후 현재 인계까지의 로컬 clock 연속성과 미반영 대기를 입증하지 못하면)
        return NET_CAN_TIME_HOLD; // event의 과거 elapsedKnown으로 새 대기까지 참이라 하지 않는다.
    CisCabinEnvironmentState_t input = event->retainedC2;
    for (온도·습도·조도의 세 채널 각각) {
        // 원 raw AGE + 확인된 미반영 경과를 포화 누적; 원 seq와 원값은 그대로다.
        해당 meta.ageMs = 해당 원 raw AGE부터 현재 M03 인계 시점까지 계산한 현재 AGE;
        // event.observedAtMs는 원 사건 시각이다. 이후 대기는 기존 M09의 실제 로컬
        // 시각 관측으로 한 번만 추가하며 event 원시각/retainedMeta를 고쳐 쓰지 않는다.
        if (현재 채널이 온도 또는 습도이고 현재 meta.ageMs >= 2500이며 VALID이고,
            현재 사건이 해당 채널의 RX_STOPPED 품질 선택을 적용하는 경우가 아니면) {
            해당 validity = VALUE_VALIDITY_INVALID;
            해당 reason = QUALITY_REASON_STALE;
            해당 meta.quality = DATA_QUALITY_STALE;
            // RX_STOPPED 대상 채널은 아래에서 채택된 비사용 품질만 적용한다.
            // 원 사건 mask 밖에서 대기 중 만료된 형제 채널도 정상으로 넘기지 않는다.
            // 실제 현재 시간/채널 근거는 같은 M09 인계 입력 관측에 함께 보존한다.
        }
        if (affectedChannels에 포함되지 않으면) continue;
        if (event->kind == NET_ENV_BOOT_CHANGED) {
            해당 validity = VALUE_VALIDITY_INVALID;
            해당 reason = QUALITY_REASON_NO_DATA;
            해당 meta.quality = DATA_QUALITY_NO_DATA;
        } else if (해당 현재 품질이 정상 사용 가능한 VALID이면) {
            해당 validity = VALUE_VALIDITY_INVALID;
            if (event->kind == NET_ENV_ORIGIN_EXPIRED) {
                해당 reason = QUALITY_REASON_STALE;
                해당 meta.quality = DATA_QUALITY_STALE;
            } else {
                // 일반 수신중단은 앞서 확인한 기존 채택 근거의 STALE 또는 NO_DATA만
                // 해당 reason/meta.quality에 이름 대응한다. 기본값을 새로 확정하지 않는다.
            }
        }
        // 만료/중단에서 이미 INVALID/NO_DATA이면 그 사유를 유지한다.
        // Boot 전이는 현재 표시를 NO_DATA로 제한하며 원 사유는 retainedRaw에 보존한다.
    }
    // 원 raw/meta와 현재 제한 input을 분리 보관하고 정상 사용 금지를 먼저 유지한다.
    기존 M09 사건 보관 항목의 eventRevision을 CALL_IN_PROGRESS로 원자 표시한다;
    // 같은 직렬 소유 경계에서 표시 후 호출한다. 재진입은 위 검사로 HOLD한다.
    DomainIf_Status_t rc = DomainIf_RxCisCabinEnvironment(&input);
    M09에 eventRevision, recordRevision, 원 Boot/seq, input, 원 rc와 실제 Core 단계 기록;
    // CALL_IN_PROGRESS는 호출 시도 사실이다. 반환 후에도 VSM·독립 Meta의 동일
    // 무효화 저장이 확인되기 전 B 정상 gate를 열지 않는다. Queue 접수/OK도 같다.
    if (Core 결과가 불명확하거나 일부만 반영되었으면) 동일 사건 재호출 HOLD;
    if (Core 무효화가 실제 끝났고 optional Observer만 실패했으면)
        그 완료와 Observer 원 실패를 따로 보존한다; // commit을 다시 실행하지 않는다.
    // 늦은 실제 저장 관측은 기존 M09 항목에 결합하고 M03의 B gate만 재검토한다.
    // 완료 확인을 위해 이 함수를 재호출하거나 새 공개 callback을 만들지 않는다.
    if (rc == DOMAIN_IF_OK) return NET_CAN_OK; // 호출 경계 성공만; VSM 무효화 판정 아님.
    if (rc == DOMAIN_IF_QUEUE_FULL) return NET_CAN_NO_CAPACITY;
    if (rc == DOMAIN_IF_NOT_INITIALIZED || rc == DOMAIN_IF_NOT_AVAILABLE)
        return NET_CAN_NOT_READY;
    return NET_CAN_CONSUMER_REJECTED;
}
```

원 표의 실제 유효성 enum은 `VALUE_VALIDITY_INVALID`이다. 위 한국어 채널 대입은 `temperatureQuality`, `humidityQuality`, `illuminanceQuality`의 각 `validity/reason/meta.quality/meta.ageMs`에만 적용한다. 새 generation·measurement seq·CAN TX·가짜 received_at_ms를 만들지 않는다. 단순 C2 OK 뒤에도 VSM/독립 Meta/Getter가 구 GOOD를 반환하면 무효화 검증은 실패 또는 미완료다. Getter↔원 raw↔event의 연결 없이는 UART projection을 HOLD한다. 온도만 새로 측정되면 해당 채널만 회복하고 나머지 NO_DATA/STALE은 유지한다.

<a id="environment-storage-order"></a>
## PG6-SW02 — r1의 조건부 환경 저장 순서

고정 r1의 [저장 순서](../../../interface/07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)와 [환경 Rx 완료 경계](../../../interface/02_NODES/DOMAIN/20_API.md#delta-environment-completion)를 인수한다. 이는 **조건부 권장 설계 / DOMAIN 저장 경계 미확정**이다. 아래 단계는 기존 M01/ST01 원본 연결과 M09/ST12 관측을 구체화한 것이며 새 전역 Manager·완료 API·receipt·완료 bool을 선언하지 않는다.

| 순서·직접 caller | 실제 근거와 동작 | 미충족 시 보존·반환 |
|---|---|---|
| 1. M01/M04가 B 수용 | 검증된 Boot 전이와 A의 원 recordRevision을 결합한다. A의 신규 전달과 미실행 패킷·대기 콜백을 기존 소유 경계에서 차단/폐기하고 B는 별도 원 DTO/meta 사본으로 보관 | 처음 본 임의 Boot가 최신이라는 가정 금지. 실행 중 객체·Driver 참조는 폐기/수정하지 않음 |
| 2. M03/F08·DOMAIN 기존 owner 연결 | A의 실행 중 callback뿐 아니라 그 호출로 시작된 모든 후속 비동기 VSM/독립 Meta 쓰기의 종료 또는 이후 commit 불가를 동일 입력·실제 저장 경계로 확인 | callback 반환/Queue가 비었다는 관측만으로 통과 금지. 실제 연결 자체가 없으면 CONTRACT_HOLD, 알려진 작업이 진행 중이면 NOT_READY. owner를 동기대기로 막거나 임의취소하지 않음 |
| 3. M09→기존 무효화 함수 | 1·2를 충족한 같은 eventRevision의 A값·원seq를 NO_DATA로 제한하여 기존 환경 C2에 한 번 인계. CALL_IN_PROGRESS를 호출 전에 보존 | 이후 A 쓰기가 끼어들 수 있으면 호출 금지. 일반 NO_DATA를 Boot/seq reset 명령으로 해석하지 않음 |
| 4. DOMAIN 실제 저장→M09 관측 | 그 NO_DATA 입력의 VSM 구 정상값과 독립 Meta 무효화가 실제 commit되었고 A의 후속 쓰기가 이를 되돌릴 수 없음을 확인 | rc/Queue 접수/callback 반환/TX local OK는 완료 증거가 아님. 거부·불명·부분 저장이면 B 정상 차단과 구 OK 실제 제거 여부를 따로 기록; 구 OK가 제거됐다고 주장하지 않음 |
| 5. M03→기존 정상 인입 | 같은 전이에 대한 4의 실제 근거 이후 B의 `(Boot, 센서, 원seq)`를 그대로 전달. B0을 A900과 전역비교하거나901로 재번호하지 않음 | 대기 원사본의 미반영 AGE를 한 번 누적. 채널별2499/2500/2501을 재검사하고2500 이상은 정상복원하지 않음. 한 채널 회복이 다른 채널을 복구하지 않음 |
| 6. M01/M04의 지연 A 차단 | 기억된 종료 A frame/대기 callback의 신규 인계와 재저장을 막는다. 완료 근거도 같은 전이·원 record에만 결합 | Boot 재사용/Network 재시작/종료이력 상실이면 영구 차단을 보장하지 않음. 재동기화 근거를 확인할 때까지 영향 경로 HOLD |

이 순서는 단일 함수에서 끝날 때까지 기다리는 루프가 아니다. M03는 실제 진행 관측이 없는 시점에는 해당 호출을 반환한다. M09에 이미 보관한 원 사건·각 호출 원 반환·확인된 저장 단계는 남기고, 실제 기존 DOMAIN 관측이 도착한 다음 owner 처리 기회에 B gate를 재검토한다. 근거를 제공하는 기존 DOMAIN 위치·수명·동기화가 아직 없으므로 재개가 구현되었다고 주장하지 않는다. 완료 확인은 무효화 재호출을 뜻하지 않는다.

Core commit이 실제 끝난 뒤 optional Observer 실패만 남은 경우는 직접 저장 실패와 다르다. 같은 commit을 반복하지 않으며 독립 Meta까지의 완료와 구 쓰기 배제가 실제로 확인되었는지 따로 판단한다. 일반 같은 Boot의 NO_DATA는 reset이 아니고, 일반 수신중단의 STALE/NO_DATA 선택은 별도 미결이다.

<a id="boot-binding-gap"></a>
### BG-R05-BOOT — r1 조건부 순서는 존재, 실제 DOMAIN 저장 경계 근거 부족

`BLOCKED_DESIGN`을 유지한다. 기존 `CisCabinEnvironmentState_t` 여섯 멤버와 `DomainIf_RxCisCabinEnvironment(const ... *state)`에는 공개 Boot/generation·완료 입력이 없다. 과거 직접 합의는 VSM·독립 Meta·Sequence 재시작의 후속 확인을 남겼고, 이제 r1에는 위 여섯 단계의 **조건부 권장안**이 존재한다. 따라서 현재 상태를 단순한 원문 부재로 설명하지 않는다.

남은 근거는 기존 DOMAIN의 실제 Rx→Manager/VSM/독립 Meta→Getter 경로에서 A 실행/후속 비동기 쓰기를 빠짐없이 배제하는 경계, 같은seq NO_DATA의 실제 commit 확인, 그 뒤 B의 원 낮은seq를 수용하는 순서 보존과 같은 원 사건을 연결한 Getter 관측이다. 기존 코드 위치·실제 관측·소유/수명 근거가 제공되지 않았으며 Network가 완료 bool을 참으로 채워 대체하지 않는다. r1 게시와 C2 OK만으로 이 차단을 BINDING_ONLY/RESOLVED/PASS로 낮추지 않는다.

무효화 거부·완료 불명에서는 B 정상 전달을 차단하되 A의 GOOD가 이미 지워졌다고 기록하지 않는다. 구 A/seq901을 기억된 종료 Boot 범위에서 거부하며 B0은0 그대로 보존한다. 실제 기존 연결로 순서를 증명할 수 없다는 근거가 나올 때만 최소 Environment Generation/완료 경계 계약 변경을 별도 검토한다. 이번에는 공개 타입/API/Wire/Header를 변경하지 않는다. 기존 IM-026/027/028·RV-R05-03/07과 Owner/S6Q 연결은 하나의 같은 blocker로 유지한다.
