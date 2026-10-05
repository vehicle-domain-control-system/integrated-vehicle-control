# ESP32 Wireless Gateway — 팀 회의 상세 협의사항 v0.1

> **프로젝트:** 통합 차량 제어 시스템 (MOBILE ↔ ESP32 ↔ S32K344 Domain)  
> **문서 성격:** Git 공유용 **회의 준비 / Open Issue / 결정 기록 문서**  
> **상태:** `REVIEW` — 이 문서의 질문·선택지는 확정 사양이 아님  
> **작성 기준:** SR v0.46 (`SR(8).md`), SysRS v0.47 (`sysRS(7).md`), `ESP32 Wireless Gateway Software Architecture v0.3`, `Domain Software Architecture v0.2`, 현재 Gateway 구현/테스트 자료  
> **권장 Git 위치:** `docs/ESP32/ESP32_팀회의_상세협의사항_v0.1.md`  
> **회의 참여:** MOBILE / ESP32 / S32K344 Domain / 네트워크(통신) 담당. WINDOW 담당은 상태 연계·범위 확인 안건에 참여.

---

## 0. 이 문서의 사용 방법

이 문서는 ESP32 소프트웨어 구조를 다시 설계하는 문서가 아니라, **이미 정리한 논리 인터페이스를 실제 ECU 연동 규격으로 구체화하기 위해 팀원에게 확인할 내용을 모은 문서**이다.

| 상태 | 의미 |
|---|---|
| **기준** | 현행 SR/SysRS/아키텍처에 기재된 요구 또는 현재 논리 책임 |
| **제안** | 현재 구현 또는 회의에서 검토할 선택지. 팀 확정 사항이 아님 |
| **TBD** | 팀 결정 또는 하드웨어/실측 확인이 필요한 항목 |
| **합의** | 회의에서 담당자·결정 사항·근거까지 작성된 항목 |

**회의 원칙**

1. **기능 의미 / ECU 책임 → Logical Signal → 메시지 계약 → 물리 프로토콜 → 타이밍 / 검증** 순서로 결정한다.
2. 값이 미정이면 `TBD`로 유지한다. 테스트용 RSSI/Timeout 값을 실제 사양으로 복사하지 않는다.
3. 일반 차량 제어의 **최종 허용·실행·Result 생성은 Domain**, 실제 ECU 보호는 각 로컬 ECU 책임이다.
4. **Bluetooth 연결 ≠ 앱 활성**, **등록됨 ≠ 현재 연결**, **UART 초기화 ≠ Domain 통신 확인**을 유지한다.
5. **Link 끊김 ≠ 차량 FAILED**, **RSSI 누락·연결 상실·STALE ≠ 실제 FAR**, **재연결 ≠ 과거 요청 재실행**을 유지한다.
6. 회의에서 나온 합의가 현행 SR/SysRS에 영향을 주면 코드를 먼저 바꾸지 않고 요구사항 변경 여부를 먼저 확인한다.

---

## 1. 회의 전 공유할 현재 구현 범위

| 항목 | 현재 준비 상태 | 확인되지 않은 부분 |
|---|---|---|
| 공통 자료형 / 시간 / 품질 | `Gateway_Types`, `Gateway_Time`, `STALE` 반영 | 실제 Wire 코드/원본·수신 시각 표현 |
| 논리 경계 | `Gateway_Interface`, `GatewayRouter` | 실제 Bluetooth/UART 바이트 변환 |
| Link / Session | `LinkStateManager`, `MessageContextManager` | 실제 Link 확인 근거, ID 폭·생성 주체·수명 |
| 기동 / 복구 | `GatewayLifecycleManager` | 재동기화 프로토콜, 완료 조건, 주기 |
| 등록 / 현재 연결 | `DeviceRegistrationManager` | BLE 등록 절차, 실제 peer reference, 다중 단말 지원 여부 |
| 근접 | `ProximityManager`, `Gateway_PolicyConfig` | RSSI 실측 기준·안정화·만료·품질 확정 |
| 앱 활성 정보 | `AppActivityIngress` + 입력 계약 초안 | ACTIVE Producer, 앱 OS 이벤트, 주기, TTL, 보안 |
| 테스트 | Host C11 단위/Mock 테스트 기반 | 실보드 ESP-IDF 빌드, 앱 연결, UART Loopback, Domain 연동 |

> **중요:** 현재 모듈들은 실제 ESP32↔S32K344 하드웨어를 통과한 완성품이 아니다. 통신 상세와 보드 검증이 남아 있다.

**관련 기존 문서:** `ESP32_Wireless_Gateway_Software_Architecture_v0.3.md`, `docs/App_Activity_입력계약_v0.1.md`, `docs/App_Activity_담당자_결정요청_v0.1.md`, `docs/GatewayRouter_설명서_v0.3.md`, `docs/GatewayLifecycleManager_설명서_v0.2.md`.

---

## 2. 우선순위별 회의 결정 목록

**P0 = 실제 구현·연동 전에 최소한 합의할 인터페이스** / **P1 = 장비 시험·기능 시연 전에 확정** / **P2 = 후속 상세화 가능**.

