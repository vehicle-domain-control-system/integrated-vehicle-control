# Open Issues

## UART-M v2 / P2 Message Codec

The P2 codec follows `docs/references/network/interface/03_COMMUNICATION/20_UART_M.md`
without promoting its `WIRE_PROPOSED`, `TEAM_DECISION_REQUIRED`, `BINDING_OPEN`, or
`NOT_EXECUTED` items to an approved contract.

| ID | Open item | Current P2 treatment |
|---|---|---|
| UART-P2-01 | S32K344 and ESP32 have not recorded bilateral adoption of UART-M v2 framing, 38-byte `M_CONTEXT`, all 23 payload layouts, and golden vectors. | Codec and host fixtures are proposal-aligned only. Board/peer interoperability remains untested. |
| UART-P2-02 | The allowed target-temperature setting range is owned by the Domain function team and remains open. | `M_REQUEST` accepts the mobile-representable signed range `-32767..32767`; `-32768` is rejected because the mobile encoder excludes it. No narrower comfort range is invented. |
| UART-P2-03 | UART-M does not explicitly define whether `SESSION_ID=0` is reserved in every payload, although the broader session design uses zero as not-ready. | Codec preserves all u64 session values and leaves current-session acceptance to the Session/Gateway layer. `DEVICE_CONTEXT_ID=0` is rejected because its reservation is explicit. |
| UART-P2-04 | `Gateway_DataQuality_t.UNKNOWN`, C2 warning `UNCONFIRMED`, availability `UNSUPPORTED`, and several C2 `ResultReason_t` values have no exact wire code. | Codec accepts only the existing wire value sets and never substitutes a normal/success value. Semantic projection from those source enums remains blocked. |
| UART-P2-05 | `M_USER_SETTINGS` has no independent presence bits for NORMAL light level/RGB. | Codec handles the published 36-byte shape and sentinels only. Producing a partially known composite remains blocked as described by the source specification. |
| UART-P2-06 | Digital Key requires `far_run_ms` and other continuous fresh-sample evidence absent from `M_CONTEXT` 38B. | No fields are added or borrowed. Automatic Unlock remains disabled. |
| UART-P2-07 | `M_QUERY_END` TX-complete counting, zero-item semantics, partial completion, timeout, and BLE completion projection require two-end agreement. | Codec validates the existing scope/status/count fields only; it does not declare a Query complete. |
| UART-P2-08 | WINDOW activation has not been approved. | `M_WINDOW_STATE` and `M_WINDOW_FAULT` schemas can be encoded/decoded and tested, but descriptors mark them `schema_only`; no routing or functional activation is added. |
| UART-P2-09 | UART port, GPIO, voltage, baud/8N1 physical binding, and actual S32K344 firmware compatibility are not verified. | P2 remains a host-testable pure C codec and does not configure hardware. |

## UART-M v2 / P3-A Transport

| ID | Open item | Current P3-A treatment |
|---|---|---|
| UART-P3A-01 | UART2 GPIO27 TX/GPIO26 RX loopback was observed, but the professor-built PCB net, S32K344 electrical compatibility, common ground, and actual peer wiring remain unverified. | P5-B safe integration binds the explicitly supplied UART2/GPIO27/GPIO26 candidate in `app_main`; it is not evidence of S32K interoperability. Flash requires the existing GPIO27 push-pull conflict check. |
| UART-P3A-02 | 115200 bps, 8N1, no flow control, and 250 ms integration frame assembly timeout remain proposed rather than an approved two-end contract. | The transport remains configurable. P5-B uses these values as an explicit integration candidate only; it does not promote them to the UART-M contract. |
| UART-P3A-03 | Production queue depths, task stack sizes/priorities, UART ring sizes, and TX completion timeout need RAM/load measurement with NimBLE active. | P5-B supplies bounded candidate values, but board high-water/RAM-load measurement remains `NOT_TESTED_ON_BOARD`. |
| UART-P3A-04 | `uart_write_bytes()` plus `uart_wait_tx_done()` proves only local ESP32 driver completion, not Domain reception or vehicle acceptance. A later TX failure reporting path into Gateway is not bound yet. | Statistics preserve queue admission, write failure, completion timeout, and local completion separately. No Domain ACCEPTED/DONE result is synthesized. |
| UART-P3A-05 | UART-M specifies modulo-65536 `LINK_SEQUENCE`, but the restart seed/persistence policy is not defined. | `first_tx_sequence` is injected by the lifecycle owner. The transport increments once per attempted encoded TX and preserves `65535 -> 0`; it does not invent persistence. |

