# UART-M Frame·Stream 내부 자료

이 문서의 C 선언은 모두 `PROPOSED_PRIVATE_TYPE`이다. 제품 헤더·새 Wire 필드·공개 API·실제 Driver 버퍼 설정을 추가하지 않는다. 기존 `NetUartRxMeta`, `NetAppContext`, `NetDownContext`와 메시지 DTO는 [기존 자료 정본](UART_M_DATA.md)에 둔다.

[UART-M 개요](../05_UART_M_OVERVIEW.md) · [Frame/Stream 함수](../functions/UART_FRAME_STREAM.md) · [Router](../functions/UART_M_ROUTER.md) · [전송 제어](../functions/UART_M_TX_RX_CONTROL.md)

<a id="type-NetUartMResult"></a>
## 내부 처리 결과

```c
typedef enum NetUartMResult {
    NET_UART_M_OK, NET_UART_M_INVALID_ARGUMENT, NET_UART_M_MALFORMED,
    NET_UART_M_CONTRACT_HOLD, NET_UART_M_CONTEXT_HOLD,
    NET_UART_M_ORDER_HOLD, NET_UART_M_TIME_HOLD,
    NET_UART_M_NOT_READY, NET_UART_M_NO_CAPACITY, NET_UART_M_DUPLICATE,
    NET_UART_M_NEED_MORE, NET_UART_M_NO_WORK,
    NET_UART_M_DRIVER_PENDING, NET_UART_M_DRIVER_REJECTED,
    NET_UART_M_CONSUMER_REJECTED
} NetUartMResult;
```

| 값 | 해당 호출의 최소 의미 |
|---|---|
| OK | 그 함수의 로컬 계약 완료. 완전 Frame 생성/한 Frame 추출/typed 전달 시도 수용 등 함수별 의미만 보장 |
| INVALID_ARGUMENT | NULL·저장소 겹침·잘못된 내부 선택자·호출 계약 오류 |
| MALFORMED | Frame/typed 표현의 길이·형식·raw·필드 조합 오류 |
| CONTRACT_HOLD | profile·subtype·표현·Owner·실행 계약 미확인. 정상 기능 소비 금지 |
| CONTEXT_HOLD | 현재 peer/Device/Session/DOMAIN Boot 또는 로컬 연결 세대 근거 불일치/미확인 |
| ORDER_HOLD / TIME_HOLD | 순서/wrap/원 identity 전이 또는 AGE/시각 근거 미확인 |
| NOT_READY / NO_CAPACITY | 필수 소비/실행 준비 부족 또는 안정 사본 공간 부족 |
| DUPLICATE | 이미 처리한 동일 사실; 새로운 기능 인계·count·해제 없음 |
| NEED_MORE | Stream에서 완전한 정상 Frame 한 건을 아직 확보하지 못함. 입력 consumed와 부분 후보 상태는 별도로 확인 |
| NO_WORK | 처리할 의미 항목 없음; 송신 성공이 아님 |
| DRIVER_PENDING | 인계 여부 또는 최종 UART 전송 관측 미확정; 원 항목 식별 유지 |
| DRIVER_REJECTED | 해당 Driver 제출의 최종 거절 관측; 차량 FAILED가 아님 |
| CONSUMER_REJECTED | 기존 typed consumer의 non-OK. 원 반환값/반영 단계를 따로 보존 |