| ID | 우선 | 논의 주제 | 주 협의 담당 | 필요한 산출물 | 상태 |
|---|---|---|---|---|---|
| ESP-01 | P0 | Bluetooth 방식 / 서비스·메시지 경계 | MOBILE + 통신 + ESP32 | BT logical/physical interface 합의 | TBD |
| ESP-02 | P0 | 등록 / 현재 연결 / DeviceContextId | MOBILE + ESP32 + Domain | 등록·연결 상태전이, ID 의미 | TBD |
| ESP-03 | P0 | VehicleId / SessionId / RequestId 및 원본 식별 보존 | MOBILE + Domain + 통신 + ESP32 | ID ownership / lifecycle 표 | TBD |
| ESP-04 | P0 | ESP32↔Domain UART 포트·프레임·검증 | 통신 + ESP32 + Domain | UART protocol 초안 | TBD |
| ESP-05 | P0 | 송수신 메시지 종류와 논리 Field | 4개 파트 공동 | ESP32↔Domain Interface Matrix | TBD |
| ESP-06 | P0 | 링크 상실 / Restart / Re-sync 기본 동작 | ESP32 + Domain + 통신 | 복구 시퀀스, stale frame 폐기 규칙 | TBD |
| ESP-07 | P1 | RSSI NEAR/FAR/UNKNOWN 기준 | ESP32 + Domain + MOBILE | 시험 계획 / calibration 후보 | TBD |
| ESP-08 | P1 | App Activity 입력 Producer / 유효 시간 | MOBILE + ESP32 + 통신 + Domain | ACTIVE/INACTIVE/UNKNOWN 계약 | TBD |
| ESP-09 | P1 | 주기 / Timeout / 원본 Age / 수신 Age | 네트워크 + ESP32 + Domain | 신호별 freshness 표 | TBD |
| ESP-10 | P1 | 등록 해제·재등록·복수 단말 지원 범위 | MOBILE + ESP32 | 등록/해제 상태전이 | TBD |
| ESP-11 | P1 | 결과 / 경고 / 상태의 모바일 표시 의미 | MOBILE + Domain + ESP32 | UI 상태 매핑 / 오류 구분 | TBD |
| ESP-12 | P1 | 실제 하드웨어 UART 전기 연결·핀·보드 시험 | ESP32 + Domain + HW/통신 | 배선도, Loopback 시험 | TBD |
| ESP-13 | P2 | Gateway FreeRTOS Task/Queue 세부 수치 | ESP32 + 통신 | Task/Queue 실행 설계 | TBD |
| ESP-14 | P2 | WINDOW 상태의 모바일 표출 범위 | WINDOW + Domain + MOBILE + ESP32 | 상태 전달 범위/표시 합의 | TBD |
| ESP-15 | P2 | 로그/진단·시험 데이터/시연 구분 | 전원 | Debug/Test plan | TBD |

> **권장:** 첫 회의에서 P0 전체의 세부 값을 억지로 확정할 필요는 없다. 적어도 **누가 결정하고 언제 초안을 낼지**까지 정하면 다음 단계 구현을 진행할 수 있다.

---

## 3. ESP-01 — MOBILE ↔ ESP32 Bluetooth 인터페이스

**현재 기준:** 무선 경로는 Bluetooth로 지정되어 있다. 세부 BLE/Classic 선택, Profile, GATT 구성, 메시지 형식은 미정이다. 기존 Architecture에서 `BLE / Classic`, `GATT UUID`, `MTU`, `Notification/Indication`을 Open Item으로 남겨 두었다.

**논의 질문**

- [ ] 실제 사용할 Bluetooth 형태는 **BLE GATT / Classic / 기타 지원 방식** 중 무엇인가? ESP32 보드/휴대폰 OS의 지원 여부를 누가 확인하는가?
- [ ] 차량이 Peripheral/Server, MOBILE이 Central/Client가 되는가? 연결 개시/재연결 주체는?
- [ ] 논리 채널을 제어 요청, 상태·결과, 경고, 등록/연결, Activity 등으로 나눌 것인가? 실제 Characteristic 개수와 논리 메시지 분류는 별개로 볼 것인가?
- [ ] 모바일→ESP32: Write with/without response 사용 범위는? ESP32→모바일: Notify / Indicate 중 어느 전달 보장이 필요한가?
- [ ] 실제 메시지는 JSON / Binary / TLV 등 어느 표현을 사용하는가? Payload 최대 크기, MTU, 분할·조립 책임은?
- [ ] 연결 성공, 애플리케이션 세션 확인, 차량 통신 준비 상태를 MOBILE UI에서 서로 구분할 수 있는가?
- [ ] 메시지 출처·재생 방지, Pairing/Bonding 및 링크 암호화의 실제 지원 방식은?

**잠정 구조 예시(확정 아님)**

```text
MOBILE App
  → BluetoothAdapter / BluetoothProfile
  → 유효 Message 변환
  → Gateway_Interface
  → GatewayRouter
  → UartAdapter
```

**결정 후 반영:** `BluetoothProfile.h/.c`, `BluetoothAdapter.h/.c`, `Network_Config`, MOBILE 송수신 사양서.