## BLE v0.3 / P4-A GATT Server

| ID | Open item | Current P4-A treatment |
|---|---|---|
| BLE-P4A-01 | The two Network `40_BLE.md` files still label UUID, properties, CCCD, MTU, and fragmentation `OPEN`, while the mobile v0.3 specification and Android source contain concrete values. | P4-A follows the checked-in Android implementation as a compatibility proposal. It does not relabel the Network contract as approved. |
| BLE-P4A-02 | The mobile v0.3 document describes MTU 23 Gateway Status READ polling, but `BleLinkProcedure.kt` rejects negotiated MTU below 24 and implements no polling loop. | The server never truncates or sends the 21-byte Status Notify below MTU 24. P4-B uses the actual Android minimum of 24; MTU 23 app behavior requires endpoint agreement. |
| BLE-P4A-03 | Android automatically calls `createBond()`, whereas the Gateway/Digital Key design requires a separately approved peer registration. | Bonded, encrypted, authenticated, and registered remain separate. P4-A routes no command to UART/Gateway and never marks a peer registered. Automatic Unlock remains disabled. |
| BLE-P4A-04 | Pairing association, MITM requirement, Secure Connections enforcement, first-device approval, and key recovery are `TBD-SEC-01`. | `ble_gatt_server_development_config()` enables bonding and SC capability with no-I/O/without MITM only as an explicit development profile. It is logged as not production-approved and must be replaced after the decision. |
| BLE-P4A-05 | The app and v0.3 GATT contract define Notify only, while the implementation request also mentions an Indicate API. | No Indicate property is invented. Public send APIs return `ESP_ERR_NOT_SUPPORTED` for Indicate mode. |
| BLE-P4A-06 | P4-B에서 Advertising, Android 연결, `encrypted=1`, `bonded=1`, App State Write 지속 수신은 확인됐다. NVS bond persistence, CCCD restore/reconnect, Gateway Status/Vehicle Data Notify의 Android 수신 및 UART/Domain end-to-end는 아직 별도 증거가 필요하다. | 확인된 항목만 board-tested로 기록한다. 미확인 Notify/복구/Domain 연동은 계속 `NOT_TESTED`이며 bonding을 registration으로 승격하지 않는다. |

## P5-A BLE ↔ UART-M Gateway Design

P5-A는 팀 Network `docs/network@00a4341`, Android `function-mobile@8054b69`,
Domain `main@fc28c2b`, ESP32 `origin/main@6900371`을 비교했다. 아래 항목은
문서 설계로 해결된 것이 아니라 P5-B 전 또는 해당 기능 활성 전 결정이 필요하다.

