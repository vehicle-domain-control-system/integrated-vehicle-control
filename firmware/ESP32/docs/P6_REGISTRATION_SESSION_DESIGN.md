# P6 Registration / Session Design

- 기준: ESP-IDF v5.5.5, BLE v0.3, UART-M v2
- 범위: P6-A 계약, P6-B1 순수 C 상태 관리자, P6-B2-1 NimBLE 관측, P6-B2-2 Registration Store 기반
- 상태 표기: `DECIDED_FOR_PROJECT`, `PROPOSED`, `OPEN`, `HOLD`

## 1. 안전 경계

BLE 연결, 암호화, 본딩, SMP `authenticated`, 등록, App 활성, Session 준비는 서로 다른 사실이다. 현재 개발·시연 정책은 사용자가 명시적으로 등록 절차를 시작한 뒤 처음 확인된 bonded+encrypted peer 하나만 등록 대상으로 허용한다(`DECIDED_FOR_PROJECT`). 이는 Production의 강한 기기 신원 인증이 아니며, Just Works 본딩만으로 등록이나 차량 권한을 자동 생성하지 않는다.

P6-B1은 순수 상태 관리자와 저장소/entropy/clock port만 제공한다. Gateway runtime 공급자 연결, 실제 UART 차량 Request, `M_CONTEXT` 송신, Digital Key 자동 Unlock은 모두 `HOLD`다. `authorization_verified`는 등록된 동일 peer, bonded+encrypted link, 새 연결 generation과 새 non-zero SessionID가 모두 확인된 격리된 P6 상태에서만 true가 된다. `P6_PRODUCTION_BINDING_ENABLED`는 0이다.

| 항목 | 상태 | 근거/범위 |
|---|---|---|
| BLE Advertising/연결/본딩/암호화 | IMPLEMENTED | 기존 P4/P5 유지, P6-B1에서 수정하지 않음 |
| 개발·시연용 최초 Bond 단일 기기 등록 | DECIDED_FOR_PROJECT | 명시적 registration pending과 mock-confirmed identity 필요 |
| 계정/로그인/사용자 권한 등급 | DECIDED_FOR_PROJECT | 이번 제품 프로파일에서 제외 |
| P6 pure FSM 및 mock ports | IMPLEMENTED | P6-B1 범위 |
| NimBLE identity/lifecycle 관측 adapter | IMPLEMENTED | P6-B2-1, read-only Bond Store 조회와 GatewayTask 단일 owner |
| NVS/CSPRNG/Reset coordinator 기반 | IMPLEMENTED_SAFE_RESTRICTED | 실제 Bond delete/승인 입력/Production binding은 HOLD |
| Gateway `authorization_verified` runtime binding | HOLD | Provider API만 존재하며 Production binding=0 |
| App State freshness timeout | OPEN | active는 보조 조건일 뿐 인증 근거가 아님 |
| RSSI/FAR/자동 Unlock | HOLD | 임계값·freshness·wire evidence가 OPEN |

## 2. 식별자와 수명

| 값 | 폭 | 생성 주체 | 수명/규칙 |
|---|---:|---|---|
| DeviceContextID | u32 | ESP32 P6 | 0 금지. 등록과 함께 저장. reset 후 현재 실행 중에는 직전 값 재사용 금지. 영구 non-reuse 정책은 `OPEN`. |
| SessionID | u64 | ESP32 P6 | 0 금지. 현재 boot의 retired cache와 중복 금지. cache 16개가 차면 덮어쓰지 않고 발급 중단. 재부팅 간 CSPRNG/epoch 정책은 `OPEN`. |
| connection generation | u32 | BLE adapter | 0 금지. Connect lifecycle만 새 값을 등록하며 현재 boot에서 serial-number 순서상 strictly newer여야 한다. Disconnect 후 같은/과거 값 재사용 금지. 재부팅 간 연속성은 `OPEN`. |
| AppInstanceID | u32 | Android | 0 금지. 동일 연결에서 변경되면 Session 회전 필요. |
| DomainBootID | u32 | Domain | 0은 unknown. 변경 또는 link-down이면 Session을 폐기한다. |

