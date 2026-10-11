# PI 로컬 App/Network 인터페이스 — Module Map

**INDEX_ONLY / BEHAVIOR_NOT_DESIGNED.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. ESP32/Pi 담당자 소유의 Peer 계약 참고. 사용자 내부 구현 대상이 아니다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-pi) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-pi)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 3행을 모듈별로 묶었다. 향후 파일은 이 폴더의 `10_BEHAVIOR.md`(동작 설명)와 `20_PSEUDOCODE.md`(한국어 C형 의사코드)로 분리한다. 둘 다 **PLANNED_ONLY / NO_BEHAVIOR / 미생성**이며 아직 링크하지 않는다. 함수별 같은 FN-ID와 상호/상위 복귀 링크를 후속 승인 범위에서 사용한다. 외부 Peer와 WINDOW의 내부 상세 구현 권한은 생기지 않는다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-pi-pinet_publishvisionresult"></a>[FN-PI-PiNet_PublishVisionResult](../../../03_FUNCTION_INDEX.md#fn-pi-pinet_publishvisionresult) | [PiNet_PublishVisionResult()](../../../../interface/02_NODES/PI/20_API.md#api-pinet_publishvisionresult) |
| <a id="fn-pi-pinet_publishvisionstatus"></a>[FN-PI-PiNet_PublishVisionStatus](../../../03_FUNCTION_INDEX.md#fn-pi-pinet_publishvisionstatus) | [PiNet_PublishVisionStatus()](../../../../interface/02_NODES/PI/20_API.md#api-pinet_publishvisionstatus) |
| <a id="fn-pi-piapp_oncispermission"></a>[FN-PI-PiApp_OnCisPermission](../../../03_FUNCTION_INDEX.md#fn-pi-piapp_oncispermission) | [PiApp_OnCisPermission()](../../../../interface/02_NODES/PI/20_API.md#api-piapp_oncispermission) |
