# S32K344 Domain Application ↔ Network C2 API Mapping

- Draft 0.1 / 2026-10-12
- 기준: `interface/02_NODES/DOMAIN/20_API.md` 및 타입/의미 문서. 이 문서는 **Network wire codec 명세가 아니라 Application 호출 계약**이다.

## 1. 출처 구분

| 층 | 문서 | 책임 |
|---|---|---|
| 기능 요구 | `SR(8).md`, `sysRS(7).md`, Digital Key v0.1(제안) | Domain 상태·정책·허용·위험·결과 |
| 공개 C2 | `DOMAIN/11_VEHICLE_TYPES.md`, `12_DOMAIN_TYPES.md`, `20_API.md` | Application 자료형, 송수신, Getter |
| Network private | `DOMAIN/25_PRIVATE_ROUTING.md`, `14_MOBILE_CODEC_DATA.md` | TX Port binding, CAN↔UART origin join, QueryEnd/Alive |
| Wire | `10_CAN_FD.md`, `20_UART_M.md`, Enum wire mapping | **타 네트워크 담당자**가 구현 |

## 2. 애플리케이션 RX 매핑

| C2 입력 | Domain 내부 소비자 | 정책 주의 |
|---|---|---|
| `DomainIf_RxMobileRequest` | RequestManager → Climate/Light/DigitalKey/Command | peer/session/request ID 확인, 중복 명령 금지 |
| `DomainIf_RxWarningAck` | WarningManager | occurrence/domain boot 불일치 시 미확인 처리 |
| `DomainIf_RxDigitalKeyConnection` | DigitalKeyManager | 연결=등록/앱 활성/근접 아님 |
| `DomainIf_RxProximity` | DigitalKeyManager | INVALID/STALE를 FAR로 변환 금지 |
| `DomainIf_RxBcmDoorState` | VehicleStateManager, DigitalKeyManager | 실제 잠금 상태와 목표를 구별 |
| `DomainIf_RxBcmClimateState` | VehicleStateManager, ClimateManager | 설정과 실제 팬/온도 상태 구별 |
| `DomainIf_RxBcmInteriorLightState` | VehicleStateManager, InteriorLightManager | 밝기·색상 적용과 명령 수용 구별 |
| `DomainIf_RxBcmCommandResult` | ResultManager, CommandManager | commandId·kind·confirmation 근거 검증 |
| `DomainIf_RxBcmStatus`, `RxBcmProtectionEvent` | AvailabilityManager, WarningManager | fault, local protection, health |
| `DomainIf_RxCisStatus`, `RxCisOccupant`, `RxCisCabinEnvironment`, `RxCisRear`, `RxCisFault` | VehicleStateManager, ClimateManager, WarningManager | 관측마다 독립 품질·AGE, 예전 샘플 재활성화 금지 |
| `DomainIf_RxVssState`, `RxVssFault` | VehicleStateManager, AvailabilityManager | VSS 실행 상태와 명령 구분 |
| `DomainIf_RxNetworkLocalFailure` | ResultManager, AvailabilityManager, Diagnostics | local 인계 후 실패를 차량 DONE/거부로 오인 금지 |
| `DomainIf_RxWindow*` | WindowManager (보류) | callback 선언 유지, 실제 제어 비활성 |

**주의**: `DomainIf_Rx...`는 주로 Network→C2 호출이다. Domain App이 이를 직접 '읽으러 호출'하는 방식으로 설계하지 않는다. Network가 C2 커밋한 뒤 observer/event를 어떻게 App에 연결하는지 실제 구현체 계약 확인.

## 3. 애플리케이션 TX 매핑

| Domain 로직 | C2 TX | 완료 조건 |
|---|---|---|
| Door lock/unlock | `DomainIf_TxBcmDoorCommand` | BCM 결과 + 해당 commandId + 실제 state/confirmation 근거 |
| Climate target/manual/auto | `DomainIf_TxBcmClimateCommand` | BCM의 기능별 결과·상태 근거 |
| Interior light | `DomainIf_TxBcmInteriorLightCommand` | BCM 조명 적용 결과/상태 |
| Sensor power permission | `DomainIf_TxCisPowerPermission` | 허용 수명과 원 입력 freshness 근거 |
| VSS sound event | `DomainIf_TxVssOneShotEvent` | VSS 기능 owner 상태/피드백 (CAN TX ≠ 출력) |
| VSS warning state | `DomainIf_TxVssWarningState` | 현재 경고 의미/원 근거 |
| Mobile Request result | `DomainIf_TxMobileRequestResult` | 원 Request와 Domain outcome을 연결 |
| Mobile warning | `DomainIf_TxMobileWarning` | 발생/해제, occurrence, ack 상태 |
| Mobile availability | `DomainIf_TxMobileFunctionAvailability` | 지원·상태·품질·사유 |
| Mobile Digital Key | `DomainIf_TxMobileDigitalKeyState`, `DomainIf_TxMobileDigitalKeyResult` | 설정/자동 해제 실행 결과 구분 |
| Mobile current state | `DomainIf_TxMobileBcmDoorState`, `TxMobileBcmClimateState`, `TxMobileBcmInteriorLightState`, `TxMobileBcmStatus`, `TxMobileCisEnvironment`, `TxMobileCisOccupant`, `TxMobileCisRear`, `TxMobileCisStatus`, `TxMobileVssStatus` | 실제 source Snapshot + quality, no fabrication |
| Mobile settings | `DomainIf_TxMobileUserSettings` | 저장/수락 여부 실제 의미 |
| WINDOW | `DomainIf_TxWindow*`, `TxMobileWindow*` | 설계 유지, 기능 활성 금지 |

