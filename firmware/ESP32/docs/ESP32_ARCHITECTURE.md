# ESP32 Firmware Software Architecture

- 문서 버전: Draft 0.2 (2026-10-17, P5-A Gateway 설계 추가)
- 대상: ESP32 자체 제작 보드, ESP-IDF v5.5.5, C, NimBLE, FreeRTOS
- 범위: **ESP32 펌웨어 전체**. Domain S32K344 Application은 사용자 별도 개발, S32K344 UART/CAN 통신은 네트워크 담당 개발
- 문서 성격: 구현 지침. `WIRE_PROPOSED`, `BINDING_OPEN`, `TEAM_DECISION_REQUIRED`는 자동 승인하지 않는다.

## 1. 참조 및 우선순위

1. **실제 모바일 앱 BLE 구현**: `mobile(1).zip` 내 `GattSpec.kt`, `BleFrameCodec.kt`, `MessageCodec.kt`, `GattPayloads.kt`, `SessionOrchestrator.kt`, `QueryCoordinator.kt` 등; 설명은 `MOBILE_BLE_코드_한줄해설.pdf` (BLE v0.3 기준).
2. **Gateway 로컬 공개 API**: `interface/02_NODES/ESP32/20_API.md`, `11_GATEWAY_TYPES.md`, `12_GATEWAY_INTERFACE_TYPES.md` (DECLARED_SOURCE; 실제 .h/.c 구현체 확인 필요).
3. **UART-M v2 제안 계약**: `interface/03_COMMUNICATION/20_UART_M.md` 및 `interface/02_NODES/ESP32/25_PRIVATE_ROUTING.md`.
4. **요구 및 책임**: `SR(8).md`, `sysRS(7).md`, `network_sw_architecture(3).md`, `40_BLE.md`.
5. **Digital Key 구현 제안**: `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md`—보안/근접 보조 설계이며 wire 정본이 아님.

상충 시 **코드 또는 사양을 묵시적으로 덮어쓰지 않는다**. `docs/DECISIONS.md`(권장 새 문서)에 대조표·선택 버전·승인자를 기록하고 영향을 받는 기능을 Mock 또는 SAFE_RESTRICTED로 둔다.

## 2. 시스템 경계

```text
Android Kotlin MOBILE
    <BLE GATT v1, 자체 8B fragment header>
ESP32 [NimBLE Adapter | BLE Codec | Registration/Session/Activity/Proximity]
       [Gateway Interface 13 public APIs | Core | Router/Query/Link]
       [UART-M Codec | Frame/CRC | streaming parser | ESP-IDF UART Driver]
    <UART-M v2 115200 bps (proposed wire details)>
S32K344 [Domain Network (타 담당자) | Domain App (사용자)]
    <CAN FD>
BCM/CIS/VSS (WINDOW 구현 보류)
```

- ESP32는 **차량 제어 정책 및 도어 구동의 최종 결정 주체가 아님**. BLE 입력 검증, 등록된 peer·현재 Session 확인, UART 메시지 중계, RSSI 품질 평가를 담당한다.
- UART-M과 BLE 프레임은 전혀 다르다. BLE Frame raw를 UART Driver로 그대로 보내거나 UART CRC를 BLE에 재사용하지 않는다.
- `M_DOMAIN_ALIVE`는 ESP32 내부 링크 감독 정보이며 앱 Vehicle Data로 중계하지 않는다. `M_QUERY_END`는 내부 조회 검증을 거쳐 모바일 앱의 0x40 조회 종료 메시지로 전달해야 한다(양단 완료 계약 확인 필요).

## 3. 모듈 및 단일 책임

