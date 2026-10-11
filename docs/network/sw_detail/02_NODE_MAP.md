# Node 지도와 Source 역색인

[↑ Network 시작](00_START_HERE.md) · [Layer](01_LAYER_MAP.md) · [Function](03_FUNCTION_INDEX.md) · [Flow](04_FLOW_INDEX.md)

노드 배치·책임은 [기존 Architecture](../network_sw_architecture.md#7-node별-network-sw-적용-범위)와 [Interface 책임](../interface/01_NETWORK_OVERVIEW.md#section-node-responsibilities)을 따른다. 아래 MCU 표시는 논리 설계 배치이며 실보드·SDK 바인딩 검증 완료를 뜻하지 않는다.

| Node | 링크·소유 | Module / Function |
|---|---|---|
| <a id="node-domain"></a>DOMAIN · S32K344 | CAN+UART-M S32K측 Network. 기능 Permission·차량 정책은 DOMAIN Owner | [App Port](layers/02_APPLICATION_PORT/DOMAIN/00_MODULE_MAP.md#functions), [private](layers/01_NODE_COMMUNICATION/DOMAIN_PRIVATE/00_MODULE_MAP.md#functions), [함수](03_FUNCTION_INDEX.md#node-domain) |
| <a id="node-cis"></a>CIS · S32K144 | CAN+UART-C, 센서 원 관측·Pi provenance의 인계. Pi 앱은 외부 Owner | [App Port](layers/02_APPLICATION_PORT/CIS/00_MODULE_MAP.md#functions), [private relay](layers/02_APPLICATION_PORT/CIS/00_MODULE_MAP.md#network-private), [함수](03_FUNCTION_INDEX.md#node-cis) |
| <a id="node-bcm"></a>BCM · S32K144 | CAN. 도어·공조·실내 조명의 실행·결과·로컬 보호는 BCM Owner | [App Port](layers/02_APPLICATION_PORT/BCM/00_MODULE_MAP.md#functions), [private 감시](layers/02_APPLICATION_PORT/BCM/00_MODULE_MAP.md#network-private), [함수](03_FUNCTION_INDEX.md#node-bcm) |
| <a id="node-vss"></a>VSS · S32K344 | CAN. DOMAIN Event/Warning의 음향 중재·실제 출력은 VSS Owner | [App Port](layers/02_APPLICATION_PORT/VSS/00_MODULE_MAP.md#functions), [private 감시](layers/02_APPLICATION_PORT/VSS/00_MODULE_MAP.md#network-private), [함수](03_FUNCTION_INDEX.md#node-vss) |
| <a id="node-window"></a>WINDOW · 예정 S32K144 | CAN 설계만 유지. **DEFERRED / [구현 보류 — 설계 유지]** | [보류 App Port](layers/02_APPLICATION_PORT/WINDOW/00_MODULE_MAP.md#functions), [함수](03_FUNCTION_INDEX.md#node-window) |
| <a id="node-esp32"></a>ESP32 · 외부 Peer | UART-M/BLE 계약 참고. Gateway·ESP-IDF 내부 구현은 외부 담당 | [Peer API](layers/02_APPLICATION_PORT/ESP32/00_MODULE_MAP.md#functions), [Peer private](layers/01_NODE_COMMUNICATION/ESP32_PRIVATE/00_MODULE_MAP.md#functions), [함수](03_FUNCTION_INDEX.md#node-esp32) |
| <a id="node-pi"></a>Pi · 외부 Peer | UART-C 계약 참고. Vision/Application 내부 구현은 외부 담당 | [Peer API](layers/02_APPLICATION_PORT/PI/00_MODULE_MAP.md#functions), [함수](03_FUNCTION_INDEX.md#node-pi) |

공통 CAN/Codec/Transport 자료는 [Layer 지도](01_LAYER_MAP.md)에 한 번 연결하며 노드별로 복제하지 않는다. BCM `INTERIOR_LIGHT`는 유지하고 `EXTERIOR_LIGHT`는 현재 구현 범위에서 제외한다. 제품 UART는 Pi↔CIS 및 ESP32↔DOMAIN이며 별도 B2의 144↔344 실험 UART와 구분한다.

<a id="source-coverage"></a>
## Node별 기존 Source — 28개

Source 상태를 확인한 뒤 관련 Module에서 동일 함수의 출처를 찾는다. 이 표는 노드 문서 전체를 직접 연결하며 Header·Data 내용을 복사하지 않는다.

| Node | 개요·Data·API 및 추가 Source |
|---|---|
| DOMAIN | [개요](../interface/02_NODES/DOMAIN/00_OVERVIEW.md), [Data 입구](../interface/02_NODES/DOMAIN/10_DATA.md), [Vehicle 타입](../interface/02_NODES/DOMAIN/11_VEHICLE_TYPES.md), [C2 타입](../interface/02_NODES/DOMAIN/12_DOMAIN_TYPES.md), [Mobile Codec Data](../interface/02_NODES/DOMAIN/14_MOBILE_CODEC_DATA.md), [API](../interface/02_NODES/DOMAIN/20_API.md), [private routing](../interface/02_NODES/DOMAIN/25_PRIVATE_ROUTING.md) |
| CIS | [개요](../interface/02_NODES/CIS/00_OVERVIEW.md), [Data](../interface/02_NODES/CIS/10_DATA.md), [API](../interface/02_NODES/CIS/20_API.md) |
| BCM | [개요](../interface/02_NODES/BCM/00_OVERVIEW.md), [Data](../interface/02_NODES/BCM/10_DATA.md), [API](../interface/02_NODES/BCM/20_API.md) |
| VSS | [개요](../interface/02_NODES/VSS/00_OVERVIEW.md), [Data](../interface/02_NODES/VSS/10_DATA.md), [API](../interface/02_NODES/VSS/20_API.md) |
| WINDOW | [개요](../interface/02_NODES/WINDOW/00_OVERVIEW.md), [Data](../interface/02_NODES/WINDOW/10_DATA.md), [API](../interface/02_NODES/WINDOW/20_API.md) — 모두 구현 보류 |
| ESP32 | [개요](../interface/02_NODES/ESP32/00_OVERVIEW.md), [Data 입구](../interface/02_NODES/ESP32/10_DATA.md), [Gateway 타입](../interface/02_NODES/ESP32/11_GATEWAY_TYPES.md), [Interface 타입](../interface/02_NODES/ESP32/12_GATEWAY_INTERFACE_TYPES.md), [API](../interface/02_NODES/ESP32/20_API.md), [private routing](../interface/02_NODES/ESP32/25_PRIVATE_ROUTING.md) — 외부 Peer 참고 |
| Pi | [개요](../interface/02_NODES/PI/00_OVERVIEW.md), [Data](../interface/02_NODES/PI/10_DATA.md), [API](../interface/02_NODES/PI/20_API.md) — 외부 Peer 참고 |

위 28개와 [Network 시작의 공통·통신·색인 11개](00_START_HERE.md#기존-정본과-전체-source)를 합쳐 기존 Interface **39/39 문서**를 발견할 수 있다. Architecture/Network 배경 2개까지 포함한 원 Source는 **41개**다.

<a id="missing-source"></a>
## 원본 C2 Header 부재

`MISSING_SOURCE / OPEN-C2-SOURCE`인 세 경로는 현재 Source SHA에서 실파일이 없다. 문서상 선언과 현재 구헤더를 동일시하지 않으며 존재하지 않는 경로에 새 링크를 만들지 않는다.

- `docs/network/interface/DECLARED_HEADERS/DOMAIN/Domain_Interface_v0.5_C2_target.h`
- `docs/network/interface/DECLARED_HEADERS/DOMAIN/Vehicle_Types_v0.2.h`
- `docs/network/interface/DECLARED_HEADERS/ESP32/Gateway_Interface.h`

실존 문서의 원형 탐색은 가능하며 실제 대상 Header 확보·바인딩은 별도 OPEN이다. [Function 색인의 근거 구분](03_FUNCTION_INDEX.md#source-evidence)을 함께 확인한다.

추가 Source 점검에서 `docs/network/interface/DECLARED_HEADERS/ESP32/Gateway_Types.h`도 부재임을 확인했다. 원 참조는 [ESP32 Gateway 타입 문서](../interface/02_NODES/ESP32/11_GATEWAY_TYPES.md)에 있으며 `KNOWN_PREEXISTING / OPEN-GW-TYPES-SOURCE`로 별도 이월한다. 기존 `OPEN-C2-SOURCE` 세 경로를 변경하거나 다른 실존 Gateway 헤더를 같은 Snapshot으로 치환하지 않는다.
