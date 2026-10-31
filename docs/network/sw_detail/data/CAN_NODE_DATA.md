# CIS·WINDOW·VSS CAN 자료와 필드 대응

이 문서는 CIS 세 보고, WINDOW 다섯 메시지, VSS 세 메시지의 DTO 전체 선언과 고정 바이트 대응을 둔다. CIS_REAR의 자료·Codec·시간은 [기존 단일 정본](CIS_REAR_DATA.md)을 사용한다. 아래 Net 선언은 설계 후보, C2 선언은 외부 제공 선언이며 생산 헤더·실물 채택을 뜻하지 않는다.

[Network](../00_START_HERE.md) · [M03](../modules/M03_TYPED_HANDOFF.md) · [M05](../modules/M05_CAN_CODEC.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md) · [공통 자료](CAN_COMMON_DATA.md)

## 공유 자료와 수명

| 자료 정본 | 참조 타입 |
|---|---|
| 기존 B11 자료 | [`CisRearState_t`](CIS_REAR_DATA.md#type-CisRearState_t), [`DataQuality_t`](CIS_REAR_DATA.md#type-DataQuality_t), [`DomainIf_Status_t`](CIS_REAR_DATA.md#type-DomainIf_Status_t), [`NetCanRxMeta`](CIS_REAR_DATA.md#type-NetCanRxMeta) |
| 기존 B11 자료 | [`NetCanSource`](CIS_REAR_DATA.md#type-NetCanSource), [`NetCisRear`](CIS_REAR_DATA.md#type-NetCisRear), [`NetProximityStatus`](CIS_REAR_DATA.md#type-NetProximityStatus), [`NetProximityStatusCode`](CIS_REAR_DATA.md#type-NetProximityStatusCode) |
| 기존 B11 자료 | [`NetQualityReason`](CIS_REAR_DATA.md#type-NetQualityReason), [`NetQualityReasonCode`](CIS_REAR_DATA.md#type-NetQualityReasonCode), [`NetSubmitResult`](CIS_REAR_DATA.md#type-NetSubmitResult), [`NetValidity`](CIS_REAR_DATA.md#type-NetValidity) |
| 기존 B11 자료 | [`NetValidityCode`](CIS_REAR_DATA.md#type-NetValidityCode), [`QualityReason_t`](CIS_REAR_DATA.md#type-QualityReason_t), [`RearMeasurementState_t`](CIS_REAR_DATA.md#type-RearMeasurementState_t), [`SignalMeta_t`](CIS_REAR_DATA.md#type-SignalMeta_t) |
| 기존 B11 자료 | [`SourceGeneration_t`](CIS_REAR_DATA.md#type-SourceGeneration_t), [`UpdateSequence_t`](CIS_REAR_DATA.md#type-UpdateSequence_t), [`ValueQuality_t`](CIS_REAR_DATA.md#type-ValueQuality_t), [`ValueValidity_t`](CIS_REAR_DATA.md#type-ValueValidity_t) |
| 공통 고정폭 자료 | [`NetAvailability`](CAN_COMMON_DATA.md#type-NetAvailability), [`NetAvailabilityCode`](CAN_COMMON_DATA.md#type-NetAvailabilityCode), [`NetCommandMeta`](CAN_COMMON_DATA.md#type-NetCommandMeta), [`NetControllerState`](CAN_COMMON_DATA.md#type-NetControllerState) |
| 공통 고정폭 자료 | [`NetControllerStateCode`](CAN_COMMON_DATA.md#type-NetControllerStateCode), [`NetFaultCategory`](CAN_COMMON_DATA.md#type-NetFaultCategory), [`NetFaultCategoryCode`](CAN_COMMON_DATA.md#type-NetFaultCategoryCode), [`NetFlag`](CAN_COMMON_DATA.md#type-NetFlag) |
| 공통 고정폭 자료 | [`NetFlagCode`](CAN_COMMON_DATA.md#type-NetFlagCode), [`NetOrigin`](CAN_COMMON_DATA.md#type-NetOrigin), [`NetOriginCode`](CAN_COMMON_DATA.md#type-NetOriginCode), [`NetReason`](CAN_COMMON_DATA.md#type-NetReason) |
| 공통 고정폭 자료 | [`NetReasonCode`](CAN_COMMON_DATA.md#type-NetReasonCode), [`NetSensorFunctionState`](CAN_COMMON_DATA.md#type-NetSensorFunctionState), [`NetSensorFunctionStateCode`](CAN_COMMON_DATA.md#type-NetSensorFunctionStateCode), [`NetTriState`](CAN_COMMON_DATA.md#type-NetTriState) |
| 공통 고정폭 자료 | [`NetTriStateCode`](CAN_COMMON_DATA.md#type-NetTriStateCode), [`NetValueQuality`](CAN_COMMON_DATA.md#type-NetValueQuality), [`NetValueQualityCode`](CAN_COMMON_DATA.md#type-NetValueQualityCode), [`NetVehicleResult`](CAN_COMMON_DATA.md#type-NetVehicleResult) |
| 공통 고정폭 자료 | [`NetVehicleResultCode`](CAN_COMMON_DATA.md#type-NetVehicleResultCode) |
| 공통 의미 자료 | [`CommandOrigin_t`](CAN_COMMON_DATA.md#type-CommandOrigin_t), [`CommonEcuState_t`](CAN_COMMON_DATA.md#type-CommonEcuState_t), [`DomainCommandId_t`](CAN_COMMON_DATA.md#type-DomainCommandId_t), [`DomainIf_CommandContext_t`](CAN_COMMON_DATA.md#type-DomainIf_CommandContext_t) |
| 공통 의미 자료 | [`FaultCategory_t`](CAN_COMMON_DATA.md#type-FaultCategory_t), [`FaultInfo_t`](CAN_COMMON_DATA.md#type-FaultInfo_t), [`FaultState_t`](CAN_COMMON_DATA.md#type-FaultState_t), [`FunctionAvailability_t`](CAN_COMMON_DATA.md#type-FunctionAvailability_t) |
| 공통 의미 자료 | [`OccurrenceId_t`](CAN_COMMON_DATA.md#type-OccurrenceId_t), [`RequestResult_t`](CAN_COMMON_DATA.md#type-RequestResult_t), [`ResultConfirmation_t`](CAN_COMMON_DATA.md#type-ResultConfirmation_t), [`ResultReason_t`](CAN_COMMON_DATA.md#type-ResultReason_t) |


포인터는 호출 동안만 빌린다. 송신 성공 전에 DTO 전체와 원 식별·AGE 기준을 Network 저장소에 함께 확보한다. 수신 DTO/meta는 같은 프레임 한 쌍이며 지연 소비자는 반환 전 둘 다 복사한다. 모든 멀티바이트는 고정 폭 LE이고 i16은 2의 보수다. struct 메모리·C enum 저장 폭·sizeof를 Wire 배치로 쓰지 않는다. BOOT_ID0은 예약이며 C2 Generation0 의미와 다르다.

<a id="CIS_ENVIRONMENT"></a>
## CIS_ENVIRONMENT: 세 센서의 독립 관측

CIS→DOMAIN, 후보 ID `0x300`, used `40`B / frame `48`B / DLC `14`. Padding: 40..47, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. 온도·습도·조도 각각의 원 생산 세대, temperature/humidity/illuminance_sequence. CAN BOOT/TX는 외피 식별이다. temperature_age_ms, humidity_age_ms, illuminance_age_ms를 독립 누적한다. 다른 채널 갱신이 원 시각을 재시작하지 않는다.

<a id="type-NetCisEnvironment"></a>
### `NetCisEnvironment`

```c
typedef struct NetCisEnvironment
{
    int16_t temperature_centi_c; /**< TEMPERATURE: 0.01°C/raw; -4000~12500; -32768=무효 */
    uint32_t temperature_sequence; /**< TEMPERATURE_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t temperature_age_ms; /**< TEMPERATURE_AGE: 해당 값 원본 측정 후 ms */
    NetValidity temperature_validity; /**< TEMPERATURE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason temperature_reason; /**< TEMPERATURE_REASON: CIS 품질 사유 */
    uint16_t humidity_centi_percent; /**< HUMIDITY: 0.01%RH/raw; 0~10000; 65535=무효 */
    uint32_t humidity_sequence; /**< HUMIDITY_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t humidity_age_ms; /**< HUMIDITY_AGE: 해당 값 원본 측정 후 ms */
    NetValidity humidity_validity; /**< HUMIDITY_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason humidity_reason; /**< HUMIDITY_REASON: CIS 품질 사유 */
    uint32_t illuminance_lux; /**< ILLUMINANCE: 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 */
    uint32_t illuminance_sequence; /**< ILLUMINANCE_SEQUENCE: 해당 값의 새 측정 번호 */
    uint16_t illuminance_age_ms; /**< ILLUMINANCE_AGE: 해당 값 원본 측정 후 ms */
    NetValidity illuminance_validity; /**< ILLUMINANCE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason illuminance_reason; /**< ILLUMINANCE_REASON: CIS 품질 사유 */
} NetCisEnvironment;
```

근거: S4A data/T09.md#type-NetCisEnvironment. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `temperature_centi_c` | `int16_t`; 16비트 부호 있음 | 0.01°C/raw; -4000~12500; -32768=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `temperature_sequence` | `uint32_t`; 32비트 부호 없음 | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `temperature_age_ms` | `uint16_t`; 16비트 부호 없음 | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `temperature_validity` | `NetValidity`; 8비트 부호 없음 | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `temperature_reason` | `NetQualityReason`; 8비트 부호 없음 | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_centi_percent` | `uint16_t`; 16비트 부호 없음 | 0.01%RH/raw; 0~10000; 65535=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_sequence` | `uint32_t`; 32비트 부호 없음 | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `humidity_age_ms` | `uint16_t`; 16비트 부호 없음 | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `humidity_validity` | `NetValidity`; 8비트 부호 없음 | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `humidity_reason` | `NetQualityReason`; 8비트 부호 없음 | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_lux` | `uint32_t`; 32비트 부호 없음 | 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_sequence` | `uint32_t`; 32비트 부호 없음 | 해당 값의 새 측정 번호 | CIS 해당 센서의 실제 새 측정 번호. 다른 센서 갱신·단순 재전송으로 증가 금지. |
| `illuminance_age_ms` | `uint16_t`; 16비트 부호 없음 | 해당 값 원본 측정 후 ms | CIS 해당 센서 원 측정 후 ms. 센서별 독립; Network 대기만 추가·65535 포화. |
| `illuminance_validity` | `NetValidity`; 8비트 부호 없음 | 0=INVALID; 1=VALID | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |
| `illuminance_reason` | `NetQualityReason`; 8비트 부호 없음 | CIS 품질 사유 | CIS 해당 센서 담당. 같은 센서 validity/reason과 함께 해석; 다른 정상 센서까지 무효화하지 않음. |

<a id="type-CisCabinEnvironmentState_t"></a>
### `CisCabinEnvironmentState_t`

```c
typedef struct
{
    int16_t cabinTemperature; /* 0.01 deg C */
    ValueQuality_t temperatureQuality;

    uint16_t cabinHumidity; /* 0.01 percent RH */
    ValueQuality_t humidityQuality;

    uint32_t cabinIlluminance; /* 1 lx; no truncation above 65535 */
    ValueQuality_t illuminanceQuality;
} CisCabinEnvironmentState_t;
```

근거: S4A data/T13.md#type-CisCabinEnvironmentState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `cabinTemperature` | `int16_t`; 16비트 부호 있음 | CIS 실내 온도 i16, centi-°C. 값 사용은 temperatureQuality 유효성에 따른다. | CIS 센서·유효성 담당자 |
| `temperatureQuality` | `ValueQuality_t`; 중첩 타입 참조; sizeof 미검증 | CIS 온도의 독립 유효성/사유/원본 생성 후 경과 시간/원본 평가 순서. 습도·조도 품질과 별개. | CIS 센서·유효성 담당자 |
| `cabinHumidity` | `uint16_t`; 16비트 부호 없음 | CIS 실내 상대습도 u16, centi-%RH; 정상 0~10000 후보, humidityQuality와 함께 소비. | CIS 센서·유효성 담당자 |
| `humidityQuality` | `ValueQuality_t`; 중첩 타입 참조; sizeof 미검증 | CIS 습도의 독립 유효성/사유/원본 생성 후 경과 시간/원본 평가 순서. | CIS 센서·유효성 담당자 |
| `cabinIlluminance` | `uint32_t`; 32비트 부호 없음 | CIS 실내 조도 u32, lux. illuminanceQuality 독립. | CIS 센서·유효성 담당자 |
| `illuminanceQuality` | `ValueQuality_t`; 중첩 타입 참조; sizeof 미검증 | CIS 실내 조도의 독립 유효성/사유/원본 생성 후 경과 시간/원본 평가 순서. | CIS 센서·유효성 담당자 |

<a id="wire-CIS_ENVIRONMENT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/CIS_ENVIRONMENT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 319 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 319 |
| 8 / 2 | `TEMPERATURE` → `temperature_centi_c` (`int16_t`) | i16 / signed / LE | 0.01°C/raw; -4000~12500; -32768=무효; 단위=0.01 degC/raw | -32768=무효 | 320 |
| 10 / 4 | `TEMPERATURE_SEQUENCE` → `temperature_sequence` (`uint32_t`) | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 321 |
| 14 / 2 | `TEMPERATURE_AGE` → `temperature_age_ms` (`uint16_t`) | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 322 |
| 16 / 1 | `TEMPERATURE_VALIDITY` → `temperature_validity` (`NetValidity`) | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 323 |
| 17 / 1 | `TEMPERATURE_REASON` → `temperature_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 324 |
| 18 / 2 | `HUMIDITY` → `humidity_centi_percent` (`uint16_t`) | u16 / unsigned / LE | 0.01%RH/raw; 0~10000; 65535=무효; 단위=0.01 %RH/raw | 65535=무효 | 325 |
| 20 / 4 | `HUMIDITY_SEQUENCE` → `humidity_sequence` (`uint32_t`) | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 326 |
| 24 / 2 | `HUMIDITY_AGE` → `humidity_age_ms` (`uint16_t`) | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 327 |
| 26 / 1 | `HUMIDITY_VALIDITY` → `humidity_validity` (`NetValidity`) | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 328 |
| 27 / 1 | `HUMIDITY_REASON` → `humidity_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 329 |
| 28 / 4 | `ILLUMINANCE` → `illuminance_lux` (`uint32_t`) | u32 / unsigned / LE | 1 lx/raw; 표현 범위 0~200000; 0xFFFFFFFF=무효; 단위=1 lx/raw | 0xFFFFFFFF=무효 | 330 |
| 32 / 4 | `ILLUMINANCE_SEQUENCE` → `illuminance_sequence` (`uint32_t`) | u32 / unsigned / LE | 해당 값의 새 측정 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 331 |
| 36 / 2 | `ILLUMINANCE_AGE` → `illuminance_age_ms` (`uint16_t`) | u16 / unsigned / LE | 해당 값 원본 측정 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 332 |
| 38 / 1 | `ILLUMINANCE_VALIDITY` → `illuminance_validity` (`NetValidity`) | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 333 |
| 39 / 1 | `ILLUMINANCE_REASON` → `illuminance_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 334 |

온도는 -4000..12500 또는 -32768, 습도는 0..10000 또는 65535, 조도는 0..200000 또는 0xFFFFFFFF다. sentinel+VALID는 모순이며 INVALID+해당 sentinel의 정상 열화 보고는 유지한다. 세 validity는 0/1, reason은 0..9다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `CisCabinEnvironmentState_t.cabinTemperature` / `int16_t` | TEMPERATURE; 0.01°C int16, sentinel 보존 |
| `CisCabinEnvironmentState_t.temperatureQuality` / `ValueQuality_t` | TEMPERATURE_VALIDITY/REASON/SEQUENCE/AGE의 독립 ValueQuality_t 투영 후보 |
| `CisCabinEnvironmentState_t.cabinHumidity` / `uint16_t` | HUMIDITY; 0.01%RH uint16, sentinel 보존 |
| `CisCabinEnvironmentState_t.humidityQuality` / `ValueQuality_t` | HUMIDITY_VALIDITY/REASON/SEQUENCE/AGE의 독립 품질 투영 후보 |
| `CisCabinEnvironmentState_t.cabinIlluminance` / `uint32_t` | ILLUMINANCE; uint32 lx 그대로, 65535 넘는 값 잘라내기 금지 |
| `CisCabinEnvironmentState_t.illuminanceQuality` / `ValueQuality_t` | ILLUMINANCE_VALIDITY/REASON/SEQUENCE/AGE의 독립 품질 투영 후보 |

cabinTemperature/cabinHumidity/cabinIlluminance에 원 raw를 그대로 옮긴다. 각 ValueQuality_t.validity/reason은 이름으로 변환하고 meta.updateSequence와 meta.ageMs에는 같은 채널의 원 순번과 소비시점 AGE를 넣는다. meta.quality는 validity/reason 숫자 비교로 만들지 않는다. 원 Boot/채널 순서와 품질 대응을 확인한 뒤 전체 C2 객체를 전달한다. 로컬 만료·무수신·기동 변화는 아래 별도 사건 자료와 기존 C2 품질로 연결한다.

**적용 제약:** CIS_QUALITY_PROJECTION, IF-O09, IF-O19, IF-O20, IF-O24: 온도·습도의 원 사용기한은 각각2500ms이며 도달 시 비사용한다. DataQuality의 나머지 원 품질 대응·조도 TTL·실제 VSM/독립 Meta 연결은 미검증이다. 원 S4A CisCabinEnvironmentState_t에는 sourceGeneration 멤버가 없으므로 추가하지 않는다.

[함수와 양단 API](../functions/can/CIS_ENVIRONMENT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#CIS_ENVIRONMENT)

<a id="CIS_OCCUPANT"></a>
## CIS_OCCUPANT: Pi 원 판정과 CIS 평가 보존

CIS→DOMAIN, 후보 ID `0x310`, used `25`B / frame `32`B / DLC `13`. Padding: 25..31, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. 원 pi_boot_id+vision_sequence와 CAN CIS BOOT/TX를 따로 보존한다. vision_function_status는 같은 occupant update의 CIS 상태다. vision_age_ms는 Pi 최초 판정 이후 누적이다. CIS 보관·CAN 송신·DOMAIN 소비의 미반영 구간만 더한다.

<a id="type-NetCisOccupant"></a>
### `NetCisOccupant`

```c
typedef struct NetCisOccupant
{
    uint32_t pi_boot_id; /**< PI_BOOT_ID: Pi 원본 기동 번호 */
    uint32_t vision_sequence; /**< VISION_SEQUENCE: Pi 새 판정 번호, 중계 시 보존 */
    uint16_t vision_age_ms; /**< VISION_AGE: Pi 원본 판정 후 누적 ms */
    NetTriState occupant_presence; /**< OCCUPANT_PRESENCE: 0=없음; 1=있음; 255=확인 불가 */
    NetValidity presence_validity; /**< PRESENCE_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason presence_reason; /**< PRESENCE_REASON: CIS 품질 사유 */
    uint8_t occupant_count; /**< OCCUPANT_COUNT: 0~5명,255=확인 불가 */
    NetValidity count_validity; /**< COUNT_VALIDITY: 0=INVALID; 1=VALID */
    NetQualityReason count_reason; /**< COUNT_REASON: CIS 품질 사유 */
    NetSensorFunctionState vision_function_status; /**< VISION_FUNCTION_STATUS: CIS 기능 상태 */
} NetCisOccupant;
```

근거: S4A data/T09.md#type-NetCisOccupant. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `pi_boot_id` | `uint32_t`; 32비트 부호 없음 | Pi 원본 기동 번호 | Pi 실행 환경/비전 원본. CIS는 수신한 값을 보존; CIS CAN Boot/TX_SEQUENCE로 대체하지 않음. |
| `vision_sequence` | `uint32_t`; 32비트 부호 없음 | Pi 새 판정 번호, 중계 시 보존 | Pi 실행 환경/비전 원본. CIS는 수신한 값을 보존; CIS CAN Boot/TX_SEQUENCE로 대체하지 않음. |
| `vision_age_ms` | `uint16_t`; 16비트 부호 없음 | Pi 원본 판정 후 누적 ms | Pi 원 판정 Age + CIS 수신 후 보관/송신 대기. 각 구간을 한 번만 추가; 새 수신·상태평가로 0 초기화 금지. |
| `occupant_presence` | `NetTriState`; 8비트 부호 없음 | 0=없음; 1=있음; 255=확인 불가 | Pi 원 값. CIS 추가 검사 실패 때 정상 없음/0명을 합성하지 않음; 해당 validity를 함께 소비. |
| `presence_validity` | `NetValidity`; 8비트 부호 없음 | 0=INVALID; 1=VALID | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `presence_reason` | `NetQualityReason`; 8비트 부호 없음 | CIS 품질 사유 | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `occupant_count` | `uint8_t`; 8비트 부호 없음 | 0~5명,255=확인 불가 | Pi 원 값. CIS 추가 검사 실패 때 정상 없음/0명을 합성하지 않음; 해당 validity를 함께 소비. |
| `count_validity` | `NetValidity`; 8비트 부호 없음 | 0=INVALID; 1=VALID | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `count_reason` | `NetQualityReason`; 8비트 부호 없음 | CIS 품질 사유 | CIS 최종 전달 평가 후보. Pi 원 품질을 cache에 보존하고 추가 검사로만 제한; 이 CAN 한 벌로 양측 품질을 동시에 표현할 수 없음(OPEN). |
| `vision_function_status` | `NetSensorFunctionState`; 8비트 부호 없음 | CIS 기능 상태 | CIS 기능 평가 담당자. 이 occupant update와 함께 고정한 CIS 비전 기능 상태; Pi VISION_STATUS 단순 숫자 그대로 형변환 금지. |

<a id="type-CisOccupantState_t"></a>
### `CisOccupantState_t`

```c
typedef struct
{
    bool occupantPresent; /* only usable when presenceQuality says usable */
    uint8_t occupantCount;
    ValueQuality_t presenceQuality;
    ValueQuality_t countQuality;
    SourceGeneration_t sourceGeneration; /* PI_BOOT_ID: Presence/Count only */
    CisFunctionStatus_t visionFunctionStatus; /* CIS-owned, same occupant update;
        not Pi observation identity; never replaced by latest separate CIS_STATUS */
} CisOccupantState_t;
```

근거: S4A data/T13.md#type-CisOccupantState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `occupantPresent` | `bool`; 저장 폭은 ABI 미검증 | Pi 원 탑승자 존재 판정 bool. 유효성 실패는 false 인 정상 관측이 아니다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `occupantCount` | `uint8_t`; 8비트 부호 없음 | Pi 원 인원수, 0~5 정상 후보; 미확인 raw=255는 countQuality 무효 의미와 함께 변환. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `presenceQuality` | `ValueQuality_t`; 중첩 타입 참조; sizeof 미검증 | Pi 존재 판정의 유효성/사유/품질/원본 생성 후 경과 시간/원 순서. countQuality와 독립. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `countQuality` | `ValueQuality_t`; 중첩 타입 참조; sizeof 미검증 | Pi 인원수의 독립 유효성/품질/AGE/순서. CIS 재평가 표현의 정확한 합성은 팀 결정 필요. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `sourceGeneration` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `visionFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | 비전 기능 준비·고장 상태. 한 번 관측 성공했다고 지속 READY 합성 금지. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |

<a id="wire-CIS_OCCUPANT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/CIS_OCCUPANT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 348 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 348 |
| 8 / 4 | `PI_BOOT_ID` → `pi_boot_id` (`uint32_t`) | u32 / unsigned / LE | Pi 원본 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 349 |
| 12 / 4 | `VISION_SEQUENCE` → `vision_sequence` (`uint32_t`) | u32 / unsigned / LE | Pi 새 판정 번호, 중계 시 보존; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 350 |
| 16 / 2 | `VISION_AGE` → `vision_age_ms` (`uint16_t`) | u16 / unsigned / LE | Pi 원본 판정 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 351 |
| 18 / 1 | `OCCUPANT_PRESENCE` → `occupant_presence` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 352 |
| 19 / 1 | `PRESENCE_VALIDITY` → `presence_validity` (`NetValidity`) | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 353 |
| 20 / 1 | `PRESENCE_REASON` → `presence_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 354 |
| 21 / 1 | `OCCUPANT_COUNT` → `occupant_count` (`uint8_t`) | u8 / unsigned / N/A | 0~5명,255=확인 불가; 단위=명 | 255=확인 불가 | 355 |
| 22 / 1 | `COUNT_VALIDITY` → `count_validity` (`NetValidity`) | u8 / unsigned / N/A | 0=NET_VALIDITY_INVALID; 1=NET_VALIDITY_VALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 356 |
| 23 / 1 | `COUNT_REASON` → `count_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 357 |
| 24 / 1 | `VISION_FUNCTION_STATUS` → `vision_function_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 358 |

presence는0/1/255, count는0..5/255, 두 validity는0/1, 두 reason은0..9, vision_function_status는0..4다. UNKNOWN을 정상 부재나0명으로 만들지 않는다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `CisOccupantState_t.occupantPresent` / `bool` | OCCUPANT_PRESENCE0/1일 때만 bool 변환; 255는 품질과 함께 비사용 |
| `CisOccupantState_t.occupantCount` / `uint8_t` | OCCUPANT_COUNT0..5; 255를0명으로 바꾸지 않음 |
| `CisOccupantState_t.presenceQuality` / `ValueQuality_t` | PRESENCE_VALIDITY/REASON과 Pi 원 sequence/AGE의 독립 품질 투영 후보 |
| `CisOccupantState_t.countQuality` / `ValueQuality_t` | COUNT_VALIDITY/REASON과 Pi 원 sequence/AGE의 독립 품질 투영 후보 |
| `CisOccupantState_t.sourceGeneration` / `SourceGeneration_t` | PI_BOOT_ID; CIS BOOT_ID와 구분, admitted lineage 바인딩 HOLD |
| `CisOccupantState_t.visionFunctionStatus` / `CisFunctionStatus_t` | VISION_FUNCTION_STATUS; 같은 OCCUPANT 갱신에서 보존, 별도 CIS_STATUS의 최신 값으로 대체 금지 |

presence0/1은 occupantPresent false/true로 이름 대응하고 count0..5는 occupantCount에 보존한다. UNKNOWN255는 품질과 별개로 확인된 부재/0명이 아니며 비사용 저장값 규칙이 없으면 typed 투영을 HOLD한다. presenceQuality/countQuality는 같은 vision_sequence·소비 AGE를 쓰되 각각 validity/reason을 유지한다. sourceGeneration은 검증된 Pi 계보만 사용한다. visionFunctionStatus는 같은 입력의 상태를 이름 변환한다.

**적용 제약:** IF-O13, IF-O09, IF-O24, CIS_QUALITY_PROJECTION: Pi 원 품질 보존과 확인된 현재 제한의 분리는 R03/R04 후보에 반영한다. 실제 제한 사건·reason 대응과 sourceGeneration의 Pi 계보 바인딩은 검증 대기다. bool에255 UNKNOWN을 손실 없이 표현하는 소비 계약도 확인해야 한다.

[함수와 양단 API](../functions/can/CIS_OCCUPANT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#CIS_OCCUPANT)

<a id="CIS_STATUS"></a>
## CIS_STATUS: 기능·인터페이스·고장 이력 보고

CIS→DOMAIN, 후보 ID `0x320`, used `34`B / frame `48`B / DLC `14`. Padding: 34..47, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. CIS 생산 세대+update_sequence는 상태 평가, last_fault_occurrence는 최근 고장 발생이다. 두 번호를 합치지 않는다. source_age_ms는 상태 평가 후 시간이다. 새 status 전송이 과거 센서 관측의 AGE를 초기화하지 않는다.

<a id="type-NetCisStatus"></a>
### `NetCisStatus`

```c
typedef struct NetCisStatus
{
    NetCisState cis_state; /**< CIS_STATE: 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT */
    NetSensorFunctionState vision_status; /**< VISION_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState temperature_status; /**< TEMPERATURE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState humidity_status; /**< HUMIDITY_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState illuminance_status; /**< ILLUMINANCE_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetSensorFunctionState rear_status; /**< REAR_STATUS: 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING */
    NetInterfaceState pi_uart_status; /**< PI_UART_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING */
    NetInterfaceState can_interface_status; /**< CAN_INTERFACE_STATUS: 0=READY; 1=UNAVAILABLE; 2=RECOVERING */
    uint16_t fault_mask; /**< FAULT_MASK: CIS 고장 비트 0~8 */
    uint16_t active_fault; /**< ACTIVE_FAULT: 현재 주요 고장 코드 */
    NetFlag recovering; /**< RECOVERING: 0=아님; 1=복구 중 */
    uint16_t last_fault; /**< LAST_FAULT: 이번 기동 이후 최근 주요 고장 코드 */
    uint32_t last_fault_occurrence; /**< LAST_FAULT_OCCURRENCE: 최근 고장 발생 번호 */
    uint8_t affected_function; /**< AFFECTED_FUNCTION: bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 */
    uint32_t update_sequence; /**< UPDATE_SEQUENCE: 이번 상태 평가 번호 */
    uint16_t source_age_ms; /**< SOURCE_AGE: 평가 후 ms */
} NetCisStatus;
```

근거: S4A data/T09.md#type-NetCisStatus. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `cis_state` | `NetCisState`; 8비트 부호 없음 | 0=STARTUP; 1=READY; 2=ACTIVE; 3=FAULT | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `vision_status` | `NetSensorFunctionState`; 8비트 부호 없음 | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `temperature_status` | `NetSensorFunctionState`; 8비트 부호 없음 | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `humidity_status` | `NetSensorFunctionState`; 8비트 부호 없음 | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `illuminance_status` | `NetSensorFunctionState`; 8비트 부호 없음 | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `rear_status` | `NetSensorFunctionState`; 8비트 부호 없음 | 0=NOT_READY; 1=READY; 2=ACTIVE; 3=FAULT; 4=RECOVERING | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `pi_uart_status` | `NetInterfaceState`; 8비트 부호 없음 | 0=READY; 1=UNAVAILABLE; 2=RECOVERING | 원천은 CIS Network의 해당 링크 관측; CIS 진단 담당자가 상태에 반영. concrete health getter/sink BINDING_OPEN. |
| `can_interface_status` | `NetInterfaceState`; 8비트 부호 없음 | 0=READY; 1=UNAVAILABLE; 2=RECOVERING | 원천은 CIS Network의 해당 링크 관측; CIS 진단 담당자가 상태에 반영. concrete health getter/sink BINDING_OPEN. |
| `fault_mask` | `uint16_t`; 16비트 부호 없음 | CIS 고장 비트 0~8 | CIS 진단 담당자. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `active_fault` | `uint16_t`; 16비트 부호 없음 | 현재 주요 고장 코드 | CIS 진단 담당자. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `recovering` | `NetFlag`; 8비트 부호 없음 | 0=아님; 1=복구 중 | CIS 기능 담당. 부분 고장과 정상 기능을 구분하며 복구 상태만으로 이전 측정을 VALID로 바꾸지 않음. |
| `last_fault` | `uint16_t`; 16비트 부호 없음 | 이번 기동 이후 최근 주요 고장 코드 | CIS 진단의 이번 기동 이후 최근 이력. active 해제·현재 상태 TTL로 이력을 삭제하지 않음. |
| `last_fault_occurrence` | `uint32_t`; 32비트 부호 없음 | 최근 고장 발생 번호 | CIS 진단의 이번 기동 이후 최근 이력. active 해제·현재 상태 TTL로 이력을 삭제하지 않음. |
| `affected_function` | `uint8_t`; 8비트 부호 없음 | bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로 | CIS 진단 담당자. 현재 고장과 영향 기능/제공 경로 구분; 별도 CIS_FAULT frame 추가 없음. |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 이번 상태 평가 번호 | CIS 새 상태 평가에서 증가; VISION_SEQUENCE/센서 원 측정 순번을 갱신하지 않음. |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 평가 후 ms | CIS 원 상태 평가 후 ms; Network 대기만 한 번 누적·65535 포화. |

<a id="type-DomainIf_CisStatusReport_t"></a>
### `DomainIf_CisStatusReport_t`

```c
typedef struct
{
    CisState_t ecuState;
    CisFunctionStatus_t occupantFunctionStatus;
    CisFunctionStatus_t temperatureFunctionStatus;
    CisFunctionStatus_t humidityFunctionStatus;
    CisFunctionStatus_t illuminanceFunctionStatus;
    CisFunctionStatus_t rearFunctionStatus;
    DomainIf_CisInterfaceStatus_t piUartStatus;
    DomainIf_CisInterfaceStatus_t canInterfaceStatus;
    uint16_t faultMask;
    uint16_t activeFaultCode;
    bool recovering;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint8_t affectedFunctionMask;
    SourceGeneration_t sourceGeneration; /* derived CIS fault-warning lineage */
    SignalMeta_t meta;
} DomainIf_CisStatusReport_t;
```

근거: S4A data/T14.md#type-DomainIf_CisStatusReport_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `ecuState` | `CisState_t`; C enum 저장 폭 미검증 | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `occupantFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | CIS 탑승자 판정 기능 상태. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `temperatureFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | CIS 온도 기능 상태. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `humidityFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | CIS 습도 기능 상태. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `illuminanceFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | CIS 실내 조도 기능 상태. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `rearFunctionStatus` | `CisFunctionStatus_t`; C enum 저장 폭 미검증 | CIS 후방 초음파 기능 상태. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `piUartStatus` | `DomainIf_CisInterfaceStatus_t`; C enum 저장 폭 미검증 | CIS Network가 관측한 Pi UART 링크 상태. 0 READY/2 UNAVAILABLE/3 RECOVERING, 공개 DEGRADED 없음. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `canInterfaceStatus` | `DomainIf_CisInterfaceStatus_t`; C enum 저장 폭 미검증 | CIS Network가 관측한 CAN 링크 상태. 원 raw와 C enum 숫자 명시 변환. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `faultMask` | `uint16_t`; 16비트 부호 없음 | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `activeFaultCode` | `uint16_t`; 16비트 부호 없음 | 활성 고장 code u16. 0 없음, lastFaultCode와 별도. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `recovering` | `bool`; 저장 폭은 ABI 미검증 | 기능 회복 진행 bool. 링크 recovering 및 성공 완료와 별개. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `lastFaultCode` | `uint16_t`; 16비트 부호 없음 | 원 ECU 최근 고장 code u16. 활성 고장 여부가 아니다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `lastFaultOccurrenceId` | `OccurrenceId_t`; 32비트 부호 없음 | 최근 고장 발생 식별, 코드·원 세대·AGE와 함께 소비. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `affectedFunctionMask` | `uint8_t`; 8비트 부호 없음 | 영향 기능 bitmask. 노드별 의미를 적용하고 알 수 없는 bit를 임의 확정하지 않는다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `sourceGeneration` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | CIS 센서/유효성 담당자; 탑승자 원 판정은 Pi |

<a id="wire-CIS_STATUS"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/CIS_STATUS.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 392 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 392 |
| 8 / 1 | `CIS_STATE` → `cis_state` (`NetCisState`) | u8 / unsigned / N/A | 0=NET_CIS_STATE_STARTUP; 1=NET_CIS_STATE_READY; 2=NET_CIS_STATE_ACTIVE; 3=NET_CIS_STATE_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 393 |
| 9 / 1 | `VISION_STATUS` → `vision_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 394 |
| 10 / 1 | `TEMPERATURE_STATUS` → `temperature_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 395 |
| 11 / 1 | `HUMIDITY_STATUS` → `humidity_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 396 |
| 12 / 1 | `ILLUMINANCE_STATUS` → `illuminance_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 397 |
| 13 / 1 | `REAR_STATUS` → `rear_status` (`NetSensorFunctionState`) | u8 / unsigned / N/A | 0=NET_SENSOR_FUNCTION_NOT_READY; 1=NET_SENSOR_FUNCTION_READY; 2=NET_SENSOR_FUNCTION_ACTIVE; 3=NET_SENSOR_FUNCTION_FAULT; 4=NET_SENSOR_FUNCTION_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 398 |
| 14 / 1 | `PI_UART_STATUS` → `pi_uart_status` (`NetInterfaceState`) | u8 / unsigned / N/A | 0=NET_INTERFACE_READY; 1=NET_INTERFACE_UNAVAILABLE; 2=NET_INTERFACE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 399 |
| 15 / 1 | `CAN_INTERFACE_STATUS` → `can_interface_status` (`NetInterfaceState`) | u8 / unsigned / N/A | 0=NET_INTERFACE_READY; 1=NET_INTERFACE_UNAVAILABLE; 2=NET_INTERFACE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 400 |
| 16 / 2 | `FAULT_MASK` → `fault_mask` (`uint16_t`) | u16 / unsigned / LE | CIS 고장 비트 0~8; 1=NET_CIS_FAULT_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_VISION_FAULT; 4=NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT; 8=NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT; 16=NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT; 32=NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT; 64=NET_CIS_FAULT_COMMUNICATION_FAULT; 128=NET_CIS_FAULT_DATA_INVALID; 256=NET_CIS_FAULT_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 401 |
| 18 / 2 | `ACTIVE_FAULT` → `active_fault` (`uint16_t`) | u16 / unsigned / LE | 0=NET_CIS_FAULT_CODE_NONE; 1=NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_CODE_VISION_FAULT; 3=NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT; 4=NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT; 5=NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT; 6=NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT; 7=NET_CIS_FAULT_CODE_COMMUNICATION_FAULT; 8=NET_CIS_FAULT_CODE_DATA_INVALID; 9=NET_CIS_FAULT_CODE_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 402 |
| 20 / 1 | `RECOVERING` → `recovering` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 403 |
| 21 / 2 | `LAST_FAULT` → `last_fault` (`uint16_t`) | u16 / unsigned / LE | 0=NET_CIS_FAULT_CODE_NONE; 1=NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE; 2=NET_CIS_FAULT_CODE_VISION_FAULT; 3=NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT; 4=NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT; 5=NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT; 6=NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT; 7=NET_CIS_FAULT_CODE_COMMUNICATION_FAULT; 8=NET_CIS_FAULT_CODE_DATA_INVALID; 9=NET_CIS_FAULT_CODE_OUT_OF_RANGE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 404 |
| 23 / 4 | `LAST_FAULT_OCCURRENCE` → `last_fault_occurrence` (`uint32_t`) | u32 / unsigned / LE | 최근 고장 발생 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 405 |
| 27 / 1 | `AFFECTED_FUNCTION` → `affected_function` (`uint8_t`) | u8 / unsigned / N/A | bit0=비전; bit1=온도; bit2=습도; bit3=조도; bit4=후방; bit5=제공 경로; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 406 |
| 28 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | 이번 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 407 |
| 32 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 408 |

cis_state0..3, 기능 상태5개는0..4, 두 interface는0..2, recovering0/1, fault_mask bit0..8, active/last_fault0..9, affected_function bit0..5만 허용한다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_CisStatusReport_t.ecuState` / `CisState_t` | CIS_STATE; 이름 대응 |
| `DomainIf_CisStatusReport_t.occupantFunctionStatus` / `CisFunctionStatus_t` | VISION_STATUS; 이름 대응 |
| `DomainIf_CisStatusReport_t.temperatureFunctionStatus` / `CisFunctionStatus_t` | TEMPERATURE_STATUS; 이름 대응 |
| `DomainIf_CisStatusReport_t.humidityFunctionStatus` / `CisFunctionStatus_t` | HUMIDITY_STATUS; 이름 대응 |
| `DomainIf_CisStatusReport_t.illuminanceFunctionStatus` / `CisFunctionStatus_t` | ILLUMINANCE_STATUS; 이름 대응 |
| `DomainIf_CisStatusReport_t.rearFunctionStatus` / `CisFunctionStatus_t` | REAR_STATUS; 이름 대응 |
| `DomainIf_CisStatusReport_t.piUartStatus` / `DomainIf_CisInterfaceStatus_t` | PI_UART_STATUS; C0/2/3↔raw0/1/2 이름 대응 |
| `DomainIf_CisStatusReport_t.canInterfaceStatus` / `DomainIf_CisInterfaceStatus_t` | CAN_INTERFACE_STATUS; C0/2/3↔raw0/1/2 이름 대응 |
| `DomainIf_CisStatusReport_t.faultMask` / `uint16_t` | FAULT_MASK; bit0..8 |
| `DomainIf_CisStatusReport_t.activeFaultCode` / `uint16_t` | ACTIVE_FAULT; 현재 주요 고장0..9 |
| `DomainIf_CisStatusReport_t.recovering` / `bool` | RECOVERING; bool0/1 |
| `DomainIf_CisStatusReport_t.lastFaultCode` / `uint16_t` | LAST_FAULT; 최근 고장0..9 |
| `DomainIf_CisStatusReport_t.lastFaultOccurrenceId` / `OccurrenceId_t` | LAST_FAULT_OCCURRENCE; 원 사건 번호 |
| `DomainIf_CisStatusReport_t.affectedFunctionMask` / `uint8_t` | AFFECTED_FUNCTION; bit0..5 |
| `DomainIf_CisStatusReport_t.sourceGeneration` / `SourceGeneration_t` | 원 CIS BOOT_ID 연결 후보; 바인딩 HOLD |
| `DomainIf_CisStatusReport_t.meta` / `SignalMeta_t` | UPDATE_SEQUENCE/SOURCE_AGE 보존; quality 생성 근거 별도 HOLD |

ecuState와 다섯 기능 상태를 의미 이름으로 대응한다. raw interface READY0→CIS_INTERFACE_AVAILABLE0, UNAVAILABLE1→CIS_INTERFACE_UNAVAILABLE2, RECOVERING2→CIS_INTERFACE_RECOVERING3이다. faultMask/activeFaultCode/recovering/lastFaultCode/lastFaultOccurrenceId/affectedFunctionMask를 같은 입력에서 구성한다. sourceGeneration은 원 CIS 계보, meta는 원 UPDATE/소비 AGE와 확인한 품질 투영으로만 채운다.

**적용 제약:** IF-O09, IF-O15, IF-O19, IF-O24: 원 세대와 SignalMeta 품질 산출 및 C2 반영 연결을 확인하기 전 기능 정상화는 HOLD다.

[함수와 양단 API](../functions/can/CIS_STATUS.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#CIS_STATUS)

<a id="WINDOW_COMMAND"></a>
## WINDOW_COMMAND: 이동·STOP 명령의 조건부 수송

DOMAIN→WINDOW, 후보 ID `0x090`, used `24`B / frame `24`B / DLC `12`. Padding: 없음, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. DOMAIN 원 commandId·원 DOMAIN 기동·TARGET_BOOT_ID를 보존한다. CAN TX_SEQUENCE를 명령 ID로 쓰지 않는다. command.command_age_ms는 최초 명령 생성 이후 누적이다. ACCEPT_LIMIT200ms 대1s 미결을 임의 결정하지 않는다.

<a id="type-NetWindowCommand"></a>
### `NetWindowCommand`

```c
typedef struct NetWindowCommand
{
    NetCommandMeta command;
    NetWindowCommandKind window_command;
    uint8_t target_channel;
    uint8_t target_closed_percent;
    NetSourceContext source_context;
} NetWindowCommand;
```

근거: S4A data/T31.md#type-NetWindowCommand. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `command` | `NetCommandMeta`; 중첩 타입 참조; sizeof 미검증 | 대상 기동·원 명령 ID·생성 Age·수용 한도 | DOMAIN C2와 Network가 분담. 수신 WINDOW App은 검증된 읽기 전용 문맥 소비 |
| `window_command` | `NetWindowCommandKind`; 8비트 부호 없음 | OPEN/CLOSE/STOP/VENT/MOVE_TO_POSITION raw1~5 | DOMAIN 실행 목표 담당자; STOP도 같은 명령 형식 |
| `target_channel` | `uint8_t`; 8비트 부호 없음 | 현재 단일 채널 0 | DOMAIN 목표 담당자; WINDOW 수신 검증 |
| `target_closed_percent` | `uint8_t`; 8비트 부호 없음 | 닫힘 비율 0~100%; 0 완전열림,100 완전닫힘 | DOMAIN 목표 담당자; VENT 70↔구 SysRS80 충돌은 TEAM_DECISION_REQUIRED |
| `source_context` | `NetSourceContext`; 8비트 부호 없음 | 상위 출처 NONE/DOMAIN_POLICY/AUTO_FUNCTION/LOCAL/TEST | DOMAIN 원 문맥; MOBILE 창문 원격제어 추가 의미 아님 |

<a id="type-DomainIf_WindowCommand_t"></a>
### `DomainIf_WindowCommand_t`

```c
typedef struct
{
    DomainIf_CommandContext_t context;
    uint8_t channel;
    WindowCommand_t command;
    uint8_t targetPositionPercent;
} DomainIf_WindowCommand_t;
```

근거: S4A data/T14.md#type-DomainIf_WindowCommand_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `context` | `DomainIf_CommandContext_t`; 중첩 타입 참조; sizeof 미검증 | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 담당자 |
| `channel` | `uint8_t`; 8비트 부호 없음 | WINDOW 채널 u8. 지원 채널 범위는 구현 직전 합의; 현재 구현 보류. | Domain 기능 결정/설정/결과 담당자 |
| `command` | `WindowCommand_t`; C enum 저장 폭 미검증 | WINDOW 목표 명령 OPEN/CLOSE/STOP/VENT. VENT70↔80% 충돌 미확정; 구현 보류. | Domain 기능 결정/설정/결과 담당자 |
| `targetPositionPercent` | `uint8_t`; 8비트 부호 없음 | WINDOW 목표 위치 0~100%. VENT70↔80% 충돌은 TEAM_DECISION_REQUIRED; 현재 구현 보류. | Domain 기능 결정/설정/결과 담당자 |

<a id="wire-WINDOW_COMMAND"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/WINDOW_COMMAND.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 157 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 157 |
| 8 / 4 | `TARGET_BOOT_ID` → `command.target_boot_id` (`uint32_t`) | u32 / unsigned / LE | 마지막 정상 대상 기동; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 158 |
| 12 / 4 | `COMMAND_ID` → `command.command_id` (`uint32_t`) | u32 / unsigned / LE | C2 commandId 보존; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 158 |
| 16 / 2 | `COMMAND_AGE` → `command.command_age_ms` (`uint16_t`) | u16 / unsigned / LE | 명령 최초 생성 이후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 158 |
| 18 / 2 | `ACCEPT_LIMIT` → `command.accept_limit_ms` (`uint16_t`) | u16 / unsigned / LE | 200ms 후보/1s 충돌 OPEN; 단위=ms | NOT_SPECIFIED | 158 |
| 20 / 1 | `WINDOW_COMMAND` → `window_command` (`NetWindowCommandKind`) | u8 / unsigned / N/A | 1=NET_WINDOW_COMMAND_OPEN; 2=NET_WINDOW_COMMAND_CLOSE; 3=NET_WINDOW_COMMAND_STOP; 4=NET_WINDOW_COMMAND_VENT; 5=NET_WINDOW_COMMAND_MOVE_TO_POSITION; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 159 |
| 21 / 1 | `TARGET_CHANNEL` → `target_channel` (`uint8_t`) | u8 / unsigned / N/A | 0=현재 단일 채널; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 160 |
| 22 / 1 | `TARGET_POSITION` → `target_closed_percent` (`uint8_t`) | u8 / unsigned / N/A | 0~100% 닫힘 비율; VENT=70; 단위=% (1 percent/raw) | NOT_SPECIFIED | 161 |
| 23 / 1 | `SOURCE_CONTEXT` → `source_context` (`NetSourceContext`) | u8 / unsigned / N/A | 0=NET_SOURCE_CONTEXT_NONE; 1=NET_SOURCE_CONTEXT_DOMAIN_POLICY; 2=NET_SOURCE_CONTEXT_AUTO_FUNCTION; 3=NET_SOURCE_CONTEXT_LOCAL; 4=NET_SOURCE_CONTEXT_TEST; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 162 |

window_command1..5, channel0, target_closed_percent0..100, source_context0..4만 허용한다. VENT70/80 충돌을 자동 보정하지 않는다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_WindowCommand_t.context` / `DomainIf_CommandContext_t` | COMMAND_ID·COMMAND_AGE, 기동 연결 및 SOURCE_CONTEXT 축약은 후보; 실행 HOLD |
| `DomainIf_WindowCommand_t.channel` / `uint8_t` | TARGET_CHANNEL; 현재0 |
| `DomainIf_WindowCommand_t.command` / `WindowCommand_t` | WINDOW_COMMAND; 이름 변환 |
| `DomainIf_WindowCommand_t.targetPositionPercent` / `uint8_t` | TARGET_POSITION; 닫힘 비율, VENT70/80 충돌 HOLD |

C2 WindowCommand_t OPEN/CLOSE/STOP/VENT/MOVE_TO_POSITION의0..4를 Net 대응 이름1..5로 바꾼다. context.commandId를 보존하되 원 COMMAND_AGE 근거·TARGET_BOOT_ID·ACCEPT_LIMIT·source_context 축약은 확인된 Owner 바인딩에서만 얻는다. context.commandAgeMs는 원 명령 생성 AGE로 누적 후 u16 포화한다. context.meta.ageMs는 판단 자료의 나이이며 명령 생성 AGE로 대체하지 않는다.

**적용 제약:** WINDOW_EXECUTION_HOLD, IF-O03, IF-O04, IF-O07, IF-O18, G05: 송수신·liveness·모터 실행·STOP 작업까지 현재 비활성이다.

[함수와 양단 API](../functions/can/WINDOW_COMMAND.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_COMMAND)

<a id="WINDOW_STATE"></a>
## WINDOW_STATE: 실제 움직임과 보호 상태

WINDOW→DOMAIN, 후보 ID `0x0B0`, used `22`B / frame `24`B / DLC `12`. Padding: 22..23, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. WINDOW 생산 세대+update_sequence는 실제 상태 평가다. fully_open_state는 현재 상태이며 FULLY_OPENED 사건과 다르다. source_age_ms는 실제 상태 평가 후 시간이다. 늦은 STATE 수신을 새 위치 관측으로 만들지 않는다.

<a id="type-NetWindowState"></a>
### `NetWindowState`

```c
typedef struct NetWindowState
{
    NetWindowMotionState motion_state;
    NetControllerState window_ecu_state;
    uint8_t position_closed_percent;
    NetValueQuality window_value_quality;
    NetTriState fully_open_state;
    NetTriState fully_closed_state;
    NetAntiPinchProtectionState antipinch_protection_status;
    NetTriState reverse_state;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWindowState;
```

근거: S4A data/T31.md#type-NetWindowState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `motion_state` | `NetWindowMotionState`; 8비트 부호 없음 | STOPPED/OPENING/CLOSING/ANTIPINCH_REVERSING/UNKNOWN | WINDOW 실제 동작 담당자; Network 접수로 STOPPED 합성 금지 |
| `window_ecu_state` | `NetControllerState`; 8비트 부호 없음 | INIT/READY/DEGRADED/FAULT | WINDOW 기능 담당자; 다른 노드 상태·CAN ACK로 정상화 금지 |
| `position_closed_percent` | `uint8_t`; 8비트 부호 없음 | 0~100% 닫힘 비율,255 확인 불가 | WINDOW 실제 위치 담당자; UNKNOWN을0으로 치환 금지 |
| `window_value_quality` | `NetValueQuality`; 8비트 부호 없음 | WINDOW 상태값의 품질 | WINDOW 실제 평가 담당자; 무효·만료를 정상 끝단 값으로 치환 금지 |
| `fully_open_state` | `NetTriState`; 8비트 부호 없음 | 0 완전열림 아님/1 완전열림/255 확인불가 | WINDOW 실제 끝단 담당자; 현재 State이며 새 FULLY_OPENED Event와 별개 |
| `fully_closed_state` | `NetTriState`; 8비트 부호 없음 | 0 완전닫힘 아님/1 완전닫힘/255 확인불가 | WINDOW 실제 끝단 담당자; 확인된 위치와 정합성 검사 |
| `antipinch_protection_status` | `NetAntiPinchProtectionState`; 8비트 부호 없음 | 0 대기/1 진행/2 완료/3 중단 | WINDOW 로컬 보호 담당자; CLEAR/ACTIVE 경고 의미와 별개. 실제 outputDisabled 근거 아님 |
| `reverse_state` | `NetTriState`; 8비트 부호 없음 | 0 역전아님/1 역전중/255 확인불가 | WINDOW 실제 구동 담당자; timeout으로 역전·이동정지 추정 금지 |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 실제 새 상태·고장 평가 번호, uint32 | WINDOW 평가 담당자; CAN TX_SEQUENCE와 별개, cached 반복 때 유지 |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 원 상태·고장 평가 이후 누적 ms, uint16 포화 | WINDOW 원본 평가 + Network 대기만 누적; 재전송으로 초기화 금지 |

<a id="type-WindowState_t"></a>
### `WindowState_t`

```c
typedef struct
{
    uint8_t channel;
    WindowMotionState_t motionState;
    CommonEcuState_t ecuState;
    uint8_t positionPercent; /* 0%=Fully Open, 100%=Fully Closed */
    bool fullyOpen;
    bool fullyClosed;
    AntiPinchState_t antiPinchState;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration; /* anti-pinch lineage; implementation deferred */
} WindowState_t;
```

근거: S4A data/T13.md#type-WindowState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `channel` | `uint8_t`; 8비트 부호 없음 | WINDOW 채널 u8. 지원 채널 범위는 구현 직전 합의; 현재 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `motionState` | `WindowMotionState_t`; C enum 저장 폭 미검증 | WINDOW 실제 이동 상태. 상위 Command 목표와 다르며 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `ecuState` | `CommonEcuState_t`; C enum 저장 폭 미검증 | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `positionPercent` | `uint8_t`; 8비트 부호 없음 | WINDOW 위치 0~100%, 원 품질/AGE와 함께 사용; 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `fullyOpen` | `bool`; 저장 폭은 ABI 미검증 | WINDOW 실제 완전 열림 끝단 bool, 관측 근거 필요; 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `fullyClosed` | `bool`; 저장 폭은 ABI 미검증 | WINDOW 실제 완전 닫힘 끝단 bool, 관측 근거 필요; 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `antiPinchState` | `AntiPinchState_t`; C enum 저장 폭 미검증 | WINDOW 끼임 의미 상태. 로컬 보호가 우선하며 무효 입력을 CLEAR로 치환 금지. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `sourceGeneration` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |

<a id="wire-WINDOW_STATE"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/WINDOW_STATE.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 424 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 424 |
| 8 / 1 | `MOTION_STATE` → `motion_state` (`NetWindowMotionState`) | u8 / unsigned / N/A | 0=NET_WINDOW_MOTION_STOPPED; 1=NET_WINDOW_MOTION_OPENING; 2=NET_WINDOW_MOTION_CLOSING; 3=NET_WINDOW_MOTION_ANTIPINCH_REVERSING; 255=NET_WINDOW_MOTION_UNKNOWN; 단위=NOT_SPECIFIED | 255=UNKNOWN | 425 |
| 9 / 1 | `WINDOW_ECU_STATE` → `window_ecu_state` (`NetControllerState`) | u8 / unsigned / N/A | 0=NET_CONTROLLER_INIT; 1=NET_CONTROLLER_READY; 2=NET_CONTROLLER_DEGRADED; 3=NET_CONTROLLER_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 426 |
| 10 / 1 | `WINDOW_POSITION` → `position_closed_percent` (`uint8_t`) | u8 / unsigned / N/A | 0~100% 닫힘 비율,255=확인 불가; 단위=% (1 percent/raw) | 255=확인 불가 | 427 |
| 11 / 1 | `WINDOW_VALUE_QUALITY` → `window_value_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 428 |
| 12 / 1 | `FULLY_OPEN_STATE` → `fully_open_state` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 429 |
| 13 / 1 | `FULLY_CLOSED_STATE` → `fully_closed_state` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 430 |
| 14 / 1 | `ANTIPINCH_PROTECTION_STATUS` → `antipinch_protection_status` (`NetAntiPinchProtectionState`) | u8 / unsigned / N/A | 0=NET_ANTIPINCH_PROTECTION_IDLE; 1=NET_ANTIPINCH_PROTECTION_IN_PROGRESS; 2=NET_ANTIPINCH_PROTECTION_DONE; 3=NET_ANTIPINCH_PROTECTION_INTERRUPTED; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 431 |
| 15 / 1 | `REVERSE_STATE` → `reverse_state` (`NetTriState`) | u8 / unsigned / N/A | 0=NET_TRISTATE_NO; 1=NET_TRISTATE_YES; 255=NET_TRISTATE_UNKNOWN; 단위=NOT_SPECIFIED | 255=확인 불가 | 432 |
| 16 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | 새 상태 평가; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 433 |
| 20 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 434 |

motion_state0..3/255, ecu_state0..3, position0..100/255, quality0..3, 세 tri-state0/1/255, protection0..3만 허용한다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `WindowState_t.channel` / `uint8_t` | Wire에 직접 필드 없음; 현재 단일 채널0 연결은 후보/실행 HOLD |
| `WindowState_t.motionState` / `WindowMotionState_t` | MOTION_STATE; T39 이름 대응 |
| `WindowState_t.ecuState` / `CommonEcuState_t` | WINDOW_ECU_STATE; 이름 대응 |
| `WindowState_t.positionPercent` / `uint8_t` | WINDOW_POSITION; 닫힘 비율,255 UNKNOWN은 정상 위치 아님 |
| `WindowState_t.fullyOpen` / `bool` | FULLY_OPEN_STATE; 0/1 변환,255는 true/false 정상 값으로 복원 불가 |
| `WindowState_t.fullyClosed` / `bool` | FULLY_CLOSED_STATE; 0/1 변환,255는 true/false 정상 값으로 복원 불가 |
| `WindowState_t.antiPinchState` / `AntiPinchState_t` | ANTIPINCH_PROTECTION_STATUS 및 REVERSE_STATE와 C2 CLEAR/ACTIVE는 동등 필드가 아님; 투영 NOT_SPECIFIED/HOLD |
| `WindowState_t.meta` / `SignalMeta_t` | WINDOW_VALUE_QUALITY/UPDATE_SEQUENCE/SOURCE_AGE의 투영 후보 |
| `WindowState_t.sourceGeneration` / `SourceGeneration_t` | 원 WINDOW BOOT_ID 연결 후보; 실행/바인딩 HOLD |

motionState는 raw255→WINDOW_MOTION_UNKNOWN0, raw0..3→STOPPED/OPENING/CLOSING/ANTIPINCH_REVERSING 이름 대응이다. ecuState·positionPercent·meta는 해당 원 필드다. channel은 Wire에 없고 단일채널0 바인딩이 필요하다. fullyOpen/fullyClosed는0/1에만 대응한다. protection/reverse를 CLEAR/ACTIVE로 임의 축약하지 않는다. sourceGeneration은 원 WINDOW 계보 확인 전 합성하지 않는다.

**적용 제약:** WINDOW_EXECUTION_HOLD, IF-O07, IF-O18, IF-O24, G05: 보호 상태4종+역전 여부를 C2 AntiPinchState_t 두 값으로 축약할 근거가 없다. 끝단 UNKNOWN255의 bool 표현도 미확인이다.

[함수와 양단 API](../functions/can/WINDOW_STATE.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_STATE)

<a id="WINDOW_EVENT"></a>
## WINDOW_EVENT: 끝단 전이와 끼임 사건

WINDOW→DOMAIN, 후보 ID `0x091`, used `26`B / frame `32`B / DLC `13`. Padding: 26..31, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. WINDOW 기동+event_kind+occurrence_id와 related_domain_boot/related_request_sequence를 구분한다. 로컬 발생의 연결0은 실제 무연계다. event_age_ms는 최초 전이/끼임 발생 후 시간이다. 반복 송신으로 새 발생번호를 만들지 않는다.

<a id="type-NetWindowEvent"></a>
### `NetWindowEvent`

```c
typedef struct NetWindowEvent
{
    NetWindowEventKind event_kind;
    uint8_t target_channel;
    uint32_t occurrence_id;
    uint32_t related_domain_boot;
    uint32_t related_request_sequence;
    NetOrigin origin;
    uint16_t event_age_ms;
    NetValueQuality event_quality;
} NetWindowEvent;
```

근거: S4A data/T31.md#type-NetWindowEvent. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `event_kind` | `NetWindowEventKind`; 8비트 부호 없음 | 1 FULLY_OPENED/2 FULLY_CLOSED/3 ANTIPINCH | WINDOW 실제 새 전이·끼임 담당자; 현재 State 재송신을 Event로 만들지 않음 |
| `target_channel` | `uint8_t`; 8비트 부호 없음 | 현재 채널0 | WINDOW 발생 담당자; 원 명령 채널과 대응 |
| `occurrence_id` | `uint32_t`; 32비트 부호 없음 | 새 끝단 전이 또는 끼임 발생 번호 | WINDOW 발생 담당자; WINDOW 기동 + occurrence로 식별. 재전송은 동일 발생 |
| `related_domain_boot` | `uint32_t`; 32비트 부호 없음 | 관련 명령의 원 DOMAIN 기동; 연결 없으면0 | 수용한 원 명령 문맥 echo; 로컬 사건에 임의 DOMAIN 문맥 생성 금지 |
| `related_request_sequence` | `uint32_t`; 32비트 부호 없음 | 원 DOMAIN 명령 연결; 연결 없으면0 | 수용 명령 ID echo. C2 commandId와의 정확 대응은 TEAM_DECISION_REQUIRED |
| `origin` | `NetOrigin`; 8비트 부호 없음 | 발생·상위 요청 출처 code | WINDOW 실제 발생 담당자 + 보존한 DOMAIN origin; SOURCE_CONTEXT와 자동 동일시 금지 |
| `event_age_ms` | `uint16_t`; 16비트 부호 없음 | 최초 Event 발생 후 ms, uint16 포화 | WINDOW 원 발생 + Network 대기만 누적 |
| `event_quality` | `NetValueQuality`; 8비트 부호 없음 | Event 원 발생 근거 품질 | WINDOW 발생 담당자; 수신 성공을 실제 보호 성공으로 변환 금지 |

<a id="wire-WINDOW_EVENT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/WINDOW_EVENT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 452 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 452 |
| 8 / 1 | `EVENT_KIND` → `event_kind` (`NetWindowEventKind`) | u8 / unsigned / N/A | 1=NET_WINDOW_EVENT_FULLY_OPENED; 2=NET_WINDOW_EVENT_FULLY_CLOSED; 3=NET_WINDOW_EVENT_ANTIPINCH; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 453 |
| 9 / 1 | `TARGET_CHANNEL` → `target_channel` (`uint8_t`) | u8 / unsigned / N/A | 현재 0; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 454 |
| 10 / 4 | `OCCURRENCE_ID` → `occurrence_id` (`uint32_t`) | u32 / unsigned / LE | 새 전이/끼임 발생 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 455 |
| 14 / 4 | `RELATED_DOMAIN_BOOT` → `related_domain_boot` (`uint32_t`) | u32 / unsigned / LE | 연결 명령의 Domain 기동 번호; 로컬은 0; 단위=NOT_SPECIFIED | 0 = 관련 명령/사건 없음 (원문 규칙) | 456 |
| 18 / 4 | `RELATED_REQUEST_SEQUENCE` → `related_request_sequence` (`uint32_t`) | u32 / unsigned / LE | Domain 명령 연계; 로컬 발생은 0; 단위=NOT_SPECIFIED | 0 = 관련 명령/사건 없음 (원문 규칙) | 457 |
| 22 / 1 | `ORIGIN` → `origin` (`NetOrigin`) | u8 / unsigned / N/A | 0=NET_ORIGIN_NONE; 1=NET_ORIGIN_MOBILE_MANUAL; 2=NET_ORIGIN_PROXIMITY_AUTO; 3=NET_ORIGIN_LOCAL; 4=NET_ORIGIN_TEST; 5=NET_ORIGIN_VEHICLE_POLICY; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 458 |
| 23 / 2 | `EVENT_AGE` → `event_age_ms` (`uint16_t`) | u16 / unsigned / LE | 발생 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 459 |
| 25 / 1 | `EVENT_QUALITY` → `event_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 460 |

event_kind1..3, channel0, origin0..5, event_quality0..3만 허용한다. related_* 원문0 규칙을 그대로 보존한다.

기존 DomainNetPriv_OnWindowEvent가 원 DTO/meta를 같은 사건으로 보존한다. ANTIPINCH에만 DomainIf_RxWindowAntiPinch 후보를 연결한다. C2 occurrenceId는 원 발생번호, meta는 원 품질·AGE 근거이나 outputDisabled/movementStopped/protectionState/reason을 이 Event Wire만으로 완성할 수 없다. END를 AntiPinch로 인계하지 않는다.

**적용 제약:** WINDOW_EXECUTION_HOLD, IF-O07, IF-O18, IF-O24, G05: END 소비자와 requestSequence↔commandId 귀속 및 AntiPinch 보호 근거가 미합의다.

[함수와 양단 API](../functions/can/WINDOW_EVENT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_EVENT)

<a id="WINDOW_RESULT"></a>
## WINDOW_RESULT: 명령 결과와 원 명령 귀속

WINDOW→DOMAIN, 후보 ID `0x092`, used `20`B / frame `20`B / DLC `11`. Padding: 없음, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. request_domain_boot+request_sequence+channel은 원 DOMAIN 명령 연결 후보이며 WINDOW 송신 BOOT/TX와 다르다. 이 Wire에는 결과 원 평가 AGE/UPDATE가 없다. C2 meta를 TX_SEQUENCE나 최근 수신 시각으로 생성하지 않는다.

<a id="type-NetWindowResult"></a>
### `NetWindowResult`

```c
typedef struct NetWindowResult
{
    uint32_t request_domain_boot;
    uint32_t request_sequence;
    uint8_t target_channel;
    NetVehicleResult result;
    NetReason result_reason;
} NetWindowResult;
```

근거: S4A data/T31.md#type-NetWindowResult. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `request_domain_boot` | `uint32_t`; 32비트 부호 없음 | 결과의 원 명령을 만든 DOMAIN 기동 | WINDOW 수용 명령 문맥 echo; 현재 DOMAIN 재기동과 같다고 가정 금지 |
| `request_sequence` | `uint32_t`; 32비트 부호 없음 | 결과의 원 DOMAIN 명령 식별 | WINDOW 명령 echo; C2 commandId 대응은 TEAM_DECISION_REQUIRED, 새 발급 금지 |
| `target_channel` | `uint8_t`; 8비트 부호 없음 | 원 명령 채널; 현재0 | WINDOW 실행 담당자; 원 명령과 일치 |
| `result` | `NetVehicleResult`; 8비트 부호 없음 | 실제 차량 실행 Result code | WINDOW 실행 담당자; local OK/CAN ACK로 DONE 생성 금지 |
| `result_reason` | `NetReason`; 16비트 부호 없음 | 실제 수용·실행·취소·실패 사유 | WINDOW 실행 담당자; Result와 함께 해석 |

<a id="type-DomainIf_WindowCommandResult_t"></a>
### `DomainIf_WindowCommandResult_t`

```c
typedef struct
{
    DomainCommandId_t commandId;
    RequestResult_t result;
    ResultReason_t reason;
    uint8_t channel;
    SignalMeta_t meta;
} DomainIf_WindowCommandResult_t;
```

근거: S4A data/T14.md#type-DomainIf_WindowCommandResult_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `commandId` | `DomainCommandId_t`; 32비트 부호 없음 | Domain 기능 결정 담당자의 단일 Command 식별 u32. Network 에서 새로 발급하지 않으며 TX_SEQUENCE·requestId와 다르다. | DOMAIN 원 명령의 ID를 WINDOW 결과가 echo하고, 승인된 연관표로 DOMAIN Network가 연결한다. Network 새 ID 발급 금지; 대응 미합의 HOLD. |
| `result` | `RequestResult_t`; C enum 저장 폭 미검증 | 기능 담당자가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | WINDOW 실제 실행·결과 Owner 생성 → DOMAIN Network 의미 투영. local OK/CAN ACK를 차량 DONE으로 만들지 않는다. |
| `reason` | `ResultReason_t`; C enum 저장 폭 미검증 | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | WINDOW 실제 실행·결과 Owner 생성 → DOMAIN Network 명시 이름 대응. 예약값·표현 불가 사유는 거부 또는 기존 HOLD. |
| `channel` | `uint8_t`; 8비트 부호 없음 | WINDOW 채널 u8. 지원 채널 범위는 구현 직전 합의; 현재 구현 보류. | WINDOW 실제 실행·결과 Owner가 수용한 원 명령 채널을 보존 → DOMAIN Network 투영. WINDOW 실행 HOLD. |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | 원 WINDOW 결과 평가 근거의 확인·바인딩이 필요하다. 이 Wire에는 원 결과 품질·AGE·UPDATE가 없으므로 TX_SEQUENCE·수신시각·AGE 0으로 합성하지 않고 기존 HOLD. |

<a id="wire-WINDOW_RESULT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/WINDOW_RESULT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 478 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 478 |
| 8 / 4 | `REQUEST_DOMAIN_BOOT` → `request_domain_boot` (`uint32_t`) | u32 / unsigned / LE | 원 명령을 생성한 Domain 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 479 |
| 12 / 4 | `REQUEST_SEQUENCE` → `request_sequence` (`uint32_t`) | u32 / unsigned / LE | 원 Domain 명령 식별; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 480 |
| 16 / 1 | `TARGET_CHANNEL` → `target_channel` (`uint8_t`) | u8 / unsigned / N/A | 현재 0; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 481 |
| 17 / 1 | `RESULT` → `result` (`NetVehicleResult`) | u8 / unsigned / N/A | 0=NET_RESULT_ACCEPTED; 1=NET_RESULT_IN_PROGRESS; 2=NET_RESULT_DONE; 3=NET_RESULT_REJECTED; 4=NET_RESULT_CANCELLED; 5=NET_RESULT_FAILED; 타입의 255=NOT_PRESENT는 차량 결과 단계가 아니며 CAN 결과 사용은 NOT_SPECIFIED/HOLD; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 482 |
| 18 / 2 | `RESULT_REASON` → `result_reason` (`NetReason`) | u16 / unsigned / LE | 0=NET_REASON_NONE; 1=NET_REASON_NOT_REGISTERED; 2=NET_REASON_LINK_LOST; 3=NET_REASON_SESSION_INVALID; 4=NET_REASON_DOOR_OPEN; 5=NET_REASON_STATE_UNTRUSTED; 6=NET_REASON_CMD_INVALID; 7=NET_REASON_NO_FEEDBACK; 8=NET_REASON_DRIVE_LIMIT_EXCEEDED; 9=NET_REASON_ALREADY_AT_TARGET; 10=NET_REASON_LOCAL_OVERRIDE; 11=NET_REASON_STOP_REQUESTED; 12=NET_REASON_SUPERSEDED; 13=NET_REASON_ANTIPINCH; 14=NET_REASON_FAULT; 15=NET_REASON_STALE; 16=NET_REASON_NO_DATA; 17=NET_REASON_NOT_READY; 18=NET_REASON_OUT_OF_RANGE; 19=NET_REASON_SOURCE_FAILURE; 20=NET_REASON_UNKNOWN_REQUEST; 21=NET_REASON_EXPIRED; 22=NET_REASON_ID_CONFLICT; 23=NET_REASON_POWER_NOT_ALLOWED; 24=NET_REASON_OVERHEAT; 25=NET_REASON_FAN_MISMATCH; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 483 |

channel0, 차량 result0..5, reason0..25만 문서상 표현한다. NetVehicleResult에 정의된255 NOT_PRESENT는 CAN 결과 단계로 승인되지 않아 HOLD다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_WindowCommandResult_t.commandId` / `DomainCommandId_t` | REQUEST_SEQUENCE와 대응 미합의; Network 새 ID 발급 금지 |
| `DomainIf_WindowCommandResult_t.result` / `RequestResult_t` | RESULT; 차량 처리 단계 이름 대응, NOT_PRESENT context HOLD |
| `DomainIf_WindowCommandResult_t.reason` / `ResultReason_t` | RESULT_REASON; 이름 대응/표현 불가 사유 HOLD |
| `DomainIf_WindowCommandResult_t.channel` / `uint8_t` | TARGET_CHANNEL; 현재0 |
| `DomainIf_WindowCommandResult_t.meta` / `SignalMeta_t` | 별도 원 평가 메타가 Wire에 없으며 TX_SEQUENCE를 원 평가번호로 사용 금지; NOT_SPECIFIED/HOLD |

commandId는 원 명령의 확정된 대응표에서만 연결한다. result/reason은 의미 이름 변환을 쓰고 C2 전용 MOTOR_FAULT/SENSOR_FAULT/TIMEOUT/OTHER 등을 임의 축약하지 않는다. channel0은 유지한다. SignalMeta 전체 근거가 없으면 DomainIf_RxWindowCommandResult를 호출하지 않는다.

**적용 제약:** WINDOW_EXECUTION_HOLD, IF-O02, IF-O07, IF-O24, CAN_RESULT_NOT_PRESENT_CONTEXT, G05: REQUEST_SEQUENCE와 commandId 대응·결과 meta 생성 근거가 미합의다.

[함수와 양단 API](../functions/can/WINDOW_RESULT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_RESULT)

<a id="WINDOW_FAULT"></a>
## WINDOW_FAULT: 현재 고장과 최근 발생 근거

WINDOW→DOMAIN, 후보 ID `0x330`, used `22`B / frame `24`B / DLC `12`. Padding: 22..23, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. WINDOW 원 생산 세대+occurrence_id와 update_sequence를 별도로 보존한다. source_age_ms는 고장 평가 후 시간이다. 고장 발생 시각이나 last event AGE를 대신하지 않는다.

<a id="type-NetWindowFault"></a>
### `NetWindowFault`

```c
typedef struct NetWindowFault
{
    NetFaultCategory fault_category;
    NetWindowFaultCode fault_code;
    NetWindowFaultState fault_status;
    uint32_t occurrence_id;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetWindowFault;
```

근거: S4A data/T31.md#type-NetWindowFault. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `fault_category` | `NetFaultCategory`; 8비트 부호 없음 | 0 없음/1 SENSOR/2 FUNCTION/3 COMM | WINDOW 기능 진단; COMM 원 근거는 WINDOW Network 관측. C2 enum cast 금지 |
| `fault_code` | `NetWindowFaultCode`; 16비트 부호 없음 | 0 없음/1 위치/2 구동/3 끝단/4 보호/5 DOMAIN timeout/6 초기화/255기타 후보 | WINDOW 진단 담당자; 발생 조건·코드 채택 TEAM_DECISION_REQUIRED |
| `fault_status` | `NetWindowFaultState`; 8비트 부호 없음 | 0 현재고장 없음/1 활성/2 복구중 | WINDOW 진단 담당자; 없음과 최근 occurrence 보존은 공존 가능 |
| `occurrence_id` | `uint32_t`; 32비트 부호 없음 | 현재 또는 최근 고장 원 발생 번호 | WINDOW 진단 담당자; 현재고장 해제와 이력 삭제를 동일시하지 않음 |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 실제 새 상태·고장 평가 번호, uint32 | WINDOW 평가 담당자; CAN TX_SEQUENCE와 별개, cached 반복 때 유지 |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 원 상태·고장 평가 이후 누적 ms, uint16 포화 | WINDOW 원본 평가 + Network 대기만 누적; 재전송으로 초기화 금지 |

<a id="wire-WINDOW_FAULT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/WINDOW_FAULT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 498 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 498 |
| 8 / 1 | `FAULT_CATEGORY` → `fault_category` (`NetFaultCategory`) | u8 / unsigned / N/A | 0=NET_FAULT_CATEGORY_NONE; 1=NET_FAULT_CATEGORY_SENSOR; 2=NET_FAULT_CATEGORY_FUNCTION; 3=NET_FAULT_CATEGORY_COMM; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 499 |
| 9 / 2 | `FAULT_CODE` → `fault_code` (`NetWindowFaultCode`) | u16 / unsigned / LE | 0=NET_WINDOW_FAULT_NONE; 1=NET_WINDOW_FAULT_POSITION; 2=NET_WINDOW_FAULT_DRIVE; 3=NET_WINDOW_FAULT_ENDPOINT; 4=NET_WINDOW_FAULT_PROTECTION; 5=NET_WINDOW_FAULT_DOMAIN_TIMEOUT; 6=NET_WINDOW_FAULT_INITIALIZATION; 255=NET_WINDOW_FAULT_OTHER; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 500 |
| 11 / 1 | `FAULT_STATUS` → `fault_status` (`NetWindowFaultState`) | u8 / unsigned / N/A | 0=NET_WINDOW_FAULT_STATE_NONE; 1=NET_WINDOW_FAULT_STATE_ACTIVE; 2=NET_WINDOW_FAULT_STATE_RECOVERING; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 501 |
| 12 / 4 | `OCCURRENCE_ID` → `occurrence_id` (`uint32_t`) | u32 / unsigned / LE | 현재/최근 고장 연결; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 502 |
| 16 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | 상태 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 503 |
| 20 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 504 |

category0..3, fault_code0..6/255, fault_status0..2와 정의된 원 식별자를 보존한다.

DomainNetPriv_OnWindowFault는 NetWindowFault/meta의 전체 근거를 보존한다. 확인된 category/code/status 이름을 FaultInfo_t에 투영할 수 있는지 먼저 확인하며 원 발생·AGE·세대의 손실을 숨기지 않는다. 실제 동일 입력의 C2 반영 증거가 없으면 raw↔snapshot 연결을 만들지 않는다.

**적용 제약:** WINDOW_EXECUTION_HOLD, IF-O07, IF-O15, IF-O24, WINDOW_FAULT_PROJECTION, G05: 좁은 FaultInfo 투영·발생 계보 바인딩은 미합의다.

[함수와 양단 API](../functions/can/WINDOW_FAULT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#WINDOW_FAULT)

<a id="VSS_EVENT"></a>
## VSS_EVENT: 의미 사건의 한 번 전달

DOMAIN→VSS, 후보 ID `0x160`, used `23`B / frame `24`B / DLC `12`. Padding: 23..23, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. DOMAIN 원 생산 세대+event_kind+occurrence_id가 의미 사건이다. related_command_id는 별도 명령 연결이다. original_age_ms는 최초 사건 후 경과, use_limit_ms는 원 사용 한도다. 후보500ms와 구 실제 음향 시작2초 요구의 기준을 임의 통합하지 않는다.

<a id="type-NetVssEvent"></a>
### `NetVssEvent`

```c
typedef struct NetVssEvent
{
    NetVssEventKind event_kind;
    uint32_t occurrence_id;
    uint16_t original_age_ms;
    uint16_t use_limit_ms;
    NetValueQuality value_quality;
    NetAvailability input_availability;
    uint32_t related_command_id;
} NetVssEvent;
```

근거: S4A data/T28.md#type-NetVssEvent. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `event_kind` | `NetVssEventKind`; 8비트 부호 없음 | 완료·오류·웰컴·굿바이 의미; 허용 코드 1~5; 단위: 의미 코드 · 무단위 | DOMAIN 기능 판단; 입력 품질·가용성·사용 한도를 함께 확인 |
| `occurrence_id` | `uint32_t`; 32비트 부호 없음 | 기동 내 일회성 발생 번호, uint32 범위; 단위: 발생 식별자 · 무단위 | DOMAIN 발생 담당자; 같은 발생 재전달 때 유지. 수용된 DOMAIN 기동과 함께 중복 판정 |
| `original_age_ms` | `uint16_t`; 16비트 부호 없음 | 최초 의미 발생 이후 누적 ms, 0~65535 포화; 단위: ms | DOMAIN 원본 + 각 Network 구간 대기만 1회 누적; 새 RX로 0 재설정 금지 |
| `use_limit_ms` | `uint16_t`; 16비트 부호 없음 | 일회성 사용 한도 ms; 500ms 후보(PROPOSED); 단위: ms | Network profile500ms↔구 SysRS 실제 음향시작2초: CONFLICT/TEAM_DECISION_REQUIRED. 수용·시작 기준 합의 필요 |
| `value_quality` | `NetValueQuality`; 8비트 부호 없음 | 의미 입력 품질; OK/STALE/INVALID/NO_DATA; 단위: 의미 코드 · 무단위 | DOMAIN 의미 담당자; 정상 의미값과 독립 |
| `input_availability` | `NetAvailability`; 8비트 부호 없음 | 의미 생성 기능의 가용성; 0 가용/1 제한/2 불가; 단위: 의미 코드 · 무단위 | DOMAIN 기능 담당자; VSS 서비스 가용성과 구분 |
| `related_command_id` | `uint32_t`; 32비트 부호 없음 | 원 DOMAIN 명령 ID; 연결이 없으면 0; 단위: 명령 식별자 · 무단위 | DOMAIN C2 명령 담당자; 새 발급 금지, occurrence·MOBILE RequestId와 별개 |

<a id="type-DomainIf_VssOneShotEvent_t"></a>
### `DomainIf_VssOneShotEvent_t`

```c
typedef struct
{
    VssOneShotEvent_t event;
    OccurrenceId_t occurrenceId;
    SignalMeta_t meta; /* quality/original age; updateSequence is ignored by
        VSS_EVENT codec and occurrence dedup/replay, but semantic zero is not forced */
    FunctionAvailability_t availability;
    bool hasRelatedCommand;
    DomainCommandId_t relatedCommandId;
} DomainIf_VssOneShotEvent_t;
```

근거: S4A data/T14.md#type-DomainIf_VssOneShotEvent_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `event` | `VssOneShotEvent_t`; C enum 저장 폭 미검증 | Domain이 확정한 VSS 일회 음향 Event. occurrence·원 quality/AGE·availability와 함께 사용. | Domain 기능 결정/설정/결과 담당자 |
| `occurrenceId` | `OccurrenceId_t`; 32비트 부호 없음 | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | Domain 기능 결정/설정/결과 담당자 |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | Domain 기능 결정/설정/결과 담당자 |
| `availability` | `FunctionAvailability_t`; C enum 저장 폭 미검증 | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | Domain 기능 결정/설정/결과 담당자 |
| `hasRelatedCommand` | `bool`; 저장 폭은 ABI 미검증 | 원 Domain 명령 연결 여부 bool. false 이면 relatedCommandId를 해석하지 않는다. | Domain 기능 결정/설정/결과 담당자 |
| `relatedCommandId` | `DomainCommandId_t`; 32비트 부호 없음 | 있으면 원 Domain Command ID echo. 로컬 사건에 가짜 명령 ID 생성 금지. | Domain 기능 결정/설정/결과 담당자 |

<a id="wire-VSS_EVENT"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/VSS_EVENT.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 518 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 518 |
| 8 / 1 | `EVENT_KIND` → `event_kind` (`NetVssEventKind`) | u8 / unsigned / N/A | 1=NET_VSS_EVENT_WELCOME; 2=NET_VSS_EVENT_GOODBYE; 3=NET_VSS_EVENT_LOCK_COMPLETE; 4=NET_VSS_EVENT_UNLOCK_COMPLETE; 5=NET_VSS_EVENT_LOCK_ERROR; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 519 |
| 9 / 4 | `OCCURRENCE_ID` → `occurrence_id` (`uint32_t`) | u32 / unsigned / LE | Domain 기동 내 의미 이벤트 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 520 |
| 13 / 2 | `ORIGINAL_AGE` → `original_age_ms` (`uint16_t`) | u16 / unsigned / LE | 이벤트 원본 발생 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 521 |
| 15 / 2 | `USE_LIMIT` → `use_limit_ms` (`uint16_t`) | u16 / unsigned / LE | 일회성 이벤트 사용 한도; 초안 500ms; 단위=ms | NOT_SPECIFIED | 522 |
| 17 / 1 | `VALUE_QUALITY` → `value_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 523 |
| 18 / 1 | `INPUT_AVAILABILITY` → `input_availability` (`NetAvailability`) | u8 / unsigned / N/A | 0=NET_AVAILABILITY_AVAILABLE; 1=NET_AVAILABILITY_LIMITED; 2=NET_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 524 |
| 19 / 4 | `RELATED_COMMAND_ID` → `related_command_id` (`uint32_t`) | u32 / unsigned / LE | 도어 명령 등 연계; 없으면 0; 단위=NOT_SPECIFIED | 0 = 관련 명령/사건 없음 (원문 규칙) | 525 |

event_kind1..5, value_quality0..3, input_availability0..2를 보존한다. 관련 명령이 없으면 wire RELATED_COMMAND_ID0 후보를 적용하고 C2 hasRelatedCommand=false인 stale 숫자는 읽지 않는다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_VssOneShotEvent_t.event` / `VssOneShotEvent_t` | EVENT_KIND; C0..4→raw1..5 이름 대응 |
| `DomainIf_VssOneShotEvent_t.occurrenceId` / `OccurrenceId_t` | OCCURRENCE_ID; 의미 사건 식별자 |
| `DomainIf_VssOneShotEvent_t.meta` / `SignalMeta_t` | quality/original AGE→VALUE_QUALITY/ORIGINAL_AGE; updateSequence는 이 Wire의 식별자가 아님 |
| `DomainIf_VssOneShotEvent_t.availability` / `FunctionAvailability_t` | INPUT_AVAILABILITY; UNSUPPORTED에 동등 raw 없음, HOLD |
| `DomainIf_VssOneShotEvent_t.hasRelatedCommand` / `bool` | false일 때 RELATED_COMMAND_ID0 후보; 실제 원 연계 근거 보존 |
| `DomainIf_VssOneShotEvent_t.relatedCommandId` / `DomainCommandId_t` | RELATED_COMMAND_ID; hasRelatedCommand와 정합성 필요 |

C2 VSS_EVENT_VEHICLE_WELCOME/GOODBYE/DOOR_LOCK_COMPLETE/DOOR_UNLOCK_COMPLETE/DOOR_LOCK_ERROR0..4를 Net 대응 이름1..5로 바꾼다. occurrenceId는 보존하고 meta.quality와 meta.ageMs만 원 품질/AGE로 연결한다. meta.updateSequence는 이 Wire와 중복 판정에 사용하지 않으며 원 DTO 값을0으로 강제하지 않는다. availability3 UNSUPPORTED는 HOLD다. hasRelatedCommand=true일 때 실제 relatedCommandId 연결을 보존한다.

**적용 제약:** A1 최초 actual 기한은 원 발생부터 `L=min(검증된 양의 USE_LIMIT,500ms,2000ms)`이며 L 도달 시 만료, USE_LIMIT0은 사용 불가다. 의미 Key는 수용된 DOMAIN Boot+occurrence(필요한 PRODUCT/TEST 문맥 포함)이고 event_kind는 같은 Key의 내용 충돌 검사다. 실제 clock/first actual·필수 후발 실패 sink·기동 수명은 E01~05 확인 대기다. C2 Event UNSUPPORTED 가용성에는 동등 raw가 없다.

[함수와 양단 API](../functions/can/VSS_EVENT.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#VSS_EVENT)

<a id="VSS_WARNING_STATE"></a>
## VSS_WARNING_STATE: 세 경고 근거와 독립 활성 평가

DOMAIN→VSS, 후보 ID `0x0C0`, used `60`B / frame `64`B / DLC `15`. Padding: 60..63, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. 끼임·탑승자·후방 각 source_boot/source_sequence와 DOMAIN rear_activation_sequence는 서로 다른 판단 근거다. 세 original_age_ms와 rear_activation_age_ms를 각각 누적한다. 후방 거리 근거의 시각을 활성 평가의 시각으로 쓰지 않는다.

<a id="type-NetVssWarningState"></a>
### `NetVssWarningState`

```c
typedef struct NetVssWarningState
{
    uint32_t antipinch_source_boot;
    uint32_t antipinch_source_sequence;
    uint16_t antipinch_original_age_ms;
    NetFlag antipinch_state;
    NetValueQuality antipinch_quality;
    NetQualityReason antipinch_reason;
    NetAvailability antipinch_availability;
    uint32_t occupant_hazard_source_boot;
    uint32_t occupant_hazard_source_sequence;
    uint16_t occupant_hazard_original_age_ms;
    NetFlag occupant_hazard_state;
    NetValueQuality occupant_hazard_quality;
    NetQualityReason occupant_hazard_reason;
    NetAvailability occupant_hazard_availability;
    uint32_t rear_obstacle_source_boot;
    uint32_t rear_obstacle_source_sequence;
    uint16_t rear_obstacle_original_age_ms;
    NetRearWarningState rear_obstacle_state;
    NetValueQuality rear_obstacle_quality;
    NetQualityReason rear_obstacle_reason;
    NetAvailability rear_obstacle_availability;
    NetFlag rear_detection_activation;
    uint8_t activation_meta_format;
    NetValueQuality rear_activation_quality;
    NetQualityReason rear_activation_reason;
    uint32_t rear_activation_sequence;
    uint16_t rear_activation_age_ms;
} NetVssWarningState;
```

근거: S4A data/T28.md#type-NetVssWarningState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `antipinch_source_boot` | `uint32_t`; 32비트 부호 없음 | 끼임 원 판단 근거 노드 기동 식별; 단위: 기동 식별자 · 무단위 | WINDOW 원 보호 근거 담당자; 제품경로 DEFERRED→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `antipinch_source_sequence` | `uint32_t`; 32비트 부호 없음 | 끼임 원 근거의 실제 새 평가 번호; 단위: 평가 번호 · 무단위 | 원 evidence 담당자; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `antipinch_original_age_ms` | `uint16_t`; 16비트 부호 없음 | 끼임 원본 관측 이후 누적 ms, uint16 포화; 단위: ms | 원본 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `antipinch_state` | `NetFlag`; 8비트 부호 없음 | 끼임 DOMAIN 확정 현재 의미; 단위: 0/1 의미값 · 무단위 | DOMAIN 위험 판단 담당자→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `antipinch_quality` | `NetValueQuality`; 8비트 부호 없음 | 끼임 입력 근거 품질; 단위: 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `antipinch_reason` | `NetQualityReason`; 8비트 부호 없음 | 끼임 품질 사유; 단위: 의미 코드 · 무단위 | 해당 evidence 평가 담당자; 무효·만료 사유 보존 |
| `antipinch_availability` | `NetAvailability`; 8비트 부호 없음 | 끼임 기능 가용성 0/1/2; 단위: 의미 코드 · 무단위 | 해당 기능 담당자; quality와 별개. WINDOW 제품 경로는 DEFERRED |
| `occupant_hazard_source_boot` | `uint32_t`; 32비트 부호 없음 | 탑승자 위험 원 판단 근거 노드 기동 식별; 단위: 기동 식별자 · 무단위 | Pi 원 비전 관측 담당자; CIS 추가 품질평가와 별개, C2 lineage 매핑 OPEN→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `occupant_hazard_source_sequence` | `uint32_t`; 32비트 부호 없음 | 탑승자 위험 원 근거의 실제 새 평가 번호; 단위: 평가 번호 · 무단위 | 원 evidence 담당자; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `occupant_hazard_original_age_ms` | `uint16_t`; 16비트 부호 없음 | 탑승자 위험 원본 관측 이후 누적 ms, uint16 포화; 단위: ms | 원본 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `occupant_hazard_state` | `NetFlag`; 8비트 부호 없음 | 탑승자 위험 DOMAIN 확정 현재 의미; 단위: 0/1 의미값 · 무단위 | DOMAIN 위험 판단 담당자→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `occupant_hazard_quality` | `NetValueQuality`; 8비트 부호 없음 | 탑승자 위험 입력 근거 품질; 단위: 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `occupant_hazard_reason` | `NetQualityReason`; 8비트 부호 없음 | 탑승자 위험 품질 사유; 단위: 의미 코드 · 무단위 | 해당 evidence 평가 담당자; 무효·만료 사유 보존 |
| `occupant_hazard_availability` | `NetAvailability`; 8비트 부호 없음 | 탑승자 위험 기능 가용성 0/1/2; 단위: 의미 코드 · 무단위 | 해당 기능 담당자; quality와 별개. |
| `rear_obstacle_source_boot` | `uint32_t`; 32비트 부호 없음 | 후방 위험 원 판단 근거 노드 기동 식별; 단위: 기동 식별자 · 무단위 | CIS 후방 원 측정 근거 담당자→DOMAIN 보존→VSS 소비. DOMAIN 링크 Boot로 치환 금지 |
| `rear_obstacle_source_sequence` | `uint32_t`; 32비트 부호 없음 | 후방 위험 원 근거의 실제 새 평가 번호; 단위: 평가 번호 · 무단위 | 원 evidence 담당자; DOMAIN 재포장·CAN 재전송으로 새 번호 생성 금지 |
| `rear_obstacle_original_age_ms` | `uint16_t`; 16비트 부호 없음 | 후방 위험 원본 관측 이후 누적 ms, uint16 포화; 단위: ms | 원본 관측 + 각 중계 대기 1회 누적; 세 경고 독립 Age |
| `rear_obstacle_state` | `NetRearWarningState`; 8비트 부호 없음 | 후방 위험 DOMAIN 확정 현재 의미; 단위: 의미 코드 · 무단위 | DOMAIN 위험 판단 담당자→VSS 음향 중재. 품질상실·음향중단을 CLEAR로 합성 금지 |
| `rear_obstacle_quality` | `NetValueQuality`; 8비트 부호 없음 | 후방 위험 입력 근거 품질; 단위: 의미 코드 · 무단위 | 해당 원 evidence + DOMAIN 판단; 다른 경고의 품질과 독립 |
| `rear_obstacle_reason` | `NetQualityReason`; 8비트 부호 없음 | 후방 위험 품질 사유; 단위: 의미 코드 · 무단위 | 해당 evidence 평가 담당자; 무효·만료 사유 보존 |
| `rear_obstacle_availability` | `NetAvailability`; 8비트 부호 없음 | 후방 위험 기능 가용성 0/1/2; 단위: 의미 코드 · 무단위 | 해당 기능 담당자; quality와 별개. |
| `rear_detection_activation` | `NetFlag`; 8비트 부호 없음 | 0 DISABLED/1 ACTIVE; 위험 CLEAR와 독립; 단위: 0/1 의미값 · 무단위 | DOMAIN 실제 활성 정책 담당자; 활성 품질·근거 한도 함께 검사 |
| `activation_meta_format` | `uint8_t`; 8비트 부호 없음 | 0 근거미지원/1 활성 meta 형식/그외 미지원; 단위: 의미 코드 · 무단위 | DOMAIN/VSS Codec format marker; App 위험값이 아님.1 양측 동시 이관 필요 |
| `rear_activation_quality` | `NetValueQuality`; 8비트 부호 없음 | 별도 DOMAIN 활성 평가 품질; 단위: 의미 코드 · 무단위 | DOMAIN 활성 담당자; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_reason` | `NetQualityReason`; 8비트 부호 없음 | 활성 평가 품질 사유; 단위: 의미 코드 · 무단위 | DOMAIN 활성 담당자; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_sequence` | `uint32_t`; 32비트 부호 없음 | 새 실제 활성 평가 번호; 단위: 평가 번호 · 무단위 | DOMAIN 활성 담당자; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |
| `rear_activation_age_ms` | `uint16_t`; 16비트 부호 없음 | 활성 원본 평가 이후 누적 ms, uint16 포화; 단위: ms | DOMAIN 활성 담당자; generation은 수용한 DOMAIN Boot. 후방 거리 evidence·TX_SEQUENCE와 별개 |

<a id="type-DomainIf_VssWarningState_t"></a>
### `DomainIf_VssWarningState_t`

```c
typedef struct
{
    AntiPinchState_t windowAntiPinchState; /* [구현 보류 — 설계 유지] */
    DomainIf_WarningInputEvidence_t antiPinchEvidence;
    OccupantHazardState_t occupantHazardState;
    DomainIf_WarningInputEvidence_t occupantEvidence;
    RearObstacleState_t rearObstacleState;
    DomainIf_WarningInputEvidence_t rearEvidence;
    RearDetectionActivation_t rearDetectionActivation;
    SignalMeta_t rearActivationMeta; /* independent Domain policy evaluation */
    QualityReason_t rearActivationReason;
    /* No format marker or separate activation source boot here. */
} DomainIf_VssWarningState_t;
```

근거: S4A data/T14.md#type-DomainIf_VssWarningState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `windowAntiPinchState` | `AntiPinchState_t`; C enum 저장 폭 미검증 | Domain에 인계된 WINDOW 끼임 의미. 현재 구현 보류. | Domain 기능 결정/설정/결과 담당자 |
| `antiPinchEvidence` | `DomainIf_WarningInputEvidence_t`; 중첩 타입 참조; sizeof 미검증 | 끼임 원 품질/가용성/세대/평가/AGE. WINDOW 구현 보류. | Domain 기능 결정/설정/결과 담당자 |
| `occupantHazardState` | `OccupantHazardState_t`; C enum 저장 폭 미검증 | Domain 잔류 탑승자 위험 CLEAR/ACTIVE/UNCONFIRMED. VSS 에서 재판정 금지. | Domain 기능 결정/설정/결과 담당자 |
| `occupantEvidence` | `DomainIf_WarningInputEvidence_t`; 중첩 타입 참조; sizeof 미검증 | Domain 잔류 탑승자 위험의 원 근거. Pi→CIS 원 lineage·품질 보존. | Domain 기능 결정/설정/결과 담당자 |
| `rearObstacleState` | `RearObstacleState_t`; C enum 저장 폭 미검증 | Domain 후방 위험 상태. VSS가 거리를 다시 평가해 만들지 않는다. | Domain 기능 결정/설정/결과 담당자 |
| `rearEvidence` | `DomainIf_WarningInputEvidence_t`; 중첩 타입 참조; sizeof 미검증 | 후방 위험 판단 근거와 원본 관측 계보. | Domain 기능 결정/설정/결과 담당자 |
| `rearDetectionActivation` | `RearDetectionActivation_t`; C enum 저장 폭 미검증 | Domain 후방 감지 활성 정책. rear 위험 상태와 독립. | Domain 기능 결정/설정/결과 담당자 |
| `rearActivationMeta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 후방 감지 활성의 독립 평가 품질/원본 생성 후 경과 시간/순서. rear 센서 관측 메타와 별도. | Domain 기능 결정/설정/결과 담당자 |
| `rearActivationReason` | `QualityReason_t`; C enum 저장 폭 미검증 | 후방 감지 활성 평가 사유. 입력 고장 및 활성 상태를 분리. | Domain 기능 결정/설정/결과 담당자 |

<a id="wire-VSS_WARNING_STATE"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/VSS_WARNING_STATE.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 541 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 541 |
| 8 / 4 | `ANTIPINCH_SOURCE_BOOT` → `antipinch_source_boot` (`uint32_t`) | u32 / unsigned / LE | 원 판단 근거 생성 노드 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 542 |
| 12 / 4 | `ANTIPINCH_SOURCE_SEQUENCE` → `antipinch_source_sequence` (`uint32_t`) | u32 / unsigned / LE | 새 유효 판단 근거 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 543 |
| 16 / 2 | `ANTIPINCH_ORIGINAL_AGE` → `antipinch_original_age_ms` (`uint16_t`) | u16 / unsigned / LE | 원 판단 근거 관측 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 544 |
| 18 / 1 | `ANTIPINCH_STATE` → `antipinch_state` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 545 |
| 19 / 1 | `ANTIPINCH_QUALITY` → `antipinch_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 546 |
| 20 / 1 | `ANTIPINCH_REASON` → `antipinch_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 547 |
| 21 / 1 | `ANTIPINCH_AVAILABILITY` → `antipinch_availability` (`NetAvailability`) | u8 / unsigned / N/A | 0=NET_AVAILABILITY_AVAILABLE; 1=NET_AVAILABILITY_LIMITED; 2=NET_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 548 |
| 22 / 4 | `OCCUPANT_HAZARD_SOURCE_BOOT` → `occupant_hazard_source_boot` (`uint32_t`) | u32 / unsigned / LE | 원 판단 근거 생성 노드 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 549 |
| 26 / 4 | `OCCUPANT_HAZARD_SOURCE_SEQUENCE` → `occupant_hazard_source_sequence` (`uint32_t`) | u32 / unsigned / LE | 새 유효 판단 근거 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 550 |
| 30 / 2 | `OCCUPANT_HAZARD_ORIGINAL_AGE` → `occupant_hazard_original_age_ms` (`uint16_t`) | u16 / unsigned / LE | 원 판단 근거 관측 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 551 |
| 32 / 1 | `OCCUPANT_HAZARD_STATE` → `occupant_hazard_state` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 552 |
| 33 / 1 | `OCCUPANT_HAZARD_QUALITY` → `occupant_hazard_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 553 |
| 34 / 1 | `OCCUPANT_HAZARD_REASON` → `occupant_hazard_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 554 |
| 35 / 1 | `OCCUPANT_HAZARD_AVAILABILITY` → `occupant_hazard_availability` (`NetAvailability`) | u8 / unsigned / N/A | 0=NET_AVAILABILITY_AVAILABLE; 1=NET_AVAILABILITY_LIMITED; 2=NET_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 555 |
| 36 / 4 | `REAR_OBSTACLE_SOURCE_BOOT` → `rear_obstacle_source_boot` (`uint32_t`) | u32 / unsigned / LE | 원 판단 근거 생성 노드 기동 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 556 |
| 40 / 4 | `REAR_OBSTACLE_SOURCE_SEQUENCE` → `rear_obstacle_source_sequence` (`uint32_t`) | u32 / unsigned / LE | 새 유효 판단 근거 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 557 |
| 44 / 2 | `REAR_OBSTACLE_ORIGINAL_AGE` → `rear_obstacle_original_age_ms` (`uint16_t`) | u16 / unsigned / LE | 원 판단 근거 관측 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 558 |
| 46 / 1 | `REAR_OBSTACLE_STATE` → `rear_obstacle_state` (`NetRearWarningState`) | u8 / unsigned / N/A | 0=NET_REAR_WARNING_CLEAR; 1=NET_REAR_WARNING_CAUTION; 2=NET_REAR_WARNING_EMERGENCY; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 559 |
| 47 / 1 | `REAR_OBSTACLE_QUALITY` → `rear_obstacle_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 560 |
| 48 / 1 | `REAR_OBSTACLE_REASON` → `rear_obstacle_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 561 |
| 49 / 1 | `REAR_OBSTACLE_AVAILABILITY` → `rear_obstacle_availability` (`NetAvailability`) | u8 / unsigned / N/A | 0=NET_AVAILABILITY_AVAILABLE; 1=NET_AVAILABILITY_LIMITED; 2=NET_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 562 |
| 50 / 1 | `REAR_DETECTION_ACTIVATION` → `rear_detection_activation` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 563 |
| 51 / 1 | `ACTIVATION_META_FORMAT` → `activation_meta_format` (`uint8_t`) | u8 / unsigned / N/A | 1=이번 activation metadata 형식; 0=legacy padding/근거 없음; 그 외=미지원; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 564 |
| 52 / 1 | `REAR_ACTIVATION_QUALITY` → `rear_activation_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 1=NET_QUALITY_STALE; 2=NET_QUALITY_INVALID; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 565 |
| 53 / 1 | `REAR_ACTIVATION_REASON` → `rear_activation_reason` (`NetQualityReason`) | u8 / unsigned / N/A | 0=NET_QUALITY_REASON_NONE; 1=NET_QUALITY_REASON_NOT_READY; 2=NET_QUALITY_REASON_OUT_OF_RANGE; 3=NET_QUALITY_REASON_SENSOR_FAULT; 4=NET_QUALITY_REASON_VISION_FAULT; 5=NET_QUALITY_REASON_STALE; 6=NET_QUALITY_REASON_NO_DATA; 7=NET_QUALITY_REASON_COMMUNICATION_FAULT; 8=NET_QUALITY_REASON_RECOVERING; 9=NET_QUALITY_REASON_DATA_INVALID; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 566 |
| 54 / 4 | `REAR_ACTIVATION_SEQUENCE` → `rear_activation_sequence` (`uint32_t`) | u32 / unsigned / LE | Domain의 새 기능 활성/허용 평가 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 567 |
| 58 / 2 | `REAR_ACTIVATION_AGE` → `rear_activation_age_ms` (`uint16_t`) | u16 / unsigned / LE | 동일 Domain 활성 평가 후 누적 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 568 |

세 품질0..3·사유0..9·가용성0..2, 끼임/탑승자/활성 flag0/1, 후방 상태0..2를 검증한다. 확장후보 B51 format1과 format0의 근거 부재를 구분한다.

### C2 전체 필드 투영

| C2 선언 멤버 / 타입 | 확인한 raw·Net 대응 또는 표현 제한 |
|---|---|
| `DomainIf_VssWarningState_t.windowAntiPinchState` / `AntiPinchState_t` | ANTIPINCH_STATE; WINDOW 실행 보류 |
| `DomainIf_VssWarningState_t.antiPinchEvidence` / `DomainIf_WarningInputEvidence_t` | ANTIPINCH_SOURCE_BOOT/SEQUENCE/ORIGINAL_AGE/QUALITY/REASON/AVAILABILITY; 독립 근거 |
| `DomainIf_VssWarningState_t.occupantHazardState` / `OccupantHazardState_t` | OCCUPANT_HAZARD_STATE; UNCONFIRMED는 VSS 전용 제한 투영으로 원 근거와 함께 보존하며 정상 CLEAR로 소비하지 않음 |
| `DomainIf_VssWarningState_t.occupantEvidence` / `DomainIf_WarningInputEvidence_t` | OCCUPANT_HAZARD_*의 독립 원 판단 근거 |
| `DomainIf_VssWarningState_t.rearObstacleState` / `RearObstacleState_t` | REAR_OBSTACLE_STATE; 이름 대응 |
| `DomainIf_VssWarningState_t.rearEvidence` / `DomainIf_WarningInputEvidence_t` | REAR_OBSTACLE_*의 독립 원 판단 근거 |
| `DomainIf_VssWarningState_t.rearDetectionActivation` / `RearDetectionActivation_t` | REAR_DETECTION_ACTIVATION; 이름 대응 |
| `DomainIf_VssWarningState_t.rearActivationMeta` / `SignalMeta_t` | 선택된 WARNING60의 B52 quality/B54..57 sequence/B58..59 age; 실제 양단 지원 검증 대기 |
| `DomainIf_VssWarningState_t.rearActivationReason` / `QualityReason_t` | 선택된 WARNING60의 B53 REAR_ACTIVATION_REASON; 실제 DOMAIN 평가 Binding 대기 |

세 DomainIf_WarningInputEvidence_t의 source.generation/sequence/ageMs, quality/reason/availability를 각 독립 Wire 묶음으로 연결한다. VSS 전용 제한 투영은 [정본](../../interface/04_COMMON/30_ENUM_WIRE_MAPPING.md)을 따른다. UNCONFIRMED의 비정상 품질은 state0 placeholder와 원 품질/근거를 유지하고 정상 CLEAR로 소비하지 않는다. UNCONFIRMED+OK 또는 지원 UNSUPPORTED는 INVALID2/DATA_INVALID9/UNAVAILABLE2/state0으로 제한하며 원 진단을 남긴다. WINDOW 미지원 정규 부재는 state0/quality3/reason6/availability2/sourceBoot0/seq0/AGE65535이며 다른 노드의 일반 무손실 대응을 바꾸지 않는다. rearDetectionActivation과 rearActivationMeta/reason은 독립 DOMAIN 평가다.

**적용 제약:** A1 WARNING60(used60/frame64/DLC15)을 선택했다. 실제 DOMAIN producer/VSS receiver·clock/owner 지원은 CF02/E01~05 확인 대기이고 WINDOW 실행 HOLD는 유지한다. profile enum 선택만으로 실행 채택을 주장하지 않는다.

[함수와 양단 API](../functions/can/VSS_WARNING_STATE.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#VSS_WARNING_STATE)

### 선택한 WARNING60과 이전 WARNING51의 구분

| 지위 | used/frame/DLC | B51..59 | padding |
|---|---|---|---|
| WARNING51 / T02 이력 | 51/64/15 | 의미 없는 바이트 | B51..63=0 |
| WARNING60 / A1 선택 | 60/64/15 | B51 format, B52 품질, B53 사유, B54..57 활성 평가번호, B58..59 AGE | B60..63=0 |

현재 `NetVssWarningState`와 Wire offset은 그대로다. WARNING51은 정상 수용 후보에서 제외하며 DLC/길이/marker로 자동 인식·fallback하지 않는다. WARNING60 format1은 독립 activation meta 검증이 필요하고 format0은 B50=0·B52..59=0인 정규 근거 부재다. 부재를 활성 OK나 위험 CLEAR로 합성하지 않으며 나머지 유효 위험 항목은 독립 보존한다. 실제 양단 지원 확인 전 encode/decode/typed 실행은 HOLD다.

<a id="VSS_STATUS"></a>
## VSS_STATUS: 음향 서비스와 고장 이력

VSS→DOMAIN, 후보 ID `0x340`, used `28`B / frame `32`B / DLC `13`. Padding: 28..31, 존재하는 padding은0이다.

원 필드 생성은 해당 기능 Owner, 바이트 변환은 Network M05, typed 투영은 M03이다. VSS 생산 세대+update_sequence는 서비스 평가, last_fault_occurrence는 최근 고장 이력이다. source_age_ms와 last_fault_age_ms는 독립이다. 최근 고장 AGE65535는 오래된 이력의 포화 하한이다.

<a id="type-NetVssStatus"></a>
### `NetVssStatus`

```c
typedef struct NetVssStatus
{
    NetVssState vss_state;
    NetVssAvailability vss_availability;
    NetFlag vss_accepting_events;
    NetFlag vss_fault_active;
    uint8_t fault_mask;
    uint16_t vss_last_fault;
    uint32_t last_fault_occurrence;
    uint16_t last_fault_age_ms;
    NetValueQuality last_fault_quality;
    uint32_t update_sequence;
    uint16_t source_age_ms;
} NetVssStatus;
```

근거: S4A data/T28.md#type-NetVssStatus. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `vss_state` | `NetVssState`; 8비트 부호 없음 | 현재 음향 서비스; STARTUP/READY/PLAYING/FAULT; 단위: 의미 코드 · 무단위 | VSS 서비스 담당자; READY/PLAYING을 개별 이벤트 재생 DONE으로 해석 금지 |
| `vss_availability` | `NetVssAvailability`; 8비트 부호 없음 | 현재 서비스 가용성; FULL/DEGRADED/UNAVAILABLE; 단위: 의미 코드 · 무단위 | VSS 서비스 담당자; 입력 Event availability와 별개 |
| `vss_accepting_events` | `NetFlag`; 8비트 부호 없음 | 현재 의미 Event/State 수용 가능, 0/1; 단위: 0/1 의미값 · 무단위 | VSS 서비스 담당자; 실제 음향 시작·완료 보장 아님 |
| `vss_fault_active` | `NetFlag`; 8비트 부호 없음 | 현재 내부 출력 고장 활성, 0/1; 단위: 0/1 의미값 · 무단위 | VSS 진단 담당자; 최근 고장 이력이 있다는 이유로 1 생성 금지 |
| `fault_mask` | `uint8_t`; 8비트 부호 없음 | 현재 고장 bit0~4; 상위 bit 예약; 단위: bit 집합 · 무단위 | VSS 진단 담당자; 현재 발생 중인 고장만. 최근 이력 mask 아님 |
| `vss_last_fault` | `uint16_t`; 16비트 부호 없음 | 기동 이후 최근 주요 고장 code0~5; 단위: 의미 코드 · 무단위 | VSS 진단 담당자; current mask 복구 후에도 이력의 의미 유지 |
| `last_fault_occurrence` | `uint32_t`; 32비트 부호 없음 | 최근 고장 원 발생 번호; 단위: 발생 식별자 · 무단위 | VSS 진단 담당자; 같은 고장 반복 보고 때 유지 |
| `last_fault_age_ms` | `uint16_t`; 16비트 부호 없음 | 최근 고장 발생부터 누적 ms, uint16 포화; 단위: ms | VSS 원 기록 + 송신 대기; 서비스 평가 Age와 별개 |
| `last_fault_quality` | `NetValueQuality`; 8비트 부호 없음 | 이력 있음=OK(0), 기록 없음=NO_DATA(3); 단위: 의미 코드 · 무단위 | VSS 진단 담당자; 최근 이력의 존재 품질. C2 faultQuality 전체 의미 투영은 OPEN |
| `update_sequence` | `uint32_t`; 32비트 부호 없음 | 새 서비스·현재 고장 평가 번호; 단위: 평가 번호 · 무단위 | VSS 실제 평가에서만 갱신; CAN TX_SEQUENCE와 별개 |
| `source_age_ms` | `uint16_t`; 16비트 부호 없음 | 현재 서비스 평가 이후 누적 ms, uint16 포화; 단위: ms | VSS 원본 평가 + 송신 대기; last_fault_age_ms를 대체하지 않음 |

<a id="wire-VSS_STATUS"></a>
### Wire와 DTO의 전체 필드 대응

근거: B07 can/VSS_STATUS.md#wire, T33 원문 행은 마지막 열. BOOT/TX는 DTO에 새 멤버를 추가하지 않고 `NetCanSource`/`NetCanRxMeta.source`로 분리한다.

| Offset / 폭(B) | Wire 필드 → typed 멤버(타입) | Raw / 부호 / 순서 | 코드·범위·단위 | sentinel / unknown | T33 행 |
|---|---|---|---|---|---|
| 0 / 4 | `BOOT_ID` → `meta.source.boot_id` (`uint32_t`) | u32 / unsigned / LE | 0 예약; 단위=NOT_SPECIFIED | 0 예약 | 586 |
| 4 / 4 | `TX_SEQUENCE` → `meta.source.tx_sequence` (`uint32_t`) | u32 / unsigned / LE | 최초1, 프레임마다 증가; wrap 정책 OPEN; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 586 |
| 8 / 1 | `VSS_STATE` → `vss_state` (`NetVssState`) | u8 / unsigned / N/A | 0=NET_VSS_STATE_STARTUP; 1=NET_VSS_STATE_READY; 2=NET_VSS_STATE_PLAYING; 3=NET_VSS_STATE_FAULT; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 587 |
| 9 / 1 | `VSS_AVAILABILITY` → `vss_availability` (`NetVssAvailability`) | u8 / unsigned / N/A | 0=NET_VSS_AVAILABILITY_FULL; 1=NET_VSS_AVAILABILITY_DEGRADED; 2=NET_VSS_AVAILABILITY_UNAVAILABLE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 588 |
| 10 / 1 | `VSS_ACCEPTING_EVENTS` → `vss_accepting_events` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 589 |
| 11 / 1 | `VSS_FAULT_ACTIVE` → `vss_fault_active` (`NetFlag`) | u8 / unsigned / N/A | 0=NET_FLAG_NO; 1=NET_FLAG_YES; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 590 |
| 12 / 1 | `FAULT_MASK` → `fault_mask` (`uint8_t`) | u8 / unsigned / N/A | VSS 고장 비트 0~4; 1=NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE; 2=NET_VSS_FAULT_PLAYBACK_START_FAILURE; 4=NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE; 8=NET_VSS_FAULT_PLAYBACK_STATE_FAILURE; 16=NET_VSS_FAULT_INITIALIZATION_FAILURE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 591 |
| 13 / 2 | `VSS_LAST_FAULT` → `vss_last_fault` (`uint16_t`) | u16 / unsigned / LE | 0=NET_VSS_FAULT_CODE_NONE; 1=NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE; 2=NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE; 3=NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE; 4=NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE; 5=NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 592 |
| 15 / 4 | `LAST_FAULT_OCCURRENCE` → `last_fault_occurrence` (`uint32_t`) | u32 / unsigned / LE | 최근 고장 발생 번호; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 593 |
| 19 / 2 | `LAST_FAULT_AGE` → `last_fault_age_ms` (`uint16_t`) | u16 / unsigned / LE | 최근 고장 발생 후 ms; 포화 허용; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 594 |
| 21 / 1 | `LAST_FAULT_QUALITY` → `last_fault_quality` (`NetValueQuality`) | u8 / unsigned / N/A | 0=NET_QUALITY_OK; 3=NET_QUALITY_NO_DATA; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 595 |
| 22 / 4 | `UPDATE_SEQUENCE` → `update_sequence` (`uint32_t`) | u32 / unsigned / LE | 이번 상태 평가; 단위=NOT_SPECIFIED | NOT_SPECIFIED | 596 |
| 26 / 2 | `SOURCE_AGE` → `source_age_ms` (`uint16_t`) | u16 / unsigned / LE | 평가 후 ms; 단위=ms | 65535 포화 하한; 정확한 경과 상한 또는 fresh의 증거 아님 | 597 |

vss_state0..3, availability0..2, 두flag0/1, fault_mask bit0..4, last_fault0..5, last_fault_quality는0 또는3만 허용한다. 일반 ValueQuality1/2는 이 필드에서 거부한다. 확인된 부재3은 code/occurrence/historyAGE 모두0이며 actual 이력0은 code1..5와 원 occurrence1..UINT32_MAX를 가진다. 부재 이력 AGE는 누적하지 않고 source AGE는 독립 누적한다.

기존 VSS INPUT 한 곳이 HEALTH의 완전 current/history·원 평가/발생 시각을 11필드로 변환하는 A2 생산 경계다. DOMAIN Network는 같은 원 DTO/meta를 보존하고 service.state/availability/acceptingEvents/faultActive와 원 update/소비 AGE를 명시 투영한다. LAST_FAULT_QUALITY0/3은 기록 존재이며 service/faultQuality 전체와 동등하지 않다. 완전 current mask의 가장 작은 정의 code1..5를 현재 Fault 대표로 선택하고 공집합은 code0/INACTIVE로 표현한다. 이력 code나 링크 상실로 현재 고장을 만들지 않는다.

**적용 제약:** A2 producer·State/Fault 독립 수용은 내부 선택이다. 같은 K 반복은 두 Rx 호출0이며 State 수용 뒤 Fault 실패로 State를 재실행하지 않는다. CALLING/UNKNOWN_FAILED와 old K 후속 쓰기 미확인은 해당 경계 HOLD다. 설치 profile·원 보고 완전성·세대/시간·Getter association·후발 sink는 E-S01~05 확인 대기([A2 정본](../functions/can/VSS_STATUS.md#a2-domain-contract)).

[함수와 양단 API](../functions/can/VSS_STATUS.md) · [흐름](../flows/CAN_CIS_WINDOW_VSS.md#VSS_STATUS)

## 중첩 자료와 노드 전용 코드

다음 선언은 위 DTO가 참조하는 실제 S4A 타입이다. raw와 C2 enum의 이름이 같더라도 숫자 cast를 하지 않는다. 각 Wire 표에 없는 코드는 그 Wire에서 허용된 값이 아니다.

<a id="type-AntiPinchState_t"></a>
### `AntiPinchState_t`

```c
typedef enum
{
    ANTIPINCH_STATE_CLEAR = 0,
    ANTIPINCH_STATE_ACTIVE
} AntiPinchState_t;
```

근거: S4A data/T13.md#type-AntiPinchState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-CisFunctionStatus_t"></a>
### `CisFunctionStatus_t`

```c
typedef enum
{
    CIS_FUNCTION_STATUS_NOT_READY = 0,
    CIS_FUNCTION_STATUS_READY,
    CIS_FUNCTION_STATUS_ACTIVE,
    CIS_FUNCTION_STATUS_FAULT,
    CIS_FUNCTION_STATUS_RECOVERING
} CisFunctionStatus_t;
```

근거: S4A data/T13.md#type-CisFunctionStatus_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-CisState_t"></a>
### `CisState_t`

```c
typedef enum
{
    CIS_STATE_STARTUP = 0,
    CIS_STATE_READY,
    CIS_STATE_ACTIVE,
    CIS_STATE_FAULT
} CisState_t;
```

근거: S4A data/T13.md#type-CisState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-DomainIf_AntiPinchProtectionState_t"></a>
### `DomainIf_AntiPinchProtectionState_t`

```c
typedef enum
{
    ANTIPINCH_PROTECTION_IDLE = 0,
    ANTIPINCH_PROTECTION_REVERSING,
    ANTIPINCH_PROTECTION_COMPLETED,
    ANTIPINCH_PROTECTION_ABORTED
} DomainIf_AntiPinchProtectionState_t;
```

근거: S4A data/T14.md#type-DomainIf_AntiPinchProtectionState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-DomainIf_AntiPinchReport_t"></a>
### `DomainIf_AntiPinchReport_t`

```c
typedef struct
{
    OccurrenceId_t occurrenceId;
    DomainIf_AntiPinchProtectionState_t protectionState;
    bool outputDisabled;
    bool movementStopped;
    ResultReason_t reason;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration; /* [구현 보류 — 설계 유지] */
} DomainIf_AntiPinchReport_t;
```

근거: S4A data/T14.md#type-DomainIf_AntiPinchReport_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `occurrenceId` | `OccurrenceId_t`; 32비트 부호 없음 | 원 사건/결과/경고 발생 식별 u32. 생산 ECU의 generation과 함께 중복 판단; 링크 전송순번과 다르다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `protectionState` | `DomainIf_AntiPinchProtectionState_t`; C enum 저장 폭 미검증 | WINDOW 로컬 anti-pinch IDLE/REVERSING/COMPLETED/ABORTED. END Event와 동의어 아님. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `outputDisabled` | `bool`; 저장 폭은 ABI 미검증 | WINDOW 로컬 출력 차단 근거 bool; 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `movementStopped` | `bool`; 저장 폭은 ABI 미검증 | WINDOW 실제 이동 정지 근거 bool; 구현 보류. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `reason` | `ResultReason_t`; C enum 저장 폭 미검증 | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |
| `sourceGeneration` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | WINDOW 로컬 기능 담당자 [구현 보류 — 설계 유지] |

<a id="type-DomainIf_CisInterfaceStatus_t"></a>
### `DomainIf_CisInterfaceStatus_t`

```c
typedef enum
{
    CIS_INTERFACE_AVAILABLE = 0, /* semantic READY; explicitly mapped */
    CIS_INTERFACE_UNAVAILABLE = 2,
    CIS_INTERFACE_RECOVERING = 3 /* no public DEGRADED; never raw-cast */
} DomainIf_CisInterfaceStatus_t;
```

근거: S4A data/T14.md#type-DomainIf_CisInterfaceStatus_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-DomainIf_VssStateReport_t"></a>
### `DomainIf_VssStateReport_t`

```c
typedef struct
{
    VssServiceState_t service;
    uint8_t faultMask;
    uint16_t lastFaultCode;
    OccurrenceId_t lastFaultOccurrenceId;
    uint32_t lastFaultAgeMs;
    DataQuality_t faultQuality;
    SourceGeneration_t sourceGeneration; /* derived VSS fault-warning lineage */
} DomainIf_VssStateReport_t;
```

근거: S4A data/T14.md#type-DomainIf_VssStateReport_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `service` | `VssServiceState_t`; 중첩 타입 참조; sizeof 미검증 | VSS 실제 음향 서비스 상태. 완전 Snapshot→Net DTO는 기존 INPUT 한 곳(A2 선택); DOMAIN 설치 profile별 C2/Getter association은 E-S05 확인 대기. | VSS 음향 서비스 담당자 |
| `faultMask` | `uint8_t`; 8비트 부호 없음 | 활성/종합 fault bitmask. 노드별 bit 계약 및 활성 code와 구분. | VSS 음향 서비스 담당자 |
| `lastFaultCode` | `uint16_t`; 16비트 부호 없음 | 원 ECU 최근 고장 code u16. 활성 고장 여부가 아니다. | VSS 음향 서비스 담당자 |
| `lastFaultOccurrenceId` | `OccurrenceId_t`; 32비트 부호 없음 | 최근 고장 발생 식별, 코드·원 세대·AGE와 함께 소비. | VSS 음향 서비스 담당자 |
| `lastFaultAgeMs` | `uint32_t`; 32비트 부호 없음 | 원 최근 고장 발생 후 u32 ms. Link 마지막 수신 AGE와 다르다. | VSS 음향 서비스 담당자 |
| `faultQuality` | `DataQuality_t`; C enum 저장 폭 미검증 | VSS 고장 정보 품질. 정상 오디오 상태와 별개. | VSS 음향 서비스 담당자 |
| `sourceGeneration` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | VSS 음향 서비스 담당자 |

<a id="type-DomainIf_WarningInputEvidence_t"></a>
### `DomainIf_WarningInputEvidence_t`

```c
typedef struct
{
    DataQuality_t quality;
    QualityReason_t reason;
    FunctionAvailability_t availability;
    SourceEvidence_t source;
} DomainIf_WarningInputEvidence_t;
```

근거: S4A data/T14.md#type-DomainIf_WarningInputEvidence_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `quality` | `DataQuality_t`; C enum 저장 폭 미검증 | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `reason` | `QualityReason_t`; C enum 저장 폭 미검증 | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `availability` | `FunctionAvailability_t`; C enum 저장 폭 미검증 | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `source` | `SourceEvidence_t`; 중첩 타입 참조; sizeof 미검증 | 원 경고 판단 근거 generation/sequence/AGE 또는 타입별 원본 출처. 다음 링크 전송순번으로 대체 금지. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-NetAntiPinchProtectionState"></a>
### `NetAntiPinchProtectionState`

```c
typedef uint8_t NetAntiPinchProtectionState;
```

근거: S4A data/T31.md#type-NetAntiPinchProtectionState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetAntiPinchProtectionStateCode"></a>
### `NetAntiPinchProtectionStateCode`

```c
enum NetAntiPinchProtectionStateCode
{
    NET_ANTIPINCH_PROTECTION_IDLE = 0,
    NET_ANTIPINCH_PROTECTION_IN_PROGRESS = 1,
    NET_ANTIPINCH_PROTECTION_DONE = 2,
    NET_ANTIPINCH_PROTECTION_INTERRUPTED = 3
};
```

근거: S4A data/T31.md#type-NetAntiPinchProtectionStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetCisFaultCode"></a>
### `NetCisFaultCode`

```c
enum NetCisFaultCode
{
    NET_CIS_FAULT_CODE_NONE = 0,
    NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE = 1,
    NET_CIS_FAULT_CODE_VISION_FAULT = 2,
    NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT = 3,
    NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT = 4,
    NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT = 5,
    NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT = 6,
    NET_CIS_FAULT_CODE_COMMUNICATION_FAULT = 7,
    NET_CIS_FAULT_CODE_DATA_INVALID = 8,
    NET_CIS_FAULT_CODE_OUT_OF_RANGE = 9
};
```

근거: S4A data/T09.md#type-NetCisFaultCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetCisFaultMask"></a>
### `NetCisFaultMask`

```c
enum NetCisFaultMask
{
    NET_CIS_FAULT_INITIALIZATION_FAILURE = (1u << 0),
    NET_CIS_FAULT_VISION_FAULT = (1u << 1),
    NET_CIS_FAULT_TEMPERATURE_SENSOR_FAULT = (1u << 2),
    NET_CIS_FAULT_HUMIDITY_SENSOR_FAULT = (1u << 3),
    NET_CIS_FAULT_ILLUMINANCE_SENSOR_FAULT = (1u << 4),
    NET_CIS_FAULT_PROXIMITY_SENSOR_FAULT = (1u << 5),
    NET_CIS_FAULT_COMMUNICATION_FAULT = (1u << 6),
    NET_CIS_FAULT_DATA_INVALID = (1u << 7),
    NET_CIS_FAULT_OUT_OF_RANGE = (1u << 8)
};
```

근거: S4A data/T09.md#type-NetCisFaultMask. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetCisState"></a>
### `NetCisState`

```c
typedef uint8_t NetCisState;
```

근거: S4A data/T09.md#type-NetCisState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetCisStateCode"></a>
### `NetCisStateCode`

```c
enum NetCisStateCode
{
    NET_CIS_STATE_STARTUP = 0,
    NET_CIS_STATE_READY = 1,
    NET_CIS_STATE_ACTIVE = 2,
    NET_CIS_STATE_FAULT = 3
};
```

근거: S4A data/T09.md#type-NetCisStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetInterfaceState"></a>
### `NetInterfaceState`

```c
typedef uint8_t NetInterfaceState;
```

근거: S4A data/T09.md#type-NetInterfaceState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetInterfaceStateCode"></a>
### `NetInterfaceStateCode`

```c
enum NetInterfaceStateCode
{
    NET_INTERFACE_READY = 0,
    NET_INTERFACE_UNAVAILABLE = 1,
    NET_INTERFACE_RECOVERING = 2
};
```

근거: S4A data/T09.md#type-NetInterfaceStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetRearWarningState"></a>
### `NetRearWarningState`

```c
typedef uint8_t NetRearWarningState;
```

근거: S4A data/T28.md#type-NetRearWarningState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetRearWarningStateCode"></a>
### `NetRearWarningStateCode`

```c
enum NetRearWarningStateCode
{
    NET_REAR_WARNING_CLEAR = 0,
    NET_REAR_WARNING_CAUTION = 1,
    NET_REAR_WARNING_EMERGENCY = 2
};
```

근거: S4A data/T28.md#type-NetRearWarningStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetSourceContext"></a>
### `NetSourceContext`

```c
typedef uint8_t NetSourceContext;
```

근거: S4A data/T31.md#type-NetSourceContext. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetSourceContextCode"></a>
### `NetSourceContextCode`

```c
enum NetSourceContextCode
{
    NET_SOURCE_CONTEXT_NONE = 0,
    NET_SOURCE_CONTEXT_DOMAIN_POLICY = 1,
    NET_SOURCE_CONTEXT_AUTO_FUNCTION = 2,
    NET_SOURCE_CONTEXT_LOCAL = 3,
    NET_SOURCE_CONTEXT_TEST = 4
};
```

근거: S4A data/T31.md#type-NetSourceContextCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssAvailability"></a>
### `NetVssAvailability`

```c
typedef uint8_t NetVssAvailability;
```

근거: S4A data/T28.md#type-NetVssAvailability. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssAvailabilityCode"></a>
### `NetVssAvailabilityCode`

```c
enum NetVssAvailabilityCode
{
    NET_VSS_AVAILABILITY_FULL = 0,
    NET_VSS_AVAILABILITY_DEGRADED = 1,
    NET_VSS_AVAILABILITY_UNAVAILABLE = 2
};
```

근거: S4A data/T28.md#type-NetVssAvailabilityCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssEventKind"></a>
### `NetVssEventKind`

```c
typedef uint8_t NetVssEventKind;
```

근거: S4A data/T28.md#type-NetVssEventKind. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssEventKindCode"></a>
### `NetVssEventKindCode`

```c
enum NetVssEventKindCode
{
    NET_VSS_EVENT_WELCOME = 1,
    NET_VSS_EVENT_GOODBYE = 2,
    NET_VSS_EVENT_LOCK_COMPLETE = 3,
    NET_VSS_EVENT_UNLOCK_COMPLETE = 4,
    NET_VSS_EVENT_LOCK_ERROR = 5
};
```

근거: S4A data/T28.md#type-NetVssEventKindCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssFaultCode"></a>
### `NetVssFaultCode`

```c
enum NetVssFaultCode
{
    NET_VSS_FAULT_CODE_NONE = 0,
    NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE = 1,
    NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE = 2,
    NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE = 3,
    NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE = 4,
    NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE = 5
};
```

근거: S4A data/T28.md#type-NetVssFaultCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssFaultMask"></a>
### `NetVssFaultMask`

```c
enum NetVssFaultMask
{
    NET_VSS_FAULT_SOUND_ASSET_UNAVAILABLE = (1u << 0),
    NET_VSS_FAULT_PLAYBACK_START_FAILURE = (1u << 1),
    NET_VSS_FAULT_AUDIO_OUTPUT_FAILURE = (1u << 2),
    NET_VSS_FAULT_PLAYBACK_STATE_FAILURE = (1u << 3),
    NET_VSS_FAULT_INITIALIZATION_FAILURE = (1u << 4)
};
```

근거: S4A data/T28.md#type-NetVssFaultMask. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssState"></a>
### `NetVssState`

```c
typedef uint8_t NetVssState;
```

근거: S4A data/T28.md#type-NetVssState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetVssStateCode"></a>
### `NetVssStateCode`

```c
enum NetVssStateCode
{
    NET_VSS_STATE_STARTUP = 0,
    NET_VSS_STATE_READY = 1,
    NET_VSS_STATE_PLAYING = 2,
    NET_VSS_STATE_FAULT = 3
};
```

근거: S4A data/T28.md#type-NetVssStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowCommandKind"></a>
### `NetWindowCommandKind`

```c
typedef uint8_t NetWindowCommandKind;
```

근거: S4A data/T31.md#type-NetWindowCommandKind. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowCommandKindCode"></a>
### `NetWindowCommandKindCode`

```c
enum NetWindowCommandKindCode
{
    NET_WINDOW_COMMAND_OPEN = 1,
    NET_WINDOW_COMMAND_CLOSE = 2,
    NET_WINDOW_COMMAND_STOP = 3,
    NET_WINDOW_COMMAND_VENT = 4,
    NET_WINDOW_COMMAND_MOVE_TO_POSITION = 5
};
```

근거: S4A data/T31.md#type-NetWindowCommandKindCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowEventKind"></a>
### `NetWindowEventKind`

```c
typedef uint8_t NetWindowEventKind;
```

근거: S4A data/T31.md#type-NetWindowEventKind. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowEventKindCode"></a>
### `NetWindowEventKindCode`

```c
enum NetWindowEventKindCode
{
    NET_WINDOW_EVENT_FULLY_OPENED = 1,
    NET_WINDOW_EVENT_FULLY_CLOSED = 2,
    NET_WINDOW_EVENT_ANTIPINCH = 3
};
```

근거: S4A data/T31.md#type-NetWindowEventKindCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowFaultCode"></a>
### `NetWindowFaultCode`

```c
typedef uint16_t NetWindowFaultCode;
```

근거: S4A data/T31.md#type-NetWindowFaultCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowFaultCodeCode"></a>
### `NetWindowFaultCodeCode`

```c
enum NetWindowFaultCodeCode
{
    NET_WINDOW_FAULT_NONE = 0,
    NET_WINDOW_FAULT_POSITION = 1,
    NET_WINDOW_FAULT_DRIVE = 2,
    NET_WINDOW_FAULT_ENDPOINT = 3,
    NET_WINDOW_FAULT_PROTECTION = 4,
    NET_WINDOW_FAULT_DOMAIN_TIMEOUT = 5,
    NET_WINDOW_FAULT_INITIALIZATION = 6,
    NET_WINDOW_FAULT_OTHER = 255
};
```

근거: S4A data/T31.md#type-NetWindowFaultCodeCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowFaultState"></a>
### `NetWindowFaultState`

```c
typedef uint8_t NetWindowFaultState;
```

근거: S4A data/T31.md#type-NetWindowFaultState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowFaultStateCode"></a>
### `NetWindowFaultStateCode`

```c
enum NetWindowFaultStateCode
{
    NET_WINDOW_FAULT_STATE_NONE = 0,
    NET_WINDOW_FAULT_STATE_ACTIVE = 1,
    NET_WINDOW_FAULT_STATE_RECOVERING = 2
};
```

근거: S4A data/T31.md#type-NetWindowFaultStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowMotionState"></a>
### `NetWindowMotionState`

```c
typedef uint8_t NetWindowMotionState;
```

근거: S4A data/T31.md#type-NetWindowMotionState. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-NetWindowMotionStateCode"></a>
### `NetWindowMotionStateCode`

```c
enum NetWindowMotionStateCode
{
    NET_WINDOW_MOTION_STOPPED = 0,
    NET_WINDOW_MOTION_OPENING = 1,
    NET_WINDOW_MOTION_CLOSING = 2,
    NET_WINDOW_MOTION_ANTIPINCH_REVERSING = 3,
    NET_WINDOW_MOTION_UNKNOWN = 255
};
```

근거: S4A data/T31.md#type-NetWindowMotionStateCode. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-OccupantHazardState_t"></a>
### `OccupantHazardState_t`

```c
typedef enum
{
    OCCUPANT_HAZARD_CLEAR = 0,
    OCCUPANT_HAZARD_ACTIVE,
    OCCUPANT_HAZARD_UNCONFIRMED
} OccupantHazardState_t;
```

근거: S4A data/T14.md#type-OccupantHazardState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-RearDetectionActivation_t"></a>
### `RearDetectionActivation_t`

```c
typedef enum
{
    REAR_DETECTION_DISABLED = 0,
    REAR_DETECTION_ACTIVE
} RearDetectionActivation_t;
```

근거: S4A data/T13.md#type-RearDetectionActivation_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-RearObstacleState_t"></a>
### `RearObstacleState_t`

```c
typedef enum
{
    REAR_OBSTACLE_STATE_CLEAR = 0,
    REAR_OBSTACLE_STATE_CAUTION,
    REAR_OBSTACLE_STATE_EMERGENCY
} RearObstacleState_t;
```

근거: S4A data/T13.md#type-RearObstacleState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-SourceEvidence_t"></a>
### `SourceEvidence_t`

```c
typedef struct
{
    SourceGeneration_t generation;
    UpdateSequence_t sequence;
    uint32_t ageMs;
} SourceEvidence_t;
```

근거: S4A data/T13.md#type-SourceEvidence_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `generation` | `SourceGeneration_t`; 32비트 부호 없음 | 원본 의미 생산자의 기동/세대. 다음 수신 홉의 BOOT_ID로 바꾸지 않는다. | 원본 의미 생산자(실행 환경 세대 포함) |
| `sequence` | `UpdateSequence_t`; 32비트 부호 없음 | 새 원본 관측·평가 번호. 캐시 재전송에서는 유지하며 TX_SEQUENCE와 다르다. | 원본 의미 생산자(실행 환경 세대 포함) |
| `ageMs` | `uint32_t`; 32비트 부호 없음 | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | 원본 의미 생산자(실행 환경 세대 포함) |

<a id="type-VssAvailability_t"></a>
### `VssAvailability_t`

```c
typedef enum
{
    VSS_AVAILABILITY_FULL = 0,
    VSS_AVAILABILITY_DEGRADED,
    VSS_AVAILABILITY_UNAVAILABLE
} VssAvailability_t;
```

근거: S4A data/T13.md#type-VssAvailability_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-VssOneShotEvent_t"></a>
### `VssOneShotEvent_t`

```c
typedef enum
{
    VSS_EVENT_VEHICLE_WELCOME = 0,
    VSS_EVENT_VEHICLE_GOODBYE,
    VSS_EVENT_DOOR_LOCK_COMPLETE,
    VSS_EVENT_DOOR_UNLOCK_COMPLETE,
    VSS_EVENT_DOOR_LOCK_ERROR
} VssOneShotEvent_t;
```

근거: S4A data/T13.md#type-VssOneShotEvent_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-VssServiceState_t"></a>
### `VssServiceState_t`

```c
typedef struct
{
    VssState_t state;
    VssAvailability_t availability;
    bool acceptingEvents;
    bool faultActive;
    SignalMeta_t meta;
} VssServiceState_t;
```

근거: S4A data/T13.md#type-VssServiceState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

| 필드 | C 타입·폭 | 값·단위·유효 조건 | 생성·관리 주체 |
|---|---|---|---|
| `state` | `VssState_t`; C enum 저장 폭 미검증 | VSS 음향 서비스의 실제 STARTUP/READY/PLAYING/FAULT 상태. READY 또는 PLAYING은 개별 Event의 실제 음향 시작·완료 증거가 아니다. | VSS 음향 서비스 담당자 |
| `availability` | `VssAvailability_t`; C enum 저장 폭 미검증 | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | VSS 음향 서비스 담당자 |
| `acceptingEvents` | `bool`; 저장 폭은 ABI 미검증 | VSS 이벤트 수용 가능 여부. 실제 재생 완료나 오디오 정상 여부와 별개. | VSS 음향 서비스 담당자 |
| `faultActive` | `bool`; 저장 폭은 ABI 미검증 | VSS 활성 고장 존재 여부. 발생 ID·last fault와 구분. | VSS 음향 서비스 담당자 |
| `meta` | `SignalMeta_t`; 중첩 타입 참조; sizeof 미검증 | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | VSS 음향 서비스 담당자 |

<a id="type-VssState_t"></a>
### `VssState_t`

```c
typedef enum
{
    VSS_STATE_STARTUP = 0,
    VSS_STATE_READY,
    VSS_STATE_PLAYING,
    VSS_STATE_FAULT
} VssState_t;
```

근거: S4A data/T13.md#type-VssState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-WindowCommand_t"></a>
### `WindowCommand_t`

```c
typedef enum
{
    WINDOW_COMMAND_OPEN = 0,
    WINDOW_COMMAND_CLOSE,
    WINDOW_COMMAND_STOP,
    WINDOW_COMMAND_VENT,
    WINDOW_COMMAND_MOVE_TO_POSITION
} WindowCommand_t;
```

근거: S4A data/T13.md#type-WindowCommand_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

<a id="type-WindowMotionState_t"></a>
### `WindowMotionState_t`

```c
typedef enum
{
    WINDOW_MOTION_UNKNOWN = 0,
    WINDOW_MOTION_STOPPED,
    WINDOW_MOTION_OPENING,
    WINDOW_MOTION_CLOSING,
    WINDOW_MOTION_ANTIPINCH_REVERSING
} WindowMotionState_t;
```

근거: S4A data/T13.md#type-WindowMotionState_t. 선언 바이트의 의미를 유지하며 제품 ABI 채택을 뜻하지 않는다.

## private 투영의 추가 필드 경계

`DomainIf_AntiPinchReport_t`의 occurrenceId는 Event 원 발생번호, sourceGeneration은 확인한 WINDOW 계보, meta는 동일 사건의 품질·AGE 연결 후보다. protectionState/outputDisabled/movementStopped/reason은 WINDOW_EVENT만으로 완성할 수 없으므로 추가 원 근거 없이는 호출하지 않는다. `DomainIf_RxWindowFault(const FaultInfo_t *)`는 occurrence/AGE/세대 전체를 표현하지 못하므로 private 원 사본과 동일 입력 결합이 필요하다.

| `FaultInfo_t` 실제 멤버 | WINDOW_FAULT 후보 대응 | VSS_STATUS 후보 대응 |
|---|---|---|
| category | raw SENSOR1→C2 SENSOR0, FUNCTION2→FUNCTION2, COMM3→COMM1을 이름으로 대응. raw NONE0의 동등 category 없음 | 완전 current mask 대표의 FUNCTION; 최근 last_fault나 CAN 통신 상실로 결정하지 않음 |
| state | raw NONE0/ACTIVE1/RECOVERING2→INACTIVE/ACTIVE/RECOVERING 이름 대응 | current mask 비공집합 ACTIVE, 공집합 INACTIVE; 과거 기록 삭제 아님 |
| faultCode | 원 fault_code와 C2 코드 채택 바인딩이 확인되어야 보존 | 완전 current mask의 최소 정의 code1..5, 공집합0; 위험 우선순위 아님 |
| quality | Wire에 독립 품질 필드가 없어 투영 근거 HOLD | LAST_FAULT_QUALITY는 이력 존재 품질이므로 현재 FaultInfo 품질과 자동 동치가 아님 |

`FaultInfo_t`에는 meta·occurrence·generation 멤버가 없다. 새 멤버를 만들거나 원 식별·AGE를 버린 채 성공 처리하지 않는다. 전체 원 수신 사본과 좁은 C2 입력의 동일성 및 실제 반영 연결을 확인하기 전에는 두 private 수신 경로의 투영을 보류한다.

`DomainIf_VssStateReport_t.service`는 NetVssStatus의 대응 상태/원 update/source AGE를 명시 투영한다. faultMask/lastFaultCode/lastFaultOccurrenceId/lastFaultAgeMs와 sourceGeneration은 완전 보고·원 세대 근거에서 보존하며 이력 품질0/3만으로 전체 faultQuality를 만들지 않는다. 두 C2 경계는 [A2 per-K 계약](../functions/can/VSS_STATUS.md#a2-domain-contract)으로 독립 수용한다. State는 Fault 미등록/실패로 취소하지 않으며 같은 K 반복·불명 결과에는 재호출하지 않는다. 참고 v0.5 service-only/observer-only와 확장 C2를 구분하고 E-S05 실제 지원·같은 입력·old K fence가 없는 해당 투영만 HOLD한다.

<a id="type-NetCanOccupantRestrictionEvidence"></a>
## PG6-SW01 private — 같은 Pi 관측의 현재 제한 근거

`PROPOSED_PRIVATE_TYPE`; 기존 M01/M04/M09 관측의 호출 중 사본이며 새 차량 상태 Manager나 Wire 자료가 아니다.

```c
typedef struct NetCanOccupantRestrictionEvidence {
    NetCisOccupant previous, incoming;
    NetCanRxMeta previousMeta, incomingMeta;
    uint32_t previousAgeAtNowMs, incomingAgeAtNowMs;
    uint32_t nowMs, originalLimitMs;
    bool elapsedKnown, originalLimitAdopted;
    bool hasStatus;
    NetCisStatus status;
    NetCanRxMeta statusMeta;
    uint8_t changedFields; /* bit0 Presence, bit1 Count; 그 외 금지 */
} NetCanOccupantRestrictionEvidence;
```

M04는 이미 보존한 동일 원 Pi Boot·vision sequence의 이전 DTO/meta와 이번 실제 수용 후보 DTO/meta를 복사한다. `changedFields`는 두 필드의 validity/reason을 직접 비교한 결과이며 caller의 승인 플래그가 아니다. AGE 둘은 동일한 `nowMs`로 환산하고 실제 시간원 연속성·포화·미반영 구간에서 `elapsedKnown`을 얻는다. `originalLimitMs`는 해당 관측의 기존 채택 근거가 있을 때만 사용하며 새로운 Pi TTL을 만들지 않는다.

`hasStatus`는 M09가 실제 수용한 CIS_STATUS 원 frame/meta를 함께 확보한 경우에만 참이다. 이 상태는 같은 현재 CIS Boot에 귀속되고 원 관측 수용 이후의 제한 사건이어야 한다. status 단순 READY는 회복 근거가 아니다. [검증 함수](../functions/can/CIS_OCCUPANT.md#restriction-check)가 같은 사건의 source·순서·시간·명시 필드값을 다시 확인한다. CIS 내부 [NetVisionRestrictionEvidence](UART_C_DATA.md#type-NetVisionRestrictionEvidence)는 Wire로 전송되지 않으므로 DOMAIN이 보유했다고 가정하지 않는다. DOMAIN에 실제 확보된 원자료로 증명되지 않는 현재 사유는 HOLD다.

CIS_STATUS의 비전 affected_function bit0는 Pi의 Presence/Count bit0/1과 범위가 다르다. 따라서 원 status의 비전 FAULT만으로 임의의 필드 제한을 입증하지 않는다. 변경된 각 필드와 그 CIS 사건의 채택된 대응·실제 원 기록 연결을 DOMAIN에서 확인할 수 없으면 해당 VISION_FAULT 분기는 HOLD다. 현재 소스에 없는 Pi 상세 사건을 전송받았다고 가정하지 않는다.

Pi의 원 INVALID 사유는 CIS의 원 기록에 그대로 남는다. CAN presence/count reason은 각 필드의 현재 전달 제한을 나타내며 이 한 벌만으로 Pi 원 사유와 현재 사유를 동시에 복원하지 않는다. 두 필드가 각각 사용 가능한 같은 판정일 때만 상호 모순을 검사한다. `ABSENT/VALID + count255/INVALID`는 모순이 아니며 UNKNOWN을0으로 바꾸지 않는다.

<a id="type-NetCisEnvironmentLocalEvent"></a>
## PG6-SW01 private — 환경의 원 기록과 로컬 무효화 사건

```c
typedef enum NetCisEnvironmentEventKind {
    NET_ENV_ORIGIN_EXPIRED, NET_ENV_RX_STOPPED, NET_ENV_BOOT_CHANGED
} NetCisEnvironmentEventKind;
typedef struct NetCisEnvironmentLocalEvent {
    NetCisEnvironmentEventKind kind;
    NetCisEnvironment retainedRaw;
    NetCanRxMeta retainedMeta;
    CisCabinEnvironmentState_t retainedC2;
    uint64_t recordRevision, eventRevision;
    uint32_t acceptedBootId, observedAtMs, elapsedSinceRxMs;
    uint32_t lastValidRxAtMs, rxStopLimitMs;
    bool elapsedKnown, rxStopPolicyAdopted;
    uint8_t affectedChannels; /* bit0 temperature, bit1 humidity, bit2 illuminance */
} NetCisEnvironmentLocalEvent;
```

이 두 private 타입은 M09의 기존 통신/인계 관측 저장소를 구체화한다. 공개 환경 DTO 여섯 멤버와 Wire40B는 그대로다. `retainedRaw/retainedMeta`는 마지막 실제 CAN 수용 사본이고 `retainedC2`는 그 입력에서 마지막으로 구성한 현재 품질의 전체 C2 사본이다. 원값·원seq·품질·AGE 기준과 서로의 연결을 함께 복사한다. `recordRevision/eventRevision`은 동일 원 기록·사건을 찾는 로컬 수명 식별이며 센서 seq·Generation·TX로 전송하지 않는다. 재사용 충돌을 막을 수 없는 바인딩에서는 사건을 만들지 않는다.

M09는 동일 로컬 clock에서 검증한 경과와 해당 CIS_ENVIRONMENT의 마지막 유효 수신 관측만 사용한다. 만료 mask는 온도·습도 각각 원 AGE+미반영 경과가2500 이상인 채널에서 계산한다. 조도1000과 CIS local5000을 이 계산에 넣지 않는다. RX_STOPPED는 채택된 해당 메시지 중단 정책과 실제 관측으로, BOOT_CHANGED는 M01이 승인한 현재 Boot 전이와 이전 원 기록 귀속으로 생성한다. 다른 메시지/CRC 오류/정확한 중복은 `lastValidRxAtMs`를 갱신하지 않는다.

사건은 새 CAN frame/meta가 아니다. 이전 meta의 BOOT/TX/수신시각을 보존하며 `observedAtMs`를 수신시각으로 덮지 않는다. 영향을 받지 않은 채널도 같은 원 기록의 현재 품질·독립 AGE를 보존한다. 새 Boot에서 아직 관측하지 않은 채널은 NO_DATA를 유지하며 이전 Boot의 큰 seq와 새 Boot의 낮은 seq를 숫자로 비교하지 않는다. 실제 C2의 세대 전이 수용 규칙은 [설계 미완료 경계](../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)에 별도 기록한다.

<a id="environment-event-observations"></a>
### PG6-SW02 — 환경 사건과 실제 저장 관측의 연결

`NetCisEnvironmentLocalEvent` 선언은 그대로다. `recordRevision`은 제한할 A 원 DTO/meta/C2 사본, `eventRevision`은 검증된 A→B 전이/제한 사건을 가리킨다. M01은 종료 A와 수용 B 및 B의 실제 원 DTO/meta를 기존 원본 저장소에서 구분한다. `transitionEpoch`는 이 두 revision·CIS BOOT_ID·센서 원 Sequence·CAN TX_SEQUENCE와 다른 로컬 전환 순서의 제안이며 DOMAIN Private 선언은 확인됐지만 Network 실제 바인딩 전 기존 Network 자료나 Wire에 struct 필드·폭·enum·영속 저장을 추가하지 않는다. 발급/수용의 Core 규칙은 [DOMAIN 순차 Epoch 구현](../functions/can/CIS_ENVIRONMENT.md#sw-b-epoch-proposal)으로 확인했으며 Network 실제 바인딩·Receipt 검증은 대기다.

기존 M09 항목은 같은 원 record/event에 대해 ① Network 소유 구 환경 대기·미실행 callback·미시작 Query 응답 차단 및 실행 중 자체 callback 직렬화, ② 고정한 BOOT 전환 요청의 한 번 제출과 실제 DOMAIN private 원 응답, ③ 현재 요청 내용/수용 BOOT/적용 Epoch/명확한 완료의 대조, ④ DOMAIN 구 작업 후속 commit 차단·VSM/독립 Meta·순서 문맥 무효화·Query 구 GOOD 방지, ⑤ 이후 B 입력의 실제 공개 Rx 반영과 Getter 관계를 구분한다. BOOT 전환용 공개 Rx NO_DATA 호출 기록을 완료 근거로 만들지 않는다. 일반 ORIGIN_EXPIRED/RX_STOPPED는 기존 공개 Rx 입력·원 반환·Core/Observer 관측을 유지한다. 이는 기존 원 사실의 구별이며 새 상태 enum/완료 bool을 추가하지 않는다. 구 작업 객체가 남아도 이후 commit이 확실히 봉쇄되고 무효화/완료가 확인되면 조건 충족 가능하다. callback 반환·빈 Queue·임의 nonzero 사건번호는 증거가 아니며 Private 원형·Receipt ABI는 확인됐지만 실제 Network Binding·환경 Rx/Query 연결은 OPEN이다.

`observedAtMs/elapsedSinceRxMs`는 원 제한 사건의 고정 시간 근거다. 이후 대기는 기존 owner의 실제 로컬 시각 관측에서 별도로 계산해 원 raw AGE에 한 번 누적한다. 사건 시각이나 retainedMeta를 나중 수신시각으로 덮지 않는다. B는 실제 B 원 기록의 시각·seq를 사용하며 A900→B0을901로 잇지 않는다. 기존 사본은 호출·비동기 관측 연결에 필요한 동안 보존하며 실제 참조 종료 전 재사용하지 않는다. 실제 DOMAIN 완료 공급 경로가 없다는 [BG-R05-BOOT](../functions/can/CIS_ENVIRONMENT.md#boot-binding-gap)는 유지한다.
