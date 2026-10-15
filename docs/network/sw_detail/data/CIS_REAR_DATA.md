# CIS_REAR — Data·Wire·시간의 단일 정의

CIS가 관측한 후방 거리와 품질을 CAN으로 운송하고 DOMAIN의 typed 입력에 연결한다. 이 문서는 본 수직 흐름에 쓰는 자료 선언의 단일 정본이다. 구조체 메모리를 Wire로 복사하지 않는다. 기존 선언은 S4A, 바이트 배치는 최신 B07 Astra, 시간·수명은 S4C를 따른다.

[전체 흐름](../flows/CIS_REAR_144_TO_344.md) · [Codec](../functions/CIS_REAR_CODEC.md) · [M05](../modules/M05_CAN_CODEC.md) · [Network](../00_START_HERE.md)

<a id="declared"></a>
## 기존 선언과 선언 상태

아래 선언은 S4A Data 정본을 그대로 인용한다. Net 타입은 `PROPOSED_DESIGN/BINDING_OPEN`, C2 타입은 외부 제공 선언 `DECLARED_SOURCE`다. 제품 헤더·ABI·컴파일·보드 검증을 뜻하지 않는다. C2 v0.6/Vehicle v0.3은 미승인 후보이며 이 흐름의 CisRearState_t/API 원형을 변경하지 않는다.

<a id="type-NetSubmitResult"></a>
### `NetSubmitResult`

```c
typedef enum NetSubmitResult
{
    NET_SUBMIT_OK = 0,             /**< 필요한 필드의 복사·로컬 보관을 완료했다. */
    NET_SUBMIT_INVALID_ARGUMENT = 1,    /**< NULL, 예약 코드, 필드 조합 또는 범위가 유효하지 않다. */
    NET_SUBMIT_NOT_READY = 2,           /**< 링크·현재 문맥이 이 제출을 수용할 준비가 되지 않았다. */
    NET_SUBMIT_NO_CAPACITY = 3      /**< 필요한 로컬 보관 공간이 없다. 기존 개별 항목을 덮어쓰지 않는다. */
} NetSubmitResult;
```

<a id="type-NetCanSource"></a>
### `NetCanSource`

```c
typedef struct NetCanSource
{
    uint32_t boot_id;       /**< 송신 노드 기동 번호. 0 예약. */
    uint32_t tx_sequence;   /**< 같은 메시지의 전송 순번. 새 관측 번호가 아니다. */
} NetCanSource;
```

<a id="type-NetCanRxMeta"></a>
### `NetCanRxMeta`

```c
typedef struct NetCanRxMeta
{
    NetCanSource source;
    uint32_t received_at_ms; /**< 로컬 단조 증가 ms. 노드 간 시계 비교에 사용하지 않는다. */
} NetCanRxMeta;
```

<a id="type-NetValidity"></a>
### `NetValidity`

```c
typedef uint8_t NetValidity;
```

<a id="type-NetValidityCode"></a>
### `NetValidityCode`

```c
enum NetValidityCode
{
    NET_VALIDITY_INVALID = 0,
    NET_VALIDITY_VALID = 1
};
```

<a id="type-NetQualityReason"></a>
### `NetQualityReason`

```c
typedef uint8_t NetQualityReason;
```

<a id="type-NetQualityReasonCode"></a>
### `NetQualityReasonCode`

```c
enum NetQualityReasonCode
{
    NET_QUALITY_REASON_NONE = 0,
    NET_QUALITY_REASON_NOT_READY = 1,
    NET_QUALITY_REASON_OUT_OF_RANGE = 2,
    NET_QUALITY_REASON_SENSOR_FAULT = 3,
    NET_QUALITY_REASON_VISION_FAULT = 4,
    NET_QUALITY_REASON_STALE = 5,
    NET_QUALITY_REASON_NO_DATA = 6,
    NET_QUALITY_REASON_COMMUNICATION_FAULT = 7,
    NET_QUALITY_REASON_RECOVERING = 8,
    NET_QUALITY_REASON_DATA_INVALID = 9
};
```

<a id="type-NetProximityStatus"></a>
### `NetProximityStatus`

```c
typedef uint8_t NetProximityStatus;
```

<a id="type-NetProximityStatusCode"></a>
### `NetProximityStatusCode`

