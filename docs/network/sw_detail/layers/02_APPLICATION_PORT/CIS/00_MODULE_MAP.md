# CIS 로컬 App/Network 인터페이스 — Module Map

**U1_INDEX_PRESERVED / U2_ACTIVE_DETAIL_CANDIDATE.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. S32K 측 Network 경계는 사용자 담당이며 기능 데이터·정책 owner는 해당 Application 담당자다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-cis) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-cis)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 8행을 모듈별로 묶었다. 이 모듈의 U2 ACTIVE 2개 함수만 `10_BEHAVIOR.md`와 `20_PSEUDOCODE.md`로 분리했다. 아래 상세 링크와 같은 FN-ID로 왕복한다. 나머지 함수는 상세 PLANNED_ONLY다. G2 독립 판정은 PENDING이며 외부 Peer와 WINDOW는 기존 보류·소유 경계를 유지한다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-cis-cisapp_ondomainpermission"></a>[FN-CIS-CisApp_OnDomainPermission](../../../03_FUNCTION_INDEX.md#fn-cis-cisapp_ondomainpermission) | [CisApp_OnDomainPermission()](../../../../interface/02_NODES/CIS/20_API.md#api-cisapp_ondomainpermission) |
| <a id="fn-cis-cisnet_publishrear"></a>[FN-CIS-CisNet_PublishRear](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishrear) | [CisNet_PublishRear()](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishrear) |
| <a id="fn-cis-cisnet_publishenvironment"></a>[FN-CIS-CisNet_PublishEnvironment](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishenvironment) | [CisNet_PublishEnvironment()](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishenvironment) |
| <a id="fn-cis-cisnet_publishoccupant"></a>[FN-CIS-CisNet_PublishOccupant](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishoccupant) | [CisNet_PublishOccupant()](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishoccupant) |
| <a id="fn-cis-cisnet_publishstatus"></a>[FN-CIS-CisNet_PublishStatus](../../../03_FUNCTION_INDEX.md#fn-cis-cisnet_publishstatus) | [CisNet_PublishStatus()](../../../../interface/02_NODES/CIS/20_API.md#api-cisnet_publishstatus) |
| <a id="fn-cis-cisapp_onpivisionresult"></a>[FN-CIS-CisApp_OnPiVisionResult](../../../03_FUNCTION_INDEX.md#fn-cis-cisapp_onpivisionresult) | [CisApp_OnPiVisionResult()](../../../../interface/02_NODES/CIS/20_API.md#api-cisapp_onpivisionresult) |
| <a id="fn-cis-cisapp_onpivisionstatus"></a>[FN-CIS-CisApp_OnPiVisionStatus](../../../03_FUNCTION_INDEX.md#fn-cis-cisapp_onpivisionstatus) | [CisApp_OnPiVisionStatus()](../../../../interface/02_NODES/CIS/20_API.md#api-cisapp_onpivisionstatus) |

<a id="network-private"></a>
## 같은 원 API 문서의 Network-private 경계

이 함수의 책임 Layer는 [Node Communication](../../01_NODE_COMMUNICATION/00_LAYER_OVERVIEW.md)이다. 노드 인터페이스 원본과 같이 찾도록 여기 한 번만 색인했으며 공개 App 호출 의무가 아니다.

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-cis-cisnetpriv_relaypipermission"></a>[FN-CIS-CisNetPriv_RelayPiPermission](../../../03_FUNCTION_INDEX.md#fn-cis-cisnetpriv_relaypipermission) | [CisNetPriv_RelayPiPermission()](../../../../interface/02_NODES/CIS/20_API.md#api-cisnetpriv_relaypipermission) |

## U2 ACTIVE 상세 후보

| FN-ID | Behavior | Pseudocode |
|---|---|---|
| `FN-CIS-CisNet_PublishRear` | [동작](10_BEHAVIOR.md#fn-cis-cisnet_publishrear) | [의사코드](20_PSEUDOCODE.md#fn-cis-cisnet_publishrear) |
| `FN-CIS-CisApp_OnDomainPermission` | [동작](10_BEHAVIOR.md#fn-cis-cisapp_ondomainpermission) | [의사코드](20_PSEUDOCODE.md#fn-cis-cisapp_ondomainpermission) |