| 컴포넌트 / 파일 제안 | 소유 상태/주요 작업 | 입력 → 출력 |
|---|---|---|
| `ble/ble_gatt.c` | GATT 4개 characteristic, advertising, connection security, CCCD | NimBLE GAP/GATT → BLE 이벤트 |
| `ble/ble_frame.c` | BLE v1 8B header, Fragment 생성/조립/검사/timeout | GATT Write/Notify ↔ 논리 `(type,payload)` |
| `ble/ble_app_codec.c` | App Command 3종 고정 길이·인수 검증 | BLE 논리 payload → Gateway 의미 레코드 |
| `uart/uart_driver.c` | 실제 GPIO/115200/8N1(제안), DMA/driver queue/에러 | UART bytes ↔ RX/TX queue |
| `uart/uart_frame.c` | UART-M v2 frame + CRC16-CCITT-FALSE + TX LINK_SEQUENCE | UART bytes ↔ 완성 프레임 |
| `uart/uart_parser.c` | sync 탐색/부분·연속 frame/length·CRC 오류 복구 | RX stream → 유효 frame |
| `uart/uart_codec.c` | 23종 Type/방향/정확한 length/LE codec/enum mapping | frame payload ↔ codec record |
| `gateway/gateway_interface.c` | 기존 Gateway 13개 API 제공·호출 연결, 복사·수명 보증 | BLE/Network Adapter ↔ Core/Ports |
| `gateway/gateway_router.c` | validated relay·원 RequestID/QueryID 보존; 채널 간 전달 | BLE logical ↔ UART typed messages |
| `session/session_manager.c` | Peer-DeviceContext-Session 일치, 재부팅 무효화 | BLE 연결·등록 이벤트 → 유효 문맥 |
| `security/registration_manager.c` | 승인된 단말, 암호화/본딩, NVS, 현장 승인·해제 | 실 peer identity → registration state |
| `activity/app_activity.c` | App State 5B, instance, 수신시각, freshness | BLE app heartbeat → 활동 품질 |
| `proximity/proximity_manager.c` | 승인된 연결의 RSSI 중앙값/NEAR·FAR/품질 | NimBLE RSSI + clock → Gateway proximity update |
| `gateway/context_publisher.c` | 등록·활성·근접 독립 근거를 `M_CONTEXT`로 합성 | Gateway Publish3 → UART-M Context |
| `gateway/link_supervisor.c` | M_DOMAIN_ALIVE timeout/Domain boot/SessionReady/부트 변경 | UART RX → Gateway Status 21B |
| `gateway/query_tracker.c` | QueryID/scope/session/boot·ITEM_COUNT·부분 처리 | M_QUERY, 응답, END → 앱 완료/부분 상태 |
| `storage/nvs_store.c` | 등록 데이터/설정의 유효한 저장·삭제 | security owner → NVS |
| `diag/diagnostics.c` | 프레임 reject, queue overflow, link reconnect, timeouts | 계층별 counters/logs |

기존 소스에서 동일 책임의 모듈이 있으면 새 이름으로 중복 작성하지 않는다. `Gateway_Interface_*` 함수명·반환형·인자 목록은 `interface/02_NODES/ESP32/20_API.md`를 그대로 보존한다.

## 4. BLE 확정 가능한 앱측 계약

- Service UUID: `A0F00000-7C21-4B5A-9E32-5F4D4F42494C`; characteristic 접두부 `A0F00001` Gateway Status(READ+NOTIFY), `A0F00002` App Command(WRITE WITH RESPONSE), `A0F00003` Vehicle Data(NOTIFY), `A0F00004` App State(WRITE WITH RESPONSE); 공통 suffix는 서비스와 동일.
- BLE protocol version 1, `TRANSFER_ID` u16 LE (0 예약), `TOTAL_PAYLOAD_LENGTH` u16 LE, fragment index/count u8; 총 헤더 8B. payload 조각 최대 = **실제 협상 MTU - 11**.
- 앱은 preferred MTU=185, min supported MTU=24. 실제 MTU=23이면 Gateway Status 21B Notify 전달 불가능하므로 앱은 연결 절차를 중단한다.
- BLE `M_REQUEST`=12B, `M_QUERY`=20B, `M_WARNING_ACK`=9B; 하향 상태 메시지는 UART semantic payload와 길이 동일(0x20..0x40), 별도 8B BLE header로 송신한다. `M_CONTEXT`, `M_DOMAIN_ALIVE`는 앱 GATT logical message 목록에 없음.
- Gateway Status **21B raw**(BLE header 없음): version/registration/link/ready/deviceContextId/sessionId/domainBootId/domainState. App State **5B raw**: active u8+appInstanceId u32 LE. 두 특성에 BLE fragment header 붙이지 않는다.
- 앱은 foreground active state를 대략 1000ms heartbeat로 보낸다. 연결됐다는 사실만으로 `ACTIVE`로 합성하지 않는다.
- 앱의 BLE v0.3 등록 흐름 예시에는 본딩 성공=등록로 표현된 부분이 있으나, Digital Key 제안은 `본딩 ≠ 차량 소유 승인`으로 더 엄격한 규칙을 요구한다. **실제 등록 승인 정책이 충돌하므로 보안 기능의 운영 활성화 전에 팀 결정 필요**.