```c
enum NetProximityStatusCode
{
    NET_PROXIMITY_VALID_DISTANCE = 0,
    NET_PROXIMITY_NO_OBJECT = 1,
    NET_PROXIMITY_UNAVAILABLE = 2,
    NET_PROXIMITY_FAULT = 3,
    NET_PROXIMITY_RECOVERING = 4,
    NET_PROXIMITY_INACTIVE = 5
};
```

<a id="type-NetCisRear"></a>
### `NetCisRear`

```c
typedef struct NetCisRear
{
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 새 거리 관측 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 거리 원본 관측 후 ms */
    uint16_t rear_distance_deci_cm; /**< REAR_DISTANCE: 0.1cm/raw; 65535=거리 없음/무효, 상태로 구분 */
    NetValidity validity; /**< VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason quality_reason; /**< QUALITY_REASON: CIS 품질 사유 */
    NetProximityStatus proximity_status; /**< PROXIMITY_STATUS: 0=VALID_DISTANCE; 1=NO_OBJECT; 2=UNAVAILABLE; 3=FAULT; 4=RECOVERING; 5=INACTIVE */
} NetCisRear;
```

<a id="type-UpdateSequence_t"></a>
### `UpdateSequence_t`

```c
typedef uint32_t UpdateSequence_t;
```

<a id="type-SourceGeneration_t"></a>
### `SourceGeneration_t`

```c
typedef uint32_t SourceGeneration_t;
```

<a id="type-DataQuality_t"></a>
### `DataQuality_t`

```c
typedef enum
{
    DATA_QUALITY_OK = 0,
    DATA_QUALITY_STALE,
    DATA_QUALITY_INVALID,
    DATA_QUALITY_NO_DATA
} DataQuality_t;
```

<a id="type-ValueValidity_t"></a>
### `ValueValidity_t`

```c
typedef enum
{
    VALUE_VALIDITY_INVALID = 0,
    VALUE_VALIDITY_VALID
} ValueValidity_t;
```

<a id="type-QualityReason_t"></a>
### `QualityReason_t`

```c
typedef enum
{
    QUALITY_REASON_NONE = 0,
    QUALITY_REASON_NOT_READY,
    QUALITY_REASON_OUT_OF_RANGE,
    QUALITY_REASON_SENSOR_FAULT,
    QUALITY_REASON_VISION_FAULT,
    QUALITY_REASON_STALE,
    QUALITY_REASON_NO_DATA,
    QUALITY_REASON_COMMUNICATION_FAULT,
    QUALITY_REASON_RECOVERING,
    QUALITY_REASON_DATA_INVALID
} QualityReason_t;
```

<a id="type-SignalMeta_t"></a>
### `SignalMeta_t`

```c
typedef struct
{
    DataQuality_t quality;
    uint32_t ageMs;
    UpdateSequence_t updateSequence;
} SignalMeta_t;
```

<a id="type-ValueQuality_t"></a>
### `ValueQuality_t`

```c
typedef struct
{
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ValueQuality_t;
```

<a id="type-RearMeasurementState_t"></a>
### `RearMeasurementState_t`

```c
typedef enum
{
    REAR_MEASUREMENT_VALID_DISTANCE = 0,
    REAR_MEASUREMENT_NO_OBJECT,
    REAR_MEASUREMENT_UNAVAILABLE,
    REAR_MEASUREMENT_FAULT,
    REAR_MEASUREMENT_RECOVERING,
    REAR_MEASUREMENT_INACTIVE
} RearMeasurementState_t;
```

<a id="type-CisRearState_t"></a>
### `CisRearState_t`

```c
typedef struct
{
    uint16_t rearDistanceMm; /* exact 0.1 cm input; no integer-cm truncation */
    RearMeasurementState_t measurementState;
    ValueQuality_t distanceQuality;
    SourceGeneration_t sourceGeneration; /* admitted original CIS lineage */
} CisRearState_t;
```

<a id="type-DomainIf_Status_t"></a>
### `DomainIf_Status_t`

```c
typedef enum
{
    DOMAIN_IF_OK = 0,
    DOMAIN_IF_INVALID_ARGUMENT,
    DOMAIN_IF_NOT_INITIALIZED,
    DOMAIN_IF_NOT_AVAILABLE,
    DOMAIN_IF_QUEUE_FULL,
    DOMAIN_IF_REJECTED,
    DOMAIN_IF_UNSUPPORTED
} DomainIf_Status_t;
```

<a id="proximity"></a>
### NetProximityStatus의 폭과 코드

