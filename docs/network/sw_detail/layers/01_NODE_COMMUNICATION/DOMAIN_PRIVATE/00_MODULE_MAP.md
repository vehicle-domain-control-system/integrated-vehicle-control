# DOMAIN Network 내부 경계 — Module Map

**U1_INDEX_PRESERVED / U2_ACTIVE_DETAIL_CANDIDATE.** 기존 private routing 문서에 있는 typed 투영·문맥·조회 경계를 찾는다. S32K 측 Network 경계는 사용자 담당이며 기능 데이터·정책 owner는 해당 Application 담당자다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-domain) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-domain)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 25행을 모듈별로 묶었다. 이 모듈의 U2 ACTIVE 1개 함수만 `10_BEHAVIOR.md`와 `20_PSEUDOCODE.md`로 분리했다. 아래 상세 링크와 같은 FN-ID로 왕복한다. 나머지 함수는 상세 PLANNED_ONLY다. G2 독립 판정은 PENDING이며 외부 Peer와 WINDOW는 기존 보류·소유 경계를 유지한다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-domain-domainnet_publishpermission"></a>[FN-DOMAIN-DomainNet_PublishPermission](../../../03_FUNCTION_INDEX.md#fn-domain-domainnet_publishpermission) | [DomainNet_PublishPermission()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnet_publishpermission) |
| <a id="fn-domain-domainnetpriv_onwindowevent"></a>[FN-DOMAIN-DomainNetPriv_OnWindowEvent](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_onwindowevent) | [DomainNetPriv_OnWindowEvent()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_onwindowevent) |
| <a id="fn-domain-domainnetpriv_onwindowfault"></a>[FN-DOMAIN-DomainNetPriv_OnWindowFault](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_onwindowfault) | [DomainNetPriv_OnWindowFault()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_onwindowfault) |
| <a id="fn-domain-domainnetpriv_onvssstatus"></a>[FN-DOMAIN-DomainNetPriv_OnVssStatus](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_onvssstatus) | [DomainNetPriv_OnVssStatus()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_onvssstatus) |
| <a id="fn-domain-domainnetpriv_ongatewaycontext"></a>[FN-DOMAIN-DomainNetPriv_OnGatewayContext](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_ongatewaycontext) | [DomainNetPriv_OnGatewayContext()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_ongatewaycontext) |
| <a id="fn-domain-domainnetpriv_processquery"></a>[FN-DOMAIN-DomainNetPriv_ProcessQuery](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_processquery) | [DomainNetPriv_ProcessQuery()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_processquery) |
| <a id="fn-domain-domainnetpriv_publishalive"></a>[FN-DOMAIN-DomainNetPriv_PublishAlive](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_publishalive) | [DomainNetPriv_PublishAlive()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_publishalive) |
| <a id="fn-domain-domainnetpriv_sendqueryend"></a>[FN-DOMAIN-DomainNetPriv_SendQueryEnd](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_sendqueryend) | [DomainNetPriv_SendQueryEnd()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_sendqueryend) |
| <a id="fn-domain-domainnetpriv_submitmobilerequestresult"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileRequestResult](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilerequestresult) | [DomainNetPriv_SubmitMobileRequestResult()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilerequestresult) |
| <a id="fn-domain-domainnetpriv_submitmobilewarning"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileWarning](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilewarning) | [DomainNetPriv_SubmitMobileWarning()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilewarning) |
| <a id="fn-domain-domainnetpriv_submitmobilefunctionavailability"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileFunctionAvailability](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilefunctionavailability) | [DomainNetPriv_SubmitMobileFunctionAvailability()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilefunctionavailability) |
| <a id="fn-domain-domainnetpriv_submitmobiledigitalkeystate"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileDigitalKeyState](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobiledigitalkeystate) | [DomainNetPriv_SubmitMobileDigitalKeyState()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobiledigitalkeystate) |
| <a id="fn-domain-domainnetpriv_submitmobiledigitalkeyresult"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileDigitalKeyResult](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobiledigitalkeyresult) | [DomainNetPriv_SubmitMobileDigitalKeyResult()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobiledigitalkeyresult) |
| <a id="fn-domain-domainnetpriv_submitmobileusersettings"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileUserSettings](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobileusersettings) | [DomainNetPriv_SubmitMobileUserSettings()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobileusersettings) |
| <a id="fn-domain-domainnetpriv_submitmobilebcmdoorstate"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileBcmDoorState](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilebcmdoorstate) | [DomainNetPriv_SubmitMobileBcmDoorState()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilebcmdoorstate) |
| <a id="fn-domain-domainnetpriv_submitmobilebcmclimatestate"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileBcmClimateState](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilebcmclimatestate) | [DomainNetPriv_SubmitMobileBcmClimateState()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilebcmclimatestate) |
| <a id="fn-domain-domainnetpriv_submitmobilebcminteriorlightstate"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileBcmInteriorLightState](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilebcminteriorlightstate) | [DomainNetPriv_SubmitMobileBcmInteriorLightState()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilebcminteriorlightstate) |
| <a id="fn-domain-domainnetpriv_submitmobilebcmstatus"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileBcmStatus](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilebcmstatus) | [DomainNetPriv_SubmitMobileBcmStatus()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilebcmstatus) |
| <a id="fn-domain-domainnetpriv_submitmobilecisenvironment"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileCisEnvironment](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilecisenvironment) | [DomainNetPriv_SubmitMobileCisEnvironment()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilecisenvironment) |
| <a id="fn-domain-domainnetpriv_submitmobilecisoccupant"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileCisOccupant](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilecisoccupant) | [DomainNetPriv_SubmitMobileCisOccupant()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilecisoccupant) |
| <a id="fn-domain-domainnetpriv_submitmobilecisrear"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileCisRear](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilecisrear) | [DomainNetPriv_SubmitMobileCisRear()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilecisrear) |
| <a id="fn-domain-domainnetpriv_submitmobilecisstatus"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileCisStatus](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilecisstatus) | [DomainNetPriv_SubmitMobileCisStatus()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilecisstatus) |
| <a id="fn-domain-domainnetpriv_submitmobilevssstatus"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileVssStatus](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilevssstatus) | [DomainNetPriv_SubmitMobileVssStatus()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilevssstatus) |
| <a id="fn-domain-domainnetpriv_submitmobilewindowstate"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileWindowState](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilewindowstate) | [DomainNetPriv_SubmitMobileWindowState()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilewindowstate) |
| <a id="fn-domain-domainnetpriv_submitmobilewindowfault"></a>[FN-DOMAIN-DomainNetPriv_SubmitMobileWindowFault](../../../03_FUNCTION_INDEX.md#fn-domain-domainnetpriv_submitmobilewindowfault) | [DomainNetPriv_SubmitMobileWindowFault()](../../../../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md#api-domainnetpriv_submitmobilewindowfault) |

C2 최신 헤더 원본 부재는 [OPEN-C2-SOURCE](../../../03_FUNCTION_INDEX.md#missing-source)로 유지한다. 구 헤더와 동일시하지 않는다.

`DomainNet_PublishPermission()`은 U0 private 목록에서 누락됐던 **기존 Source 선언** 1개다. 신규 API를 추가한 것이 아니며 원 Producer/150 ms 합의나 composer 구현은 OPEN이다.

## U2 ACTIVE 상세 후보

| FN-ID | Behavior | Pseudocode |
|---|---|---|
| `FN-DOMAIN-DomainNet_PublishPermission` | [동작](10_BEHAVIOR.md#fn-domain-domainnet_publishpermission) | [의사코드](20_PSEUDOCODE.md#fn-domain-domainnet_publishpermission) |
