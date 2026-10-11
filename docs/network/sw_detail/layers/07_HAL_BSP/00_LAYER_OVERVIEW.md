# HAL Adapter / BSP

[↑ Layer 지도](../../01_LAYER_MAP.md) · [Network 시작](../../00_START_HERE.md) · [Node](../../02_NODE_MAP.md)

기존 [HAL/BSP 책임](../../../network_sw_architecture.md#4-layer별-책임)은 vendor SDK·MCU·보드 차이를 격리하는 것이다. Network semantic이나 기능 정책을 소유하지 않는다. 공통 상위 소스·의미를 보존하고 노드별 Backend 차이를 별도 근거로 확인한다.

| 탐색 근거 | 현재 지위 |
|---|---|
| [미확정 실제 타깃 항목](../../../network_sw_architecture.md#11-현재-확정하지-않는-항목) | SDK/RTD·board revision·핀·전압·transceiver·termination·timing·자원 OPEN |
| [Node별 책임](../../../network_sw_architecture.md#7-node별-network-sw-적용-범위) | 144/344 차이를 유지하며 물리검증 완료로 해석하지 않음 |
| [Driver](../06_DRIVER_PORT/00_LAYER_OVERVIEW.md) | 실제 SDK primitive와 상위 Port 의미의 연결 경계 |

현행 Network 트리에 실제 타깃 프로젝트·HAL 소스·빌드/실측 증거가 없다(`SOURCE_ONLY / IMPLEMENTATION_NOT_PRESENT / TARGET_BINDING_OPEN`). U1에서 보드별 빈 구현 파일이나 함수 Module을 만들지 않는다.

메인 경로의 실제 HAL/핀/OS binding은 **U10**에서 G9 + 해당 노드 B3 + SDK/보드 근거 + 허용된 계약 범위를 함께 확인한 뒤 별도 단계로 진행한다. Host/Mock·타깃 빌드·Board 시험 상태는 독립이다. 현재 상세 Behavior/Pseudocode는 `PLANNED_ONLY / BEHAVIOR_NOT_DESIGNED`다.