식별자는 로그에 출력하지 않는다. 저장 레코드는 schema version, 등록 상태, BLE identity와 DeviceContextID만 포함하며 SessionID는 저장하지 않는다.

## 3. 상태 머신

| 상태 | 진입 근거 | 허용 전이 | 실패 처리 |
|---|---|---|---|
| UNREGISTERED | 저장 레코드 없음 | 명시적 시작 → REGISTRATION_PENDING | 자동 등록 금지 |
| REGISTRATION_PENDING | 사용자/상위 계층의 등록 시작 | allowed peer + bonded + encrypted + 저장 성공 → REGISTERED | 보안·저장·entropy 실패 시 권한 없음 |
| REGISTERED | ACTIVE 저장 레코드 복구/생성 | 동일 peer 보안 연결 → SESSION_PENDING | 다른 peer 거부 |
| SESSION_PENDING | 현재 generation과 등록 peer 확인 | non-zero 새 SessionID 생성 → AUTHORIZED | 생성 실패 시 권한 없음 |
| AUTHORIZED | 위 모든 근거 유효 | disconnect/timeout/회전 근거 → SESSION_EXPIRED; reset → RESET_REQUESTED | App active만으로 진입 불가 |
| SESSION_EXPIRED | Session 무효화 | 동일 등록 peer의 새 보안 연결/회전 → SESSION_PENDING | 물리 명령 replay 금지 |
| REVOKED | 저장 손상 등 fail-closed fault | 승인된 복구/reset만 | 차량 권한 없음 |
| RESET_REQUESTED | 호출자가 reset 요청, RAM 권한 즉시 폐기 | marker COMMITTED → accepted/erase; 미반영 → RESET_REJECTED | 성공 반환 아님 |
| RESET_REQUIRED | durable marker는 있으나 erase 완료 미확인 | erase retry 성공 → UNREGISTERED | 이전 ACTIVE 승인 금지 |
| RESET_REJECTED | marker NOT_COMMITTED/UNKNOWN | 명시적 reset 재시도 또는 reboot | 현재 boot 권한 없음; reboot 복구 한계 명시 |

Session timeout은 주입된 `uint64_t` monotonic millisecond clock으로만 계산한다. clock rollback/wrap은 정상 경과로 해석하지 않고 Session을 폐기한다. BLE disconnect, Domain link-down, DomainBootID 변경, AppInstanceID 변경도 fail-closed 회전 근거다.

최초 등록 흐름은 `UNREGISTERED → REGISTRATION_PENDING → Connect → Security → REGISTERED`이며, 실제 NimBLE가 확인한 identity를 Connect lifecycle이 공급하기 전에는 완료할 수 없다. Session 생성은 미리 등록된 현재 connection generation과 별도의 bonded+encrypted Security 관측이 모두 있어야 한다. 재연결은 저장 identity를 유지하되 strictly newer generation과 새 SessionID를 요구한다. 다른 peer는 거절하며, 스마트폰 교체는 성공한 개발자 reset 뒤에만 가능하다. Disconnect는 Session만 폐기하고 저장된 등록과 bond를 자동 삭제하지 않는다.

`authorization_verified`는 P6가 가진 현재 연결/등록/Session 증거가 일치한다는 뜻이다. Domain link와 Gateway `session_ready`, Request replay/cache, 차량 정책 허용 및 실제 실행 결과는 별도 P5/Domain gate다. 따라서 이 값 하나로 차량 제어를 허용할 수 없다.

## 4. 저장과 소유권

