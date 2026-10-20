# ESP32 Interface Mapping — Android BLE v0.3 ↔ Gateway API ↔ UART-M v2

- 상태: Draft / 상호 운용성 개발 계약 후보 (2026-10-17, P5-A 검토)
- 본 문서의 앱 BLE 사실은 `mobile(1).zip`, `MOBILE_BLE_코드_한줄해설.pdf`; UART는 `20_UART_M.md`, Gateway는 `interface/02_NODES/ESP32/*` 기준.
- ESP32는 **양쪽 프로토콜을 연결하는 Adapter를 직접 구현**한다. 실제 Domain 프로토콜 adoption/보드 시험은 아직 확인되지 않음.

## 1. 중요한 규칙

1. BLE `PROTOCOL_VERSION=1`과 UART-M `VERSION=2`는 별개다.
2. BLE App Command와 Vehicle Data의 8B 헤더는 UART Frame Header가 아니다.
3. BLE 상향 논리 payload는 `M_REQUEST=12`, `M_QUERY=20`, `M_WARNING_ACK=9`B. UART-M 상향은 해당 payload 앞에 **검증된 device 4B + session 8B**를 삽입해 각각 24/32/21B로 송신한다.
4. UART-M 하향 상태 payload는 `DOWN_CONTEXT=20B`를 포함한 원래 길이로 앱에 BLE 분할 전송한다. 단 수신 peer/Session/Domain Boot/QueryID를 검증하고, 앱에 공개하지 않는 private `M_DOMAIN_ALIVE`는 제외한다.
5. CRC, UART link sequence, raw UART header는 BLE로 중계하지 않는다.
6. 송수신하는 C 구조체를 `memcpy(sizeof(struct))`로 직접 직렬화하지 않고 필드별 오프셋·LE codec을 사용한다.
7. 메시지 Type/길이를 허용 목록으로 엄격히 검사한다. 앱 소스의 `MessageType.kt` 목록을 근거로 한다.

## 2. BLE GATT 계약

| GATT 객체 | UUID | Properties / byte |
|---|---|---|
| Service | `A0F00000-7C21-4B5A-9E32-5F4D4F42494C` | Advertising Service Data/UUID에 포함 |
| Gateway Status | `A0F00001-7C21-4B5A-9E32-5F4D4F42494C` | Read + Notify, raw 21B |
| App Command | `A0F00002-7C21-4B5A-9E32-5F4D4F42494C` | Write With Response, BLE v1 framed |
| Vehicle Data | `A0F00003-7C21-4B5A-9E32-5F4D4F42494C` | Notify, BLE v1 framed |
| App State | `A0F00004-7C21-4B5A-9E32-5F4D4F42494C` | Write With Response, raw 5B |

- BLE 8B fragment header: `B0 version=1`, `B1 type`, `B2..3 transferId LE`, `B4..5 totalPayloadLength LE`, `B6 index`, `B7 count`, `B8..fragment`. `transferId=0` 예약; wrap `65535→1`; 분리된 방향별 sequence.
- App preferred MTU 185, minimum supported 24. Fragment useful bytes = negotiated ATT MTU - 11. 앱 코드에는 1000ms reassembly timeout.
- 한 방향의 서로 다른 Transfer fragment를 interleave하지 않는다; 재연결·timeout에 incomplete fragment 무효화. TYPE, total, count가 다르면 조립 폐기.
- `Gateway Status`와 `App State`에는 **이 8B 헤더가 없다**.

### Gateway Status raw(21B)

| byte | field |
|---|---|
| 0 | BLE protocol version 1 |
| 1 | registration (0 not registered, 1 registered, 255 unknown) |
| 2 | Domain link up (0/1) |
| 3 | session ready (0/1) |
| 4..7 | deviceContextId u32 LE, 0 reserved |
| 8..15 | sessionId u64 LE, 0 reserved as not ready |
| 16..19 | domainBootId u32 LE; not known=0 |
| 20 | domainState u8; unknown=255 |

