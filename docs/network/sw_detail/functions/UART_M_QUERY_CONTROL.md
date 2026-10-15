# M02/F04–F05 — UART-M 조회 생산과 종료

[M02 모듈](../modules/M02_QUERY_CONTROL.md) · [L01](../layers/L01_NODE_CONTEXT.md) · [M_QUERY](uart_m/M_QUERY.md) · [M_QUERY_END](uart_m/M_QUERY_END.md) · [Query flow](../flows/UART_M_QUERY_AND_PROJECTION.md)

Network는 기존 DOMAIN Getter의 사본을 기존 17개 하향 schema로 전달한다. Getter 본체·기록 접근 권한·Manager 상태·Session 발급은 DOMAIN 소유다. 같은 MCU의 로컬 함수 호출이며 원격 ECU RPC·새 차량 상태 cache가 아니다. 이 문서는 기존 Query 선언의 조건부 상세이며 IF-O12/IF-O25/D-03/D-05/CF01을 확정하지 않는다.

<a id="contracts"></a>
## 기존 함수 원형과 자료 수명

```c
DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message, const NetUartRxMeta *meta);
NetSubmitResult DomainNetPriv_SendQueryEnd(const NetMobileQueryEnd *message);
DomainIf_Status_t DomainIf_ValidatePassiveQueryContext( DeviceContextId_t deviceContextId, SessionId_t sessionId);
```

`ProcessQuery`의 message/meta는 동일 수신 사건의 호출 중 borrowed 자료다. 후속 처리를 접수한다면 현재 Device/Session·원 QueryID/scope·과거 Request 참조·원 received_at_ms와 해당 시점 DOMAIN Boot를 함께 사본으로 보존해야 한다. `NetUartRxMeta`에는 Boot 멤버가 없으므로 존재하지 않는 필드에 접근하지 않는다. DOMAIN Boot는 M01이 실제로 확인한 로컬 기동 근거다. `DomainQueryResponseContext_t`도 recipient/queryId/scope만 가지고 있으며 Boot를 임의 멤버로 추가하지 않는다.

`nowMs`는 같은 로컬 단조시간의 실제 Getter 호출 시점이다. 노드 간 시간을 차감하거나 source TTL·Session TTL을 새로 정하지 않는다. Getter OUT은 호출자 저장소이고 성공 시에만 읽는다. 실패이면 OUT·outHasRecord·outCount 모두 소비하지 않는다. 성공과 원 품질 OK는 별개이므로 INVALID/STALE/NO_DATA도 해당 Wire에 표현 가능하면 원 의미를 보존한다.

`ProcessQuery`의 DOMAIN_IF_OK는 로컬 Query 처리 경계의 성공 후보이며 응답 TX 완료가 아니다. 부분 Getter·Submit 실패의 집계 반환이 미정이므로 실제 바인딩 전에는 성공을 약속하지 않는다. NOT_INITIALIZED/NOT_AVAILABLE/REJECTED/UNSUPPORTED 등 실제 C2 상태를 진단에서 보존하고 하나의 차량 실패로 합치지 않는다. `SendQueryEnd`는 NET_SUBMIT_* 네 반환의 로컬 접수 의미를 따르며 최종 UART TX와 별도다.

<a id="getter18"></a>
## 18 Getter와 17 응답 schema

다음 원형은 S4A `api/domain_query.md`의 기존 선언이다. 표 순서는 생산 순서·우선순위 또는 최대 응답 개수를 강제하지 않는다. RequestResult의 단건/목록 두 Getter가 같은 M_RESULT schema를 쓰므로 18 Getter와 17 schema의 수가 다르다.