## 5. UART-M v2 제안 계약

- 헤더: `A5 5A`(2), version `02`(1), `MESSAGE_TYPE`(1), `PAYLOAD_LENGTH` LE(2), 방향별 `LINK_SEQUENCE` LE(2), payload 0..128, CRC16 LE(2). Frame total = payload + 10.
- CRC16/CCITT-FALSE: poly 0x1021, init 0xFFFF, xorout 0, reflected false, CRC range version(byte2)~payload end. Test `123456789`→`0x29B1`.
- Parser는 stream 기반. 분할/연속 수신, payload에 `A5 5A` 포함, 잘린 frame, CRC mismatch, overflow, frame assemble timeout 50ms(제안)를 테스트한다.
- Version 2의 `M_CONTEXT`=38B payload(48B frame), `M_REQUEST`=24B(34B frame), `M_QUERY`=32B(42B frame), `M_WARNING_ACK`=21B(31B frame). `M_DOMAIN_ALIVE`=12B(22B frame).
- 상태 하향: `DOWN_CONTEXT` 20B 포함. APP BLE 상향 semantic 12/20/9B 앞에 ESP32가 검증된 `(DeviceContextId 4B, SessionId 8B)`를 붙여 UART 24/32/21B로 구성. 요청 ID는 MOBILE 원본을 보존한다.
- `M_CONTEXT`의 등록/연결, App Activity, Proximity는 **별도 원 품질·시각·sequence**를 갖고 Compose할 뿐 UART 반복 송신으로 원 관측 번호가 증가하지 않는다.
- `TYPE + P`는 `Gateway_RelayMessageView_t.payload` 내부에서 메시지 subtype을 표현하는 **제안**이다. UART Frame `MESSAGE_TYPE`과 동일한 층이 아니다. 승인 전 별도 `gateway_relay_codec`에서만 구현하고 public typedef는 바꾸지 않는다.

## 6. Gateway public API 사용

문서의 **13개 함수**: `Gateway_Interface_Init`, `Reset`, `IsInitialized`, `IsValidByteView`, `IsValidMessageContext`, `IsValidRelayMessage`, `OnMobileMessage`, `OnDomainMessage`, `SendToDomain`, `SendToMobile`, `PublishRegistrationConnection`, `PublishAppActivity`, `PublishProximity`.

```text
NimBLE WRITE → BLE reassembly → verified current peer & semantic payload
 → Gateway_Interface_OnMobileMessage(view) → core/adapter
 → Gateway_Interface_SendToDomain(view) → UART codec/frame → UART Tx queue

UART RX stream → UART parser/typed codec → context & freshness checks
 → Gateway_Interface_OnDomainMessage(view) → gateway/adapter
 → Gateway_Interface_SendToMobile(view) → BLE fragment/notify queue
```

이 흐름은 **목표 연결도**이며 API 선언만으로 현재 펌웨어 구현이 존재하는 것은 아니다. 호출 후 local OK는 송신 성공·Domain ACCEPTED·차량 DONE을 의미하지 않는다. Borrowed ByteView는 콜백 종료 이후 보관하면 안 되며 async 작업 전 deep copy한다.

## 7. Security / Session / Recovery

- 승인된 단말 identity, 실제 encrypted 연결, 수용된 Session, 현재 App Activity, RSSI 표본을 **독립적으로 검증**. Bonding의 `encrypted`와 `authenticated`는 다르다.
- SessionId는 0이 아닌 중복되지 않는 식별자로 생성(보안 난수 권장), peer/device/connection/앱 문맥에 bind; 세부 lifecycle/registration 팀 합의 필요. `TRANSFER_ID`(BLE fragment), `LINK_SEQUENCE`(UART frame), `REQUEST_ID`(앱 요청)는 SessionId와 별개다.
- BLE disconnect, 새 peer, ESP32 reboot, Domain boot 변경, security loss: 연관 Query/미완성 BLE fragment/대기 중 미송신 명령 무효화; 이전 명령 자동 replay 금지. `SESSION_READY=1`은 앱의 실제 Query 목록 완료와 동일하지 않다.
- 링크 상실 시 `Gateway Status.domainLinkUp=0`, `sessionReady=0`, `domainBootId`/`domainState` 미확인 처리(필드 표현은 앱 계약에 맞춤), 새 제어 Request 차단. Mock 연결을 실제 Domain_READY로 광고 금지.
- 메시지 버퍼 정책: Event/Result/Warning/QueryEnd와 반복 상태 Snapshot을 분리. 상태는 최신값 교체 가능하나 결과·경고·원 요청 이벤트는 임의 덮어쓰기 금지. 응답 불명 시 물리 명령 중복 재실행 대신 Query로 확인.