### App State raw(5B)

- offset0 `APP_ACTIVE_STATE` 0/1; offset1..4 `APP_INSTANCE_ID` u32 LE, 0 invalid.
- 앱 foreground heartbeat 기본 1000ms; ESP32 수신 이후 monotonic age를 관리하고 activity TTL은 별도 정책값(디지털키 제안 3000ms)으로 평가.

## 3. 상향 BLE → UART-M 바이트 매핑

| 메시지/type | BLE Payload (의미 오프셋) | UART-M Payload (의미 오프셋) | 검증 |
|---|---|---|---|
| `M_REQUEST 0x10` | 12B: `requestId[0..3]`, kind[4], op[5], arg[6..11] | 24B: `deviceId[0..3]`, `sessionId[4..11]`, BLE12B → UART[12..23] | kind/op/arg, current registered encrypted peer, 신뢰 가능한 Session, READY 조건 |
| `M_QUERY 0x11` | 20B: `queryId[0..3]`, scope[4], reserved[5], requestSession[6..13], requestId[14..17], cursor[18..19] | 32B: Device/Session[0..11] + BLE20B → UART[12..31] | `queryId!=0`, scope 0..3, reserved=0, cursor=0, 원 요청 조회 context |
| `M_WARNING_ACK 0x12` | 9B: warningType[0], occurrenceId[1..4], warningDomainBoot[5..8] | 21B: Device/Session[0..11] + BLE9B → UART[12..20] | 현재 peer/Session, 원 Domain Boot와 warning occurrence 일치 |

- `REQUEST_ID`는 앱 발급. UART `LINK_SEQUENCE`는 ESP32 UART 송신 owner. Domain 명령 ID는 Domain App 발급. **임의 재발급/혼용 금지**.
- 최초 Query는 `SESSION_READY=0`에서도 Network가 문맥 검사 후 허용할 수 있도록 설계됨. **모든 Query를 READY=1로 차단하면 앱 초기 Sync와 교착**한다. Query는 수동 차량 제어가 아니며 별도 수용 규칙을 적용.
- 앱 foreground 및 이전 요청 결과 확인에 필요한 Query와 차량 동작 Request를 동일한 재송신 정책으로 처리하지 않는다.

## 4. 하향 UART-M → BLE Vehicle Data

아래 **UART payload semantic length = 앱 BLE semantic length**, 단 BLE 전송 시 반드시 별도 8B BLE Fragment header를 붙인다.

| Type | name | semantic B | Route |
|---|---|---:|---|
| `0x20` | M_RESULT | 44 | Domain→ESP32→App |
| `0x21` | M_WARNING | 40 | Domain→ESP32→App |
| `0x22` | M_AVAILABILITY | 62 | Domain→ESP32→App |
| `0x23` | M_DIGITAL_STATUS | 30 | Domain→ESP32→App |
| `0x24` | M_DIGITAL_RESULT | 35 | Domain→ESP32→App |
| `0x25` | M_USER_SETTINGS | 36 | Domain→ESP32→App |
| `0x30` | M_BCM_DOOR_STATE | 40 | Domain→ESP32→App |
| `0x31` | M_BCM_CLIMATE_STATE | 44 | Domain→ESP32→App |
| `0x32` | M_BCM_LIGHT_STATE | 45 | Domain→ESP32→App |
| `0x33` | M_BCM_STATUS | 42 | Domain→ESP32→App |
| `0x34` | M_CIS_ENVIRONMENT | 60 | Domain→ESP32→App |
| `0x35` | M_CIS_OCCUPANT | 45 | Domain→ESP32→App |
| `0x36` | M_CIS_REAR | 39 | Domain→ESP32→App |
| `0x37` | M_CIS_STATUS | 54 | Domain→ESP32→App |
| `0x38` | M_WINDOW_STATE | 42 | 설계 유지/기능 구현 보류 |
| `0x39` | M_WINDOW_FAULT | 42 | 설계 유지/기능 구현 보류 |
| `0x3A` | M_VSS_STATUS | 48 | Domain→ESP32→App |
| `0x40` | M_QUERY_END | 24 | Query validator → 앱 Query 종료 반영 |