P6-B1 코어는 heap을 사용하지 않고 단일 owner가 변경한다. callback/producer는 고정 크기 event를 기존 Gateway queue에 deep-copy해야 하며 queue full이면 manager를 호출하지 않고 권한을 올리지 않는다. Snapshot은 값 복사이며 BLE identity나 secret을 노출하지 않는다. Owner는 Snapshot/Provider를 사용하기 전에 `p6_session_manager_tick()`을 호출해야 하며 Provider 자체는 시간을 진행시키지 않는다.

### Reset 접수 계약

Reset은 아래 의미를 별도로 관리한다.

| Reset 상태 | 의미 | API 처리 |
|---|---|---|
| `RESET_REQUESTED` | 호출자가 요청했고 RAM Session/Authorization은 즉시 폐기됨 | 아직 성공/접수로 반환하지 않음 |
| `RESET_ACCEPTED` | `RESET_PENDING` intent가 Store에서 `COMMITTED`됨 | 이전 ACTIVE를 승인하지 않고 erase 단계 진행 |
| `RESET_REQUIRED` | intent는 durable하지만 erase가 완료되지 않았거나 결과가 불명확함 | `P6_SESSION_RESET_REQUIRED`, reboot/retry 모두 fail-closed |
| `RESET_COMPLETE` | intent 기록 후 registration record erase까지 COMMITTED | `P6_SESSION_OK`, 새 등록 가능 |
| `RESET_REJECTED` | intent가 `NOT_COMMITTED` 또는 `UNKNOWN` | 성공 아님. 현재 boot 권한은 폐기하지만 reboot 복구 가능성 존재 |

Store mutation 결과는 `COMMITTED`, `NOT_COMMITTED`, `UNKNOWN`으로 구분한다. `UNKNOWN`은 성공으로 취급하지 않는다. marker가 COMMITTED이면 erase 실패/중단 후 재부팅에서 RESET_REQUIRED로 복구한다. marker가 NOT_COMMITTED이거나 UNKNOWN이고 실제로 기록되지 않았다면 과거 ACTIVE와 정상 미-reset 상태를 재부팅 후 구분할 정보가 없다. Reset 완료 후 구형 ACTIVE snapshot rollback 역시 현재 단일 레코드 계약으로 검출할 수 없다. 이 한계는 mock characterization test로 명시하며 Production binding은 계속 0이다.

P6-B2는 아래 중 하나의 별도 승인된 내구성 계약 없이는 reset을 활성화하지 않는다.

- transactional tombstone과 ACTIVE를 함께 원자적으로 관리하는 dual-slot journal
- 별도 monotonic registration/reset epoch와 rollback 방지 저장소
- Reset Gate가 COMMITTED된 뒤에만 bond/registration erase를 수행하는 두 단계 transaction

RAM fault flag나 `UNKNOWN`을 성공으로 바꾸는 mock은 재부팅 보장을 제공하지 않는다.

## 5. ABI 및 단계 분리

- BLE UUID, App Command 8B fragment, App State 5B, Gateway Status 21B는 변경하지 않는다.
- UART-M v2와 `M_CONTEXT` 38B, DeviceContextID/SessionID wire 폭을 변경하지 않는다.
- P6-B1: 순수 FSM, mock store/entropy/clock, Host test. 실제 BLE/Gateway/UART adapter 없음.
- P6-B2: 승인된 NVS/CSPRNG과 BLE identity adapter, reset 복구 계약.
- P6-C: Gateway authorization provider 바인딩과 `M_CONTEXT` publisher. 양단 계약 전 HOLD.
- P6-D: proximity/FAR 및 Digital Key. 신선도·보안 증거 wire 계약 전 HOLD.

### P6-B2-1 NimBLE 관측 경계

