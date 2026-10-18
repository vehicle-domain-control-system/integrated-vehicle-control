# S32K344 Domain Application Software Architecture

- 버전: Draft 0.1 / 2026-10-12
- 대상: S32K344 Domain Controller, C, S32 Design Studio 3.6.6 / S32K3 RTD 7.0.1(프로젝트 개발 환경 기준, 현장 설치 확인 필요)
- 책임: **Domain Application/Policy 전부 구현**. CAN FD/UART-M Driver·Framer·Codec·Network Adapter는 네트워크 담당자가 제공하는 `DomainIf_*` 구현과 연결한다.
- 참조 정본: `SR(8).md`, `sysRS(7).md`, `interface/02_NODES/DOMAIN/{00_OVERVIEW,10_DATA,11_VEHICLE_TYPES,12_DOMAIN_TYPES,20_API,25_PRIVATE_ROUTING}.md`, `network_sw_architecture(3).md`, `Digital_Key_Auto_Unlock_구현명세_v0.1(2).md`(제안), `06_OPEN_FOR_TEAM.md`.

## 1. 시스템 및 담당 경계

```text
MOBILE -- BLE --> ESP32 -- UART-M v2 --> [Domain Network: 타 담당자]
                                             |
                                        DomainIf_* C2
                                             |
[Domain Application: 본인]
  RequestManager | VehicleStateManager | DigitalKeyManager | ClimateManager
  InteriorLightManager | PermissionManager | WarningManager | VssEventManager
  ResultManager | AvailabilityManager | SettingsManager | PolicyCoordinator
                                             |
                                        DomainIf_Tx*
                                             |
                            [Domain CAN Network: 타 담당자]
                                             |
                               BCM / CIS / VSS (WINDOW 보류)
```

Network는 frame CRC, CAN ID, UART TYPE, TX_SEQUENCE, RX 검증, Query stream/`M_QUERY_END`, raw source provenance linkage, network health 전송을 담당한다. Domain App은 **수신된 의미 데이터의 사용 가능 여부·차량 정책·명령 intent·결과 의미와 snapshot**을 소유한다. `DomainIf_Tx...()` 로컬 OK는 **ECU 실행 완료 아님**.

## 2. 모듈 책임

| 모듈 | 입력 | 관리 상태·결정 | 출력 API/내부 결과 |
|---|---|---|---|
| `VehicleStateManager` | `RxBcmDoorState`, `RxBcmClimateState`, `RxBcmInteriorLightState`, CIS/VSS/BCM health | 현재 스냅샷, 품질·유효성·최신성, 원 상태 기록 | 18종 `DomainIf_QueryGet...()`에 상태 제공; 필요시 `TxMobile...State` |
| `RequestManager` | `RxMobileRequest` | 원 Device/Session/RequestID, 요청 종류·인자, 허용·거부, 명령 진행상태 | PolicyManager, ResultManager, `TxMobileRequestResult` |
| `CommandManager` | 허용된 기능 intent | `commandId` 발급(도메인 소유), `decisionSequence`, 명령 origin·age, command↔request 연결 | `TxBcmDoorCommand`, `TxBcmClimateCommand`, `TxBcmInteriorLightCommand` |
| `ResultManager` | `RxBcmCommandResult`, 상태 관측, local send fail | ACCEPTED/완료/실패/UNKNOWN 증거 구분, bounded 최근 결과 | `TxMobileRequestResult`, QueryGetRequestResult/RecentResults |
| `DigitalKeyManager` | `RxDigitalKeyConnection`, `RxProximity`, App 활성 근거, 도어 상태 | 설정, 접근(FAR→NEAR) state machine, 동일 접근 1회, 수동 Lock 우선 | BCM Unlock 명령 **허용 조건 충족 시에만**, `TxMobileDigitalKeyState/Result` |
| `ClimateManager` | 온습도·탑승자/차량 허용·모바일 공조 요청 | Manual/AUTO 목표, 제약·상태 판단 | `TxBcmClimateCommand` |
| `InteriorLightManager` | 사용자 설정, 조도/탑승자 입력 | 조명 ON/OFF, 밝기·RGB, 자동 정책 | `TxBcmInteriorLightCommand` |
| `PermissionManager` | 차량 power/use 상태·CIS 상황, quality | `CisPowerPermission` 및 공통 허용 근거; 불명 입력 보수적 제한 | `TxCisPowerPermission`, 승인된 기존 C2 경로 |
| `WarningManager` | CIS rear/occupant, fault, BCM 이벤트 | 경고 발생·해제·Occurrence ID, 읽음 ACK, 유효성 | `TxMobileWarning`, `TxVssWarningState` |
| `VssEventManager` | 도어 잠금 결과, 경고 정책, 동작 결과 | 알림음 사건 중복 방지·우선순위 입력 | `TxVssOneShotEvent`, `TxVssWarningState` |
| `SettingsManager` | 모바일 설정 | 사용자가 승인한 설정과 현재 확정 상태 | `TxMobileUserSettings`, QueryGetUserSettings |
| `AvailabilityManager` | 기능 상태·Network health·입력 품질 | 항목별 지원/가용/불가 사유 | `TxMobileFunctionAvailability`, QueryGetFunctionAvailability |
| `DomainApp` | Network C2 callbacks, monotonic time | 초기화/이벤트 분배·순서 보장, 테스트 Hook | 위 모듈 호출 및 상위 통합 |