**회의 기록:** 선택 방식 `TBD` / 프로토콜 담당 `TBD` / 지원 보드·OS 검증 `TBD`.

---

## 4. ESP-02, ESP-03, ESP-10 — 등록·연결·식별·세션

**현재 기준:** SysRS §2.1.1은 Bluetooth 계층의 기기 등록을 사용하고 별도의 앱 계정/사용자별 권한 시스템을 요구하지 않는다. 현재 연결된 상대 기기와 등록된 기기가 모두 확인돼야 현재 등록 연결로 취급한다. 기존 `DeviceRegistrationManager`는 **단일 등록 peer reference**를 관리하는 구현 초안이다.

### 4.1 등록·현재 연결

- [ ] 등록 절차의 시작·승인·성공·실패를 어떤 이벤트로 표현하는가? 사용자 OS 승인 과정은?
- [ ] `DeviceContextId`는 누가 생성·저장하는가? Bluetooth 내부 비밀키를 노출하지 않고 peer를 비교할 수 있는 **논리 참조**는 무엇인가?
- [ ] 현재 연결 peer A와 등록 record A가 일치한다는 근거를 어떤 계층에서 검증하는가? 단순 기기 이름/주소 비교로 처리하지 않는가?
- [ ] Bluetooth bonding record는 어디에 영속 보관되는가? ESP32 재부팅 시 어떻게 복원·재검증하는가?
- [ ] 시연에서 **등록 단말 1개만 지원**할 것인가? 복수 등록·단말 교체·동시 연결은 범위에 포함되는가?
- [ ] 등록 해제 후 앱/ESP32 양쪽 기록을 어떻게 각각 지우고, 최종 해제 완료를 언제 표시하는가?
- [ ] 등록 해제 직후 Bluetooth가 여전히 물리 연결 상태인 짧은 구간에 새 요청·디지털 키 응답을 어떻게 중단하는가?

### 4.2 Vehicle / Device / Session / Request Identity

| 식별자 | 현재 의미 | 반드시 결정할 항목 | 기본 논의 주체 |
|---|---|---|---|
| `VehicleId` | 대상 차량의 논리 식별 | 고정 ID 여부, 생성/전달 필요성, 표현·길이 | Domain + MOBILE |
| `DeviceContextId` | 현재 등록·연결 peer 참조 | 생성 주체, 재등록 시 변화, BT 내부 키와 분리 | ESP32 + MOBILE |
| `SessionId` | 현재 논리 연결·세션 | 생성 주체, 로그인 없는 세션 표현, reconnect 후 유지/변경, 수명 | Domain + MOBILE + ESP32 |
| `RequestId` | 동일 사용자 요청의 식별 | **MOBILE 원 요청 발급/유지**, 고유성 범위, 재전송 규칙 | MOBILE + Domain |
| `Generation/Sequence` | 지연·역순·재시작 구분 후보 | 실제 Wire 필드 채택 여부, 재시작 시 처리 | 네트워크 + Domain + ESP32 |

- [ ] `RequestId`는 앱 생성 후 요청/중계/응답 동안 동일 값을 유지하는가? 동일 값이 다른 Session에서 등장하면 어떻게 구분하는가?
- [ ] 재연결 전후 같은 `SessionId`를 재사용할 수 있는가? 구별을 위한 실행 구간 식별은 필요한가?
- [ ] Context가 없는 사전 등록/초기 상태 조회 메시지를 허용할 경우, 현재 `Current Session` 중심 GatewayRouter 규칙을 어떤 메시지에 한해 예외 처리하는가?
- [ ] 식별 필드가 누락·유효하지 않을 때 **전송 계층 오류**와 **차량 측 Request 거부**를 어떻게 분리하는가?

**결정 후 반영:** `DeviceRegistrationManager`, `MessageContextManager`, `GatewayRouter`, 양방향 인터페이스 DTO, 등록 시퀀스.

**회의 기록:** 단말 수 `TBD` / Session 생성 주체 `TBD` / ID 필드 표현 `TBD` / 등록 해제 주체 `TBD`.

---

## 5. ESP-04 — ESP32 ↔ S32K344 UART 프로토콜

**현재 기준:** ESP32↔Domain **UART 경로는 지정**됐지만 실제 포트, Baud Rate, 메시지 Framing, Integrity, Timeout은 `NETWORK-TBD`이다. `uart_driver_install()` 성공만으로 Domain과 통신 가능한 상태라고 판단하지 않는다.

