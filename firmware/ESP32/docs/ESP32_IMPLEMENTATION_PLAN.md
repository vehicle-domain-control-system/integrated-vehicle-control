# ESP32 Implementation Plan & Verification Gates

- 2026-10-17 / Draft 0.2 (P5-A 설계 반영)
- 목표: ESP32 BLE v0.3 ↔ UART-M v2 실제 end-to-end Gateway. ESP-IDF v5.5.5, C/NimBLE/FreeRTOS.
- 책임: ESP32 **전 코드** 사용자 구현; S32K344 UART/CAN 스택 별도 담당; 도메인 기능 정책은 사용자 Domain App.

## 0. 프로젝트 준비

**먼저 수집**: `interface.zip` 압축 해제본(정확한 ESP32 Gateway API), `20_UART_M.md`, `40_BLE.md`, `network_sw_architecture.md`, `mobile(1).zip` BLE protocol 관련 `.kt`, `MOBILE_BLE_코드_한줄해설.pdf`, BLE v0.3 명세 원본(구할 수 있으면), `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md`, `SR(8).md`, `sysRS(7).md`, 실제 보드 핀맵 및 기동 성공 NimBLE 소스/로그.

`docs/references/`는 원문 보관·변경 금지. 개발 판단은 `docs/architecture/*.md`, 논쟁점은 `docs/DECISIONS.md`/`docs/OPEN_ISSUES.md`에 기록. **현재 없는 원본/GATT/하드웨어 값을 만들어 채우지 않는다.**

폴더: `main/`, `components/ble`, `components/uart`, `components/gateway`, `components/session`, `components/proximity`, `components/security`, `components/services`, `tests/`. 실제 코드/파일명은 기존 동작하는 프로젝트 구조와 중복 없이 결정한다. 루트 `CMakeLists.txt`, `main/CMakeLists.txt`는 ESP-IDF 공식 형식. `sdkconfig`는 소스에 무분별하게 하드코딩하지 않는다.

## 1. 단계·의존성·완료 기준

| 단계 | 범위 (이번 단계 외 작업 금지) | 산출물 예시 | Gate |
|---|---|---|---|
| P0 계약 및 빌드 | 기존 NimBLE 프로젝트 백업, MCU target, toolchain, 실제 핀 미정 목록, app protocol golden samples | `docs/DECISIONS.md`, `docs/OPEN_ISSUES.md`, base build | 기존 펌웨어 백업, `idf.py build` 완료, 미확정 명시 |
| P1 UART Frame | CRC, little-endian byte helpers, TX framer, streaming RX parser, bounded buffers | `uart_crc16.c`, `uart_frame.c`, `uart_parser.c`, host tests | CRC 123456789=0x29B1, split/concat/CRC fail/resync/length pass |
| P2 UART Codec | UART-M 23종 정확한 type/size, 3 uplink + 18 downlink + private Context/Alive, Wire enum mapping | `uart_codec.c`, vector tests | `20_UART_M.md` 모든 활성 message golden vector, invalid enum/size reject; WINDOW inactive |
| P3 ESP-IDF UART | UART Driver/event queue/RX/TX worker; UART 하드웨어 값은 확인 후 선택 | `uart_driver.c`, link fixture tests | ESP32↔PC loopback/USB-TTL TX/RX valid frame; error counters |
| P4 BLE GATT/Frame | 앱 UUID 5개, advertising, status 21B, state5B, v1 header8B, fragment/reassembly, subscribe | `ble_gatt.c`, `ble_frame.c`, `ble_adapter.c` | APK 연결, app state, status read+notify, MTU≥24, GATT write/notify mock |
| P5 Gateway API/Bridge | 정확한 public13 API, relay/core+ports, BLE request 12/20/9 ↔ UART 24/32/21, payload ownership | `gateway_interface.c`, `gateway_router.c`, tests | 모의 Domain에서 M_REQUEST→M_RESULT 정상, local OK vs send 확인 구별 |
| P6 Session/Registration | 승인 peer 정책, NVS, security, session ID, reconnect, app activity, link supervisor | `session_manager.c`, `registration_manager.c`, `app_activity.c`, `link_supervisor.c` | 미등록/구세션 거절, Domain reset시 Ready=0, old request not replay; 계약 승인 전 통제된 mock |
| P7 Query/Status full | 상향 Query, 전체 상태 하향 18종, warning/availability/settings/digital/query end, dedup, count | `query_tracker.c`, fixture tests | 앱 필수 현재 상태 12종 + 경고 8종 scope 완료/부분/누락 일치 검증 |
| P8 RSSI/Digital Key inputs | 현재 peer RSSI 중앙값, NEAR/FAR, VALIDITY/STALE, age, activity gating | `proximity_manager.c`, tests | 가상 표본/실측 로그, 가까이 초기 접속 시 자동 Unlock 0; **실제 Unlock은 wire 승인 전 비활성** |
| P9 Integration/Hardening | ESP32↔S32K344 UART-M, BCM 결과, reconnect, queue pressure, restart, security | scenarios, recorded logs | E2E request→Domain→BCM→Result, 재시작 안정성, 불명 결과 UNKNOWN 유지 |

