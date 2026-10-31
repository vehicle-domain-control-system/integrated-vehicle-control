# WINDOW_RESULT — 명령 결과와 원 명령 귀속

[Network](../../00_START_HERE.md) · [M03](../../modules/M03_TYPED_HANDOFF.md) · [M05](../../modules/M05_CAN_CODEC.md) · [Data/전체 필드](../../data/CAN_NODE_DATA.md#WINDOW_RESULT) · [흐름](../../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_RESULT)

`WIRE-P01-WINDOW_RESULT` / `RX-P01-WINDOW_RESULT-DOMAIN`. WINDOW→DOMAIN, 후보 ID 0x092 / used20 / frame20 / DLC11. Wire는 다른 MCU 사이의 프레임 전달이며 원격 C 호출이 아니다.

## 책임과 호출 경계

WINDOW 기능 Owner가 원 의미를 만들고 Network가 M03 typed 인계→M04 보관·시간→M05 Codec→M07/M08 Driver를 맡는다. 수신 측은 역방향으로 검사하여 DOMAIN 로컬 소비자에게 전달한다. ISR은 안정된 장치 관측 확보까지만 맡으며 의미 API는 Task/소유자 문맥에서 호출한다. 실제 Task·Lock·queue 용량·Driver HAL 이름은 미검증이다.

<a id="api-WindowNet_SendResult"></a>
```c
NetSubmitResult WindowNet_SendResult(const NetWindowResult *message);
```

출처: S4A api/window.md#api-WindowNet_SendResult.
<a id="api-DomainIf_RxWindowCommandResult"></a>
```c
DomainIf_Status_t DomainIf_RxWindowCommandResult( const DomainIf_WindowCommandResult_t *result);
```

출처: S4A api/domain_rx.md#api-DomainIf_RxWindowCommandResult.

송신 API 인자는 NULL 불가 IN 객체이며 호출 동안만 읽는다. 수신 callback의 message/meta 또는 state/status/result도 NULL 불가이고, 이후 필요하면 소비자가 반환 전에 복사한다. 공개 DomainIf 본체·Manager 상태는 DOMAIN 소유이고 Network가 중복 구현하지 않는다. NetSubmitResult/C2 반환의 전체 의미는 [기존 반환 정본](../../data/CIS_REAR_DATA.md#declared), 내부 함수 반환은 [공통 자료](../../data/CAN_COMMON_DATA.md)에 있다.

**실행에 필요한 미결:** WINDOW_EXECUTION_HOLD, IF-O02, IF-O07, IF-O24, CAN_RESULT_NOT_PRESENT_CONTEXT, G05: REQUEST_SEQUENCE와 commandId 대응·결과 meta 생성 근거가 미합의다.

<a id="submit"></a>
## 송신 인계

`WindowNet_SendResult` 본체는 송신 노드 Network M03/M04 소유다. 센서·실행·진단 Owner가 자기 Task/Superloop에서 제출한다. message는 NULL 불가 IN 사본이며 호출 중 불변이다. 다음 절차는 기존 선언에 대한 설계이며 제품 코드를 변경하지 않는다.

```c
NetSubmitResult WindowNet_SendResult(const NetWindowResult *message)
{
    if (message == NULL) return NET_SUBMIT_INVALID_ARGUMENT;
    if (message->result == NET_RESULT_NOT_PRESENT) return NET_SUBMIT_NOT_READY; // CAN 사용 문맥 미합의
    if (단일 필드표의 코드·범위·명시 조합이 잘못되면)
        return NET_SUBMIT_INVALID_ARGUMENT;
    if (WINDOW 실제 실행이 보류되어 있거나 초기화·원 기동·시간·profile·필요한 소비 경계가 준비되지 않았으면)
        return NET_SUBMIT_NOT_READY;
    // request_domain_boot+request_sequence+channel은 원 DOMAIN 명령 연결 후보이며 WINDOW 송신 BOOT/TX와 다르다.
    // 이 Wire에는 결과 원 평가 AGE/UPDATE가 없다. C2 meta를 TX_SEQUENCE나 최근 수신 시각으로 생성하지 않는다.
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

두 원형은 `PROPOSED_PRIVATE_FUNCTION`이다. 호출자는 WINDOW의 M04 송신 선택과 DOMAIN의 M07/M04 수신 처리다. 전체 공통 검사 순서는 [공통 Codec](../CAN_COMMON_CODEC.md#check-frame), 전체 필드 변환은 [단일 필드표](../../data/CAN_NODE_DATA.md#wire-WINDOW_RESULT)에 있다. 이 함수는 시간원·Driver·기능 API를 호출하지 않는다. 실행은 Network 소유자 Task/Superloop이며 ISR에서 프레임 해석을 하지 않는다.

| 인자 | 방향·범위·소유·수명 |
|---|---|
| message | IN `NetWindowResult` 전체, NULL 불가. 원 결과 귀속을 보존한 호출 내 불변 사본; Wire에 없는 결과 AGE를 생성하지 않음 |
| source | IN `NetCanSource`, NULL 불가. 확인한 로컬 Boot/TX; Codec는 새 번호 발급/소비를 하지 않음 |
| frame | IN `NetCanFrame`, NULL 불가. 실제 Driver flags/길이/수신시각과 바이트가 동일 입력인 안정 사본 |
| profile | IN 값. 후보 종류를 표시할 뿐 양단 채택 증거를 대신하지 않음 |
| out, meta | OUT caller 저장소, NULL 불가. 입력 및 서로와 겹치지 않음. 모든 검사 성공 뒤 함께 반영; 실패 시 불변 |

Encode `NET_CAN_OK`는 완전 프레임 작성, Decode `NET_CAN_OK`는 형식 변환만 뜻한다. INVALID_ARGUMENT은 NULL/alias 또는 송신 값 오류, MALFORMED는 수신 ID/flags/DLC/길이/padding/코드/명시 조합 오류, CONTRACT_HOLD는 형식·실행 계약 미확인, ORDER_HOLD는 송신 Boot/TX 전제 미확인이다. Decode 성공은 admission·신선도·C2 성공이 아니다.

```c
NetCanResult NetCan_EncodeWindowResult(const NetWindowResult *message,
    const NetCanSource *source, NetCanProfile profile, NetCanFrame *out)
{
    if (message == NULL || source == NULL || out == NULL || 입출력이 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (WINDOW 실제 송수신·소비의 별도 활성화 합의가 없으면)
        return NET_CAN_CONTRACT_HOLD; // 현재 경로는 이 분기에서 끝난다.
    if (profile에 대한 실제 양단 선택·지원 근거가 없으면)
        return NET_CAN_CONTRACT_HOLD;
    if (source의 Boot가0이거나 송신 순번 수용 전제를 확인하지 못하면)
        return NET_CAN_ORDER_HOLD;
    if (message->result == NET_RESULT_NOT_PRESENT) return NET_CAN_CONTRACT_HOLD;
    if (단일 필드표의 모든 코드·범위·sentinel·명시 조합을 만족하지 못하면)
        return NET_CAN_INVALID_ARGUMENT;
    // channel0, 차량 result0..5, reason0..25만 문서상 표현한다. NetVehicleResult에 정의된255 NOT_PRESENT는 CAN 결과 단계로 승인되지 않아 HOLD다.
    NetCanFrame tmp = {0};
    tmp.canId = 0x092; tmp.dlc = 11;
    tmp.length = 20;
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
    // padding 영역(없음)이 있으면 초기값0을 유지한다.
    // 전체 byte/flags/필드표 충족을 재확인한 뒤 한 번에 반영한다.
    *out = tmp; // Driver 호출·TX 순번 발급은 없다.
    return NET_CAN_OK;
}

NetCanResult NetCan_DecodeWindowResult(const NetCanFrame *frame,
    NetCanProfile profile, NetWindowResult *out, NetCanRxMeta *meta)
{
    if (frame == NULL || out == NULL || meta == NULL || 저장소가 겹치면)
        return NET_CAN_INVALID_ARGUMENT;
    if (WINDOW 실제 송수신·소비의 별도 활성화 합의가 없으면)
        return NET_CAN_CONTRACT_HOLD; // 현재 경로는 이 분기에서 끝난다.
    NetCanResult rc = NetCan_CheckFrame(frame, NET_CAN_WINDOW_RESULT,
        NET_CAN_NODE_DOMAIN, profile);
    if (rc != NET_CAN_OK) return rc;
    NetWindowResult tmp;
    NetCanRxMeta tmpMeta;
    for (단일 필드표의 각 필드) {
        uint32_t raw = 0;
        for (uint8_t k = 0; k < 표의 width; ++k)
            raw |= (uint32_t)frame->data[표의 offset + k] << (8u * k);
        // unsigned 필드는 해당 고정폭 멤버에 범위 확인 뒤 담는다.
        // i16은 raw<32768이면 raw, 아니면 (int32_t)raw-65536으로 복원한다.
        // 같은 표의 member를 tmp 또는 tmpMeta.source에 저장한다. enum cast는 없다.
    }
    if (tmp.result == NET_RESULT_NOT_PRESENT) return NET_CAN_CONTRACT_HOLD;
    if (단일 필드표의 코드·예약bit·범위·sentinel 조합 또는 Boot0이 발견되면)
        return NET_CAN_MALFORMED;
    // channel0, 차량 result0..5, reason0..25만 문서상 표현한다. NetVehicleResult에 정의된255 NOT_PRESENT는 CAN 결과 단계로 승인되지 않아 HOLD다.
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

아래 `PROPOSED_PRIVATE_FUNCTION`은 수신 Network M03의 투영·호출 절차다. 원 public `DomainIf_RxWindowCommandResult` 본체는 DOMAIN Owner 소유다. message/meta는 admission에 성공한 동일 입력의 IN 읽기 전용 쌍이고 NULL 불가다. caller는 원 수신시각·원 AGE를 보존하며 [공통 수신/시간 절차](../CAN_TRANSFER_CONTROL.md)로 소비시점의 미반영 경과를 정확히 한 번 계산한 소유자 문맥이다. 이 함수는 별도 현재 시각을 발급하지 않는다.

commandId는 원 명령의 확정된 대응표에서만 연결한다. result/reason은 의미 이름 변환을 쓰고 C2 전용 MOTOR_FAULT/SENSOR_FAULT/TIMEOUT/OTHER 등을 임의 축약하지 않는다. channel0은 유지한다. SignalMeta 전체 근거가 없으면 DomainIf_RxWindowCommandResult를 호출하지 않는다.

```c
NetCanResult NetCan_DeliverWindowResult(const NetWindowResult *message,
    const NetCanRxMeta *meta)
{
    if (message == NULL || meta == NULL || 같은 수신 사건의 쌍이 아니면)
        return NET_CAN_INVALID_ARGUMENT;
    if (WINDOW 실제 송수신·소비의 별도 활성화 합의가 없으면)
        return NET_CAN_CONTRACT_HOLD; // 현재 경로는 이 분기에서 끝난다.
    if (message->result == NET_RESULT_NOT_PRESENT) return NET_CAN_CONTRACT_HOLD;
    if (입력 코드·명시 조합이 잘못되면) return NET_CAN_INVALID_ARGUMENT;
    if (수신 소유자 문맥·실제 소비자 지원·초기화가 준비되지 않았으면)
        return NET_CAN_NOT_READY;
    if (원 생산 세대·순서·identity 대응이 입증되지 않았으면)
        return NET_CAN_ORDER_HOLD;
    if (소비시점의 미반영 홉·수신 대기 또는 필요한 원 사용 한도를 입증하지 못하면)
        return NET_CAN_TIME_HOLD;
    if (본문의 필수 품질·enum·원본 투영 계약이 미합의이거나 정보가 손실되면)
        return NET_CAN_CONTRACT_HOLD;
    // commandId는 원 명령의 확정된 대응표에서만 연결한다. result/reason은 의미 이름 변환을 쓰고 C2 전용 MOTOR_FAULT/SENSOR_FAULT/TIMEOUT/OTHER 등을 임의 축약하지 않는다. channel0은 유지한다. SignalMeta 전체 근거가 없으면 DomainIf_RxWindowCommandResult를 호출하지 않는다.
    DomainIf_WindowCommandResult_t input;
    // 같은 원 DTO/meta와 확인된 표만으로 input 전체를 구성한다.
    if (원 반환과 입력 식별·반영 단계를 보존할 기존 M09 관측을 사전 확보하지 못하면)
        return NET_CAN_NOT_READY;
    DomainIf_Status_t rc = DomainIf_RxWindowCommandResult(&input);
    M09의 확보한 기존 관측에 원 rc, 입력의 BOOT/TX와 본문에 정의한 의미 식별자,
        호출 대상 DomainIf_RxWindowCommandResult, 실제로 확인한 Core/선택 Observer 반영 단계를 보존한다;
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

request_domain_boot+request_sequence+channel은 원 DOMAIN 명령 연결 후보이며 WINDOW 송신 BOOT/TX와 다르다. 이 Wire에는 결과 원 평가 AGE/UPDATE가 없다. C2 meta를 TX_SEQUENCE나 최근 수신 시각으로 생성하지 않는다.

통신 주기/수신 중단: 일회성 결과 단계. 무발생 timeout 없음. 이 수치는 채택·실측되지 않은 후보이며 `>`/`>=` 경계나 원 TTL을 임의 추가하지 않는다. 형식 오류·중복을 정상 heartbeat나 semantic fresh로 쓰지 않는다.

명령·사건·결과는 개별 identity와 본문을 유지한다. 다른 사건으로 덮어쓰지 않는다. Driver가 참조 중인 프레임은 AGE나TX를 수정하지 않는다. 저장 공간 부족·사본 준비 실패는 전체 새 접수 실패이며 순번 장부부터 진전시키지 않는다.

접수 OK, Driver accepted, 최종 TX, 참조 해제, 특정 상대 수신, 기능 완료는 각각 별도 사실이다. bus-off·옛 callback·중복 callback·미확인 결과 처리와 AGE 계산의 단일 절차는 [전송 공통](../CAN_TRANSFER_CONTROL.md), [전체 경로](../../flows/CAN_NETWORK_PATHS.md)를 따른다. 현재 품질 보고를 명령 실패 kind로 위조하지 않는다. identity 있는 discrete의 후발 실패는 원 항목에 연결하며 DOMAIN 송신은 D-03/D-05/O-C10 필수 sink가 준비되기 전 안정 접수 성공을 약속하지 않는다. 다른 ECU에 DomainIf 실패 API를 복제하지 않는다.

## 정상·경계·거부 사례

| 사례 | 기대 처리 |
|---|---|
| 정상 또는 조건부 경로 | 현재는 실행 HOLD다. 향후 같은 원 명령 DONE 뒤 늦은 IN_PROGRESS가 와도 종결 상태를 되돌리지 않는 경계를 유지한다. |
| 고유 경계 | result255는 일반 enum 선언 존재만으로 CAN에서 허용하지 않는다. 원 명령 귀속이 불명확한0도 자동 새 명령으로 만들지 않는다. |
| 의미 반례 | CAN ACK 또는 Driver TX만으로 result DONE·C2 confirmation을 만들지 않는다. |
| ID/FDF/BRS/DLC/길이/padding 또는 raw 예약값 오류 | 임시 변환 전체 폐기, typed 호출·정상 수신 시각·순번 갱신 없음 |
| 같은 원 identity 재송신·AGE 감소 | 같은 소비시점으로 환산해 원본을 젊게 만들지 않음. 순서/clock/wrap 미확인이면 HOLD |
| Queue full·C2 미준비·소비 거부 | 새 항목 전체 인계 실패. 기존 상태·Driver 참조 버퍼 불변, 반환 원인 보존 |
| accepted 뒤 Driver 실패·옛 callback·bus-off | 실제 관측을 원 제출과 연결하고 참조 해제 전 재사용 금지. 자동 재송신·기능 완료 생성 없음 |

근거: B07 can/WINDOW_RESULT.md 전문, S4A의 위 API/Data 절, 최신 S4C TIME_IDENTITY_CONTRACT/FRESHNESS_AGE_TIMEOUT/RESOURCE_QUEUE_DRIVER_FLOW/FAILURE_RECOVERY_CONTRACT. 출처 채택·팀 변경은 별도이며 본문에서 기존 공개 선언이나 Wire를 변경하지 않는다.