- `ble_gap_conn_find()`의 `peer_id_addr`를 안정 Identity 후보로 사용하고 `peer_ota_addr`는 현재 연결의 OTA 주소로만 취급한다. `peer_id_addr` 자체가 RPA이면 Identity 미해결로 거절한다.
- `ble_store_util_bonded_peers()`의 read-only 결과와 정확히 일치할 때만 `bond_store_present=true`다. Bond key, IRK, 전체 peer 주소는 로그하지 않는다.
- BLE GATT Server만 connection generation을 증가시킨다. Callback은 identity/security snapshot을 기존 고정 크기 queue에 복사하고, GatewayTask만 P6 lifecycle/manager를 변경한다.
- 연결 시 안정 Identity와 Bond Store evidence가 있으면 `p6_session_manager_connect()`를 호출한다. Security event에서 같은 generation/Identity, encrypted, bonded를 다시 확인한 뒤 `observe_security()`와 `start_session()`을 호출한다.
- P6-B2-1 Firmware의 Registration Store는 항상 EMPTY이고 mutation은 NOT_COMMITTED다. 따라서 기존 NimBLE Bond가 있어도 P6 등록/Session Authorization은 생성되지 않는다. 이 observer 결과는 Gateway routing에 공급되지 않는다.
- SMP `authenticated`는 MITM 사실로 별도 보존하며 P6 Application Authorization으로 해석하지 않는다. App State active만으로 권한이 생성되지 않는다.
- Disconnect, identity mismatch, lookup error 및 stale generation은 fail-closed다. Bond 삭제, NVS erase/migration, 자동 peer 교체는 호출하지 않는다.
- BLE Server는 event queue와 독립된 32-bit `connection_epoch + generation + handle + connected` snapshot을 기존 critical section 아래 게시한다. Disconnect/새 Connect가 BLE Server queue 또는 Gateway queue에서 유실되어도 GatewayTask는 매 event와 tick 전에 snapshot을 대조하여 이전 P6 link를 폐기한다. 64-bit atomic을 가정하지 않으며 epoch/generation은 0을 건너뛰고 equality pair로만 사용한다.
- Authorization admission은 manager의 과거 boolean만 사용하지 않고 최신 BLE snapshot, 현재 encrypted+bonded link, P6가 승인한 epoch/generation/handle의 일치를 다시 확인한다. snapshot unavailable, link security 상실 또는 불일치는 즉시 false이며, Production binding은 계속 0이다.
- `ble_store_util_bonded_peers()`는 NimBLE host가 제공하는 Store 순회 API이고 현재 최대 bond 3건으로 bounded되어 있어 GAP callback의 identity snapshot 시점에 유지한다. 별도 task로 이동할 경우 NimBLE Store thread-safety 보장이 확인되지 않았으므로 임의 이동하지 않는다. 보드에서는 callback 지연과 Queue overflow를 측정한다.

비파괴 보드 확인은 기존 Firmware를 Flash한 뒤 기존 bonded Android 기기로 재연결하여 `BLE_CONNECTED`, `SECURITY_VERIFIED`, `P6_REGISTRATION_NOT_AVAILABLE`, `SESSION_NOT_AUTHORIZED`, `BLE_DISCONNECTED` 계열 사실 로그만 관찰한다. Flash 전 NVS erase를 실행하지 않고, repeat pairing에서도 저장 Bond를 자동 삭제하지 않는다. 이 절차는 사용자가 수행하기 전까지 `NOT_TESTED_ON_BOARD`다.

### P6-B2-2 Persistent Registration Store / Reset 경계

