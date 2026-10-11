# BCM 로컬 App/Network 인터페이스 — Module Map

**U1_INDEX_PRESERVED / U2_ACTIVE_DETAIL_CANDIDATE.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. BCM `INTERIOR_LIGHT`를 유지하고 `EXTERIOR_LIGHT`는 제외한다. S32K 측 Network 경계는 사용자 담당이며 기능 데이터·정책 owner는 해당 Application 담당자다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-bcm) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-bcm)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 10행을 모듈별로 묶었다. 이 모듈의 U2 ACTIVE 1개 함수만 `10_BEHAVIOR.md`와 `20_PSEUDOCODE.md`로 분리했다. 아래 상세 링크와 같은 FN-ID로 왕복한다. 나머지 함수는 상세 PLANNED_ONLY다. G2 독립 판정은 PENDING이며 외부 Peer와 WINDOW는 기존 보류·소유 경계를 유지한다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-bcm-bcmapp_ondomaindoorcommand"></a>[FN-BCM-BcmApp_OnDomainDoorCommand](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmapp_ondomaindoorcommand) | [BcmApp_OnDomainDoorCommand()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmapp_ondomaindoorcommand) |
| <a id="fn-bcm-bcmapp_ondomainclimatecommand"></a>[FN-BCM-BcmApp_OnDomainClimateCommand](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmapp_ondomainclimatecommand) | [BcmApp_OnDomainClimateCommand()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmapp_ondomainclimatecommand) |
| <a id="fn-bcm-bcmapp_ondomainlightcommand"></a>[FN-BCM-BcmApp_OnDomainLightCommand](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmapp_ondomainlightcommand) | [BcmApp_OnDomainLightCommand()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmapp_ondomainlightcommand) |
| <a id="fn-bcm-bcmnet_sendresult"></a>[FN-BCM-BcmNet_SendResult](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_sendresult) | [BcmNet_SendResult()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_sendresult) |
| <a id="fn-bcm-bcmnet_sendevent"></a>[FN-BCM-BcmNet_SendEvent](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_sendevent) | [BcmNet_SendEvent()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_sendevent) |
| <a id="fn-bcm-bcmnet_publishdoorstate"></a>[FN-BCM-BcmNet_PublishDoorState](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_publishdoorstate) | [BcmNet_PublishDoorState()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_publishdoorstate) |
| <a id="fn-bcm-bcmnet_publishclimatestate"></a>[FN-BCM-BcmNet_PublishClimateState](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_publishclimatestate) | [BcmNet_PublishClimateState()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_publishclimatestate) |
| <a id="fn-bcm-bcmnet_publishlightstate"></a>[FN-BCM-BcmNet_PublishLightState](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_publishlightstate) | [BcmNet_PublishLightState()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_publishlightstate) |
| <a id="fn-bcm-bcmnet_publishstatus"></a>[FN-BCM-BcmNet_PublishStatus](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnet_publishstatus) | [BcmNet_PublishStatus()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnet_publishstatus) |

<a id="network-private"></a>
## 같은 원 API 문서의 Network-private 경계

이 함수의 책임 Layer는 [Node Communication](../../01_NODE_COMMUNICATION/00_LAYER_OVERVIEW.md)이다. 노드 인터페이스 원본과 같이 찾도록 여기 한 번만 색인했으며 공개 App 호출 의무가 아니다.

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-bcm-bcmnetpriv_ondomainpermission"></a>[FN-BCM-BcmNetPriv_OnDomainPermission](../../../03_FUNCTION_INDEX.md#fn-bcm-bcmnetpriv_ondomainpermission) | [BcmNetPriv_OnDomainPermission()](../../../../interface/02_NODES/BCM/20_API.md#api-bcmnetpriv_ondomainpermission) |

## U2 ACTIVE 상세 후보

| FN-ID | Behavior | Pseudocode |
|---|---|---|
| `FN-BCM-BcmNetPriv_OnDomainPermission` | [동작](10_BEHAVIOR.md#fn-bcm-bcmnetpriv_ondomainpermission) | [의사코드](20_PSEUDOCODE.md#fn-bcm-bcmnetpriv_ondomainpermission) |