**P2 수량 주의:** UART-M은 23종 = 상향 MOBILE 3종 + 하향 MOBILE 18종 + private `M_CONTEXT`/`M_DOMAIN_ALIVE` 2종이다. BLE 앱은 상향3 + 하향18 = 21종이다. WINDOW 2종은 정의만 유지하고 실제 기능은 비활성이다.

## 2. 단위 테스트와 테스트 데이터 규칙

### UART
- CRC known vector `123456789 -> 0x29B1`.
- `M_REQUEST` payload 24B→전체 frame 34B, `M_CONTEXT` 38→48B, `M_DOMAIN_ALIVE` 12→22B.
- 1바이트씩 분할 수신, 붙어 온 2~4 frame, sync bytes 포함 payload, junk prefix, header split, CRC fail, version fail, unsupported message Type, payload >128, exact subtype size, timeout 50ms(문서 후보).
- TX Sequence 65535→0(16-bit modulo), 공통 `LINK_SEQUENCE`는 원 Request/Session/Command 식별자와 절대 혼용 금지.
- parser fuzz/무작위 데이터로 out-of-bounds, signed overflow, buffer overflow, unbounded allocation 금지.

### BLE
- UUID 128-bit 일치, SERVICE UUID advertising에 포함.
- MTU 실제 협상값 사용; 앱 min MTU24, preferred185. 21B status raw가 한 번의 Notify로 완성되는지 시험.
- Frame8B, fragmentation header consistency, reserved transferId=0, index/count 범위, 1000ms reassembly expiry, duplicate/out-of-order policy, disconnect cleanup.
- App Command Type 0x10/0x11/0x12만 수신, unknown/reserved/restricted 값 명시 거절; WRITE WITH RESPONSE 상태 값 정책 협의.
- CCCD subscription 전 Notify 부적절 송신 금지, notify resource busy/queue backpressure 분리, 실패시 queue drain 정책.

### Gateway / Query / Session
- BLE M_REQUEST 12B + validated device/Session 12B = UART 24B (M_QUERY20→32, ACK9→21).
- SessionReady 0인 초기 유효 Query 허용 케이스와 물리 Request 금지 케이스 구분.
- Domain Alive 정상/750ms timeout(후보)/Boot 변경 → Gateway Status state reset, 이전 Query 무효화.
- Query End `ITEM_COUNT`, status 0/1/2/3, missing required 12 state/8 warning, duplicated response, boot/session changed, delayed END, TX acceptance vs actual completion 구분.
- 알 수 없는 차량 결과/ACK-only/TX success를 `DONE`으로 변환하지 않기.

### Digital Key 제안 (§10 측정)
- 가상 RSSI: median5, gap 400ms, NEAR >=-60 dBm 1초, FAR <=-70 dBm 2초, middle hysteresis, invalid/STALE, filtered/warmup.
- '가까이에서 초기 연결/ON/재기동 각10회' Unlock0, FAR->NEAR20회 1회/접근, manual LOCK 우선, NEAR 지속 자동 재실행0, 앱 종료 heartbeat TTL 3초 등 **명세 제안의 테스트 조건** 사용.
- 실제 자동 Unlock 허용은 등록 승인 정책, `far_run_ms` 전송 근거, BCM 연계 확인이 완료된 이후의 별도 Gate.