- ESP32 소유 NVS namespace는 `p6_reg`, key는 `record` 하나다. NimBLE Bond namespace나 partition table을 변경하지 않는다.
- 값은 C struct dump가 아닌 고정 28B blob이다: `P6R2` magic(4), schema u32 LE(4), state(1), identity type(1), resolved identity(6), DeviceContextID u32 LE(4), Registration Epoch u32 LE(4), 앞 24B의 IEEE CRC-32 u32 LE(4).
- ACTIVE는 valid identity, non-zero DeviceContextID와 non-zero Registration Epoch를 모두 요구한다. RESET_PENDING은 durable tombstone이며 Session/Authorization을 복구하지 않는다.
- `nvs_set_blob`/`nvs_erase_key` 뒤 `nvs_commit`이 ESP_OK인 경우만 COMMITTED다. write/erase/commit 오류는 COMMIT_UNKNOWN으로 현재 boot에서 fail-closed하며 전체 NVS erase로 복구하지 않는다. Registration ACTIVE save의 UNKNOWN이 실제 flash에 반영됐다면 재부팅 load에서 ACTIVE가 보일 수 있다. 이 경우에도 Session/Authorization은 복구하지 않고 exact Bond reconciliation을 다시 요구하지만, UNKNOWN의 실제 반영 여부를 NVS 단일 record만으로 증명할 수 없는 한계는 Production blocker다.
- runtime은 Store와 ESP-IDF CSPRNG을 P6 owner에 주입하지만 Android Command, Bond 존재, App State `active=1` 중 어느 것도 `begin_registration()`을 호출하지 않는다. 최초 등록 승인 진입점은 아직 runtime에 없다.
- NimBLE host sync 뒤 bounded Bond Store snapshot을 GatewayTask에 deep-copy한다. ACTIVE Registration identity와 exact match가 확인되기 전에는 Connect/Session 후보를 거절한다. 조회 실패·overflow·Bond 누락은 Registration/NimBLE 데이터를 자동 삭제하지 않고 fail-closed REVOKED 상태로 남긴다.
- Reset production 순서는 `request_reset()`의 RESET_PENDING commit → exact identity Bond delete port → `complete_reset()`의 Registration erase다. delete 실패/불명은 marker를 보존하고 reboot 시 RESET_REQUIRED로 재개한다.
- Reset marker는 valid ACTIVE Registration, valid identity, non-zero DeviceContextID/Epoch가 있을 때만 생성한다. EMPTY/CORRUPT/RESET_REQUIRED는 거부하며 NimBLE-only orphan cleanup은 별도 승인 기능이다.
- `p6_bond_reset_coordinator`는 위 순서를 Mock으로 검증하지만 실제 NimBLE delete port와 개발자 CLI/physical-presence gate는 연결하지 않는다. `P6_DEVELOPER_BOND_RESET_ENABLED=0`이며 BLE로 Reset할 수 없다.
- NVS commit이 UNKNOWN이고 marker가 실제로 미반영된 경우, NVS 단독으로 old ACTIVE와 정상 미-reset 상태를 구분할 수 없다. 현재 boot는 revoke되지만 강한 reboot anti-rollback은 독립 durable monotonic/revocation source가 필요해 OPEN이다.
- Durable revocation 후보는 (A) 별도 이중 슬롯 revocation journal+monotonic generation, (B) flash-encryption/secure-boot와 결합한 authenticated monotonic record, (C) eFuse/보안 소자의 write-once/monotonic anchor다. 수명·wear·복구 절차와 실제 보드 지원을 검증하기 전에는 어떤 안도 Production 계약으로 채택하지 않는다.
- 부팅은 Registration record만 복구하고 Session/Authorization은 복구하지 않는다. 이후 P6-B2-1이 현재 generation/epoch의 resolved identity, exact Bond Store presence, encrypted+bonded link를 다시 확인해야 Session 후보가 된다.
- `P6_PRODUCTION_BINDING_ENABLED=0`, Production UART Request/M_CONTEXT TX, Domain SessionReady, 자동 Unlock은 계속 HOLD다.

### 개발 전용 실제 NVS 영속성 검증