명칭은 **Application 내부 제안 파일 이름**이다. 공개 `DomainIf_*`, `Vehicle_Types_v0.2.h`, `Domain_Interface_v0.5_C2_target.h` 선언을 임의로 바꾸지 않는다. 현재 문서의 API는 72종 및 TxPort callback 26종, RxObserver callback 11종, QueryGet 18종이 기준이다.

## 3. C2 API 호출 계약

- App은 `DomainIf_Tx...`로 명령/상태/결과를 송신한다. 상대 BCM 함수, CAN driver나 private `DomainNetPriv_*`를 호출하지 않는다.
- Network가 `DomainIf_Rx...`에 typed 입력을 전달한다. App에서 직접 `DomainIf_Rx...`로 가짜 네트워크 메시지를 만들지 않는다(단위 테스트는 명시적 테스트 Harness로만).
- `DomainIf_SetRxObserver`는 기존 인터페이스의 post-commit 소비 통지를 연결하는 선택적 Observer다. 실제 App 이벤트 전달 주체와 초기화 순서는 제공되는 구현체 기준으로 검증한다.
- `DomainIf_SetTxPort`는 Network Adapter가 사용할 TX 포트 바인딩이며 실제 **누가 등록하는지** 통합 계약에 따라 결정한다. Application이 Network 구현을 중복 작성하지 않는다.
- `DomainIf_QueryGet...` 18종은 Domain의 로컬 캐시 Snapshot Getter다. Network가 M_QUERY를 파싱/종결한다. Getter는 조회시점 데이터의 품질·age를 유지하고 stale 값에 임의 fresh 부여 금지.
- `DomainIf_ValidatePassiveQueryContext`에서 읽기 전용 Query 문맥 검사와 물리 명령 수용 판단을 혼동하지 않는다. 부팅 초기 `SESSION_READY=0`이더라도 정의된 passive Query 흐름을 방해하지 않는다.
- API 입력 포인터는 콜백 동안만 유효한 borrowed pointer. 후속 처리시 bounded deep copy; out getter 객체는 성공 때만 유효. No ISR-level business policy.

## 4. Request → BCM Door Result 경로

1. Network `DomainIf_RxMobileRequest()` → C2 검증 및 Domain App 수신 event.
2. RequestManager: 등록·세션 현재성, RequestID 중복 및 요청 유효성, 충돌 정책 결정.
3. CommandManager: 도어 intent 작성, 도메인 `commandId` 생성(원 요청과 연결); 임의 새 RequestID 생성 금지.
4. `DomainIf_TxBcmDoorCommand(&command)` 제출 후 반환 상태 기록. `DOMAIN_IF_OK`는 Network local 인계에만 해당.
5. Network → BCM CAN FD 송신 및 BCM 결과 수신 → `DomainIf_RxBcmCommandResult()`.
6. ResultManager: 원 `commandId`/대상/상태 결과 확인. 보장된 confirmation 없이 DONE 선언 금지; 미확인=UNKNOWN.
7. `DomainIf_TxMobileRequestResult()` 전송. 현재 Session과 **원 요청의 과거 Session**은 서로 구분.

