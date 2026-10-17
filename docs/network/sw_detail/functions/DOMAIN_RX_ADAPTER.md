# DOMAIN Rx Adapter — 공개 Rx22와 Observer11 연결

[B15 개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [M03](../modules/M03_TYPED_HANDOFF.md) · [수직 흐름](../flows/NETWORK_TO_DOMAIN.md) · [Query 원본 연결](DOMAIN_QUERY_PROVENANCE.md)

이 문서는 Network가 기존 C2 수신 함수를 호출하기 전후의 변환·수명·결과 경계를 명세한다. 공개 Rx22 본체와 DOMAIN snapshot/Manager는 DOMAIN 소유다. `DECLARED_SOURCE`는 실제 헤더·등록·ABI·실행 채택 증거가 아니다. G6 당시 Interface39는 역사적 봉인 기준선으로 보존하고, 현재 별도 후보의 문서 정본 참조는 고정 r1 Interface40개로 구분한다. C2 v0.6/Vehicle v0.3은 미승인 후보다. 새 Wire/API/Manager를 만들지 않는다.

<a id="common"></a>
## 공통 호출 계약

CAN은 기존 M05/F14–F15 codec과 원본 admission, UART-M은 M06/F16–F18·Router를 거친 동일 수신 사건을 M03/F08에서 인계한다. Wire Rx가 아닌 NetworkLocalFailure는 M03/F09의 별도 후발 실패 역할이다. DOMAIN의 ISR 밖 단일 담당 실행 문맥에서만 C2를 호출하며 ISR에서 직접 호출하지 않는다. Driver callback은 안정된 값/길이/원 수신시각을 넘기는 범위에 머문다. 실제 Task/Lock/Queue/초기화·재진입 바인딩은 D-05/G06 OPEN이다.

각 `const T *`는 NULL 불가 IN, 호출 동안 안정된 borrowed 객체다. `deviceContextId`는 현재 검증된 단말 값이다. typed 값과 raw/meta가 같은 수신 사건인지 확인한 뒤 호출하고, 반환 후 사용하려는 소비자가 반환 전에 복사한다. 실패 OUT은 없으며 입력 객체를 수정하지 않는다. C2 enum은 이름별 대응으로 변환하고 raw 숫자 cast, `sizeof` 기반 Wire 복사, 값 없음의 정상0 대체를 금지한다. sourceGeneration0·BOOT_ID 대응은 회신 대기다. 온도·습도 원 TTL2500ms(기한 도달 비사용)는 합의 기준이며 조도 TTL과 실제 Binding은 별개다.

공개 타입의 필드·enum 정본은 [Vehicle types](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md), [DOMAIN types](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md), [Data 색인](../../interface/02_NODES/DOMAIN/10_DATA.md)이다. 아래 API별 링크는 기존 변환/의사코드 정본이며 본문을 복제해 두 번째 구현을 만들지 않는다.

| C2 반환 | Network가 보존할 의미와 후속 처리 |
|---|---|
| DOMAIN_IF_OK | 해당 로컬 수용 성공. 원 입력의 실제 snapshot commit·원자적 다중 Rx 처리·차량 DONE·원 CAN association은 별도 증거가 필요하다. |
| DOMAIN_IF_INVALID_ARGUMENT | NULL/enum/필드 조합 오류. 같은 값의 무조건 재시도 없이 원 입력과 실패 경계 기록. |
| DOMAIN_IF_NOT_INITIALIZED | DOMAIN 초기화 미확인. 새 Init 호출이나 재시도를 ISR에서 합성하지 않는다. |
| DOMAIN_IF_NOT_AVAILABLE | 필수 binding/현재 문맥/상태 연결 미입증. 영향 경로만 제한한다. |
| DOMAIN_IF_QUEUE_FULL | 소비 bounded capacity 부족. Event/Result를 덮어쓰거나 전체 사건을 무한 재호출하지 않는다. |
| DOMAIN_IF_REJECTED | DOMAIN 의미상 거부를 보존한다. Network transport 오류나 차량 FAILED로 변환하지 않는다. |
| DOMAIN_IF_UNSUPPORTED | 기능/형식 지원 미확인. WINDOW HOLD를 자동 해제하지 않는다. |
| 그 외 값 | 원 숫자 및 호출 경계를 진단에 보존하고 성공을 주장하지 않는다. 공개 enum을 추가하지 않는다. |

Core commit은 optional Observer보다 먼저다. 따라서 최종 C2 반환만으로 commit 유무를 추측하지 않는다. Observer 실패가 확인된 입력을 다시 C2에 넣으면 중복 반영/구동 위험이 있다. 실제 commit/observer 단계 결과를 Owner가 제공하지 않으면 UNKNOWN으로 보존하고 자동 재호출을 허용하지 않는다. 결과·참조 해제·복구의 공통 처리는 [수명 정본](NETWORK_SERVICE_LIFETIME.md), 필수 후발 실패는 [Failure/Health](NETWORK_FAILURE_HEALTH.md)에서만 정의한다.

<a id="rx22"></a>
## 공개 Rx22 — 정확한 선언과 개별 연결

각 행은 독립 Wire 개수와 다르다. M_CONTEXT는 Rx2개, CIS_STATUS/VSS_STATUS는 상태/고장 파생 호출이 있고 NetworkLocalFailure는 Wire 수신 API가 아니다. 공개 선언은 [C2 API 정본](../../interface/02_NODES/DOMAIN/20_API.md)에 있다.

<a id="api-DomainIf_RxDigitalKeyConnection"></a>
### DomainIf_RxDigitalKeyConnection

```c
DomainIf_Status_t DomainIf_RxDigitalKeyConnection( DeviceContextId_t deviceContextId, const DigitalKeyConnectionState_t *connection);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. M_CONTEXT → [`DigitalKeyConnectionState_t` 변환 정본](uart_m/M_CONTEXT.md#admission) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxdigitalkeyconnection)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** context.device_context_id → deviceContextId, context.session_id → sessionId. registration/btConnection은 B31..37의 공동 quality/update/AGE를, appActive/appMeta는 별도 B15..21을 사용한다. raw0/1/255를 이름별 enum으로 바꾸며 BLE connected를 App ACTIVE로 합성하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onMobileRequest 아님; 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** CF01·IF-O08·IF-O23·C2-H01; 두 Rx의 부분 반영·직렬화와 근접 quality 바인딩 미확인 시 호출 전 제한. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxProximity"></a>
### DomainIf_RxProximity

```c
DomainIf_Status_t DomainIf_RxProximity( DeviceContextId_t deviceContextId, const ProximityInput_t *proximity);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. M_CONTEXT → [`ProximityInput_t` 변환 정본](uart_m/M_CONTEXT.md#admission) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxproximity)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** B22 state0/1/255 → FAR/NEAR/UNKNOWN; B23 validity와 B24 reason은 이름별 변환. B25 update/B29 AGE는 등록·연결 및 App과 독립이다. validity/reason→SignalMeta.quality 대응은 Owner 확인 전이며 INVALID를 FAR로 바꾸지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** CF01·IF-O08·C2-H01; 첫 Connection 반영 후 실패해도 두 호출 전체를 반복하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxMobileRequest"></a>
### DomainIf_RxMobileRequest

```c
DomainIf_Status_t DomainIf_RxMobileRequest( const DomainIf_MobileRequest_t *request);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. M_REQUEST → [`DomainIf_MobileRequest_t` 변환 정본](uart_m/M_REQUEST.md#deliver) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxmobilerequest)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 현재 Device/Session 및 원 RequestID를 유지하고 kind/op의 8가지 허용 조합만 C2 tagged union에 대응한다. RGB(0,128,255), 온도 centi-°C, levelPercent를 보존한다. 24B에 없는 원 SENT/SignalMeta를 received_at_ms 또는 AGE0으로 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onMobileRequest. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O03·IF-O08·IF-O23·IF-N01·C2-H01; 원 SENT3s 정책의 효력·측정 근거는 OPEN. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxWarningAck"></a>
### DomainIf_RxWarningAck

```c
DomainIf_Status_t DomainIf_RxWarningAck( const DomainIf_WarningAck_t *ack);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. M_WARNING_ACK → [`DomainIf_WarningAck_t` 변환 정본](uart_m/M_WARNING_ACK.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxwarningack)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** raw warning1..8을 REAR_OBSTACLE/OCCUPANT_HAZARD/WINDOW_ANTIPINCH/BCM_FAULT/CIS_FAULT/WINDOW_FAULT/VSS_FAULT/DOOR_LEFT_OPEN으로 명시 변환한다. 현재 Device/Session 및 원 DOMAIN Boot·kind·occurrence를 검증한다. C2 ack에는 Boot가 없으므로 private 수신 근거를 보존한다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onWarningAck. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O08·IF-O23·C2-H01; 읽음 ACK를 위험 CLEAR·차량 DONE·음향 STOP으로 확장하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxBcmDoorState"></a>
### DomainIf_RxBcmDoorState

```c
DomainIf_Status_t DomainIf_RxBcmDoorState( const DoorState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_DOOR_STATE → [`DoorState_t` 변환 정본](can/BCM_DOOR_STATE.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcmdoorstate)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 실측 lock/open 및 composite 품질·AGE·update를 각각 유지한다. 물리 상태를 최근 명령 target으로 대체하지 않으며 raw UNKNOWN/NO_DATA를 false/정상0으로 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24; source generation/BOOT 및 동일 snapshot association OPEN. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxBcmClimateState"></a>
### DomainIf_RxBcmClimateState

```c
DomainIf_Status_t DomainIf_RxBcmClimateState( const ClimateState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_CLIMATE_STATE → [`ClimateState_t` 변환 정본](can/BCM_CLIMATE_STATE.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcmclimatestate)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** commandedFanLevel/measuredFanLevel, thermalDirection, thermalOutputLevelPercent, heatRemovalState와 각각의 다섯 품질을 구분한다. measured255→FAN_LEVEL_UNKNOWN, heatRemoval255→UNMEASURABLE을 명시 대응하고 원 update·AGE를 유지한다. 이 상태 DTO에 없는 목표 온도/auto 설정을 새 필드로 만들지 않으며 선택 v2/Vehicle v0.3 후보의 sourceGeneration을 포함하여 전체12멤버를 보존한다. 검증된 동일 원 생산 세대 근거만 대입하고 CAN BOOT를 자동 복사하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24·C2-H01; v0.3 후보 ABI와 sourceGeneration 정책은 회신 대기. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxBcmInteriorLightState"></a>
### DomainIf_RxBcmInteriorLightState

```c
DomainIf_Status_t DomainIf_RxBcmInteriorLightState( const InteriorLightState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_LIGHT_STATE → [`InteriorLightState_t` 변환 정본](can/BCM_LIGHT_STATE.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcminteriorlightstate)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** type/levelPercent/color(RGB), applyState와 physicalFeedbackSupported 및 type/level/color/apply 네 품질을 보존한다. APPLIED를 물리 광출력 확인으로 바꾸지 않고 RGB 각0..255를 정상0으로 축소하지 않는다. enabled는 추가하지 않는다. 선택 후보의 sourceGeneration을 포함한 전체11멤버를 유지하고 품질 저하로 알고 있던 세대를0으로 지우지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24·C2-H01; 실제 feedback·동일 원본 결합은 Owner 근거 필요. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxCisStatus"></a>
### DomainIf_RxCisStatus

```c
DomainIf_Status_t DomainIf_RxCisStatus( const DomainIf_CisStatusReport_t *status);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. CIS_STATUS → [`DomainIf_CisStatusReport_t` 변환 정본](can/CIS_STATUS.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisstatus)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** raw interface READY(0), UNAVAILABLE(1), RECOVERING(2)는 각각 CIS_INTERFACE_AVAILABLE(0), CIS_INTERFACE_UNAVAILABLE(2), CIS_INTERFACE_RECOVERING(3)으로 명시 변환한다. 현재 C2 정본에는 값1 또는 DEGRADED 열거자가 없으므로 숫자 cast를 금지한다. Pi READY가 전체 CIS 센서 정상이나 새 Vision 관측을 뜻하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onCisStatus. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O13·IF-O24; 부분 축소된 quality/fault 표현 미확인 상태는 해당 투영 제한. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxCisOccupant"></a>
### DomainIf_RxCisOccupant

```c
DomainIf_Status_t DomainIf_RxCisOccupant( const CisOccupantState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. CIS_OCCUPANT → [`CisOccupantState_t` 변환 정본](can/CIS_OCCUPANT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisoccupant)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** Pi Boot/VisionSequence·vision AGE와 presence/count의 독립 validity/reason을 raw 근거에 남긴다. count255를 정상0으로 만들지 않는다. bool presence가 UNKNOWN255를 표현하지 못하는 경우 미사용 값/품질 결합 정책 확인 전 소비를 제한한다. 새 관측뿐 아니라 [검증된 CURRENT_RESTRICTION](can/CIS_OCCUPANT.md#restriction-check)도 기존 C2로 전달하며 원 Pi Boot·seq·원값·AGE 기준은 보존한다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O13·IF-O09·IF-O24; 원 Pi 사유/현재 제한 구별은 후보 반영; 실제 사건·Core/Getter 연결은 BINDING_NOT_VERIFIED. 부분 반영/UNKNOWN의 자동 재호출이나 VALID rollback은 금지한다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxCisCabinEnvironment"></a>
### DomainIf_RxCisCabinEnvironment

```c
DomainIf_Status_t DomainIf_RxCisCabinEnvironment( const CisCabinEnvironmentState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. CIS_ENVIRONMENT → [`CisCabinEnvironmentState_t` 변환 정본](can/CIS_ENVIRONMENT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxciscabinenvironment)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 온도 i16 centi-°C(-32768 sentinel), 습도 u16 centi-%RH(65535 sentinel), 조도 u32 lx(0xFFFFFFFF sentinel)를 유지한다. 세 센서의 update/AGE/validity/reason은 독립이며 공통 정상 meta로 덮지 않는다. 정상 CAN 관측과 [M09 로컬 무효화 사건](can/CIS_ENVIRONMENT.md#local-invalidation) 중 일반 ORIGIN_EXPIRED/RX_STOPPED는 서로 다른 출처로 같은 기존 C2를 호출한다. 후자는 원 frame/meta를 위조하지 않고 채택된 현재 NO_DATA/STALE을 전달한다. BOOT_CHANGED는 DOMAIN 소유 단일 동기 private 전환 목표로 분리하며 BOOT용 Rx NO_DATA를 중복 호출하지 않는다. 이 공개 Rx는 확인된 전환 완료 뒤 새 원 관측의 값 수신 경계로 유지한다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24; 온습도 각2500ms는 합의 완료, 조도 TTL은 별도 미합의다. 환경 generation 필드를 추가하지 않는다. [r1 조건부 순서](can/CIS_ENVIRONMENT.md#environment-storage-order)는 Network 자기 환경 대기·콜백/미시작 Query 차단 → 단일 동기 private 목표 → DOMAIN 구 작업 후속 commit 봉쇄·VSM/Meta·순서 문맥/구 Query 무효화 → 현재 요청/수용 BOOT/적용 Epoch/명확한 완료 대조 → B 원 낮은seq의 공개 Rx다. 구 작업 객체가 남아도 확실한 후속 commit 차단과 나머지 완료 조건이 성립하면 가능하며 Network가 DOMAIN 내부 Queue를 지우지 않는다. 담당자 부분 Core/Host 완료 보고와 별개로 실제 private 선언·연속 Epoch 수용·Rx/Query Binding이 미확인이므로 [BG-R05-BOOT](can/CIS_ENVIRONMENT.md#boot-binding-gap)를 유지한다. Queue/일반 C2 OK로 B를 열지 않으며 실패/불명에서 구 GOOD 제거를 주장하지 않는다. 명확한 Core 완료 뒤 Observer 오류로 재실행하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxCisRear"></a>
### DomainIf_RxCisRear

```c
DomainIf_Status_t DomainIf_RxCisRear( const CisRearState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. CIS_REAR → [`CisRearState_t` 변환 정본](DOMAIN_CIS_REAR_INGRESS.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisrear)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** NetCisRear.rear_distance_deci_cm → CisRearState_t.rearDistanceMm. 0.1cm/raw=1mm/raw이므로 raw123은 123mm 그대로이며 정수 cm 변환이 없다. raw65535는 측정 거리로 쓰지 않는다. proximity_status → measurementState, validity/quality_reason 및 원 update/누적 AGE → distanceQuality를 연결하며 NO_OBJECT와 FAULT/UNAVAILABLE/RECOVERING/INACTIVE를 분리한다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24·C2-H01; sourceGeneration과 실제 Core commit/원 CAN association은 별도 확인. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxCisFault"></a>
### DomainIf_RxCisFault

```c
DomainIf_Status_t DomainIf_RxCisFault( const FaultInfo_t *fault);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. [CIS_STATUS 원 보고 변환 정본](can/CIS_STATUS.md)은 `DomainIf_CisStatusReport_t`와 `DomainIf_RxCisStatus`를 다룬다. 같은 원 보고의 고장 부분을 `FaultInfo_t`로 투영하는 별도 조건부 경계의 정본은 이 절이다. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxcisfault)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 기존 CIS_STATUS의 원 고장 근거에서 `FaultInfo_t.category`, `faultCode`, `state`, `quality` 각각의 원 의미와 명시 대응이 확인된 경우에만 이 경계를 사용할 수 있다. 원 occurrence/AGE/세대 중 공개 FaultInfo로 옮길 수 없는 식별은 같은 사건의 raw association에 보존한다. 현재 필드 대응·필수 Core 지원·status/fault 부분 성공 처리 계약은 미확인이므로 **고장 경계 호출 전 HOLD**한다. 코드·품질·AGE0이나 새 CIS_FAULT Wire를 만들지 않는다. `NetCan_DeliverCisStatus`의 정상 status 인계가 이 함수의 자동 호출 또는 고장 처리 완료를 뜻하지 않는다.

**반환 이후:** 위 전제와 실제 호출 근거가 확보된 경우에만 원 C2 7값, 해당 status/fault 각각의 Core 반영, 선택적 onCisFault 결과를 [공통 결과](#common)와 [로컬 결과 분류](#outcome)에 분리해 기록한다. 이미 성공한 Core를 fault/Observer 실패 때문에 전체 재호출하지 않으며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O13·IF-O15·IF-O24·G06의 고장 투영·동시성·Core 바인딩 미결을 유지한다. 고장 필드 하나라도 근거가 없거나 부분 성공 처리 근거가 없으면 호출 전 HOLD하며, 정상 status 수신만으로 두 경계의 완료를 선언하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxWindowState"></a>
### DomainIf_RxWindowState

```c
DomainIf_Status_t DomainIf_RxWindowState( const WindowState_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. WINDOW_STATE → [`WindowState_t` 변환 정본](can/WINDOW_STATE.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxwindowstate)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** raw motion255→C2 UNKNOWN0, raw0..3은 대응 이름으로 변환한다. channel 및 tri-state bool, pinch 4상태를 손실 없이 표현할 수 있는 Owner 규칙이 없으면 전달하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O07·G05·IF-O24; WINDOW 실행·표시·timeout 모두 HOLD. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxWindowCommandResult"></a>
### DomainIf_RxWindowCommandResult

```c
DomainIf_Status_t DomainIf_RxWindowCommandResult( const DomainIf_WindowCommandResult_t *result);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. WINDOW_RESULT → [`DomainIf_WindowCommandResult_t` 변환 정본](can/WINDOW_RESULT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxwindowcommandresult)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 원 Command/대상 Boot/종결 결과를 분리한다. requestSequence→commandId 연결을 추정하지 않는다. Wire에 없는 Result AGE/UPDATE를 수신시각·LINK_SEQUENCE로 만들어 넣지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onWindowCommandResult. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O07·G05·D-05; WINDOW HOLD 및 결과 correlation/상태 변환 OPEN. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxWindowAntiPinch"></a>
### DomainIf_RxWindowAntiPinch

```c
DomainIf_Status_t DomainIf_RxWindowAntiPinch( const DomainIf_AntiPinchReport_t *report);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. WINDOW_EVENT 중 anti-pinch 후보 → [`DomainIf_AntiPinchReport_t` 변환 정본](can/WINDOW_EVENT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxwindowantipinch)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** anti-pinch 사건과 END를 구분한다. raw에 없는 outputDisabled/movementStopped/protectionState/reason을 정상 false/0으로 채우지 않는다. 원 occurrence 및 AGE를 유지한다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onWindowAntiPinch. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O07·G05; 없는 필드의 원 생산자 근거 전 후보 투영 HOLD. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxWindowFault"></a>
### DomainIf_RxWindowFault

```c
DomainIf_Status_t DomainIf_RxWindowFault( const FaultInfo_t *fault);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. WINDOW_FAULT → [`FaultInfo_t` 변환 정본](can/WINDOW_FAULT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxwindowfault)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 표현 가능한 고장과 발생 근거만 전달하고 정상화·새 fault 생성 없이 원 세대/품질/AGE를 보존한다. 비활성 WINDOW 무수신으로 상시 fault를 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onWindowFault. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O07·G05·IF-O24; WINDOW 정상 경로 활성화 금지. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxVssState"></a>
### DomainIf_RxVssState

```c
DomainIf_Status_t DomainIf_RxVssState( const DomainIf_VssStateReport_t *state);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. VSS_STATUS → [`DomainIf_VssStateReport_t` 변환 정본](can/VSS_STATUS.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxvssstate)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** A2의 기존 INPUT 단일 완전 보고 producer 뒤 DOMAIN Network가 service/current/history를 명시 투영한다. [per-K 정본](can/VSS_STATUS.md#a2-domain-contract)의 State/Fault 독립 결과를 따른다. 실제 설치 profile·원 품질·세대·Getter association E-S05가 미확인인 해당 투영은 제한한다. WARNING60 형식 선택은 Status 실제 Binding을 해제하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. 전용 Observer 없음. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O06·D-05·IF-O24/E-S05의 실제 설치·수용·old K fence·후발 관측 대기. 같은 K 반복은 Rx 호출0이며 State 수용 뒤 Fault 실패로 State를 재실행하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md#vss-a2-receive).

<a id="api-DomainIf_RxVssFault"></a>
### DomainIf_RxVssFault

```c
DomainIf_Status_t DomainIf_RxVssFault( const FaultInfo_t *fault);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. VSS_STATUS의 고장 근거 → [`FaultInfo_t` 변환 정본](can/VSS_STATUS.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxvssfault)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 같은 완전 VSS_STATUS current mask의 가장 작은 정의 code1..5를 현재 대표로 선택한다. 공집합은 code0/state INACTIVE이며 lastFaultCode는 현재 고장으로 쓰지 않는다. 원 전체 mask/occurrence/AGE/generation은 State/M09에 남긴다. 새 VSS_FAULT Wire는 없다.

**반환 이후:** 실제 설치 profile의 원 raw·callback/consumer 결과를 보존한다. 참고 v0.5 RxVssFault는 observer-only이므로 선행 Core commit은 해당 없음이다. 이를 [Core→Observer classifier](#outcome)에 넣기 위해 APPLIED를 합성하지 않고 기존 M09에 직접 원 결과를 남긴다. 후보7값과 v0.5 6값을 혼용하지 않는다. 이미 수용한 State는 Fault 실패/불명으로 재호출하지 않는다.

**미결·반례:** E-S05 실제 설치·Observer 등록·같은 K 수용·후발 실패 상관·old K fence는 대기다. CALLING/UNKNOWN_FAILED는 자동 재호출0, 미호출/실증된 무부작용 미접수만 [A2 정본](can/VSS_STATUS.md#a2-domain-contract)의 bounded 후속 조건으로 재검토한다.

<a id="api-DomainIf_RxBcmStatus"></a>
### DomainIf_RxBcmStatus

```c
DomainIf_Status_t DomainIf_RxBcmStatus( const DomainIf_BcmStatusReport_t *status);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_STATUS → [`DomainIf_BcmStatusReport_t` 변환 정본](can/BCM_STATUS.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcmstatus)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** service/quality/active fault의 명시 매핑을 적용한다. activeFaultCode0이면 나머지 activeFault 관련 raw 필드는 무시 대상이며 이를 C2 SENSOR0 고장으로 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onBcmStatus. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24; Boot·generation lifecycle 및 FaultInfo 표현 OPEN. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxBcmCommandResult"></a>
### DomainIf_RxBcmCommandResult

```c
DomainIf_Status_t DomainIf_RxBcmCommandResult( const DomainIf_BcmCommandResult_t *result);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_RESULT → [`DomainIf_BcmCommandResult_t` 변환 정본](can/BCM_RESULT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcmcommandresult)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** COMMAND_DOMAIN_BOOT/commandId/kind로 원 명령을 대조하고 result/reason을 이름별 매핑한다. 도어 terminal evidence는 같은 결과의 atomic 묶음으로 유지한다. Wire에 없는 confirmation은 동일 명령의 실제 증거가 없으면 UNCONFIRMED 후보를 유지하며 DONE을 물리 확인으로 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onBcmCommandResult. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** D-04·G01·IF-O24; 일반 Result에 없는 AGE/UPDATE를 도어 evidence AGE로 대신하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxBcmProtectionEvent"></a>
### DomainIf_RxBcmProtectionEvent

```c
DomainIf_Status_t DomainIf_RxBcmProtectionEvent( const DomainIf_BcmProtectionEvent_t *event);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F08 호출자. BCM_EVENT → [`DomainIf_BcmProtectionEvent_t` 변환 정본](can/BCM_EVENT.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxbcmprotectionevent)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** event type/occurrence/quality/AGE/source generation 및 optional command 관계·affected mask를 보존한다. 사건은 도어 Result와 독립이며 동일 occurrence 반복을 새 보호 사건으로 만들지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. onBcmProtectionEvent. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** IF-O09·IF-O24·D-05; 동일 command/occurrence association·Owner 처리 결과 OPEN. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="api-DomainIf_RxNetworkLocalFailure"></a>
### DomainIf_RxNetworkLocalFailure

```c
DomainIf_Status_t DomainIf_RxNetworkLocalFailure(const NetworkLocalFailure_t *failure);
```

**소유·호출:** DOMAIN 공개 본체, Network M03/F09 호출자. 실제 접수 뒤 Network 후발 실패 → [`NetworkLocalFailure_t` 변환 정본](NETWORK_FAILURE_HEALTH.md) → 이 C2 경계 → DOMAIN Owner. [정확한 API](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_rxnetworklocalfailure)의 common 계약과 위 borrowed 수명을 따른다.

**Data·변환:** 원본 ID·접수 사실과 기존 세 사유인 전송 전 만료/전송 전 문맥 무효/최종 Driver 제출 실패에 정확히 대응하는 근거를 연결하는 공통 정본만 따른다. 이미 전송 중인 오류나 결과 미확정을 DRIVER_SUBMISSION_FINAL로 축약하지 않는다. COMMAND/REQUEST_RESULT/OCCURRENCE의 필수 Manager 소비 경로이며 optional Observer11로 대체하지 않는다.

**반환 이후:** 원 C2 7값은 [공통 결과](#common)로 보존한다. Observer11 대상 아님; 필수 failure sink. 실제 commit과 전달 결과를 분리하며 미확인 단계를 완료로 기록하지 않는다.

**미결·반례:** D-03·IF-O12·G06; CRC reject·Query 부분 실패·주기 상태 누락을 이 경로로 무조건 확대하지 않는다. [역방향 추적](../flows/NETWORK_TO_DOMAIN.md)에서 해당 원 수신으로 돌아간다.

<a id="observer11"></a>
## Observer11 — DOMAIN 소비자 경계

정본은 [DomainIf_RxObserver_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-rxobserver-t)와 [SetRxObserver](../../interface/02_NODES/DOMAIN/20_API.md#api-domainif_setrxobserver)이다. DOMAIN 통합 Owner가 선택적 table을 등록하며 table은 값복사한다. 실제 소비자 함수·등록 여부·교체 동기화는 OPEN이다. Network가 Observer 본체나 별도 Manager를 작성하지 않는다. callback은 해당 Core commit 이후 같은 처리 경계에서 전달하는 borrowed IN이며 지연 소비자는 즉시 복사한다. 반환 7값과 raw 원인은 보존하되 observer 실패를 Core rollback 또는 같은 입력 재실행 근거로 사용하지 않는다.

| 정확한 멤버 선언 | Core 인입·정본 | 소비 의미·부분 실패 |
|---|---|---|
| `DomainIf_Status_t (*onMobileRequest)( const DomainIf_MobileRequest_t *request);` | [DomainIf_RxMobileRequest](#api-DomainIf_RxMobileRequest) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onWarningAck)( const DomainIf_WarningAck_t *ack);` | [DomainIf_RxWarningAck](#api-DomainIf_RxWarningAck) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onBcmStatus)( const DomainIf_BcmStatusReport_t *status);` | [DomainIf_RxBcmStatus](#api-DomainIf_RxBcmStatus) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onBcmCommandResult)( const DomainIf_BcmCommandResult_t *result);` | [DomainIf_RxBcmCommandResult](#api-DomainIf_RxBcmCommandResult) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onBcmProtectionEvent)( const DomainIf_BcmProtectionEvent_t *event);` | [DomainIf_RxBcmProtectionEvent](#api-DomainIf_RxBcmProtectionEvent) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onCisStatus)( const DomainIf_CisStatusReport_t *status);` | [DomainIf_RxCisStatus](#api-DomainIf_RxCisStatus) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onCisFault)( const FaultInfo_t *fault);` | [DomainIf_RxCisFault](#api-DomainIf_RxCisFault) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onWindowCommandResult)( const DomainIf_WindowCommandResult_t *result);` | [DomainIf_RxWindowCommandResult](#api-DomainIf_RxWindowCommandResult) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onWindowAntiPinch)( const DomainIf_AntiPinchReport_t *report);` | [DomainIf_RxWindowAntiPinch](#api-DomainIf_RxWindowAntiPinch) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onWindowFault)( const FaultInfo_t *fault);` | [DomainIf_RxWindowFault](#api-DomainIf_RxWindowFault) | DOMAIN 기능 Owner의 선택적 관찰. 실제 본체/등록 OPEN; Core 반영 후 실패를 재입력하지 않음. |
| `DomainIf_Status_t (*onVssFault)( const FaultInfo_t *fault);` | [DomainIf_RxVssFault](#api-DomainIf_RxVssFault) | 참고 v0.5는 observer-only. 실제 설치/등록 E-S05 대기; 원 callback/consumer 결과 보존, Core APPLIED 합성·불명 결과 자동 재호출 금지. |

NetworkLocalFailure의 필수 Manager sink는 이 11멤버에 없다. `onBcmCommandResult`가 성공해도 물리 도어 확인을 새로 생성하지 않는다. `onWarningAck`가 성공해도 위험 CLEAR가 아니며 `onWindow*` 문서 존재가 WINDOW 활성화 근거가 아니다.

<a id="outcome"></a>
## Network의 로컬 결과 분류 — 단일 private 정본

다음은 B15 `PROPOSED_PRIVATE_FUNCTION`과 문서용 private 타입의 유일한 선언이다. 제품 코드/헤더가 아니다. 상태 저장소·공개 API·새 enum Wire 값을 추가하지 않는다. 이 순수 분류기는 호출 결과와 실제 Owner 증거를 분리해 기록하며 재시도 권한을 발급하지 않는다. `commit`/`observer`는 실제 Owner 단계 증거로만 입력한다. Rx OK에서 추론하면 안 되며 해당 관측 채널이 아직 없으면 UNKNOWN만 사용한다.

```c
typedef enum {
    NET_DOMAIN_COMMIT_UNKNOWN,
    NET_DOMAIN_COMMIT_NOT_APPLIED,
    NET_DOMAIN_COMMIT_APPLIED
} NetDomainCommitEvidence;
typedef enum {
    NET_DOMAIN_OBSERVER_UNKNOWN,
    NET_DOMAIN_OBSERVER_NOT_CALLED,
    NET_DOMAIN_OBSERVER_SUCCEEDED,
    NET_DOMAIN_OBSERVER_FAILED
} NetDomainObserverEvidence;
typedef struct {
    DomainIf_Status_t callStatus; /* 원 상태 그대로; aggregate binding OPEN */
    NetDomainCommitEvidence commit;
    NetDomainObserverEvidence observer;
} NetDomainRxOutcome;

bool NetDomain_ClassifyRxCompletion(DomainIf_Status_t callStatus,
    NetDomainCommitEvidence commit, NetDomainObserverEvidence observer,
    NetDomainRxOutcome *out)
{
    if (out == NULL) return false;
    switch (commit) {
    case NET_DOMAIN_COMMIT_UNKNOWN:
    case NET_DOMAIN_COMMIT_NOT_APPLIED:
    case NET_DOMAIN_COMMIT_APPLIED: break;
    default: return false;
    }
    switch (observer) {
    case NET_DOMAIN_OBSERVER_UNKNOWN:
    case NET_DOMAIN_OBSERVER_NOT_CALLED: break;
    case NET_DOMAIN_OBSERVER_SUCCEEDED:
    case NET_DOMAIN_OBSERVER_FAILED:
        if (commit != NET_DOMAIN_COMMIT_APPLIED) return false;
        break;
    default: return false;
    }
    NetDomainRxOutcome tmp = { callStatus, commit, observer };
    *out = tmp;
    return true;
}
```

**명명된 설계 호출자:** [NETWORK_TO_DOMAIN의 결과 관측 단계](../flows/NETWORK_TO_DOMAIN.md#rx-outcome-callsite)가 `NetCan_RxBcmResult` 등 기존 typed 경로의 실제 C2 반환과 Owner 단계 증거를 받아 이 분류기를 호출한다. 함수명·반환만으로 Core 증거를 추정하지 않으며 실제 관측 공급/Task 바인딩은 OPEN이다.

세 값 인자는 IN이다. `out`은 NULL 불가 caller-owned OUT이며 호출 동안 유효해야 한다. false이면 OUT 불변, true이면 원 값과 단계 증거의 일관성 분류 사본만 얻는다. true는 C2 성공·차량 성공이 아니며 Core 미반영 판정도 자체 생성하지 않는다. 이 함수는 allocation/queue/lock/시계/콜백 없이 M03 Task 문맥에서 호출하며 스스로 입력을 재호출하지 않는다. 알려지지 않은 callStatus도 원 값을 보존한다. signed/unsigned enum의 숫자 크기로 성공을 판단하지 않는다.

| 자체검수 사례 | 예상 결과 |
|---|---|
| OK + commit UNKNOWN + observer UNKNOWN | 분류 true, 실제 snapshot/association 여전히 미확인 |
| REJECTED + commit APPLIED + observer FAILED | 분류 true, Core 완료 사실과 후발 observer 실패를 각각 보존; 전체 재실행 금지 |
| OK + commit NOT_APPLIED + observer SUCCEEDED | 입력 증거 모순이므로 false, OUT 불변 |
| 유효 raw 거리123 | rearDistanceMm123; 값/단위 손실 없음 |
| raw CIS interface1 | CIS_INTERFACE_UNAVAILABLE(2)로 명시 대응. 공개 값1/DEGRADED 부재, 숫자 cast 금지 |
| 첫 M_CONTEXT Rx 반영 뒤 둘째 실패 | 각각 결과 보존, rollback/전체 재호출 없음 |

근거는 봉인 S4A `api/domain_rx.md`, `api/callbacks.md`, S4C `TIME_IDENTITY_CONTRACT.md#projection` 및 위 B11–B14 정본이다. 자체검수는 문서·선언·호출 경계 검증이며 실제 DOMAIN 본체/Task/ABI/동작 시험 PASS를 주장하지 않는다.