기존 NetSubmit/C2/Gateway enum으로 이 숫자를 캐스팅하지 않는다. [반환·후발 실패](../functions/UART_M_TX_RX_CONTROL.md#returns)를 따른다.

<a id="type-NetUartMDirection"></a>
```c
typedef enum NetUartMDirection {
    NET_UART_M_UPLINK, NET_UART_M_DOWNLINK
} NetUartMDirection;
```

UPLINK는 ESP32→DOMAIN, DOWNLINK는 DOMAIN→ESP32다. 수신한 값이 아니라 초기화/호출 경계에서 알고 있는 물리 링크 방향이다.

<a id="type-NetUartMProfile"></a>
```c
typedef enum NetUartMProfile {
    NET_UART_M_PROFILE_UNSELECTED,
    NET_UART_M_PROFILE_V2_CANDIDATE
} NetUartMProfile;
```

enum 선택은 양단 채택 근거가 아니다. M01은 DOMAIN·ESP32·Network의 실제 동일 revision 지원과 실행 전제를 확인한 근거를 따로 연결해야 한다. CF01의 v1 31/41B를 v2 38/48B로 자동 해석하지 않는다.

<a id="type-NetUartMFrame"></a>
```c
typedef struct NetUartMFrame {
    uint16_t length;
    uint32_t receivedAtMs;
    uint8_t bytes[138];
} NetUartMFrame;
```

length는 실제 확보한 Frame 길이이며 미수신 바이트를 0으로 채워 수용하지 않는다. bytes138은 후보 P≤128에 외형10을 더한 **문서 내부 논리 상한**이다. 실제 Queue/FIFO/DMA/ring 용량을 정하지 않는다. 등록 UART-M 최대는 P62/Frame72이고 P0/128 Type은 없다. TX에서 receivedAtMs는 의미 없는 내부 자리이며 새 수신시각/AGE를 만들지 않는다. RX에서는 마지막 Frame 바이트의 실제 로컬 수신 근거를 담는다. 처리 시각이나 원 노드 생성시각으로 대체하지 않는다.

<a id="type-NetUartMRegistryEntry"></a>
```c
typedef struct NetUartMRegistryEntry {
    uint8_t type;
    uint16_t payloadLength;
    NetUartMDirection direction;
} NetUartMRegistryEntry;
```

등록표는 [23종 Frame 정본](../functions/UART_FRAME_STREAM.md#registry)에 한 번 정의한다. 내부 type 바이트 값은 출처 그대로이며 새 type을 발급하지 않는다.

<a id="type-NetUartMStreamInput"></a>
```c
typedef struct NetUartMStreamInput {
    const uint8_t *bytes;
    const uint32_t *capturedAtMs;
    uint16_t length;
    uint32_t linkEpoch;
    uint32_t nowMs;
    bool elapsedUnambiguous;
    bool timerCheckPermitted;
} NetUartMStreamInput;
```

bytes/capturedAtMs는 호출 중 borrowed이며 length개의 byte와 각 byte의 확인된 로컬 수신시각을 연결한다. 동일 batch에 같은 시각을 쓰려면 그 값의 관측 의미와 조립시간·완료시각 상한을 실제 Driver에서 입증해야 한다. 없는 byte 시각을 임의로 Worker 현재시각으로 만들지 않는다. 이를 공급할 실제 Driver 메타/보수적 시간 바인딩은 OPEN이며 준비 전 TIME_HOLD다.

elapsedUnambiguous는 해당 로컬 시간원/간격의 wrap·불연속 처리가 확인되었다는 증거다. 임의 bool=true로 계약을 채택하지 않는다. timerCheckPermitted는 nowMs까지 이전 수신 byte가 미처리 Driver/Worker 저장소에 남아 있지 않아 조립 timer를 평가할 수 있다는 근거다. 오래 전에 제때 완성된 Frame을 Worker 지연만으로 조립 timeout 처리하지 않는다. length0으로 이미 보관한 suffix를 처리하거나 허용된 timer 확인을 수행할 수 있다.

<a id="type-NetUartMParser"></a>
```c
typedef struct NetUartMParser {
    uint8_t bytes[138];
    uint32_t capturedAtMs[138];
    uint16_t count;
    uint32_t linkEpoch;
    bool timePolicyBound;
    uint32_t assemblyLimitMs;
    bool expiresAtEquality;
    uint32_t malformedHeaders, badCrc, assemblyTimeouts, byteLosses;
} NetUartMParser;
```

M06 소유 지역 parser 하나의 내부 후보/진단 자료다. 원 Session·차량 상태·기능 cache가 아니다. count0부터 시작하며 연결별 한 소유자 문맥에서 직렬화한다. 배열의 실제 memory allocation/동기화·Driver transport와 배치는 미확인이다. 조립 제한50ms는 후보이고 timePolicyBound가 참이 되려면 기준점·clock·동등 비교(>/≥)·실제 채택 근거를 확인해야 한다. 각 suffix byte의 원 시각도 함께 이동시켜 재동기화할 때 기한을 새로 부여하지 않는다. counter는 u32 포화하며 정상 Frame/원격 재부팅만으로 초기화하지 않는다.

<a id="type-NetUartMPortOutcome"></a>
```c
typedef enum NetUartMPortOutcome {
    NET_UART_M_TX_UNKNOWN, NET_UART_M_TX_SUCCEEDED, NET_UART_M_TX_FAILED
} NetUartMPortOutcome;
```

<a id="type-NetUartMTxRecord"></a>
```c
typedef struct NetUartMTxRecord {
    uint64_t localToken;
    uint32_t localEpoch;
    bool acceptedKnown, accepted;
    bool driverReferenceOutstanding, bufferReturned, identityRetained;
    NetUartMPortOutcome outcome;
    bool queryItem;
    bool queryTerminalAccounted;
} NetUartMTxRecord;
```

<a id="type-NetUartMPortEvent"></a>
```c
typedef struct NetUartMPortEvent {
    uint64_t localToken;
    uint32_t localEpoch;
    bool admissionObserved, accepted, releaseObserved;
    NetUartMPortOutcome outcome;
} NetUartMPortEvent;
```

token/epoch는 로컬 제출 수명의 제안 식별자이며 Wire LINK_SEQUENCE·DOMAIN Boot·Session과 다르다. Query의 완전 문맥/원 request·source identity는 안정 소유 항목에 별도로 남긴다. record만 복사했다고 nested payload 전체를 확보한 것이 아니다. queryTerminalAccounted는 같은 terminal 관측을 두 번 집계하지 않기 위한 제안 상태이며 enqueue나 release 때 참이 되지 않는다. Query count의 실제 완료 바인딩은 [M02](../functions/UART_M_QUERY_CONTROL.md)를 따른다.

<a id="type-NetUartMRxComparison"></a>
```c
typedef struct NetUartMRxComparison {
    bool contextBound, sameContext, transitionAuthorized;
    bool hasAcceptedFrame, nonWrappingOrderKnown;
    uint16_t previousLinkSequence, incomingLinkSequence;
    bool rawExactlyEqual;
    bool sameImmutableInput, semanticExactlyEqual;
    bool functionalTransitionKnown, ageRelationAccepted;
} NetUartMRxComparison;
```

M04가 정상 형식과 원 문맥·기능 identity를 이미 검토한 뒤 만든 내부 비교 입력이다. sameImmutableInput은 같은 평가/관측·명령·occurrence·결과 단계의 원 입력을 뜻한다. 동일 commandId 안의 ACCEPTED→DONE은 다른 결과 단계이므로 Owner 전이 규칙을 확인한다. rawExactlyEqual에는 원 LINK_SEQUENCE와 raw AGE를 포함하고, 소비시점의 보수적 AGE는 분리한다. wrap 순서창은 이번에 확정하지 않는다.

<a id="type-NetUartMDecodedValue"></a>
## NetUartMDecodedValue / NetUartMDecoded

PROPOSED_PRIVATE_TYPE. typed dispatch 임시 저장소이며 Wire struct·공개 API·새 차량 cache가 아니다. type에 해당하는 union 멤버만 유효하고 방향/메타는 같은 입력에 묶인다. 성공 OUT은 호출자 소유, consumer가 지연 처리하면 전체 의미 사본을 확보한다. 제품 stack/정렬/capacity는 미확인이다.

```c
typedef union {
    NetMobileAvailability M_AVAILABILITY;
    NetMobileBcmClimateState M_BCM_CLIMATE_STATE;
    NetMobileBcmDoorState M_BCM_DOOR_STATE;
    NetMobileBcmLightState M_BCM_LIGHT_STATE;
    NetMobileBcmStatus M_BCM_STATUS;
    NetMobileCisEnvironment M_CIS_ENVIRONMENT;
    NetMobileCisOccupant M_CIS_OCCUPANT;
    NetMobileCisRear M_CIS_REAR;
    NetMobileCisStatus M_CIS_STATUS;
    NetMobileContext M_CONTEXT;
    NetMobileDigitalResult M_DIGITAL_RESULT;
    NetMobileDigitalStatus M_DIGITAL_STATUS;
    NetMobileDomainAlive M_DOMAIN_ALIVE;
    NetMobileQuery M_QUERY;
    NetMobileQueryEnd M_QUERY_END;
    NetMobileRequest M_REQUEST;
    NetMobileResult M_RESULT;
    NetMobileUserSettings M_USER_SETTINGS;
    NetMobileVssStatus M_VSS_STATUS;
    NetMobileWarning M_WARNING;
    NetMobileWarningAck M_WARNING_ACK;
    NetMobileWindowFault M_WINDOW_FAULT;
    NetMobileWindowState M_WINDOW_STATE;
} NetUartMDecodedValue;
```

<a id="type-NetUartMDecoded"></a>
```c
typedef struct {
    uint8_t type;
    NetUartMDirection direction;
    NetUartRxMeta meta;
    NetUartMDecodedValue value;
} NetUartMDecoded;
```
