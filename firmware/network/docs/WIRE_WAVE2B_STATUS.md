# UART Wave2B Host raw 후보

Wave2B는 분리된 후보 라이브러리에 UART-M20과 UART-C P_VISION_STATUS를 더한다. 기존 CAN22, UART-M3, UART-C2는 그대로다. 지금의 형식 기능 조회는 CAN22/UART-M23/UART-C3을 다루고, 과거 `typed_wave1`은 여전히 정확히 일곱 레코드만 표시한다. registry ID, 버전, 경로, 길이는 바뀌지 않았다. 이것은 Host 기능 메타데이터이고, 제품 채택은 HOLD 그대로다.

새 production 파일은 모두 L04 Frame Codec / M06 UART Codec에 속한다. Layer 5개와 Module 9개는 바뀌지 않았다. `network_wire_candidate`는 기본 OFF이고 runtime, Host Mock, C2, MCU, SDK, RTOS, heap에 의존하지 않는다. 모든 호출에 `NET_WIRE_CANDIDATE_ONLY`를 명시해야 한다. MCU Port는 NOT_CONFIGURED 그대로다.

| 공개 헤더 / 소스 | raw 후보 묶음 |
|---|---|
| net_wire_uart_m_core.h / net_wire_uart_m_core.c | M_QUERY, M_WARNING_ACK, M_DOMAIN_ALIVE, M_WARNING, M_AVAILABILITY, M_DIGITAL_STATUS, M_DIGITAL_RESULT, M_USER_SETTINGS, M_QUERY_END |
| net_wire_uart_m_projection.h / net_wire_uart_m_projection.c | M_BCM_DOOR_STATE/CLIMATE_STATE/LIGHT_STATE/STATUS; M_CIS_ENVIRONMENT/OCCUPANT/REAR/STATUS; M_VSS_STATUS; M_WINDOW_STATE/FAULT |
| net_wire_uart_c_status.h / net_wire_uart_c_status.c | P_VISION_STATUS |
| net_wire_uart_typed_internal.h/.c (private M06) | 새 DTO가 함께 쓰는 인자·payload 검사 |

공개 묶음 헤더는 `net_wire_uart_dto.h`다. 새 `NetWire...Raw` 레코드마다 원래 프레임 encoder/decoder와 CRC를 쓰는 명시적인 encode/decode 쌍이 있다. decode는 link sequence를 따로 돌려준다. 정수는 모두 명시적인 LE 연산을 쓰고, signed i16은 기존 이식형 helper를 쓴다. 구조체 padding은 wire 데이터가 아니다. 실패하면 출력 프레임, DTO, link sequence 전체가 그대로다. NULL, 그리고 입력·출력 사이와 두 decode 출력 사이의 완전/부분 겹침은 거부한다. 호출자는 살아 있고 안정된 버퍼를 주고, ISR 밖에서 직렬로 부른다.

지금의 Device/Session/DomainBoot/Query, 원 CAN Boot/TX, Pi 관측, source 평가 순번과 AGE는 서로 구별된 채다. 시계 읽기, 식별 할당, 출처 결합, source AGE 재시작, runtime 접수, 차량 동작은 구현하지 않는다. UART-M version2와 UART-C version1은 따로다. 방향은 API/registry 제약이지, 지어낸 payload 주소 바이트가 아니다.

raw 경계와 풀리지 않은 의미:

- M_QUERY는 0이 아닌 QueryID, reserved0, cursor0이 필요하다. Scope0의 요청 참조는 문서대로 0이어야 한다. scope1의 섞인 요청 식별과 scope2/3의 0이 아닌 요청 필터는 보수적으로 CONTRACT_HOLD다. Session 발급과 consumer 접수(admission)는 OPEN 그대로다.
- M_WARNING_ACK가 참조한 DomainBoot0과 M_WARNING source Boot0은 합의된 부재 표현이 없어 HOLD다. ACK는 경고를 CLEAR하거나 소리를 STOP하지 않는다. 알려진 품질과 모순된 reason이 함께 오면 그것도 HOLD이고, 필드를 고치지 않는다. 진리표가 없는 현재/이력과 의미 관계는 raw 코덱 밖에 둔다.
- M_AVAILABILITY는 명시적인 기능 ID 순서 1..9로 항목 9개가 모두 있어야 한다. C2 enum 인덱스 0..8을 그대로 cast해 쓰거나 reason/가용성을 지어내지 않는다.
- M_DIGITAL_RESULT는 ORIGIN2를 유지하고 Result255와 Result0을 구별한다. 확인은 있는데 결과가 없으면 잘못된 형식이다. DONE은 실제 Door Unlock 전환을 증명하지 않는다.
- M_USER_SETTINGS는 signed 온도와 -32768, OFF/0과 UNKNOWN255, RGB 바이트 순서, raw 품질을 그대로 둔다. 고정된 Fan 코드는 0..3/255다. kit 시험 예시의 fan0..100 표현은 새 코드 범위가 아니다. 밝기는 0..100이다. 알려진 품질과 unknown sentinel이 함께 오면 HOLD다. NORMAL 밝기/RGB가 없는 경우의 표현은 없다. 호출자가 값을 만들어 내지 말고 복합 내보내기 정책을 정해야 한다. 기능상 온도 설정 범위는 OPEN 그대로다.
- M_QUERY_END는 넘겨받은 scope/status/count만 직렬화한다. 최종 TX 수를 계산하거나, 빈 성공을 증명하거나, Query 상태를 닫거나, BLE/App 수신을 확인하지 않는다. raw status/count 조합을 포함해 G06은 HOLD 그대로다.
- 투영 11개는 DOWN_CONTEXT20 뒤의, CAN이 쓰는 바이트를 정확히 그대로 둔다. 코덱은 CAN/C2에서 투영을 만들지 않는다. Environment에는 독립 lane 3개가 있고 sentinel, 품질, age는 따로이며 TTL은 계산하지 않는다. Rear raw123은 정수 cm로 자르지 않은 123mm다.
- M_VSS_STATUS는 u8 현재 FaultMask를 마지막 고장 이력과 따로 둔다. same-K 상태/고장 commit은 M06 밖이다. M_WINDOW_STATE/FAULT는 raw Host 후보로 남고, 실행/TX/표시/감시는 HOLD다.
- P_VISION_STATUS는 Pi Boot/상태 순번, 상태, 현재/마지막 고장, 상태 age, 영향 비트 0..1을 그대로 둔다. 마지막 고장 이력을 남긴 READY도 표현할 수 있다. 앞의 P_VISION_RESULT나 Permission은 갱신하지 않는다.

P0 기준은 source67 SHA, ON101/OFF41이다. 원래 CTest 이름은 모두 남는다. `wire.can22_typed_coverage`만 낡은 UART5/P_STATUS 미구현 기대를 UART26/P_STATUS 구현으로 바꾸고, CAN22, 과거 UART5/일곱, 모르는 ID 검사는 그대로 둔다. 새 full48 기능 시험은 따로다. 다른 기준 시험 파일과 원래 codec/CRC/stream/LE 구현은 바이트까지 같다. 정확한 실행 결과, 새 type별 양성 Golden21, 구조 전용 기준 references42, 독립 검토, SHA, 한계는 `_local_review/wire_wave2b_20261017_v1.0/`과 전달 ZIP에 있다. 구조용 0 payload와 패턴 payload는 의미상 유효한 형식 Golden으로 세지 않는다.

```sh
cmake -S firmware/network -B firmware/network/_local_review/wave2b-build -DNETWORK_TARGET=host -DNETWORK_BUILD_WIRE_CANDIDATE=ON
cmake --build firmware/network/_local_review/wave2b-build --parallel
ctest --test-dir firmware/network/_local_review/wave2b-build --output-on-failure
```

Round4 지적 ID 11개/위치 32개, G01..G08, 제품 ABI, producer/consumer 합의, Permission 전파, Query finalTX, VSS, WINDOW, ESP32 소유는 OPEN/HOLD 그대로다. 하드웨어/Flash/전기/물리 링크 시험은 NOT_RUN이다.