## 3. 단계별 Git 작업 규칙

- 각 PR은 **한 책임/한 테스트 레벨**에 집중. 예: `feat/esp32-uart-frame`, `feat/esp32-ble-gatt`, `feat/esp32-gateway-bridge`.
- 기존 BLE 연결 성공 baseline은 태그/브랜치로 보존.
- 변경 후 `idf.py build` 및 해당 단위 테스트(실행 불가능하면 원인과 미실행 명시).
- PR 템플릿: 목적/범위, 명세 근거 링크, 변경 파일, 테스트 명령·결과, 변경한 가정, OPEN 항목, 실제 보드 결과.
- 발견한 문서 충돌은 소스코드에서 몰래 수정하지 않고 Issue+합의 기록으로 추적.

## 4. 우선 협의해야 할 외부 계약

1. **Domain UART 네트워크 담당**: UART-M v2 VERSION=2, 38B Context, Type/CRC, UART-M API 구현 예상 연결시점, QueryEnd producer `ITEM_COUNT` 실제 TX 완료 기준.
2. **Android 앱 담당**: BLE v0.3 원본 승인, bonding=registration 여부, 현장 승인·등록 해제, GATT ATT error codes와 Android status 충돌, BLE Session 교체 규칙.
3. **Domain App 담당(사용자)**: 명령 id 및 결과 정책, Digital Key FAR 근거 요구, manual lock priority, no confirmed unlock until actual state feedback.
4. **보드 담당/교수**: ESP32 실 MCU·GPIO·UART 전압, Boot pins·전원, UART 연결용 level shifter 필요 여부.

**BLOCKER DK-01:** `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md`가 요구한 `far_run_ms`는 UART-M M_CONTEXT 38B에 직접 없다. ESP32 내부 판정 코드는 작성 가능하지만 실제 자동 Unlock 전송·허용은 양 끝 변경 또는 명세 재합의 후 Gate.

**BLOCKER SEC-01:** 앱 본딩 성공을 등록으로 보는 흐름과 Digital Key 명세의 별도 승인 등록 정책 충돌. 최종 사용자 인증 정책 합의 전 원격 자동 Unlock fail closed.

## 5. P5-B 구현 작업 분해

P5-B는 한 번에 Gateway 전체를 구현하지 않고 아래 순서로 작은 변경과 Host test gate를 둔다. P6 소유 상태는 mock port로 주입하며 실제 등록/Session/Digital Key를 구현하지 않는다.

| 순서 | 좁은 범위 | 산출물 / Gate |
|---|---|---|
| P5-B0 계약 Gate | 공개 header의 `update_sequence`, QueryEnd partial/count/dedup, BLE semantic reject 경계, Network의 P4 BLE 계약 채택 확인 | 해결된 결정 기록. 미해결 항목은 기능 flag/HOLD로 유지하고 wire/API 변경 금지 |
| P5-B1 pure mapping | BLE 3종 decode → UART typed record, UART private/downlink → BLE logical record의 순수 변환 | 양쪽 원문에서 독립 작성한 golden vector, exact length/enum/reserved/LE negative test |
| P5-B2 core/event queue | 단일 `GatewayTask` state, 고정 크기 event, mock clock/UART/BLE ports, ownership/backpressure | callback 원본 버퍼 변경 후에도 복사본 유지, FIFO, queue full, stop/restart, heap-free 경로 test |
| P5-B3 uplink route | 현재 P6 mock context prepend, Request/Query/WarningAck admission, duplicate key 및 no-replay | BLE12/20/9→UART24/32/21, invalid session/registration/security/READY, duplicate/reconnect/rotation test |
| P5-B4 downlink route | Alive supervisor, 16개 활성 downlink relay, QueryEnd 격리; WINDOW runtime 미활성 | context/boot/query mismatch, quality/AGE 보존, no synthetic Result, BLE backpressure test |
| P5-B5 query tracker | 단일 active query, 고유 응답 count, status/timeout/late End/partial 처리 | 0·1·5건, duplicate, unsolicited, missing, wrong boot/session/scope, late/duplicate End test |
| P5-B6 context publisher | 승인된 P6 producer snapshot을 38B로 compose | `update_sequence` binding 승인 후에만 production enable; repeat publish AGE/sequence test |
| P5-B7 ESP-IDF 연결 | P3 callback과 P4 event/send API에 adapter 연결, lifecycle 및 diagnostics | Host regression 후 증분 `idf.py build`; board/Android/Domain은 별도 P5-C 실기기 gate |

