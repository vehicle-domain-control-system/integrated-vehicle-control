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
- P3의 CRC/length/direction/driver 오류 통계와 Adapter의 UART queue-full/type/Core-reject 통계는 서로 다른 계층에 보존한다. P6-B4-A GatewayTask는 P3 callback의 typed message를 deep-copy한 뒤 descriptor·RX direction·schema를 재검증한다.
- UART `M_DOMAIN_ALIVE`는 Core가 quality/AGE를 수용한 경우에만 safe Gateway Status의 boot/state snapshot에 반영한다. stale/invalid Alive는 link를 fail-closed로 내리며 `registered`, `sessionReady`, DeviceContextID, SessionID를 생성하지 않는다.
- Gateway Status READ/Notify는 기존 P4 API를 유지한다. Vehicle Data/Query completion/M_CONTEXT production routing은 P6 및 OPEN 계약 승인 전 HOLD다.
- 기본 queue 12개와 GatewayTask stack 6144 bytes는 측정 전 candidate다. Queue/Task는 FreeRTOS heap에서 bounded allocation되며 실제 high-water와 NimBLE 동시 부하는 `NOT_TESTED_ON_BOARD`다.

## P6 Registration / Session OPEN items

| ID | OPEN | 현재 fail-closed 처리 |
|---|---|---|
| P6-OPEN-01 | Production 최초 등록 승인과 strong peer credential | 명시적 registration pending + bonded/encrypted 단일 peer만 개발 정책으로 허용; runtime binding 없음 |
| P6-OPEN-02 | NVS namespace/schema migration/encryption, durable reset intent와 stale ACTIVE rollback 방지 | Reset 성공은 intent COMMITTED 이후만 반환. NOT_COMMITTED/UNKNOWN은 RESET_REJECTED. intent 미기록 재부팅은 증명 불가능하므로 Production reset/binding HOLD |
| P6-OPEN-03 | DeviceContextID/SessionID CSPRNG와 재부팅 간 non-reuse | 현재 boot는 16개 retired Session cache가 가득 차면 발급 중단. Production generator/boot epoch HOLD |
| P6-OPEN-04 | Session timeout/reconnect/AppInstance rotation 양단 합의 | configurable Host 정책; 불일치 시 Session 폐기 |
| P6-OPEN-05 | Bond 삭제와 registration reset 원자성 | COMMITTED marker 이후만 erase. dual-slot journal/epoch 없이 NOT_COMMITTED 재부팅을 식별할 수 없어 실제 NimBLE/NVS 삭제 HOLD |
| P6-OPEN-06 | Gateway 공개 API에 authorization provider 연결 | `P6_PRODUCTION_BINDING_ENABLED=0`; 차량 UART TX 비활성 유지 |
| P6-OPEN-07 | Registration/Session 폐기 BLE 통지 | 기존 UUID/payload에 새 값을 만들지 않음 |
| P6-OPEN-08 | RSSI/FAR freshness와 38B `M_CONTEXT` 증거 | Digital Key 자동 Unlock 비활성 |
| P6-OPEN-09 | NimBLE Bond Store에는 기존 peer가 있어도 P6 Registration Store 매핑이 없음 | P6-B2-1은 identity/security를 관측만 하고 Session Authorization과 Production routing을 생성하지 않음 |
| P6-OPEN-10 | P6 등록 최대 1대와 NimBLE `CONFIG_BT_NIMBLE_MAX_BONDS` 기본 저장 개수의 정리 정책 | 기존 Bond/NVS 보존을 위해 설정·삭제를 변경하지 않으며 P6-B2-2 승인 전 자동 교체 금지 |
| P6-OPEN-11 | GAP callback의 `ble_store_util_bonded_peers()` bounded Store 조회 지연 | NimBLE Store thread-safety 근거 없이 다른 task로 이동하지 않음; 실제 보드 callback 시간과 BLE/Gateway queue overflow를 측정 |
| P6-OPEN-12 | 최초 등록을 승인할 Production workflow와 권한 주체 | P6-B3 개발 보드에서 기본 비활성 UART0 request/approve, 30초 TTL, 격리 `p6_regdev` 저장·재부팅 Bond reconciliation을 확인했다. 이는 물리 포트 접근에 의존하는 개발 fixture일 뿐 Production 승인 근거가 아니며 Android/Bond/App State 자동 등록은 계속 금지한다. |
| P6-OPEN-13 | 실제 NimBLE exact-peer delete port 및 개발자 전용 reset 진입 보호 | 2단계 coordinator와 Mock recovery만 구현; `P6_DEVELOPER_BOND_RESET_ENABLED=0`, BLE binding 없음, 실기기 삭제 NOT_TESTED |
| P6-OPEN-14 | NVS commit UNKNOWN 뒤 old ACTIVE rollback을 독립적으로 판별할 monotonic revocation source | 현재 boot는 즉시 revoke하고 확인된 RESET_PENDING은 재부팅 후 fail-closed. marker가 실제 미반영된 UNKNOWN은 NVS 단독으로 증명 불가. dual-slot journal, authenticated monotonic record, eFuse/보안소자 anchor는 PROPOSED이며 검증 전 Production 사용 금지 |
| P6-OPEN-15 | Registration ACTIVE save의 COMMIT_UNKNOWN이 실제 반영됐는지 재부팅 후 판별할 durable admission marker | 현재 boot는 REGISTERED 전이를 금지한다. 실제 반영된 UNKNOWN은 재부팅 시 ACTIVE load가 가능하나 Session/Authorization은 복구하지 않고 exact Bond reconciliation을 요구한다. transactional Registration/journal 승인 전 Production binding HOLD |