- `M_DOMAIN_ALIVE(0x02,12B)`는 UART RX까지만, ESP32 private 링크 감독. 앱으로 forwarding **금지**.
- `M_CONTEXT(0x01,38B)`는 ESP32의 세 독립 관측 업데이트(RegistrationConnection/AppActivity/Proximity)를 합성해 UART로 주기 송신, BLE 앱에서는 수신하지 않음.
- `M_QUERY_END`: 원 QueryID, scope, current Device/Session, DomainBoot 대조, 실제 유효 수신 count와 itemCount 검증. 앱은 상태 scope 0의 필수 12종, 경고 scope 2의 8종도 검증한다. `STATUS=2` 일부 확인 불가는 전체 성공으로 오인 금지; 앱이 이해하는 원 0x40/24B 종료 의미 유지.
- `M_RESULT`의 `request_session`은 **요청을 생성한 과거 세션**일 수 있으므로 현재 수신 단말 Session과 혼동하지 않는다. `CONFIRMED`는 실제 피드백 증거가 있을 때만.

## 5. Gateway public API와 호환

다음 13개 선언은 `interface/02_NODES/ESP32/20_API.md`와 일치해야 한다. 이 문서에서는 축약 표기이며 **코드에 복사할 실제 원형은 원문에 있는 반환형/인자를 우선**한다.

| 방향 | Gateway public name |
|---|---|
| 초기화·검증 | `Gateway_Interface_Init`, `Reset`, `IsInitialized`, `IsValidByteView`, `IsValidMessageContext`, `IsValidRelayMessage` |
| BLE Adapter → Core | `Gateway_Interface_OnMobileMessage` |
| UART Adapter → Core | `Gateway_Interface_OnDomainMessage` |
| Core → UART-M | `Gateway_Interface_SendToDomain` |
| Core → BLE | `Gateway_Interface_SendToMobile` |
| 측정 원 상태 | `Gateway_Interface_PublishRegistrationConnection`, `PublishAppActivity`, `PublishProximity` |

`Gateway_RelayMessageView_t`는 `type/direction/context/payload`를 가진 view다. TYPE+P(payload 맨 앞 1B subtype)는 `25_PRIVATE_ROUTING.md`의 PROPOSED 규칙이고 UART raw frame이 아니다. Bridge의 선택 구현은 별도 내부 codec에 가두며 타입 enum의 의미 코드와 wire 숫자를 직접 memcpy하지 않는다.

## 6. UART-M Frame/CRC 및 API acceptance

| UART frame byte | 필드 |
|---|---|
| 0..1 | Sync `A5 5A` |
| 2 | VERSION `02` |
| 3 | MESSAGE_TYPE |
| 4..5 | PAYLOAD_LENGTH u16 LE 0..128 |
| 6..7 | LINK_SEQUENCE u16 LE |
| 8..(7+P) | Payload P bytes |
| 8+P..9+P | CRC16 little endian, CRC-16/CCITT-FALSE of bytes 2..7+P |

- Max frame=138B. `M_REQUEST` payload24B → UART frame34B. `M_CONTEXT` payload38B → UART frame48B.
- UART receiver는 bytes chunk boundary가 message boundary라고 가정하지 않는다. `SYNC`가 payload에도 올 수 있으므로 length·CRC로 조립한다.
- exact Type payload length / reserved enum / Device+Session / Boot+Seq, age+freshness를 검증하고 valid typed record만 상위로 전달한다.
- `NET_SUBMIT_OK`, `GATEWAY_STATUS_OK`, UART TX 완료, 상대 Accepted, 차량 Done은 **전부 다른 상태**다.

## 7. P5-A 23종 라우팅 표

