# Network 상세설계 읽기

이 문서는 차량 노드 사이에서 의미 데이터가 전송 바이트로 바뀌고, 수신 노드의 타입 기반 API에 전달되는 과정을 설명한다. 현재 상세화한 범위는 **CAN FD 22 Wire·25 수신 경계, UART-M 23 Wire·23 수신 경계와 UART-C 3 Wire·3 수신 경계**다. DOMAIN Adapter와 Network 공통 Service는 이 메시지 정본을 기존 C2의 등록·수신·송신·조회·후발 관측 경계에 연결한다. CIS의 후방 관측 → DOMAIN 입력은 기존 대표 수직 흐름을 그대로 사용하며, 나머지 CAN 경로는 고유 자료·함수·미결 분기를 추가했다. 실제 양단 채택·Driver/보드 실행은 별도 확인 대상이다.

1. [Network 전체 개요](01_NETWORK_OVERVIEW.md)에서 역할과 처리 순서를 확인한다.
2. [노드·링크 지도](02_NODE_LINK_MAP.md)에서 담당 보드와 통신 구간을 찾는다.
3. [Layer·Module 색인](03_LAYER_MODULE_INDEX.md)에서 책임을 따라 하위 문서로 이동한다.
4. [CAN FD 전체 상세](04_CAN_FD_OVERVIEW.md)에서 22종의 양단 함수·Data·흐름을 찾는다.
5. [UART-M 전체 상세](05_UART_M_OVERVIEW.md)에서 상향4·하향17·private2와 공통 Stream, Query, 원 CAN 투영을 따라간다.
6. [UART-C 전체 상세](06_UART_C_OVERVIEW.md)에서 Pi 비전·상태와 CIS Permission 중계의 25필드·양방향 흐름을 확인한다.
7. [DOMAIN Adapter·공통 Service](07_DOMAIN_ADAPTER_SERVICE.md)에서 공개72·TxPort26·Observer11의 소유권과 등록·수신·송신·수명을 확인한다.
8. [노드별 Network 경계](08_NODE_BOUNDARIES.md)에서 8개 노드의 생산자·소비자·실패·수명과 17개 링크를 연결한다.
9. [종단 기능·오류·복구](09_E2E_FAILURE_RECOVERY.md)에서 SC01~SC18의 생산자·소비 Owner·정상/실패/복구와 실제 완료 경계를 따른다.
10. [CIS_REAR Data 정본](data/CIS_REAR_DATA.md)과 [전체 송수신 흐름](flows/CIS_REAR_144_TO_344.md)에서 양단 의미를 연결한다.

함수 문서는 원형, 매개변수와 수명, 정상·실패 동작, 한국어 C형 의사코드를 함께 제공한다. 기존 공개 선언과 새 내부 책임 후보는 구분한다. 내부 후보 원형은 제품 헤더나 실제 SDK API가 아니다.

CAN 필드 배치와 시간 수치에는 아직 양단 채택이 필요한 항목이 있다. 원 생산 기동·세대와 품질을 C2로 연결할 근거가 없으면 해당 인계를 진행하지 않는다. 문서를 작성했다는 사실은 실제 Driver·ABI·보드 동작이 검증됐다는 뜻이 아니다. 기존 [Interface 문서](../interface/00_START_HERE.md)의 C2 v0.6/Vehicle v0.3도 미승인 후보다.

요구103·Owner 미결35·Wire48/Rx51·시나리오18과 G6 당시 Interface39 봉인본을 보존한다. 기존 S6/B20/G6 PASS는 당시 기준선의 판정이며, SW 상세설계 후보 126개는 당시 PR #68 Commit `4b7fc906efb601901f4d5859ffd47817cd780fe8`의 r1 Interface 40개를 기준으로 동기화했다. 이후 Interface 교정과 일부 상세설계 내용의 보완은 별도 변경이며, 전체 상세설계의 최신 정합성·제품 ABI·양단 통신 채택 완료를 의미하지 않는다. 후보 profile, 원 기동/세대, CIS Environment, 실제 Driver·Clock·수명·same-observation·App/BLE 연결은 확인된 근거 범위에서만 사용한다. WINDOW 실행·표시·감시와 공식 게시의 기존 HOLD를 유지한다. 본문의 S6 이후 협의·승인 안내는 남은 미결과 후속 공식·제품 채택에 적용하며, 이미 완료한 r1 후보 동기화를 대기 상태로 되돌리는 의미가 아니다. 추가 협의 반영은 별도 근거·승인 범위를 따른다.
