# VSS 로컬 App/Network 인터페이스 — Module Map

**INDEX_ONLY / BEHAVIOR_NOT_DESIGNED.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. S32K 측 Network 경계는 사용자 담당이며 기능 데이터·정책 owner는 해당 Application 담당자다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-vss) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-vss)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 4행을 모듈별로 묶었다. 향후 파일은 이 폴더의 `10_BEHAVIOR.md`(동작 설명)와 `20_PSEUDOCODE.md`(한국어 C형 의사코드)로 분리한다. 둘 다 **PLANNED_ONLY / NO_BEHAVIOR / 미생성**이며 아직 링크하지 않는다. 함수별 같은 FN-ID와 상호/상위 복귀 링크를 후속 승인 범위에서 사용한다. 외부 Peer와 WINDOW의 내부 상세 구현 권한은 생기지 않는다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-vss-vssapp_ondomainwarningstate"></a>[FN-VSS-VssApp_OnDomainWarningState](../../../03_FUNCTION_INDEX.md#fn-vss-vssapp_ondomainwarningstate) | [VssApp_OnDomainWarningState()](../../../../interface/02_NODES/VSS/20_API.md#api-vssapp_ondomainwarningstate) |
| <a id="fn-vss-vssapp_ondomainevent"></a>[FN-VSS-VssApp_OnDomainEvent](../../../03_FUNCTION_INDEX.md#fn-vss-vssapp_ondomainevent) | [VssApp_OnDomainEvent()](../../../../interface/02_NODES/VSS/20_API.md#api-vssapp_ondomainevent) |
| <a id="fn-vss-vssnet_publishstatus"></a>[FN-VSS-VssNet_PublishStatus](../../../03_FUNCTION_INDEX.md#fn-vss-vssnet_publishstatus) | [VssNet_PublishStatus()](../../../../interface/02_NODES/VSS/20_API.md#api-vssnet_publishstatus) |

<a id="network-private"></a>
## 같은 원 API 문서의 Network-private 경계

이 함수의 책임 Layer는 [Node Communication](../../01_NODE_COMMUNICATION/00_LAYER_OVERVIEW.md)이다. 노드 인터페이스 원본과 같이 찾도록 여기 한 번만 색인했으며 공개 App 호출 의무가 아니다.

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-vss-vssnetpriv_ondomainpermission"></a>[FN-VSS-VssNetPriv_OnDomainPermission](../../../03_FUNCTION_INDEX.md#fn-vss-vssnetpriv_ondomainpermission) | [VssNetPriv_OnDomainPermission()](../../../../interface/02_NODES/VSS/20_API.md#api-vssnetpriv_ondomainpermission) |
