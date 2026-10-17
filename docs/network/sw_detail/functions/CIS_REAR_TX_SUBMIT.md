# CisNet_PublishRear — CIS 관측의 안정된 로컬 접수

<a id="publish-rear"></a>

```c
NetSubmitResult CisNet_PublishRear(const NetCisRear *message);
```

기존 S4A/T10:73~75에 있는 `PROPOSED_DESIGN/BINDING_OPEN` 원형이다. CIS 초음파 관측 담당이 CIS Network에 거리·상태·품질을 제출한다. 필요한 값을 독립 사본으로 확보한 성공만 반환하며 실제 CAN 송신이나 DOMAIN 소비는 보증하지 않는다.

## Caller·Called·자료

Caller는 CIS 센서 담당의 Task 또는 Superloop다. 본체는 CIS Network/M03·M04, S2 F10/F11 역할에 대응한다. 이 접수 단계에서 Driver나 DOMAIN API를 호출하지 않는다. 이후 [NetRear_Dispatch](CIS_REAR_TX_DISPATCH.md)가 미송신 자료를 선택한다.

| 인자 | 방향·C 타입·범위·단위 | NULL·수명·소유 |
|---|---|---|
| message | IN `const NetCisRear *`; [여섯 멤버와 Wire 의미](../data/CIS_REAR_DATA.md#meaning). 거리 raw0.1cm=1mm, AGE ms | NULL 불가. 호출 동안만 읽고 OK 전에 모든 필드·동일 관측 문맥을 복사 |

반환은 [NetSubmitResult 정본](../data/CIS_REAR_DATA.md#type-NetSubmitResult)의 네 값이다. INVALID_ARGUMENT은 포인터·enum·명시 조합 오류, NOT_READY는 초기화/기동/시간/링크 바인딩 부족, NO_CAPACITY는 완전 사본 공간 부족, OK는 전체 접수다. 품질 INVALID 자체는 인자 오류가 아니다.

## 상태·선후조건·동시성

읽는 상태는 CIS 원 관측 문맥과 M04/ST05 미송신 현재값, 로컬 접수시각 근거다. OK일 때만 ST05 의미 사본과 접수 기준점을 함께 반영한다. 같은 관측 반복의 AGE를 이전보다 젊게 만들지 않는다. 다른 관측으로 교체할 때도 순서 수용이 확인되어야 한다. 이미 Driver가 참조하는 M07/ST09 프레임은 교체 대상이 아니다.

Driver 제출 결과가 미확인이라 선택 예약된 의미 항목도 아직 시도하지 않은 현재값과 구별한다. 예약된 원 관측·결과 귀속을 새 입력으로 덮어쓰지 않는다. 별도의 안전한 대기 공간에 새 관측을 받는 경우에도 미확인 시도의 같은 관측을 다시 선택하지 않는 [송신 예약 규칙](CIS_REAR_TX_DISPATCH.md#dispatch)을 적용한다.

동일 Owner가 접수와 송신 선택을 직렬 처리한다는 전제다. ISR 직접 호출·재진입은 허용하지 않는 설계이며 실제 Task/Lock/Queue 구현은 미검증이다. 성공 후 caller는 입력을 재사용할 수 있고, 실패 후 기존 대기는 바뀌지 않는다. Queue 깊이와 물리 센서 최대 범위는 새로 정하지 않는다.

## 정상·오류·의사코드

NULL/코드/명시 모순을 먼저 검사하고 준비·원 관측 연결을 확인한다. 이어 완전 사본을 위한 공간을 확보한 뒤 한 번에 pending을 교체한다. 정상 NO_OBJECT·FAULT·NO_DATA를 0거리 또는 정상 거리로 바꾸지 않는다. 같은 종류의 아직 송신하지 않은 현재값만 교체 가능하며 새 개별 사건의 덮어쓰기에 확대하지 않는다.

```c
NetSubmitResult CisNet_PublishRear(const NetCisRear *message)
{
    if (message == NULL) {
        // 입력을 읽지 않고 기존 대기를 유지한다.
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (validity가 0~1 밖이거나 reason이 0~9 밖이거나 status가 0~5 밖이면) {
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (VALID_DISTANCE인데 거리 raw가 65535이면) {
        // 무효 거리 표식을 실제 거리로 접수하지 않는다.
        return NET_SUBMIT_INVALID_ARGUMENT;
    }
    if (초기화·원 관측 세대·순서·수용 시각·경로 준비를 확인하지 못하면) {
        // 이전 기동의 자료를 새 기동 관측으로 재생하지 않는다.
        return NET_SUBMIT_NOT_READY;
    }
    if (같은 원 관측을 반복 제출하면) {
        if (기존/새 AGE를 같은 수용 시점으로 비교할 시간 근거가 없으면)
            return NET_SUBMIT_NOT_READY;
        // 기존/새 AGE를 같은 수용 시점으로 환산하고 더 오래된 근거를 보존한다.
    }
    if (기존 미송신 현재값을 안전하게 교체할 수 없고 완전 사본을 둘 공간도 없으면) {
        return NET_SUBMIT_NO_CAPACITY;
    }
    // 여섯 멤버와 동일 관측의 수용 문맥·시각을 Network 임시 사본으로 확보한다.
    // 전체 확보 뒤 ST05에 함께 반영한다. 실패 중간에는 기존 사본을 바꾸지 않는다.
    // 원 update_sequence와 품질은 유지하고 caller 포인터를 남기지 않는다.
    return NET_SUBMIT_OK;
}
```

정상 형식의 source_age_ms=65535는 포화 하한으로 보존할 수 있다. 그것이 freshness를 증명하지는 않는다. 접수 거절 뒤 생산자가 다시 제출할 때도 실제 경과·현재 관측을 다시 확인한다. 자동 재시도 횟수·새 관측 번호는 만들지 않는다. 접수 뒤 Bus-off나 Driver 실패는 [Port 관측](CAN_PORT_RECOVERY.md)으로 별도 다룬다.

[M03](../modules/M03_TYPED_HANDOFF.md) · [M04](../modules/M04_MESSAGE_SERVICE.md) · [Data](../data/CIS_REAR_DATA.md) · [전체 흐름](../flows/CIS_REAR_144_TO_344.md)