### P6-M5 monitoring Context binding boundary

- GatewayTask의 `synchronize_monitor_context()`는 Session tick 후 기존 P6 Snapshot,
  exact Bond reconciliation, 현재 BLE handle/generation/epoch 및 Domain Boot/Link를
  재검증하고 `gateway_core_enqueue_context()`에 deep copy한다. Source AGE/Sequence는 만들지 않는다.
- 모니터링 Context는 `session_ready=false`이며 이 모드의 BLE App Command는 UART 후보를 만들지 않는다.
  Session/인증/연결/Boot/Link 무효화 시 Core admission과 기존 Notify/Request/Context 후보를 폐기한다.
- 기본 Firmware는 모니터링 연결도 비활성이다. P6-M6의 기본 n인
  `CONFIG_P6_READ_ONLY_MONITORING`은 차량 제어의 `P6_PRODUCTION_BINDING_ENABLED=0`과
  독립적으로 기존 승인된 P6 Snapshot만 소비한다. 개발 Registration/NVS 테스트 옵션과 함께 켤 수 없다.
  Host 명시적 opt-in은 실제 Pure C Session FSM과 Mock Store/BLE로만 연결을 시험하며 Production 승인 근거가 아니다.
- 실제 모바일 표시는 P6-OPEN-01/03/04/06/12/15의 승인과, Gateway Status의 실제
  DeviceContextID/SessionID를 Domain과 공유하는 승인된 연결이 필요하다. 모니터링 opt-in에서
  검증된 현재 Context만 기존 21B Gateway Status의 등록=1, DeviceContextID(B4), SessionID(B8)에 복사한다.
  SessionReady(B3)는 0이며 무효화 시 식별자는 0, 등록은 UNKNOWN으로 돌아간다.
  Production M_REQUEST/M_CONTEXT/M_QUERY TX, Auto Unlock 및 차량 제어는 계속 비활성이다.

### P6-M6 remaining end-to-end dependency

- `sdkconfig.defaults;sdkconfig.p6_monitoring.defaults`를 `SDKCONFIG_DEFAULTS`로 지정한
  별도 build/config에서만 모니터링 옵션을 명시적으로 켠다. `p6_reg`/Bond를 쓰거나 삭제하지 않는다.
  기존 등록정보의 Production 승인 정책 자체는 위 OPEN 항목이며 이 옵션이 승인 근거를 만들지 않는다.