- `CONFIG_P6_NVS_DEV_TEST`는 기본값 `n`이며 활성화된 별도 개발 빌드에서만 테스트 소스를 컴파일한다. Production build와 기존 `sdkconfig.p6b2_2`에는 자동 적용하지 않는다.
- 테스트는 production `p6_reg` 및 NimBLE Bond Store와 분리된 `p6_nvstest` namespace만 사용한다. 권한 후보가 될 수 있는 ACTIVE Registration을 만들지 않고 테스트 전용 RESET_PENDING record를 저장한다.
- 첫 boot는 save+commit, 즉시 read, 같은 boot의 close/open read를 검증한다. 이 reopen 결과는 reboot 검증으로 보고하지 않는다. phase key commit 뒤 `esp_restart()`를 호출하며, 다음 `app_main()`에서 record를 복원해야 `REBOOT_RESTORE_PASS`다.
- reboot 복원 성공 뒤 test record와 phase key를 각각 commit하여 삭제하고 close/open 후 EMPTY를 재검증한다. NVS namespace의 내부 allocation metadata는 남을 수 있지만 test key는 남기지 않는다.
- Test build는 일반 boot마다 위 2-boot sequence를 다시 시작할 수 있으므로 검증 후 기본 build로 복귀한다. 실패 시 Gateway/BLE/UART 시작 전에 fail-closed로 중단한다.
- 이 기능은 Registration 승인 입력, Session/Authorization, Bond delete, Production UART/M_CONTEXT TX, 자동 Unlock을 연결하지 않는다. 실제 보드 결과는 Serial log를 확인하기 전까지 `NOT_TESTED_ON_BOARD`다.

### P6-B3 개발 전용 Registration 승인

- `CONFIG_P6_DEV_REGISTRATION_APPROVAL`은 기본값 `n`이고 `CONFIG_P6_NVS_DEV_TEST`와 동시에 활성화할 수 없다. 활성화된 빌드만 UART0 입력 task와 Gateway의 개발 승인 이벤트 API를 포함한다.
- UART0에서 `p6-reg-request`와 `p6-reg-approve`를 별도 줄로 입력한다. 첫 명령은 현재 connection handle/generation/epoch, resolved Bond identity, encrypted+bonded 상태와 GatewayTask monotonic request time을 1회성 pending snapshot으로 고정할 뿐 Registration을 쓰지 않는다. 두 번째 명령은 같은 snapshot을 다시 검증한 뒤에만 기존 `begin_registration`과 `register_confirmed_bond`를 호출한다.
- Request TTL은 30초이며 정확히 30초인 경계부터 거부한다. request/approval/처리 시각의 역행, 64-bit 큰 시간 점프, stale event도 pending을 폐기한다. Disconnect, connection generation/epoch 변화, peer identity 또는 security fact 변화, 중복 승인, queue full/input loss의 기존 무효화 규칙도 유지한다.
- NVS save가 `COMMITTED`가 아니면 현재 boot에서 ACTIVE Registration과 REGISTERED 전이를 만들지 않는다. COMMIT_UNKNOWN이 실제 반영된 경우 재부팅 후 ACTIVE가 보일 수 있다는 위 P6-B2-2 한계는 Host characterization으로 유지하며 transactional admission marker는 이번 단계에서 구현하지 않는다.
- UART0 접근은 물리적 개발 포트 접근에 의존하며 암호학적으로 인증된 Production 승인 주체가 아니다. Android BLE/App State/Bond 완료에서 이 경로를 호출하지 않으며 기본 build에는 입력 API가 없다.
- ESP-IDF 기본 UART VFS는 RX FIFO에 현재 도착한 바이트만 반환하므로 Console은 `fgets()`의 반환을 한 줄로 가정하지 않는다. bounded byte parser가 CR, LF, CRLF까지 누적한 뒤에만 명령을 전달하며 48B를 넘는 줄은 terminator까지 전부 폐기한 후 다음 줄에서 복구한다.
- 개발 승인 build는 기존 Store/28B Codec을 그대로 사용하되 격리 namespace `p6_regdev`에 ACTIVE를 저장한다. 기본 build는 계속 production `p6_reg`만 열므로 개발 시험 뒤 기본 Firmware로 돌아오면 `p6_regdev` record를 권한 근거로 읽지 않는다. 기존 NimBLE Bond와 production Registration은 변경하지 않는다.
- `p6_regdev` ACTIVE 자체는 NVS에 남으며 현재 P6-B3에는 이를 삭제하는 runtime 입력이 없다. 재시험용 삭제를 임의 key erase로 추가하지 않는다. 별도 개발 보드/백업 NVS 또는 향후 승인된 registration-only transaction을 사용하고, production Bond Reset의 marker→exact Bond delete→registration erase 순서를 우회하지 않는다.
- Registration 성공은 Production Authorization이 아니다. `P6_PRODUCTION_BINDING_ENABLED=0`, UART Request/M_CONTEXT TX, Query completion, 자동 Unlock, Bond delete는 계속 HOLD다.
- 2026-11-14 개발 보드에서 UART0 request/approve, 30초 TTL 만료와 TTL 내 승인, `p6_regdev` ACTIVE 저장, 재부팅 후 기존 Bond exact reconciliation을 확인했다. 이후 기본 Firmware 복원에서도 기존 BLE Bond와 Safe Integration Mode가 유지되고 기본 `p6_reg`가 개발 namespace를 권한 근거로 사용하지 않음을 확인했다. 이 결과는 Production 승인 주체, COMMIT_UNKNOWN 내구성, Domain SessionReady, 차량 UART TX, `M_CONTEXT` TX 또는 자동 Unlock 검증이 아니다.