| 항목 | 회의에서 정할 사항 | 결정값 |
|---|---|---|
| 물리 인터페이스 | ESP32 UART 번호 / S32K344 LPUART 번호 / GPIO Pinmux / TX·RX·GND / 실제 보드 커넥터 | TBD |
| 전압 레벨 | **보드 입력전원**과 별도로 양측 UART GPIO HIGH/LOW 레벨 확인, 필요 시 Level Shift | TBD |
| UART 설정 | Baud / Data bits / Parity / Stop bits / Flow control | TBD |
| Frame 경계 | Start pattern / Length / Delimiter / Escape / 재동기화 방법 | TBD |
| 최대 메시지 | Header + Payload + Integrity 전체 길이, Buffer 한도, 초과 처리 | TBD |
| 논리 메시지 구분 | `MessageType` 필드 및 실제 ID 값 할당 | TBD |
| 유효성 | Length 범위, 허용 Type, CRC/Checksum 등 채택할 Integrity 검증 | TBD |
| 순서·중복 | Sequence / RequestId / Session 또는 Restart epoch 사용 여부 | TBD |
| ACK / 오류 | Byte/Frame 전송 ACK와 Domain의 차량 `ACCEPTED/DONE/...` 분리 | TBD |
| Link 유효 근거 | Handshake, Heartbeat 또는 필수 주기 메시지 중 무엇을 사용? | TBD |
| Timeout·복구 | 수신 기한, Link Loss, Buffer Flush, 상태 재조회, 정상 복귀 조건 | TBD |

**실제로 합의할 예외 사례**

- [ ] Frame 길이가 잘못되거나 Integrity 실패 시 버퍼에서 어디까지 버리고 다음 메시지 경계를 찾는가?
- [ ] 잘린 Frame, 연속 Frame, 잘못된 길이, 지원하지 않는 Type, 다수 요청 동시 발생을 어떻게 처리하는가?
- [ ] UART 송신 함수가 성공했더라도 Domain 수용 여부가 불명확한 상황에서 앱에 어떤 **전달 상태**를 표시하는가?
- [ ] Domain 또는 ESP32가 재시작했을 때 이전 RX 버퍼·부분 Frame·지연 결과를 어떻게 구분·폐기하는가?
- [ ] UART 링크 감시용 주기 메시지가 꼭 필요한가? 기존 필수 주기 메시지가 있다면 감시 근거로 재사용 가능한가?

**주의:** 위의 `CRC`, `Sequence`, `Heartbeat`는 **후보**다. 특정 방식이 이미 확정된 것처럼 기록하지 않는다.

**결정 후 반영:** `Network_Config.h`, `UartFrameCodec.h/.c`, `UartAdapter.h/.c`, S32K344 UART counterpart, 시험용 정상/비정상 Frame 예제.

---

## 6. ESP-05 — ESP32 ↔ Domain Logical Interface Matrix 확정

**현재 기준:** 정보 방향과 의미는 정의되어 있지만, 모든 논리 이름을 별도의 물리 Message ID로 구성한다는 결정은 아직 없다.

| 방향 | 논리 정보 | Producer | Consumer / 처리 주체 | 반드시 합의할 상세 필드 | 상태 |
|---|---|---|---|---|---|
| ESP32 → Domain | Registration / Connection | ESP32 BT 계층 | GatewaySessionManager 등 | DeviceContext, 등록·연결 상태, 품질, 생성 근거 | TBD |
| ESP32 → Domain | App Activity | MOBILE 또는 BT 확인 경로 → ESP32 | GatewaySessionManager / DigitalKeyManager | ACTIVE/INACTIVE/UNKNOWN, session, quality, age | TBD |
| ESP32 → Domain | Session Context | 확인된 연결/세션 경로 | GatewaySessionManager | vehicle/device/session, 재시작 구분 | TBD |
| ESP32 → Domain | Proximity | ESP32 | DigitalKeyManager / VehicleStateManager | NEAR/FAR/UNKNOWN, quality, 원본 age | TBD |
| ESP32 → Domain | MOBILE Request | MOBILE → ESP32 중계 | GatewaySessionManager → RequestManager | target, action, value, RequestId, SessionId | TBD |
| ESP32 → Domain | State Query | MOBILE → ESP32 중계 | RequestManager / MobileStateMapper | 조회 대상, 요청 식별, 반환 범위 | TBD |
| ESP32 → Domain | Warning Ack | MOBILE → ESP32 중계 | WarningManager | warning occurrence ID, ack 시각·의미 | TBD |
| Domain → ESP32 | Request Result | RequestManager / MobileStateMapper | MOBILE | RequestId, 단계/최종 결과, 사유, 원본 시점 | TBD |
| Domain → ESP32 | Vehicle / MOBILE State | MobileStateMapper | MOBILE | 상태 값, source, quality, age, 지원 여부 | TBD |
| Domain → ESP32 | Warning / history | WarningManager | MOBILE | 발생 ID, 등급, 현재/이력, ACK 관계 | TBD |
| Domain → ESP32 | Function Availability | DiagnosticManager / MobileStateMapper | MOBILE | supported/available/reason, freshness | TBD |
| Domain → ESP32 | Digital Key setting/result | DigitalKeyManager / 결과 제공 기능 | MOBILE | ON/OFF 반영, 이용 가능성, 해제 원인/결과 | TBD |

**모든 행에 공통으로 답할 질문:**

