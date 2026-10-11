# ESP32 로컬 App/Network 인터페이스 — Module Map

**INDEX_ONLY / BEHAVIOR_NOT_DESIGNED.** 기존 노드 API 문서에 있는 로컬 typed 인계 경계를 찾는다. ESP32/Pi 담당자 소유의 Peer 계약 참고. 사용자 내부 구현 대상이 아니다.

[상위 Layer](../00_LAYER_OVERVIEW.md) · [Network 입구](../../../00_START_HERE.md) · [Node](../../../02_NODE_MAP.md#node-esp32) · [Function 색인](../../../03_FUNCTION_INDEX.md#node-esp32)

기존 입력/출력·호출자·반환·수명·계약 상태는 각 함수 Source에서 읽는다. local `NET_SUBMIT_OK`는 안정된 로컬 인계이며 TX 완료·상대 수신·ECU ACCEPTED·차량 DONE·ALLOW가 아니다. 실제 Port 연결은 `BINDING_OPEN`이다.

현재 13행을 모듈별로 묶었다. 향후 파일은 이 폴더의 `10_BEHAVIOR.md`(동작 설명)와 `20_PSEUDOCODE.md`(한국어 C형 의사코드)로 분리한다. 둘 다 **PLANNED_ONLY / NO_BEHAVIOR / 미생성**이며 아직 링크하지 않는다. 함수별 같은 FN-ID와 상호/상위 복귀 링크를 후속 승인 범위에서 사용한다. 외부 Peer와 WINDOW의 내부 상세 구현 권한은 생기지 않는다.

<a id="functions"></a>
## Function → 기존 Source

| FN-ID → 색인/모듈 복귀 | 기존 함수 선언 Source |
|---|---|
| <a id="fn-esp32-gateway_interface_init"></a>[FN-ESP32-Gateway_Interface_Init](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_init) | [Gateway_Interface_Init()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_init) |
| <a id="fn-esp32-gateway_interface_reset"></a>[FN-ESP32-Gateway_Interface_Reset](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_reset) | [Gateway_Interface_Reset()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_reset) |
| <a id="fn-esp32-gateway_interface_isinitialized"></a>[FN-ESP32-Gateway_Interface_IsInitialized](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_isinitialized) | [Gateway_Interface_IsInitialized()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_isinitialized) |
| <a id="fn-esp32-gateway_interface_onmobilemessage"></a>[FN-ESP32-Gateway_Interface_OnMobileMessage](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_onmobilemessage) | [Gateway_Interface_OnMobileMessage()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_onmobilemessage) |
| <a id="fn-esp32-gateway_interface_ondomainmessage"></a>[FN-ESP32-Gateway_Interface_OnDomainMessage](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_ondomainmessage) | [Gateway_Interface_OnDomainMessage()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_ondomainmessage) |
| <a id="fn-esp32-gateway_interface_sendtodomain"></a>[FN-ESP32-Gateway_Interface_SendToDomain](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_sendtodomain) | [Gateway_Interface_SendToDomain()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_sendtodomain) |
| <a id="fn-esp32-gateway_interface_sendtomobile"></a>[FN-ESP32-Gateway_Interface_SendToMobile](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_sendtomobile) | [Gateway_Interface_SendToMobile()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_sendtomobile) |
| <a id="fn-esp32-gateway_interface_publishregistrationconnection"></a>[FN-ESP32-Gateway_Interface_PublishRegistrationConnection](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_publishregistrationconnection) | [Gateway_Interface_PublishRegistrationConnection()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_publishregistrationconnection) |
| <a id="fn-esp32-gateway_interface_publishappactivity"></a>[FN-ESP32-Gateway_Interface_PublishAppActivity](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_publishappactivity) | [Gateway_Interface_PublishAppActivity()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_publishappactivity) |
| <a id="fn-esp32-gateway_interface_publishproximity"></a>[FN-ESP32-Gateway_Interface_PublishProximity](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_publishproximity) | [Gateway_Interface_PublishProximity()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_publishproximity) |
| <a id="fn-esp32-gateway_interface_isvalidbyteview"></a>[FN-ESP32-Gateway_Interface_IsValidByteView](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_isvalidbyteview) | [Gateway_Interface_IsValidByteView()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_isvalidbyteview) |
| <a id="fn-esp32-gateway_interface_isvalidmessagecontext"></a>[FN-ESP32-Gateway_Interface_IsValidMessageContext](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_isvalidmessagecontext) | [Gateway_Interface_IsValidMessageContext()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_isvalidmessagecontext) |
| <a id="fn-esp32-gateway_interface_isvalidrelaymessage"></a>[FN-ESP32-Gateway_Interface_IsValidRelayMessage](../../../03_FUNCTION_INDEX.md#fn-esp32-gateway_interface_isvalidrelaymessage) | [Gateway_Interface_IsValidRelayMessage()](../../../../interface/02_NODES/ESP32/20_API.md#api-gateway_interface_isvalidrelaymessage) |