- Gateway Status가 현재 Context를 Notify API에 접수하기 전에는 Vehicle Data 후보를 보류한다.
  ATT 접수는 Android 수신 확인이 아니다. Android `SessionContext.matches()`는
  SessionReady=false에서도 세 식별자를 비교하므로 기존 Read-only 수신 경로를 재사용할 수 있다.
- Network `0cc4ae0`의 `nd_context()`는 현재 UART PortEpoch에 수신된 M_CONTEXT가 있어야
  자발적 Downlink의 수신 DeviceContextID/SessionID를 선택할 수 있다. BLE Status만으로 Domain은 알 수 없다.
  승인된 M_CONTEXT freshness/TTL/재전송·무효화 정책 및 Domain Owner 연결 전까지 E2E는 BLOCKED다.
  기존 Metadata Producer의 미설정 정책을 임의 OK/age=0으로 바꾸거나 자동 UART TX를 연결하지 않는다.

### P6-M7 M_CONTEXT-only submission boundary

아래 M7/M8의 NULL 정책·미승인·SessionReady 충돌 기록은 당시 기준이다.
최신 read-only 시연 설정과 확인된 로컬 Provider는 마지막 Runtime 항목을 따른다.

- `gateway_adapter_bind_monitor_context_tx()`는 Transport의 borrowed handle과 정책을
  GatewayTask queue로 복사한다. main은 기존 UART init 후 **NULL 정책**으로 port만 연결한다.
  모니터링 옵션은 기본 n, `P6_PRODUCTION_BINDING_ENABLED=0`이며 실제 TX는 계속 HOLD다.
- 승인된 정책에는 기존 Producer의 TTL/Sequence seed와 Context 갱신 주기가 모두 필요하다.
  코드가 값을 선택하지 않는다. Host의 10/100ms 값은 테스트 fixture일 뿐 Production 정책이 아니다.
  binding queue 포화로 입력이 손실되면 Adapter 재시작 전까지 Context TX를 닫는다.
- 승인 정책 하에서만 GatewayTask가 기존 후보를 다시 만들고 P6/Session/Generation/Epoch,
  Domain Boot/Link, Gateway Status의 Device/Session 일치 및 두 Source Quality=OK를 재검증한다.
  M8의 `uart_m_transport_send_context()`에 넘기는 Type은 M_CONTEXT로 고정이며 접수는 deep-copy queue
  admission만 뜻한다. Driver 완료는 Transport 통계, Domain 수용은 별도 검증이다.
- Queue Full/오류는 성공으로 세지 않는다. 같은 실패 buffer를 보관하거나 즉시 재시도하지 않는다.
  다음 승인 주기에 현재 증거로 다시 조립하며 Source AGE/Sequence를 송신 때문에 갱신하지 않는다.
- **BLOCKED:** 승인된 갱신 주기, Source TTL/Sequence/reset 정책, Domain Context TTL과 철회 통지가 없다.
  Network `0cc4ae0`은 UART link quiesce/resume/실패 시 Context를 비우지만 BLE Disconnect를
  직접 알 수 없다. 송신 중단만으로 Domain Context가 언제 폐기되는지 합의가 필요하다.
- P6-M8은 기존 TX Queue에 Context 전용 생성시각/TTL/revision을 추가했다. Driver 제출 선점 전
  오래된 revision은 버리고, 원본 Source AGE에 대기를 더해 Codec/CRC로 Frame을 만든다.
  Queue 체류 한도와 폐기/TTL 계약 승인 전 실제 정책은 계속 NULL로 유지한다.
- Domain 원격 develop=`abf12cd`, Network 원격=`0cc4ae0`. 읽은 별도 P6-B8-E Domain 작업물의
  `DigitalKeyContextManager`는 같은 Sequence의 AGE 역행/Quality 완화를 거절하고 명시적
  `Invalidate()`를 제공하지만 미커밋 파일이다. 최신 Phase Production 통합/TTL 근거로 확정하지 않는다.

### P6-M8 Context queue cancellation / expiry