## 8. Digital Key — ESP32 책임과 안전 경계

참조: `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md`, **구현 시작용 제안**.

- ESP32는 등록·암호화된 현재 peer RSSI만 읽는다. 제안값: 200ms 표본, 최근 5개 중앙값, gap 400ms 이상시 reset, NEAR ≥ -60 dBm 1000ms, FAR ≤ -70 dBm 2000ms, stale 1000ms, report 200ms.
- NEAR/FAR 미충족·실패·경과/품질 불명은 `UNKNOWN/INVALID`, **FAR로 변환 금지**. 연속 FAR 근거와 최초 연결 직후 NEAR는 별개다.
- 앱 Heartbeat 1000ms, active TTL 3000ms(제안). 등록 해제·보안 상실·새 연결·앱 문맥 변화는 근접 근거 초기화. 자동 Unlock의 실제 정책(FAR→NEAR 1회, 수동 LOCK 우선)과 BCM 명령 판단은 **Domain** 책임이다.
- **차단 조건 DK-OPEN-01:** Digital Key 제안의 `far_run_ms`, RSSI/sample/report sequence 및 age/generation 정보가 현재 UART-M v2 `M_CONTEXT` 38B 필드에 모두 존재하지 않는다. UART-M의 필드를 임의 확장하거나 예약 bytes에 넣지 않는다. 양단 wire 조정/기존 타입 활용 승인 전에는 `far_run_ms` 의존 자동 Unlock 활성화 금지. BLE RSSI 계측과 UI 표시용 모의 테스트는 가능.
- **차단 조건 DK-OPEN-02:** 앱 BLE 연결 문서의 '본딩=등록'과 본 명세의 현장 승인 등록 정책을 확정해야 한다. 실제 사용자/차량 인증이 구현되기 전 데모라도 자동 Unlock fail-closed.
- 실제 RSSI 거리를 보장하지 않는다. 암호화/bonding만으로 안전한 Passive Entry/거리증명/relay-attack 방어가 되지 않는다.

## 9. Task ownership / concurrency (설계 권고)

| Task / execution context | 대상 | 규칙 |
|---|---|---|
| NimBLE host callbacks | GAP/GATT IRQ-safe 경계 | 수신 검증·bounded copy·nonblocking enqueue만 수행 |
| `GatewayTask` | Gateway, session, query, app activity, context publish | 단일 논리 상태 소유, 직렬 event 처리 |
| `UartRxTask` | Driver RX bytes + parser | frame 유효성 검사 후 Gateway queue 인계 |
| `UartTxTask` | UART TX frame queue | TX completion/error 구분, 큐 지연 측정 |
| `SupervisorTask` | UART alive, expiry, RSSI sampling, timeouts | Timer 기반; 긴 delay로 NEAR/FAR hold 구현 금지 |

Task 및 Queue 개수, 버퍼 크기, priority/pinning은 **실제 RAM·NimBLE·115200bps 부하 측정 전 권고**이다. `vTaskDelay` 반복으로 source timestamp를 새 평가로 재라벨하지 않는다.

## 10. P5-A Gateway bridge 설계

### 10.1 검토 기준선

이 절의 설계 비교에는 다음 snapshot을 사용했다. commit이 바뀌면 P5-B 시작 전에 다시 대조한다.

| 대상 | Branch / commit | 용도 |
|---|---|---|
| Network interface | 팀 `docs/network` / `00a43414807e7a5b9c087d589a1183d1989e5b91` | UART-M, Gateway type/API/private routing |
| Android | 팀 `function-mobile` / `8054b69d3ab2a91d80819a58e3443653781c8ed3` | 실제 Kotlin BLE codec/session/query 동작 |
| Domain | 개인 Domain `main` / `fc28c2b9c80a25b9936b5efa95d87c550951934b` | 공개 DomainIf, Result/Query 동작 |
| ESP32 | 개인 `origin/main` / `690037197885907bd0d5c0d019c0f79df0600cb5` | P1~P4 실제 API 및 구현 |

