# DOMAIN Adapter·공통 Service의 자료와 수명

[B15 개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [수명 제어](../functions/NETWORK_SERVICE_LIFETIME.md) · [실패·Health](../functions/NETWORK_FAILURE_HEALTH.md)

이 문서는 기존 M01/M03/M04/M07/M08/M09 자료 경계를 연결한다. 공개 C2·Wire 자료의 단일 선언은 아래 기존 정본에 있다. 여기서 정의하는 `NetSvc*`는 **PROPOSED_PRIVATE_FUNCTION에 쓰는 문서 내부 자료**이며 Header·제품 구현·새 공통 Manager가 아니다. 실제 저장 위치·Task·잠금·용량은 D-05/G06 바인딩 확인 전 정하지 않는다.

## 기존 자료의 단일 정본

| 자료 | 실제 선언과 역할 |
|---|---|
| `NetSubmitResult` | [CIS_REAR_DATA](CIS_REAR_DATA.md#type-NetSubmitResult). 네 가지 로컬 접수 결과. C2 정수 cast 금지 |
| `DomainIf_Status_t` | [DOMAIN 원 선언](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-status-t). C2 일곱 상태의 개별 지원·부분 반영은 API별 확인 |
| `DomainIf_TxPort_t` / `DomainIf_RxObserver_t` | [TxPort26](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-txport-t) / [Observer11](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-rxobserver-t). table 값복사와 callback/실행상태 수명은 별개 |
| C2 DTO·포인터·수명 | [DOMAIN Data](../../interface/02_NODES/DOMAIN/10_DATA.md), [Vehicle 타입](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md), [DOMAIN 타입](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md). 현재 v0.5 정본을 보존하며 후보 ABI를 섞지 않음 |
| `NetworkLocalFailure_t` 및 kind/reason | [CAN 공통 Data 정본](CAN_COMMON_DATA.md#type-NetworkLocalFailure_t), [원 C2 선언](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-networklocalfailure-t). 아래 내부 상관 자료는 이 공개 DTO를 대체하지 않음 |
| `NetworkLinkId_t`, `NetworkTransportState_t`, `NetworkOperationalCounters_t`, `NetworkHealthSnapshot_t` | [기존 Health 자료](CAN_COMMON_DATA.md#type-NetworkHealthSnapshot_t), [원 C2 전체 선언](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-networkhealthsnapshot-t) |
| CAN·UART 제출 및 수신 원 자료 | [CAN](CAN_COMMON_DATA.md), [UART-M](UART_M_DATA.md), [UART 공통 Frame](UART_FRAME_DATA.md), [UART-C](UART_C_DATA.md). 전체 Frame·meta·원 identity 사본을 함께 유지 |

기존 공개 원형을 반복 정의하거나 기존 타입에 포인터·새 generation 필드를 추가하지 않는다. Receiver용 `NetCanRxMeta`와 C2 `SignalMeta_t`는 같은 자료가 아니며, 원 CAN Boot/TX/원 AGE와 C2 Snapshot의 동일 관측 결합은 [Query 출처 경계](../functions/DOMAIN_QUERY_PROVENANCE.md)에서 따로 확인한다.

<a id="evidence-boundary"></a>
## 설계 근거와 실행 중 자료의 차이

| 구분 | 누가 무엇으로 확인하는가 | 확인 전 처리 |
|---|---|---|
| 제품 바인딩 계약 | DOMAIN/Network/Driver Owner가 실제 등록·실행 문맥·clock·callback identity·참조 종료 의미·필수 sink 원자성을 연결한 근거 | 해당 외부 호출 경로 BINDING_OPEN. 단순 `ready=true` 입력으로 이 절차를 대체하지 않음 |
| 실행 중 수명 자료 | 위 바인딩이 있는 경로에서 M04 접수 기록, M08 실제 장치 사건, M07 회수 완료, M03/M02 소비 기록으로 생성 | 실제 관측이 없는 상태는 아래 UNKNOWN/POSSIBLE/WAITING 유지 |
| 순수 내부 판정 | 같은 소유자 문맥에서 잠금/직렬화된 자료 사본을 검증하여 다음 허용 행동만 계산 | 판정 OK는 메모리 회수·전송·Core commit 실행 완료가 아님 |

아래 enum은 runtime에서 관측 결과를 기록하는 **내부 제안 값**이다. 외부 합의의 존재 여부를 임의로 바꾸는 공개 API 인자가 아니다. 원 사건 출처·담당자·미결은 각 함수의 선행 계약 표로 역추적한다. 숫자상 bool 대신 enum을 사용해도 근거 없는 상태 대입이 허용되는 것은 아니다.

<a id="type-NetSvcResult"></a>
```c
typedef enum NetSvcResult {
    NET_SVC_OK, NET_SVC_INVALID_ARGUMENT, NET_SVC_NOT_READY,
    NET_SVC_TIME_HOLD, NET_SVC_EVIDENCE_HOLD, NET_SVC_DUPLICATE
} NetSvcResult;
```

`NetSvcResult`는 내부 판정 결과다. `DomainIf_Status_t` 또는 `NetSubmitResult`로 정수 변환하지 않는다. 호출자가 기존 공개 반환을 정해야 할 때는 [TxPort 명시 변환](../functions/DOMAIN_TX_PORT.md)의 기존 표만 사용한다. 새 내부 HOLD를 공개 enum에 추가하지 않는다.

<a id="type-NetSvcLifetimeView"></a>
<a id="type-NetSvcRetirement"></a>
```c
typedef enum NetSvcReference {
    NET_SVC_REFERENCE_NEVER_EXPOSED,
    NET_SVC_REFERENCE_POSSIBLE,
    NET_SVC_REFERENCE_ENDED
} NetSvcReference;
typedef enum NetSvcTerminal {
    NET_SVC_TERMINAL_UNKNOWN,
    NET_SVC_TERMINAL_TX_SUCCEEDED,
    NET_SVC_TERMINAL_TX_FAILED,
    NET_SVC_TERMINAL_STOPPED_BEFORE_SEND
} NetSvcTerminal;
typedef enum NetSvcResultConsumer {
    NET_SVC_CONSUMER_NOT_REQUIRED,
    NET_SVC_CONSUMER_WAITING,
    NET_SVC_CONSUMER_CONFIRMED
} NetSvcResultConsumer;
typedef enum NetSvcReservation {
    NET_SVC_SOURCE_RESERVED,
    NET_SVC_SOURCE_RECONCILED
} NetSvcReservation;
typedef struct NetSvcLifetimeView {
    uint64_t localToken;
    uint32_t localEpoch;
    NetSvcReference reference;
    NetSvcTerminal terminal;
    NetSvcResultConsumer consumer;
    NetSvcReservation reservation;
    bool frameStorageAcquired;
    bool frameMemoryReturned;
} NetSvcLifetimeView;
typedef struct NetSvcRetirement {
    bool mayReturnFrameMemory;
    bool mayEndSourceReservation;
    bool mayRetireIdentity;
} NetSvcRetirement;
```

| 전체 멤버 | 생성·수명·의미 |
|---|---|
| `localToken`, `localEpoch` | M07이 Driver 호출 전에 준비한 로컬 시도 식별. 실제 CAN/UART record의 동일 키를 옮긴 읽기용 사본. commandId·원 Generation·LinkSequence 대신 사용하지 않음 |
| `reference` | 최초 NEVER_EXPOSED. 실제 Driver 호출 직전 POSSIBLE. 실제 참조 종료/참조 미취득의 계약 증거가 있어야 ENDED. accepted=false·timeout·reset만으로 ENDED 금지 |
| `terminal` | 해당 시도의 최종 관측. 송신 성공·실패·증명된 송신 전 중단을 구분. callback 부재는 UNKNOWN |
| `consumer` | Query는 M02 최종 TX 집계, 후발 실패 대상은 M03 필수 귀속, 다른 항목은 해당 결과 소비 기록. 필수 여부는 접수 시 원 항목별로 고정. 전달 불명확을 NOT_REQUIRED로 바꾸지 않음 |
| `reservation` | M04의 해당 원 관측/개별 항목 시도 예약. 결과·후속 인계가 처리된 실제 원자 반영 뒤 RECONCILED. 버퍼 release는 이 상태를 바꾸지 않음 |
| `frameStorageAcquired` | 이 접수 항목/시도가 Frame 저장소를 실제 취득했는지의 이력. 취득 뒤 반환해도 true 유지. 의미 Queue에서 송신 전 중단되어 아직 취득하지 않았다면 false |
| `frameMemoryReturned` | 취득한 M07 저장소 반환 작업이 실제 완료됐다는 기록. 반환 가능성 판정과 구별. 미취득 항목에서 true로 합성하지 않음 |
| `NetSvcRetirement` 세 bool | 함수의 계산 결과. 실제 회수/예약 종료/identity 제거는 해당 Owner가 별도로 직렬 처리한다. 한 값으로 세 사건을 합치지 않음 |

이 view에는 payload pointer가 없다. view를 복사했다고 caller의 원 DTO, `ByteView.data`, Driver frame 또는 callback table 뒤 실행상태까지 복사한 것은 아니다. M04/M07의 실제 저장소와 원 identity는 기존 프로토콜 문서가 정본이다.

<a id="type-NetSvcClockStamp"></a>
```c
typedef struct NetSvcClockStamp {
    uint32_t localClockEpoch;
    uint64_t monotonicMs;
} NetSvcClockStamp;
```

M08의 실제 로컬 시간 바인딩이 ms 단위·wrap·중단·재시작을 판별해 만든 **내부 표준화 관측**이다. 64비트 타이머가 제품에 존재한다고 선언하지 않으며, u32 `nowMs`를 단순히 u64로 cast해 과거 wrap을 복구하지 않는다. 관측점을 만들 수 없으면 시간 의존 경로를 호출하지 않는다. 서로 다른 노드·clockEpoch·미측정 홉의 차감은 금지한다. 공개 C2의 `uint32_t nowMs` 원형은 그대로다.

<a id="type-NetSvcCorrelation"></a>
<a id="type-NetSvcFailureEvidence"></a>
```c
typedef struct NetSvcCorrelation {
    NetworkLinkId_t link;
    NetworkLocalItemKind_t itemKind;
    SourceGeneration_t identityGeneration;
    bool hasCommandId;
    DomainCommandId_t commandId;
    bool hasRequestContext;
    DeviceContextId_t requestDeviceContextId;
    RequestContext_t requestContext;
    bool hasOccurrenceId;
    OccurrenceId_t occurrenceId;
} NetSvcCorrelation;
typedef enum NetSvcFailureTrigger {
    NET_SVC_FINAL_SUBMISSION_FAILURE,
    NET_SVC_EXPIRED_BEFORE_SEND,
    NET_SVC_CONTEXT_INVALID_BEFORE_SEND,
    NET_SVC_OTHER_OR_UNRESOLVED_ERROR
} NetSvcFailureTrigger;
typedef enum NetSvcSendPhase {
    NET_SVC_SEND_NOT_STARTED,
    NET_SVC_SEND_STARTED,
    NET_SVC_SEND_START_UNKNOWN
} NetSvcSendPhase;
typedef struct NetSvcFailureEvidence {
    uint64_t localToken;
    uint32_t localEpoch;
    NetSvcCorrelation original;
    NetSvcClockStamp admittedAt;
    NetSvcClockStamp observedAt;
    NetSvcFailureTrigger trigger;
    NetSvcSendPhase sendPhase;
} NetSvcFailureEvidence;
```

상관 자료는 **로컬 OK가 실제 반환된 항목**의 M04 원 접수 기록에서만 만든다. original의 각 필드는 기존 `NetworkLocalFailure_t`의 같은 필드와 같은 타입이며 위 C2 정본의 존재 플래그/생산자 의미를 따른다. 포인터가 없는 `RequestContext_t`는 기존 Session/Request 값 전체를 복사한다. flag=false인 선택 멤버는 읽지 않는다. Generation0의 채택을 함수가 판단하지 않으며, 원 세대 수용 근거가 없는 항목은 접수 단계에서 제한한다. 값 0을 새 generation으로 채우거나 무조건 예약값으로 정하지 않는다.

COMMAND는 원 commandId, REQUEST_RESULT는 원 Device/RequestContext, OCCURRENCE는 원 occurrenceId가 필수다. command가 모바일 요청에서 유래했다면 해당 RequestContext를 함께 유지하고 정책 명령에는 가짜 Request를 만들지 않는다. `identityGeneration`은 명령/결과발행/사건 생산자의 **접수 당시 세대**이며 대상 ECU Boot·Request Session·현재 App Session과 다르다.

`trigger`는 정확한 원인 분류를 제공하는 기존 M04/M08 사건에서 생성한다. Driver outcome FAILED라는 한 값만으로 FINAL_SUBMISSION_FAILURE를 만들지 않는다. 송신 중 오류를 이 reason으로 확장할 근거가 없으면 OTHER_OR_UNRESOLVED_ERROR다. `sendPhase`는 실제 송신 시작 여부의 관측이며 DOMAIN의 DISPATCHED 상태를 복사한 값이 아니다. token/epoch와 원 접수 기록의 결합이 확인된 사본만 함수에 전달한다.

<a id="type-NetSvcFailureDelivery"></a>
```c
typedef enum NetSvcDeliveryState {
    NET_SVC_DELIVERY_UNSENT,
    NET_SVC_DELIVERY_CALL_IN_PROGRESS,
    NET_SVC_DELIVERY_BOUNDARY_OK,
    NET_SVC_DELIVERY_UNRESOLVED,
    NET_SVC_DELIVERY_CORE_CONFIRMED
} NetSvcDeliveryState;
typedef struct NetSvcFailureDelivery {
    uint64_t localToken;
    uint32_t localEpoch;
    NetworkLocalFailure_t message;
    NetSvcDeliveryState state;
    bool hasC2Return;
    DomainIf_Status_t lastC2Return;
} NetSvcFailureDelivery;
```

message는 MakeLocalFailure 성공 OUT을 안정 복사한 최종 객체다. kind/reason/원 identity는 첫 호출 뒤 불변이다. C2 원 반환을 받기 전 `lastC2Return`은 읽지 않는다. BOUNDARY_OK는 공개 함수 반환의 로컬 성공이며 같은 자료의 Core commit·필수 Owner 수용과 연결됐다는 실제 증거는 별도다. optional Observer 실패만 확인된 경우 이미 완료한 Core 입력을 재호출하지 않는다. CORE_CONFIRMED 전이의 실제 증거 API/처리 시점은 Owner 바인딩 OPEN이며, 새 공개 receipt API를 만들지 않는다.

<a id="type-NetSvcHealthView"></a>
<a id="type-NetSvcCounterEvent"></a>
```c
typedef struct NetSvcHealthView {
    NetworkLinkId_t link;
    NetworkTransportState_t transportState;
    bool hasValidRx;
    NetSvcClockStamp lastValidRx;
    NetworkOperationalCounters_t counters;
} NetSvcHealthView;
typedef enum NetSvcCounterEvent {
    NET_SVC_COUNT_CRC_REJECT,
    NET_SVC_COUNT_FORMAT_REJECT,
    NET_SVC_COUNT_RX_OVERFLOW,
    NET_SVC_COUNT_TX_FINAL_FAILURE,
    NET_SVC_COUNT_CAN_BUS_OFF
} NetSvcCounterEvent;
```

HealthView는 기존 M09 관측의 **조회 시 일관된 사본**이며 새 차량 상태 cache가 아니다. transportState는 실제 로컬 Driver/link 관측으로만 정한다. valid Rx는 해당 링크의 기존 admission 정의를 만족한 관측이며 malformed로 갱신하지 않는다. 같은 관측의 반복 수신은 원 source AGE 기준시각을 갱신하지 않는다. 반면 exact duplicate가 링크 lastValidRx를 갱신하는지는 기존 메시지별 정상 수신 계약으로 별도 바인딩하며, 확인 전에는 OPEN으로 보존한다. 품질 INVALID/NO_DATA가 raw 형식상 유효한 보고이면 링크 수신과 기능 품질을 분리한다. lastValidRx는 hasValidRx=true에서만 읽는다. counters 다섯 필드는 원 C2 u32 포화 의미를 유지한다.

CounterEvent에는 원 사건 dedup ID를 새 Wire로 넣지 않는다. M09 Owner는 기존 파서 사건/Driver token·epoch/overflow·bus-off 관측을 한 번만 집계했다는 journal을 **동일 원자 처리**로 유지해야 한다. 그 식별·증가 단위·중복 제거가 바인딩되지 않았으면 카운터를 추정 증가시키지 않는다. CAN 소프트웨어가 UART CRC를 검사하는 것처럼 가정하거나 UART에 bus-off를 만들지 않는다.

## 역할별 인자 수명

| 경계 | 입력·OUT 수명 | 성공 뒤에도 남길 자료 |
|---|---|---|
| Lifetime 판정 | const view는 호출 동안 borrowed, OUT은 caller-owned·성공 때만 갱신 | 실제 M04/M07 원장과 frame/identity |
| Failure 생성 | 원 접수/원인/시각의 같은 사건 사본, OUT은 전 검사 후 일괄 반영 | 원 correlation과 token/epoch·정확 reason 근거 |
| Failure C2 호출 | delivery가 소유한 message를 callback 반환까지 const borrow | 원 C2 반환·Core/Observer 단계가 불명확하면 전체 identity |
| Health 내부 조회 | M09가 하나의 직렬 처리에서 읽은 view와 로컬 now, OUT 일괄 반영 | 원 수신시각·포화 counter, 조회 자체로 reset하지 않음 |

금지된 추가: 공개 Header·Wire·새 TxTicket·전역 singleton·차량 Manager cache·마법의 Queue 깊이·확정되지 않은 timeout/retry 수. 기존 외부 HOLD와 [S6 이후 통합 방침](../07_DOMAIN_ADAPTER_SERVICE.md#holds)을 유지한다.
