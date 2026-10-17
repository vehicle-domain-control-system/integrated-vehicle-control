# NetRear_ObservePort — Driver 결과·버퍼 해제·복구 관측

<a id="observe-port"></a>

```c
NetRearResult NetRear_ObservePort(NetRearPortEvent event,
                            const NetCanSource *submitted,
                            bool referenceReleased,
                            uint32_t nowMs);
```

`PROPOSED_PRIVATE_FUNCTION`. S2 F21/F22/F23/F24의 rear 경로 책임을 묶은 내부 관측 원형이다. M08이 받은 장치 사실을 Network Owner 문맥으로 전달하며 송신 접수·최종 결과·참조 해제를 구분한다. 실제 HAL callback 원형이나 제품 복구 API가 아니다.

## Caller·Called·인자·반환

Caller는 Driver 통지를 안정 사본으로 확보한 M08/M07의 소유자 Task/Superloop다. ISR은 실제 사실·프레임 식별을 안전히 저장하는 경계까지만 맡고 이 의미 처리 함수를 직접 호출하지 않는다. Called는 M07 저장소 반환 및 M09 관측 역할이며 구체 SDK 함수는 미확정이다. 후방 주기 상태에는 DOMAIN의 명령 후발 실패 함수를 호출하지 않는다.

| 인자 | C 타입·방향·범위·단위 | NULL·수명 |
|---|---|---|
| event | IN `NetRearPortEvent`, 정의된 7종 중 하나 | 값 전달. 실제 장치 관측과 명시 대응 |
| submitted | IN `const NetCanSource *`, TX 결과/참조 해제 대상의 CIS 송신 Boot+TX | 해당 세 TX 사건에는 NULL 불가. BUS_OFF/RX_OVERFLOW/RECOVERED/LOCAL_RESET에는 NULL만 전달. 호출 동안 borrow |
| referenceReleased | IN `bool`, 해당 프레임의 Driver/DMA 참조 종료를 실제 확인한 값 | true가 TX 성공을 뜻하지 않음. 대상 없는 사건에는 false |
| nowMs | IN `uint32_t`, 해당 노드 단조 ms 관측 | 값 전달. 로컬 reset 전후 시계를 섞지 않음 |

정확한 인자 조합은 다음 **내부 후보 호출 계약**을 따른다. 실제 HAL의 반환/IRQ 인자에 이 형태가 이미 존재한다고 주장하지 않는다.

| 사건 | submitted | referenceReleased |
|---|---|---|
| NET_REAR_PORT_TX_COMPLETED / NET_REAR_PORT_TX_FINAL_FAILED | NULL 불가, 같은 제출 Boot/TX | 실제 근거에 따라 true 또는 false. 결과와 해제를 분리 |
| NET_REAR_PORT_REFERENCE_RELEASED | NULL 불가, 같은 제출 Boot/TX | **true 필수**. false면 INVALID_ARGUMENT |
| NET_REAR_PORT_BUS_OFF / NET_REAR_PORT_RX_OVERFLOW / NET_REAR_PORT_RECOVERED / NET_REAR_PORT_LOCAL_RESET | **NULL 필수** | **false 필수**. 개별 프레임 해제는 별도 사건으로 전달 |

반환 OK는 이번 관측과 해당 상태 처리를 완료했다는 뜻이다. 중복 관측을 변경 없이 처리하거나, 결과가 미확인이어도 참조 해제 관측을 처리한 경우 OK일 수 있으며 제출 성공·재전송 허가를 뜻하지 않는다. INVALID_ARGUMENT은 잘못된 event/인자 조합, ORDER_HOLD는 현재 제출 identity와 불일치하거나 오래된 callback 분리가 안 됨, NOT_READY는 복구 필수조건 미충족이다. DRIVER_PENDING은 이번 처리에서 참조 종료 등 필요한 관측 근거가 아직 부족하여 대기한다는 뜻이다. 결과 미확인 identity와 원 관측 예약은 최종 결과 확인까지 별도로 유지한다. 실패 반환 자체를 장치 참조 종료 근거로 쓰지 않는다. 별도의 명시적 참조 해제가 입증된 경우에만 해당 저장소를 한 번 반환할 수 있다.

## 상태·복구 계약

M07/ST09의 제출 identity·최종 결과·참조 상태와 M08/ST11의 현재 Driver callback 수명을 확인한다. Boot/TX 일치만으로 reset 전 callback이 아니라는 것을 증명할 수 없으므로 실제 Driver 수명 근거도 필요하다. 참조 해제 전에 주소를 재사용하지 않는다. Buffer 반납과 TX 성공을 하나의 flag로 합치지 않는다.

같은 terminal 통지나 참조 해제 통지는 한 번만 반영한다. 이미 해제한 저장소를 다시 반환하지 않으며 동일 통지로 성공/실패 counter를 두 번 증가시키지 않는다. 같은 제출 identity에 완료와 최종 실패가 상충하면 어느 쪽도 새 확정 결과로 덮어쓰지 않고 ORDER_HOLD와 원 관측을 남긴다. 참조 해제가 먼저 왔다면 byte 저장소는 안전하게 돌려줄 수 있으나 결과 미확인 identity 기록은 별도로 유지한다. 이후 통지를 이 기록과 연결하지 못하면 현재 재사용 버퍼에 적용하지 않는다. 실제 보관 한도·기간이 없는 상태에서는 무제한 이력 또는 구 identity 안전 판별을 보증하지 않는다.