Network 담당자와의 현재 통합 방향은 UART-M v2, `M_DOMAIN_ALIVE=0x02`, payload 12B/frame 22B, 115200 8N1, Alive 250ms, supervision 750ms다. P6-B1은 이 값을 문서 확인에만 사용하며 UART codec/parser/timeout을 변경하지 않는다. DomainBootID는 외부 입력으로만 관측하고 ESP32가 생성하지 않으며, Alive 수신만으로 authorization 또는 session ready를 올리지 않는다. Network RX → Domain 처리 → Network TX superloop의 구체 API는 `OPEN`이다.

ESP32 UART2 후보는 GPIO27 TX/GPIO26 RX, 3.3V이며 실제 S32K344 UART-M 핀은 미확정이다. 공유 후보 중 LPUART11은 5V I/O 가능성이 있어 직접 결선 금지, LPUART0/1도 mux·connector·전압 확인 전 `HOLD`다. P6-B1은 핀, UART driver, 배선을 변경하지 않는다.

## 6. OPEN 계약

1. Production 최초 등록 승인 UX와 강한 peer identity/credential.
2. BLE bond 삭제와 P6 registration reset의 양방향 원자성.
3. NVS schema migration/암호화와 commit-UNKNOWN rollback을 막는 독립 durable monotonic/revocation source.
4. Production DeviceContextID/SessionID CSPRNG, boot epoch와 재부팅 간 non-reuse/replay window.
5. Session timeout, reconnect 재발급, AppInstance 변경 정책의 Android 합의.
6. DomainBoot 변화와 Session 재승인의 Network/Domain 합의.
7. `authorization_verified`를 Gateway 공개 13 API에 연결하는 binding.
8. Registration/Session 폐기 통지용 BLE 메시지 부족 여부.
9. RSSI/FAR freshness와 `M_CONTEXT` 38B 간 증거 부족.
10. 분실 기기 revoke와 다중 기기 지원 여부(현재 P6 등록 최대 1대, NimBLE Store의 기존 bond 수와 별개).
11. NimBLE `peer_id_addr`/Bond Store 관측과 향후 Registration record의 원자적 일치 정책.
12. 최초 등록을 승인할 실제 physical/developer workflow와 권한 주체.
13. 실제 NimBLE exact-peer delete port 및 개발자 전용 reset 진입 보호와 실기기 파괴 시험 승인.