- [ ] 신규 수신 때만 전송하는 Event인가, 상태 변화 시 Event인가, 주기 State인가?
- [ ] 해당 값의 실제 **최초 생성 시점**, **ESP32 수신 시점**, **MOBILE 수신 시점**을 어떻게 구분할 것인가?
- [ ] `VALID / INVALID / NO_DATA / STALE / UNKNOWN` 등을 값과 함께 제공하는가? 데이터에 따라 Quality 의미가 다른가?
- [ ] 요청 처리 상태는 `SENT`, 차량 수용 상태, 최종 실행 결과를 어떻게 구분하고, 어느 노드가 생성하는가?
- [ ] Warning은 **발생 ID / 현재 위험 / 사용자의 확인**을 분리하는가? 연결이 끊겼을 때 재연결 후 어떤 이력을 조회하는가?
- [ ] Payload, 필수 필드, Enum 값, Byte 길이, 오류 사유는 누가 정의하고 누가 승인하는가?

**결정 후 반영:** `ESP32_Domain_Logical_Interface_Matrix.md`(추후 생성), 모바일 메시지 계약, Domain 입력/출력 DTO, CAN 연계 요구.

---

## 7. ESP-06, ESP-09 — Timeout / Freshness / 복구 규칙

### 7.1 먼저 구분할 4가지 시간/상태

| 분류 | 설명 | 주 판단 위치 |
|---|---|---|
| Bluetooth / UART **Link Liveness** | 통신 경로 자체를 사용할 수 있는가? | ESP32 LinkStateManager + 해당 Driver/Protocol |
| **정보 Freshness** | 상태/근접/앱 활성의 원본이 아직 유효한가? | Producer 근거 제공, 최종 사용 측에서 판정 |
| **Request 응답 기한** | 앱 전송 이후 차량 수용 및 최종 결과까지 기다릴 기준 | MOBILE 표시 + Domain 요청 처리 계약 |
| **Gateway READY** | 현재 연결·세션 확인, 복구 후 재동기화 완료 여부 | ESP32 Lifecycle + Domain 합의 |

- [ ] Bluetooth 링크 상실 조건과 UART 링크 상실 조건을 서로 다르게 정의할 것인가?
- [ ] 주기 정보가 없는 Event형 메시지에 일반 상태 메시지의 Timeout을 적용하지 않는가?
- [ ] Proximity / App Activity 유효기간은 **최초 관측 시각**과 **원본 정보 경과 시간**으로 계산하는가?
- [ ] UART/Bluetooth 중계 지연에 따라 원본 Age가 소실되지 않도록 어떤 시간 필드를 추가할 것인가? 현재 DTO의 단일 `age_ms`만으로 충분한가?
- [ ] 송신 재시도/재전송이 `is_new_update`나 원본 Freshness를 새 값처럼 바꾸지 않는가?
- [ ] 각 메시지 별로 송신 주기, 최초 예정 도착 기한, Timeout/오류 보고 주체를 정할 것인가?

### 7.2 복구 시퀀스 합의

```text
Bluetooth 또는 UART 상실 / Domain 재시작 감지
        ↓
Link UNAVAILABLE / Gateway READY 해제
        ↓
현재 Session confirmation 무효화, 신규 차량 제어 제한
        ↓
물리 링크 복구 + 실제 Domain/Peer 확인
        ↓
새 연결/Session 근거 확인
        ↓
필요한 상태·결과·경고 이력 재조회 / 동기화
        ↓
SYNC 완료 확인 후 Gateway READY
```

- [ ] 위 단계 중 누가 Sync 시작과 완료를 선언하는가? Domain 측 `GatewaySessionManager`와 어떤 메시지를 교환하는가?
- [ ] Sync 과정에서 허용할 메시지(예: `STATE_QUERY`)와 금지할 새 차량 제어 요청을 어떻게 구분하는가?
- [ ] UART 복구 후 프레임 경계 재탐색 및 구형 RX 버퍼 제거 기준은 무엇인가?
- [ ] 과거 `RequestId` 결과가 재연결 후 도착하면 폐기/조회 이력 중 어느 방식으로 다루는가?
- [ ] 재접속 때 **이전 미완료 Request 자동 Replay 금지**를 MOBILE/ESP32/Domain이 모두 지키는가?

**실측 기준 참고:** SysRS §6.13의 표시/응답/재연결 수치는 **잠정 후보**이며 ESP32 근접 정보와 UART Link Timeout까지 동일하게 적용한다고 결정된 것이 아니다.

**결정 후 반영:** `Network_Config`, `GatewayLifecycleManager`, `LinkStateManager`, `MessageContextManager`, Message DTO, Domain 통신 복구 테스트.

---

## 8. ESP-07 — Digital Key RSSI / Proximity 정책

**현재 기준:** ESP32의 잠정 RSSI 기반 근접 결과는 `NEAR / FAR / UNKNOWN + Quality/Update 근거`이고, **새 접근 판단·최종 Auto Unlock은 Domain** 책임이다.