| 기존 정확한 원형 | 응답 schema | 개별 OUT·지원 조건 |
|---|---|---|
| `DomainIf_Status_t DomainIf_QueryGetBcmDoorState(uint32_t nowMs, DoorState_t *outState);` | [M_BCM_DOOR_STATE](uart_m/M_BCM_DOOR_STATE.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetBcmClimateState(uint32_t nowMs, ClimateState_t *outState);` | [M_BCM_CLIMATE_STATE](uart_m/M_BCM_CLIMATE_STATE.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetBcmInteriorLightState(uint32_t nowMs, InteriorLightState_t *outState);` | [M_BCM_LIGHT_STATE](uart_m/M_BCM_LIGHT_STATE.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetBcmStatus(uint32_t nowMs, DomainIf_BcmStatusReport_t *outState);` | [M_BCM_STATUS](uart_m/M_BCM_STATUS.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetCisEnvironment(uint32_t nowMs, CisCabinEnvironmentState_t *outState);` | [M_CIS_ENVIRONMENT](uart_m/M_CIS_ENVIRONMENT.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetCisOccupant(uint32_t nowMs, CisOccupantState_t *outState);` | [M_CIS_OCCUPANT](uart_m/M_CIS_OCCUPANT.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetCisRear(uint32_t nowMs, CisRearState_t *outState);` | [M_CIS_REAR](uart_m/M_CIS_REAR.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetCisStatus(uint32_t nowMs, DomainIf_CisStatusReport_t *outState);` | [M_CIS_STATUS](uart_m/M_CIS_STATUS.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetVssStatus(uint32_t nowMs, DomainIf_VssStateReport_t *outState);` | [M_VSS_STATUS](uart_m/M_VSS_STATUS.md) | 성공 OUT만 사용; 동일 원 CAN association 필요 |
| `DomainIf_Status_t DomainIf_QueryGetWindowState(uint32_t nowMs, WindowState_t *outState);` | [M_WINDOW_STATE](uart_m/M_WINDOW_STATE.md) | WINDOW 실행·Tx/Rx·표시·timeout HOLD; 현재 getter→전송 경로 미활성 |
| `DomainIf_Status_t DomainIf_QueryGetWindowFault(uint32_t nowMs, DomainIf_WindowFaultState_t *outState);` | [M_WINDOW_FAULT](uart_m/M_WINDOW_FAULT.md) | WINDOW 실행·Tx/Rx·표시·timeout HOLD; 현재 getter→전송 경로 미활성 |
| `DomainIf_Status_t DomainIf_QueryGetUserSettings( uint32_t nowMs, DomainIf_UserSettingsState_t *outSettings);` | [M_USER_SETTINGS](uart_m/M_USER_SETTINGS.md) | 성공 OUT만 사용; 의미·품질/원 문맥 보존 |
| `DomainIf_Status_t DomainIf_QueryGetFunctionAvailability( uint32_t nowMs, DomainIf_FunctionAvailabilitySet_t *outAvailability);` | [M_AVAILABILITY](uart_m/M_AVAILABILITY.md) | 성공 OUT만 사용; 의미·품질/원 문맥 보존 |
| `DomainIf_Status_t DomainIf_QueryGetDigitalKeyState( uint32_t nowMs, DigitalKeyState_t *outState);` | [M_DIGITAL_STATUS](uart_m/M_DIGITAL_STATUS.md) | 성공 OUT만 사용; 의미·품질/원 문맥 보존 |
| `DomainIf_Status_t DomainIf_QueryGetCurrentWarning( DomainWarningType_t type, uint32_t nowMs, DomainIf_WarningInfo_t *outWarning);` | [M_WARNING](uart_m/M_WARNING.md) | 정의된 경고8종 이름만; DOMAIN_WARNING_COUNT 거부; 실패를 CLEAR로 만들지 않음 |
| `DomainIf_Status_t DomainIf_QueryGetRequestResult( DeviceContextId_t requestingDevice, const RequestContext_t *targetRequest, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResult, bool *outHasRecord);` | [M_RESULT](uart_m/M_RESULT.md) | 성공 후 outHasRecord를 읽고 true일 때만 result 사용; false는 확인된 기록 부재 |
| `DomainIf_Status_t DomainIf_QueryGetRecentRequestResults( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_MobileRequestResult_t *outResults, size_t capacity, size_t *outCount);` | [M_RESULT](uart_m/M_RESULT.md) | 최신순 최대5, capacity=min(실제 배열 원소 수,5); 성공 후 outCount≤capacity 확인; 실패 count/OUT 읽기 금지; 0/NULL 조합은 OPEN |
| `DomainIf_Status_t DomainIf_QueryGetLastDigitalKeyResult( DeviceContextId_t requestingDevice, uint32_t nowMs, DomainIf_DigitalKeyResult_t *outResult, bool *outHasRecord);` | [M_DIGITAL_RESULT](uart_m/M_DIGITAL_RESULT.md) | 성공 후 outHasRecord를 읽고 true일 때만 result 사용; false는 확인된 기록 부재 |

scope0(현재 상태),1(요청 결과),2(경고),3(전체)은 원 Wire enum이다. 다음 명시 호출 절차는 이 범주에 대응하는 **조건부 생산 계획**이며 실제 목록·접근 권한·필터·동시 snapshot 선정은 IF-O25에서 양단 Owner가 확인해야 한다. scope0의 REQUEST_SESSION/ID=0, CURSOR0은 현재 확정된 형식 제약이다. scope1/2/3의 요청 참조 조합을 추정하여 자동 수정하지 않는다. 전체 scope가 모든 기능의 신규 지원을 허용하지 않는다.

<a id="process"></a>
## 현재 문맥 → 성공 OUT → 안정된 응답 사본

DOMAIN ResultManager는 Device별 최근 최대8건을 보관하며 최근 결과 Getter는 현재 requestingDevice에 대해 최신순 최대5건을 제공한다. Network는 실제 배열 용량과5 중 작은 capacity를 전달하고 성공 count 범위만 기존 M_RESULT로 순차 제출한다. 내부 보관·동일 요청 갱신·진행중 기록·권한 필터는 DOMAIN 책임이다. 이 상한은 최근 결과 분기에만 적용하며 상태/경고/단건 Query의 응답 수를 줄이지 않는다.

처리 순서는 선행 M_CONTEXT 반영 문맥 확인→passive 검증→선택 목록 고정→Getter→원본/표현검사→동일 Query에 사본 접수다. 최초 SESSION_READY0만으로 차단하지 않으며 passive 검증이 Session을 발급·활성화하지 않는다. Getter 성공 뒤 Session/Boot가 바뀌면 이미 복사한 출력도 새 문맥의 응답이 되지 않는다.

아래는 기존 `DomainNetPriv_ProcessQuery`의 C형 설명 절차다. 주석 조건은 실제 팀·Driver·직렬화 근거를 요구하는 조건이며 임의 bool 설정이나 새로운 공개 API가 아니다. `partial` 등은 설명용 로컬 집계이고 새 Wire status가 아니다. ST03에 남길 응답 identity는 각 기존 submit이 보존하는 Query/원본에 연결되며, 연결을 증명하지 못하면 submit 전에 제한한다.

```c
DomainIf_Status_t DomainNetPriv_ProcessQuery(const NetMobileQuery *message,
                                           const NetUartRxMeta *meta)
{
    if (message == NULL || meta == NULL) return DOMAIN_IF_INVALID_ARGUMENT;
    if (message->context.device_context_id == 0 || message->query_id == 0 ||
        message->query_scope > 3 || message->cursor != 0)
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (message->query_scope == 0 && (message->request_session != 0 || message->request_id != 0))
        return DOMAIN_IF_INVALID_ARGUMENT;
    if (/* profile·기동·순서·동일 Owner 직렬화 또는 Query 집계 반환 바인딩 미입증 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    DomainIf_Status_t context = DomainIf_ValidatePassiveQueryContext(
        message->context.device_context_id, message->context.session_id);
    if (context != DOMAIN_IF_OK) return context; // Getter OUT에 접근하지 않는다.
    if (/* scope별 목록/접근권한/과거 Request 참조 조합의 계약 미확인 */)
        return DOMAIN_IF_NOT_AVAILABLE;
    if (/* 응답 사본·순서·final TX association을 보관할 준비/용량 미확인 */)
        return DOMAIN_IF_NOT_AVAILABLE;

    NetMobileQuery request = *message; // 원 요청 사본, 기존 message 주소를 보관하지 않는다.
    NetUartRxMeta rx = *meta;
    DomainQueryResponseContext_t response;
    response.recipient = request.context;
    response.queryId = request.query_id;
    response.scope = request.query_scope;
    // ST03은 여기서 확인한 DOMAIN Boot·현재 문맥·선택 목록과 원 rx를 함께 보존한다.
    uint32_t nowMs = /* 동일 로컬 단조시간의 실제 관측값; clock 바인딩 미확인 시 진행 금지 */;
    bool partial = false;

    if (request.query_scope == NET_QUERY_SCOPE_STATE || request.query_scope == NET_QUERY_SCOPE_ALL) {
        DoorState_t door;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_door = DomainIf_QueryGetBcmDoorState(nowMs, &door);
        if (r_door == DOMAIN_IF_OK) {
            NetSubmitResult s_door = DomainNetPriv_SubmitMobileBcmDoorState(&door, &response);
            if (s_door != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        ClimateState_t climate;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_climate = DomainIf_QueryGetBcmClimateState(nowMs, &climate);
        if (r_climate == DOMAIN_IF_OK) {
            NetSubmitResult s_climate = DomainNetPriv_SubmitMobileBcmClimateState(&climate, &response);
            if (s_climate != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        InteriorLightState_t light;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_light = DomainIf_QueryGetBcmInteriorLightState(nowMs, &light);
        if (r_light == DOMAIN_IF_OK) {
            NetSubmitResult s_light = DomainNetPriv_SubmitMobileBcmInteriorLightState(&light, &response);
            if (s_light != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DomainIf_BcmStatusReport_t bcm;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_bcm = DomainIf_QueryGetBcmStatus(nowMs, &bcm);
        if (r_bcm == DOMAIN_IF_OK) {
            NetSubmitResult s_bcm = DomainNetPriv_SubmitMobileBcmStatus(&bcm, &response);
            if (s_bcm != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        CisCabinEnvironmentState_t environment;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_environment = DomainIf_QueryGetCisEnvironment(nowMs, &environment);
        if (r_environment == DOMAIN_IF_OK) {
            NetSubmitResult s_environment = DomainNetPriv_SubmitMobileCisEnvironment(&environment, &response);
            if (s_environment != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        CisOccupantState_t occupant;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_occupant = DomainIf_QueryGetCisOccupant(nowMs, &occupant);
        if (r_occupant == DOMAIN_IF_OK) {
            NetSubmitResult s_occupant = DomainNetPriv_SubmitMobileCisOccupant(&occupant, &response);
            if (s_occupant != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        CisRearState_t rear;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_rear = DomainIf_QueryGetCisRear(nowMs, &rear);
        if (r_rear == DOMAIN_IF_OK) {
            NetSubmitResult s_rear = DomainNetPriv_SubmitMobileCisRear(&rear, &response);
            if (s_rear != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DomainIf_CisStatusReport_t cis;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_cis = DomainIf_QueryGetCisStatus(nowMs, &cis);
        if (r_cis == DOMAIN_IF_OK) {
            NetSubmitResult s_cis = DomainNetPriv_SubmitMobileCisStatus(&cis, &response);
            if (s_cis != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DomainIf_VssStateReport_t vss;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_vss = DomainIf_QueryGetVssStatus(nowMs, &vss);
        if (r_vss == DOMAIN_IF_OK) {
            NetSubmitResult s_vss = DomainNetPriv_SubmitMobileVssStatus(&vss, &response);
            if (s_vss != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DomainIf_UserSettingsState_t settings;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_settings = DomainIf_QueryGetUserSettings(nowMs, &settings);
        if (r_settings == DOMAIN_IF_OK) {
            NetSubmitResult s_settings = DomainNetPriv_SubmitMobileUserSettings(&settings, &response);
            if (s_settings != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DomainIf_FunctionAvailabilitySet_t availability;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_availability = DomainIf_QueryGetFunctionAvailability(nowMs, &availability);
        if (r_availability == DOMAIN_IF_OK) {
            NetSubmitResult s_availability = DomainNetPriv_SubmitMobileFunctionAvailability(&availability, &response);
            if (s_availability != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        DigitalKeyState_t digital;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간; reset/wrap 경과 미입증 시 보류 */;
        DomainIf_Status_t r_digital = DomainIf_QueryGetDigitalKeyState(nowMs, &digital);
        if (r_digital == DOMAIN_IF_OK) {
            NetSubmitResult s_digital = DomainNetPriv_SubmitMobileDigitalKeyState(&digital, &response);
            if (s_digital != NET_SUBMIT_OK) partial = true;
            // OK는 안정된 응답 접수 기록만 남긴다. finalTxCount를 증가시키지 않는다.
        } else partial = true; // 실패 OUT은 읽지 않는다.
        // WINDOW 두 Getter는 원형/응답 관계를 위 표에 남기고 현재 호출→전송은 활성화하지 않는다.
        // 요청 목록에 WINDOW가 필요했다면 미지원 항목을 partial로 기록한다.
        // Owner가 정의한 scope에서 제외된 항목이면 완전성 분모에서도 제외했음을 근거로 남긴다.
        if (/* 확인된 이번 목록에 WINDOW_STATE 또는 WINDOW_FAULT가 필요함 */) partial = true;
    }

    if (request.query_scope == NET_QUERY_SCOPE_WARNING || request.query_scope == NET_QUERY_SCOPE_ALL) {
        const DomainWarningType_t kinds[8] = {
            DOMAIN_WARNING_WINDOW_ANTIPINCH, DOMAIN_WARNING_OCCUPANT_HAZARD,
            DOMAIN_WARNING_REAR_OBSTACLE, DOMAIN_WARNING_DOOR_LEFT_OPEN,
            DOMAIN_WARNING_BCM_FAULT, DOMAIN_WARNING_CIS_FAULT,
            DOMAIN_WARNING_WINDOW_FAULT, DOMAIN_WARNING_VSS_FAULT
        };
        for (size_t i = 0; i < 8; ++i) { // 정의된 경고8종만, COUNT sentinel은 호출하지 않는다.
            DomainIf_WarningInfo_t warning;
            nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간 */;
            DomainIf_Status_t r = DomainIf_QueryGetCurrentWarning(kinds[i], nowMs, &warning);
            if (r != DOMAIN_IF_OK) { partial = true; continue; }
            NetSubmitResult s = DomainNetPriv_SubmitMobileWarning(&warning, &response);
            if (s != NET_SUBMIT_OK) partial = true;
        }
    }

    if (request.query_scope == NET_QUERY_SCOPE_RESULT || request.query_scope == NET_QUERY_SCOPE_ALL) {
        if (/* Owner가 승인한 이번 대상이 지정된 과거 Request 한 건 */) {
            RequestContext_t target;
            target.sessionId = request.request_session; // 현재 recipient Session으로 대체하지 않는다.
            target.requestId = request.request_id;
            DomainIf_MobileRequestResult_t result;
            bool hasRecord;
            nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간 */;
            DomainIf_Status_t r = DomainIf_QueryGetRequestResult(
                request.context.device_context_id, &target, nowMs, &result, &hasRecord);
            if (r != DOMAIN_IF_OK) partial = true; // result와 hasRecord 모두 읽지 않는다.
            else if (!hasRecord) {
                // 확인된 원 요청 기록 부재를 보존한다. 아직 전체 scope의 status1이라고 확정하지 않는다.
            } else {
                NetSubmitResult s = DomainNetPriv_SubmitMobileRequestResult(&result, &response);
                if (s != NET_SUBMIT_OK) partial = true;
            }
        } else if (/* Owner가 승인한 이번 대상이 해당 단말의 최근 기록 목록 */) {
            DomainIf_MobileRequestResult_t *results = /* 확보된 호출자 소유 배열 */;
            size_t arrayCapacity = /* 확보된 배열의 실제 원소 수 */;
            size_t capacity = arrayCapacity < 5u ? arrayCapacity : 5u; // 최근 결과 분기만 적용
            if (/* capacity=0/NULL 또는 부족 반환 의미의 바인딩 미확인 */)
                return DOMAIN_IF_NOT_AVAILABLE;
            size_t count;
            nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간 */;
            DomainIf_Status_t r = DomainIf_QueryGetRecentRequestResults(
                request.context.device_context_id, nowMs, results, capacity, &count);
            if (r != DOMAIN_IF_OK) {
                partial = true; // count/results 읽기 금지
                // ST03에 원 Getter 반환 r과 이 분기의 미제공 원인을 보존한다.
            }
            else if (count > capacity) {
                // ST03에 원 Getter 반환·capacity/count 계약 위반과 이미 접수한 응답을 보존한다.
                // 결과를 capacity로 자르거나 정상 빈 목록·DONE으로 처리하지 않는다.
                return DOMAIN_IF_NOT_AVAILABLE; // 범위 밖 사용 금지
            }
            else {
                for (size_t i = 0; i < count; ++i) {
                    NetSubmitResult s = DomainNetPriv_SubmitMobileRequestResult(&results[i], &response);
                    if (s != NET_SUBMIT_OK) partial = true;
                }
                // ST03에 최신순으로 선정한 count개와 capacity를 보존한다. DOMAIN 내부 최대8 보관은 별도다.
                // count==capacity는 전체 이력 소진 증거가 아니다. 원 선택 범위를 나중에 줄여 성공으로 바꾸지 않는다.
            }
        } else partial = true; // 미확정 조합을 단건/목록으로 임의 선택하지 않는다.

        DomainIf_DigitalKeyResult_t digitalResult;
        bool hasDigital;
        nowMs = /* 이 Getter 호출 시점의 같은 로컬 단조시간 */;
        DomainIf_Status_t rd = DomainIf_QueryGetLastDigitalKeyResult(
            request.context.device_context_id, nowMs, &digitalResult, &hasDigital);
        if (rd != DOMAIN_IF_OK) partial = true;
        else if (hasDigital) {
            NetSubmitResult sd = DomainNetPriv_SubmitMobileDigitalKeyResult(&digitalResult, &response);
            if (sd != NET_SUBMIT_OK) partial = true;
        } // hasDigital=false의 확인된 부재를 값0/DONE으로 채우지 않는다.
    }
    // ST03에 partial·확인된 부재·미제공 이유와 실제 접수 항목을 보존한다.
    // 위 선택 목록·집계 반환의 실제 채택 근거를 통과한 조건에서만 로컬 접수 OK 후보다.
    // 이 함수에서 enqueue 수를 세어 SendQueryEnd(status0,count)를 호출하지 않는다.
    return DOMAIN_IF_OK;
}
```

17개 `DomainNetPriv_SubmitMobile*`의 원형은 각 응답 메시지 문서와 [하향 flow](../flows/UART_M_DOWNLINK.md)에 연결된다. 원 CAN 11 projection은 Getter값=최신 raw값이라는 비교로 결합할 수 없다. 같은 NetDTO/NetCanRxMeta→C2 실제 반영→Getter/TxPort의 association 근거가 없으면 그 submit만 NOT_READY/부분 상태로 제한한다. WINDOW2는 현 실행 경로 자체가 HOLD다. UserSettings의 has 없는 NORMAL 밝기/RGB, DigitalResult의 실제 unlock 전이처럼 Wire에 표현할 수 없는 정보를 새 필드/기본값으로 채우지 않는다.

위 순차 의사코드는 Getter 여러 개가 동일 시각의 원자 snapshot이라는 보증이 아니다. 일반 Getter에는 Device 인자가 없고 세 기록 Getter에만 requestingDevice가 있으므로 private Network 코드에서 새 권한 정책을 만들 수 없다. 동일 `nowMs`를 전달하는 것만으로 동시 갱신·C2 commit association 문제가 해결되지 않는다.

<a id="terminal"></a>
## F05 — 응답 final TX와 종료 조건

ST03의 Query 문맥은 현재 Device/Session·확인된 DOMAIN Boot·QueryID/scope이고, 각 응답은 보존된 원 identity와 연결된다. Gateway 재접속·Session 교체·DOMAIN 재기동은 완료 자격을 무효화할 수 있지만 Driver 참조 중 frame을 해제하지는 않는다. M07/M08의 callback generation·제출 수명·최종 outcome·release를 독립 확인한다. LINK_SEQUENCE만으로 늦은 구 callback을 현재 Query에 귀속할 수 없다.

| 관측 | 기존 status/count 후보 | 제출 조건 |
|---|---|---|
| 필요한 목록·Getter·표현·동일 원본 근거가 모두 확보되고 전 응답 final TX 확인 | DONE(0) / 확인된 N | 전 결과·동일 UART 순서 확인 후 End를 뒤에 배치 |
| Getter 실패·표현 불가·association 부재·확정 일부 송신 실패 | PARTIAL_UNCONFIRMED(2) / 확인된 N | 미확정 응답 outcome이 없어야 count/순서 근거 가능; N0도 빈 성공 아님 |
| recent로 선정한5개 전부 final TX·다른 누락 없음 | DONE(0) / 5 | 이번 범위가 recent5라는 근거·현재 문맥·순서를 확인. DOMAIN 보관8개 중 미선정3개는 실패가 아님 |
| 생산 범위8개 중5 final TX·3 미확정 | End 보류 | recent 상한을 소급 적용하여 분모를5로 바꾸지 않음. 3개가 확정 실패가 되면 기존 부분종결 조건 적용 |
| 원 요청 기록 부재를 DOMAIN이 확인 | NOT_FOUND(1) / 0 | 해당 요청 조회의 부재 의미·현재 End 전달 가능성 확인 |
| passive 문맥 거부 | CONTEXT_REJECTED(3) / 0 | 해당 recipient로 거부 End를 보낼 수 있는 별도 문맥/정책 확인 |
| scope의 실제 빈 집합이 입증됨 | DONE(0) / 0 | Getter 실패·부족·필터 미정·무응답과 구분 |
| Driver final TX 미관측·순서 불명·구 callback·count overflow | 성공 End 보류 | 로컬 미완료 근거 보존; enqueue 수로 추정하지 않음 |

```c
// F05의 설명 절차: 기존 DomainNetPriv_SendQueryEnd를 호출하기 전의 ST03 감독.
// 별도 공개 함수/ACK/ordinal/Query failure kind를 추가하지 않는다.
if (/* 기존 Query의 현재 문맥·선택 목록·응답 identity 연결이 무효 */) {
    // 완료 자격을 보류한다. 구 응답/End를 새 Session에 자동 재전송하지 않는다.
} else if (/* 한 항목이라도 final TX outcome 또는 송신 순서가 미확정 */) {
    // End 생성/송신 완료 선언 보류. 원 pending identity는 물리 buffer release와 별도로 유지.
} else {
    uint32_t finalTxCount = 0; // 범위 검사 전 정수 wrap도 허용하지 않는다.
    bool countOverflow = false;
    // 정확히 이번 ST03의 예정 응답 기록을 순회한다. 공개 Wire ordinal은 없다.
    for (/* 현재 Query에 속한 각각의 기존 응답 기록 */) {
        if (/* QUERY_ID0 자발 갱신 또는 End 자신 또는 재전송 복제 */) continue;
        if (/* 현재 제출 수명과 연결된 최초의 실제 최종 UART TX 완료 */) {
            if (finalTxCount == 65535u) { countOverflow = true; break; }
            ++finalTxCount;
        }
        // accepted/메모리 release/ESP32 수신 추정은 여기서 계수하지 않는다.
    }
    if (countOverflow) {
        // u16 wrap·포화 count로 완전성 선언하지 않는다. 현재 Query 종료 바인딩 HOLD.
    } else if (/* 현재 recipient에서 End를 응답 뒤에 송신할 순서 근거가 있음 */) {
        NetMobileQueryEnd end = {0}; // 이 0은 준비 저장소이며 정상 문맥 기본값이 아니다.
        end.context.recipient.device_context_id = /* ST03에 보존한 동일 현재 DeviceContextId */;
        end.context.recipient.session_id = /* ST03에 보존한 현재 recipient Session */;
        end.context.domain_boot_id = /* ST03에 연결된 실제 DOMAIN Boot */;
        end.context.query_id = /* ST03에 보존한 원 nonzero QueryID */;
        end.query_scope = /* ST03에 보존한 원 scope */;
        end.query_status = /* 위 상태 표에서 증거로 선택한 기존 NetQueryStatus 0..3 */;
        end.item_count = (uint16_t)finalTxCount;
        // 주석식에 해당하는 문맥·분류 근거를 실제로 확보하지 못하면 이 분기에 들어오지 않는다.
        NetSubmitResult r = DomainNetPriv_SendQueryEnd(&end);
        if (r != NET_SUBMIT_OK) {
            // 새 End 부분 접수 없음. 원 Query 미완료·실제 거부 원인 보존.
        } else {
            // End도 아직 로컬 접수. End의 final TX와 참조 해제를 뒤에서 따로 관측한다.
        }
    }
}
```

순회 대상은 새로 만든 가상 DB가 아니라 ST03가 기존 송신 항목과 연결해 보관해야 하는 관측 책임이다. 그 저장소·동기화·콜백 상관관계 구현이 미검증이면 이 정상 분기는 실행 승인되지 않는다. 종료 기한·수용창·retry 횟수를 새 숫자로 정하지 않는다. 합의된 recent 최대5는 다른 Query의 최대 목록 수를 정하지 않는다. 결과가 불명확한 응답을 새 LINK_SEQUENCE로 재송신해 N을 늘리지 않는다. Source Wire에는 ordinal이 없어 이런 재송신을 새로운 응답과 안전하게 구별할 근거가 부족하다.

ESP32는 같은 활성 문맥/scope/Boot의 유효 수신 수·중복 및 End count를 비교하고, 손실/순서불명/늦은 End는 목록 완료로 쓰지 않는다. UART final TX·ESP32 유효 RX·BLE 전달·App 목록 완료는 각각 별도 사실이다. `EspNetPriv_OnQueryEnd`의 void 반환은 App 완료가 아니다. Query 자체는 NetworkLocalFailure의 COMMAND/REQUEST_RESULT/OCCURRENCE가 아니므로 Query 실패를 임의 C2 후발 실패로 위조하지 않는다.

<a id="counterexamples"></a>
## 전문 반례 검토

| 사례 | 기대 처리·불변 조건 |
|---|---|
| SESSION_READY0인 최초 유효 Query | 등록·현재 peer와 선행 M_CONTEXT의 passive 검증부터 수행. Session 생성 안 함 |
| QueryID0·CURSOR1·scope0+과거 RequestID | 입력 형식 거부; Getter 호출/정상 수신 commit 없음 |
| Getter 오류인데 stack OUT은 이전 정상값 | OUT/outHasRecord/outCount 미소비, 부분/미완료 근거 보존 |
| 18 Getter 모두 실패 | status0/count0 금지; 전체 성공·정상 빈 집합 아님 |
| GetRecent 성공 count>capacity | 범위 밖 접근 금지; 계약 위반 제한. capacity값이 실제 저장소 한도여야 함 |
| 같은 값의 CAN snapshot이지만 원 Sequence 다름 | IF-O24 동일 입력 association 전 해당 projection 미제공 |
| 응답10 enqueue·8 final TX·2 미확정 | 성공 End 보류, count10/추정 count8 종료 금지 |
| 응답10 중8 final TX·2 확정 실패 | 같은 순서·현재 문맥 근거가 있으면 status2/count8 후보 |
| 자발 QUERY_ID0·중복 final callback·End 자기 자신 | count에 미합산; 같은 제출의 terminal을 중복 계수하지 않음 |
| final TX 뒤 release 전·release 뒤 outcome 미확정 | 첫 경우 frame 재사용 금지, 둘째 경우 identity/outcome 보존 및 End 보류 |
| 이전 Boot/Session의 늦은 End 또는 구 Driver callback | 종료 Query 재개/새 Query count·release 참여 금지 |
| End final TX가 성공해도 ESP32 count 불일치·BLE 전달 실패 | App 완료 선언 금지, 실제 부분 자료/원 품질·AGE 보존 |
| Window 항목 조회 | 원형/2 Wire 추적만 유지, 실제 Tx/Rx·표시·liveness 미활성 |

정적 문서 반례이며 실제 Getter·Driver·ESP32·BLE 시험은 수행하지 않았다. 관련 IF-O08/09/12/14/21/22/23/24/25, CF01, D-02/03/05, C2-H01은 기존 Owner35 아래 유지하고 S6 후 변경 큐에만 누적한다.

봉인 출처: S4A `api/domain_query.md`, `api/domain_private.md`, `api/domain_management.md`, `api/domain_submit.md`, `api/common.md`; B08 `UART_M_ROUTING.md#query`, `uart_m/private.md#m_query_end`, `uart_m/uplink.md#m_query`; S4C `TIME_IDENTITY_CONTRACT.md#query`, `FAILURE_RECOVERY_CONTRACT.md#query-recovery`, `RESOURCE_QUEUE_DRIVER_FLOW.md#queues`.

[입력 Wire](uart_m/M_QUERY.md#wire) · [종료 Wire](uart_m/M_QUERY_END.md#wire) · [Data 정본](../data/UART_M_DATA.md) · [M02](../modules/M02_QUERY_CONTROL.md) · [Flow](../flows/UART_M_QUERY_AND_PROJECTION.md)
