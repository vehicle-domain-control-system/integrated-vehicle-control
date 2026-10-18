# DOMAIN Query와 원 CAN snapshot provenance

[B15 개요](../07_DOMAIN_ADAPTER_SERVICE.md) · [B13 Query 단일 정본](UART_M_QUERY_CONTROL.md) · [Rx22](DOMAIN_RX_ADAPTER.md) · [기존 Query 흐름](../flows/UART_M_QUERY_AND_PROJECTION.md)

B15는 Getter18의 소유·자료 출처와 동일 원 CAN 입력 연결을 보완한다. `DomainNetPriv_ProcessQuery`의 구현 정본은 [B13 process](UART_M_QUERY_CONTROL.md#process)이며 여기서 재구현하지 않는다. Getter18 본체·Manager snapshot·기록 접근 권한은 DOMAIN 소유다. Network는 기존 Getter 사본과 기존 17 schema submit을 연결한다. 신규 cache/Getter/공개 API/헤더를 만들지 않는다.

<a id="context"></a>
## 현재 문맥과 출력 수명

```c
DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);
```

위 선언의 상세 수명·반환은 [B13 contracts](UART_M_QUERY_CONTROL.md#contracts) 정본이다. `message/meta`는 같은 UART 수신 사건의 borrowed 입력, `NetUartRxMeta`에는 Boot가 없다. `DomainQueryResponseContext_t`에도 Boot를 새 필드로 넣지 않는다. 실제 M01 로컬 기동 근거를 원 Query Device/현재 Session/QueryID/scope/received_at_ms와 연결하여 ST03에서 구 문맥을 분리한다. 저장소의 실제 원자성/용량/직렬화는 바인딩 OPEN이다.

선행 M_CONTEXT를 반영한 같은 DOMAIN Owner 직렬 문맥에서 passive 검증을 먼저 수행한다. SESSION_READY0 자체는 최초 Query 거부 사유가 아니다. 인증·등록·현재 peer·Device/Session 확인과 원본 품질은 필요하며 이 검증이 Session을 발급·활성화하지 않는다. `nowMs`는 실제 Getter 호출 시점의 동일 로컬 단조 ms다. 원 ECU timestamp를 빼거나 producer TTL을 새로 정하지 않는다.

Getter OUT은 caller-owned 쓰기 가능 객체, NULL 불가이며 호출 동안 유효하다. `DOMAIN_IF_OK`일 때만 읽는다. 실패이면 값뿐 아니라 `outHasRecord/outCount`도 읽지 않는다. 성공한 STALE/INVALID/NO_DATA는 함수 실패가 아니며 Wire에 의미를 표현할 수 있을 때 원 품질을 유지한다. public status 7값은 [Rx 공통](DOMAIN_RX_ADAPTER.md#common)과 [C2 API](../../interface/02_NODES/DOMAIN/20_API.md)의 의미를 그대로 따른다. 부재 flag가 없는 Getter에 flag를 추가하지 않는다.

<a id="getter18"></a>
## 18개 선언 → 17개 기존 schema

원형은 [20_API](../../interface/02_NODES/DOMAIN/20_API.md) 및 [B13 Getter 정본](UART_M_QUERY_CONTROL.md#getter18)과 대조했다. 공개 Data는 [Vehicle types](../../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md)·[DOMAIN types](../../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md), 원 Network DTO는 각 schema의 data-api 정본을 따른다. 아래 순서는 전송 순서나 응답 최대 개수를 강제하지 않는다.

<a id="api-DomainIf_QueryGetBcmDoorState"></a>
### DomainIf_QueryGetBcmDoorState

```c
DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_BCM_DOOR_STATE](uart_m/M_BCM_DOOR_STATE.md)의 기존 private Submit → UART-M. 원 자료: **BCM_DOOR_STATE**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `DoorState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. lock/open·각 quality/update/AGE; 명령 target을 관측 상태로 대체하지 않는다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetBcmClimateState"></a>
### DomainIf_QueryGetBcmClimateState

```c
DomainIf_Status_t DomainIf_QueryGetBcmClimateState(uint32_t nowMs, ClimateState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_BCM_CLIMATE_STATE](uart_m/M_BCM_CLIMATE_STATE.md)의 기존 private Submit → UART-M. 원 자료: **BCM_CLIMATE_STATE**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `ClimateState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. commanded/measured fan, thermal direction/output 및 heat-removal 상태와 다섯 품질·meta·후보 sourceGeneration을 전부 보존한다. 같은 원 CAN 관측과 실제 C2 commit의 생산 세대 대응을 확인하며 없는 온도/auto 필드는 추가하지 않는다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetBcmInteriorLightState"></a>
### DomainIf_QueryGetBcmInteriorLightState

```c
DomainIf_Status_t DomainIf_QueryGetBcmInteriorLightState(uint32_t nowMs, InteriorLightState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_BCM_LIGHT_STATE](uart_m/M_BCM_LIGHT_STATE.md)의 기존 private Submit → UART-M. 원 자료: **BCM_LIGHT_STATE**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `InteriorLightState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. type/levelPercent/RGB, applyState와 feedback capability 및 네 품질·meta·후보 sourceGeneration을 전부 보존한다. 같은 원 CAN 관측·C2 commit의 생산 세대 연결이 필요하며 실제 물리 확인 여부를 유지한다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetBcmStatus"></a>
### DomainIf_QueryGetBcmStatus

```c
DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_BCM_STATUS](uart_m/M_BCM_STATUS.md)의 기존 private Submit → UART-M. 원 자료: **BCM_STATUS**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_BcmStatusReport_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. activeFaultCode0 및 service 상태의 명시 변환; 원 Boot/update/AGE 결합.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetCisEnvironment"></a>
### DomainIf_QueryGetCisEnvironment

```c
DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_CIS_ENVIRONMENT](uart_m/M_CIS_ENVIRONMENT.md)의 기존 private Submit → UART-M. 원 자료: **CIS_ENVIRONMENT**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `CisCabinEnvironmentState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 온도·습도·조도 각각의 sequence/AGE/validity/reason; sentinel/부호/단위를 보존한다. 온습도 각각2500ms부터 비사용이며 한 채널 복구가 다른 채널을 복구하지 않는다. 보존한 원 CAN과 [로컬 무효화 사건](can/CIS_ENVIRONMENT.md#local-invalidation) 중 일반 만료/중단의 공개 Rx와 BOOT의 단일 동기 private 결과를 구분한다. [구 쓰기 배제→무효화 실제 commit→B 원seq 인계](can/CIS_ENVIRONMENT.md#environment-storage-order)는 DOMAIN 후속 commit 봉쇄·VSM/Meta·순서 문맥 무효화·Query 구 GOOD 방지와 현재 요청/수용 BOOT/적용 Epoch/완료 대조를 요구한다. 이후 B 공개 Rx 실제 반영과 동일 원 입력의 Getter 연결도 별도로 확인한다. 일반 C2 OK/Queue/콜백 반환만으로 완료를 판단하지 않는다. DOMAIN Private ABI·순차 Epoch Core는 확인됐지만 Network의 실제 호출·Receipt·환경 Rx/Query Binding이 미검증인 [BG-R05-BOOT](can/CIS_ENVIRONMENT.md#boot-binding-gap)는 BLOCKED_DESIGN이다. 실패/불명 또는 완료 주장 뒤 구 GOOD 잔존은 해당 환경 항목만 NOT_READY/부분 처리하며 구 값에 B 출처를 붙이지 않는다. 명확한 Core 완료 뒤 Observer 오류로 동일 commit을 반복하지 않는다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetCisOccupant"></a>
### DomainIf_QueryGetCisOccupant

```c
DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t nowMs, CisOccupantState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_CIS_OCCUPANT](uart_m/M_CIS_OCCUPANT.md)의 기존 private Submit → UART-M. 원 자료: **CIS_OCCUPANT**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `CisOccupantState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. Pi Boot/VisionSequence와 count/presence의 독립 품질을 보존한다. 같은 원 관측의 검증된 현재 제한은 [CIS_OCCUPANT 수신](can/CIS_OCCUPANT.md)의 제한 증거·실제 C2/Core 반영과 결합해 읽는다. 과거 raw VALID를 최신 품질로 다시 투영하거나 높은 TX만으로 회복하지 않는다. UNKNOWN255를 정상0으로 만들지 않는다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetCisRear"></a>
### DomainIf_QueryGetCisRear

```c
DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t nowMs, CisRearState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_CIS_REAR](uart_m/M_CIS_REAR.md)의 기존 private Submit → UART-M. 원 자료: **CIS_REAR**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `CisRearState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. rearDistanceMm123=원 raw123mm; sentinel65535와 proximity 품질 보존.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetCisStatus"></a>
### DomainIf_QueryGetCisStatus

```c
DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_CIS_STATUS](uart_m/M_CIS_STATUS.md)의 기존 private Submit → UART-M. 원 자료: **CIS_STATUS**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_CisStatusReport_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. CIS interface named enum·독립 Pi 상태 보존; READY를 새 vision으로 취급하지 않음.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetVssStatus"></a>
### DomainIf_QueryGetVssStatus

