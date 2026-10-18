# DOMAIN — Vehicle_Types_v0.3.h 전체 의미 타입

Interface v2 · r1 · [규격과 연동 조건](../../07_CROSS_NODE_INTERFACE_RULES.md#status)

<a id="production-compatibility"></a>
## Production 참고 타입과 후보의 호환 조건

현재 `develop`의 Production C2 Target 소스는 `firmware/DOMAIN/src/interface/Vehicle_Types_v0.2.h`와 `Domain_Interface_v0.5_C2_target.h`로 확인됐다. 아래 전체 C 선언은 별도의 **Vehicle Types v0.3 후보**다. 과거 참고 `docs/Domain/architecture/1_Vehicle_Types/Vehicle_Types_v0.2.h`와 현재 Target v0.2가 같은 필드 구성이라고 가정하지 않는다. Target 빌드/ABI·실보드 검증은 별도다. 다음 차이는 참고 v0.2와 후보 v0.3의 선언 차이이며, 이행 승인이나 실제 ABI 검증 결과가 아니다.

| 대상 | v0.2 참고 → v0.3 후보의 차이 | 연결 전 확인 |
|---|---|---|
| `SessionId_t` | uint32_t → uint64_t | Session 절단·확장과 이를 포함한 모든 Request/Context/Result의 배치. 기존 Wire 폭 유지 |
| `CisCabinEnvironmentState_t.cabinIlluminance` | uint16_t → uint32_t | 조도 표현 범위·절단·정렬. 센서별 독립 품질 유지 |
| `CisRearState_t` | rearDistanceCm → rearDistanceMm, sourceGeneration 추가 | 현재 Production Target v0.2 `rearDistanceMm`에는 CAN raw 0.1cm=1mm를 그대로 전달한다. 과거 cm 참고 타입과 혼용 금지, Wire 단위 변경 금지 |
| `CisOccupantState_t` | 단일 quality → presenceQuality/countQuality, sourceGeneration·visionFunctionStatus 추가 | 필드별 부분 무효화·기능 상태·원 세대 보존 |
| `DoorState_t`, `ClimateState_t` | 필드별 DataQuality와 sourceGeneration 추가 | 실제 생산 세대와 CAN BOOT의 동일성 가정 금지 |
| `InteriorLightState_t` | bool commandApplied → applyState enum, 필드별 품질·sourceGeneration 추가 | 적용 성공·실패·미확인 의미 및 실제 피드백 구분 |
| `WindowState_t` | sourceGeneration 추가 | 상위 VehicleState/Getter까지 파급 확인. WINDOW 구현 보류 유지 |
| `DigitalKeyConnectionState_t`, `DigitalKeyState_t`, `ProximityInput_t` | 각각 appMeta, reason/meta, validity/reason 추가 | App 관측·기능 결과·근접 품질의 독립 의미 보존 |
| enum·원 출처 타입 | QualityReason/RearMeasurementState/ResultReason 확장; CisFunctionStatus·CommandOrigin·InteriorLightApplyState·SourceEvidence 추가 | C enum을 기존 Wire raw로 직접 복사하지 않음. 미지원 의미의 기존 차단 규칙 유지 |
| 별칭·include 경계 | SourceGeneration 추가, DeviceContextId/DomainCommandId 정의 위치 이동, include guard·version 매크로 변경 | 헤더 혼용·동명 타입 중복·잘못된 include 방지 |

변경 타입을 포함하는 상위 구조체와 모든 API 인자에도 크기·정렬 차이가 전파된다. 실제 ARM 컴파일러 옵션, sizeof/offset/alignment, enum 표현, 포인터·호출 ABI, Getter/TxPort의 복사 방식 및 단위 변환은 **BINDING_NOT_VERIFIED**다. 아래 후보 선언을 Production 선언으로 교체하거나, 아직 없는 세대·품질을 임의 생성하지 않는다. [C2 인자와 포트 차이](12_DOMAIN_TYPES.md#production-c2-compatibility)도 함께 적용한다.

**DECLARED_SOURCE / BINDING_OPEN.** 아래 선언은 Vehicle_Types v0.3 후보이며 ClimateState_t·InteriorLightState_t에 sourceGeneration을 포함한다. 양단 동일 헤더·ABI 연결은 검증 전이다. 설명 표에는 원본 데이터의 생성 주체·유효성·수명을 명시한다. 이 문서는 컴파일용 헤더가 아니며 C enum/padding을 Wire로 복사하지 않는다.

[노드 Data](10_DATA.md) · [노드 API](20_API.md) · [원 선언 헤더](https://github.com/vehicle-domain-control-system/integrated-vehicle-control/blob/36b4f43db03859518dd82664faa2464d13875a75/firmware/DOMAIN/src/interface/Vehicle_Types_v0.3.h)

<a id="r1-generation-preservation"></a>
`ClimateState_t.sourceGeneration`과 `InteriorLightState_t.sourceGeneration`은 이 초안의 기존 후보 선언에 이미 포함된 필드다. r1에서는 두 C 선언을 그대로 보존한다. 초기화·복사·상위 복합 자료·Getter·MOBILE 투영 전 과정가 원 생산 세대를 보존하는지 실제 Header/ABI와 함께 확인해야 한다. 유효 관측의 nonzero, 이미 알려진 세대를 INVALID/NO_DATA라고 0으로 지우지 않는 조건을 유지하며, **STALE + sourceGeneration 0은 POLICY_OPEN**이다. CAN BOOT와 생산 세대의 lifecycle이 같다는 근거 없이 단순 대입하지 않는다. 필드 존재는 실제 양단 연결 PASS가 아니다.

## 타입 바로 찾기

| 사용하는 의미 | 전체 선언·필드 표 |
|---|---|
| 식별·원 출처 | [`RequestId_t`](#type-requestid-t), [`SessionId_t`](#type-sessionid-t), [`OccurrenceId_t`](#type-occurrenceid-t), [`UpdateSequence_t`](#type-updatesequence-t), [`DeviceContextId_t`](#type-devicecontextid-t), [`DomainCommandId_t`](#type-domaincommandid-t), [`SourceGeneration_t`](#type-sourcegeneration-t), [`SourceEvidence_t`](#type-sourceevidence-t) |
| 품질·가용성·메타 | [`CommandOrigin_t`](#type-commandorigin-t), [`DataQuality_t`](#type-dataquality-t), [`ValueValidity_t`](#type-valuevalidity-t), [`QualityReason_t`](#type-qualityreason-t), [`FunctionAvailability_t`](#type-functionavailability-t), [`SignalMeta_t`](#type-signalmeta-t), [`ValueQuality_t`](#type-valuequality-t) |
| 요청·결과·Fault | [`RequestResult_t`](#type-requestresult-t), [`ResultConfirmation_t`](#type-resultconfirmation-t), [`ResultReason_t`](#type-resultreason-t), [`RequestContext_t`](#type-requestcontext-t), [`RequestResultInfo_t`](#type-requestresultinfo-t), [`FaultCategory_t`](#type-faultcategory-t), [`FaultState_t`](#type-faultstate-t), [`FaultInfo_t`](#type-faultinfo-t) |
| BCM 도어·ECU | [`CommonEcuState_t`](#type-commonecustate-t), [`DoorLockState_t`](#type-doorlockstate-t), [`DoorOpenState_t`](#type-dooropenstate-t), [`DoorCompositeState_t`](#type-doorcompositestate-t), [`DoorLockTarget_t`](#type-doorlocktarget-t), [`DoorState_t`](#type-doorstate-t) |
| BCM 공조·실내 조명 | [`FanLevel_t`](#type-fanlevel-t), [`ThermalDirection_t`](#type-thermaldirection-t), [`HeatRemovalState_t`](#type-heatremovalstate-t), [`ClimateState_t`](#type-climatestate-t), [`InteriorLightType_t`](#type-interiorlighttype-t), [`RgbColor_t`](#type-rgbcolor-t), [`InteriorLightApplyState_t`](#type-interiorlightapplystate-t), [`InteriorLightState_t`](#type-interiorlightstate-t) |
| 등록·연결·근접 | [`DeviceRegistrationState_t`](#type-deviceregistrationstate-t), [`BtConnectionState_t`](#type-btconnectionstate-t), [`AppActiveState_t`](#type-appactivestate-t), [`ProximityState_t`](#type-proximitystate-t), [`DigitalKeySetting_t`](#type-digitalkeysetting-t), [`UnlockOrigin_t`](#type-unlockorigin-t) |
| DigitalKey 의미 | [`DigitalKeyConnectionState_t`](#type-digitalkeyconnectionstate-t), [`ProximityInput_t`](#type-proximityinput-t), [`DigitalKeyInput_t`](#type-digitalkeyinput-t), [`DigitalKeyState_t`](#type-digitalkeystate-t) |
| CIS 센서·관측 | [`RearMeasurementState_t`](#type-rearmeasurementstate-t), [`CisState_t`](#type-cisstate-t), [`CisFunctionStatus_t`](#type-cisfunctionstatus-t), [`CisOccupantState_t`](#type-cisoccupantstate-t), [`CisCabinEnvironmentState_t`](#type-ciscabinenvironmentstate-t), [`CisRearState_t`](#type-cisrearstate-t), [`CisEnvironmentState_t`](#type-cisenvironmentstate-t) |
| WINDOW · 구현 보류 | [`WindowCommand_t`](#type-windowcommand-t), [`WindowMotionState_t`](#type-windowmotionstate-t), [`AntiPinchState_t`](#type-antipinchstate-t), [`WindowState_t`](#type-windowstate-t), [`WindowCommandRequest_t`](#type-windowcommandrequest-t) |
| VSS·경고·통합 Snapshot | [`VssState_t`](#type-vssstate-t), [`VssAvailability_t`](#type-vssavailability-t), [`RearObstacleState_t`](#type-rearobstaclestate-t), [`RearDetectionActivation_t`](#type-reardetectionactivation-t), [`VssOneShotEvent_t`](#type-vssoneshotevent-t), [`VssServiceState_t`](#type-vssservicestate-t), [`VehicleState_t`](#type-vehiclestate-t) |

<a id="type-requestid-t"></a>
## `RequestId_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t RequestId_t;
```

**범위·소유:** u32 MOBILE 원 요청 식별. 재전달/조회로 새로 만들지 않는다.

<a id="type-sessionid-t"></a>
## `SessionId_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint64_t SessionId_t;
```

**범위·소유:** 검증된 u64 Session. 수신 대상 Session과 과거 요청 Session을 구분. 발급·교체 정책 미확정.

<a id="type-occurrenceid-t"></a>
## `OccurrenceId_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t OccurrenceId_t;
```

**범위·소유:** u32 의미 사건 식별. 원 producer generation과 함께 사용.

<a id="type-updatesequence-t"></a>
## `UpdateSequence_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t UpdateSequence_t;
```

**범위·소유:** u32 새 원본 평가 번호. frame 송신 번호와 다름.

<a id="type-devicecontextid-t"></a>
## `DeviceContextId_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t DeviceContextId_t;
```

**범위·소유:** u32 검증된 단말 context. BLE 실제 peer와 binding.

<a id="type-domaincommandid-t"></a>
## `DomainCommandId_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t DomainCommandId_t;
```

**범위·소유:** DOMAIN이 확정한 u32 명령 식별자. 단일 Domain 기능 owner 발급, Network 재생성 금지.

<a id="type-sourcegeneration-t"></a>
## `SourceGeneration_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef uint32_t SourceGeneration_t;
```

**범위·소유:** u32 원 의미 생산 세대. 링크마다 바뀌는 transport Boot와 구별.

<a id="type-sourceevidence-t"></a>
## `SourceEvidence_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    SourceGeneration_t generation;
    UpdateSequence_t sequence;
    uint32_t ageMs;
} SourceEvidence_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `generation` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원본 의미 생산자의 기동/세대. 다음 수신 홉의 BOOT_ID로 바꾸지 않는다. | 원 의미 생산자(runtime 세대 포함) |
| `sequence` | [`UpdateSequence_t`](11_VEHICLE_TYPES.md#type-updatesequence-t) | 새 원본 관측·평가 번호. 캐시 재전송에서는 유지하며 TX_SEQUENCE와 다르다. | 원 의미 생산자(runtime 세대 포함) |
| `ageMs` | `uint32_t` | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | 원 의미 생산자(runtime 세대 포함) |

<a id="type-commandorigin-t"></a>
## `CommandOrigin_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    COMMAND_ORIGIN_MOBILE_REQUEST = 0,
    COMMAND_ORIGIN_DIGITAL_KEY,
    COMMAND_ORIGIN_CLIMATE_POLICY,
    COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY,
    COMMAND_ORIGIN_AUTO_VENTILATION,
    COMMAND_ORIGIN_SYSTEM
} CommandOrigin_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `COMMAND_ORIGIN_MOBILE_REQUEST` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `COMMAND_ORIGIN_DIGITAL_KEY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `COMMAND_ORIGIN_CLIMATE_POLICY` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `COMMAND_ORIGIN_AUTO_VENTILATION` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `COMMAND_ORIGIN_SYSTEM` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-dataquality-t"></a>
## `DataQuality_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DATA_QUALITY_OK = 0,
    DATA_QUALITY_STALE,
    DATA_QUALITY_INVALID,
    DATA_QUALITY_NO_DATA
} DataQuality_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DATA_QUALITY_OK` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DATA_QUALITY_STALE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DATA_QUALITY_INVALID` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DATA_QUALITY_NO_DATA` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-valuevalidity-t"></a>
## `ValueValidity_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    VALUE_VALIDITY_INVALID = 0,
    VALUE_VALIDITY_VALID
} ValueValidity_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `VALUE_VALIDITY_INVALID` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VALUE_VALIDITY_VALID` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-qualityreason-t"></a>
## `QualityReason_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `QUALITY_REASON_NONE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_NOT_READY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_OUT_OF_RANGE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_SENSOR_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_VISION_FAULT` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_STALE` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_NO_DATA` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_COMMUNICATION_FAULT` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_RECOVERING` | 8 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `QUALITY_REASON_DATA_INVALID` | 9 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-functionavailability-t"></a>
## `FunctionAvailability_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    FUNCTION_AVAILABILITY_AVAILABLE = 0,
    FUNCTION_AVAILABILITY_DEGRADED,
    FUNCTION_AVAILABILITY_UNAVAILABLE,
    FUNCTION_AVAILABILITY_UNSUPPORTED
} FunctionAvailability_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `FUNCTION_AVAILABILITY_AVAILABLE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FUNCTION_AVAILABILITY_DEGRADED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FUNCTION_AVAILABILITY_UNAVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FUNCTION_AVAILABILITY_UNSUPPORTED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-signalmeta-t"></a>
## `SignalMeta_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DataQuality_t quality;
    uint32_t ageMs;
    UpdateSequence_t updateSequence;
} SignalMeta_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `ageMs` | `uint32_t` | 원본 관측/평가/사건 후 누적 경과, ms, u32. 수신·대기 경과를 한 번만 더하고 중계에서 0으로 초기화하지 않는다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `updateSequence` | [`UpdateSequence_t`](11_VEHICLE_TYPES.md#type-updatesequence-t) | 새 원본 평가 번호. 송신 횟수·요청 ID와 분리하며 원본 재전달에서 증가하지 않는다. | 이 타입을 포함한 객체의 원 평가/고장 owner |

<a id="type-valuequality-t"></a>
## `ValueQuality_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ValueQuality_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `validity` | [`ValueValidity_t`](11_VEHICLE_TYPES.md#type-valuevalidity-t) | 해당 값의 유효성을 나타내는 typed 상태. 품질 손실을 정상 bool/0으로 표현하지 않는다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `reason` | [`QualityReason_t`](11_VEHICLE_TYPES.md#type-qualityreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | 이 타입을 포함한 객체의 원 평가/고장 owner |

<a id="type-requestresult-t"></a>
## `RequestResult_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    REQUEST_RESULT_ACCEPTED = 0,
    REQUEST_RESULT_IN_PROGRESS,
    REQUEST_RESULT_DONE,
    REQUEST_RESULT_REJECTED,
    REQUEST_RESULT_CANCELLED,
    REQUEST_RESULT_FAILED
} RequestResult_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `REQUEST_RESULT_ACCEPTED` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REQUEST_RESULT_IN_PROGRESS` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REQUEST_RESULT_DONE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REQUEST_RESULT_REJECTED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REQUEST_RESULT_CANCELLED` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REQUEST_RESULT_FAILED` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-resultconfirmation-t"></a>
## `ResultConfirmation_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    RESULT_CONFIRMATION_UNCONFIRMED = 0,
    RESULT_CONFIRMATION_CONFIRMED
} ResultConfirmation_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `RESULT_CONFIRMATION_UNCONFIRMED` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_CONFIRMATION_CONFIRMED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-resultreason-t"></a>
## `ResultReason_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    RESULT_REASON_NONE = 0,
    RESULT_REASON_LOCAL_OVERRIDE,
    RESULT_REASON_STOP_REQUESTED,
    RESULT_REASON_SUPERSEDED,
    RESULT_REASON_ANTIPINCH,
    RESULT_REASON_NOT_ALLOWED,
    RESULT_REASON_INVALID_COMMAND,
    RESULT_REASON_STATE_UNTRUSTED,
    RESULT_REASON_NO_FEEDBACK,
    RESULT_REASON_DRIVE_LIMIT_EXCEEDED,
    RESULT_REASON_MOTOR_FAULT,
    RESULT_REASON_SENSOR_FAULT,
    RESULT_REASON_TIMEOUT,
    RESULT_REASON_OTHER, /* internal-only unless an explicit wire mapping exists */
    RESULT_REASON_NOT_REGISTERED,
    RESULT_REASON_LINK_LOST,
    RESULT_REASON_SESSION_INVALID,
    RESULT_REASON_DOOR_OPEN,
    RESULT_REASON_ALREADY_AT_TARGET,
    RESULT_REASON_FAULT,
    RESULT_REASON_STALE,
    RESULT_REASON_NO_DATA,
    RESULT_REASON_NOT_READY,
    RESULT_REASON_OUT_OF_RANGE,
    RESULT_REASON_SOURCE_FAILURE,
    RESULT_REASON_UNKNOWN_REQUEST,
    RESULT_REASON_EXPIRED,
    RESULT_REASON_ID_CONFLICT,
    RESULT_REASON_POWER_NOT_ALLOWED,
    RESULT_REASON_OVERHEAT,
    RESULT_REASON_FAN_MISMATCH
} ResultReason_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `RESULT_REASON_NONE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_LOCAL_OVERRIDE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_STOP_REQUESTED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_SUPERSEDED` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_ANTIPINCH` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_NOT_ALLOWED` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_INVALID_COMMAND` | 6 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_STATE_UNTRUSTED` | 7 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_NO_FEEDBACK` | 8 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_DRIVE_LIMIT_EXCEEDED` | 9 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_MOTOR_FAULT` | 10 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_SENSOR_FAULT` | 11 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_TIMEOUT` | 12 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_OTHER` | 13 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_NOT_REGISTERED` | 14 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_LINK_LOST` | 15 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_SESSION_INVALID` | 16 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_DOOR_OPEN` | 17 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_ALREADY_AT_TARGET` | 18 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_FAULT` | 19 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_STALE` | 20 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_NO_DATA` | 21 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_NOT_READY` | 22 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_OUT_OF_RANGE` | 23 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_SOURCE_FAILURE` | 24 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_UNKNOWN_REQUEST` | 25 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_EXPIRED` | 26 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_ID_CONFLICT` | 27 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_POWER_NOT_ALLOWED` | 28 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_OVERHEAT` | 29 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `RESULT_REASON_FAN_MISMATCH` | 30 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-requestcontext-t"></a>
## `RequestContext_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    SessionId_t sessionId;
    RequestId_t requestId;
} RequestContext_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `sessionId` | [`SessionId_t`](11_VEHICLE_TYPES.md#type-sessionid-t) | 원 Session 식별 u64. 현재 전달 대상 Session과 원 요청 Session을 섞지 않는다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `requestId` | [`RequestId_t`](11_VEHICLE_TYPES.md#type-requestid-t) | MOBILE 원 요청 식별 u32. 동일 요청 재전달·결과 조회 시 불변. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |

<a id="type-requestresultinfo-t"></a>
## `RequestResultInfo_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    RequestContext_t context;
    RequestResult_t result;
    ResultReason_t reason;
    ResultConfirmation_t confirmation;
} RequestResultInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`RequestContext_t`](11_VEHICLE_TYPES.md#type-requestcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `result` | [`RequestResult_t`](11_VEHICLE_TYPES.md#type-requestresult-t) | 기능 owner가 판정한 차량 처리 단계. Network local OK와 구분하며 DONE은 실행 근거가 있을 때만. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |
| `confirmation` | [`ResultConfirmation_t`](11_VEHICLE_TYPES.md#type-resultconfirmation-t) | 차량 결과 확인 여부. UNCONFIRMED를 FAILED로 치환하지 않는다. | MOBILE 원 의도→Gateway 인증→Domain 수용 owner |

<a id="type-faultcategory-t"></a>
## `FaultCategory_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    FAULT_CATEGORY_SENSOR = 0,
    FAULT_CATEGORY_COMM,
    FAULT_CATEGORY_FUNCTION
} FaultCategory_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `FAULT_CATEGORY_SENSOR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAULT_CATEGORY_COMM` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAULT_CATEGORY_FUNCTION` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-faultstate-t"></a>
## `FaultState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    FAULT_STATE_INACTIVE = 0,
    FAULT_STATE_ACTIVE,
    FAULT_STATE_RECOVERING
} FaultState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `FAULT_STATE_INACTIVE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAULT_STATE_ACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAULT_STATE_RECOVERING` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-faultinfo-t"></a>
## `FaultInfo_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    FaultCategory_t category;
    FaultState_t state;
    uint16_t faultCode;
    DataQuality_t quality;
} FaultInfo_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `category` | [`FaultCategory_t`](11_VEHICLE_TYPES.md#type-faultcategory-t) | 고장의 SENSOR/COMM/FUNCTION 범주. C SENSOR=0과 Wire NONE=0의 의미가 다르다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `state` | [`FaultState_t`](11_VEHICLE_TYPES.md#type-faultstate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `faultCode` | `uint16_t` | 고장 식별 u16. 0은 활성 고장 없음; 이때 범주·상태·품질 해석을 하지 않는 source 계약을 적용한다. | 이 타입을 포함한 객체의 원 평가/고장 owner |
| `quality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 이 값의 품질. OK/STALE/INVALID/NO_DATA를 정상 값/위험 해제 여부와 분리한다. | 이 타입을 포함한 객체의 원 평가/고장 owner |

<a id="type-commonecustate-t"></a>
## `CommonEcuState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    ECU_STATE_INIT = 0,
    ECU_STATE_READY,
    ECU_STATE_DEGRADED,
    ECU_STATE_FAULT
} CommonEcuState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `ECU_STATE_INIT` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ECU_STATE_READY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ECU_STATE_DEGRADED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ECU_STATE_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-doorlockstate-t"></a>
## `DoorLockState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOOR_LOCK_STATE_UNKNOWN = 0,
    DOOR_LOCK_STATE_LOCKED,
    DOOR_LOCK_STATE_UNLOCKED
} DoorLockState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOOR_LOCK_STATE_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_LOCK_STATE_LOCKED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_LOCK_STATE_UNLOCKED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-dooropenstate-t"></a>
## `DoorOpenState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOOR_OPEN_STATE_UNKNOWN = 0,
    DOOR_OPEN_STATE_CLOSED,
    DOOR_OPEN_STATE_OPEN
} DoorOpenState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOOR_OPEN_STATE_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_OPEN_STATE_CLOSED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_OPEN_STATE_OPEN` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-doorcompositestate-t"></a>
## `DoorCompositeState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOOR_COMPOSITE_STATE_NORMAL = 0,
    DOOR_COMPOSITE_STATE_INCONSISTENT,
    DOOR_COMPOSITE_STATE_UNTRUSTED
} DoorCompositeState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOOR_COMPOSITE_STATE_NORMAL` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_COMPOSITE_STATE_INCONSISTENT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_COMPOSITE_STATE_UNTRUSTED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-doorlocktarget-t"></a>
## `DoorLockTarget_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DOOR_TARGET_LOCK = 0,
    DOOR_TARGET_UNLOCK
} DoorLockTarget_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DOOR_TARGET_LOCK` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DOOR_TARGET_UNLOCK` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-doorstate-t"></a>
## `DoorState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DoorLockState_t lockState;
    DoorOpenState_t openState;
    DoorCompositeState_t compositeState;
    DataQuality_t lockQuality;
    DataQuality_t openQuality;
    DataQuality_t compositeQuality;
    SourceGeneration_t sourceGeneration; /* DOOR_LEFT_OPEN warning source */
    SignalMeta_t meta; /* shared age/sequence; quality is conservative summary */
} DoorState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `lockState` | [`DoorLockState_t`](11_VEHICLE_TYPES.md#type-doorlockstate-t) | BCM 실제 잠금 상태. UNKNOWN/LOCKED/UNLOCKED, lockQuality와 함께 소비. | BCM 해당 구동·관측·보호 owner |
| `openState` | [`DoorOpenState_t`](11_VEHICLE_TYPES.md#type-dooropenstate-t) | BCM 실제 개폐 상태. UNKNOWN/CLOSED/OPEN, openQuality와 함께 소비. | BCM 해당 구동·관측·보호 owner |
| `compositeState` | [`DoorCompositeState_t`](11_VEHICLE_TYPES.md#type-doorcompositestate-t) | BCM 잠금·개폐 조합 상태. 기본 관측과 모순 없는지 검사하고 compositeQuality를 유지. | BCM 해당 구동·관측·보호 owner |
| `lockQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 잠금 관측의 독립 품질. 타 필드 품질로 덮어쓰지 않는다. | BCM 해당 구동·관측·보호 owner |
| `openQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 개폐 관측의 독립 품질. | BCM 해당 구동·관측·보호 owner |
| `compositeQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 잠금·개폐 조합 평가 품질. | BCM 해당 구동·관측·보호 owner |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 owner |

<a id="type-fanlevel-t"></a>
## `FanLevel_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    FAN_LEVEL_OFF = 0,
    FAN_LEVEL_LOW,
    FAN_LEVEL_MEDIUM,
    FAN_LEVEL_HIGH,
    FAN_LEVEL_UNKNOWN
} FanLevel_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `FAN_LEVEL_OFF` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAN_LEVEL_LOW` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAN_LEVEL_MEDIUM` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAN_LEVEL_HIGH` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `FAN_LEVEL_UNKNOWN` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-thermaldirection-t"></a>
## `ThermalDirection_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    THERMAL_DIRECTION_IDLE = 0,
    THERMAL_DIRECTION_COOL,
    THERMAL_DIRECTION_HEAT
} ThermalDirection_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `THERMAL_DIRECTION_IDLE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `THERMAL_DIRECTION_COOL` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `THERMAL_DIRECTION_HEAT` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-heatremovalstate-t"></a>
## `HeatRemovalState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    HEAT_REMOVAL_STATE_NORMAL = 0,
    HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT,
    HEAT_REMOVAL_STATE_UNMEASURABLE
} HeatRemovalState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `HEAT_REMOVAL_STATE_NORMAL` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `HEAT_REMOVAL_STATE_UNMEASURABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-climatestate-t"></a>
## `ClimateState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    FanLevel_t commandedFanLevel;
    FanLevel_t measuredFanLevel;
    ThermalDirection_t thermalDirection;
    uint8_t thermalOutputLevelPercent;
    HeatRemovalState_t heatRemovalState;
    DataQuality_t commandedFanQuality;
    DataQuality_t measuredFanQuality;
    DataQuality_t thermalDirectionQuality;
    DataQuality_t thermalOutputQuality;
    DataQuality_t heatRemovalQuality;
    SignalMeta_t meta; /* per-field quality is authoritative */
    SourceGeneration_t sourceGeneration; /* accepted candidate: original producer generation */
} ClimateState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `commandedFanLevel` | [`FanLevel_t`](11_VEHICLE_TYPES.md#type-fanlevel-t) | BCM에 적용된 팬 목표 OFF/LOW/MEDIUM/HIGH; measuredFanLevel과 별개. | BCM 해당 구동·관측·보호 owner |
| `measuredFanLevel` | [`FanLevel_t`](11_VEHICLE_TYPES.md#type-fanlevel-t) | BCM 실제 팬 피드백. 목표만으로 실제 구동을 확인했다고 판단하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `thermalDirection` | [`ThermalDirection_t`](11_VEHICLE_TYPES.md#type-thermaldirection-t) | BCM 열 출력 방향 IDLE/COOL/HEAT. 방향 품질과 별도로 소비. | BCM 해당 구동·관측·보호 owner |
| `thermalOutputLevelPercent` | `uint8_t` | BCM 열 출력 지시량 0~100%, 실제 열 효과 확인과 구분. | BCM 해당 구동·관측·보호 owner |
| `heatRemovalState` | [`HeatRemovalState_t`](11_VEHICLE_TYPES.md#type-heatremovalstate-t) | BCM 잔열 제거 동작 상태. 목표 팬 값과 별도 기능 판단. | BCM 해당 구동·관측·보호 owner |
| `commandedFanQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 팬 지시값 품질. | BCM 해당 구동·관측·보호 owner |
| `measuredFanQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 팬 실제 피드백 품질. | BCM 해당 구동·관측·보호 owner |
| `thermalDirectionQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 열 방향 평가 품질. | BCM 해당 구동·관측·보호 owner |
| `thermalOutputQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 열 출력 지시량 품질. | BCM 해당 구동·관측·보호 owner |
| `heatRemovalQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 잔열 제거 상태 품질. | BCM 해당 구동·관측·보호 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 owner |
| `sourceGeneration` | `SourceGeneration_t` | 원 상태 생산 세대. 유효 관측 nonzero 후보, 미확인 0. 이미 아는 세대를 무효 품질 때문에 지우지 않음. STALE+0 정책 미결. | 해당 상태 원 생산자 |

<a id="type-interiorlighttype-t"></a>
## `InteriorLightType_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    INTERIOR_LIGHT_TYPE_NORMAL = 0,
    INTERIOR_LIGHT_TYPE_GOODBYE,
    INTERIOR_LIGHT_TYPE_WARNING,
    INTERIOR_LIGHT_TYPE_FAULT
} InteriorLightType_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `INTERIOR_LIGHT_TYPE_NORMAL` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `INTERIOR_LIGHT_TYPE_GOODBYE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `INTERIOR_LIGHT_TYPE_WARNING` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `INTERIOR_LIGHT_TYPE_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-rgbcolor-t"></a>
## `RgbColor_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RgbColor_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `red` | `uint8_t` | RGB 빨강 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `green` | `uint8_t` | RGB 초록 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `blue` | `uint8_t` | RGB 파랑 채널, 0~255. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-interiorlightapplystate-t"></a>
## `InteriorLightApplyState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    INTERIOR_LIGHT_APPLY_UNKNOWN = 0,
    INTERIOR_LIGHT_APPLY_APPLIED,
    INTERIOR_LIGHT_APPLY_FAILED
} InteriorLightApplyState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `INTERIOR_LIGHT_APPLY_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `INTERIOR_LIGHT_APPLY_APPLIED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `INTERIOR_LIGHT_APPLY_FAILED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-interiorlightstate-t"></a>
## `InteriorLightState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    InteriorLightType_t type;
    uint8_t levelPercent;
    RgbColor_t color;
    InteriorLightApplyState_t applyState; /* UNKNOWN is not false/FAILED */
    bool physicalFeedbackSupported;
    DataQuality_t typeQuality;
    DataQuality_t levelQuality;
    DataQuality_t colorQuality;
    DataQuality_t applyQuality;
    SignalMeta_t meta;
    SourceGeneration_t sourceGeneration; /* accepted candidate: original producer generation */
} InteriorLightState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `type` | [`InteriorLightType_t`](11_VEHICLE_TYPES.md#type-interiorlighttype-t) | BCM에 적용된 실내 조명 종류. 실제 점등 확인과 구분. | BCM 해당 구동·관측·보호 owner |
| `levelPercent` | `uint8_t` | 밝기 0~100%. 값 범위를 확인하고 enum/목표와 함께 부호화. | BCM 해당 구동·관측·보호 owner |
| `color` | [`RgbColor_t`](11_VEHICLE_TYPES.md#type-rgbcolor-t) | RGB 각 0~255 인 의미 색상. C struct padding을 raw로 복사하지 않는다. | BCM 해당 구동·관측·보호 owner |
| `applyState` | [`InteriorLightApplyState_t`](11_VEHICLE_TYPES.md#type-interiorlightapplystate-t) | BCM 조명 지시 적용 상태. 실제 물리 점등 확인과 구분. | BCM 해당 구동·관측·보호 owner |
| `physicalFeedbackSupported` | `bool` | 실제 점등 피드백 지원 여부 bool. false 이면 실제 점등 완료를 만들어내지 않는다. | BCM 해당 구동·관측·보호 owner |
| `typeQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 조명 종류 독립 품질. | BCM 해당 구동·관측·보호 owner |
| `levelQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 조명 밝기 독립 품질. | BCM 해당 구동·관측·보호 owner |
| `colorQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | RGB 독립 품질. | BCM 해당 구동·관측·보호 owner |
| `applyQuality` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 조명 지시 적용 독립 품질. | BCM 해당 구동·관측·보호 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | BCM 해당 구동·관측·보호 owner |
| `sourceGeneration` | `SourceGeneration_t` | 원 상태 생산 세대. 유효 관측 nonzero 후보, 미확인 0. 이미 아는 세대를 무효 품질 때문에 지우지 않음. STALE+0 정책 미결. | 해당 상태 원 생산자 |

<a id="type-deviceregistrationstate-t"></a>
## `DeviceRegistrationState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DEVICE_REGISTRATION_UNKNOWN = 0,
    DEVICE_REGISTRATION_NOT_REGISTERED,
    DEVICE_REGISTRATION_REGISTERED
} DeviceRegistrationState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DEVICE_REGISTRATION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DEVICE_REGISTRATION_NOT_REGISTERED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DEVICE_REGISTRATION_REGISTERED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-btconnectionstate-t"></a>
## `BtConnectionState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    BT_CONNECTION_UNKNOWN = 0,
    BT_CONNECTION_DISCONNECTED,
    BT_CONNECTION_CONNECTED
} BtConnectionState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `BT_CONNECTION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BT_CONNECTION_DISCONNECTED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `BT_CONNECTION_CONNECTED` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-appactivestate-t"></a>
## `AppActiveState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    APP_ACTIVE_UNKNOWN = 0,
    APP_ACTIVE_INACTIVE,
    APP_ACTIVE_ACTIVE
} AppActiveState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `APP_ACTIVE_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `APP_ACTIVE_INACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `APP_ACTIVE_ACTIVE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-proximitystate-t"></a>
## `ProximityState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    PROXIMITY_UNKNOWN = 0,
    PROXIMITY_FAR,
    PROXIMITY_NEAR
} ProximityState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `PROXIMITY_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `PROXIMITY_FAR` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `PROXIMITY_NEAR` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-digitalkeysetting-t"></a>
## `DigitalKeySetting_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    DIGITAL_KEY_SETTING_OFF = 0,
    DIGITAL_KEY_SETTING_ON
} DigitalKeySetting_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `DIGITAL_KEY_SETTING_OFF` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `DIGITAL_KEY_SETTING_ON` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-unlockorigin-t"></a>
## `UnlockOrigin_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    UNLOCK_ORIGIN_USER_REQUEST = 0,
    UNLOCK_ORIGIN_PROXIMITY_AUTO
} UnlockOrigin_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `UNLOCK_ORIGIN_USER_REQUEST` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `UNLOCK_ORIGIN_PROXIMITY_AUTO` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-digitalkeyconnectionstate-t"></a>
## `DigitalKeyConnectionState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DeviceRegistrationState_t registration;
    BtConnectionState_t btConnection;
    AppActiveState_t appActive;
    SessionId_t sessionId;
    SignalMeta_t meta;    /* ONE coherent Registration/Connection quality/sequence/age */
    SignalMeta_t appMeta; /* App activity sequence/age/quality, independently */
} DigitalKeyConnectionState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `registration` | [`DeviceRegistrationState_t`](11_VEHICLE_TYPES.md#type-deviceregistrationstate-t) | 검증된 등록 단말 여부. BLE 연결·이름·주소만으로 등록 성공 생성 금지. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `btConnection` | [`BtConnectionState_t`](11_VEHICLE_TYPES.md#type-btconnectionstate-t) | 현재 BLE 연결 관측. App ACTIVE 및 차량 등록 여부와 분리. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `appActive` | [`AppActiveState_t`](11_VEHICLE_TYPES.md#type-appactivestate-t) | MOBILE 원 활성 평가. 단순 BLE CONNECTED로 ACTIVE를 만들지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `sessionId` | [`SessionId_t`](11_VEHICLE_TYPES.md#type-sessionid-t) | 원 Session 식별 u64. 현재 전달 대상 Session과 원 요청 Session을 섞지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `appMeta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | App 활성의 독립 품질/AGE/UPDATE_SEQUENCE. 등록·연결 공동 meta와 다르다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-proximityinput-t"></a>
## `ProximityInput_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    ProximityState_t state;
    ValueValidity_t validity;
    QualityReason_t reason;
    SignalMeta_t meta;
} ProximityInput_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`ProximityState_t`](11_VEHICLE_TYPES.md#type-proximitystate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `validity` | [`ValueValidity_t`](11_VEHICLE_TYPES.md#type-valuevalidity-t) | 해당 값의 유효성을 나타내는 typed 상태. 품질 손실을 정상 bool/0으로 표현하지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `reason` | [`QualityReason_t`](11_VEHICLE_TYPES.md#type-qualityreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-digitalkeyinput-t"></a>
## `DigitalKeyInput_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DigitalKeyConnectionState_t connection;
    ProximityInput_t proximity;
} DigitalKeyInput_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `connection` | [`DigitalKeyConnectionState_t`](11_VEHICLE_TYPES.md#type-digitalkeyconnectionstate-t) | 해당 peer 연결 관측/문맥. BLE 연결 여부로 사용자 요청 수용을 대신하지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `proximity` | [`ProximityInput_t`](11_VEHICLE_TYPES.md#type-proximityinput-t) | ESP32 근접 판정과 독립 평가 근거. STALE/INVALID/미수신을 FAR로 바꾸지 않는다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-digitalkeystate-t"></a>
## `DigitalKeyState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DigitalKeySetting_t setting;
    FunctionAvailability_t availability;
    UnlockOrigin_t lastUnlockOrigin; /* Domain internal observation */
    ResultReason_t reason;
    SignalMeta_t meta;
} DigitalKeyState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `setting` | [`DigitalKeySetting_t`](11_VEHICLE_TYPES.md#type-digitalkeysetting-t) | Domain의 디지털 키 설정 ENABLE/DISABLE. 링크 연결과 별개. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `availability` | [`FunctionAvailability_t`](11_VEHICLE_TYPES.md#type-functionavailability-t) | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `lastUnlockOrigin` | [`UnlockOrigin_t`](11_VEHICLE_TYPES.md#type-unlockorigin-t) | 최근 잠금 해제 출처. 실제 unlock 전이 근거에 의해서만 변경. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `reason` | [`ResultReason_t`](11_VEHICLE_TYPES.md#type-resultreason-t) | 결과·품질·가용성을 설명하는 typed 사유. 예약 값 거부, raw 숫자 변환은 명시 표를 따른다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<a id="type-rearmeasurementstate-t"></a>
## `RearMeasurementState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `REAR_MEASUREMENT_VALID_DISTANCE` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_MEASUREMENT_NO_OBJECT` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_MEASUREMENT_UNAVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_MEASUREMENT_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_MEASUREMENT_RECOVERING` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_MEASUREMENT_INACTIVE` | 5 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-cisstate-t"></a>
## `CisState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    CIS_STATE_STARTUP = 0,
    CIS_STATE_READY,
    CIS_STATE_ACTIVE,
    CIS_STATE_FAULT
} CisState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `CIS_STATE_STARTUP` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_STATE_READY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_STATE_ACTIVE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_STATE_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-cisfunctionstatus-t"></a>
## `CisFunctionStatus_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `CIS_FUNCTION_STATUS_NOT_READY` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_FUNCTION_STATUS_READY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_FUNCTION_STATUS_ACTIVE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_FUNCTION_STATUS_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `CIS_FUNCTION_STATUS_RECOVERING` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-cisoccupantstate-t"></a>
## `CisOccupantState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `occupantPresent` | `bool` | Pi 원 탑승자 존재 판정 bool. 유효성 실패는 false 인 정상 관측이 아니다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `occupantCount` | `uint8_t` | Pi 원 인원수, 0~5 정상 후보; 미확인 raw=255는 countQuality 무효 의미와 함께 변환. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `presenceQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | Pi 존재 판정의 유효성/사유/품질/원 AGE/원 순서. countQuality와 독립. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `countQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | Pi 인원수의 독립 유효성/품질/AGE/순서. CIS 재평가 표현의 정확한 합성 정책 미확정. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `visionFunctionStatus` | [`CisFunctionStatus_t`](11_VEHICLE_TYPES.md#type-cisfunctionstatus-t) | 비전 기능 준비·고장 상태. 한 번 관측 성공했다고 지속 READY 합성 금지. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |

<a id="type-ciscabinenvironmentstate-t"></a>
## `CisCabinEnvironmentState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `cabinTemperature` | `int16_t` | CIS 실내 온도 i16, centi-°C. 값 사용은 temperatureQuality 유효성에 따른다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `temperatureQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | CIS 온도의 독립 유효성/사유/원 AGE/원 평가 순서. 습도·조도 품질과 별개. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `cabinHumidity` | `uint16_t` | CIS 실내 상대습도 u16, centi-%RH; 정상 0~10000 후보, humidityQuality와 함께 소비. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `humidityQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | CIS 습도의 독립 유효성/사유/원 AGE/원 평가 순서. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `cabinIlluminance` | `uint32_t` | CIS 실내 조도 u32, lux. illuminanceQuality 독립. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `illuminanceQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | CIS 실내 조도의 독립 유효성/사유/원 AGE/원 평가 순서. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |

<a id="type-cisrearstate-t"></a>
## `CisRearState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    uint16_t rearDistanceMm; /* exact 0.1 cm input; no integer-cm truncation */
    RearMeasurementState_t measurementState;
    ValueQuality_t distanceQuality;
    SourceGeneration_t sourceGeneration; /* admitted original CIS lineage */
} CisRearState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `rearDistanceMm` | `uint16_t` | CIS 초음파 후방 거리 u16, mm. 미측정/범위 초과를 실제 0mm로 합성 금지. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `measurementState` | [`RearMeasurementState_t`](11_VEHICLE_TYPES.md#type-rearmeasurementstate-t) | 후방 거리 센서의 측정 상태. 유효성·거리 값과 별도로 해석. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `distanceQuality` | [`ValueQuality_t`](11_VEHICLE_TYPES.md#type-valuequality-t) | 거리의 원본 유효성/AGE/갱신 번호. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |

<a id="type-cisenvironmentstate-t"></a>
## `CisEnvironmentState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    CisOccupantState_t occupant;
    CisCabinEnvironmentState_t cabin;
    CisRearState_t rear;
} CisEnvironmentState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `occupant` | [`CisOccupantState_t`](11_VEHICLE_TYPES.md#type-cisoccupantstate-t) | 탑승자 의미 묶음; 환경·후방과 별도 품질/갱신. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `cabin` | [`CisCabinEnvironmentState_t`](11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t) | 온도·습도·조도 묶음, 센서별 독립 품질. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |
| `rear` | [`CisRearState_t`](11_VEHICLE_TYPES.md#type-cisrearstate-t) | 초음파 후방 관측 묶음. | CIS 센서/유효성 owner; 탑승자 원 판정은 Pi |

<a id="type-windowcommand-t"></a>
## `WindowCommand_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `WINDOW_COMMAND_OPEN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_COMMAND_CLOSE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_COMMAND_STOP` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_COMMAND_VENT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_COMMAND_MOVE_TO_POSITION` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-windowmotionstate-t"></a>
## `WindowMotionState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `WINDOW_MOTION_UNKNOWN` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_MOTION_STOPPED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_MOTION_OPENING` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_MOTION_CLOSING` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `WINDOW_MOTION_ANTIPINCH_REVERSING` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-antipinchstate-t"></a>
## `AntiPinchState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    ANTIPINCH_STATE_CLEAR = 0,
    ANTIPINCH_STATE_ACTIVE
} AntiPinchState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `ANTIPINCH_STATE_CLEAR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `ANTIPINCH_STATE_ACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-windowstate-t"></a>
## `WindowState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `channel` | `uint8_t` | WINDOW 채널 u8. 지원 채널 범위는 구현 전 확정 필요; 현재 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `motionState` | [`WindowMotionState_t`](11_VEHICLE_TYPES.md#type-windowmotionstate-t) | WINDOW 실제 이동 상태. 상위 Command 목표와 다르며 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `ecuState` | [`CommonEcuState_t`](11_VEHICLE_TYPES.md#type-commonecustate-t) | 원 ECU 기능 상태. 링크 사용 가능성과 구별. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `positionPercent` | `uint8_t` | WINDOW 위치 0~100%, 원 품질/AGE와 함께 사용; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `fullyOpen` | `bool` | WINDOW 실제 완전 열림 끝단 bool, 관측 근거 필요; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `fullyClosed` | `bool` | WINDOW 실제 완전 닫힘 끝단 bool, 관측 근거 필요; 구현 보류. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `antiPinchState` | [`AntiPinchState_t`](11_VEHICLE_TYPES.md#type-antipinchstate-t) | WINDOW 끼임 의미 상태. 로컬 보호가 우선하며 무효 입력을 CLEAR로 치환 금지. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |
| `sourceGeneration` | [`SourceGeneration_t`](11_VEHICLE_TYPES.md#type-sourcegeneration-t) | 원 관측/사건 생산 세대. 원 warning/fault 계보에 사용하고 마지막 송신 홉 Boot로 재라벨하지 않는다. | WINDOW 로컬 기능 owner [구현 보류 — 설계 유지] |

<a id="type-windowcommandrequest-t"></a>
## `WindowCommandRequest_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    RequestContext_t context;
    uint8_t channel;
    WindowCommand_t command;
    uint8_t targetPositionPercent;
    DataQuality_t validity;
} WindowCommandRequest_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `context` | [`RequestContext_t`](11_VEHICLE_TYPES.md#type-requestcontext-t) | 해당 객체의 원 요청/명령 문맥. 하위 타입 전체 정의와 유효성 조건을 함께 적용한다. | Domain 기능 결정/설정/결과 owner |
| `channel` | `uint8_t` | WINDOW 채널 u8. 지원 채널 범위는 구현 전 확정 필요; 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `command` | [`WindowCommand_t`](11_VEHICLE_TYPES.md#type-windowcommand-t) | WINDOW 목표 명령 OPEN/CLOSE/STOP/VENT. VENT70↔80% 충돌 미확정; 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `targetPositionPercent` | `uint8_t` | WINDOW 목표 위치 0~100%. VENT70↔80% 충돌은 FUNCTION_POLICY_OPEN; 현재 구현 보류. | Domain 기능 결정/설정/결과 owner |
| `validity` | [`DataQuality_t`](11_VEHICLE_TYPES.md#type-dataquality-t) | 해당 값의 유효성을 나타내는 typed 상태. 품질 손실을 정상 bool/0으로 표현하지 않는다. | Domain 기능 결정/설정/결과 owner |

<a id="type-vssstate-t"></a>
## `VssState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    VSS_STATE_STARTUP = 0,
    VSS_STATE_READY,
    VSS_STATE_PLAYING,
    VSS_STATE_FAULT
} VssState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `VSS_STATE_STARTUP` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_STATE_READY` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_STATE_PLAYING` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_STATE_FAULT` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-vssavailability-t"></a>
## `VssAvailability_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    VSS_AVAILABILITY_FULL = 0,
    VSS_AVAILABILITY_DEGRADED,
    VSS_AVAILABILITY_UNAVAILABLE
} VssAvailability_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `VSS_AVAILABILITY_FULL` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_AVAILABILITY_DEGRADED` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_AVAILABILITY_UNAVAILABLE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-rearobstaclestate-t"></a>
## `RearObstacleState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    REAR_OBSTACLE_STATE_CLEAR = 0,
    REAR_OBSTACLE_STATE_CAUTION,
    REAR_OBSTACLE_STATE_EMERGENCY
} RearObstacleState_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `REAR_OBSTACLE_STATE_CLEAR` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_OBSTACLE_STATE_CAUTION` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_OBSTACLE_STATE_EMERGENCY` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-reardetectionactivation-t"></a>
## `RearDetectionActivation_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef enum
{
    REAR_DETECTION_DISABLED = 0,
    REAR_DETECTION_ACTIVE
} RearDetectionActivation_t;
```

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `REAR_DETECTION_DISABLED` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `REAR_DETECTION_ACTIVE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-vssoneshotevent-t"></a>
## `VssOneShotEvent_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| C 열거자 | 실제 C 정수 | 유효 조건 |
|---|---:|---|
| `VSS_EVENT_VEHICLE_WELCOME` | 0 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_EVENT_VEHICLE_GOODBYE` | 1 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_EVENT_DOOR_LOCK_COMPLETE` | 2 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_EVENT_DOOR_UNLOCK_COMPLETE` | 3 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |
| `VSS_EVENT_DOOR_LOCK_ERROR` | 4 | 이 enum 의미로만 소비. raw code는 명시 mapping 후 사용. |

<a id="type-vssservicestate-t"></a>
## `VssServiceState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

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

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `state` | [`VssState_t`](11_VEHICLE_TYPES.md#type-vssstate-t) | 해당 타입의 의미 상태. 타입별 enum의 UNKNOWN/UNCONFIRMED/STALE를 정상 CLEAR/FAR로 만들지 않는다. | VSS 음향 서비스 owner |
| `availability` | [`VssAvailability_t`](11_VEHICLE_TYPES.md#type-vssavailability-t) | 해당 기능의 사용 가능성. Network 링크 availability와 별개. | VSS 음향 서비스 owner |
| `acceptingEvents` | `bool` | VSS 이벤트 수용 가능 여부. 실제 재생 완료나 오디오 정상 여부와 별개. | VSS 음향 서비스 owner |
| `faultActive` | `bool` | VSS 활성 고장 존재 여부. 발생 ID·last fault와 구분. | VSS 음향 서비스 owner |
| `meta` | [`SignalMeta_t`](11_VEHICLE_TYPES.md#type-signalmeta-t) | 원 생산자의 품질·원본 AGE·평가 번호. 이 메타는 링크 BOOT_ID/TX_SEQUENCE가 아니다. | VSS 음향 서비스 owner |

<a id="type-vehiclestate-t"></a>
## `VehicleState_t`

**원 소속:** `Vehicle_Types_v0.3.h` · **지위:** DECLARED_SOURCE · **소비:** [노드 API](20_API.md).

```c
typedef struct
{
    DoorState_t door;
    ClimateState_t climate;
    InteriorLightState_t interiorLight;
    CisEnvironmentState_t environment;
    WindowState_t window;
    VssServiceState_t vss;
    DigitalKeyState_t digitalKey;
} VehicleState_t;
```

| 멤버 | 정확한 C 타입 | 역할·범위·유효성 | 원 생성자 |
|---|---|---|---|
| `door` | [`DoorState_t`](11_VEHICLE_TYPES.md#type-doorstate-t) | BCM 도어 상태 묶음. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `climate` | [`ClimateState_t`](11_VEHICLE_TYPES.md#type-climatestate-t) | BCM 공조 상태 묶음. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `interiorLight` | [`InteriorLightState_t`](11_VEHICLE_TYPES.md#type-interiorlightstate-t) | BCM 실내 조명 상태 묶음(EXTERIOR_LIGHT 제외). | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `environment` | [`CisEnvironmentState_t`](11_VEHICLE_TYPES.md#type-cisenvironmentstate-t) | CIS 환경 의미 묶음. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `window` | [`WindowState_t`](11_VEHICLE_TYPES.md#type-windowstate-t) | WINDOW 상태 묶음; 구현 보류. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `vss` | [`VssServiceState_t`](11_VEHICLE_TYPES.md#type-vssservicestate-t) | 별도 S32K344 VSS 음향 상태. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |
| `digitalKey` | [`DigitalKeyState_t`](11_VEHICLE_TYPES.md#type-digitalkeystate-t) | Domain 디지털 키 의미 상태. | 해당 의미 객체의 원 생산자; Domain은 소비/집계 |

<!-- 기존 문서 anchor 호환; 본문은 위 전체 선언과 설명으로 승계 -->
<a id="domain--vehicle_types_v02h-전체-의미-타입"></a>