아래 표는 팀 Network `docs/network@00a4341`, Android `function-mobile@8054b69`, 현재 P2 codec을 대조한 결과다. 길이와 방향은 세 대상에서 일치한다. `HOLD`/`OPEN` 행은 schema decode 가능 여부와 production route 활성 여부를 구분한다.

| UART Type | Payload | 방향 | P5 route / 검증 |
|---|---:|---|---|
| `0x01 M_CONTEXT` | 38 | ESP32→Domain private | P6의 세 독립 snapshot을 P5가 compose. `update_sequence` public-header binding 해결 전 production publish `HOLD`. BLE 전달 금지. |
| `0x02 M_DOMAIN_ALIVE` | 12 | Domain→ESP32 private | link state/DomainBoot 갱신과 timeout에만 사용. BLE Vehicle Data 전달 금지. |
| `0x10 M_REQUEST` | 24 | App→Domain | BLE 12B 앞에 verified current Device+Session 12B. 중복 Request/READY/등록/보안 검사 후 1회 enqueue. |
| `0x11 M_QUERY` | 32 | App→Domain | BLE 20B 앞에 current context. active Query key 생성; 초기 read-only recovery와 차량 Request gate를 분리. |
| `0x12 M_WARNING_ACK` | 21 | App→Domain | BLE 9B 앞에 current context. warning type/occurrence/domain boot 일치 확인. |
| `0x20 M_RESULT` | 44 | Domain→App | down context와 enum 검증 후 BLE logical 0x20. request session은 과거 원 요청 session일 수 있음. confirmation 보존. |
| `0x21 M_WARNING` | 40 | Domain→App | occurrence/domain boot와 품질 검증. Query 응답과 unsolicited(`QueryID=0`) 분리. |
| `0x22 M_AVAILABILITY` | 62 | Domain→App | 원 quality/sequence/age 보존. 반복 relay로 age/sequence 재생성 금지. |
| `0x23 M_DIGITAL_STATUS` | 30 | Domain→App | 표시 상태 전달만. ESP32가 자동 Unlock 성공을 합성하지 않음. |
| `0x24 M_DIGITAL_RESULT` | 35 | Domain→App | Domain 결과를 그대로 투영. local submit/TX를 result로 변환 금지. |
| `0x25 M_USER_SETTINGS` | 36 | Domain→App | sentinel/presence 한계 보존. 부분 composite를 정상값으로 채우지 않음. |
| `0x30 M_BCM_DOOR_STATE` | 40 | Domain→App | source boot/sequence/age/quality 검증 후 relay. |
| `0x31 M_BCM_CLIMATE_STATE` | 44 | Domain→App | source provenance 및 범위 검증 후 relay. |
| `0x32 M_BCM_LIGHT_STATE` | 45 | Domain→App | source provenance 및 reserved 검증 후 relay. |
| `0x33 M_BCM_STATUS` | 42 | Domain→App | source provenance/quality/age 보존. |
| `0x34 M_CIS_ENVIRONMENT` | 60 | Domain→App | source provenance/quality/age 보존. |
| `0x35 M_CIS_OCCUPANT` | 45 | Domain→App | source provenance/quality/age 보존. |
| `0x36 M_CIS_REAR` | 39 | Domain→App | source provenance/quality/age 보존. |
| `0x37 M_CIS_STATUS` | 54 | Domain→App | source provenance/quality/age 보존. |
| `0x38 M_WINDOW_STATE` | 42 | Domain→App | schema-only. `[구현 보류 — 설계 유지]`; runtime route `HOLD`. |
| `0x39 M_WINDOW_FAULT` | 42 | Domain→App | schema-only. `[구현 보류 — 설계 유지]`; runtime route `HOLD`. |
| `0x3A M_VSS_STATUS` | 48 | Domain→App | source provenance/quality/age 보존. |
| `0x40 M_QUERY_END` | 24 | Domain→App/control | active key, status, scope, unique response count 검증 후 BLE 전달. 완료 의미는 `OPEN`. |

