# C enum과 Wire 숫자값의 명시 변환

**WIRE_PROPOSED / BINDING_OPEN.** 아래 표는 원 C2/Gateway 헤더의 enum 정수값과 제안 Wire의 의미 대응이다. 같은 숫자도 정렬·폭이 같은 메모리 배열이라는 뜻은 아니다. `sizeof(enum)`·struct padding·raw cast 전송은 금지한다.

[공통 타입](10_DATA_AND_ID_RULES.md) · [CAN](../03_COMMUNICATION/10_CAN_FD.md) · [UART-M](../03_COMMUNICATION/20_UART_M.md) · [UART-C](../03_COMMUNICATION/30_UART_C.md)

## 필요한 변환 찾기

- 품질: [DataQuality_t](#map-dataquality-t) · [ValueValidity_t](#map-valuevalidity-t) · [QualityReason_t](#map-qualityreason-t) · [FunctionAvailability_t](#map-functionavailability-t)
- 실행 결과: [RequestResult_t](#map-requestresult-t) · [ResultConfirmation_t](#map-resultconfirmation-t) · [ResultReason_t](#map-resultreason-t)
- BCM: [DoorLockState_t](#map-doorlockstate-t) · [FanLevel_t](#map-fanlevel-t) · [DomainIf_BcmCommandKind_t](#map-domainif-bcmcommandkind-t)
- CIS/Pi 근거: [RearMeasurementState_t](#map-rearmeasurementstate-t) · [CisFunctionStatus_t](#map-cisfunctionstatus-t) · [OccupantHazardState_t](#map-occupanthazardstate-t)
- VSS: [VssOneShotEvent_t](#map-vssoneshotevent-t) · [VssState_t](#map-vssstate-t) · [VssAvailability_t](#map-vssavailability-t) · [RearObstacleState_t](#map-rearobstaclestate-t)
- WINDOW 설계: [WindowCommand_t](#map-windowcommand-t) · [AntiPinchState_t](#map-antipinchstate-t) · [DomainIf_AntiPinchProtectionState_t](#map-domainif-antipinchprotectionstate-t)
- Gateway: [Gateway_DataQuality_t](#map-gateway-dataquality-t) · [Gateway_ConnectionState_t](#map-gateway-connectionstate-t) · [CommandOrigin_t](#map-commandorigin-t)
- 새 Net 별칭의 전체 선언: [노드별 별칭 코드 색인](#node-alias-index)

## 변환 불가 값과 안전한 경계

원 헤더 `FunctionAvailability_t.UNSUPPORTED`, `DomainWarningState_t.UNCONFIRMED`, `OccupantHazardState_t.UNCONFIRMED` 및 일부 `ResultReason_t`는 현 Wire 코드에 동등한 표현이 없다. `NOT_ALLOWED/MOTOR_FAULT/SENSOR_FAULT/TIMEOUT/OTHER`를 아무 근거 없이 POWER_NOT_ALLOWED/FAULT/EXPIRED로 바꾸지 않는다. 해당 경로는 **TEAM_DECISION_REQUIRED / 표현 확정 전 구현 차단**이며 정상 CLEAR/AVAILABLE·사유 NONE·차량 FAILED를 합성하지 않는다.

수신 Wire가 유효한 코드라 해도 C2에서 표현할 수 없는 세부 evidence는 node private 보관과 기능 owner 결정이 필요하다. 송신에 없는 code를 새로 만들지 않는다. code sentinel과 예약 값은 수용하지 않는다. 실제 Codec round-trip은 **NOT_EXECUTED**이고 본 표는 정적 제안 대조이다.

<a id="map-dataquality-t"></a>
## `DataQuality_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DATA_QUALITY_OK` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DATA_QUALITY_STALE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DATA_QUALITY_INVALID` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DATA_QUALITY_NO_DATA` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-valuevalidity-t"></a>
## `ValueValidity_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `VALUE_VALIDITY_INVALID` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VALUE_VALIDITY_VALID` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-qualityreason-t"></a>
## `QualityReason_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `QUALITY_REASON_NONE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_NOT_READY` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_OUT_OF_RANGE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_SENSOR_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_VISION_FAULT` | 4 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_STALE` | 5 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_NO_DATA` | 6 | 6 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_COMMUNICATION_FAULT` | 7 | 7 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_RECOVERING` | 8 | 8 | 의미로 명시 변환; C 숫자 cast 금지 |
| `QUALITY_REASON_DATA_INVALID` | 9 | 9 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-functionavailability-t"></a>
## `FunctionAvailability_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `FUNCTION_AVAILABILITY_AVAILABLE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FUNCTION_AVAILABILITY_DEGRADED` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FUNCTION_AVAILABILITY_UNAVAILABLE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FUNCTION_AVAILABILITY_UNSUPPORTED` | 3 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |



<a id="map-requestresult-t"></a>
## `RequestResult_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `REQUEST_RESULT_ACCEPTED` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REQUEST_RESULT_IN_PROGRESS` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REQUEST_RESULT_DONE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REQUEST_RESULT_REJECTED` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REQUEST_RESULT_CANCELLED` | 4 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REQUEST_RESULT_FAILED` | 5 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-resultconfirmation-t"></a>
## `ResultConfirmation_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `RESULT_CONFIRMATION_UNCONFIRMED` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_CONFIRMATION_CONFIRMED` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-resultreason-t"></a>
## `ResultReason_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `RESULT_REASON_NONE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_LOCAL_OVERRIDE` | 1 | 10 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_STOP_REQUESTED` | 2 | 11 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_SUPERSEDED` | 3 | 12 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_ANTIPINCH` | 4 | 13 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_NOT_ALLOWED` | 5 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |
| `RESULT_REASON_INVALID_COMMAND` | 6 | 6 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_STATE_UNTRUSTED` | 7 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_NO_FEEDBACK` | 8 | 7 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_DRIVE_LIMIT_EXCEEDED` | 9 | 8 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_MOTOR_FAULT` | 10 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |
| `RESULT_REASON_SENSOR_FAULT` | 11 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |
| `RESULT_REASON_TIMEOUT` | 12 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |
| `RESULT_REASON_OTHER` | 13 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |
| `RESULT_REASON_NOT_REGISTERED` | 14 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_LINK_LOST` | 15 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_SESSION_INVALID` | 16 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_DOOR_OPEN` | 17 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_ALREADY_AT_TARGET` | 18 | 9 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_FAULT` | 19 | 14 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_STALE` | 20 | 15 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_NO_DATA` | 21 | 16 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_NOT_READY` | 22 | 17 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_OUT_OF_RANGE` | 23 | 18 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_SOURCE_FAILURE` | 24 | 19 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_UNKNOWN_REQUEST` | 25 | 20 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_EXPIRED` | 26 | 21 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_ID_CONFLICT` | 27 | 22 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_POWER_NOT_ALLOWED` | 28 | 23 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_OVERHEAT` | 29 | 24 | 의미로 명시 변환; C 숫자 cast 금지 |
| `RESULT_REASON_FAN_MISMATCH` | 30 | 25 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-faultcategory-t"></a>
## `FaultCategory_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `FAULT_CATEGORY_SENSOR` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAULT_CATEGORY_COMM` | 1 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAULT_CATEGORY_FUNCTION` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-faultstate-t"></a>
## `FaultState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `FAULT_STATE_INACTIVE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAULT_STATE_ACTIVE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAULT_STATE_RECOVERING` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-commonecustate-t"></a>
## `CommonEcuState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `ECU_STATE_INIT` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ECU_STATE_READY` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ECU_STATE_DEGRADED` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ECU_STATE_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-doorlockstate-t"></a>
## `DoorLockState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOOR_LOCK_STATE_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_LOCK_STATE_LOCKED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_LOCK_STATE_UNLOCKED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-dooropenstate-t"></a>
## `DoorOpenState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOOR_OPEN_STATE_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_OPEN_STATE_CLOSED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_OPEN_STATE_OPEN` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-doorcompositestate-t"></a>
## `DoorCompositeState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOOR_COMPOSITE_STATE_NORMAL` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_COMPOSITE_STATE_INCONSISTENT` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_COMPOSITE_STATE_UNTRUSTED` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-doorlocktarget-t"></a>
## `DoorLockTarget_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOOR_TARGET_LOCK` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOOR_TARGET_UNLOCK` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-fanlevel-t"></a>
## `FanLevel_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `FAN_LEVEL_OFF` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAN_LEVEL_LOW` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAN_LEVEL_MEDIUM` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAN_LEVEL_HIGH` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `FAN_LEVEL_UNKNOWN` | 4 | 255 | 상태만 255. 명령·모바일 Fan 설정에 사용 금지 |



<a id="map-thermaldirection-t"></a>
## `ThermalDirection_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `THERMAL_DIRECTION_IDLE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `THERMAL_DIRECTION_COOL` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `THERMAL_DIRECTION_HEAT` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-heatremovalstate-t"></a>
## `HeatRemovalState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `HEAT_REMOVAL_STATE_NORMAL` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `HEAT_REMOVAL_STATE_BLOCKED_OVERHEAT` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `HEAT_REMOVAL_STATE_UNMEASURABLE` | 2 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-interiorlighttype-t"></a>
## `InteriorLightType_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `INTERIOR_LIGHT_TYPE_NORMAL` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `INTERIOR_LIGHT_TYPE_GOODBYE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `INTERIOR_LIGHT_TYPE_WARNING` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `INTERIOR_LIGHT_TYPE_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-interiorlightapplystate-t"></a>
## `InteriorLightApplyState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `INTERIOR_LIGHT_APPLY_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `INTERIOR_LIGHT_APPLY_APPLIED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `INTERIOR_LIGHT_APPLY_FAILED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-deviceregistrationstate-t"></a>
## `DeviceRegistrationState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DEVICE_REGISTRATION_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DEVICE_REGISTRATION_NOT_REGISTERED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DEVICE_REGISTRATION_REGISTERED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-btconnectionstate-t"></a>
## `BtConnectionState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `BT_CONNECTION_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BT_CONNECTION_DISCONNECTED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BT_CONNECTION_CONNECTED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-appactivestate-t"></a>
## `AppActiveState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `APP_ACTIVE_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `APP_ACTIVE_INACTIVE` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `APP_ACTIVE_ACTIVE` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-proximitystate-t"></a>
## `ProximityState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `PROXIMITY_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `PROXIMITY_FAR` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `PROXIMITY_NEAR` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-digitalkeysetting-t"></a>
## `DigitalKeySetting_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DIGITAL_KEY_SETTING_OFF` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DIGITAL_KEY_SETTING_ON` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-unlockorigin-t"></a>
## `UnlockOrigin_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `UNLOCK_ORIGIN_USER_REQUEST` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `UNLOCK_ORIGIN_PROXIMITY_AUTO` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-rearmeasurementstate-t"></a>
## `RearMeasurementState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `REAR_MEASUREMENT_VALID_DISTANCE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_MEASUREMENT_NO_OBJECT` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_MEASUREMENT_UNAVAILABLE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_MEASUREMENT_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_MEASUREMENT_RECOVERING` | 4 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_MEASUREMENT_INACTIVE` | 5 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-cisstate-t"></a>
## `CisState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `CIS_STATE_STARTUP` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_STATE_READY` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_STATE_ACTIVE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_STATE_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-cisfunctionstatus-t"></a>
## `CisFunctionStatus_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `CIS_FUNCTION_STATUS_NOT_READY` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_FUNCTION_STATUS_READY` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_FUNCTION_STATUS_ACTIVE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_FUNCTION_STATUS_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_FUNCTION_STATUS_RECOVERING` | 4 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-windowcommand-t"></a>
## `WindowCommand_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `WINDOW_COMMAND_OPEN` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_COMMAND_CLOSE` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_COMMAND_STOP` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_COMMAND_VENT` | 3 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_COMMAND_MOVE_TO_POSITION` | 4 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-windowmotionstate-t"></a>
## `WindowMotionState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `WINDOW_MOTION_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_MOTION_STOPPED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_MOTION_OPENING` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_MOTION_CLOSING` | 3 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `WINDOW_MOTION_ANTIPINCH_REVERSING` | 4 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-antipinchstate-t"></a>
## `AntiPinchState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `ANTIPINCH_STATE_CLEAR` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ANTIPINCH_STATE_ACTIVE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-vssstate-t"></a>
## `VssState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `VSS_STATE_STARTUP` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_STATE_READY` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_STATE_PLAYING` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_STATE_FAULT` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-vssavailability-t"></a>
## `VssAvailability_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `VSS_AVAILABILITY_FULL` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_AVAILABILITY_DEGRADED` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_AVAILABILITY_UNAVAILABLE` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-rearobstaclestate-t"></a>
## `RearObstacleState_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `REAR_OBSTACLE_STATE_CLEAR` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_OBSTACLE_STATE_CAUTION` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_OBSTACLE_STATE_EMERGENCY` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-reardetectionactivation-t"></a>
## `RearDetectionActivation_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `REAR_DETECTION_DISABLED` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `REAR_DETECTION_ACTIVE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-vssoneshotevent-t"></a>
## `VssOneShotEvent_t`

원 선언: `Vehicle_Types_v0.2.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `VSS_EVENT_VEHICLE_WELCOME` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_EVENT_VEHICLE_GOODBYE` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_EVENT_DOOR_LOCK_COMPLETE` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_EVENT_DOOR_UNLOCK_COMPLETE` | 3 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `VSS_EVENT_DOOR_LOCK_ERROR` | 4 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainfunctionid-t"></a>
## `DomainFunctionId_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOMAIN_FUNCTION_DOOR` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_CLIMATE` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_INTERIOR_LIGHT` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_DIGITAL_KEY` | 3 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_WINDOW` | 4 | 8 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_REAR_WARNING` | 5 | 7 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_OCCUPANT_WARNING` | 6 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_VSS` | 7 | 9 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_ENVIRONMENT` | 8 | 6 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_FUNCTION_COUNT` | 9 | 미정 | 개수 sentinel; 값으로 송신 금지 |



<a id="map-domainif-mobilerequesttype-t"></a>
## `DomainIf_MobileRequestType_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `MOBILE_REQUEST_DOOR_LOCK` | 0 | kind1 / op0 LOCK,1 UNLOCK / ARG0..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_CLIMATE_TARGET_TEMPERATURE` | 1 | kind2 / op1 / ARG0..1=i16 LE centi-°C, ARG2..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_CLIMATE_AUTO_ENABLE` | 2 | kind2 / op2 / ARG0=bool0/1, ARG1..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_FAN_LEVEL` | 3 | kind2 / op3 / ARG0=Fan0..3, ARG1..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_INTERIOR_LIGHT_ENABLE` | 4 | kind3 / op1 / ARG0=bool0/1, ARG1..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_INTERIOR_LIGHT_LEVEL` | 5 | kind3 / op2 / ARG0=0..100%, ARG1..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_INTERIOR_LIGHT_COLOR` | 6 | kind3 / op3 / ARG0..2=RGB0..255, ARG3..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_REQUEST_DIGITAL_KEY_SETTING` | 7 | kind4 / op1 / ARG0=OFF0/ON1, ARG1..5=0 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainwarningtype-t"></a>
## `DomainWarningType_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOMAIN_WARNING_WINDOW_ANTIPINCH` | 0 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_OCCUPANT_HAZARD` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_REAR_OBSTACLE` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_DOOR_LEFT_OPEN` | 3 | 8 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_BCM_FAULT` | 4 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_CIS_FAULT` | 5 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_WINDOW_FAULT` | 6 | 6 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_VSS_FAULT` | 7 | 7 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_COUNT` | 8 | 미정 | 개수 sentinel; 값으로 송신 금지 |



<a id="map-domainwarningseverity-t"></a>
## `DomainWarningSeverity_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOMAIN_WARNING_SEVERITY_INFO` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_SEVERITY_CAUTION` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_SEVERITY_EMERGENCY` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainwarningstate-t"></a>
## `DomainWarningState_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `DOMAIN_WARNING_STATE_CLEAR` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_STATE_ACTIVE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `DOMAIN_WARNING_STATE_UNCONFIRMED` | 2 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |



<a id="map-domainif-bcmcommandkind-t"></a>
## `DomainIf_BcmCommandKind_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `BCM_COMMAND_KIND_DOOR` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_COMMAND_KIND_CLIMATE` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_COMMAND_KIND_INTERIOR_LIGHT` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainif-bcmdoorevidencetype-t"></a>
## `DomainIf_BcmDoorEvidenceType_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `BCM_DOOR_EVIDENCE_LOCK_COMPLETED` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_DOOR_EVIDENCE_UNLOCK_COMPLETED` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_DOOR_EVIDENCE_ALREADY_AT_TARGET` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_DOOR_EVIDENCE_REQUEST_REJECTED` | 3 | 4 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_DOOR_EVIDENCE_REQUEST_FAILED` | 4 | 5 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainif-bcmprotectioneventtype-t"></a>
## `DomainIf_BcmProtectionEventType_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `BCM_PROTECTION_OVERHEAT_DETECTED` | 0 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_PROTECTION_FAN_MISMATCH_DETECTED` | 1 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `BCM_PROTECTION_RECOVERY_CONFIRMED` | 2 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainif-mobileresulttarget-t"></a>
## `DomainIf_MobileResultTarget_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `MOBILE_RESULT_TARGET_DOMAIN_SETTING` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_RESULT_TARGET_BCM` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `MOBILE_RESULT_TARGET_WINDOW` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainif-cisinterfacestatus-t"></a>
## `DomainIf_CisInterfaceStatus_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `CIS_INTERFACE_AVAILABLE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_INTERFACE_UNAVAILABLE` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `CIS_INTERFACE_RECOVERING` | 3 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-domainif-antipinchprotectionstate-t"></a>
## `DomainIf_AntiPinchProtectionState_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `ANTIPINCH_PROTECTION_IDLE` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ANTIPINCH_PROTECTION_REVERSING` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ANTIPINCH_PROTECTION_COMPLETED` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `ANTIPINCH_PROTECTION_ABORTED` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-occupanthazardstate-t"></a>
## `OccupantHazardState_t`

원 선언: `Domain_Interface_v0.5_C2_target.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `OCCUPANT_HAZARD_CLEAR` | 0 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `OCCUPANT_HAZARD_ACTIVE` | 1 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |
| `OCCUPANT_HAZARD_UNCONFIRMED` | 2 | 미정 | 현 Wire에 동등한 표현 없음. 임의 대체·새 code 추가 금지 |



<a id="map-gateway-registrationstate-t"></a>
## `Gateway_RegistrationState_t`

원 선언: `Gateway_Types.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `GATEWAY_REGISTRATION_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_NOT_REGISTERED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_REGISTERED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-gateway-connectionstate-t"></a>
## `Gateway_ConnectionState_t`

원 선언: `Gateway_Types.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `GATEWAY_CONNECTION_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_DISCONNECTED` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_CONNECTED` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-gateway-appactivitystate-t"></a>
## `Gateway_AppActivityState_t`

원 선언: `Gateway_Types.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `GATEWAY_APP_ACTIVITY_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_APP_INACTIVE` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_APP_ACTIVE` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-gateway-proximitystate-t"></a>
## `Gateway_ProximityState_t`

원 선언: `Gateway_Types.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `GATEWAY_PROXIMITY_UNKNOWN` | 0 | 255 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_PROXIMITY_FAR` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_PROXIMITY_NEAR` | 2 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-gateway-dataquality-t"></a>
## `Gateway_DataQuality_t`

원 선언: `Gateway_Types.h` · **선언은 보존**, 아래 Wire mapping은 제안 Codec 계약이다.

| 기존 C 상수 | 실제 C 값 | 제안 Wire 값 / 분해 | 유효 조건 |
| --- | --- | --- | --- |
| `GATEWAY_DATA_QUALITY_UNKNOWN` | 0 | 미정 | 0을 Wire OK로 cast 금지. 실제 자료 부재·유효성 근거 없으면 사용 차단, 구체 표현 OPEN |
| `GATEWAY_DATA_QUALITY_VALID` | 1 | 0 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_DATA_QUALITY_INVALID` | 2 | 2 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_DATA_QUALITY_NO_DATA` | 3 | 3 | 의미로 명시 변환; C 숫자 cast 금지 |
| `GATEWAY_DATA_QUALITY_STALE` | 4 | 1 | 의미로 명시 변환; C 숫자 cast 금지 |



<a id="map-commandorigin-t"></a>
## `CommandOrigin_t` → ORIGIN/SOURCE_CONTEXT

**PROPOSED_DESIGN / TEAM_DECISION_REQUIRED.** ORIGIN은 축약 projection이다. C2 origin을 역방향으로 복구할 수 없다. decisionSequence·원 요청 등 Domain 내부 provenance는 소멸시키지 않는다. 실제 소비자에게 세부 policy가 필요하면 축약을 승인하기 전 바인딩을 차단한다.

| 기존 C 상수 | C 값 | ORIGIN 후보 | 제약 |
| --- | --- | --- | --- |
| COMMAND_ORIGIN_MOBILE_REQUEST | 0 | 1 | MOBILE_MANUAL; 원 Request context 유지 |
| COMMAND_ORIGIN_DIGITAL_KEY | 1 | 2 | PROXIMITY_AUTO |
| COMMAND_ORIGIN_CLIMATE_POLICY | 2 | 5 | VEHICLE_POLICY; 세부 provenance는 Domain 내부만 |
| COMMAND_ORIGIN_INTERIOR_LIGHT_POLICY | 3 | 5 | VEHICLE_POLICY; 세부 provenance는 Domain 내부만 |
| COMMAND_ORIGIN_AUTO_VENTILATION | 4 | 5 | VEHICLE_POLICY; WINDOW SOURCE_CONTEXT에는 AUTO_FUNCTION 2 후보 |
| COMMAND_ORIGIN_SYSTEM | 5 | 5 | VEHICLE_POLICY 후보; SYSTEM/LOCAL/TEST 구체 입력 합의 필요 |


WINDOW SOURCE_CONTEXT(0 없음 / 1 Domain 정책 / 2 자동 기능 / 3 로컬 / 4 시험)는 ORIGIN(0..5)과 다른 코드다. MOBILE 창문 제어는 현 범위 밖이며 관련 요청을 임의 송신하지 않는다.

<a id="node-alias-index"></a>
## 노드별 별칭 코드 색인

전체 alias/enum 선언의 정본은 각 노드 Data다. 아래는 Wire code 검색용 표이며 동일 타입 선언을 중복 배포하지 않는다.

| 정본 타입 | 허용 코드·숫자 |
| --- | --- |
| [`NetDoorTarget`](../02_NODES/BCM/10_DATA.md#type-netdoortarget) | `NET_DOOR_TARGET_LOCK`=0, `NET_DOOR_TARGET_UNLOCK`=1, `NET_DOOR_TARGET_NOT_APPLICABLE`=255 |
| [`NetDoorLockState`](../02_NODES/BCM/10_DATA.md#type-netdoorlockstate) | `NET_DOOR_LOCK_LOCKED`=0, `NET_DOOR_LOCK_UNLOCKED`=1, `NET_DOOR_LOCK_UNKNOWN`=255 |
| [`NetDoorOpenState`](../02_NODES/BCM/10_DATA.md#type-netdooropenstate) | `NET_DOOR_OPEN_CLOSED`=0, `NET_DOOR_OPEN_OPEN`=1, `NET_DOOR_OPEN_UNKNOWN`=255 |
| [`NetDoorCompositeState`](../02_NODES/BCM/10_DATA.md#type-netdoorcompositestate) | `NET_DOOR_COMPOSITE_NORMAL`=0, `NET_DOOR_COMPOSITE_INCONSISTENT`=1, `NET_DOOR_COMPOSITE_UNTRUSTED`=2 |
| [`NetHeatRemovalState`](../02_NODES/BCM/10_DATA.md#type-netheatremovalstate) | `NET_HEAT_REMOVAL_NORMAL`=0, `NET_HEAT_REMOVAL_OVERHEAT_CUTOFF`=1, `NET_HEAT_REMOVAL_UNAVAILABLE`=255 |
| [`NetLightApplyResult`](../02_NODES/BCM/10_DATA.md#type-netlightapplyresult) | `NET_LIGHT_APPLY_APPLIED`=0, `NET_LIGHT_APPLY_FAILED`=1, `NET_LIGHT_APPLY_UNKNOWN`=255 |
| [`NetBcmCommandKind`](../02_NODES/BCM/10_DATA.md#type-netbcmcommandkind) | `NET_BCM_COMMAND_DOOR`=1, `NET_BCM_COMMAND_CLIMATE`=2, `NET_BCM_COMMAND_LIGHT`=3 |
| [`NetDoorEvent`](../02_NODES/BCM/10_DATA.md#type-netdoorevent) | `NET_DOOR_EVENT_NONE`=0, `NET_DOOR_EVENT_LOCK_COMPLETED`=1, `NET_DOOR_EVENT_UNLOCK_COMPLETED`=2, `NET_DOOR_EVENT_ALREADY_AT_TARGET`=3, `NET_DOOR_EVENT_REQUEST_REJECTED`=4, `NET_DOOR_EVENT_REQUEST_FAILED`=5 |
| [`NetBcmEventKind`](../02_NODES/BCM/10_DATA.md#type-netbcmeventkind) | `NET_BCM_EVENT_OVERHEAT_DETECTED`=1, `NET_BCM_EVENT_FAN_MISMATCH_DETECTED`=2, `NET_BCM_EVENT_RECOVERY_CONFIRMED`=3 |
| [`NetBcmFaultMask`](../02_NODES/BCM/10_DATA.md#type-netbcmfaultmask) | Wire u16 비트 집합. 상수 enum의 bit 정의·허용 mask는 정본 Data 참조; C enum 크기를 전송 폭으로 사용하지 않음 |
| [`NetBcmFaultCode`](../02_NODES/BCM/10_DATA.md#type-netbcmfaultcode) | `NET_BCM_FAULT_CODE_NONE`=0, `NET_BCM_FAULT_CODE_DOOR_SENSOR_FAULT`=1, `NET_BCM_FAULT_CODE_FAN_SENSOR_FAULT`=2, `NET_BCM_FAULT_CODE_TEMPERATURE_SENSOR_FAULT`=3, `NET_BCM_FAULT_CODE_OVERHEAT_FAULT`=4, `NET_BCM_FAULT_CODE_FAN_FAULT`=5, `NET_BCM_FAULT_CODE_LOCK_ACTUATOR_FAULT`=6, `NET_BCM_FAULT_CODE_LIGHT_APPLY_FAULT`=7, `NET_BCM_FAULT_CODE_COMM_TIMEOUT_FAULT`=8 |
| [`NetCisState`](../02_NODES/CIS/10_DATA.md#type-netcisstate) | `NET_CIS_STATE_STARTUP`=0, `NET_CIS_STATE_READY`=1, `NET_CIS_STATE_ACTIVE`=2, `NET_CIS_STATE_FAULT`=3 |
| [`NetInterfaceState`](../02_NODES/CIS/10_DATA.md#type-netinterfacestate) | `NET_INTERFACE_READY`=0, `NET_INTERFACE_UNAVAILABLE`=1, `NET_INTERFACE_RECOVERING`=2 |
| [`NetProximityStatus`](../02_NODES/CIS/10_DATA.md#type-netproximitystatus) | `NET_PROXIMITY_VALID_DISTANCE`=0, `NET_PROXIMITY_NO_OBJECT`=1, `NET_PROXIMITY_UNAVAILABLE`=2, `NET_PROXIMITY_FAULT`=3, `NET_PROXIMITY_RECOVERING`=4, `NET_PROXIMITY_INACTIVE`=5 |
| [`NetCisFaultMask`](../02_NODES/CIS/10_DATA.md#type-netcisfaultmask) | Wire u16 비트 집합. 상수 enum의 bit 정의·허용 mask는 정본 Data 참조; C enum 크기를 전송 폭으로 사용하지 않음 |
| [`NetCisFaultCode`](../02_NODES/CIS/10_DATA.md#type-netcisfaultcode) | `NET_CIS_FAULT_CODE_NONE`=0, `NET_CIS_FAULT_CODE_INITIALIZATION_FAILURE`=1, `NET_CIS_FAULT_CODE_VISION_FAULT`=2, `NET_CIS_FAULT_CODE_TEMPERATURE_SENSOR_FAULT`=3, `NET_CIS_FAULT_CODE_HUMIDITY_SENSOR_FAULT`=4, `NET_CIS_FAULT_CODE_ILLUMINANCE_SENSOR_FAULT`=5, `NET_CIS_FAULT_CODE_PROXIMITY_SENSOR_FAULT`=6, `NET_CIS_FAULT_CODE_COMMUNICATION_FAULT`=7, `NET_CIS_FAULT_CODE_DATA_INVALID`=8, `NET_CIS_FAULT_CODE_OUT_OF_RANGE`=9 |
| [`NetRequestKind`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netrequestkind) | `NET_REQUEST_DOOR`=1, `NET_REQUEST_CLIMATE`=2, `NET_REQUEST_INTERIOR_LIGHT`=3, `NET_REQUEST_DIGITAL_KEY`=4 |
| [`NetDoorOperation`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netdooroperation) | `NET_DOOR_OPERATION_LOCK`=0, `NET_DOOR_OPERATION_UNLOCK`=1 |
| [`NetClimateOperation`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netclimateoperation) | `NET_CLIMATE_OPERATION_TARGET_TEMPERATURE`=1, `NET_CLIMATE_OPERATION_AUTO`=2, `NET_CLIMATE_OPERATION_FAN`=3 |
| [`NetLightOperation`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netlightoperation) | `NET_LIGHT_OPERATION_ENABLE`=1, `NET_LIGHT_OPERATION_LEVEL`=2, `NET_LIGHT_OPERATION_RGB`=3 |
| [`NetDigitalKeyOperation`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netdigitalkeyoperation) | `NET_DIGITAL_KEY_OPERATION_ENABLE`=1 |
| [`NetQueryScope`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netqueryscope) | `NET_QUERY_SCOPE_STATE`=0, `NET_QUERY_SCOPE_RESULT`=1, `NET_QUERY_SCOPE_WARNING`=2, `NET_QUERY_SCOPE_ALL`=3 |
| [`NetQueryStatus`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netquerystatus) | `NET_QUERY_STATUS_DONE`=0, `NET_QUERY_STATUS_NOT_FOUND`=1, `NET_QUERY_STATUS_PARTIAL_UNCONFIRMED`=2, `NET_QUERY_STATUS_CONTEXT_REJECTED`=3 |
| [`NetCommandTarget`](../02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md#type-netcommandtarget) | `NET_COMMAND_TARGET_DOMAIN_SETTING`=0, `NET_COMMAND_TARGET_BCM`=1, `NET_COMMAND_TARGET_WINDOW`=2 |
| [`NetVisionFaultCode`](../02_NODES/PI/10_DATA.md#type-netvisionfaultcode) | `NET_VISION_FAULT_NONE`=0, `NET_VISION_FAULT_INITIALIZATION`=1, `NET_VISION_FAULT_CAMERA`=2, `NET_VISION_FAULT_INFERENCE`=3, `NET_VISION_FAULT_DATA_INVALID`=4 |
| [`NetVssEventKind`](../02_NODES/VSS/10_DATA.md#type-netvsseventkind) | `NET_VSS_EVENT_WELCOME`=1, `NET_VSS_EVENT_GOODBYE`=2, `NET_VSS_EVENT_LOCK_COMPLETE`=3, `NET_VSS_EVENT_UNLOCK_COMPLETE`=4, `NET_VSS_EVENT_LOCK_ERROR`=5 |
| [`NetVssState`](../02_NODES/VSS/10_DATA.md#type-netvssstate) | `NET_VSS_STATE_STARTUP`=0, `NET_VSS_STATE_READY`=1, `NET_VSS_STATE_PLAYING`=2, `NET_VSS_STATE_FAULT`=3 |
| [`NetVssAvailability`](../02_NODES/VSS/10_DATA.md#type-netvssavailability) | `NET_VSS_AVAILABILITY_FULL`=0, `NET_VSS_AVAILABILITY_DEGRADED`=1, `NET_VSS_AVAILABILITY_UNAVAILABLE`=2 |
| [`NetRearWarningState`](../02_NODES/VSS/10_DATA.md#type-netrearwarningstate) | `NET_REAR_WARNING_CLEAR`=0, `NET_REAR_WARNING_CAUTION`=1, `NET_REAR_WARNING_EMERGENCY`=2 |
| [`NetVssFaultMask`](../02_NODES/VSS/10_DATA.md#type-netvssfaultmask) | Wire u8 비트 집합. 상수 enum의 bit 정의·허용 mask는 정본 Data 참조; C enum 크기를 전송 폭으로 사용하지 않음 |
| [`NetVssFaultCode`](../02_NODES/VSS/10_DATA.md#type-netvssfaultcode) | `NET_VSS_FAULT_CODE_NONE`=0, `NET_VSS_FAULT_CODE_SOUND_ASSET_UNAVAILABLE`=1, `NET_VSS_FAULT_CODE_PLAYBACK_START_FAILURE`=2, `NET_VSS_FAULT_CODE_AUDIO_OUTPUT_FAILURE`=3, `NET_VSS_FAULT_CODE_PLAYBACK_STATE_FAILURE`=4, `NET_VSS_FAULT_CODE_INITIALIZATION_FAILURE`=5 |
| [`NetWindowCommandKind`](../02_NODES/WINDOW/10_DATA.md#type-netwindowcommandkind) | `NET_WINDOW_COMMAND_OPEN`=1, `NET_WINDOW_COMMAND_CLOSE`=2, `NET_WINDOW_COMMAND_STOP`=3, `NET_WINDOW_COMMAND_VENT`=4, `NET_WINDOW_COMMAND_MOVE_TO_POSITION`=5 |
| [`NetSourceContext`](../02_NODES/WINDOW/10_DATA.md#type-netsourcecontext) | `NET_SOURCE_CONTEXT_NONE`=0, `NET_SOURCE_CONTEXT_DOMAIN_POLICY`=1, `NET_SOURCE_CONTEXT_AUTO_FUNCTION`=2, `NET_SOURCE_CONTEXT_LOCAL`=3, `NET_SOURCE_CONTEXT_TEST`=4 |
| [`NetWindowMotionState`](../02_NODES/WINDOW/10_DATA.md#type-netwindowmotionstate) | `NET_WINDOW_MOTION_STOPPED`=0, `NET_WINDOW_MOTION_OPENING`=1, `NET_WINDOW_MOTION_CLOSING`=2, `NET_WINDOW_MOTION_ANTIPINCH_REVERSING`=3, `NET_WINDOW_MOTION_UNKNOWN`=255 |
| [`NetAntiPinchProtectionState`](../02_NODES/WINDOW/10_DATA.md#type-netantipinchprotectionstate) | `NET_ANTIPINCH_PROTECTION_IDLE`=0, `NET_ANTIPINCH_PROTECTION_IN_PROGRESS`=1, `NET_ANTIPINCH_PROTECTION_DONE`=2, `NET_ANTIPINCH_PROTECTION_INTERRUPTED`=3 |
| [`NetWindowEventKind`](../02_NODES/WINDOW/10_DATA.md#type-netwindoweventkind) | `NET_WINDOW_EVENT_FULLY_OPENED`=1, `NET_WINDOW_EVENT_FULLY_CLOSED`=2, `NET_WINDOW_EVENT_ANTIPINCH`=3 |
| [`NetWindowFaultState`](../02_NODES/WINDOW/10_DATA.md#type-netwindowfaultstate) | `NET_WINDOW_FAULT_STATE_NONE`=0, `NET_WINDOW_FAULT_STATE_ACTIVE`=1, `NET_WINDOW_FAULT_STATE_RECOVERING`=2 |
| [`NetWindowFaultCode`](../02_NODES/WINDOW/10_DATA.md#type-netwindowfaultcode) | `NET_WINDOW_FAULT_NONE`=0, `NET_WINDOW_FAULT_POSITION`=1, `NET_WINDOW_FAULT_DRIVE`=2, `NET_WINDOW_FAULT_ENDPOINT`=3, `NET_WINDOW_FAULT_PROTECTION`=4, `NET_WINDOW_FAULT_DOMAIN_TIMEOUT`=5, `NET_WINDOW_FAULT_INITIALIZATION`=6, `NET_WINDOW_FAULT_OTHER`=255 |
