# DOMAIN 후방 typed 인계 — Network Adapter와 공개 API의 경계

<a id="adapter"></a>
## NetRear_Deliver

```c
NetRearResult NetRear_Deliver(uint32_t nowMs);
```

S2 F01/F08에 대응하는 `PROPOSED_PRIVATE_FUNCTION`이다. DOMAIN Network의 수신 소유자가 호출하고, 안정 수신 사본을 C2 자료로 변환해 DOMAIN 소유 공개 API에 전달한다. Network가 위험 판단이나 Manager 상태 본체를 구현하는 함수가 아니다.

| 인자 | 타입·방향·단위·범위 | 수명·NULL |
|---|---|---|
| nowMs | IN `uint32_t`, DOMAIN 단조 ms, 수신시각과 같은 기동·비교 가능한 시계 | 값 전달. NULL 해당 없음 |

Caller는 [수신 수용](CIS_REAR_RX_ADMISSION.md) 뒤 pending을 소비하는 ISR 밖 동일 단일 소유자 문맥이다. Called는 아래 실제 선언 `DomainIf_RxCisRear` 한 개다. ISR 호출·재진입은 제외하는 설계이며 정확한 Task/직렬화 연결은 D-05/G06에서 확인해야 한다.

읽는 상태는 M07/ST10의 같은 Net/meta 사본과 M01/ST01의 원 관측 연결이다. C2 OK 뒤 Network의 전달 기록은 남길 수 있지만, 현재 DOMAIN Store는 `VEHICLE_STATE_MANAGER_DUPLICATE`도 `DOMAIN_IF_OK`로 반환한다. 신규 cache 반영은 호출 전후 `DomainIf_QueryGetCisRear()`의 `updateSequence`·`sourceGeneration`·값/품질을 원 입력과 대조해 별도로 확인한다. 함수가 만든 CisRearState_t 임시는 호출 동안만 유효하며 DOMAIN이 이후 필요하면 반환 전에 복사한다.

| B11 반환 | 결과·pending 처리 |
|---|---|
| NO_WORK | 소비할 사본 없음 |
| NOT_READY / ORDER_HOLD / TIME_HOLD | 초기화·품질/세대·시간 바인딩 부족. pending은 원 근거로 제한 보관, 정상 호출 없음 |
| NO_CAPACITY | C2 QUEUE_FULL을 관측. 사본 보존·추후 재평가, 자동 busy loop 없음 |
| CONSUMER_REJECTED | C2의 INVALID_ARGUMENT/REJECTED/UNSUPPORTED 또는 미지원 결과. 해당 호출을 성공으로 세지 않고 격리·관측 |
| OK | C2 로컬 성공(중복 가능). 동일 pending 재호출 금지, 신규 Manager 반영·위험판정·VSS 완료는 별도 확인 |

NOT_INITIALIZED/NOT_AVAILABLE은 NOT_READY로 관측하되 원 C2 상태값을 진단에 보존한다. 위 대응은 Network 내부 분류 제안이며 C2 enum/API를 변경하지 않는다. 실패 시 DOMAIN 내부 상태가 항상 불변이라고 보증할 수 없으므로 근거 없이 같은 입력을 무조건 재호출하지 않는다. 재시도는 API 실패의 미접수 의미와 소비자 준비가 확인된 경우에만 원 사본·증가 AGE로 수행하며 횟수/간격은 미합의다.

## 명시 변환과 의사코드

