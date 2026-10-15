# DOMAIN TxPort — 26개 콜백의 접수·복사·전송 경계

[개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [등록](DOMAIN_REGISTRATION.md) · [명령 흐름](../flows/DOMAIN_COMMAND_TO_NETWORK.md) · [공통 수명](NETWORK_SERVICE_LIFETIME.md) · [실패·Health](NETWORK_FAILURE_HEALTH.md)

DOMAIN은 공개 `DomainIf_Tx*` 26개 본체를 소유하고 Network는 같은 DOMAIN MCU의 `DomainIf_TxPort_t` 26개 콜백을 제공한다. 이 문서의 연결은 L02/M03/F07 → L03/M04/F11·F12 → L04/M05 또는 M06 → L05/M07·M08이다. F-ID는 책임 분모이며 함수 수를 늘리는 번호가 아니다. C2 72개, TxPort26개, RxObserver11개의 분모를 서로 더하지 않는다.

선언 출처는 [C2 TxPort 전체 선언](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-txport-t), [공개 API 원문](../../interface/02_NODES/DOMAIN/20_API.md), [기존 private 경로](../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#tx-port)다. 원형 보존은 `DECLARED_SOURCE`; 실제 함수 주소·Task·Driver·capacity·필수 sink는 `BINDING_OPEN`이다. B15는 설명용 private C 의사코드만 추가한다. 제품 함수/헤더/등록 코드가 존재하거나 실행됐다는 뜻이 아니다.

<a id="admission"></a>
## 모든 콜백에 적용하는 인수 순서

1. 호출자는 DOMAIN owner Task다. ISR에서 C2나 아래 콜백을 직접 실행하지 않는다. callback은 Driver/상대 응답을 기다리지 않는다.
2. NULL·enum·범위·조건부 필드를 검증한다. 잘못된 C2 코드와 미합의인 raw 매핑을 구별한다. 미합의 계약은 정상 숫자0을 넣어 통과시키지 않는다.
3. 현재 준비 상태, 원 의미 identity와 AGE 기준, 대상 기동 또는 recipient binding, 필요한 raw evidence, 해당 route의 필수 failure sink를 확인한다. `DomainIf_IsInitialized()==true`만으로 이 조건들을 만족했다고 보지 않는다.
4. 반환 전에 자기 소유 항목에 **아래 필드표의 모든 활성 필드와 중첩 값**, 원 identity/시간 기준, 필요한 Queue 슬롯을 함께 확보한다. 임시 검증/예약 실패는 새 항목 전체를 미접수로 끝내고 이전 항목을 그대로 둔다.
5. 검증된 안정 사본과 추적 문맥의 게시가 원자적으로 완료된 후에만 `DOMAIN_IF_OK`다. 단순 지역 변수 복사, queue 슬롯 예약, raw byte 생성만으로 OK를 반환하지 않는다. 실제 Driver 인계 직전 AGE/Boot/Session을 다시 검사한다.
6. Driver accept·terminal outcome·reference release는 각 submission identity에 별도로 기록한다. release 선도착은 바이트 수명만 끝내며 결과 미결·재제출 금지 상태는 유지한다. 동일 원 명령이 결과 미확인인데 빈 슬롯이 생겼다는 이유로 다시 제출하지 않는다.

**복사 범위:** 현재 26개 source DTO와 그 중첩 타입은 scalar/enum/값 struct/고정 배열이며 내부 borrowed data pointer가 없다. 따라서 유효하게 초기화된 활성 필드의 값 복사는 충분하다. `DomainIf_FunctionAvailabilitySet_t.items[DOMAIN_FUNCTION_COUNT]`도 배열 전체를 값 복사한다. 함수포인터가 든 등록 table의 복사는 데이터 DTO와 별개다. 향후 source ABI에 포인터가 추가되면 pointed data의 길이·소유·해제까지 다시 설계해야 하며 shallow copy를 승인하지 않는다. C 구조체 padding을 Wire로 보내는 memcpy는 금지한다.

조건부 멤버는 원 의미대로 해석한다. `hasRequestContext=false`, `hasVehicleResult=false`, 설정의 `has*=false`, `activeFault.faultCode=0`은 대응 비활성 값을 읽어 검증하거나 정상값을 생성하는 근거가 아니다. 아래 필드표의 “보존”은 **유효한 활성 값의 안정 보존**이며 미초기화된 비활성 메모리를 전부 읽으라는 뜻이 아니다.

<a id="status-map"></a>
## 반환값을 명시적으로 연결

`NetDomain_MapSubmitStatus`는 이번 설명용 `PROPOSED_PRIVATE_FUNCTION`이다. 원 반환 계약은 [Interface 공통 표](../../interface/02_NODES/DOMAIN/20_API.md#ports)를 따른다. C2의 NOT_INITIALIZED·REJECTED·UNSUPPORTED를 Network4 상태로 역변환하지 않는다. 알 수 없는 내부 값은 준비 미확인으로 제한하고 M09 진단에 원 값을 남기는 후보이며 성공/차량 FAILED를 합성하지 않는다.

```c
DomainIf_Status_t NetDomain_MapSubmitStatus(NetSubmitResult submitted)
{
    switch (submitted) {
    case NET_SUBMIT_OK:               return DOMAIN_IF_OK;
    case NET_SUBMIT_INVALID_ARGUMENT: return DOMAIN_IF_INVALID_ARGUMENT;
    case NET_SUBMIT_NOT_READY:        return DOMAIN_IF_NOT_AVAILABLE;
    case NET_SUBMIT_NO_CAPACITY:      return DOMAIN_IF_QUEUE_FULL;
    default:                         return DOMAIN_IF_NOT_AVAILABLE;
    }
}
```

인자는 값 전달이고 OUT/저장 부수효과는 없다. `NET_SUBMIT_OK`는 호출된 기존 submitter가 완전 안정 사본·원 문맥·슬롯의 원자 게시를 끝냈다는 증거가 있어야 한다. 이 switch가 그 사실을 새로 만들어 주지는 않는다. source 선언의 상태 숫자를 단순 cast하면 NOT_READY=2가 NOT_INITIALIZED=2로 잘못 연결되므로 금지한다.

<a id="nested-fields"></a>
## 중첩 값과 후발 결과의 공통 규칙

| 중첩 자료 정본 | 반드시 보존할 활성 값 | 해석 제한 |
|---|---|---|
| [DomainIf_CommandContext_t](../data/CAN_COMMON_DATA.md#type-DomainIf_CommandContext_t) | commandId, decisionSequence, hasRequestContext, requestContext.sessionId/requestId, requestDeviceContextId, origin, commandAgeMs, meta 전체 | decisionSequence는 DOMAIN 내부 출처이고 새 Wire/App 필수 필드가 아니다. commandAge와 판단자료 AGE를 분리한다. |
| [SignalMeta_t/ValueQuality_t](../data/CIS_REAR_DATA.md#declared) | quality, ageMs, updateSequence; validity, reason, meta | 같은 입력 재전송은 새 평가가 아니다. 독립 필드 AGE를 서로 초기화하지 않는다. |
| [SourceEvidence_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-sourceevidence-t) | generation, sequence, ageMs | 마지막 송신 홉 Boot나 LinkSeq로 원 관측을 대체하지 않는다. Generation0 정책은 회신 대기다. |
| [RequestResultInfo_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-requestresultinfo-t) | context.sessionId/requestId, 의미상 활성 result, reason, confirmation | 원 과거 RequestSession과 현재 recipient Session은 독립이다. |
| [NetworkLocalFailure_t](../data/CAN_COMMON_DATA.md#type-NetworkLocalFailure_t) | 최초 admission 생성 세대와 실제 있는 Command/Request/Occurrence identity | target ECU Boot·현재 App Session으로 재라벨하지 않는다. 필수 DOMAIN sink는 optional Observer로 대체할 수 없다. |

동기 거부는 caller에게 반환하며 이미 성공한 항목의 후발 실패처럼 중복 계상하지 않는다. 큐 인수 뒤 **최종 Driver 제출 실패** 또는 실제 **송신 전** 만료·문맥 무효를 확인한 경우만 해당 기존 3개 failure reason을 사용한다. 이미 전송 중인 오류를 `DRIVER_SUBMISSION_FINAL`로 축약하지 않는다. 임시 busy·알 수 없는 terminal 원인은 근거를 진단과 identity HOLD로 보존한다. 주기 상태의 유실은 M09 계측으로 남기며 임의 Command/Request를 만들어 failure API에 넣지 않는다. [공통 실패 정본](NETWORK_FAILURE_HEALTH.md)이 해당 항목의 terminal·통지·release 수명을 관리한다.

## 콜백별 원형·전체 필드·실제 경로

각 표의 모든 필드는 IN/borrowed이며 호출 중 동일 owner의 불변 자료다. 표시한 기본 원형과 private 구현 후보를 구별한다. 17개 Mobile callback의 `query=NULL`은 자발 출력이며 Query는 동일 submitter에 검증된 non-NULL 문맥을 넣는 기존 [M02/B13 흐름](../flows/UART_M_QUERY_AND_PROJECTION.md)을 재사용한다. 전송 바이트와 AGE 계산은 각 기존 leaf 정본에 하나만 유지한다.

<a id="txBcmDoorCommand"></a>
### 1. `txBcmDoorCommand` → `BCM_DOOR_COMMAND`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxBcmDoorCommand( const DomainIf_BcmDoorCommand_t *command);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txBcmDoorCommand)( const DomainIf_BcmDoorCommand_t *command);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_BcmDoorCommand_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-bcmdoorcommand-t)이며 세부 Wire/Codec/수신 의무는 [BCM_DOOR_COMMAND](can/BCM_DOOR_COMMAND.md)다. 실제 함수 주소는 `NetCan_TxBcmDoorCommand` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `context` | `DomainIf_CommandContext_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `target` | `DoorLockTarget_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

도어 LOCK/UNLOCK을 이름으로 변환한다. 원 commandId·decisionSequence·hasRequestContext·과거 RequestContext·원 단말·origin·commandAgeMs·meta를 함께 보존한다. target Boot는 마지막 정상 BCM 기동과 승인된 수명 근거가 있어야 한다. COMMAND_ORIGIN_SYSTEM의 LOCAL/TEST 축약 및 200ms/1s 비교 경계는 미확정이므로 기본값을 넣지 않는다.

**후발 결과:** COMMAND: 최초 admission의 DOMAIN 생성 세대·commandId·원 RequestContext; 기존 3개 reason의 관측·시점 조건에 정확히 해당할 때만 필수 NetworkLocalFailure sink. 이미 전송 중인 오류는 최종 제출 실패로 축약하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxBcmDoorCommand(const DomainIf_BcmDoorCommand_t *command);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txBcmDoorCommand = NetCan_TxBcmDoorCommand;
```

의사코드 본문은 [BCM_DOOR_COMMAND의 기존 송신 설계](can/BCM_DOOR_COMMAND.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txBcmClimateCommand"></a>
### 2. `txBcmClimateCommand` → `BCM_CLIMATE_COMMAND`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxBcmClimateCommand( const DomainIf_BcmClimateCommand_t *command);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txBcmClimateCommand)( const DomainIf_BcmClimateCommand_t *command);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_BcmClimateCommand_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-bcmclimatecommand-t)이며 세부 Wire/Codec/수신 의무는 [BCM_CLIMATE_COMMAND](can/BCM_CLIMATE_COMMAND.md)다. 실제 함수 주소는 `NetCan_TxBcmClimateCommand` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `context` | `DomainIf_CommandContext_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `fanTarget` | `FanLevel_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalDirection` | `ThermalDirection_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalTargetLevelPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

팬 목표 OFF/LOW/MEDIUM/HIGH만 명령으로 허용한다. FAN_LEVEL_UNKNOWN은 보고용 값이며 목표로 변환하지 않는다. thermalDirection은 IDLE/COOL/HEAT 이름 대응, thermalTargetLevelPercent는 0..100이다. 원 CommandContext의 센서 meta.ageMs를 commandAgeMs로 사용하지 않는다.

**후발 결과:** COMMAND: 최초 admission의 DOMAIN 생성 세대·commandId·원 RequestContext; 기존 3개 reason의 관측·시점 조건에 정확히 해당할 때만 필수 NetworkLocalFailure sink. 이미 전송 중인 오류는 최종 제출 실패로 축약하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxBcmClimateCommand(const DomainIf_BcmClimateCommand_t *command);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txBcmClimateCommand = NetCan_TxBcmClimateCommand;
```

의사코드 본문은 [BCM_CLIMATE_COMMAND의 기존 송신 설계](can/BCM_CLIMATE_COMMAND.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txBcmInteriorLightCommand"></a>
### 3. `txBcmInteriorLightCommand` → `BCM_LIGHT_COMMAND`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxBcmInteriorLightCommand( const DomainIf_BcmInteriorLightCommand_t *command);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txBcmInteriorLightCommand)( const DomainIf_BcmInteriorLightCommand_t *command);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_BcmInteriorLightCommand_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-bcminteriorlightcommand-t)이며 세부 Wire/Codec/수신 의무는 [BCM_LIGHT_COMMAND](can/BCM_LIGHT_COMMAND.md)다. 실제 함수 주소는 `NetCan_TxBcmLightCommand` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `context` | `DomainIf_CommandContext_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `type` | `InteriorLightType_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `levelPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `color` | `RgbColor_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

InteriorLightType_t의 NORMAL/GOODBYE/WARNING/FAULT를 명시 변환하고 밝기 0..100, RGB 각 uint8_t 값을 보존한다. NORMAL을 선택했다는 이유만으로 실제 적용/피드백 성공을 만들지 않는다. 원 CommandContext 전체를 송신 항목과 함께 소유한다.

**후발 결과:** COMMAND: 최초 admission의 DOMAIN 생성 세대·commandId·원 RequestContext; 기존 3개 reason의 관측·시점 조건에 정확히 해당할 때만 필수 NetworkLocalFailure sink. 이미 전송 중인 오류는 최종 제출 실패로 축약하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxBcmLightCommand(const DomainIf_BcmInteriorLightCommand_t *command);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txBcmInteriorLightCommand = NetCan_TxBcmLightCommand;
```

의사코드 본문은 [BCM_LIGHT_COMMAND의 기존 송신 설계](can/BCM_LIGHT_COMMAND.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txCisPowerPermission"></a>
### 4. `txCisPowerPermission` → `DOMAIN_PERMISSION`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxCisPowerPermission( const DomainIf_CisPowerPermission_t *permission);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txCisPowerPermission)( const DomainIf_CisPowerPermission_t *permission);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_CisPowerPermission_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-cispowerpermission-t)이며 세부 Wire/Codec/수신 의무는 [DOMAIN_PERMISSION](can/DOMAIN_PERMISSION.md)다. 실제 함수 주소는 `NetDomain_TxCisPowerPermission` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `vehiclePowerAllowed` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `quality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

vehiclePowerAllowed와 quality 및 meta의 품질·AGE·updateSequence는 같은 원 성공 평가와 현재 품질에 연결한 입력이다. 원 성공 평가 번호와 CAN 합성 UPDATE_SEQUENCE는 다른 번호 공간이다. 이 3개 필드만으로 POWER_OPERATION_STATE·WINDOW 동작 허용·합성 revision·공유 AGE를 만들 수 없다. 원본 평가와 합성 계약이 없으면 부분 접수 성공도 반환하지 않는다.

r1 [Permission 의미 투영](../../interface/07_CROSS_NODE_INTERFACE_RULES.md#permission-delta)의 표는 **권장안 / DOMAIN 기능 정책 미확정**이다. 실제 채택·생성원 근거가 있는 경우 확정 ALLOW true/OK는1/OK, 정상 평가의 확정 DENY false/OK는0/OK로 구분한다. 확인된 미관측·만료·평가실패를 DOMAIN이 false+NO_DATA/STALE/INVALID로 투영한 경우에만255+해당 품질의 조건부 경로를 검토한다. 일반 false에서 UNKNOWN을 추측하지 않는다. UNKNOWN+OK를 무손실 보존해야 하면 기존 bool+quality+meta로 표현할 수 없으므로 해당 경로는 NOT_AVAILABLE/HOLD다. quality와 meta.quality가 충돌하면 임의 OK를 선택하지 않고 원 충돌을 보존한다. 정상 형식의 품질 저하를 인자오류나 DENY/OK로 덮지 않는다. 원 의미·원 품질·원인은 DOMAIN 원 기록 소유이며 Network에 새 사유 필드·기능 판단을 추가하지 않는다. 실제 세부 검사는 [합성 Flow](../flows/CAN_BCM_AND_PERMISSION.md#permission-projection)의 같은 조건을 따른다.

실패 즉시 UNKNOWN도 조건부 권장안이다. 성공17 뒤200ms 평가실패에서 원17·마지막 성공시각을 보존하며 성공18/AGE0을 만들지 않는다. 검증된 현재 제한은 같은 원 성공seq라는 이유만으로 중복 폐기하지 않는다. [번호 공간](../flows/CAN_BCM_AND_PERMISSION.md#permission-sequences)의 CAN 합성 UPDATE, CAN TX, UART LINK, CIS relay UPDATE를 원 성공번호와 구분한다. 실제 새 합성 revision 발급 규칙·필수 입력·공통 AGE는 미결이다. 성공 이력이 없으면 정상 원 순번·시간을 발명하지 않는다. 후방 원500ms/실제 재평가 약200ms와 CAN50/150·UART500/1500·후방거리150 후보·USE_LIMIT1500 후보/500 조건부는 각각 보존한다.

검증된 현재 DOMAIN 제한 사건은 원 소유자의 기존 기록에 귀속한다. 그 제한 이후 미시작 구 ALLOW의 pending/retry 재게시를 막는 관측 fence와 새 전송 항목의 원자 접수는 별개다. 용량 실패로 제한 전달이 실패해도 검증된 원 제한을 이전 ALLOW로 되돌리지 않는다. 검증 전 입력·malformed·일반 접수 실패로 fence를 만들지는 않는다. Driver 참조 중 프레임은 수정·임의취소·재사용하지 않으며 원격 CIS/Pi 동시 제한 반영을 보장하지 않는다. 전송 불능은 실제 실패로 남긴다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, D-02, G02, IF-O05, IF-O09, IF-O24, Q11. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION / BINDING_OPEN — 제품 함수가 아니다. */
DomainIf_Status_t NetDomain_TxCisPowerPermission(const DomainIf_CisPowerPermission_t *permission)
{
    if (permission == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (/* 선언 밖 enum/형식 오류. 정상 형식의 NO_DATA/STALE/INVALID 자체는 오류가 아님 */)
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (/* UNKNOWN+OK 무손실 필요, quality/meta 충돌 또는 실제 원인·DOMAIN 투영 채택 근거 부재 */)
        return DOMAIN_IF_NOT_AVAILABLE; // 원 품질·실패 근거 보존; 임의 DENY/OK 없음.
    // 아래 의미 매핑·실패 즉시 제한은 r1 조건부 권장안이다. 실제 채택 전 정상 경로는 열지 않는다.
    // owner 직렬 문맥·원 성공/현재 제한 결합·POWER_OPERATION_STATE 생성자·공유 AGE/합성순번의
    // 승인 근거와 기존 source별 사본·전체 게시 공간이 모두 있어야 한다.
    if (/* 어느 하나의 합성/등록/시간 근거가 없거나 해당 기능이 구현 보류임 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 전체 사본·원 평가 연결·게시 슬롯을 함께 예약할 수 없음 */)
        return DOMAIN_IF_QUEUE_FULL;
    DomainIf_CisPowerPermission_t local = {0};
    local.vehiclePowerAllowed = permission->vehiclePowerAllowed;
    local.quality = permission->quality;
    local.meta = permission->meta;
    // local.meta는 실제 원 성공 평가의 순번·시각/AGE를 보존한다. 실패 시17→18/AGE0 금지.
    // 채택된 의미 매핑에서만 true/OK→1/OK, 확정 false/OK→0/OK,
    // 확인된 원인의 false/nonOK→255/같은 품질. 합성 UPDATE는 별도 검증 대상이다.
    // 실제 현재 제한의 원기록 fence로 미시작 구ALLOW 재게시를 막되,
    // 이 local의 일부를 게시하거나 Queue 실패를 성공으로 반환하지 않는다.
    // local은 아직 지역 임시 사본이다. 여기서 OK를 반환하지 않는다.
    // 같은 원자 구간에서 기존 다른 입력의 원 평가·누적 AGE와 이 local을 결합한다.
    // 7개 NetDomainPermission 필드와 원본 연결을 완성한다. 누락을 0/OK로 채우지 않는다.
    // 기존 DomainNet_PublishPermission의 소유 사본/게시와 source 연결 보존을
    // 하나의 접수 경계로 확장할 실제 바인딩이 없으면 예약을 반환하고 NOT_AVAILABLE.
    // 모든 게시가 완료된 경우만 DOMAIN_IF_OK; 일부만 반영한 채 실패하지 않는다.
    return /* 위 원자 게시의 실제 DomainIf_Status_t 결과 */;
}
```

<a id="txWindowCommand"></a>
### 5. `txWindowCommand` → `WINDOW_COMMAND`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxWindowCommand( const DomainIf_WindowCommand_t *command);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txWindowCommand)( const DomainIf_WindowCommand_t *command);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_WindowCommand_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-windowcommand-t)이며 세부 Wire/Codec/수신 의무는 [WINDOW_COMMAND](can/WINDOW_COMMAND.md)다. 실제 함수 주소는 `NetCan_TxWindowCommand` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `context` | `DomainIf_CommandContext_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `channel` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `command` | `WindowCommand_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `targetPositionPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

channel·command·targetPositionPercent 및 원 CommandContext를 보존한다. 채널·VENT70/80·기능 실행 조건은 G05/IF-O07 구현 보류다. 문서에 원형이 있다는 이유로 정상 경로를 활성화하지 않는다.

**후발 결과:** COMMAND: 최초 admission의 DOMAIN 생성 세대·commandId·원 RequestContext; 기존 3개 reason의 관측·시점 조건에 정확히 해당할 때만 필수 NetworkLocalFailure sink. 이미 전송 중인 오류는 최종 제출 실패로 축약하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12, G05, IF-O07, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxWindowCommand(const DomainIf_WindowCommand_t *command);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txWindowCommand = NetCan_TxWindowCommand;
```

의사코드 본문은 [WINDOW_COMMAND의 기존 송신 설계](can/WINDOW_COMMAND.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txWindowOperationPermission"></a>
### 6. `txWindowOperationPermission` → `DOMAIN_PERMISSION`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxWindowOperationPermission( const DomainIf_WindowOperationPermission_t *permission);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txWindowOperationPermission)( const DomainIf_WindowOperationPermission_t *permission);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_WindowOperationPermission_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-windowoperationpermission-t)이며 세부 Wire/Codec/수신 의무는 [DOMAIN_PERMISSION](can/DOMAIN_PERMISSION.md)다. 실제 함수 주소는 `NetDomain_TxWindowOperationPermission` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `operationAllowed` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `quality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

operationAllowed·quality·meta를 별도 원 평가로 보존한다. 새 power 입력이 기존 operation AGE를 0으로 갱신하지 않는다. WINDOW 기능 보류와 shared DOMAIN_PERMISSION 합성 미결을 유지한다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, D-02, G02, G05, IF-O05, IF-O07, IF-O09, IF-O24, Q11. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION / BINDING_OPEN — 제품 함수가 아니다. */
DomainIf_Status_t NetDomain_TxWindowOperationPermission(const DomainIf_WindowOperationPermission_t *permission)
{
    if (permission == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (/* 아래 source 선언의 활성 품질·필드 조합이 잘못됨 */)
        return DOMAIN_IF_INVALID_ARGUMENT;
    // owner 직렬 문맥·동일 평가 결합·POWER_OPERATION_STATE 생성자·공유 AGE/순번의
    // 승인 근거와 기존 source별 사본·전체 게시 공간이 모두 있어야 한다.
    if (/* 어느 하나의 합성/등록/시간 근거가 없거나 해당 기능이 구현 보류임 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 전체 사본·원 평가 연결·게시 슬롯을 함께 예약할 수 없음 */)
        return DOMAIN_IF_QUEUE_FULL;
    DomainIf_WindowOperationPermission_t local = {0};
    local.operationAllowed = permission->operationAllowed;
    local.quality = permission->quality;
    local.meta = permission->meta;
    // local은 아직 지역 임시 사본이다. 여기서 OK를 반환하지 않는다.
    // 같은 원자 구간에서 기존 다른 입력의 원 평가·누적 AGE와 이 local을 결합한다.
    // 7개 NetDomainPermission 필드와 원본 연결을 완성한다. 누락을 0/OK로 채우지 않는다.
    // 기존 DomainNet_PublishPermission의 소유 사본/게시와 source 연결 보존을
    // 하나의 접수 경계로 확장할 실제 바인딩이 없으면 예약을 반환하고 NOT_AVAILABLE.
    // 모든 게시가 완료된 경우만 DOMAIN_IF_OK; 일부만 반영한 채 실패하지 않는다.
    return /* 위 원자 게시의 실제 DomainIf_Status_t 결과 */;
}
```

<a id="txWindowDomainAlive"></a>
### 7. `txWindowDomainAlive` → `DOMAIN_PERMISSION`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxWindowDomainAlive( const DomainIf_WindowDomainAlive_t *alive);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txWindowDomainAlive)( const DomainIf_WindowDomainAlive_t *alive);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_WindowDomainAlive_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-windowdomainalive-t)이며 세부 Wire/Codec/수신 의무는 [DOMAIN_PERMISSION](can/DOMAIN_PERMISSION.md)다. 실제 함수 주소는 `NetDomain_TxWindowDomainAlive` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `domainLinkAvailable` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

DOMAIN 기능 Owner가 원 alive 평가와 meta를 생성하고 Network TxPort가 복사·보존한다. WINDOW는 조건부 수신 노드이며 실행 HOLD를 유지한다. Alive 입력은 차량 허용/새 heartbeat frame이 아니다. 통합 Permission 및 기존 수신 감독과의 관계·실제 생산자 바인딩은 확인 전 HOLD하며, Interface 타입 표의 WINDOW 생산자 표기는 S6 이후 협의 기록으로 보존한다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, D-02, G05, IF-O05, IF-O07, IF-O09, IF-O24, Q11. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION / BINDING_OPEN — 제품 함수가 아니다. */
DomainIf_Status_t NetDomain_TxWindowDomainAlive(const DomainIf_WindowDomainAlive_t *alive)
{
    if (alive == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (/* 아래 source 선언의 활성 품질·필드 조합이 잘못됨 */)
        return DOMAIN_IF_INVALID_ARGUMENT;
    // owner 직렬 문맥·동일 평가 결합·POWER_OPERATION_STATE 생성자·공유 AGE/순번의
    // 승인 근거와 기존 source별 사본·전체 게시 공간이 모두 있어야 한다.
    if (/* 어느 하나의 합성/등록/시간 근거가 없거나 해당 기능이 구현 보류임 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 전체 사본·원 평가 연결·게시 슬롯을 함께 예약할 수 없음 */)
        return DOMAIN_IF_QUEUE_FULL;
    DomainIf_WindowDomainAlive_t local = {0};
    local.domainLinkAvailable = alive->domainLinkAvailable;
    local.meta = alive->meta;
    // local은 아직 지역 임시 사본이다. 여기서 OK를 반환하지 않는다.
    // 같은 원자 구간에서 기존 다른 입력의 원 평가·누적 AGE와 이 local을 결합한다.
    // 7개 NetDomainPermission 필드와 원본 연결을 완성한다. 누락을 0/OK로 채우지 않는다.
    // 기존 DomainNet_PublishPermission의 소유 사본/게시와 source 연결 보존을
    // 하나의 접수 경계로 확장할 실제 바인딩이 없으면 예약을 반환하고 NOT_AVAILABLE.
    // 모든 게시가 완료된 경우만 DOMAIN_IF_OK; 일부만 반영한 채 실패하지 않는다.
    return /* 위 원자 게시의 실제 DomainIf_Status_t 결과 */;
}
```

<a id="txVssOneShotEvent"></a>
### 8. `txVssOneShotEvent` → `VSS_EVENT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxVssOneShotEvent( const DomainIf_VssOneShotEvent_t *event);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txVssOneShotEvent)( const DomainIf_VssOneShotEvent_t *event);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_VssOneShotEvent_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-vssoneshotevent-t)이며 세부 Wire/Codec/수신 의무는 [VSS_EVENT](can/VSS_EVENT.md)다. 실제 함수 주소는 `NetCan_TxVssEvent` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `event` | `VssOneShotEvent_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `availability` | `FunctionAvailability_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasRelatedCommand` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `relatedCommandId` | `DomainCommandId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

event·occurrenceId·원 meta.quality/ageMs·availability·hasRelatedCommand·relatedCommandId를 보존한다. meta.updateSequence는 VSS_EVENT wire와 occurrence 중복 판정에 사용하지 않지만 원 사본에서 0으로 바꾸지 않는다. hasRelatedCommand=false일 때 stale relatedCommandId를 해석하지 않는다. UNSUPPORTED 가용성을 정상 raw로 축약하지 않는다.

**후발 결과:** OCCURRENCE: 원 occurrence 생성 세대·occurrenceId·실제 존재하는 related Command; 결합 근거 없는 가짜 request를 추가하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxVssEvent(const DomainIf_VssOneShotEvent_t *event);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txVssOneShotEvent = NetCan_TxVssEvent;
```

의사코드 본문은 [VSS_EVENT의 기존 송신 설계](can/VSS_EVENT.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txVssWarningState"></a>
### 9. `txVssWarningState` → `VSS_WARNING_STATE`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxVssWarningState( const DomainIf_VssWarningState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txVssWarningState)( const DomainIf_VssWarningState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_VssWarningState_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-vsswarningstate-t)이며 세부 Wire/Codec/수신 의무는 [VSS_WARNING_STATE](can/VSS_WARNING_STATE.md)다. 실제 함수 주소는 `NetCan_TxVssWarningState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `windowAntiPinchState` | `AntiPinchState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `antiPinchEvidence` | `DomainIf_WarningInputEvidence_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occupantHazardState` | `OccupantHazardState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occupantEvidence` | `DomainIf_WarningInputEvidence_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearObstacleState` | `RearObstacleState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearEvidence` | `DomainIf_WarningInputEvidence_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearDetectionActivation` | `RearDetectionActivation_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearActivationMeta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearActivationReason` | `QualityReason_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

antiPinch/occupant/rear 3개 판단과 각 quality/reason/availability/source.generation/sequence/ageMs를 독립 보존한다. rearDetectionActivation·rearActivationMeta·rearActivationReason은 별도 DOMAIN 정책 평가다. CF02 used51/60 및 양단 선택 전 전송 보류이며 UNCONFIRMED/UNSUPPORTED를 CLEAR/AVAILABLE로 바꾸지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, CF02, IF-O09, IF-O24, IF-O01, IF-O02, IF-O06, IF-O17. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* 기존 B12 PROPOSED_PRIVATE_FUNCTION. 본체는 연결한 leaf 정본을 재사용한다. */
DomainIf_Status_t NetCan_TxVssWarningState(const DomainIf_VssWarningState_t *state);
/* 설명용 등록 연결: 실제 SetTxPort 호출은 등록 문서의 조건을 충족한 뒤만. */
// candidatePort.txVssWarningState = NetCan_TxVssWarningState;
```

의사코드 본문은 [VSS_WARNING_STATE의 기존 송신 설계](can/VSS_WARNING_STATE.md)에 있다. 전체 source 사본/시간/identity 확보 → 명시 투영 → bounded 게시 → C2 반환 순서를 그대로 사용한다. Network가 `DomainIf_Tx*`를 다시 호출하여 순환하거나 중복 전송하지 않는다.

<a id="txMobileRequestResult"></a>
### 10. `txMobileRequestResult` → `M_RESULT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileRequestResult( const DomainIf_MobileRequestResult_t *result);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileRequestResult)( const DomainIf_MobileRequestResult_t *result);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_MobileRequestResult_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-mobilerequestresult-t)이며 세부 Wire/Codec/수신 의무는 [M_RESULT](uart_m/M_RESULT.md)다. 실제 함수 주소는 `NetDomain_TxMobileRequestResult` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `deviceContextId` | `DeviceContextId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `resultInfo` | `RequestResultInfo_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasVehicleResult` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `target` | `DomainIf_MobileResultTarget_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `commandId` | `DomainCommandId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `origin` | `CommandOrigin_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `resultAgeMs` | `uint32_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

resultInfo.context의 원 RequestSession(uint64_t)/RequestID와 deviceContextId를 보존한다. 현재 recipient Session은 별도 envelope다. hasVehicleResult=false이면 resultInfo.result를 검사해 DONE/FAILED를 만들지 않는다. setting-only commandId=0 의미·origin·resultAgeMs를 유지하며 같은 원 결과를 최신 상태로 덮지 않는다.

**후발 결과:** REQUEST_RESULT: publication 생성 세대·원 단말·과거 RequestContext 및 실제 존재하는 commandId; 차량 result 자체는 변경하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileRequestResult(const DomainIf_MobileRequestResult_t *result)
{
    if (result == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileRequestResult(result, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileRequestResult`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_RESULT 정본](uart_m/M_RESULT.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileBcmDoorState"></a>
### 11. `txMobileBcmDoorState` → `M_BCM_DOOR_STATE`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileBcmDoorState(const DoorState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileBcmDoorState)(const DoorState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DoorState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-doorstate-t)이며 세부 Wire/Codec/수신 의무는 [M_BCM_DOOR_STATE](uart_m/M_BCM_DOOR_STATE.md)다. 실제 함수 주소는 `NetDomain_TxMobileBcmDoorState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `lockState` | `DoorLockState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `openState` | `DoorOpenState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `compositeState` | `DoorCompositeState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lockQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `openQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `compositeQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

lock/open/composite와 각 독립 품질, sourceGeneration, meta를 전부 복사한다. 같은 값이더라도 다른 원 BOOT/TX/관측을 붙이지 않는다. 현재 목표 Command나 동일 잠금값만으로 실제 도어 실행 완료를 만들지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileBcmDoorState(const DoorState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileBcmDoorState(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileBcmDoorState`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_BCM_DOOR_STATE 정본](uart_m/M_BCM_DOOR_STATE.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileBcmClimateState"></a>
### 12. `txMobileBcmClimateState` → `M_BCM_CLIMATE_STATE`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileBcmClimateState(const ClimateState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileBcmClimateState)(const ClimateState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [ClimateState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-climatestate-t)이며 세부 Wire/Codec/수신 의무는 [M_BCM_CLIMATE_STATE](uart_m/M_BCM_CLIMATE_STATE.md)다. 실제 함수 주소는 `NetDomain_TxMobileBcmClimateState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `commandedFanLevel` | `FanLevel_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `measuredFanLevel` | `FanLevel_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalDirection` | `ThermalDirection_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalOutputLevelPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `heatRemovalState` | `HeatRemovalState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `commandedFanQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `measuredFanQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalDirectionQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `thermalOutputQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `heatRemovalQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 같은 원 생산 세대를 전체 사본에 보존; CAN Boot에서 임의 생성하지 않음 |

commanded/measured fan, thermal direction/output, heat removal 및5개 품질·meta·sourceGeneration을 모두 보존한다. Vehicle v0.3 후보의12개 멤버를 동일 버전의 C2 사본으로 취급하며 실제 생산 세대와 원 CAN/C2 commit 연결을 확인한다. CAN Boot 자동 복사·품질 저하에 따른 generation 0 초기화는 금지하고 generation 0 수용 정책은 미결이다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileBcmClimateState(const ClimateState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 sourceGeneration을 포함한 전체 source·recipient·원 시간/identity를 안정 보관한다.
    // 검증된 생산 세대 연결·generation 0 수용 근거가 없으면 NOT_READY이며 raw로 보충하지 않는다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileBcmClimateState(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileBcmClimateState`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_BCM_CLIMATE_STATE 정본](uart_m/M_BCM_CLIMATE_STATE.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileBcmInteriorLightState"></a>
### 13. `txMobileBcmInteriorLightState` → `M_BCM_LIGHT_STATE`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileBcmInteriorLightState(const InteriorLightState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileBcmInteriorLightState)(const InteriorLightState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [InteriorLightState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-interiorlightstate-t)이며 세부 Wire/Codec/수신 의무는 [M_BCM_LIGHT_STATE](uart_m/M_BCM_LIGHT_STATE.md)다. 실제 함수 주소는 `NetDomain_TxMobileBcmInteriorLightState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `type` | `InteriorLightType_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `levelPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `color` | `RgbColor_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `applyState` | `InteriorLightApplyState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `physicalFeedbackSupported` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `typeQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `levelQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `colorQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `applyQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 같은 원 생산 세대를 전체 사본에 보존; CAN Boot에서 임의 생성하지 않음 |

type/level/color/applyState, physicalFeedbackSupported와4개 품질·meta·sourceGeneration의 후보11개 멤버를 모두 보존한다. 지시 적용과 물리 피드백은 별개다. 실제 생산 세대와 같은 원 CAN/C2 commit 연결을 확인하며 품질 저하로 알려진 세대를0으로 지우지 않는다. generation 0 수용 정책은 미결이다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileBcmInteriorLightState(const InteriorLightState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 sourceGeneration을 포함한 전체 source·recipient·원 시간/identity를 안정 보관한다.
    // 검증된 생산 세대 연결·generation 0 수용 근거가 없으면 NOT_READY이며 raw로 보충하지 않는다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileBcmInteriorLightState(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileBcmInteriorLightState`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_BCM_LIGHT_STATE 정본](uart_m/M_BCM_LIGHT_STATE.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileBcmStatus"></a>
### 14. `txMobileBcmStatus` → `M_BCM_STATUS`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileBcmStatus)(const DomainIf_BcmStatusReport_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_BcmStatusReport_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-bcmstatusreport-t)이며 세부 Wire/Codec/수신 의무는 [M_BCM_STATUS](uart_m/M_BCM_STATUS.md)다. 실제 함수 주소는 `NetDomain_TxMobileBcmStatus` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `ecuState` | `CommonEcuState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `faultMask` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `activeFault` | `FaultInfo_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `affectedFunctionMask` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `recovering` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

ecuState/faultMask/activeFault/affectedFunctionMask/recovering/sourceGeneration/meta를 보존한다. activeFault.faultCode=0인 경우 category/state/quality는 소비하지 않는다. raw NONE=0과 C SENSOR=0을 cast로 연결하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileBcmStatus(const DomainIf_BcmStatusReport_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileBcmStatus(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileBcmStatus`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_BCM_STATUS 정본](uart_m/M_BCM_STATUS.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileCisEnvironment"></a>
### 15. `txMobileCisEnvironment` → `M_CIS_ENVIRONMENT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileCisEnvironment)(const CisCabinEnvironmentState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [CisCabinEnvironmentState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-ciscabinenvironmentstate-t)이며 세부 Wire/Codec/수신 의무는 [M_CIS_ENVIRONMENT](uart_m/M_CIS_ENVIRONMENT.md)다. 실제 함수 주소는 `NetDomain_TxMobileCisEnvironment` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `cabinTemperature` | `int16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `temperatureQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `cabinHumidity` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `humidityQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `cabinIlluminance` | `uint32_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `illuminanceQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

온도(int16_t centi-°C)·습도(uint16_t)·조도(uint32_t)와 각 ValueQuality_t를 독립 보존한다. ValueQuality_t의 validity/reason/meta 전체를 복사한다. 온습도 각2500ms 만료·수신 중단·Boot 변경의 현재 품질은 [원 raw·로컬 사건·Core/Getter 연결](DOMAIN_QUERY_PROVENANCE.md#association)로 확인하며 raw GOOD로 복구하지 않는다. r1 [조건부 Boot 저장 순서](../../interface/07_CROSS_NODE_INTERFACE_RULES.md#environment-delta)는 구 A 미실행 입력 차단→실행 중 callback 및 후속 비동기 쓰기의 실제 종료/향후 commit 불가→기존 C2 NO_DATA→VSM·독립 Meta의 실제 무효화 저장 완료→새 B 원seq0 순서다. Queue 접수·callback 반환·DOMAIN_IF_OK는 저장 완료가 아니다. 완료 미확인/무효화 거부이면 B 정상 투영을 차단하되 과거 OK가 이미 제거됐다고 주장하지 않는다. Core commit이 확인되고 optional Observer만 실패했다면 같은 저장을 재실행하지 않는다. 새 B 대기 AGE는 한 번 누적하며 온습도2500ms 도달부터 정상복원하지 않는다. 일반 NO_DATA를 Boot/reset 신호로 쓰거나 A900 뒤 B0을901로 바꾸지 않는다. [BG-R05-BOOT](can/CIS_ENVIRONMENT.md#boot-binding-gap)는 r1 조건부 설계안이 있어도 실제 DOMAIN 저장 경계·낮은seq 수용 근거가 없어 BLOCKED_DESIGN으로 유지한다. 환경 generation 필드는 추가하지 않고 조도 TTL·실제 바인딩은 미결이다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileCisEnvironment(const CisCabinEnvironmentState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileCisEnvironment(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileCisEnvironment`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_CIS_ENVIRONMENT 정본](uart_m/M_CIS_ENVIRONMENT.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileCisOccupant"></a>
### 16. `txMobileCisOccupant` → `M_CIS_OCCUPANT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileCisOccupant(const CisOccupantState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileCisOccupant)(const CisOccupantState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [CisOccupantState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-cisoccupantstate-t)이며 세부 Wire/Codec/수신 의무는 [M_CIS_OCCUPANT](uart_m/M_CIS_OCCUPANT.md)다. 실제 함수 주소는 `NetDomain_TxMobileCisOccupant` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `occupantPresent` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occupantCount` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `presenceQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `countQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `visionFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

occupantPresent/count와 presenceQuality/countQuality를 독립 복사한다. sourceGeneration·visionFunctionStatus와 실제 원 PiBoot/VisionSeq/AGE의 연결을 따로 확인한다. Pi 품질과 CIS 추가 제한 사유를 정상/0으로 합치지 않는다. [NetCanOccupantRestrictionEvidence](../data/CAN_NODE_DATA.md#type-NetCanOccupantRestrictionEvidence)의 같은 관측 제한이 실제 Core/Getter에 반영된 근거를 보존하고 과거 raw VALID로 되돌리지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O13, IF-O24, IF-O19. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileCisOccupant(const CisOccupantState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileCisOccupant(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileCisOccupant`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_CIS_OCCUPANT 정본](uart_m/M_CIS_OCCUPANT.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileCisRear"></a>
### 17. `txMobileCisRear` → `M_CIS_REAR`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileCisRear(const CisRearState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileCisRear)(const CisRearState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [CisRearState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-cisrearstate-t)이며 세부 Wire/Codec/수신 의무는 [M_CIS_REAR](uart_m/M_CIS_REAR.md)다. 실제 함수 주소는 `NetDomain_TxMobileCisRear` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `rearDistanceMm` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `measurementState` | `RearMeasurementState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `distanceQuality` | `ValueQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

rearDistanceMm·measurementState·distanceQuality·sourceGeneration을 보존한다. 원 NetCisRear/CIS_REAR evidence의 실제 필드와 단위 대응을 기존 Codec로 확인하며 같은 값의 다른 관측을 결합하지 않는다. 측정 불가 sentinel을 정상 거리로 만들지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileCisRear(const CisRearState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileCisRear(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileCisRear`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_CIS_REAR 정본](uart_m/M_CIS_REAR.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileCisStatus"></a>
### 18. `txMobileCisStatus` → `M_CIS_STATUS`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileCisStatus)(const DomainIf_CisStatusReport_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_CisStatusReport_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-cisstatusreport-t)이며 세부 Wire/Codec/수신 의무는 [M_CIS_STATUS](uart_m/M_CIS_STATUS.md)다. 실제 함수 주소는 `NetDomain_TxMobileCisStatus` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `ecuState` | `CisState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occupantFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `temperatureFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `humidityFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `illuminanceFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `rearFunctionStatus` | `CisFunctionStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `piUartStatus` | `DomainIf_CisInterfaceStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `canInterfaceStatus` | `DomainIf_CisInterfaceStatus_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `faultMask` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `activeFaultCode` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `recovering` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastFaultCode` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastFaultOccurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `affectedFunctionMask` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

ECU/5개 기능 상태·Pi UART/CAN interface 상태·faultMask/activeFaultCode/recovering/lastFaultCode/lastFaultOccurrenceId/affectedFunctionMask/sourceGeneration/meta를 모두 복사한다. ACTIVE=0은 LAST_FAULT 삭제 근거가 아니며 source에 없는 DEGRADED를 READY로 정상화하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileCisStatus(const DomainIf_CisStatusReport_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileCisStatus(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileCisStatus`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_CIS_STATUS 정본](uart_m/M_CIS_STATUS.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileVssStatus"></a>
### 19. `txMobileVssStatus` → `M_VSS_STATUS`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileVssStatus(const DomainIf_VssStateReport_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileVssStatus)(const DomainIf_VssStateReport_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_VssStateReport_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-vssstatereport-t)이며 세부 Wire/Codec/수신 의무는 [M_VSS_STATUS](uart_m/M_VSS_STATUS.md)다. 실제 함수 주소는 `NetDomain_TxMobileVssStatus` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `service` | `VssServiceState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `faultMask` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastFaultCode` | `uint16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastFaultOccurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastFaultAgeMs` | `uint32_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `faultQuality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

service 전체·faultMask/lastFaultCode/lastFaultOccurrenceId/lastFaultAgeMs/faultQuality/sourceGeneration을 보존한다. R4/R6→DTO Owner binding과 같은 원 CAN evidence의 결합은 별도 미결이다. 기능 재생 DONE을 Network가 생성하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O06, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileVssStatus(const DomainIf_VssStateReport_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileVssStatus(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileVssStatus`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_VSS_STATUS 정본](uart_m/M_VSS_STATUS.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileWindowState"></a>
### 20. `txMobileWindowState` → `M_WINDOW_STATE`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileWindowState(const WindowState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileWindowState)(const WindowState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [WindowState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-windowstate-t)이며 세부 Wire/Codec/수신 의무는 [M_WINDOW_STATE](uart_m/M_WINDOW_STATE.md)다. 실제 함수 주소는 `NetDomain_TxMobileWindowState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `channel` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `motionState` | `WindowMotionState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `ecuState` | `CommonEcuState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `positionPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `fullyOpen` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `fullyClosed` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `antiPinchState` | `AntiPinchState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

channel/motionState/ecuState/positionPercent/fullyOpen/fullyClosed/antiPinchState/meta/sourceGeneration을 보존한다. WINDOW 기능 경로는 구현 보류이며 임의 가상 정상 상태를 UART로 보내지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, G05, IF-O07, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileWindowState(const WindowState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileWindowState(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileWindowState`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_WINDOW_STATE 정본](uart_m/M_WINDOW_STATE.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileWindowFault"></a>
### 21. `txMobileWindowFault` → `M_WINDOW_FAULT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileWindowFault(const DomainIf_WindowFaultState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileWindowFault)(const DomainIf_WindowFaultState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_WindowFaultState_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-windowfaultstate-t)이며 세부 Wire/Codec/수신 의무는 [M_WINDOW_FAULT](uart_m/M_WINDOW_FAULT.md)다. 실제 함수 주소는 `NetDomain_TxMobileWindowFault` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `fault` | `FaultInfo_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `sourceGeneration` | `SourceGeneration_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

fault·occurrenceId·meta·sourceGeneration 전체를 유지한다. 좁은 FaultInfo_t만 저장해 원 사건을 잃지 않는다. WINDOW 기능 구현 보류를 해제하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12, G05, IF-O07, IF-O09, IF-O24. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileWindowFault(const DomainIf_WindowFaultState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileWindowFault(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileWindowFault`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_WINDOW_FAULT 정본](uart_m/M_WINDOW_FAULT.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileWarning"></a>
### 22. `txMobileWarning` → `M_WARNING`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileWarning( const DomainIf_WarningInfo_t *warning);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileWarning)( const DomainIf_WarningInfo_t *warning);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_WarningInfo_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-warninginfo-t)이며 세부 Wire/Codec/수신 의무는 [M_WARNING](uart_m/M_WARNING.md)다. 실제 함수 주소는 `NetDomain_TxMobileWarning` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `type` | `DomainWarningType_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `severity` | `DomainWarningSeverity_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `state` | `DomainWarningState_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `isRead` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `simulated` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `quality` | `DataQuality_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `qualityReason` | `QualityReason_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `source` | `SourceEvidence_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

type/severity/state/occurrenceId/isRead/simulated/quality/qualityReason/source를 모두 보존한다. isRead는 위험 CLEAR가 아니다. UNCONFIRMED 및 simulated 등의 현재 raw 미표현 정보는 전송 제한이며 정상 경고로 손실 export하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileWarning(const DomainIf_WarningInfo_t *warning)
{
    if (warning == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileWarning(warning, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileWarning`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_WARNING 정본](uart_m/M_WARNING.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileFunctionAvailability"></a>
### 23. `txMobileFunctionAvailability` → `M_AVAILABILITY`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileFunctionAvailability( const DomainIf_FunctionAvailabilitySet_t *availability);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileFunctionAvailability)( const DomainIf_FunctionAvailabilitySet_t *availability);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_FunctionAvailabilitySet_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-functionavailabilityset-t)이며 세부 Wire/Codec/수신 의무는 [M_AVAILABILITY](uart_m/M_AVAILABILITY.md)다. 실제 함수 주소는 `NetDomain_TxMobileFunctionAvailability` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `items[DOMAIN_FUNCTION_COUNT]` | `DomainIf_FunctionAvailability_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

items[DOMAIN_FUNCTION_COUNT]는 내부 포인터가 아닌 고정 배열이다. 9개 각 functionId/availability/reason과 meta를 모두 복사하고 ID 중복/누락·COUNT 센티널·부적합 enum을 거부한다. C2 WINDOW UNIMPLEMENTED/UNSUPPORTED 의미를 AVAILABLE로 채우지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileFunctionAvailability(const DomainIf_FunctionAvailabilitySet_t *availability)
{
    if (availability == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileFunctionAvailability(availability, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileFunctionAvailability`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_AVAILABILITY 정본](uart_m/M_AVAILABILITY.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileDigitalKeyState"></a>
### 24. `txMobileDigitalKeyState` → `M_DIGITAL_STATUS`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileDigitalKeyState( const DigitalKeyState_t *state);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileDigitalKeyState)( const DigitalKeyState_t *state);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DigitalKeyState_t](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md#type-digitalkeystate-t)이며 세부 Wire/Codec/수신 의무는 [M_DIGITAL_STATUS](uart_m/M_DIGITAL_STATUS.md)다. 실제 함수 주소는 `NetDomain_TxMobileDigitalKeyState` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `setting` | `DigitalKeySetting_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `availability` | `FunctionAvailability_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `lastUnlockOrigin` | `UnlockOrigin_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `reason` | `ResultReason_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

setting/availability/lastUnlockOrigin/reason/meta를 보존한다. 원 가용성/미지원 사유가 raw에서 표현되지 않으면 해당 전송을 제한한다. 새 Session 발급이나 차량 잠금 해제 판단은 수행하지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileDigitalKeyState(const DigitalKeyState_t *state)
{
    if (state == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileDigitalKeyState(state, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileDigitalKeyState`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_DIGITAL_STATUS 정본](uart_m/M_DIGITAL_STATUS.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileDigitalKeyResult"></a>
### 25. `txMobileDigitalKeyResult` → `M_DIGITAL_RESULT`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileDigitalKeyResult( const DomainIf_DigitalKeyResult_t *result);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileDigitalKeyResult)( const DomainIf_DigitalKeyResult_t *result);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_DigitalKeyResult_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-digitalkeyresult-t)이며 세부 Wire/Codec/수신 의무는 [M_DIGITAL_RESULT](uart_m/M_DIGITAL_RESULT.md)다. 실제 함수 주소는 `NetDomain_TxMobileDigitalKeyResult` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `hasVehicleResult` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `result` | `RequestResult_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `reason` | `ResultReason_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `confirmation` | `ResultConfirmation_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `origin` | `UnlockOrigin_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `relatedDoorCommandId` | `DomainCommandId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `occurrenceId` | `OccurrenceId_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `resultAgeMs` | `uint32_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `actualUnlockTransition` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

hasVehicleResult/result/reason/confirmation/origin/relatedDoorCommandId/occurrenceId/resultAgeMs/actualUnlockTransition을 보존한다. ALREADY_AT_TARGET+DONE은 실제 unlock transition이 아니다. 자동 해제 occurrence와 원 요청/결과 identity 연결 미확인은 새 Session으로 보충하지 않는다.

**후발 결과:** OCCURRENCE: 원 occurrence 생성 세대·occurrenceId·실제 존재하는 related Command; 결합 근거 없는 가짜 request를 추가하지 않음.

**관련 HOLD:** D-03, D-05, G06, IF-O12, IF-O11. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileDigitalKeyResult(const DomainIf_DigitalKeyResult_t *result)
{
    if (result == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileDigitalKeyResult(result, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileDigitalKeyResult`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_DIGITAL_RESULT 정본](uart_m/M_DIGITAL_RESULT.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="txMobileUserSettings"></a>
### 26. `txMobileUserSettings` → `M_USER_SETTINGS`

```c
/* DECLARED_SOURCE: 공개 본체는 DOMAIN 소유 */
DomainIf_Status_t DomainIf_TxMobileUserSettings( const DomainIf_UserSettingsState_t *settings);
/* DECLARED_SOURCE: DomainIf_TxPort_t의 Network callback 멤버 */
DomainIf_Status_t (*txMobileUserSettings)( const DomainIf_UserSettingsState_t *settings);
```

공개 Wrapper가 이 멤버를 한 번 호출한다. source DTO 정본은 [DomainIf_UserSettingsState_t](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md#type-domainif-usersettingsstate-t)이며 세부 Wire/Codec/수신 의무는 [M_USER_SETTINGS](uart_m/M_USER_SETTINGS.md)다. 실제 함수 주소는 `NetDomain_TxMobileUserSettings` 후보이며 본체·등록·Driver 연결은 `BINDING_OPEN`이다.

| source 멤버 | 정확한 C 타입 | 복사·수명 |
|---|---|---|
| `hasTargetTemperature` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `targetTemperatureCentiC` | `int16_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasClimateAutoSetting` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `climateAutoEnabled` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasUserFanLevel` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `userFanLevel` | `FanLevel_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasInteriorLightEnableSetting` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `interiorLightEnabled` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasNormalLightLevel` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `normalLightLevelPercent` | `uint8_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `hasNormalLightColor` | `bool` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `normalLightColor` | `RgbColor_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |
| `meta` | `SignalMeta_t` | 활성 값과 중첩 자료를 접수 항목에 값 복사; caller 반환 후 주소 사용 금지 |

13개 선언 멤버를 복사하되 각 has*가 false인 값은 읽거나 정상 기본값으로 export하지 않는다. 온도·fan·light level/color 부분 설정의 raw 표현 한계는 기존 M_USER_SETTINGS대로 유지한다. 별도 설정 cache/정책은 만들지 않는다.

**후발 결과:** 일반 상태/Permission은 periodic loss를 개별 NetworkLocalFailure로 만들지 않음. 원 항목의 Driver terminal/release와 M09 계측은 별도로 유지.

**관련 HOLD:** D-03, D-05, G06, IF-O12. C2 v0.6/Vehicle v0.3 후보의 제품 채택·ABI·실제 바인딩 승인은 미완료다.

```c
/* PROPOSED_PRIVATE_FUNCTION: C2 멤버와 동일한 반환/인자. */
DomainIf_Status_t NetDomain_TxMobileUserSettings(const DomainIf_UserSettingsState_t *settings)
{
    if (settings == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    // 현재 owner 문맥/등록/해당 route 준비와 아래 leaf의 전체 활성 필드 검증.
    // 실제 근거가 없으면 호출 전에 NOT_AVAILABLE, 잘못된 source는 INVALID_ARGUMENT.
    // 기존 submitter가 전체 source·recipient·원 시간/identity를 안정 보관한다.
    NetSubmitResult submitted = DomainNetPriv_SubmitMobileUserSettings(settings, NULL);
    return NetDomain_MapSubmitStatus(submitted);
}
```

`DomainNetPriv_SubmitMobileUserSettings`의 정확한 2인자 원형·전체 바이트 투영·선행 조건·실패는 [M_USER_SETTINGS 정본](uart_m/M_USER_SETTINGS.md)을 적용한다. 별도 중간 DTO나 Manager cache를 추가하지 않는다. Queue/Driver/late failure 관측은 [공통 수명](NETWORK_SERVICE_LIFETIME.md)으로 연결된다.

<a id="counterexamples"></a>
## 자체검수 반례

| ID | 반례 | 필요한 결과 |
|---|---|---|
| B15-TX-01 | 콜백 OK 뒤 caller 지역 객체 변경/소멸 | 자기 소유 활성 필드·배열·원 문맥은 불변; caller 주소를 다시 읽지 않는다. |
| B15-TX-02 | 복사할 공간은 있으나 Queue 게시 슬롯 부족 | QUEUE_FULL; 새 항목 부분 접수와 기존 discrete 항목 덮어쓰기 없음. |
| B15-TX-03 | NET_SUBMIT_NOT_READY=2 단순 cast | 금지; NOT_AVAILABLE로 switch 대응한다. C2 NOT_INITIALIZED와 합치지 않는다. |
| B15-TX-04 | OK 후 Driver 최종 제출 실패가 확정 | 최초 admission의 실제 identity로 필수 sink에 한 번; 이미 전송 중 오류는 이 reason으로 바꾸지 않고 차량 FAILED/DONE 또는 과거 OK 변경 없음. |
| B15-TX-05 | Driver release가 terminal보다 먼저 | 저장소 해제 가능 사실과 결과 미결을 분리; 새 버퍼/빈 슬롯으로 같은 원 항목 재제출 금지. |
| B15-TX-06 | 구 Boot의 Driver callback이 새 항목 슬롯을 지칭 | epoch/submission/identity 불일치로 격리; 새 버퍼 회수/새 명령 실패 귀속 금지. |
| B15-TX-07 | Permission power만 새 평가, operation은 오래됨 | operation AGE·순번을 갱신하지 않으며 전체 합성 계약이 없으면 NOT_AVAILABLE. |
| B15-TX-08 | WindowAlive true만으로 신규 heartbeat/차량 power 허용 생성 | 금지. 기존 통합 Permission 연결 미결 유지. |
| B15-TX-09 | M_RESULT 원 RequestSession=A, 현재 recipient=B | 원 A와 전달 envelope B를 독립 보존; A를 B로 치환하지 않는다. |
| B15-TX-10 | M_RESULT hasVehicleResult=false, 비활성 result 쓰레기값 | 그 result를 읽어 DONE/FAILED로 해석하지 않는다. 활성 나머지 필드만 검증한다. |
| B15-TX-11 | 동일 값 Snapshot에 더 최신 raw CAN BOOT/TX 결합 | 동일 관측 증거 없으면 해당 projection만 NOT_READY; 다른 확인된 route 유지. |
| B15-TX-12 | BcmStatus faultCode=0, category=SENSOR enum0 | category를 해석하지 않고 raw NONE 의미를 유지; enum cast 금지. |
| B15-TX-13 | availability 배열 중복 functionId/COUNT | INVALID_ARGUMENT; 고정 배열 전체 검증, 임의 정상 항목 보충 없음. |
| B15-TX-14 | warning UNCONFIRMED/simulated의 raw 표현 없음 | 명시 제한/HOLD, CLEAR/실측 경고로 정상화하지 않는다. |
| B15-TX-15 | 정기 상태 하나의 유실에 request identity가 없음 | M09 로컬 관측 보존; 가짜 NetworkLocalFailure Command/Request 생성 없음. |
| B15-TX-16 | WINDOW/CF02 callback 주소를 table에 넣음 | 등록 준비와 기능 계약을 구별; 주소 존재만으로 구현 보류를 해제하지 않는다. |
| B15-TX-17 | local stable copy 뒤 publish 실패 | 성공 반환 없음; 임시 예약 rollback, source/기존 queue 불변. |
| B15-TX-18 | 원자료 전체 사본 없이 byte frame만 보존 | 후발 identity/AGE 재검사 근거 부족이면 OK 금지; 현재 API 원형을 바꾸지 않고 내부 보관 설계로 제한. |

위는 정적 설계 반례 검토이며 Host·제품·보드 실행 시험이 아니다. source Header와 팀 후보의 실제 채택은 별도 승인·바인딩 근거를 요구한다.