- [ ] 근접 신호의 실제 수집 방식은 무엇인가? 연결 RSSI를 어떤 이벤트/주기로 읽을 수 있는가?
- [ ] 차량과 휴대폰 실측에서 **NEAR 진입·FAR 이탈 RSSI 후보 범위**를 각각 어떻게 잡을 것인가? 실내/실외·방향·장애물 영향은?
- [ ] RSSI raw → 필터(이동평균/중앙값 등 **후보**) → hysteresis → stable sample 순서를 사용할 것인가?
- [ ] 안정화 기준은 샘플 횟수, 시간 연속성, 둘 다 중 어느 방식인가?
- [ ] `NEAR/FAR` 유효 기간과 누락·약한 신호·INVALID·STALE 처리 기준은?
- [ ] 새 연결이나 다른 peer 전환 시 과거 `NEAR`를 폐기하는가? 원본 관측 시각을 보존하는가?
- [ ] **유효한 FAR → 유효한 NEAR** 새 접근 여부는 Domain이 판단한다는 경계를 유지하는가?
- [ ] 시험용/모의 RSSI를 사용하면 `REAL / MOCK` 출처 구분이 가능한가? 모의를 실검증으로 표시하지 않는가?

**현재 테스트에 있는 RSSI·시간 수치는 Unit Test 샘플이며 실제 Calibration 값이 아니다.**

**결정 후 반영:** `Gateway_PolicyConfig`, `ProximityManager`, Domain `DigitalKeyManager`, 근접 시험 기록표.

---

## 9. ESP-08 — App Activity 입력 계약

**현재 기준:** App Active 정보 제공 필요성은 정의되어 있으나 **실제 Producer는 아직 미정**이다. `AppActivityIngress`는 확인된 단말·세션에서 전달받은 논리 상태를 검증할 뿐 앱이 실제 활성인지 추론하지 않는다.

| 토픽 | 결정해야 할 질문 | 임시 제안 / 현재 상태 |
|---|---|---|
| Producer | MOBILE 앱이 자체 화면 Lifecycle을 보고할까, Bluetooth 계층의 별도 신뢰 근거를 쓸까? | MOBILE 명시적 보고 **제안** |
| ACTIVE 정의 | Android 실제 어떤 화면/Foreground 상태인가? | TBD |
| INACTIVE 정의 | 명시적 비활성 이벤트 기준과 발생 누락 가능성은? | TBD |
| UNKNOWN | 앱 강제 종료, 연결 상실, Session 불일치, 만료 시 어디서 UNKNOWN 처리? | 불명확하면 ACTIVE 금지 |
| 전송 | 전이 이벤트만? 주기적 재확인도 할까? | TBD |
| 보안 | 현재 Bluetooth peer와 현재 Session의 보고라는 근거는? | TBD |
| 시간 | 원본 생성 Age와 ESP32 수신 Age를 나누는가? | 현 DTO 표현 확장 검토 |
| UART | 별도 논리 업데이트를 독립 Frame으로 보낼까, 복합 Context에 포함할까? | NETWORK-TBD |

- [ ] **CONNECTED만으로 ACTIVE를 생성하지 않는다.** Disconnect 역시 INACTIVE로 단정하지 않는다.
- [ ] ACTIVE 보고가 오래된 경우 단순 재Publish만으로 유효시간을 연장하지 않는다.
- [ ] Activity가 UNKNOWN/INACTIVE이면 Domain의 신규 자동 해제 허용 조건에 어떻게 반영되는지 확인한다.

**결정 후 반영:** `docs/App_Activity_입력계약_v0.1.md` 개정, MOBILE 구현, `AppActivityIngress`, Gateway/Domain Context Update.

---

## 10. ESP-11, ESP-14 — MOBILE 상태·Result·경고, WINDOW 연계

### 10.1 MOBILE에 표출할 정보

- [ ] 앱에 보여줄 차량 상태, 동작 진행, 고장, Warning, Function Availability의 **최소 시연 항목**을 결정했는가?
- [ ] `요청 전송됨` / `Domain 수용` / `실행 완료` / `거부·실패` / `확인 불가`를 각각 어떻게 표시하는가?
- [ ] 차량 현재 상태와 이전 요청 Result가 모순되는 것처럼 보일 때 어느 정보가 실제 현재 상태인지 구분하는가?
- [ ] 통신이 끊기면 기존 상태를 현재 정상값으로 표시하지 않고 **연결 상실 / 마지막 관측 시각 / 확인 불가**를 보여주는가?
- [ ] 안전 경고는 일반 State 주기와 별도로 우선 전달하고, 단절 중 미수신 이력은 재연결 후 조회하는가?

### 10.2 WINDOW 연계 시 ESP32가 확인할 내용

**현재 Domain Architecture v0.2에는 `MOBILE → WINDOW 직접 원격 제어`가 `OUT-OF-SCOPE`로 표기**되어 있다. 따라서 이번 회의에서는 ESP32에서 WINDOW 직접 구동 기능을 추가하는 전제가 아니라 다음을 확인한다.

- [ ] 앱에 창문 `위치 / 이동 방향 / 정지 / Anti-Pinch / Fault / 품질` 중 어떤 항목을 **읽기 전용**으로 보여줄 것인가?
- [ ] WINDOW가 생산한 원본 상태를 Domain의 어느 모듈이 모바일용 상태로 매핑하는가?
- [ ] 창문 상태의 주기·만료·fault 분류·안전 경고는 어떤 Field로 전달하는가?
- [ ] 나중에 MOBILE 원격 WINDOW 제어가 범위에 추가된다면 **SR/SysRS 변경을 먼저 할 것인가?** (이번 회의에서 자동 확정하지 않음)