각 downlink의 첫 20B `DOWN_CONTEXT`는 이미 UART semantic payload 일부다. P5는 현재 context와 대조하되 이를 제거하거나 현재 context를 다시 붙이지 않고, 앱 `MessageCodec`이 기대하는 원 semantic payload를 BLE fragment에 넣는다.

## 8. 최신 계약 차이와 P5 결정 Gate

| 항목 | 확인된 상태 | P5-A 처리 |
|---|---|---|
| `M_CONTEXT` | 최신 Network/UART와 P2는 38B로 일치한다. | 38B 외 byte 추가 금지. 세 producer의 sequence/AGE를 보존한다. |
| Recent Result | Domain은 장치별 최대 8건을 보관하되 query 출력은 최대 5건이다. 최신 Network도 최대 5건이다. 모바일 설명 일부는 여전히 최대 8건이고 Kotlin은 `recentRequestLimit=5`, 별도 recovery follow-up 한도는 8이다. | 한 번의 recent Result 응답은 최대 5로 설계. 앱 문서 및 follow-up 상한 의미는 `OPEN`. |
| `M_QUERY_END.ITEM_COUNT` | 최신 Network는 같은 active key에 대해 실제 성공 전송한 고유 응답 수(End/unsolicited/duplicate 제외)를 제안한다. | 고유 valid receive count와 대조. status/전송완료/receipt 의미가 `BINDING_OPEN`이므로 최종 완료는 양단 합의 필요. |
| Query partial | Network의 status 2는 partial이다. 현재 Kotlin `QueryCoordinator`는 0/1/2를 모두 `statusOk`로 처리한다. | ESP32는 2를 complete로 승격하지 않는다. Android 수정/합의 전 P5 Query completion `OPEN`. |
| Query duplicate | Network count는 duplicate를 제외한다. Kotlin은 matching response callback마다 `received++`한다. | ESP32 tracker는 고유 응답을 세도록 제안. 앱의 dedup key와 목록 규칙 합의 필요. |
| `Gateway_UpdateBasis_t` | 최신 Network 설계에는 `update_sequence`가 있으나 실제 공개 `Gateway_Types.h`에는 없다. 문서도 `HOLD_BINDING`으로 표시한다. | 13 API 및 struct ABI를 변경하지 않는다. 승인된 header 전 production `M_CONTEXT` binding `HOLD`. |
| Generic relay subtype | `TYPE + P`는 Network private routing의 `PROPOSED` 내부 표현이다. | 별도 private codec 후보로만 취급. BLE frame/UART frame/public typedef에 혼입 금지. |
| BLE 계약 승인 | Android/P4는 고정 UUID, MTU 24/185, 8B fragmentation을 사용하지만 Network BLE 문서는 아직 이를 `OPEN`으로 둔다. | 동작 중인 P4 wire를 바꾸지 않는다. 팀 계약 채택 기록 요청. |
| BLE semantic reject | 모바일 문서는 `SESSION_NOT_READY`, `NOT_REGISTERED`, `QUERY_BUSY` ATT 오류를 기술한다. 현재 P4는 write를 event queue에 넣은 뒤 callback 밖에서 P5가 판정하므로 같은 ATT response에 비동기 결과를 넣을 수 없다. | P4 API/실행 경계 변경 또는 별도 application result 정책이 필요. P5가 임의 ACK/Result를 만들지 않음. |
| Timeout | Android Query timeout은 3000ms, BLE reassembly는 1000ms. Network Alive 250/750ms 및 일부 state freshness는 제안값이며 Request 원 전송 age는 wire에 없다. | 각 timer를 별개로 관리. 3초 초과를 wire 근거 없이 차량 실패로 만들지 않음. |
| Result 의미 | Network/Domain은 local submit, UART TX, Domain accepted, vehicle done 및 confirmation을 구분한다. | P5는 원 `RESULT`, `REASON`, `CONFIRMATION`, quality/age를 보존하고 성공을 합성하지 않는다. |
| Reconnect replay | 팀의 기존 ESP32 참고 header `docs/esp32/6_GatewayRouter/GatewayRouter.h` 설명에는 경로 복구 후 과거 Request 자동 replay가 있으나 최신 Network와 ESP32 안전 계약은 이를 금지한다. | P5는 물리 Request를 replay하지 않는다. 기존 참고 header는 team-owned이므로 이 PR에서 수정하지 않고 owner 정정을 요청한다. |