## 5. 상태와 이벤트의 독립성

- 상태(State)는 마지막 원 평가 값 및 provenance, Event는 새 occurrence, Result는 특정 Request/Command 완료 여부. 그 세 가지를 `VehicleState_t` 한 구조체 변화로 대체하지 않는다.
- `SignalMeta_t`의 quality/source age/update sequence를 보존. 네트워크의 새 TX frame sequence만으로 새 원 관측/age0을 만들지 않는다.
- 매니저에서 시간은 monotonic clock 기준(32-bit overflow 처리). 정의되지 않은 timeout은 정책 config + OPEN 표식으로 둔다.
- QueryGetter는 동일한 owner context에서 복사하여 reader/writer race를 막는다. ISR 직접 사용 금지.
- VSS는 최종 음향 실행/중재 owner. Domain은 **확정 이벤트·위험 의미**만 전달한다.

## 6. Digital Key 상태 머신

`UNAVAILABLE → WAIT_VALID_CONTEXT → WAIT_FAR_ARM → ARMED_FAR → NEAR_CANDIDATE → UNLOCK_PENDING → (CONFIRMED / UNKNOWN / REARM_WAIT)` 등은 **Application 설계 예시**이며 실제 공개 enum 변경이 아니다.

- BLE CONNECTED만으로 등록·APP_ACTIVE·FAR 근거로 취급하지 않는다.
- 초기 또는 재연결 직후 NEAR만으로 Unlock하지 않는다. 신뢰되는 원 FAR→NEAR 관측과 동일 접근 최대 1회, 수동 LOCK 우선, Door feedback confirmation 필요.
- Digital Key v0.1의 제안 `far_run_ms` 증거는 현재 UART-M `M_CONTEXT` 38B에 직접 대응하는 필드가 없다. 양 끝 계약 합의 전 **실제 Unlock 자동 구동 비활성/Mock only**.
- RSSI 임계값/TTL/샘플 수와 같은 제안값을 새 사실로 취급하지 않는다. Fail-closed.

## 7. Runtime / FreeRTOS 권고 (미확정)

S32K344 프로젝트의 실제 RTD/FreeRTOS 사용여부·Network 쪽 Task 계획이 확인될 때 선택한다. FreeRTOS가 필수로 확정된 상태가 아니면 먼저 **이벤트 처리 loop + monotonic Tick() 기반 순수 C Application**을 구현하고, 나중에 Task 적응 계층을 연결한다.

```text
Network ingress (its task) → bounded App EventQueue (value copy)
 → DomainPolicy owner context → State/Request/Command/Result Managers
 → DomainIf_Tx* (non-blocking local submit)
Network Query callback → immutable Snapshot copy (short critical section)
```

네트워크 ISR 또는 이벤트 Callback 안에서 대기·CAN 송신 드라이버 호출·오래 걸리는 정책 계산을 하지 않는다. 한 ID의 처리 순서를 유지하고 race/reentrancy를 테스트한다.

## 8. 범위 및 리스크

- 현재 WINDOW는 **[구현 보류 — 설계 유지]**, EXTERIOR_LIGHT는 제외. interior lighting은 유지.
- `DOMAIN_PERMISSION` 입력 원 품질·합성, BCM ACCEPT_LIMIT 200ms vs SysRS 1s, Network C2 origin association, VSS 세부 mapping 및 unsupported enum은 `06_OPEN_FOR_TEAM`의 미정사항이다. 가짜 policy OK·DONE·원 출처 생성 금지.
- 구현체를 제공받기 전 `DomainIf_*`는 **동일 공개 선언을 따르는 mock/stub adapter**로 호스트 단위 테스트. 실제 통신 시험 성공을 주장하지 않는다.
