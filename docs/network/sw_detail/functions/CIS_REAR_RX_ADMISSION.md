# CIS_REAR 수신 — 안정된 프레임 확보와 원 관측 접수

<a id="receive-frame"></a>
## NetRear_ReceiveFrame

```c
NetRearResult NetRear_ReceiveFrame(const NetRearFrame *frame,
                                  uint32_t nowMs);
```

`PROPOSED_PRIVATE_FUNCTION`, S2 F20의 후방 프레임 소비 경계다. Driver Port가 수명을 보장하는 rear 전용 프레임 view를 호출 동안 빌려 받아 Network 소유의 bounded 사본을 확보한다. 이 사본을 Codec와 admission에 전달하며 원 Driver 주소를 이후 처리에 남기지 않는다.

Caller는 M08의 Driver 관측을 소비하는 DOMAIN Network 소유자 Task다. ISR이 직접 의미 API를 호출하지 않는다. Called는 [NetRear_Decode](CIS_REAR_CODEC.md#decode)와 아래 NetRear_Admit이며, C2 인계는 접수 성공 뒤 같은 소유자의 [NetRear_Deliver](DOMAIN_CIS_REAR_INGRESS.md#adapter)가 수행한다.

| 인자 | C 타입·방향·범위·단위 | NULL·수명 |
|---|---|---|
| frame | IN `const NetRearFrame *`, Driver가 확보한 ID/flags/DLC/실제 길이/실제 receivedAtMs 및20B 이하 view | NULL 불가. 호출 동안 불변·접근 가능해야 하며 반환 후 원 주소 보관 금지. DMA/ISR가 동시에 바꿀 수 있는 저장소라면 호출 전 안정 확보 필요 |
| nowMs | IN `uint32_t`, DOMAIN 단조 ms | 값 전달, NULL 해당 없음. 실제 receivedAtMs를 대체하지 않음 |

읽는 상태는 M08의 수신 view 수명·Driver 오류와 M07/ST10의 raw 사본 공간이다. 보유하는 임시는 `NetRearFrame`, `NetCisRear`, `NetCanRxMeta`이며 타입 정의는 [Data](../data/CIS_REAR_DATA.md#private)에만 있다. 안정 저장소가 없으면 NO_CAPACITY, NULL/안정성 전제가 없으면 INVALID_ARGUMENT, 실제 length/DLC/Driver 오류는 MALFORMED다. 이후 Codec·Admit의 OK/NOT_READY/DUPLICATE/ORDER_HOLD/TIME_HOLD/NO_CAPACITY를 그대로 반환한다. OK는 안정 수신 사본의 로컬 접수이며 C2 성공이 아니다.

```c
NetRearResult NetRear_ReceiveFrame(const NetRearFrame *frame,
                                  uint32_t nowMs)
{
    if (frame == NULL || 호출 동안 프레임 view의 안정성을 보장할 수 없으면)
        return NET_REAR_INVALID_ARGUMENT;
    if (Driver 오류 또는 DLC11/실제길이20 불일치가 있으면)
        return NET_REAR_MALFORMED;
    if (M07이20B raw 사본과 메타를 안전하게 둘 공간이 없으면)
        return NET_REAR_NO_CAPACITY;
    NetRearFrame raw;
    NetCisRear message;
    NetCanRxMeta meta;
    // 먼저 완전한 flags·ID·DLC·실제길이·실제수신시각과20B를 raw에 복사한다.
    // 이 복사 전에20B보다 큰 payload를 data 배열로 옮기지 않는다.
    // raw는 Network 소유 임시 공간이며 Driver 주소를 저장하지 않는다.
    NetRearResult result = NetRear_Decode(&raw, &message, &meta);
    if (result != NET_REAR_OK) {
        // raw 임시 공간을 반환한다. 수신 순번·정상 liveness는 진전시키지 않는다.
        return result;
    }
    result = NetRear_Admit(&message, &meta, nowMs);
    // Admit 성공은 필요한 Net/meta의 독립 사본을 모두 확보했다는 의미다.
    // 성공/실패 모두 raw와 지역 message/meta 임시 공간을 반환할 수 있다.
    // Admit 실패이면 장부를 이 함수가 대신 진전시키지 않는다.
    return result;
}
```

여기서 “임시 공간 반환”은 함수 지역/사전 확보 저장소의 수명 종료를 뜻하며 제품 heap/free API를 새로 정한 것이 아니다. Driver/DMA의 원 수신 버퍼 반환은 원 Port 계약에 따라 M08이 처리한다. 이 함수가 Network 사본을 버렸다는 이유만으로 장치 저장소 소유권까지 해제됐다고 주장하지 않는다.

<a id="admit"></a>
## NetRear_Admit — 원 관측·전송 순서와 수신 사본 접수

```c
NetRearResult NetRear_Admit(const NetCisRear *message,
                           const NetCanRxMeta *meta,
                           uint32_t nowMs);
```

`PROPOSED_PRIVATE_FUNCTION`, S2 F01/F13/F20 역할이다. [Codec](CIS_REAR_CODEC.md#decode)가 검증한 한 프레임의 Net 값과 메타를 함께 받아 원 관측을 분리하고 안정 수신 사본으로 접수한다. 이 함수의 성공은 DOMAIN cache 반영이 아니다.

## Caller·Called·인자·반환

Caller는 DOMAIN Network 수신 소유자 Task의 위 NetRear_ReceiveFrame이다. 즉시 ISR에서 호출하지 않는다. 후속 Called는 동일 소유자에서 실행할 [NetRear_Deliver](DOMAIN_CIS_REAR_INGRESS.md#adapter)이며 queue 접수와 실제 C2 호출은 별도 단계다. 별도 가상 Boot 승인 API나 SDK 함수를 만들지 않는다.

| 인자 | C 타입·방향·단위 | NULL·수명·범위 |
|---|---|---|
| message | IN `const NetCisRear *`; raw 거리·ms AGE·원 update | NULL 불가. Codec 성공한 사본, 반환 이후 필요분 전체 복사 |
| meta | IN `const NetCanRxMeta *`; source Boot/TX, DOMAIN 로컬 received_at_ms | NULL 불가. message와 같은 프레임, 별개 “가장 최근 메타” 연결 금지 |
| nowMs | IN `uint32_t`, DOMAIN 단조 ms | 값 전달, NULL 해당 없음. received_at과 같은 기동/시계의 경과 근거 필요 |

| 반환 | 의미·변경 범위 |
|---|---|
| NET_REAR_OK | 전체 수신 사본과 admission 장부를 함께 반영. C2 호출 전 |
| INVALID_ARGUMENT | NULL 또는 같은 수신 건 연결 불능. 기존 저장소 유지 |
| NOT_READY | 초기화/소비 경계 미준비. 정상 Application 인계 없음 |
| DUPLICATE | 이미 접수한 같은 프레임 또는 같은 원 관측 재전달. 새 기능 호출 없음 |
| ORDER_HOLD | 기동/순서/wrap·identity 충돌 미확인. 기존 최신 상태를 대체하지 않음 |
| TIME_HOLD | 동일 소비시각 AGE 계산 또는 필요한 사용 한도 근거 부족 |
| NO_CAPACITY | 완전 사본 공간 없음. **순번 장부를 먼저 진전시키지 않음** |

## 상태·수용 제안

M04/ST07은 수용한 최신 프레임의 Boot·TX 순번과 **원 SOURCE_AGE를 그대로 둔 프레임 필드 사본**을 보유한다. M01/ST01은 같은 원 관측 연결과 **같은 소비 시점으로 누적해 보존한 보수적인 AGE 근거**를 유지한다. 프레임의 원 AGE와 누적 AGE는 비교 목적이 다르며 한 값을 다른 값으로 덮어쓰지 않는다. 기존 자료의 명시 필드와 수신 기준점을 사용하며 새 공개 DTO/Wire 멤버를 추가하지 않는다. M07/ST10은 미소비 사본을 소유한다. DOMAIN Manager의 상태 정본은 별도다. 수신 형식 관측, Network admission, C2 성공 반환, 실제 Manager 반영을 네 사건으로 구분한다.

다음은 외부 순서 정책이 없는 상태에서의 **조건부 내부 처리안**이다. 수용된 Boot, wrap/재사용이 없다는 관측 구간 근거가 있을 때만 같은 Boot의 더 큰 TX/UPDATE를 전진으로 해석한다. 순번이 작아진 경우를 무조건 오래됨으로 버리지 않고 ORDER_HOLD한다. half-range·허용 gap·보관 기간을 새로 정하지 않는다. 최초 관측도 현재 CIS Boot와 소비 바인딩이 확인돼야 접수한다.

| 상황 | 수용 결과 |
|---|---|
| 같은 Boot/TX, 같은 본문 | SOURCE_AGE를 포함한 원 의미 바이트가 같으면 완전 중복. 장부·기준시각·C2 호출 모두 무변경 |
| 같은 Boot/TX, 다른 본문 | identity 충돌. 기존 사본 보존·관측, ORDER_HOLD |
| TX만 증가, 같은 원 UPDATE/의미 필드 | 같은 관측 반복. 최신 프레임 관측과 보수적인 AGE만 함께 반영. 원 UPDATE·C2 처리 이력 유지, 새 관측 호출 없음 |
| 같은 원 UPDATE인데 거리/품질/status가 바뀜 | 같은 원본의 상충 표현. 자동 최신값 채택 없이 ORDER_HOLD |
| 새 UPDATE와 전진 TX, 기동·비wrap 근거 있음 | 전체 사본 공간 확보 뒤 새로운 관측 접수 |
| 낮은 seq, wrap 불명확, Boot 변경 | 원 Owner의 기동/순서 수용 근거 필요. 수치 크기만으로 자동 reset 없음 |

동일 원 관측 비교는 [AGE 규칙](../data/CIS_REAR_DATA.md#age)을 따른다. **같은 Boot/TX 프레임의 동일성은 원 SOURCE_AGE를 포함한 전체 의미 바이트로 확인한다.** 같은 Boot/TX인데 AGE만 바뀌었어도 동일 프레임이라고 수용하지 않는다. 수신시각은 로컬 메타이므로 프레임 본문 비교에 넣지 않고, 완전 중복 때문에 기존 수신 기준시각을 갱신하지 않는다. C 구조체의 padding을 포함한 메모리 비교도 사용하지 않는다.

TX가 증가한 같은 원 UPDATE의 반복에서는 거리·validity·reason·status를 대조하고 AGE는 같은 소비 시점으로 각각 누적해 더 오래된 근거를 보존한다. **최신 프레임 TX와 그 원 필드 사본도 함께 진전시켜야** 뒤늦게 온 작은 TX가 이전 장부보다 크다는 이유로 수용되지 않는다. 예를 들어 TX10→TX20의 동일 관측 반복을 수용한 뒤 도착한 TX15는, 동일 Boot·비wrap이라는 전제가 확인된 경우 ORDER_HOLD다. 프레임 사본 확보나 동시 반영이 실패하면 최신 TX·원 프레임 필드·보수적 AGE 모두 이전 상태를 유지한다. 기존 안정 사본을 갱신하는 동작이며 C2 재호출이나 새 관측 번호를 만들지 않는다.

보수적 AGE가70ms인 동안 마지막 TX20의 원 SOURCE_AGE가15ms였다면, 다시 온 TX20/AGE15는 완전 중복이다. 보수적70ms와 raw15ms를 비교하여 본문 충돌로 만들지 않는다. 반대로 TX20/AGE70으로 바뀐 프레임은 원 프레임과 다른 내용이다. 같은 관측 반복에 대한 liveness 갱신은 실제 메시지별 정상 수신 계약에 따라 별도로 바인딩하며 기능 freshness와 합치지 않는다.

## 의사코드와 원자 반영

```c
NetRearResult NetRear_Admit(const NetCisRear *message,
                           const NetCanRxMeta *meta, uint32_t nowMs)
{
    if (message == NULL || meta == NULL || 같은 프레임의 쌍임을 확인하지 못하면)
        return NET_REAR_INVALID_ARGUMENT;
    if (DOMAIN 수신 소유자·필수 소비 경계가 준비되지 않았으면)
        return NET_REAR_NOT_READY;
    if (CIS Boot 수용이나 같은 기동의 프레임/원 관측 순서 근거가 없으면)
        return NET_REAR_ORDER_HOLD;
    if (보존한 최신 프레임 기록이 있고 같은 Boot/TX이면) {
        if (그 프레임의 원 SOURCE_AGE를 포함한 의미 필드가 다르면)
            return NET_REAR_ORDER_HOLD;
        // 완전 중복이다. 보수적 AGE와 raw AGE를 비교하지 않는다.
        // 장부·원 수신 기준점·원 관측·C2 호출을 모두 그대로 둔다.
        return NET_REAR_DUPLICATE;
    }
    if (현재 수용한 CIS Boot의 최신 프레임 기록이 있는데,
        합의된 비wrap 전제에서 그 최신 TX보다 전진했음을 확인하지 못하면)
        return NET_REAR_ORDER_HOLD;
    if (같은 수용 Boot의 같은 원 UPDATE인데 거리·validity·reason·status 원 의미 필드가 다르면)
        return NET_REAR_ORDER_HOLD;
    if (같은 수용 Boot의 기존 원 관측이 있고 UPDATE가 달라졌는데,
        합의된 비wrap 전제에서 원 UPDATE도 전진했음을 확인하지 못하면)
        return NET_REAR_ORDER_HOLD;
    if (수신 이후 경과·미반영 홉을 같은 nowMs까지 확정할 수 없거나,
        같은 Boot/UPDATE의 기존 관측이 있는데 그 AGE를 같은 시점으로 비교할 수 없으면)
        return NET_REAR_TIME_HOLD;
    if (이미 접수한 같은 수용 Boot/UPDATE와 같은 원 의미의 반복이면) {
        if (최신 프레임 원 필드 사본과 보수적 AGE 근거를 함께 준비할 공간이 없으면)
            return NET_REAR_NO_CAPACITY;
        // 기존/새 AGE를 같은 nowMs로 환산해 더 오래된 근거를 준비한다.
        // 새 프레임의 원 SOURCE_AGE·Boot/TX·수신시각 사본은 별도로 준비한다.
        if (두 근거의 전체 확보나 같은 소유자 안의 동시 반영에 실패하면)
            return NET_REAR_NO_CAPACITY;
        // 전체 준비 뒤 최신 프레임 관측과 보수적인 AGE만 함께 반영한다.
        // 원 UPDATE·C2 처리 이력은 유지한다. 새 C2 호출·fresh 판정을 만들지 않는다.
        return NET_REAR_DUPLICATE;
    }
    if (완전한 수신 사본과 대응 메타를 보관할 공간이 없으면)
        return NET_REAR_NO_CAPACITY;
    // message·meta와 AGE 반영 기준을 임시 사본으로 먼저 확보한다.
    // 같은 소유자 처리 안에서 전체 사본 공개와 ST07 admission 장부 진전을 함께 반영한다.
    // 복사/queue 저장이 실패하면 장부는 이전 상태 그대로다.
    // 이후 같은 사본을 NetRear_Deliver가 소비한다. 아직 C2 성공·cache 반영은 아니다.
    return NET_REAR_OK;
}
```

Queue가 가득 찬 입력의 순번을 먼저 기록하면 이후 같은 정상 입력이 “이미 처리됨”으로 버려질 수 있으므로 금지한다. peer reboot 수용 때도 과거 사본과 새 Boot를 섞지 않으며 실제 장치 참조 해제와 DOMAIN 상태 무효화는 각 Owner가 맡는다. RX overflow로 잃은 프레임은 가상의 누락 바이트를 채워 복원하지 않는다.

[M01](../modules/M01_SOURCE_CONTEXT.md) · [M04](../modules/M04_MESSAGE_SERVICE.md) · [M07](../modules/M07_BUFFER_OUTCOME.md) · [Data](../data/CIS_REAR_DATA.md) · [DOMAIN 인계](DOMAIN_CIS_REAR_INGRESS.md)
