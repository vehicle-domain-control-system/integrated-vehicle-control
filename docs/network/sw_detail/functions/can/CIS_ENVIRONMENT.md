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
    if (수용 B의 첫 정상 인계인데 아래 동기 private 전환의 실제 DOMAIN 연결 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD; // BG-R05-BOOT: 제안·담당자 보고는 실제 Binding 증거가 아니다.
    if (BOOT 전이의 영향을 받는 B 입력이고 현재 요청 내용·수용 BOOT·적용 Epoch·명확한 private 완료의 대조가 미완료이면)
        return NET_CAN_NOT_READY; // 환경 B만 차단하고 원사본/원시각을 보존한다.
    // DOMAIN의 후속 commit 차단·무효화·Query 구 GOOD 방지 완료가 필요하다.
    // 구 비동기 객체가 남아 있다는 사실만으로 미완료로 보지 않는다.
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

이 `PROPOSED_PRIVATE_FUNCTION`은 M09의 기존 관측 단계가 M03/F08의 직렬 문맥으로 전달한 [사건 사본](../../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)을 읽는다. event는 NULL 불가·호출 중 불변이며 raw/meta를 새 수신으로 제출하지 않는다. M09가 만료 채널/수신중단/검증된 Boot 전이의 실제 원자료와 원 기록을 함께 확보한다. ORIGIN_EXPIRED/RX_STOPPED만 기존 공개 환경 Rx 품질 경로로 인계한다. BOOT_CHANGED에서는 Network 자체 환경 gate를 유지한 채 DOMAIN 소유 단일 동기 private 전환을 목표로 하며 전환용 Rx NO_DATA를 중복 호출하지 않는다. DOMAIN Production Private API 선언·결과 Receipt 및 첫 Epoch 1/연속 N+1 처리 코드는 확인됐으나 Network의 실제 호출·초기 정합·반환 대조·환경 VSM/Query 연동은 미검증이므로 아래 BOOT 분기는 CONTRACT_HOLD다. M01은 새 BOOT 검증 시 자체 전달을 먼저 닫고, 사건·시간·binding 검사 실패로 일찍 반환해도 이를 해제하지 않는다. 실제 Task 공급·C2→VSM/Query Binding은 미검증이다.

```c
/* PSEUDOCODE_ONLY: 기존 Network 내부 후보의 분기 설명. DOMAIN private C 원형이 아니다. */
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
            return NET_CAN_CONTRACT_HOLD; // BOOT private 경로로 일반 단절 품질을 확정하지 않는다.
        break;
    case NET_ENV_BOOT_CHANGED:
        if (acceptedBootId가 기존 M01의 실제 수용 Boot와 다르거나
            acceptedBootId == retainedMeta.source.boot_id)
            return NET_CAN_ORDER_HOLD;
        if (이전 Boot에 속한 채널 집합과 mask가 다르면)
            return NET_CAN_ORDER_HOLD;
        if (M01의 같은 전이 기록에서 구 환경 신규 전달·미실행 패킷/콜백·미시작 Query 재게시 차단이 확인되지 않으면)
            return NET_CAN_ORDER_HOLD;
        if (실행 중 Network callback과 전환 요청의 기존 owner 직렬 문맥이 미확인이면)
            return NET_CAN_NOT_READY;
        // DOMAIN Private 원형/Receipt ABI는 Production에 존재. Network 실제 Binding은 미확인.
        // Core는 첫 Epoch1, 완료 후 N+1과 STALE/ORDER_ERROR를 구현. 실제 Receipt 없이 성공으로 간주 금지.
        // 향후 Binding 목표(실제 C 호출/결과 필드를 여기서 발명하지 않음):
        // - 기존 record/event와 원 old/new BOOT·요청 내용·제안 Epoch를 같은 전환으로 고정.
        // - CALL_IN_PROGRESS는 기존 eventRevision의 시도 기록일 뿐, 완료가 아니다.
        // - 고정 요청을 DOMAIN 소유 동기 private 경계에 한 번 제출.
        // - DOMAIN이 인계된 A 작업의 미래 commit을 봉쇄하고 VSM/Meta·순서 문맥을
        //   무효화하여 Query에서 구 GOOD를 재노출하지 않는 상태를 동기 반환해야 한다.
        // - Network가 현재 요청 내용/수용 BOOT/적용 Epoch/명확한 완료를 대조.
        //   거부·불명·부분 적용·불일치는 환경 B 차단/원 사실·오류 보존, 자동 재호출 없음.
        // - 확인된 완료 뒤에도 B의 원 seq/누적 AGE/품질을 다시 검사하여 기존 정상 Rx로 인계.
        // 구 작업 객체의 물리적 소멸이나 DOMAIN 내부 Queue drain을 Network가 요구하지 않는다.
        // 후속 commit 봉쇄와 무효화/Query/완료가 확인되면 잔존 작업이 있어도 조건 충족 가능.
        // BOOT는 아래 일반 품질용 공개 Rx로 내려가지 않는다. 새 timeout/enum/Token 없음.
        return NET_CAN_CONTRACT_HOLD; // Network→DOMAIN Private 호출/완료 대조 미검증. B gate는 열지 않는다.
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
        if (해당 현재 품질이 정상 사용 가능한 VALID이면) {
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
        // BOOT_CHANGED는 위에서 종료하므로 전환용 NO_DATA를 여기서 만들지 않는다.
    }
    // 원 raw/meta와 현재 제한 input을 분리 보관하고 정상 사용 금지를 먼저 유지한다.
    기존 M09 사건 보관 항목의 eventRevision을 CALL_IN_PROGRESS로 원자 표시한다;
    // 같은 직렬 소유 경계에서 표시 후 호출한다. 재진입은 위 검사로 HOLD한다.
    DomainIf_Status_t rc = DomainIf_RxCisCabinEnvironment(&input);
    M09에 eventRevision, recordRevision, 원 Boot/seq, input, 원 rc와 실제 Core 단계 기록;
    // CALL_IN_PROGRESS는 일반 만료/중단 Rx 시도 사실이다. C2 OK는 실제 저장 증거가 아니다.
    if (Core 결과가 불명확하거나 일부만 반영되었으면) 동일 사건 재호출 HOLD;
    if (Core 무효화가 실제 끝났고 optional Observer만 실패했으면)
        그 완료와 Observer 원 실패를 따로 보존한다; // commit을 다시 실행하지 않는다.
    // 같은 원 제한의 실제 저장/Getter 관측은 기존 M09 항목에 결합한다.
    // 이 일반 Rx 결과로 BOOT private 완료나 B gate 승인을 만들지 않는다.
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

최종 IF-A-R의 [저장 순서](../../../interface/07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)와 [환경 Rx 완료 경계](../../../interface/02_NODES/DOMAIN/20_API.md#delta-environment-completion)를 인수한다. BOOT 무효화는 **DOMAIN 소유 단일 동기 private 경계**이고 공개 Rx NO_DATA를 중복 호출하지 않는다. 아래는 실제 private 선언·결과 ABI와 Rx→VSM→Query Binding이 확보된 뒤의 목표다. 현재 의사코드의 BOOT 경로는 CONTRACT_HOLD이며 새 함수 원형·결과 flag·Token·완료 bool·manager를 만들지 않는다.

| 순서·직접 caller | 실제 근거와 목표 동작 | 미충족 시 보존·반환 |
|---|---|---|
| 1. M01/M04가 B 전이 검증 | 원 출처·형식·수용 세대·원 기록을 검증한다. CRC/처음 본 BOOT만으로 최신 승인 금지. 자기 소유 A 환경 대기·미실행 callback·미시작 Query 응답 재게시를 차단하고 B 원 DTO/meta를 보존 | 실행 중 Network callback은 기존 owner 문맥으로 직렬화. 다른 노드 Queue/Query 전체 차단이나 Driver 참조 frame 취소 없음 |
| 2. M03/F08가 요청 고정 | 같은 recordRevision/eventRevision과 원 old/new BOOT·내용·제안 Epoch의 연결을 고정하고 기존 M09 시도/결과 공간을 확보 | Private 선언/ABI는 확인됐지만 Network 바인딩·양쪽 초기 정합·호출 수명·Receipt/Query 완료 대조가 없으면 CONTRACT_HOLD. 한쪽 상태를 임의 초기화하지 않음 |
| 3. DOMAIN 소유 동기 private 경계 | M03/F08가 고정 요청을 한 번 제출. DOMAIN이 이미 인계된 A 작업의 후속 commit을 막고 환경 VSM/독립 Meta·관련 순서 문맥을 무효화하여 Getter/Query 구 GOOD 재노출을 방지 | Network는 DOMAIN 내부 Queue를 직접 drain하거나 VSM을 수정하지 않음. 구 작업 객체가 남아도 후속 commit이 확실히 봉쇄되고 나머지 완료 조건이 충족되면 가능 |
| 4. M03/F08·M09 완료 대조 | 실제 결과의 현재 요청 내용·수용 BOOT·적용 Epoch·명확한 완료를 대조해 원 사실과 함께 기록. 일반 C2 OK/Queue 접수/단순 callback 반환과 구분 | 확정 거부·부분 적용·불명·모순·불일치는 관련 환경 B 차단·오류 보고. 원 요청/결과를 보존하며 자동 재호출·rollback·다음 Epoch 발급 없음. Network gate가 A GOOD 제거 증거는 아님 |
| 5. M03→기존 공개 환경 Rx | 일치한 private 완료 뒤 B의 원 출처·Sequence·AGE·품질·수신 순서를 재검증해 실제 새 관측을 전달. A900→B0은0 그대로이며901로 재발급하지 않음 | 미반영 대기는 원 생성 기준에서 한 번 누적. 온습도2499ms도 품질 검증 필요,2500/2501ms 비사용. 새 온도만 습도/조도를 회복시키지 않음. 조도1000ms 후보 유지 |
| 6. M01/M02/M04와 DOMAIN Query | 기억된 종료 A의 지연 frame/callback을 전달·정상수신 갱신·복구 근거로 쓰지 않음. DOMAIN private 완료의 구 GOOD 차단과 B 공개 Rx 실제 반영/동일 원 입력 Getter 연결을 각각 확인 | 구 GOOD 잔존/association 부재는 환경 항목만 미제공·기존 Query 부분 처리. 미시작 A 환경 응답 재게시 금지. 이미 Driver가 참조하는 frame이나 과거 원격 전달을 소급 무효화하지 않음 |

물리적으로 구 비동기 작업이 남았다는 이유만으로 전부 소멸할 때까지 무한 대기하지 않는다. 필요한 것은 DOMAIN이 보장하는 미래 commit 불가·무효화·Query 구 GOOD 방지와 실제 동기 결과의 대조다. 그 근거가 불명확하면 B 차단을 유지한다. ISR 호출이나 필요한 DOMAIN 실행을 막는 lock/Queue를 점유한 동기 대기는 금지하며 새 timeout 수치를 정하지 않는다. 현재 실제 연결이 없는 상태에서 나중 callback이 올 것이라고 가정해 성공을 만들지 않는다.

적용 완료와 optional Observer/부가 진단 오류는 별개다. 명확한 완료 뒤 부가 오류로 재무효화하지 않고, 모순/불명은 원 사실을 보존하여 HOLD한다. 일반 같은 BOOT의 센서 NO_DATA·원 만료·수신중단은 기존 공개 Rx·시간·품질 경로이며 private 전환/Epoch 사건이 아니다. RX_STOPPED의 STALE/NO_DATA 선택은 별도 POLICY_OPEN이다.

<a id="sw-b-epoch-proposal"></a>
### SW-B — DOMAIN Core 순차 Epoch 구현과 실제 Network 연결 한계

**DOMAIN_CORE_SOURCE_VERIFIED / NETWORK_BINDING_NOT_VERIFIED:** `develop`의 `domain_cis_environment_private.h/.c`는 최초 Epoch 1, 완료된 Epoch 뒤 N+1, 동일 완료 요청 `DUPLICATE_APPLIED`, 충돌 `ID_CONFLICT`, 과거 `STALE`, 건너뜀 `ORDER_ERROR`, 실패 차단을 구현한다. 레거시 `EPOCH_ORDER_UNAVAILABLE` enum은 순차 계약에서 반환하지 않는다. Host 시험 PASS는 DOMAIN 담당자의 보고이며 여기서 독립 재실행하지 않았다. **Network의 실제 Private 호출·완료 Receipt 대조·환경 VSM/Query 정합은 미검증**으로 해당 환경 경로의 HOLD는 유지한다.

제안 범위는 DOMAIN과 Network가 정상 실행을 계속하고 초기 상태가 서로 맞는 동안의 CIS 환경 연속 전환이다. 양쪽 적용 상태·진행 요청·구 작업을 정합한 명시적 초기화 뒤 첫 Epoch1, Network가 직전 완료를 현재 요청과 대조한 뒤 검증된 새 BOOT에 정확히 다음+1을 한 번 발급한다. 같은 BOOT의 센서 갱신·일반 NO_DATA·CAN 반복은 발급 사건이 아니다. 미완료 중 선발급·동시 승인·자동 다음 요청을 하지 않는다. 실행 중 감소/reset/wrap 재사용 금지, 현재 DOMAIN Private 타입은 `uint64_t`이고 Epoch0은 INVALID_ARGUMENT, UINT64_MAX에서 다음 Epoch로의 증가를 허용하지 않는다. 이를 새 공개 필드·Wire·영속 정보로 승격하지 않는다.

| 제안 요청 판정 | 목표 / 현재 한계 |
|---|---|
| 정합 초기 상태의 첫1 / 직전 완료 후 정확히 다음+1 | Private Core는 첫 1/완료 후 N+1 순차 적용을 지원. Network 호출·초기 정합·Receipt/Query Binding 미검증으로 해당 환경 경로 HOLD |
| 현재 마지막 적용 Epoch·동일 완료 내용 | 기존 완료 결과만 재사용, 이후 B 정상값을 다시 지우지 않음. 진행 중·실패·불명은 완료된 중복이 아니며 자동 재호출하지 않음 |
| 같은 Epoch·다른 내용 / 과거 Epoch / 건너뜀 | 충돌·과거·strict-next 위반 거부 목표. 과거에 완료한 요청도 현재 gate 승인으로 사용하지 않음 |
| 진행 중 새 BOOT / 결과 내용·Epoch 불일치 | 새 선발급·무한 Queue 없음. 관련 환경 차단·오류/원 사실 보존 |

요청 동일성은 원 old/new BOOT·현재 전환 내용·적용 Epoch의 실제 private 근거로 확인한다. `transitionEpoch`는 BOOT 진위 증거·센서 원 Sequence·CAN TX·eventRevision/recordRevision이 아니며 기존 struct에 추가하지 않는다. 유일성은 양쪽 실행·초기 정합 구간에 한한다. Network 단독 재시작 자동 복구·영속 Epoch·복구 handshake 제외는 이번 발송된 지원 범위 제안이고 최종 양단 수용은 대기다. 오류/단독 재시작·수명 정합 상실에서는 환경을 HOLD하고 구 작업·양쪽 상태를 맞추는 명시적 초기화의 실제 호출 근거 전 재개하지 않는다. 다른 노드 BOOT·ESP32 BLE 세션에 이 Epoch를 자동 적용하지 않는다. CIS BOOT_ID 생성·재사용 방지 조건은 CIS 확인 사항이며 ID 재사용/기억 상실/구분 불가를 Epoch로 해결한다고 주장하지 않는다.

<a id="boot-binding-gap"></a>
### BG-R05-BOOT — r1 조건부 순서는 존재, 실제 DOMAIN 저장 경계 근거 부족

환경 BOOT 전환의 `BLOCKED_DESIGN`/국소 HOLD는 **Network–DOMAIN 실제 Binding 미검증**으로 유지한다. Private Core의 순서 검사는 구현되어 있다. 기존 `CisCabinEnvironmentState_t` 여섯 멤버와 `DomainIf_RxCisCabinEnvironment(const ... *state)`에는 공개 Boot/generation·완료 입력이 없다. 최종 IF-A-R에는 단일 동기 private 방향과 조건부 연속 전환 목표가 있으며 담당자 부분 Core/Host 완료 보고도 있다. 문서 근거가 없거나 전부 구현 전이라고 기록하지 않는다.

DOMAIN Private 원형·Receipt ABI와 Core의 첫 1/완료 후 N+1 처리 자체는 Production 소스로 확인됐다. 남은 것은 Network가 이 Private 함수를 실제 직렬 문맥에서 호출해 반환·BOOT·Epoch·Barrier Revision을 대조하는지, 양쪽 초기 정합·수명을 보장하는지, DOMAIN 구 쓰기 후속 commit 봉쇄와 VSM/독립 Meta/Query 구 GOOD 차단이 실제로 완결되는지, 이후 B의 원 낮은 seq가 기존 공개 환경 Rx·Getter에 같은 입력으로 연결되는지에 대한 증거다. 레거시 `EPOCH_ORDER_UNAVAILABLE`는 enum 호환용으로 남지만 현재 순차 Epoch Core의 반환 상태가 아니다. Rx→VSM→Query `BINDING_NOT_VERIFIED`는 그대로다. Network가 완료 bool·C2 OK·빈 Queue로 대신하지 않으며 IF 문서 PASS만으로 BINDING_ONLY/RESOLVED/PASS로 낮추지 않는다.

무효화 거부·완료 불명에서는 B 정상 전달을 차단하되 A의 GOOD가 이미 지워졌다고 기록하지 않는다. 구 A/seq901을 기억된 종료 Boot 범위에서 거부하며 B0은0 그대로 보존한다. 실제 기존 연결로 순서를 증명할 수 없다는 근거가 나올 때만 최소 Environment Generation/완료 경계 계약 변경을 별도 검토한다. 이번에는 공개 타입/API/Wire/Header를 변경하지 않는다. 기존 IM-026/027/028·RV-R05-03/07과 Owner/S6Q 연결은 하나의 같은 blocker로 유지한다.