`NetProximityStatus`는 CIS 선언의 `uint8_t` 별칭이다. 0~5는 각각 `NET_PROXIMITY_VALID_DISTANCE`, `NET_PROXIMITY_NO_OBJECT`, `NET_PROXIMITY_UNAVAILABLE`, `NET_PROXIMITY_FAULT`, `NET_PROXIMITY_RECOVERING`, `NET_PROXIMITY_INACTIVE`다. 이 이름과 동일 의미의 `RearMeasurementState_t` 상수로 명시 대응한다. 코드가 우연히 같아도 C enum을 Wire로 캐스팅하지 않는다.

<a id="wire"></a>
## Wire 배치와 프레임 조건

방향은 **CIS/S32K144 → DOMAIN/S32K344**, 메시지는 `WIRE-P01-CIS_REAR`, 수신 의무는 `RX-P01-CIS_REAR-DOMAIN`이다. 메시지 이름·방향을 보존하며 다음 수치와 프로파일은 양단 미승인 Wire 후보다.

| 항목 | 후보 값·검증 조건 |
|---|---|
| ID/형식 | 표준 11-bit `0x0A0`, FDF=1, BRS=1, RTR 미사용 |
| 비트 전송률 | nominal 500 kbps / data 2 Mbps. 보드 타이밍·종단·트랜시버 미검증 |
| DLC/물리 길이/사용 길이 | DLC 코드 **11**, 실제 데이터 **20B**, 의미 사용 **19B** |
| Padding | B19=0, 한 바이트. 누락을 0으로 채워 수신하지 않음 |
| CRC | CAN FD 장치 자체 CRC/오류 검출. **응용 payload CRC 필드 없음** |
| 버전 | **CIS_REAR payload VERSION 필드 없음**. 배포 시 양단이 같은 후보 profile을 선택해야 함 |
| 분절 | 이 메시지는 단일 CAN FD 프레임. UART stream 재조립·SYNC·추가 fragment header를 적용하지 않음 |

| Offset/폭 | 필드 | typed 연결 | 허용 값·단위 |
|---|---|---|---|
| 0/4 | BOOT_ID | meta.source.boot_id | u32LE, 송신 CIS Network 기동, 0 예약 |
| 4/4 | TX_SEQUENCE | meta.source.tx_sequence | u32LE, 프레임 순번, 최초1 후보, wrap 수용 미합의 |
| 8/4 | UPDATE_SEQUENCE | message.update_sequence | u32LE, 새 거리 관측 번호. 송신 때 재발급 금지 |
| 12/2 | SOURCE_AGE | message.source_age_ms | u16LE, 원 관측 이후 ms, 65535는 포화 하한 |
| 14/2 | REAR_DISTANCE | message.rear_distance_deci_cm | u16LE, raw 0.1cm=1mm. 65535는 거리 없음/무효 |
| 16/1 | VALIDITY | message.validity | u8, INVALID0/VALID1 |
| 17/1 | QUALITY_REASON | message.quality_reason | u8, 정의된 0~9만 |
| 18/1 | PROXIMITY_STATUS | message.proximity_status | u8, 정의된 0~5만 |
| 19/1 | padding | typed 멤버 없음 | 0만 |

LE는 각 고정 offset에서 바이트를 조합한다는 뜻이다. C enum 폭, struct padding, `sizeof(NetCisRear)`는 위 길이의 근거가 아니다. Driver가 거부한 CRC 오류 프레임은 정상 Codec 입력으로 되살리지 않는다. CRC reject 수를 기록할 때도 장치가 실제 알려준 오류 단위를 사용한다.

<a id="meaning"></a>
## 값·품질·원본 식별