Network의 UART-M/Gateway binding에는 아직 `WIRE_PROPOSED`, `BINDING_OPEN`, `HOLD_BINDING`이 남아 있다. 이 절은 P5-B 구현 후보 설계이며 그 표식을 승인 상태로 바꾸지 않는다.

### 10.2 데이터 흐름과 책임 경계

```text
Android App Command Write
  -> NimBLE callback: 문법/길이 검사, bounded copy, enqueue
  -> BLE application event task
  -> Gateway event queue: 연결 generation과 수신 시각을 포함해 다시 소유권 이전
  -> GatewayTask: 등록/암호화/Session/중복/Query 상태 검사
  -> BLE semantic payload 12/20/9B decode
  -> 현재 검증 문맥 DeviceContextId 4B + SessionId 8B prepend
  -> UART-M typed message 24/32/21B encode/enqueue
  -> UART transport local queue/TX (Domain 수신 또는 차량 완료를 뜻하지 않음)

UART RX
  -> frame/parser/CRC 및 P2 typed codec 검증
  -> transport dispatch callback: bounded copy 후 Gateway event enqueue
  -> GatewayTask: Domain boot/current context/query/freshness 검증
  -> private Alive/QueryEnd 처리 또는 BLE semantic payload 선택
  -> BLE fragment/Notify API
  -> local notify submission (Android 수신/처리를 뜻하지 않음)
```

- `GatewayTask` 하나가 현재 연결 generation, current Device/Session/Domain boot, pending request key, active query와 link 상태를 소유한다. NimBLE 및 UART callback은 이 상태를 직접 변경하지 않는다.
- P5는 검증된 식별 문맥을 **사용**하지만 등록 승인, Session 생성/회전, App Activity 및 Proximity 판정은 P6 소유다. Bonding만으로 `registered` 또는 `sessionReady`를 만들지 않는다.
- Domain이 Command 허용과 최종 Result를 소유한다. P5는 local queue admission, UART local completion, Domain Result와 차량 `DONE/CONFIRMED`를 구분하고 결과를 합성하지 않는다.
- `M_DOMAIN_ALIVE`는 link supervisor 입력이며 BLE Vehicle Data로 내보내지 않는다. `M_QUERY_END`는 Query tracker의 의미 검증을 거친 뒤에만 앱으로 전달한다.
- Digital Key 자동 Unlock은 이 단계의 route에 포함하지 않는다.

### 10.3 내부 API와 메모리 소유권 — PROPOSED

공개된 13개 `Gateway_Interface_*` API는 그대로 유지하고, 아래 항목은 그 뒤의 private core/port 경계 후보이다. 실제 이름과 크기는 P5-B PR에서 별도 검토한다.

| Private 경계 | 입력 수명 / 처리 |
|---|---|
| `GatewayCore_PostBleEvent` | P4 callback에서 받은 borrowed payload를 함수 반환 전에 고정 크기 event storage로 복사한다. connection handle 자체 대신 재연결마다 증가하는 connection generation을 함께 기록한다. |
| `GatewayCore_PostUartMessage` | P3 callback의 borrowed `uart_m_message_t`를 반환 전에 복사한다. UART link sequence는 전달 상태 관측용이며 Request/Query 식별자로 사용하지 않는다. |
| `GatewayCore_PostLinkEvent` | BLE security/subscription/disconnect 및 UART link/Domain boot 변경을 직렬화한다. disconnect/boot/session rotation은 관련 pending state를 무효화한다. |
| `GatewayCore_ProcessOne` | `GatewayTask`에서만 한 event를 처리한다. callback 또는 Driver task에서 blocking route를 실행하지 않는다. |
| UART send port | typed message를 P3 TX queue가 복사한다. 반환 OK는 local queue admission뿐이다. |
| BLE send port | logical Type+semantic payload를 P4가 호출 중 복사/fragment한다. Notify submission과 앱 receipt는 별개다. |
| monotonic clock port | 수신 시각, deadline, AGE 계산에 사용한다. wall clock이나 반복 송신 시각으로 source AGE를 초기화하지 않는다. |

