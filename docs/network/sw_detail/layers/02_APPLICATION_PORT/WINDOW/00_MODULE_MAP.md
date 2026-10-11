# WINDOW 로컬 App/Network 인터페이스 — Module Map

**INDEX_ONLY / BEHAVIOR_NOT_DESIGNED.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. **[구현 보류 — 설계 유지] / DEFERRED**이며 활성 구현 대상이 아니다. S32K 측 Network 경계는 사용자 담당이며 기능 데이터·정책 owner는 해당 Application 담당자다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-window) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-window)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 6행을 모듈별로 묶었다. 향후 파일은 이 폴더의 `10_BEHAVIOR.md`(동작 설명)와 `20_PSEUDOCODE.md`(한국어 C형 의사코드)로 분리한다. 둘 다 **PLANNED_ONLY / NO_BEHAVIOR / 미생성**이며 아직 링크하지 않는다. 함수별 같은 FN-ID와 상호/상위 복귀 링크를 후속 승인 범위에서 사용한다. 외부 Peer와 WINDOW의 내부 상세 구현 권한은 생기지 않는다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-window-windowapp_ondomainpermission"></a>[FN-WINDOW-WindowApp_OnDomainPermission](../../../03_FUNCTION_INDEX.md#fn-window-windowapp_ondomainpermission) | [WindowApp_OnDomainPermission()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windowapp_ondomainpermission) |
| <a id="fn-window-windowapp_ondomaincommand"></a>[FN-WINDOW-WindowApp_OnDomainCommand](../../../03_FUNCTION_INDEX.md#fn-window-windowapp_ondomaincommand) | [WindowApp_OnDomainCommand()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windowapp_ondomaincommand) |
| <a id="fn-window-windownet_sendevent"></a>[FN-WINDOW-WindowNet_SendEvent](../../../03_FUNCTION_INDEX.md#fn-window-windownet_sendevent) | [WindowNet_SendEvent()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windownet_sendevent) |
| <a id="fn-window-windownet_sendresult"></a>[FN-WINDOW-WindowNet_SendResult](../../../03_FUNCTION_INDEX.md#fn-window-windownet_sendresult) | [WindowNet_SendResult()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windownet_sendresult) |
| <a id="fn-window-windownet_publishstate"></a>[FN-WINDOW-WindowNet_PublishState](../../../03_FUNCTION_INDEX.md#fn-window-windownet_publishstate) | [WindowNet_PublishState()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windownet_publishstate) |
| <a id="fn-window-windownet_publishfault"></a>[FN-WINDOW-WindowNet_PublishFault](../../../03_FUNCTION_INDEX.md#fn-window-windownet_publishfault) | [WindowNet_PublishFault()](../../../../interface/02_NODES/WINDOW/20_API.md#api-windownet_publishfault) |