| ID | Open contract / conflict | Current P5-A treatment | Decision owner / request |
|---|---|---|---|
| P5-OPEN-01 | Network 설계의 `Gateway_UpdateBasis_t.update_sequence`가 실제 공개 `Gateway_Types.h`에 없고 문서도 `HOLD_BINDING`이다. | 13 public API와 struct를 변경하지 않는다. production `M_CONTEXT` binding은 HOLD, Host fixture만 명시 sequence 주입. | Network ESP32 API owner와 양 endpoint가 header/ABI 및 migration 승인 필요. |
| P5-OPEN-02 | `Gateway_RelayMessageView_t` 내부 `TYPE + P` subtype convention은 `PROPOSED/BINDING_OPEN`이다. | UART/BLE raw wire와 분리한 private relay codec 후보로만 취급한다. | Network Gateway API owner가 subtype representation과 owner를 결정. |
| P5-OPEN-03 | 최신 Network `M_QUERY_END`는 고유 성공 전송 수를 세고 status 2를 partial로 보지만 Android는 matching callback마다 count를 올리고 status 0/1/2를 모두 `statusOk`로 본다. | P5 tracker는 unique valid receive와 status 2 incomplete를 제안하되 binding/앱 완료 표시는 HOLD. | Network, Android, ESP32가 count/dedup/status/zero/late-End semantics와 fixtures 합의. |
| P5-OPEN-04 | 최신 Network/Domain query 출력은 recent Result 최대 5이나 모바일 설명 일부는 최대 8이다. Kotlin은 `recentRequestLimit=5`, 개별 recovery follow-up 한도는 8이다. | 한 recent bulk query의 relay 상한은 5. follow-up 8의 부하/의미는 변경하지 않고 확인 요청. | Android 문서/설정 owner가 bulk와 follow-up 한도를 명확히 기록. |
| P5-OPEN-05 | 모바일 문서의 ATT `SESSION_NOT_READY/NOT_REGISTERED/QUERY_BUSY`는 GATT Write response 시점의 결과를 요구하지만 현재 P4는 callback에서 enqueue한 뒤 P5가 비동기 판정한다. | 같은 ATT response에 결과를 허위로 넣지 않는다. P4 callback/API 변경이나 별도 application acknowledgement 정책 확정 전 semantic reject 경계 HOLD. | Android와 ESP32 BLE/Gateway owner가 동기 admission 범위 또는 응답 메시지 결정. |
| P5-OPEN-06 | 실제 Android/P4는 고정 UUID, MTU 24/185와 8B fragmentation을 사용하지만 Network BLE 문서는 여전히 해당 binding을 OPEN으로 둔다. | 검증된 P4 wire를 바꾸지 않는다. 팀 계약 승인 전 compatibility implementation으로 표시. | Network 문서 owner가 앱/P4 값을 채택하거나 양단 변경 Issue를 제시. |
| P5-OPEN-07 | Android Query timeout 3000ms, BLE reassembly 1000ms, Network Alive 250/750ms 및 state freshness 후보가 섞여 있으며 Request 원 전송 AGE는 UART wire에 없다. | timer를 용도별 분리하고 timeout으로 vehicle result를 합성하지 않는다. | Network/Android/Domain이 Query deadline, liveness, stale 및 request-age 처리 확정. |
| P5-OPEN-08 | SessionID 발급/회전, Domain reboot 이후 SessionReady 복구와 query-before-ready 허용 범위가 최종 승인되지 않았다. | P5는 P6가 제공한 verified context만 사용한다. bonding/연결로 ready를 추론하지 않고 reconnect 명령 replay 금지. | P6 security/session 설계와 Network/Android 양단 합의. |
| P5-OPEN-09 | 11개 CAN-projection/downlink schema의 source boot/sequence/quality/age를 Domain UART producer가 어떤 실제 source에서 채우는지 end-to-end evidence가 없다. | 수신 필드는 보존하며 zero/VALID/fresh 값을 만들어 채우지 않는다. | Domain Network/CAN owner가 각 schema provenance 및 golden vector 제공. |
| P5-OPEN-10 | Domain UART TX 완료, ESP32 수신, BLE notify submission 및 Android receipt는 서로 다른 완료 시점이다. QueryEnd producer/consumer가 어느 시점을 보장하는지 미확정이다. | 계층별 counter를 분리하고 BLE 성공이나 Query complete를 UART TX 완료에서 추론하지 않는다. | Network와 Android가 end-to-end completion/재시도 또는 무응답 정책 결정. |
| P5-OPEN-11 | `M_CONTEXT` 세 producer의 UNKNOWN wire mapping, TTL, sequence 시작/회전과 snapshot 원자성이 최종 binding되지 않았다. | 원 상태가 없으면 정상/VALID로 대체하지 않는다. producer별 timestamp/sequence를 독립 보존. | Gateway/P6 및 Network owner가 mapping, TTL, boot/reset fixtures 승인. |
| P5-OPEN-12 | Digital Key의 fresh continuous FAR evidence(`far_run_ms` 등)가 38B `M_CONTEXT`에 없다. | 실제 자동 Unlock route/enable 금지. BLE/UART reserved byte 또는 새 enum을 차용하지 않음. | Digital Key, Network, Domain의 two-end contract 승인 필요. |
| P5-OPEN-13 | 팀의 기존 ESP32 참고 header `docs/esp32/6_GatewayRouter/GatewayRouter.h`는 경로 복구 후 과거 Request 자동 replay를 책임으로 적지만 최신 Network/ESP32 안전 규칙은 replay를 금지한다. | P5는 reconnect/Domain reboot/Session rotation 뒤 물리 Request를 replay하지 않는다. team-owned 문서는 변경하지 않음. | 팀 ESP32 문서 owner가 기존 설명을 no-replay/query-only recovery로 정정할지 결정. |

### P5-B blocking subset

- P5-B pure mapper/core mock은 P5-OPEN-01/03/05를 명시적 HOLD fixture로 두고 시작할 수 있다.
- production `M_CONTEXT`는 P5-OPEN-01/11 해결 전 활성화하지 않는다.
- complete Query UI/판정은 P5-OPEN-03/10 해결 전 성공으로 표시하지 않는다.
- 실제 vehicle Request admission은 P5-OPEN-05/08과 P6 verified context 없이 활성화하지 않는다.
- Digital Key 자동 Unlock은 P5-OPEN-12 및 기존 UART-P2-06/BLE-P4A-03 해결 전 계속 비활성이다.