Gateway event는 `kind`, `received_at_ms`, `connection_generation`과 고정 크기 union을 갖는 것을 제안한다. Heap 할당과 borrowed pointer 보관을 금지한다. 초기 queue depth 후보는 16이나 **PROPOSED**이며 NimBLE+UART 동시 부하 측정 뒤 결정한다. Queue full은 계층별 counter/error로 남기고, 차량 `FAILED`/`DONE` 또는 BLE 전송 성공으로 변환하지 않는다.

반복 상태 snapshot은 최신값으로 coalesce할 수 있지만 `M_RESULT`, `M_WARNING`, `M_DIGITAL_RESULT`, `M_QUERY_END`와 모바일 원 요청은 덮어쓰지 않는다. Query 응답은 active QueryID가 있는 항목과 unsolicited `QueryID=0`을 분리한다. callback 이후 처리할 데이터는 언제나 Gateway queue가 소유하며, dequeue 이후에는 `GatewayTask`가 처리 완료까지 소유한다.

### 10.4 식별자, 중복 및 수명 규칙

| 식별자 | 발급/소유 | Gateway 규칙 |
|---|---|---|
| RequestID | Android | current Device+Session과 묶어 중복을 판별하고 그대로 UART에 전달한다. reconnect/Session 회전 뒤 pending 물리 Request를 replay하지 않는다. |
| QueryID | Android | 한 current context에서 단일 active Query를 권고한다. 동일 Query 재수신은 재실행하지 않고 기존 tracker와의 관계를 판정한다. 세부 응답은 OPEN이다. |
| SessionID | P6 정책 owner | P5가 발급하거나 bonding으로 추론하지 않는다. 0/not-ready 및 회전 의미는 P6 계약을 따른다. |
| DeviceContextID | 등록 owner | peer 주소 문자열로 합성하지 않는다. 승인된 등록 mapping에서만 받는다. |
| DomainBootID | Domain Alive/response | 변경되면 active query와 Domain 준비 상태를 무효화한다. 과거 boot 응답은 폐기한다. |
| LinkSequence | UART transport | P3 방향별 modulo-65536 sequence. application 중복키로 쓰지 않는다. |
| transferId | BLE fragmentation | BLE 방향별 fragment 조립 식별자. Request/Query/Session과 혼용하지 않는다. |

중복 Request 방어 cache의 크기·보존 시간과 이미 제출된 Request 재응답 방식은 `OPEN`이다. 최소 안전 규칙은 동일 `(DeviceContextID, SessionID, RequestID)` 물리 명령을 두 번 enqueue하지 않는 것이다. 결과를 모르면 `UNKNOWN`으로 유지하고 새 `DONE/FAILED`를 만들지 않는다.

### 10.5 Query tracker와 `M_QUERY_END` — PROPOSED / OPEN

active key 후보는 `(DeviceContextID, current SessionID, DomainBootID, QueryID, scope)`이다. tracker는 유효하고 해당 key와 일치한 **고유 응답**만 세며, 늦거나 중복된 응답, `QueryID=0` unsolicited snapshot, 다른 boot/session/scope 항목은 `ITEM_COUNT`에 포함하지 않는다.

- 최신 Network 제안에서 recent Result query는 Domain 보관 최대 8건 중 최신 최대 5건이다.
- `STATUS=0`은 필요한 목록 및 provenance가 모두 유효하고 실제 고유 수신 수 `N == ITEM_COUNT`일 때만 complete 후보이다.
- `STATUS=1`은 요청 결과 조회에서 원 Request 기록 없음이 확인된 경우, `STATUS=2`는 partial, `STATUS=3`은 context reject이다. 빈 scope의 정상 0건은 `STATUS=0,count=0` 후보이며 partial을 complete로 승격하지 않는다.
- 생산자는 같은 query의 전송 완료 항목 수를 `ITEM_COUNT`에 넣고 pending 항목보다 End를 늦춰야 한다는 것이 최신 Network 제안이다. 그러나 UART TX 완료는 ESP32 수신이나 BLE Notify receipt를 보장하지 않는다.
- duplicate/late End는 완료된 tracker를 다시 열지 않는다. timeout/disconnect/Domain boot/Session 회전은 tracker를 실패/불완전으로 닫고 물리 Request를 replay하지 않는다.