`rearDistanceMm`은 raw 정수를 그대로 옮긴다. validity0/1, reason0~9, status0~5는 [Data의 선언 이름](../data/CIS_REAR_DATA.md#meaning)으로 대응한다. UPDATE_SEQUENCE는 meta.updateSequence, 소비 시점 AGE는 meta.ageMs에 연결한다. Wire에 독립 DataQuality byte가 없으므로 validity/reason/status에서 meta.quality로 가는 투영은 기존 `CIS_QUALITY_PROJECTION` 미결의 조건부 제안이다.

`sourceGeneration`은 수용된 원 CIS 생산 계보의 값이다. 수신 CAN BOOT_ID가 같은 수명을 표현한다는 근거가 확인될 때만 대응 가능하며 DOMAIN Boot나 임의0으로 채우지 않는다. 세대0 허용 정책 미회신을 NO_DATA 자동 강등으로 숨기지 않는다.

```c
NetRearResult NetRear_Deliver(uint32_t nowMs)
{
    if (안정된 수신 pending이 없으면) return NET_REAR_NO_WORK;
    if (C2 초기화·지원·필수 소비 경계·소유자 문맥이 확인되지 않으면)
        return NET_REAR_NOT_READY;
    if (원 CIS 세대와 BOOT의 대응을 확인할 수 없으면)
        return NET_REAR_ORDER_HOLD;
    if (미반영 홉·수신 대기를 nowMs까지 정확히 한 번 누적할 수 없으면)
        return NET_REAR_TIME_HOLD;
    if (validity/reason/status 조합의 C2 품질 투영이 합의되지 않았으면)
        return NET_REAR_NOT_READY;
    CisRearState_t state;
    // 같은 pending의 값·원 관측·meta로만 state의 전체 멤버를 구성한다.
    // 유효 raw123은 rearDistanceMm=123이다. 정수 cm로 절삭하지 않는다.
    // sentinel65535는 상태·품질과 함께 보존하며 실제 거리로 소비하지 않도록 전달한다.
    // UPDATE_SEQUENCE를 유지하고 sourceGeneration에는 확인된 원 계보만 연결한다.
    // 같은 관측의 기존 AGE보다 젊어지지 않는 소비시점 ageMs를 사용한다.
    DomainIf_Status_t status = DomainIf_RxCisRear(&state);
    if (status == DOMAIN_IF_OK) {
        // 중복 Manager 관측도 DOMAIN_IF_OK이므로 이 pending을 재호출하지 않는다.
        // 신규 commit 여부는 원 입력과 Query 전후 seq/세대/품질의 연결로 별도 확인한다.
        return NET_REAR_OK;
    }
    if (status == DOMAIN_IF_QUEUE_FULL) {
        // 원 사본은 유지한다. 부분 접수가 없다는 계약 확인 없이 즉시 재호출하지 않는다.
        return NET_REAR_NO_CAPACITY;
    }
    if (status == DOMAIN_IF_NOT_INITIALIZED || status == DOMAIN_IF_NOT_AVAILABLE)
        return NET_REAR_NOT_READY;
    // INVALID_ARGUMENT/REJECTED/UNSUPPORTED/미지원 결과를 원 상태대로 관측한다.
    // 같은 입력의 무한 재시도·차량 FAILED 합성·관측 AGE0 재시작을 하지 않는다.
    return NET_REAR_CONSUMER_REJECTED;
}
```

위 의사코드의 `state`는 [CisRearState_t 정본](../data/CIS_REAR_DATA.md#type-CisRearState_t) 전체 멤버를 채운 지역 자료다. 본문은 설명용 절차이며 컴파일할 제품 구현이 아니다.

<a id="domain-api"></a>
## DomainIf_RxCisRear — 기존 외부 소유 선언

```c
DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);
```

설계 원형은 T16:407/S4A `api/domain_rx.md`에서 출발했으며, 현재 `develop`의 `Domain_Interface_v0.5_C2_target.h` 선언과 `domain_interface_state_binding.c` 제품 본체에서 동일 Rx 함수를 확인했다. **본체는 DOMAIN 담당**, Caller는 DOMAIN MCU 안 Network Adapter다. 이 문서는 Network의 호출 조건을 설계한다. 외부 본체의 내부 Manager 호출명이나 상태 변경 알고리즘을 새로 만들지 않는다.

| 인자/반환 | 계약 |
|---|---|
| state | IN `const CisRearState_t *`, NULL 불가. 전체 필드·단위·상태는 [Data](../data/CIS_REAR_DATA.md#meaning). 호출 동안 borrow, 이후 필요한 자료는 DOMAIN이 반환 전 복사 |
| 반환 | `DomainIf_Status_t`: OK, INVALID_ARGUMENT, NOT_INITIALIZED, NOT_AVAILABLE, QUEUE_FULL, REJECTED, UNSUPPORTED의 공통 의미. 공통 enum 목록이며 현재 RxCisRear가 모두 직접 반환한다는 뜻은 아님(Manager capacity 계열 실패는 NOT_AVAILABLE 매핑) |
| 상태·선후조건 | Network의 형식·원본·시간·품질 투영이 완료돼야 호출. DOMAIN은 공개 API 본체/기능 State를 소유. OK는 로컬 성공이며 같은 입력의 cache commit을 자동 증명하지 않음 |
| 실행 문맥 | DOMAIN 초기 Superloop의 단일 담당 문맥에서 직렬 처리, ISR 직접 호출 제외. 비차단·초기화·지원·실제 복사/내부 오류 원자성은 Owner 확인 대상 |
| Called | 현재 DOMAIN 본체는 `DomainIfStateBinding_Store()`→`VehicleStateManager_Update()`로 상태를 저장하는 코드가 확인됨. Network가 내부 Manager나 BoardRuntime을 직접 호출하지 않음 |

| 기존 C2 반환 | Network가 관측할 의미 |
|---|---|
| DOMAIN_IF_OK | 로컬 수신 경계 성공으로 VSM 중복도 포함. 신규 snapshot 반영은 Query 전후 원 입력·순번·품질 확인 필요 |
| DOMAIN_IF_INVALID_ARGUMENT | NULL·enum·필드 조합·범위 오류. 같은 잘못된 입력 재시도 금지 |
| DOMAIN_IF_NOT_INITIALIZED | C2 초기화 전. CAN 링크 상태와 구분 |
| DOMAIN_IF_NOT_AVAILABLE | 필요한 소비 경계 또는 현재 문맥이 준비되지 않음 |
| DOMAIN_IF_QUEUE_FULL | 공통 C2 enum. 현재 RxCisRear의 VSM capacity 계열 실패는 NOT_AVAILABLE로 매핑되므로 이 함수가 직접 QUEUE_FULL을 반환한다고 가정하지 않음. 무조건 재호출·덮어쓰기 금지 |
| DOMAIN_IF_REJECTED | DOMAIN이 의미상 거부. Network 형식 오류나 차량 FAILED로 바꾸지 않음 |
| DOMAIN_IF_UNSUPPORTED | 해당 인계 기능을 실제로 지원하지 않음. 선언 존재만으로 지원 확정 금지 |

현재 DOMAIN의 `DomainIf_RxCisRear()` 본체는 `DomainIfStateBinding_Store()`→`VehicleStateManager_Update()`를 호출하며, 별도의 CIS rear RxObserver는 호출하지 않는다. 따라서 제품 Core를 문서에서 재구현하지 않고 **Network가 소유한 호출 절차만 위 Adapter 의사코드로 기술**한다. `DOMAIN_IF_OK`에는 중복 관측도 포함될 수 있으며, 신규 저장 여부·위험 판단·VSS 실행은 Query/후속 기능 경로에서 별도 검증한다.

[M01](../modules/M01_SOURCE_CONTEXT.md) · [M03](../modules/M03_TYPED_HANDOFF.md) · [L02](../layers/L02_TYPED_HANDOFF.md) · [흐름](../flows/CIS_REAR_144_TO_344.md)