- M_CONTEXT는 `uart_m_transport_send_context()`로만 접수한다. 기존 일반 send는 Context에
  NOT_READY를 반환하며 다른 Type의 Queue/Driver 동작은 유지한다. Guard의 TTL 값은 필수 입력이다.
  Gateway는 현재 DTO 생성 시각과 승인 정책의 Source/Queue TTL을 복사한다. 기존 Source TTL은
  Registration/Connection의 결합 필드이며 제안된 개별 600/1000ms를 임의로 선택하지 않는다.
- Gateway Context 무효화, Port/정책 변경, binding 입력 손실, 정상 Task stop은 Transport revision을
  무효화한다. 후보/미제출 항목과 Driver로 넘긴 항목을 구분한다. Source/Context 만료·시각 역행은
  Driver 직전에도 검사한다. 취소된 항목은 dequeue 때 버리므로 다른 메시지를 비우지 않으며,
  폐기 전까지 Queue 용량을 차지할 수 있다. 재접속/복구가 이전 revision을 되살리지 않는다.
- 대기 AGE는 원본 DTO의 AGE+생성 이후 monotonic 경과다. Source Sequence 및 Quality/Validity/reason은
  그대로 둔다. UNKNOWN AGE는 유지하고, INVALID의 알려진 AGE overflow는 UINT16_MAX로 포화한다.
  OK AGE가 UNKNOWN으로 overflow하거나 TTL 경계 이상이면 Context를 보내지 않는다.
- Driver 제출의 최종 lock에서 TTL/revision 확인과 고정 38B payload의 순수 Codec/CRC를 함께
  처리하고 제출을 선점한다. 할당·callback·UART/RTOS 대기는 lock에 넣지 않는다. 그 전에 도착한
  invalidate는 폐기를 보장한다. **선점 후 uart_write_bytes 진입 전 선점당한 구간도 in-progress**로
  반환하며 그 Frame이 취소됐다고 주장하지 않는다. UART 호출/대기 자체는 lock 밖에서 실행한다.
  이미 Driver buffer/FIFO/전선으로 넘어간 데이터는 회수하지 않는다. 짧은 write/timeout도 일부 전송
  가능성이 있어 UNKNOWN 결과이며 성공/차량 실행 결과를 만들거나 재시도하지 않는다.
- Transport 통계 `tx_messages_queued`, `tx_context_cancelled/expired`,
  `tx_context_driver_submit_calls`, `tx_write_errors`, `tx_frames_completed`를 구분한다.
  Queue 접수, 제출 in-progress, Driver API 호출, 물리 TX 완료는 서로 다른 관측이다.
- Domain 후보 200/600/1000/750ms는 **미승인**이며 Production 설정에 넣지 않는다. Host의 5/30/50/100ms
  등은 테스트 fixture다. Domain의 SessionReady=false 보호 상태 차단과 ESP32 모니터링의 false 유지가
  충돌하므로 **BLOCKED**다. 기존 SessionReady/인증 의미는 변경하지 않는다.
- 실제 dual-core 선점 및 UART 물리 시험은 NOT_TESTED. 선점 이후의 절대 취소가 필요하다면 별도
  Driver 제출 계약/동기화 승인이 필요하며 현재 닫힌 Gate를 해제할 근거로 Host 결과를 사용하지 않는다.

### Read-only Monitoring Runtime — 채택된 초기 운영 계약

- 사용자 승인값: Context 갱신 200ms, Registration/Connection 결합 Source TTL 600ms,
  APP_STATE TTL 1000ms, Domain 수신 Context lease 750ms. Queue TTL은 필수 Source의 600ms를
  상한으로 사용하며 원본 AGE 때문에 더 일찍 만료될 수 있다. Proximity는 UNKNOWN/NO_DATA다.
- `CONFIG_P6_READ_ONLY_MONITORING`은 기본 n이다. 명시적 monitoring profile만 위 정책을
  기존 GatewayTask/guarded Transport에 연결한다. P6/차량 제어 Production binding은 계속 0,
  ESP SessionReady=false이며 M_REQUEST/M_QUERY/Auto Unlock은 연결하지 않는다.