```c
DomainIf_Status_t DomainIf_QueryGetVssStatus(uint32_t nowMs, DomainIf_VssStateReport_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_VSS_STATUS](uart_m/M_VSS_STATUS.md)의 기존 private Submit → UART-M. 원 자료: **VSS_STATUS**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_VssStateReport_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. VSS service/fault 원문 의미와 converter Owner 미결 유지.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetWindowState"></a>
### DomainIf_QueryGetWindowState

```c
DomainIf_Status_t DomainIf_QueryGetWindowState(uint32_t nowMs, WindowState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_WINDOW_STATE](uart_m/M_WINDOW_STATE.md)의 기존 private Submit → UART-M. 원 자료: **WINDOW_STATE**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `WindowState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. WINDOW 실행·표시·timeout HOLD; 정상 Query 응답 활성화 금지.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetWindowFault"></a>
### DomainIf_QueryGetWindowFault

```c
DomainIf_Status_t DomainIf_QueryGetWindowFault(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_WINDOW_FAULT](uart_m/M_WINDOW_FAULT.md)의 기존 private Submit → UART-M. 원 자료: **WINDOW_FAULT**. 동일 원 CAN 사본/NetCanRxMeta와 실제 Owner commit의 연결이 필수다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_WindowFaultState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. WINDOW 고장 snapshot 및 원 occurrence/AGE association 필요; HOLD 유지.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetUserSettings"></a>
### DomainIf_QueryGetUserSettings

