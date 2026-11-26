# BLE — MOBILE ↔ ESP32 담당 경계

Interface v2 · r1 · [규격과 연동 조건](../07_CROSS_NODE_INTERFACE_RULES.md#status)

**연결 경로 확정 / 프로파일·Wire·성능 OPEN.** [ESP32 Data/API](../02_NODES/ESP32/00_OVERVIEW.md) · [UART-M v2](20_UART_M.md) · [기술 제약과 연동 조건](../06_TECHNICAL_CONSTRAINTS.md)

| 항목 | 책임·상태 |
| --- | --- |
| 생산·소비 | MOBILE 사용자 요청·조회·읽음·실제 App 활성 ↔ ESP32 Gateway 등록·현재 연결·Session 검증 |
| UART-M 연계 | 원본 Request/Query/Warning Occurrence의 의미 보존. UART-M v2 / Context 38 B의 양 끝 정합 필요 |
| GATT Service/Characteristic UUID | OPEN. 과거 예제값 사용 금지 |
| Write/Notify/Indicate/CCCD | OPEN. 각 방향의 수용·전달 확인 수준을 구분해 결정 |
| ATT_MTU / fragmentation / reassembly | OPEN. UART overhead·CRC를 BLE 형식으로 대신 확정하지 않음 |
| connection interval / PHY / throughput | OPEN. 최종 단말 실측 NOT_EXECUTED |
| Session 발급·재접속·App 활성 확인 | ESP32/MOBILE 담당 FUNCTION_POLICY_OPEN. 연결 사실만으로 ACTIVE 합성 금지 |
| Protocol version | 기존 BLE_PROTOCOL_VERSION=1 참조는 endpoint 의존 정보. UART-M v2에 맞춰 자동 상향 금지 |

M_DOMAIN_ALIVE는 ESP32 private 링크 감시에서 끝나며 App 원 중계 대상이 아니다. M_QUERY_END는 조회 제어 종료 정보로 App에 의미 전달할 수 있지만 차량 기능의 실행 Result와 구분한다. ESP32의 SessionReady와 App의 실제 Query 응답 목록·ITEM_COUNT 완료 판단은 독립이다.

UART-M 하향 주기·집중 후보는 최대 2797/5949 wire B/s(WINDOW 설계 예약 포함)이며 최근 결과 조회의 최대 5건 제한을 반영한 [상위 통신량 계산](../../network_design_draft_v0.1.md#uart-load)을 따른다. 이는 설계 계산 후보이며 BLE에 동일한 바이트 총량을 그대로 대입할 수 없다. UART SYNC·CRC·Alive 제거, BLE 의미 encoder, MTU별 분할·전송 횟수·재접속·ACK에 따라 달라진다. UART/CAN 여유로 BLE E2E 지연·무손실을 보장하지 않는다.

MOBILE이 주장하는 DeviceContext/Session 숫자를 그대로 신뢰하지 않는다. ESP32는 실제 등록된 현재 BLE 상대 기기에 묶어 검증해 DOMAIN으로 전달한다. 실제 보안 정책·암호·인증 방식을 이 문서에서 추가 확정하지 않는다. 링크 회복 뒤 과거 물리 동작 요청을 자동 재실행하지 않는다.

<a id="p4a-adoption"></a>
## P4-A 양단 채택 확인 조건

ESP32의 P4-A 호환 구현은 담당자 보고 지위이며, Network가 적용할 실제 값의 양단 수용과 P4-B 보드·휴대폰 시험 완료를 뜻하지 않는다. **ESP32_P4A_ACCEPTANCE_PENDING / PRODUCT_TEST_NOT_RUN** 상태에서 다음 자료가 확인되기 전까지 위 OPEN을 유지한다.

- ESP32/Android 동일 구현 버전·참조와 Service UUID, 4개 Characteristic UUID 및 Read/Write/Notify·보안 Properties.
- ATT MTU 협상값·최소 지원값, 낮은 MTU에서 Status read/notify 처리.
- 분할 프레임 헤더·CRC·전송 ID·idx/count·timeout 및 분할·재조립·손실 처리.
- 실제 단말·보드에서 확인한 범위와 미실행 시험 구분. UART TX 완료는 BLE App 수신 완료가 아니다.

과거 BLE v0.3의 M_CONTEXT payload 31B·최근 결과 8건 전달 설명은 현재 채택 근거가 아니다. Network가 사용하는 M_CONTEXT의 payload 38B/전체 프레임 48B와 Device별 8건 보관·최신 5건 조회를 유지하며 실제 P4-A의 양단 호환을 확인한다. UUID·fragment 형식·ACK 필드·Query status를 추정하여 추가하지 않는다. Bonding/Registration/Session 정책과 Digital Key Unlock 성공 판정은 해당 기능 담당 범위다.