### P5-B Host test matrix

- **Golden mapping:** 모바일 Kotlin과 UART-M 표에서 각각 독립 계산한 3개 uplink 및 모든 활성 downlink vector. UART header/CRC/link sequence와 BLE 8B header를 semantic mapper 입력으로 섞지 않는다.
- **식별자:** RequestID/QueryID/SessionID/DeviceContextID/DomainBootID/LinkSequence/transferId가 서로 바뀌지 않음을 byte-level로 검증한다.
- **Result 안전:** local enqueue/TX success나 timeout으로 `ACCEPTED`, `DONE`, `FAILED`, `CONFIRMED`를 만들지 않고 원 confirmation/quality/AGE를 보존한다.
- **Query:** recent result 최대 5, status 0/1/2/3, `ITEM_COUNT` 0/1/5, duplicate response, `QueryID=0`, incomplete list, timeout, boot/session/disconnect 변경, late response/End를 검증한다. Android와 미합의인 status 2 완료 처리는 expected-HOLD로 분리한다.
- **소유권/동시성:** callback stack/temporary payload를 callback 직후 덮어써도 Gateway event가 유지되고, 느린 mock consumer·queue full·stop/restart에서 UAF/overwrite/blocking이 없음을 검증한다.
- **Failure recovery:** BLE reconnect와 UART/Domain reboot 뒤 pending physical command를 replay하지 않고 read-only Query만 허용하는 경로를 검증한다.
- **Context:** 세 producer의 sequence/AGE가 독립이고 반복 UART 송신으로 source sequence/AGE가 재설정되지 않음을 승인된 binding 이후 검증한다.

### P5-B 완료 및 보류 기준

- P1/P2/P3/P4 공개 API와 golden vector 회귀를 변경하지 않는다.
- Host test에서 mapper/core/query를 Driver 없이 실행하고, ESP-IDF 연결 변경 시 기존 Worktree 전용 cache로 증분 빌드한다.
- 실제 BLE↔UART end-to-end, ESP32 Flash, Android Notify receipt, S32K344 UART 수신은 사용자 요청에 따른 별도 `NOT_TESTED` 항목이다.
- P6 정보가 없는 빌드에서는 실제 vehicle Request route를 fail-closed하거나 명시적 isolated mock configuration에서만 연다.
- `update_sequence`, QueryEnd, ATT semantic reject 중 wire/API 변경이 필요한 결정은 별도 Issue/PR로 분리한다.

## 6. 전체 완료 정의 (Definition of Done)

- [ ] MOBILE 실제 APK와 GATT + 21B Gateway Status + 5B App State 정상
- [ ] BLE 21개 Type 방향별 고정 길이와 UART-M 23개 Type 방향/길이 완전 테스트
- [ ] UART-M Packet CRC + Frame parser + streaming, HW 연결 시험
- [ ] Gateway 공개 13개 API 선언 보존 및 기능/에러/메모리 수명 테스트
- [ ] 등록/본딩/암호화/Session/재시작·재연결·Ready 처리, 과거 Request replay 없음
- [ ] Query의 상태12종·경고8종·결과/END와 itemCount·partial 처리
- [ ] M_CONTEXT 근접·활성·등록 원 quality/AGE/sequence 유지
- [ ] UNKNOWN/STALE/미인증 입력에서 자동 제어 불허
- [ ] 실제 UART Domain 통합에서 물리 명령 단 한번, 결과 확인 증거 유지
- [ ] 문서 불일치 해결 기록과 실제 장비 테스트 로그