**결정 후 반영:** WINDOW→Domain Logical Interface, `MobileStateMapper`, MOBILE 표시 계약, Gateway 중계 메시지.

---

## 11. ESP-12, ESP-13, ESP-15 — 하드웨어·FreeRTOS·검증

### 11.1 실제 보드 연결

- [ ] 현재 ESP32 보드가 어떤 칩·Revision·UART 핀을 쓰는지 확인했는가?
- [ ] S32K344 UART 포트·Pinmux와 실제 커넥터/TX/RX/GND 배선을 확정했는가?
- [ ] **보드 전원 입력 5V 여부와 UART GPIO 신호 3.3V/5V 레벨은 별개**임을 확인하고 데이터시트·측정으로 상호 연결 가능성을 검증했는가?
- [ ] Debug 콘솔 UART와 Domain 데이터 UART가 충돌하지 않는가?
- [ ] BLE와 UART를 동시 사용하면서 Buffer Overrun/전원 부족/재부팅이 발생하지 않는가?

### 11.2 FreeRTOS Integration

- [ ] Bluetooth callback, UART RX, Link Supervision, Router 이벤트를 **어느 Task/Queue**로 직렬화할 것인가?
- [ ] ISR/BT stack callback에서 공유 Manager를 직접 호출하지 않고 최소 Event를 전달하도록 할 것인가?
- [ ] Request Payload와 Context의 소유/복사 책임, Queue 최대 크기·Overflow 시 처리 규칙은?
- [ ] 경고·상태·요청·연결 이벤트가 동시에 발생하면 어떤 우선순위·처리량을 보장해야 하는가?
- [ ] Queue Overflow와 UART 송신 실패가 **차량 FAILED 결과로 위조되지 않도록** 어떤 진단/앱 표시 경로를 쓸 것인가?

### 11.3 검증 시나리오

| ID | 테스트 상황 | 기대 확인 | 실제 HW 검증 |
|---|---|---|---|
| T-01 | 미등록 peer 연결 | `CONNECTED`이어도 등록된 현재 peer로 인정하지 않음 | ☐ |
| T-02 | 등록 peer 연결, UART 미응답 | 차량 정상 연결/Request 수용으로 표시하지 않음 | ☐ |
| T-03 | 앱 ACTIVE 미보고 / 앱 강제 종료 | 연결 상태를 ACTIVE로 추론하지 않음 | ☐ |
| T-04 | RSSI valid FAR → valid NEAR | 안정화된 근접 보고, Unlock 정책 판단은 Domain | ☐ |
| T-05 | RSSI 누락 / INVALID / 만료 / BT 상실 | `FAR` 위조 금지, `UNKNOWN` 및 품질 제공 | ☐ |
| T-06 | 동일 RequestId 중복 전달 | 새 RequestId 생성·중복 차량 실행 방지 규칙 확인 | ☐ |
| T-07 | UART 파편/연속 Frame/Integrity 오류 | 완전·유효 Message 외에는 전달하지 않음 | ☐ |
| T-08 | UART 연결 중단 후 복구 | 이전 제어 Replay 금지, Session·상태 재동기화 | ☐ |
| T-09 | 오래된 State/Result 뒤늦게 도착 | 최신값 오염/이전 Session 오인 방지 | ☐ |
| T-10 | 등록 해제 직후 재연결 | 새 제어/키 응답 중단, 등록 기록 정리 확인 | ☐ |
| T-11 | Warning 전달 중 Bluetooth 단절 | 가짜 ACK/위험 해제 없이 재연결 시 이력 조회 | ☐ |
| T-12 | ESP32 실제 보드 + Domain UART 양방향 | 핀·전압·Framing·Timeout·RTOS 동시 구동 확인 | ☐ |

> Host 단위 테스트 `PASS`는 위 **실보드/End-to-End** 시나리오 검증을 대신하지 않는다.

---

## 12. 회의 진행 순서 제안

> 제한된 시간에 **의존성이 큰 것부터** 결정하기 위한 순서다. 수치 전부를 한 회의에서 확정할 필요는 없다.

| 단계 | 권장 시간 | 논의 | 반드시 얻을 결과 |
|---|---:|---|---|
| 1 | 5분 | 아키텍처/책임·현재 구현 범위 확인 | Source of Truth / Out of Scope 합의 |
| 2 | 10분 | 등록·현재 연결·Session/Request ID | ID 생성·관리 담당자 지정 |
| 3 | 15분 | Bluetooth·UART Logical Message / Frame | Message Matrix Owner + 프로토콜 초안 담당 |
| 4 | 10분 | Link Loss/Resync/Freshness | 최소 상태전이/시간 설계 방향 |
| 5 | 10분 | Digital Key RSSI/App Activity | 실측·앱 입력 계약 담당과 시험 방법 |
| 6 | 5분 | WINDOW 표시 범위 및 일정 | 범위 확인, 미정항목 Issue 등록 |

### 회의 전 각 담당자 준비물