TX 함수의 반환값 `DOMAIN_IF_OK`는 '안정적 로컬 인계'다. 실제 CAN 전송/BCM 수용/완료가 아니다. 송신 실패·비가용은 기록하되 무근거 재시도로 물리 중복 실행하지 않는다.

## 4. Snapshot/QueryGet 18종

- Getter: BCM Door / Climate / InteriorLight / BCM Status; CIS Environment / Occupant / Rear / Status; VSS Status; Window State / Window Fault(보류); UserSettings; FunctionAvailability; DigitalKeyState; CurrentWarning; RequestResult; RecentRequestResults; LastDigitalKeyResult.
- 원형·인자는 `20_API.md`가 정본. `out` 메모리는 호출자 소유이고 getter 실패시 output을 정상값으로 가정하지 않는다.
- Query는 Network가 `M_QUERY`를 파싱하고 Getter를 호출한 후 `M_QUERY_END`를 생성한다. Application이 UART QueryEnd Frame/CRC를 직접 조립하지 않는다.
- Network의 원 CAN↔C2 Snapshot association(`O24`)이 완성되지 않은 projection은 원 BOOT/TX/AGE를 가짜로 합성하지 않는다. 그 projection만 `NOT_READY`/부분 결과로 제한.

## 5. ID·AGE·결과 소유

| 키 | 생산자 | Domain 책임 |
|---|---|---|
| DeviceContextId / SessionId | ESP32 | 입력 검증 후 해당 context에 요청/결과 bind. 재접속 후 이전 세션과 구분 |
| RequestId / QueryId | MOBILE | 원 값 보존, Command ID와 혼동 금지 |
| commandId | DOMAIN 기능 결정 | 새 기능 명령에 유일한 u32 발급; 원 명령 재전송에는 동일 ID |
| decisionSequence | DOMAIN 내부 평가 | 내부 판정 출처와 증가 타이밍 관리 |
| CAN/UART TxSequence / Boot | Network | App이 임의 생성·변경 금지 |
| Source updateSequence, quality, age | 각 ECU 관측 producer | 유효 조건 확인·Snapshot 보존·stale 평가 |
| OccurrenceId | 사건 owner | Warning/보호 이벤트 반복 및 ACK 대상 추적 |

Network node communication의 wire correlation과 Domain App의 **의미적 Request→Command→Result 기록**을 분리한다. 중복 소유를 만들지 않는다.

## 6. 구현에서 보류할 불일치

1. **Digital Key**: `far_run_ms` 연속 FAR 증거 ↔ UART-M `M_CONTEXT`38B 계약 불일치. 기능 state machine+Mock 테스트 가능, 실제 자동 Unlock 비활성.
2. **BCM 실행 결과 확인**: `BCM_RESULT`의 confirmation을 임의로 확정하지 않는다. 부족한 근거는 `UNKNOWN`.
3. **C2 raw enum→Wire**: `UNSUPPORTED`, `UNCONFIRMED`, 일부 ResultReason 매핑 손실; unsupported 표현을 정상으로 치환 금지.
4. **명령 deadline**: CAN 200ms 후보와 기존 SysRS 1초 충돌. 구성값으로 분리; 확정 전 구동 보장 주장 금지.
5. **Permission**: input quality/AGE 공통 합성 및 power operation source OPEN. 필요한 신뢰 입력이 없으면 허용하지 않는다.
6. **VSS / WINDOWS**: VSS 일부 원 데이터/세대 변환 OPEN; WINDOW 구현 보류, EXTERIOR_LIGHT 제외.

## 7. Module API → Network API 단위 테스트 예시

- `RequestManager`에 `MobileRequest`를 넣고 정책 허용 후 `DomainIf_TxBcmDoorCommand` Mock이 **한 번** 호출됐는지 검사.
- `RxBcmCommandResult` 확인 없이 `TxMobileRequestResult(DONE)` 금지.
- stale CIS 입력을 받은 경우 Climate AUTO 및 위험 판단이 해당 quality를 유지하는지 검사.
- QueryGet Snapshot이 실제 마지막 accepted 원 관측과 같은 generation을 사용하는지 검사.
- Device/Session 변경/NetworkLocalFailure 시 pending 요청이 중복 실행되지 않는지 검사.
