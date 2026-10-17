# NetRear_Dispatch — 송신할 관측 선택과 Driver 인계

<a id="dispatch"></a>

```c
NetRearResult NetRear_Dispatch(uint32_t nowMs);
```

`PROPOSED_PRIVATE_FUNCTION`. S2 F12/F19/F21/F22의 rear 경로를 설명하기 위한 내부 원형이다. 미송신 관측을 선택하고 최종 AGE를 반영한 뒤 안정 주소의 프레임을 Driver에 넘긴다. 기존 공개 API나 실재 SDK 함수의 이름으로 주장하지 않는다.

## Caller·Called·인자·반환

Caller는 CIS Network의 소유자 Task/Superloop다. Called는 [NetRear_Encode](CIS_REAR_CODEC.md#encode)와 M08 장치 제출 경계이며 실제 HAL 함수 원형은 미확정이다. Driver 결과는 [NetRear_ObservePort](CAN_PORT_RECOVERY.md)로 같은 소유자에게 돌아온다는 후보 구조다.

| 인자 | C 타입·방향·단위·범위 | 수명 |
|---|---|---|
| nowMs | IN `uint32_t`, CIS 단조 ms. 접수시각과 같은 시계·기동의 비교 가능한 관측 | 값 전달, NULL 해당 없음. 단순 unsigned 차감만으로 wrap 안전성을 가정하지 않음 |

| 반환 | 보장과 후속 상태 |
|---|---|
| NET_REAR_NO_WORK | 선택할 pending 없음 |
| NET_REAR_NOT_READY / ORDER_HOLD / TIME_HOLD | 경로/원본/시간 근거 부족. 정상 TX로 기록하지 않음 |
| NET_REAR_NO_CAPACITY | 아직 Driver 소유인 프레임 등으로 안정 저장소 없음; pending 보존 |
| NET_REAR_INVALID_ARGUMENT | Codec가 입력 모순 검출. 해당 pending을 재검토 대상으로 격리 |
| NET_REAR_DRIVER_REJECTED | 장치가 미접수를 확실히 통지. 재제출은 새 평가에서만 가능 |
| NET_REAR_DRIVER_PENDING | 장치가 제출을 접수했거나 소유권 결과가 아직 불명확. 버퍼 유지; TX 완료 아님 |

## 상태·순서 선택·수명

M04/ST05의 의미 사본을 읽고 ST06의 송신 BOOT/TX_SEQUENCE를 사용한다. M07/ST09는 실제 프레임과 제출 identity를 소유한다. 최초 TX_SEQUENCE1, 프레임마다 증가하는 원 후보를 보존한다. **순번을 소비하는 시점은 Driver 제출을 시도하는 직전**으로 제안한다. 성공 여부가 불명확한 제출에는 그 번호를 재사용하지 않는다. 이는 새 Wire 승인이 아닌 내부 귀속 제안이며 시작/상한/wrap/양단 허용 gap 바인딩을 기다린다. `0xFFFFFFFF` 다음 순번은 임의0/1로 감지·재시작하지 않고 ORDER_HOLD다.

원 관측 UPDATE_SEQUENCE는 바꾸지 않는다. encode가 실패해 Driver에 넘기지 않았다면 순번을 소비하지 않는다. 장치가 명확히 미접수했어도 시도 번호를 소비한 기록은 유지한다. 다음 시도는 새 프레임·AGE·순번으로 준비하며 자동 retry 횟수는 정하지 않는다. 재기동하거나 기한/순서 근거가 사라진 pending을 자동 재송신하지 않는다.

**제출 결과가 불명확한 시도에는 원 pending 관측을 예약한다.** 안정 프레임 공간이 추가로 남아 있어도 같은 관측을 새 버퍼·새 TX_SEQUENCE로 다시 선택하지 않는다. 이 예약은 Driver가 확실히 미접수와 참조 없음을 함께 통지하거나, 기존 시도의 결과와 참조 상태가 확인된 뒤에만 해제한다. 그 뒤에도 실제 원 AGE·기동·순서를 다시 확인한다. 결과가 미확인인데 byte 저장소만 해제됐다는 이유로 같은 관측을 재제출하지 않는다.

순수 Codec 전에 원 접수시각→nowMs 대기만 한 번 더한다. 참조 중인 프레임에는 AGE/TX_SEQUENCE를 다시 쓰지 않는다. 프레임 제출 뒤의 Driver 지연은 미측정일 수 있으므로 hop5ms로 덮어 성공 freshness를 주장하지 않는다.

## 의사코드

```c
NetRearResult NetRear_Dispatch(uint32_t nowMs)
{
    if (ST05에 미송신 관측이 없으면) return NET_REAR_NO_WORK;
    if (그 원 관측이 결과 미확인 시도에 예약되어 있거나 관련 Driver 참조가 남아 있으면) {
        // 다른 프레임 공간이 있어도 같은 관측을 새 순번으로 중복 제출하지 않는다.
        return NET_REAR_DRIVER_PENDING;
    }
    if (장치·profile·원본 기동 또는 호출 문맥이 준비되지 않았으면)
        return NET_REAR_NOT_READY;
    if (같은 로컬 기동의 경과를 확정할 수 없으면)
        return NET_REAR_TIME_HOLD;
    if (원 사용 한도를 확인해야 하는데 근거가 없거나 만료가 확인되면) {
        // 영향받는 유효 사용을 제한하고 원 AGE를 0으로 재시작하지 않는다.
        return NET_REAR_TIME_HOLD;
    }
    if (다음 TX_SEQUENCE 또는 원 관측 수용 순서를 확정할 수 없으면)
        return NET_REAR_ORDER_HOLD;
    if (M07의 안정 주소 프레임을 새로 소유할 수 없으면)
        return NET_REAR_NO_CAPACITY;
    // ST05 사본의 원 AGE + 아직 미반영인 접수 후 경과를 충분한 폭으로 계산한다.
    // u16 최대65535에서 포화하고 원 UPDATE_SEQUENCE·품질은 유지한다.
    // NetRear_Encode(경과 반영 사본, 현재 CIS Boot와 다음 프레임 순번, M07 프레임).
    if (Codec가 입력 모순으로 실패하면) {
        // 임시 공간만 반환한다. Driver 호출과 순번 소비는 아직 없다.
        return NET_REAR_INVALID_ARGUMENT;
    }
    if (Codec가 profile·Boot 준비 부족을 반환하면) {
        // 임시 프레임 공간을 반환하고 pending과 시도 순번을 그대로 둔다.
        return NET_REAR_NOT_READY;
    }
    // 원 관측을 이 시도에 예약하고 제출 identity를 기록한 뒤 시도 순번을 소비한다.
    // 장치에 전달하는 주소는 M07 소유이며 함수 스택을 가리키지 않는다.
    // 실제 장치 제출 경계를 호출한다. ISR 즉시 완료 통지도 이 identity와 직렬화한다.
    if (장치가 미접수와 참조 없음 둘 다 확실히 알려주면) {
        // 프레임 공간과 이 시도의 예약을 반환한다. pending 재평가가 필요하며 성공으로 세지 않는다.
        return NET_REAR_DRIVER_REJECTED;
    }
    // 접수 또는 결과 미확인 동안 프레임을 잠그고 바이트를 유지한다.
    // 확인된 접수일 때만 선택한 미송신 항목을 소비한 것으로 처리한다.
    // 접수 미확인 pending은 예약한 채로 보존하여 다음 Dispatch에서 재선택되지 않게 한다.
    return NET_REAR_DRIVER_PENDING;
}
```

마지막 반환은 상대 수신·CAN ACK·DOMAIN 처리 성공을 의미하지 않는다. busy/즉시 오류라도 Driver가 참조하지 않는다는 계약이 없으면 버퍼를 해제하지 않는다. 후방 주기 상태 손실을 Command/RequestResult의 `NetworkLocalFailure_t`로 위조하지 않고 기존 transport 진단에 연결한다.

[M04](../modules/M04_MESSAGE_SERVICE.md) · [M07](../modules/M07_BUFFER_OUTCOME.md) · [M08](../modules/M08_DEVICE_PORT.md) · [Data/AGE](../data/CIS_REAR_DATA.md#age) · [흐름](../flows/CIS_REAR_144_TO_344.md)