| 자료/멤버 | 생성·소비와 범위 | 수명·특이값 |
|---|---|---|
| NetCisRear 여섯 멤버 | CIS 초음파 담당이 값·validity·reason·status·원 update·원 AGE 생성 | 호출 동안 borrow, 접수 OK 전에 모두 Network 사본으로 확보 |
| NetCanSource 두 멤버 | CIS Network 송신 기동 및 프레임 순번; DOMAIN Network 수신 메타 | UPDATE_SEQUENCE 또는 DOMAIN 로컬 Boot로 교체 금지 |
| NetCanRxMeta.received_at_ms | DOMAIN의 실제 동일 프레임 수신 시각, u32 단조 ms | callback 시각으로 재설정 금지, 노드 간 시계 차감 금지 |
| CisRearState_t.rearDistanceMm | raw 정수를 그대로 mm로 옮김 | **raw123 → 123mm**, `/10` 뒤 `*10` 금지. 65535는 실측값 아님 |
| measurementState | 위 6개 의미를 이름으로 대응 | NO_OBJECT는 유효한 무검출일 수 있음. FAULT를 NO_OBJECT로 바꾸지 않음 |
| distanceQuality.validity/reason | 선언된 같은 의미의 C2 상수로 명시 대응 | 숫자 cast와 정상값 대체 금지 |
| distanceQuality.meta.updateSequence | 원 UPDATE_SEQUENCE | TX_SEQUENCE를 넣지 않음 |
| distanceQuality.meta.ageMs | 같은 소비 시점까지 누적한 원 AGE, u32 ms | 원 u16 포화는 정확한 나이로 복원 불가 |
| distanceQuality.meta.quality | 원 validity/reason/status와 시간 근거에서 투영 | Wire에 독립 DATA_QUALITY byte가 없음. 아래 품질 대응의 팀 바인딩 필요 |
| sourceGeneration | 수용이 확인된 원 CIS 생산 계보, u32 | BOOT_ID와 같은 수명인지 확인 후 연결. DOMAIN Boot 대입 금지 |

`VALID_DISTANCE + 65535`는 모순이므로 거부한다. 정상 유효거리의 표현 범위는 0~65534raw이며 실제 센서 최대 범위·차량 위험 임계값을 뜻하지 않는다. `NO_OBJECT + VALID + NONE + 65535`는 무검출 보고 예시다. FAULT/UNAVAILABLE/RECOVERING/INACTIVE의 65535는 해당 상태와 품질을 유지한다. 원 출처에 없는 모든 조합을 임의로 유효/무효로 확정하지 않는다.

**품질 투영 제안:** VALID+NONE이며 상태·거리 조합이 확인된 관측은 OK, 사유 STALE은 STALE, 사유 NO_DATA는 NO_DATA, 나머지 확인된 INVALID 보고는 INVALID에 대응시킨다. NONE·VALID 이외의 미합의 조합, 상충하는 품질 근거는 기본 OK로 채우지 않는다. 이 대응은 `CIS_QUALITY_PROJECTION`의 적용 조건이며 팀 채택·실제 연결은 미확인이다. 명시 상수 대응은 기존 enum 의미를 보존하되 `meta.quality`의 합성 규칙 승인을 대신하지 않는다.

Generation 0은 원 생산 세대 미확인이라는 최신 **제안**이며 NO_DATA와 자동 동치가 아니다. 원 Boot 수용, 세대 lifecycle, 품질 대응, 소비 시각 연결이 없으면 해당 C2 투영을 보류한다. raw 진단 보존은 기능 정상 인계와 구분한다. 관련 기존 미결은 IF-O09/IF-O24/D-02/Q03/C2-H01이다.

<a id="age"></a>
## AGE·순서·수신 중단

TX 접수 때의 AGE + 미반영 CIS 대기 + 아직 반영하지 않은 전달 경과/여유 + DOMAIN 수신 이후 대기를 각 한 번만 계산한다. 계산은 충분히 넓은 임시 값에서 하고 u16 Wire 출력은 65535에 포화한다. Driver가 프레임을 참조하기 시작한 뒤 바이트는 불변이다. 그 뒤 미측정 Driver 지연을 0으로 간주하지 않는다. CAN 전달 여유 5ms는 제안이며 실제 상한이 아니다.

같은 원 관측의 기존/신규 AGE는 같은 DOMAIN 소비 시점까지 각각 누적한 후 더 오래된 근거를 보존한다. 예를 들어 수신100ms/AGE20ms와 수신130ms/AGE15ms를 now150ms에서 비교하면 70ms와35ms이므로 35ms로 젊게 만들지 않는다. 포화된65535도 fresh 증거가 아니다. 같은 시계 epoch·wrap·관측간격을 입증하지 못하면 뺄셈 결과만으로 경과를 확정하지 않는다.

CIS_REAR 보고주기50ms/정상수신 중단150ms는 원 후보다. source TTL과 다르며 `>`/`>=`, 타이머 시작/취소, 실제 주기와 장치 지연은 미검증이다. 잘못된 프레임은 정상 liveness를 갱신하지 않는다. 같은 오래된 관측이 반복되어도 기능 freshness는 복원되지 않는다.