### 식별자 매핑

| ID | BLE / UART 위치 | 의미 |
|---|---|---|
| DeviceContextID | Gateway Status, UART context/down-context | 승인된 device context. peer 주소나 bonding만으로 만들지 않음. |
| SessionID | Gateway Status, uplink prefix, down-context | 현재 논리 session. request의 과거 session 필드와 current session을 구분. |
| RequestID | BLE/UART Request 및 Result | Android가 발급한 원 요청 ID. Domain command ID와 별개. |
| QueryID | BLE/UART Query 및 query-scoped downlink/End | Android가 발급. 0은 unsolicited 의미가 있는 downlink에서 active query count에 포함하지 않음. |
| DomainBootID | Alive/down-context/Gateway Status | Domain 부트 세대. 변경 시 active query와 readiness 무효화. |
| update_sequence | source state/context fields | source producer 갱신 순서. UART link sequence나 BLE transferId와 별개. |

## 9. Digital Key 제안과 UART 필드의 불일치 — 자동 제어 활성 보류

참조 `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md` §4,§7,§11.

- 제안 ESP32: 200ms RSSI, 최근5 중앙값, far <=-70 2초 / near >=-60 1초, stale/invalid 분리, `far_run_ms` 전달.
- **현 UART-M v2 `M_CONTEXT` 38B**: Device+Session12, registration+connection2, app state/quality/sequence/age9, proximity state/validity/reason/sequence/age9, reg/connection quality/sequence/age7 등으로 구성됨(정확한 바이트 표는 `20_UART_M.md`). `far_run_ms`, raw/filtered RSSI, 별도 표본/보고 sequence는 별도 Wire 필드로 존재하지 않는다.
- 기존 `Gateway_ProximityUpdate_t`와 `M_CONTEXT`가 표현하는 정보로 Domain의 **새 연속 FAR 2초 증명**이 가능한지는 결정되지 않았다. Domain의 `DigitalKeyManager`가 요구하는 의사 결정 근거를 wire에서 임의로 만들어서는 안 된다.
- 대응안은 (A) 명세 범위에서 기존 정보로 재무장 증거를 재설계하고 팀 승인, (B) UART-M v2의 양 끝 메시지 schema를 공식 변경·버전 관리, (C) 디지털키 자동 해제 기능을 테스트/SAFE_RESTRICTED로 유지 중 **합의 선택**. 임의 확장/예약코드 차용 금지.
- 보안 정책도 충돌: 앱 예시에서는 bonding 성공=등록으로 동작하는 반면 Digital Key 명세는 현장 승인된 별도 registration을 요구. Domain 자동 Unlock을 실제로 열기 전 반드시 공동 결정.

## 10. 구현 전 승인 체크

- [ ] 실제 S32K344 측 `UART-M VERSION=2/38B context/CRC16` 채택과 빌드된 코덱 버전 확인
- [ ] BLE v0.3 APP 실코드/GATT UUID/byte lengths/MTU min 24 채택
- [ ] `Gateway_RelayMessageView_t` subtype TYPE+P 단일 책임/owner 결정
- [ ] BLE 등록 승인/본딩 불일치 해소 및 재등록/해제 절차
- [ ] SessionId 발급·변경·Domain reset 시 `SessionReady` 절차 확정
- [ ] Query partial status, item count, timeout, BLE 0x40 전달 정합성 테스트
- [ ] Digital Key `far_run_ms` 문맥 간 원본 근거 전달 합의 (불합의 시 자동 Unlock 금지)
- [ ] 하드웨어 UART GPIO/전압/GND/배선/버전 확인
