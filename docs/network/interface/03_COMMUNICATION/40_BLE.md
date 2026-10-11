# BLE — MOBILE ↔ ESP32 담당 경계

**연결 경로 확정 / 프로파일·Wire·성능 OPEN.** [ESP32 Data/API](../02_NODES/ESP32/00_OVERVIEW.md) · [UART-M v2](20_UART_M.md) · [팀 결정 항목](../06_OPEN_FOR_TEAM.md)

| 항목 | 책임·상태 |
| --- | --- |
| 생산·소비 | MOBILE 사용자 요청·조회·읽음·실제 App 활성 ↔ ESP32 Gateway 등록·현재 연결·Session 검증 |
| UART-M 연계 | 원 Request/Query/Warning Occurrence의 의미 보존. UART-M v2 / Context 38 B의 양 끝 정합 필요 |
| GATT Service/Characteristic UUID | OPEN. 과거 예제값 사용 금지 |
| Write/Notify/Indicate/CCCD | OPEN. 각 방향의 수용·전달 확인 수준을 구분해 결정 |
| ATT_MTU / fragmentation / reassembly | OPEN. UART overhead·CRC를 BLE 형식으로 대신 확정하지 않음 |
| connection interval / PHY / throughput | OPEN. 최종 단말 실측 NOT_EXECUTED |
| Session 발급·재접속·App 활성 확인 | ESP32/MOBILE 담당 TEAM_DECISION_REQUIRED. 연결 사실만으로 ACTIVE 합성 금지 |
| Protocol version | 기존 BLE_PROTOCOL_VERSION=1 참조는 endpoint 의존 정보. UART-M v2에 맞춰 자동 상향 금지 |

M_DOMAIN_ALIVE는 ESP32 private 링크 감시에서 끝나며 App 원 중계 대상이 아니다. M_QUERY_END는 조회 제어 종료 정보로 App에 의미 전달할 수 있지만 차량 실행 Result와 구분한다. ESP32의 SessionReady와 App의 실제 Query 응답 목록·ITEM_COUNT 완료 판단은 독립이다.

UART-M 하향 주기·집중 후보는 최대 2797/6111 wire B/s(WINDOW 설계 예약 포함)이며 BLE에 동일 byte 총량을 그대로 대입할 수 없다. UART SYNC·CRC·Alive 제거, BLE 의미 encoder, MTU별 분할·전송 횟수·재접속·ACK에 따라 달라진다. UART/CAN 여유로 BLE E2E 지연·무손실을 보장하지 않는다.

MOBILE이 주장하는 DeviceContext/Session 숫자를 그대로 신뢰하지 않는다. ESP32는 실제 등록된 현재 BLE peer에 묶어 검증해 DOMAIN으로 전달한다. 실제 보안 정책·암호·인증 방식을 이 문서에서 추가 확정하지 않는다. 링크 회복 뒤 과거 물리 동작 요청을 자동 재실행하지 않는다.