현재 Android `QueryCoordinator`는 status 2도 `statusOk`로 보고, matching response마다 duplicate 여부와 무관하게 received count를 증가시킨다. 이 차이와 partial/timeout의 사용자 표시를 양 끝에서 합의하기 전 Query completion binding은 `OPEN`이다.

### 10.6 `M_CONTEXT`와 `update_sequence`

P6의 세 producer가 Registration+Connection, App Activity, Proximity snapshot을 제공하고, P5 context publisher가 이를 38B `M_CONTEXT`로 조합·전송하는 책임 분리를 사용한다. 반복 publish는 producer의 `update_sequence`를 증가시키지 않고 monotonic AGE만 누적한다. 한 producer가 갱신되어도 다른 producer의 sequence/원 시각을 바꾸지 않는다.

최신 Network 설계 문서의 `Gateway_UpdateBasis_t`에는 `update_sequence`가 제안되어 있으나 실제 공개 `Gateway_Types.h`에는 `quality`, `age_ms`, `is_new_update`만 있다. 이는 ABI/소스 계약 차이이므로 public struct를 P5에서 임의 변경하지 않는다. 양단 header와 13 API binding 승인 전 `M_CONTEXT` production binding은 HOLD이며 mock fixture에서만 명시 sequence를 주입한다.

### 10.7 실패 및 모바일 표시 정책

- UART/Domain link down: Gateway Status의 `domainLinkUp=0`, `sessionReady=0`; 새 차량 Request 거절. 등록/본딩 사실을 지우거나 가짜 Domain state/result를 만들지 않는다.
- BLE disconnect/security loss: 해당 connection generation의 incomplete BLE transfer, pending-not-sent command와 active Query를 폐기한다. 이미 Domain에 제출된 Request를 자동 재전송하지 않고 재연결 후 읽기 전용 Query로 결과를 확인한다.
- BLE Notify queue/resource failure: local delivery failure로 기록한다. 원 Domain Result의 confirmation/quality/age를 바꾸지 않는다.
- stale/invalid source: 원 quality와 누적 AGE를 보존하고 차량 제어에는 fail-closed한다. 재중계가 freshness를 새로 만들지 않는다.
- 실제 Android UI가 partial/timeout/link-down을 표시하는 방식은 앱 담당자 결정 항목이며, P5는 정의되지 않은 `CONFIRMED`, `DONE`, `FAILED`를 만들어 보완하지 않는다.

### 10.8 P5와 P6 경계

| P5 Gateway bridge | P6 Session / Security |
|---|---|
| Type/길이/enum 검증 후 BLE↔UART payload 변환 | BLE peer 승인과 DeviceContextID mapping |
| event queue, routing, backpressure, timeout dispatch | SessionID 생성·회전·폐기 정책 |
| Request/Query 중복키와 active query lifecycle | encrypted/authenticated/bonded/registered 상태의 독립 판정 |
| Domain Alive/Boot를 입력으로 route 무효화 | App Activity 및 Proximity producer의 신뢰/TTL 판정 |
| Context snapshot을 UART 38B로 serialize | Context에 들어갈 승인된 원 상태 제공 |
| Gateway Status에 검증된 상태를 투영 | `registered`/`sessionReady` 상태 전이의 승인 규칙 |

P5는 P6의 실제 승인 정보가 없으면 차량 Request를 fail-closed하고, 격리된 mock test에서만 synthetic context를 주입한다.

## 11. 하드웨어 및 출시 차단 항목

- 실제 보드 MCU model, UART GPIO 번호, TX/RX 교차 연결, GND 공통, 전압(3.3V CMOS 여부), S32K344 UART 실측, 전원/Boot strap pin 확인 **미완료**. GPIO 번호/전압 레벨을 임의 지정하지 않는다.
- UART-M Domain 측과 **version/type/length/CRC/context 38B** 확정 및 양단 golden byte fixture 교환.
- GATT 128-bit UUID 5개, WRITE RESPONSE, CCCD, MTU≥24, fragment timeout, 앱 실제 APK 호환 시험.
- 등록 관리 및 디지털 키 원격 해제는 명시적 승인·현장 테스트 전 fail-closed.
- WINDOW는 `구현 보류 — 설계 유지`: decode 타입/테스트 fixture 가능, 실제 활성·fault alarm 금지.