```c
DomainIf_Status_t DomainIf_QueryGetUserSettings( uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_USER_SETTINGS](uart_m/M_USER_SETTINGS.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN user settings**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_UserSettingsState_t *outSettings` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 각 사용자 설정 값과 원 quality/update/AGE를 유지하고 기능 활성/권한으로 승격하지 않음.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetFunctionAvailability"></a>
### DomainIf_QueryGetFunctionAvailability

```c
DomainIf_Status_t DomainIf_QueryGetFunctionAvailability( uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_AVAILABILITY](uart_m/M_AVAILABILITY.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN function availability**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `uint32_t nowMs` IN; `DomainIf_FunctionAvailabilitySet_t *outAvailability` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 기능별 available/reason·독립 의미 보존; C2에서 Wire로 표현 불가능한 reason은 정상0으로 축소하지 않음.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetDigitalKeyState"></a>
### DomainIf_QueryGetDigitalKeyState

```c
DomainIf_Status_t DomainIf_QueryGetDigitalKeyState( uint32_t nowMs, DigitalKeyState_t *outState);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_DIGITAL_STATUS](uart_m/M_DIGITAL_STATUS.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN digital key state**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `uint32_t nowMs` IN; `DigitalKeyState_t *outState` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 현재 digital key 설정·연결·근접·품질을 구분; 지원/보안/차량 수행 완료는 별도.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetCurrentWarning"></a>
### DomainIf_QueryGetCurrentWarning

```c
DomainIf_Status_t DomainIf_QueryGetCurrentWarning( DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_WARNING](uart_m/M_WARNING.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN current warning by type**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `DomainWarningType_t type` IN; `uint32_t nowMs` IN; `DomainIf_WarningInfo_t *outWarning` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 정의된 8가지 DomainWarningType_t만 요청, COUNT 거부. 없는 경고를 실패 OUT으로 CLEAR 합성하지 않음.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetRequestResult"></a>
### DomainIf_QueryGetRequestResult

```c
DomainIf_Status_t DomainIf_QueryGetRequestResult( DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_RESULT](uart_m/M_RESULT.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN request result by original RequestContext**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `DeviceContextId_t requestingDevice` IN; `const RequestContext_t *targetRequest` IN; `uint32_t nowMs` IN; `DomainIf_MobileRequestResult_t *outResult` OUT; `bool *outHasRecord` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. requestingDevice의 권한과 별도 과거 targetRequest(sessionId/requestId)를 검증; 현재 recipient Session으로 덮지 않음. 성공 뒤 outHasRecord=true일 때만 result 사용.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetRecentRequestResults"></a>
### DomainIf_QueryGetRecentRequestResults

```c
DomainIf_Status_t DomainIf_QueryGetRecentRequestResults( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_RESULT](uart_m/M_RESULT.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN recent request records**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `DeviceContextId_t requestingDevice` IN; `uint32_t nowMs` IN; `DomainIf_MobileRequestResult_t *outResults` OUT; `size_t capacity` IN; `size_t *outCount` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. 현재 requestingDevice의 최신순 최대5건을 조회하며 Network는 capacity=min(확보한 실제 배열 원소 수,5)를 전달한다. DOMAIN의 Device별 최대8건 보관을 Network cache로 복제하지 않는다. 성공 뒤 outCount≤capacity 및 유효 각 record를 확인한다. count>capacity를 오류로 보존하고 조용히 잘라 정상 종료하지 않는다. 실패 OUT/count는 읽지 않는다. count==capacity는 전체8건 소진의 증거가 아니며0/NULL 허용 정책은 OPEN이다. 동일 요청의 Session/Request 식별·진행중 기록·필터는 DOMAIN 책임이다.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

<a id="api-DomainIf_QueryGetLastDigitalKeyResult"></a>
### DomainIf_QueryGetLastDigitalKeyResult

```c
DomainIf_Status_t DomainIf_QueryGetLastDigitalKeyResult( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);
```

**소유·경로:** DOMAIN snapshot/기록 Owner → Network M02/F04 호출자 OUT → [M_DIGITAL_RESULT](uart_m/M_DIGITAL_RESULT.md)의 기존 private Submit → UART-M. 원 자료: **DOMAIN digital-key result record**. 원 DOMAIN 기능/기록의 identity·품질·시간 근거를 보존한다.

**인자·소비:** `DeviceContextId_t requestingDevice` IN; `uint32_t nowMs` IN; `DomainIf_DigitalKeyResult_t *outResult` OUT; `bool *outHasRecord` OUT. `nowMs`는 해당 호출의 로컬 단조시간이다. 값/포인터 수명은 [공통](#context)과 기존 schema 정본을 따른다. requestingDevice 권한·현재 recipient와 과거 결과 identity를 분리. 성공 뒤 outHasRecord=false만 확인된 부재이며 실패 OUT/flag는 읽지 않음.

**결과·미결:** 성공 OUT만 기존 schema에 투영한다. 불법 enum/조합은 기존 submitter의 INVALID_ARGUMENT 의미로 구분하고, 정당한 값의 표현 정책 미합의 또는 association 미입증은 해당 항목만 `NET_SUBMIT_NOT_READY`/`DOMAIN_IF_NOT_AVAILABLE` 경계 및 Query 부분 결과로 제한한다. 접근권한·scope 목록·snapshot 직렬화·후발 TX 집계는 IF-O25/D-05/IF-O12 회신 대기다.

RequestResult 단건과 RecentRequestResults 목록은 같은 M_RESULT를 사용하므로 18 Getter/17 schema다. 두 Getter를 새 Wire2개로 세지 않는다. scope0/1/2/3은 현재 상태/요청 결과/경고/전체지만 실제 목록·권한·필터는 IF-O25 OPEN이다. scope0의 과거 Request 참조0 및 CURSOR0을 지키고 scope1..3을 추정하여 단건/전체로 바꾸지 않는다.

<a id="association"></a>
## 원 CAN → C2 commit → 같은 Getter/TxPort 출력

다음 판정표는 런타임 receipt API나 새로운 Header 계약이 아니다. 기존 ST01/M01의 raw↔typed 연결 및 M02/M03의 실제 호출 근거를 설명한다. 필요한 Owner 관측 채널이 없으면 그 단계는 BINDING_OPEN이며 값 비교로 대체하지 않는다.

| 경계 | 실제 입력 근거와 대조 항목 | 통과하지 못할 때 |
|---|---|---|
| ① codec → admission | 같은 실제 frame의 Net DTO/NetCanRxMeta, 원 ECU Boot/TX, 실제 received_at_ms, 형식·padding 검증 | 형식 오류는 typed 미생산; 정상 CRC/Frame 자체를 상태 commit으로 기록하지 않음 |
| ② admission → C2 호출 | 해당 원 입력의 순서/Boot·원 관측 update/AGE/품질 검증 및 정확한 변환 사본. CIS/Pi Boot·VisionSequence는 서로 별도 | 원본/Generation/품질 표현 근거 부족이면 영향 투영 제한; 최근 raw와 접합 금지 |
| ③ C2 호출 → Core commit | DOMAIN Owner가 이 호출 입력을 반영한 사실과 해당 처리의 직렬화, optional Observer 결과와 구별된 commit 단계 | Rx OK/최종 observer 반환만으로 commit을 추정하지 않음. 실제 owner correlation 부재는 OPEN |
| ④ commit → Getter/TxPort | 이 출력의 원 관측 및 현재 제한 revision이 ③의 해당 반영 사건에 귀속된 Owner 처리 근거. 중간 쓰기/Boot/Session 교체를 구분하고 원 관측 identity는 불변으로 확인 | 값 동일·같은 Boot만으로 일치 선언 금지. 두번째 cache 만들지 않음 |
| ⑤ 출력 → UART projection | 대응 source DTO/meta와 검증된 현재 제한이 같은 보존 기록에서 나옴. 기존 원 CAN used layout과 DOWN_CONTEXT20B를 사용하며 padding 제외. 현재 품질을 원 raw GOOD로 되돌리지 않음. 이미 반영된 AGE 기준점과 미반영 경과 구분 | 대응 미입증은 해당11개 projection만 미제공/부분. unrelated 보고·Query 전체의 영구 차단 없음 |

현재 품질과 원 관측을 연결할 때에는 원 raw의 불변 사본, 동일 관측을 제한한 실제 사건, 그 제한을 반영한 C2/Core 처리, 같은 제한을 반환한 Getter를 함께 확인한다. 환경에서는 [NetCisEnvironmentLocalEvent](../data/CAN_NODE_DATA.md#type-NetCisEnvironmentLocalEvent)의 출처·채널·관측시각을 원 raw/meta에 결합한다. Occupant에서는 [NetCanOccupantRestrictionEvidence](../data/CAN_NODE_DATA.md#type-NetCanOccupantRestrictionEvidence)로 적법한 새 CAN TX와 동일 원 Pi 관측·값 및 확인된 기능별 제한을 연결한다. 제한 없는 원 raw GOOD와 현재 STALE이 바이트 동등해야 한다고 요구하지 않으며, 입증된 파생 관계만 허용한다. 이 관계가 없거나 기존 UART 필드로 현재 품질을 표현할 수 없으면 해당 항목만 HOLD한다. 가짜 새 CAN meta/TX/원 seq를 만들거나 제한 이전 품질로 돌아가지 않는다. C2 부분 반영·결과 미확인은 자동 rollback/재호출하지 않고 해당 revision을 미검증으로 남긴다.

여기서 필요한 실제 Owner correlation 증거는 새 `bool ready=true`가 아니다. 어떤 API 호출 입력이 어떤 Core 반영 사건이 되었고 어떤 Getter/TxPort 출력까지 중간 덮어쓰기 없이 이어졌는지, 같은 처리 문맥의 관측과 원 데이터 식별로 확인해야 한다. 그 증거를 넘기는 제품 메커니즘·재진입 정책·동시성 primitive는 D-05/IF-O24 회신 전이며 B15에서 승인하지 않는다.

원 관측 key에는 메시지별로 ECU Boot와 source generation의 의미 대응, update/occurrence 및 CIS_OCCUPANT의 Pi Boot/VisionSequence 등 기존 식별을 함께 고려한다. TX_SEQUENCE는 원 관측 번호가 아니고 raw 값이 같아도 VisionSequence9/10은 다른 관측일 수 있다. 반대로 동일 source update를 TX_SEQUENCE11/12로 반복하면 새 원 관측이 되지 않는다. 주기마다 임의로 generation/AGE/순번을 새로 만들지 않는다.

선택 Vehicle v0.3 후보의 ClimateState_t/InteriorLightState_t sourceGeneration은 성공 OUT의 같은 관측 검증에 포함한다. 후보 필드만으로 BOOT_ID lifecycle·generation 0 수용·원자적 snapshot association을 증명하지 않는다. 알려진 세대는 품질 저하에도 보존한다. 환경 DTO는 generation을 추가하지 않고 온습도2500ms 합의와 조도 TTL 미결을 분리한다. 공개 receipt나 sourceGeneration=BOOT_ID 자동 복사를 추가하지 않는다.

<a id="reuse"></a>
## 기존 실행 의사코드의 재사용 지점

1. 기존 [DomainNetPriv_ProcessQuery](UART_M_QUERY_CONTROL.md#process)가 M_QUERY의 ID/scope/CURSOR와 현재 문맥을 검증한다.
2. 같은 정본이 위 Getter18 중 승인된 기존 호출 계획에 해당하는 Getter만 호출하고 성공 OUT을 얻는다. 실패 OUT은 소비하지 않는다.
3. CAN 기원11개는 위 association 근거 및 각 기존 [하향 정본](../flows/UART_M_DOWNLINK.md)을 적용하고 나머지6 schema는 기존 typed 변환을 따른다. 요청결과 Getter2개가 하나의 M_RESULT schema를 공유한다.
4. 기존 `DomainNetPriv_SubmitMobile*`가 전체 identity/값/품질/AGE를 안정된 사본으로 접수한 후 로컬 성공을 반환한다. 접수 count를 최종 TX count로 사용하지 않는다.
5. 기존 [terminal 절차](UART_M_QUERY_CONTROL.md#terminal)와 [M_QUERY_END](uart_m/M_QUERY_END.md)가 최종 TX 결과·순서를 입증한 응답 수와 원 Query를 연결한다. 새 ProcessQuery 본체를 만들지 않는다.

최근 결과의 선정 범위가5건이고 모두 final TX이면 다른 누락·현재 문맥·순서를 확인해 기존 DONE/count5 후보를 만든다. DOMAIN 보관8건 중 미선정3건은 실패가 아니지만 실제 생산 범위8건 중5건 성공·3건 미확정이면 End를 보류한다. recent5 중4건 완료·1건 미확정도 보류하며 확정 실패 뒤에만 PARTIAL/count4의 기존 조건을 확인한다.

응답10개 접수/8개 final TX/2개 미확정이면 End를 보류한다. 8개 final TX/2개 확정 실패 및 같은 Query/송신 순서가 입증된 경우 status2/count8 후보이며 End 자체·중복·QUERY_ID0 자발 보고는 count에서 제외한다. 실제 빈 목록의 성공0건과 Getter 실패0건은 다르다. Driver 참조 해제만으로 최종 TX가 입증되지 않으며 release 후에도 미확정 identity를 잊고 재송신하지 않는다.

Session/DOMAIN Boot가 교체되면 구 Query 결과를 새 recipient로 재귀속하지 않는다. 현재 recipient Session과 과거 RequestContext.sessionId를 끝까지 구분한다. Query deadline·재시도·wrap 비교창은 임의로 정하지 않는다. recent capacity의 상한5 합의는 다른 Query의 개수·실제 배열 용량을 확정하지 않는다. Link 회복/같은 값 재수신은 구 Query 재개 근거가 아니다.

<a id="review"></a>
## 전문 자체검수 반례

| 입력/상황 | 정적 기대 결과 |
|---|---|
| 성공 Getter가 STALE 거리123을 반환 | 상태 성공과 품질 분리; 원 AGE 유지; 지원 schema로 표현 가능한 경우에만 제출 |
| 같은 occupant count2, 원 Pi VisionSequence9/10 | 값 동등성을 association으로 쓰지 않음; CIS_REAR에 Pi Vision 식별을 잘못 연결하지 않음 |
| Getter 실패, 이전 OUT에 정상값 남음 | 값/flag/count 모두 소비하지 않음 |
| Recent 결과 outCount=capacity+1 | 범위 위반, 배열 넘어읽기/End 성공 없음 |
| count==capacity, 다음 기록 존재 여부 불명 | 전체 history 완료로 주장하지 않음 |
| Core commit 성공 뒤 Observer 실패 | 원 commit 유지; 동일 Rx 재호출/rollback 없음 |
| 수신 정상/후보 Generation0/qualityOK | 세대 확인 또는 NO_DATA 자동 강등 모두 금지 |
| WINDOW2 Query schema | schema 추적 유지, 정상 전송/표시/timeout 활성화 금지 |
| 8 final TX와2 미확정 후 release 전부 관측 | count8이어도 종료 순서/미확정 해소 전 End 완료 주장 없음 |

근거: S4A `api/domain_query.md`의 Getter18, S4C `TIME_IDENTITY_CONTRACT.md#query/#projection`와 `FRESHNESS_AGE_TIMEOUT.md`, B13 Query/17개 schema 정본. 자체검수는 선언/문서 의미 검토이며 실제 ABI·Owner commit instrumentation·Driver final TX 시험 증거가 아니다.

### PG6-SW02 환경 Getter의 저장 순서 검수

성공 OUT이 NO_DATA라는 값만으로 BOOT 전이나 seq reset을 추론하지 않는다. BOOT에서는 같은 원 record/event와 고정 요청에 대한 단일 동기 private 결과, 적용 Epoch/내용 일치, DOMAIN A 작업의 미래 commit 봉쇄·VSM/Meta·관련 순서 문맥 무효화·Query 구 GOOD 방지, 이후 B 원seq의 공개 Rx 실제 저장과 Getter/TxPort 동일 입력 관계를 각각 확인한다. Getter와 원자료를 같은 owner 구간에서 확보해 중간 A 쓰기/새 B 쓰기와 혼합되지 않았음을 확인한다. 일반 만료/중단은 기존 공개 Rx의 현재 제한 근거를 유지한다. Network는 미시작 A 환경 Query 응답의 재게시를 막고 DOMAIN은 이미 인계된 구 작업/Getter의 재노출을 방지한다. 이미 Driver가 참조하는 frame의 임의 수정·취소나 과거 원격 전달 소급 무효화를 주장하지 않는다. B gate 차단은 A GOOD 제거 증거가 아니며 private 완료 주장 뒤에도 Getter가 A GOOD이면 Binding 실패/HOLD다. 관련 환경 항목만 기존 미제공/부분 처리하고 다른 Query·Getter18·schema17·M_RESULT·M_QUERY_END/count 규칙은 유지한다. B 원seq0은0 그대로이고 원 기준의 미반영 AGE는 한 번 누적하며2500ms 이상 온습도는 비사용, 채널별 품질은 독립이다.