### P5-B first implementation treatment

| Area | Implemented boundary | Still HOLD / OPEN |
|---|---|---|
| Typed mapper | P4의 BLE semantic validator와 P2 UART-M codec을 조합해 0x10~0x12에 mock verified Device+Session을 prepend하고, 활성 downlink typed message를 BLE semantic bytes로 변환한다. | BLE/UART frame 자체와 public 13 API ABI는 변경하지 않는다. WINDOW route와 private Alive forwarding은 거절한다. |
| `M_CONTEXT` | `uart_m_context_t`를 정확한 38B schema로 encode하는 순수 helper만 제공한다. 모든 sequence/AGE는 호출자가 명시해야 한다. | P5-OPEN-01/11 해결 전 scheduler, UART enqueue와 production publish는 없다. |
| Gateway Core | Heap 없는 고정 배열, configurable bounded queue, enqueue deep copy, 단일 owner `process_next`, isolated mock candidate observer를 제공한다. | FreeRTOS multi-producer synchronization과 P3/P4 adapter 연결은 다음 단계다. `GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED=0`. |
| Request | `(DeviceContextID, SessionID, RequestID)` 최대 8개를 Session 동안 보존한다. 승인된 retire 규칙이 없으므로 full이면 새 candidate를 fail-closed하며 순환 덮어쓰지 않는다. Disconnect/Domain Boot에서는 보존하고 명시적 Session rotation에서만 비운다. | cache depth/retention 및 실제 UART 제출 실패 lifecycle은 P5-OPEN-05/07/08/10과 함께 OPEN이다. |
| Query | active context/Boot/scope를 확인하고 Result/Warning은 identity key, 단일 상태는 Type key로 중복 count를 제외한다. status2는 PARTIAL, count mismatch와 32개 고유 응답 capacity overflow는 별도 fail-closed outcome이며 QueryEnd BLE candidate를 만들지 않는다. | response identity/최대 32개 fixture와 완료/부분 Notify binding은 PROPOSED. Android `received++` gap(P5-OPEN-03)은 해결되지 않았다. |
| Timeout/Stale | Host config로 event/context/query/Alive deadline을 분리하고 timeout을 차량 Result로 바꾸지 않는다. | 수치는 isolated mock proposal이며 production policy 승인이 아니다. |

`authenticated`는 BLE SMP의 MITM/authenticated-link 사실이며 Just Works에서는 false일 수 있다. P5-B는 이를 Application 인증으로 승격하지 않고, P6 또는 isolated Mock이 별도로 제공하는 `authorization_verified`와 verified Device/Session, registration을 admission 근거로 사용한다. `authorization_verified`는 연결·암호화·bonding만으로 생성하지 않으며 production routing은 계속 비활성이다.

Core 시간 입력은 wrap되지 않는 `uint64_t` monotonic milliseconds 계약이다. ESP-IDF adapter는 `esp_timer_get_time()/1000` 같은 64-bit 원천을 사용하고 32-bit FreeRTOS tick을 단순 승격하지 않는다. Mock observer가 받는 candidate pointer는 callback 동안만 유효하며 보관하려면 반환 전에 deep copy한다.

### P5-B second integration treatment

- `GatewayTask` 하나만 `gateway_core_t`를 소유한다. P3 dispatch task와 P4 event task는 고정 크기 event를 FreeRTOS queue에 zero-wait deep copy하며 full을 별도 통계로 남긴다.
- 런타임 adapter에는 P6 Context 공급 API와 UART TX port가 없다. BLE command는 Core의 fail-closed gate에서 거절되며 `production_uart_tx_count`는 구조적으로 0이다.
- P3의 CRC/length/direction/driver 오류 통계와 Adapter queue-full 통계는 서로 다른 계층에 보존한다. Adapter는 검증 완료된 P3 callback만 수신한다.
- UART `M_DOMAIN_ALIVE`는 Core liveness/boot invalidation과 fail-closed Gateway Status에만 반영한다. `registered`, `sessionReady`, DeviceContextID, SessionID를 생성하지 않는다.
- Gateway Status READ/Notify는 기존 P4 API를 유지한다. Vehicle Data/Query completion/M_CONTEXT production routing은 P6 및 OPEN 계약 승인 전 HOLD다.
- 기본 queue 12개와 GatewayTask stack 6144 bytes는 측정 전 candidate다. Queue/Task는 FreeRTOS heap에서 bounded allocation되며 실제 high-water와 NimBLE 동시 부하는 `NOT_TESTED_ON_BOARD`다.