| 담당 | 미리 준비해올 자료 |
|---|---|
| ESP32 | 현재 Core/Feature 구조, Bluetooth·UART 보드 핀 후보, 이번 결정 리스트 |
| MOBILE | Android 앱 기능 범위, 등록 흐름, 실제 ACTIVE 이벤트 후보, 요청/표시 항목 |
| 네트워크 | UART/BT 메시지 구성 후보, Framing·Integrity, Link 감시·주기 설계 방식 |
| S32K344 Domain | `GatewaySessionManager`/`RequestManager` 입력 요구, Result/State/Warning 출력 필드, Sync 완료 조건 |
| WINDOW | Domain에 제공할 상태/Result/Anti-Pinch/Fault 및 화면 표시 범위 확인 |

---

## 13. 회의 결정 기록 템플릿

회의가 끝나면 아래 표를 채우고, **합의되지 않은 것은 TBD로 그대로 남긴다.**

| Issue ID | 최종 결정 내용 | 상태 (`합의`/`TBD`/`보류`) | 결정 담당 | 영향받는 파일·요구사항 | 목표일 |
|---|---|---|---|---|---|
| ESP-01 |  | TBD |  |  |  |
| ESP-02/10 |  | TBD |  |  |  |
| ESP-03 |  | TBD |  |  |  |
| ESP-04 |  | TBD |  |  |  |
| ESP-05 |  | TBD |  |  |  |
| ESP-06/09 |  | TBD |  |  |  |
| ESP-07 |  | TBD |  |  |  |
| ESP-08 |  | TBD |  |  |  |
| ESP-11/14 |  | TBD |  |  |  |
| ESP-12/13/15 |  | TBD |  |  |  |

**회의 정보**

- 일시:
- 참석자:
- 변경될 SR/SysRS 요구사항 ID:
- 신규 Git Issue/PR:
- 다음 회의 일정:
- 합의되지 않은 주요 리스크:

---

## 14. 회의 후 변경 작업 순서

1. **요구사항과 책임 확정:** SR/SysRS·Domain/ESP32 Architecture의 범위 차이나 담당 충돌 먼저 정리.
2. **논리 정보 표 확정:** Producer/Consumer, Request/State/Event/Fault/Data, 필수 필드, Quality, 시간 근거 정의.
3. **통신 상세 설계:** Bluetooth Profile / UART Protocol / ID·Byte Allocation / Period / Timeout / Recovery.
4. **코드 변경:** `Network_Config` → `BluetoothAdapter`/`UartFrameCodec`/`UartAdapter` → ESP-IDF `Gateway_Task` 연동. 필요 시 기존 Manager DTO 수정.
5. **시험:** Host 단위 테스트 재실행 → 실제 양쪽 보드 Loopback → MOBILE↔ESP32↔Domain End-to-End → 오류/복구 시나리오.
6. **Git 관리:** 이번 문서는 `docs/ESP32/`에 보관, 각 TBD를 Issue로 등록하고 합의된 결정만 별도 PR에 반영.

**코드·문서 변경 시 체크**

- [ ] 임시 값과 최종 합의값 구분 (`TBD`/`PROVISIONAL`/`CONFIRMED`)
- [ ] 요구사항 ID 또는 회의 결정 Issue를 수정 근거로 연결
- [ ] 다른 담당자 소유 인터페이스를 협의 없이 수정하지 않음
- [ ] Source-of-truth 문서 간 서로 다른 범위/이름/필드가 생기지 않도록 확인
- [ ] 기존 Host 테스트 유지 및 새로운 경계·실보드 시험 추가
- [ ] 테스트용 Device ID/Private Key/실제 Bonding Secret을 Git에 커밋하지 않음

---

## 15. 참고한 프로젝트 문서

- **SR v0.46** — `SR(8).md`: 사용자 기능, 모바일 제어·표시, DIGITAL KEY, WINDOW 상위 범위.
- **SysRS v0.47** — `sysRS(7).md`: §2.1.1 등록/연결, §3.2.6 Digital Key, §3.2.8 UART Gateway, §6.13·§6.14 MOBILE 후보/미정, 부록 `A.MOBILE.2~5`.
- **ESP32 Architecture v0.3** — `ESP32_Wireless_Gateway_Software_Architecture_v0.3.md`: §7 Feature, §8 Logical Matrix, §9 요청 중계, §12 Freshness, §13~15 통신 미정, §16 복구, §21 Open/TBD.
- **Domain Architecture v0.2** — `Domain_Software_Architecture_v0.2(1).md`: §2.2 ESP32, §2.5 WINDOW, §11.1 ESP32↔Domain Matrix. 특히 MOBILE→WINDOW 직접 원격 제어는 현재 범위 밖.
- **현재 ESP32 구현** — `ESP32_Gateway_Phase2_v0.1.zip`의 `common/`, `interface/`, `core/`, `service/`, `feature/`, `docs/`, `test/`.

> 이 문서의 선택지는 개발 회의를 위한 질문이다. 확정되지 않은 메시지 형식, 필드 폭, RSSI 숫자, Timeout, UART Pin/Baud Rate를 새 요구사항으로 추가한 것이 아니다.