<a id="private"></a>
## 새 내부 함수에 필요한 최소 자료 후보

다음 세 선언만 B11 의사코드의 새 내부 자료다. 모두 `PROPOSED_PRIVATE_TYPE`이며 제품 헤더/API/ABI 또는 새 Wire가 아니다. 초기화·용량·동기화·드라이버 연결이 승인됐다는 뜻도 아니다. 나머지 상태는 [M04](../modules/M04_MESSAGE_SERVICE.md)·[M07](../modules/M07_BUFFER_OUTCOME.md)·[M08](../modules/M08_DEVICE_PORT.md)의 역할로 설명하며 별도 차량 상태 cache를 만들지 않는다.

```c
typedef enum NetRearResult {
    NET_REAR_OK, NET_REAR_INVALID_ARGUMENT, NET_REAR_MALFORMED,
    NET_REAR_NOT_READY, NET_REAR_NO_CAPACITY, NET_REAR_NO_WORK,
    NET_REAR_DUPLICATE, NET_REAR_ORDER_HOLD, NET_REAR_TIME_HOLD,
    NET_REAR_DRIVER_PENDING, NET_REAR_DRIVER_REJECTED,
    NET_REAR_CONSUMER_REJECTED
} NetRearResult;

typedef struct NetRearFrame {
    uint32_t canId;
    uint8_t dlc;
    uint8_t length;
    bool extended;
    bool fdf;
    bool brs;
    bool rtr;
    bool driverError;
    uint32_t receivedAtMs;
    uint8_t data[20];
} NetRearFrame;

typedef enum NetRearPortEvent {
    NET_REAR_PORT_TX_COMPLETED, NET_REAR_PORT_TX_FINAL_FAILED,
    NET_REAR_PORT_REFERENCE_RELEASED, NET_REAR_PORT_BUS_OFF,
    NET_REAR_PORT_RX_OVERFLOW, NET_REAR_PORT_RECOVERED,
    NET_REAR_PORT_LOCAL_RESET
} NetRearPortEvent;
```

| 후보 자료 | 의미·수명 |
|---|---|
| NetRearResult | 함수별 표의 로컬 처리 결과. Wire/C2 반환값이나 차량 결과가 아님 |
| NetRearFrame.canId/dlc/length | ID와 DLC코드, 실제 확보 byte수. 이 rear 전용 사본에서 length20만 정상 |
| extended/fdf/brs/rtr | Driver가 실제 관측한 flags. RX에서 임의로 맞춰 쓰지 않음 |
| driverError | CRC 등 장치 오류 관측이 연결된 입력. 오류 종류를 이 bool만으로 합성하지 않음 |
| receivedAtMs | RX 동일 노드 실제 수신시각; TX에서는 의미 없고 0 초기화 가능 |
| data[20] | rear 전용 완전 사본. 범용 CAN 64B buffer 선언이 아님. M07은 복사 전에 실제 길이를 확인하고20B를 넘는 입력을 이 배열에 복사하지 않음 |
| NetRearPortEvent | 실제 장치 사실을 Owner 문맥에 인계할 내부 분류. HAL 반환 숫자 직접 cast 금지 |

모든 포인터는 별도 명시가 없으면 NULL 불가이고 호출 동안만 borrow한다. 비동기 Driver가 읽는 프레임은 M07이 안정 주소로 소유한다. 함수 스택 임시 프레임 주소를 비동기 제출하지 않는다. 완료와 참조 해제는 다른 사실이며 장치 계약으로 두 사실이 동시임을 입증한 경우에만 함께 처리한다.

## 근거와 다음 문서

정확한 제공 근거는 S4C `evidence/b07/can/CIS_REAR.md`, `evidence/b07/CAN_CODEC_SPEC.md`, S4A `data/T09.md`, `data/T13.md`, `data/T37.md`, `api/cis.md`, `api/domain_rx.md`다. 이 이름은 제출 출처 색인과 대응한다.

[송신 접수](../functions/CIS_REAR_TX_SUBMIT.md) → [송신 선택](../functions/CIS_REAR_TX_DISPATCH.md) → [Codec](../functions/CIS_REAR_CODEC.md) → [수신 수용](../functions/CIS_REAR_RX_ADMISSION.md) → [DOMAIN 인계](../functions/DOMAIN_CIS_REAR_INGRESS.md).