- NimBLE Host queue에서 200ms마다 현재 descriptor, resolved Identity와 실제 Bond Store를
  읽는다. Generation/Epoch/handle, 등록 Identity, 현재 P6 증거가 일치한 관측만 GatewayTask가
  Source에 반영한다. Timer 실행/재송신은 재관측이 아니며 Session을 갱신하거나 승인하지 않는다.
  관측 손실·불일치·만료는 fail-closed다. App Activity는 APP_STATE 수신으로만 갱신한다.
- 2026-11-25 로컬 Production 프로젝트
  `<DOMAIN_PROJECT>`을 읽기 전용 확인했다.
  `src/application/domain_network_admission_bridge_private.c/.h`의 InitMonitoring/BindApi/WrapOwner와
  `network/src/node/net_node_domain.c`의 실제 C2 binding이 위 TTL을 사용한다.
  `.cproject`의 CPU_S32K344/NET_BOARD_DOMAIN_C2_HOOK도 확인했다. Git main/develop을
  이 미커밋 Production 구현의 대체 근거로 사용하지 않는다. Domain ARM/보드 실행은 NOT_TESTED다.
- Domain private `monitoringSessionReady`는 read-only 수용 사실이며 ESP SessionReady나
  P6/control authorization이 아니다. 공개 Wire에 P6 증명을 추가하지 않는다. 이 저전압 시연은
  신뢰된 ESP32 단독 UART Producer를 전제로 한다. 외부 UART 주입자를 인증하는 계약은 아니다.
  일반 Transport send는 M_CONTEXT를 거부하고 기존 Gateway 검증된 guarded 경로만 사용한다.
- Android `function-mobile@8054b69`의 heartbeat 기본값은 1000ms여서 APP_STATE TTL 경계와
  충돌한다. 앱 담당자는 실제 foreground를 반영한 500ms 목표 heartbeat와 적절한 tick
  (예: 100ms 이하)을 적용해야 한다. inactive는 기존 실제 상태 전환 이벤트를 사용한다.
  ESP32가 화면 활성 여부를 추정하지 않으며 수신 중단 시 Source가 만료된다.
- 기존 Session 만료/Disconnect/Boot/Link 무효화와 M8 Driver 제출 선점 후 회수 불가 경계는 유지한다.
  Host 검증은 물리 TX 완료, Domain 수용 또는 Android Notify 수신 성공을 뜻하지 않는다.
- 최종 소프트웨어 교차 검증은 `tests/monitoring_interop`의 기존 양쪽 fixture 재사용 target이다.
  CMake 입력은 `DOMAIN_PROJECT`(위 Production root), `DOMAIN_FIXTURE_DIR`(기존
  `_worktrees/phase5m33q2/DOMAIN/tests/host`)이다. 외부 프로젝트에는 쓰지 않는다.
  단일 Host 프로세스에서 실제 Gateway/P6 FSM → 실제 UART Queue/AGE/Encoder → 실제 Domain
  Decoder/Monitoring Provider → CIS Environment·Rear Runtime Publisher → 실제 Network Encoder →
  ESP32 Parser/Core/Mapper → BLE Notify Mock을 실행한다. P6/Bond/NVS 입력·시계·센서 관측과
  물리 Driver/NimBLE Notify는 Host fixture다. 실제 Android 수신이나 양쪽 보드 E2E PASS가 아니다.
- 검증 중 Domain 담당자의 Production 수정이 반영됐다. 최종 Provider SHA-256은
  `AE9F7A475A49450334FF8A48D553C08A94469840B8E03A99F0FE27D7D29AD584`이다.
  실제 `main.c`의 `domain_network_transition_binding()` → `NetDomainBoard_SetTransitionBinding()`을
  fixture에서도 재사용한다. Environment 첫 보고는 barrier HOLD, 실제 C2 step의 Domain transition
  완료 후 다음 보고만 수용한다. Receipt/confirm_snapshot 성공을 위조하지 않는다.
- Domain Hardware Gate=false로 Net_Init이 실행되지 않는 현재 보드 상태는 별도 제한이다.
  실제 Hardware Gate 승인/펌웨어 적용, UART 전기 조건, Android 500ms heartbeat와 실제 Notify 수신은
  통합 실기기 검증 전 완료해야 한다. 본 작업은 Flash나 Gate 변경을 수행하지 않는다.