Bus-off는 새 제출 준비를 닫고 진행 중 프레임 결과를 별도로 확인할 사건이다. Driver가 실제 회복·참조 해제를 알려준 뒤 profile/현재기동/시간/필수소비 경계를 재확인해야 새 자료를 접수한다. 고정 회복 시간·retry 횟수·자동 purge 정책을 새로 만들지 않는다. 과거 pending을 새 기동으로 재라벨하여 자동 송신하지 않는다.

RX overflow는 실제 손실 관측이며 특정 sensor의 FAULT나 유효 NO_OBJECT가 아니다. CAN 단일 프레임의 빠진 조각을 임의 결합하지 않는다. local reset은 옛 시계/순번/연결을 분리하지만 Driver 참조를 즉시 해제할 근거는 아니다.

## 의사코드

```c
NetRearResult NetRear_ObservePort(NetRearPortEvent event,
                            const NetCanSource *submitted,
                            bool referenceReleased, uint32_t nowMs)
{
    if (event가 선언 집합 밖이거나 사건별 submitted/해제 인자가 맞지 않으면)
        return NET_REAR_INVALID_ARGUMENT;
    if (TX 결과 또는 참조 해제 사건이면) {
        if (현재 또는 결과 미확인으로 보존한 제출 Boot/TX·실제 Driver 수명이 입력과 맞지 않으면) {
            // 오래된 callback으로 새 프레임을 완료·해제하지 않는다.
            return NET_REAR_ORDER_HOLD;
        }
        if (같은 identity의 확인된 완료와 최종 실패 통지가 상충하면) {
            // 두 원 관측을 보존한다. 성공/실패를 소급 교체하거나 buffer를 다시 반환하지 않는다.
            // 이 통지로 참조 해제가 새로 입증됐다면 아직 반환하지 않은 저장소만 한 번 반환한다.
            return NET_REAR_ORDER_HOLD;
        }
        if (이미 반영한 같은 결과/해제 통지이고 새로 확인된 사실이 없으면) {
            // 중복 통지는 상태·counter·공간 반환을 반복하지 않는다.
            return NET_REAR_OK;
        }
        // 실제 완료/최종 실패만 기록한다. 참조 해제 통지는 결과 성공이 아니다.
        if (참조 해제가 확인되지 않았으면)
            return NET_REAR_DRIVER_PENDING;
        // 결과가 미확인이라도 안전하게 해제된 byte 저장소는 반환할 수 있다.
        // 아직 반환하지 않은 저장소만 한 번 반환한다.
        // 단, 원 제출 identity와 결과 미확인 기록은 성공으로 지우지 않는다.
        // 후방 주기 상태 실패를 차량 FAILED나 새 NetworkLocalFailure kind로 만들지 않는다.
        return NET_REAR_OK;
    }
    if (event == NET_REAR_PORT_BUS_OFF) {
        // 해당 CAN의 새 제출 준비를 닫는다. 진행 중 버퍼는 참조 종료까지 보존한다.
        // 실제 Bus-off 관측만 기록하고 다른 링크/모든 peer의 고장을 합성하지 않는다.
        return NET_REAR_NOT_READY;
    }
    if (event == NET_REAR_PORT_RX_OVERFLOW) {
        // 실제 손실 단위·경계만 기록하고 사라진 frame이나 정상수신시각을 합성하지 않는다.
        return NET_REAR_OK;
    }
    if (event == NET_REAR_PORT_LOCAL_RESET) {
        // 이전 clock·순번·source 연결을 새 기동과 분리한다.
        // 장치가 참조하는 버퍼는 논리 무효화만으로 덮어쓰지 않는다.
        return NET_REAR_NOT_READY;
    }
    // RECOVERED라도 장치 up만으로 기능 허용을 복원하지 않는다.
    if (참조 해제·profile·현재 기동·시간·필수소비 경계를 확인하지 못하면)
        return NET_REAR_NOT_READY;
    // 새 유효 관측을 재평가할 준비만 연다. 이전 pending의 자동 송신은 하지 않는다.
    return NET_REAR_OK;
}
```

관측 counter를 사용하는 경우 원 `NetworkHealthSnapshot_t`의 포화 규칙을 따른다. 실제 장치 오류와 count 단위가 확인되지 않으면 새 수치를 만들지 않는다. Bus-off·peer reboot·정상수신을 이유로 counter를0으로 초기화하지 않으며 실제 local Network init/reset만 별도 근거가 된다. Counter의 상세 공개 조회 API는 이 B11 흐름의 신규 기능으로 확대하지 않는다.

[M07](../modules/M07_BUFFER_OUTCOME.md) · [M08](../modules/M08_DEVICE_PORT.md) · [M09](../modules/M09_OBSERVATION.md) · [L05](../layers/L05_TRANSPORT_PORT.md) · [Data](../data/CIS_REAR_DATA.md#private) · [흐름](../flows/CIS_REAR_144_TO_344.md)
