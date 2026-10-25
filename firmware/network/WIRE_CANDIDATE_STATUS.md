# Wire Wave1 기준 후보 경계

지금의 CAN20 확장과 읽기 전용 검증 도구 수정은 `WIRE_WAVE2A_STATUS.md`를 본다. 이 문서는 보존한 Wave1 범위를 기록한다.

`network_wire_candidate`는 따로 있는 SDK 없는 C11 라이브러리다. `network_common`, `network_host_mock`, C2, 어댑터, MCU Port에 의존하지 않는다. CMake 기본값은 OFF이고, 호출하려면 CANDIDATE_ONLY 프로필을 명시해야 한다. 제품 채택, wire/ABI 동결, runtime 사용, scheduling, 송신, 물리 시험은 HOLD 그대로다. S32K144/S32K344는 NOT_CONFIGURED 그대로다.

| Layer/module | 추가한 구현 | 시험한 후보 동작 |
|---|---|---|
| L04/M05 | `net_wire_can.c`, `net_wire_can.h` | CIS_REAR 0x0A0과 DOMAIN_PERMISSION 0x080, used19/frame20/DLC11 |
| L04/M06 | `net_wire_uart.c`, `net_wire_uart_dto.c`, `net_wire_stream.c`; 공개 UART 헤더 | UART-C v1 P_VISION_RESULT/P_PERMISSION; UART-M v2 M_CONTEXT/M_REQUEST/M_RESULT; CRC와 링크별로 상한이 있는 stream |
| L04 M05/M06 공용 | `net_wire_common.c`, `net_wire_internal.h`, `net_wire_registry.c`, `net_wire.h` | 하나뿐인 LE 구현, 범위/프로필 helper, CAN22/M23/C3 registry |

이 공용 유틸리티는 추가 Layer나 Module이 아니다. 기존 Layer 5개/Module 9개와 원래 C/H 파일 28개는 모두 그대로다. M02는 설계만 있고 M03 제품 handoff는 연결되지 않은 채다. registry에만 있는 type의 일반 UART envelope 검사는 구조 검사이며, 구현된 형식 DTO를 만들지 않는다.

CAN은 표준 11-bit ID, extended/RTR/driver error 없음, FDF/BRS 참, DLC11, capture 바이트 20개, 마지막 padding 0을 명시적으로 검사한다. Raw123은 123 mm다. NO_OBJECT/65535/VALID/NONE은 표현할 수 있고, VALID_DISTANCE/65535는 잘못된 값이다. 지원하지 않는 필드 간 의미 조합은 CONTRACT_HOLD를 돌려준다. DOMAIN_PERMISSION은 허가를 계산하지 않고 선언된 enum 바이트와 원 AGE를 그대로 둔다. 품질 투영과 하류 철회는 HOLD 그대로다.

UART는 A5 5A, type별 정확한 길이, little-endian 정수와 CRC 바이트, B2..B(P+7)에 대한 CRC16-CCITT-FALSE(poly1021/initFFFF/no reflection/xorout0)를 쓴다. P<=128과 frame<=138은 절대 상한이고, 알려진 type은 그래도 자기의 정확한 P가 필요하다. 예전 M_CONTEXT P31/frame41은 거부한다. DTO padding은 직렬화하지 않는다. 지금 downlink SESSION과 원 REQUEST_SESSION은 서로 독립된 필드다. M_REQUEST는 인자 분류 8개 전부(문 열기/닫기를 포함한 kind/op 쌍 9개), 예약 0 바이트, boolean/Fan/RGB, signed i16 centi-C를 검사한다. 온도를 범위에 맞춰 자르거나 요청을 실행하지 않는다. P_PERMISSION USE_LIMIT500과 1500, source AGE, 모든 순번은 raw 값 그대로다. TTL/ALLOW 판단을 뜻하지 않는다. 지원하지 않는 의미 모순은 고치지 않고 HOLD로 둔다.

모든 공개 encode/decode는 실패하면 출력을 바꾸지 않고, 겹치는 입력/출력 범위를 거부한다. UART 형식 decode의 두 출력은 서로 겹치지 않아야 한다. 호출자는 유효하고 크기가 충분한 C 객체와 안정된 빌린 배열을 준다. heap, 벤더 타입, wire cast는 쓰지 않는다. API는 호출자가 ISR 밖에서 직렬로 부른다. 인터럽트/스레드 동기화를 주장하지 않는다.

stream 인스턴스는 M/C 링크와 방향마다 따로 둔다. 호출 한 번에 <=512 바이트와 바이트마다의 원 단조 uint64 ms 시각을 받는다. `OK`는 완성된 구조 프레임 하나와 소비한 수를 돌려준다. `NEED_MORE`는 소비한 입력과 진단을 반영하고 출력 프레임은 바꾸지 않는다. 다른 실패는 상태, 소비 수, 프레임을 모두 바꾸지 않는다(트랜잭션). 호출자는 소비되지 않은 뒷부분을 다시 넘겨야 한다. sync 복구는 거부한 sync에서 한 바이트 나아가고, 뒷부분의 시각은 그대로 둔다. 프레임 경계가 확정된 payload 안의 SYNC는 opaque로 둔다(sync로 보지 않는다).

시간 정책은 명시적으로 골라야 한다. 50ms는 PROPOSED이고, 같을 때의 동작은 호출자가 고른다. `timer_check_permitted`는 앞서 큐에 들어온 RX를 다 비웠다는 뜻이다. 현재 시각으로는 완성되지 않은 데이터만 만료시킬 수 있다. 완성 프레임 조립은 원래 마지막 바이트 시각을 쓰므로 worker 지연으로 age가 늘지 않는다. 받아들인 타이머 watermark는 넘겨받은 묶음을 다 소비한 뒤에만 기록한다. 뒤로 가거나 모호한 capture·타이머 시각은 TIME_HOLD를 돌려준다. 관측된 링크 끊김은 부분 데이터를 버리지만 단조 watermark는 유지한다. 명시적으로 다시 초기화하려면 호출자가 새 시계 epoch를 검증해야 한다. uint64 wrap과 시간 영역 사이의 변환은 추측하지 않는다. parser 카운터는 포화하며 차량 Health나 Fault 이벤트가 아니다.

독립 literal golden 7개, 엄격한 GCC 시험, signed-i16 전수 변환, CAN 무작위 왕복 4,000회, 후보 stream 재생 프레임 3,000개를 결과 ZIP에 기록했다. 원래 합성 fixture stress는 따로다. 로컬에는 Clang이 없고, ASan/UBSan 링크 라이브러리가 없으며, GCC6.3은 -fanalyzer를 거부한다. 이 검사들은 NOT_RUN이고, 과거 검토 결과에서 추정하지 않는다. HAL/flash/물리 시험은 NOT_RUN이다.

G01..G08과 Round4 P1/P2는 OPEN/HOLD 그대로다. 소스, offset, 소스 SHA, 정확한 시험 명령, 남은 모호함, all48 진행 상황은 결과 ZIP에 있다.
