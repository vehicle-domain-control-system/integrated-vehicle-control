# S32K344 Domain Application Implementation Plan

- Draft 0.1 / 2026-10-12
- 대상: S32K344 Domain 기능 정책 Application(C)
- 제외: S32K344의 UART/CAN Framer, Codec, Driver, 송수신 큐, Network Adapter 내부, ESP32 BLE/Driver.

## 0. 준비 및 기준

1. 프로젝트 루트에는 `AGENTS.md`, `docs/DOMAIN_ARCHITECTURE.md`, `docs/DOMAIN_INTERFACE_MAPPING.md`, `docs/DOMAIN_IMPLEMENTATION_PLAN.md`를 배치한다.
2. `docs/references/requirements/SR(8).md`, `sysRS(7).md`; `docs/references/network/interface/` 압축해제 전체, Network SW Architecture; `docs/references/digital_key/` Digital Key 명세를 원본으로 보관한다.
3. 네트워크 담당자의 실제 C2 `.h`/`.c`, UART/CAN Network Adapter는 제공받을 때 `external/network/` 또는 별도 저장소/submodule로 연결한다. 사양 문서만 있는 상태라면 mock adapter로 컴파일/테스트한다.
4. S32DS 3.6.6 + S32K3 RTD 7.0.1 개발 환경과 정확한 IDE 프로젝트/보드·컴파일러 옵션을 확인한다. 기존 정상 프로젝트가 있으면 반드시 보존하고, 이를 기반으로 추가한다. FreeRTOS는 실제 배치 결정 전 강제하지 않는다.

## 1. 권장 디렉터리

```text
Domain344/
├── AGENTS.md
├── docs/
│   ├── DOMAIN_ARCHITECTURE.md
│   ├── DOMAIN_INTERFACE_MAPPING.md
│   ├── DOMAIN_IMPLEMENTATION_PLAN.md
│   └── references/
│       ├── requirements/
│       ├── network/interface/02_NODES/DOMAIN/...
│       ├── network/interface/04_COMMON/...
│       ├── network/interface/03_COMMUNICATION/...
│       ├── network/network_sw_architecture.md
│       └── digital_key/
├── app/
│   ├── include/
│   ├── domain_app.c
│   ├── vehicle_state_manager.c
│   ├── request_manager.c
│   ├── command_manager.c
│   ├── result_manager.c
│   ├── digital_key_manager.c
│   ├── climate_manager.c
│   ├── interior_light_manager.c
│   ├── permission_manager.c
│   ├── warning_manager.c
│   ├── vss_event_manager.c
│   ├── settings_manager.c
│   └── availability_manager.c
├── port/ (OS/timebase/DomainIf adapter; Network 구현 중복 금지)
├── tests/ (host mock + tests)
└── [기존 S32DS IDE 메타데이터/RTD generated files]
```

새 파일 이름은 예시일 뿐이며 S32DS 프로젝트의 기존 디렉터리 구조를 존중한다.

## 2. 권장 단계와 완료 조건

| 단계 | 작업 | 단위 테스트/완료 Gate |
|---|---|---|
| D0 프로젝트 검토 | C2 72 선언·26 TxPort·11 observer·18 QueryGet, RTD/S32DS 기존 skeleton 확인 | 변경 없음 분석 보고, 실제/Mock 경계 기록 |
| D1 기본 Build | 기존 S32DS S32K344 프로젝트 확보, App entry + monotonic time + bounded event queue | 빌드 성공, **보드 실행 별도** |
| D2 VehicleStateManager | BCM/CIS/VSS typed 입력, quality/age/snapshot, QueryGet18 제공 | 각 상태 갱신/불명/만료, QueryGetter snapshot coherence |
| D3 Request/Command/Result | Mobile 요청 검증, 도어 명령 ID, duplicate control, BCM result correlation, Mobile result | Request 1회 → Command 1회, local TX OK ≠ DONE, timeout UNKNOWN |
| D4 Basic control | BCM Door + Climate + InteriorLight 명령 정책/설정 처리 | stale·invalid 조건에서 기능별 올바른 거부/보수 대응 |
| D5 Digital Key | Session/registration/proximity 입력 소비, FAR→NEAR FSM, 설정·결과 관리 | 초기 NEAR만으로 Unlock 0, 연결변경·stale에서 unlock 0, 실제 구동은 계약 승인 때까지 차단 |
| D6 Warning/VSS | CIS rear/occupant warning, BCM fault/protection, VSS event와 warning state | occurrence 중복/clear/ack, 실제 원 이벤트만 출력 |
| D7 Permission/Availability | CIS power permission, 통신·기능 가용성, UserSettings | quality·source age 보존, 불명 power면 미허용 |
| D8 Query & status integration | 조회·복구 snapshot, recent results/current warnings, UI 갱신 | getter 18종 및 부분/없음 상태, 중복/레이스 테스트 |
| D9 real Network integration | Network C2 실제 라이브러리 연결, UART-M ↔ ESP32, CAN ↔ BCM/CIS/VSS | typed C2 → 물리 ECU 명령/피드백 E2E, Boot/ID/age 품질 로그 |

초기 고가치 E2E 경로: `M_REQUEST(도어 Unlock)` → `DomainIf_RxMobileRequest` → `RequestManager` → `DomainIf_TxBcmDoorCommand` → BCM Result → `DomainIf_RxBcmCommandResult` → `DomainIf_TxMobileRequestResult`.

## 3. 테스트 매트릭스

### Request / Command / Result
- 새로운 Request(device, session, requestId)에서는 허용 조건 충족 시 명령 **1회**.
- 동일 요청 재수신·Network 재연결로 중복 물리 실행 금지.
- 명령 ID 생성/순환 충돌 방지 정책; 원 RequestID와 별개.
- `DOMAIN_IF_OK`인 경우에도 결과를 `DONE`으로 내지 않는다.
- BCM ACCEPTED / DONE / FAILED / UNKNOWN / 현상태 맞음 결과를 각각 구분.
- BCM 결과의 commandId/kind mismatch, late result, stale source, NetworkLocalFailure, timeout 처리.

### Vehicle State / QueryGet
- 정상/INVALID/STALE/NO_DATA 상태 및 개별 품질 반환.
- `out` buffer 초기화·복사 원자성·동시 업데이트 보호.
- 18 Getter 중 WINDOW는 선언/의미는 유지하되 NOT_AVAILABLE 또는 기존 약속된 미지원 의미로 제한.
- RecentRequestResults capacity=0/1/N, 데이터 없음, Device 별 격리.
- `GetNetworkHealthSnapshot` 의존 기능은 실제 network health 가 없으면 UNKNOWN/NOT_AVAILABLE.
- Query 응답 프레임/QueryEnd는 App 직접 구현 금지.

### Digital Key
- 초기 BLE 연결 NEAR·앱 활성만으로 Unlock 0.
- 실제 FAR→NEAR 새 관측 후 단 한 번 검토; 연결 상실, 출처 세대변경/부트·stale ⇒ 재무장.
- 수동 Lock 발생 시 원 자동 해제 재실행 차단.
- `far_run_ms` wire 미합의 시 실제 자동 Unlock API 호출 0, Mock FSM만 테스트.
- `TxMobileDigitalKeyResult`의 설정 결과와 물리 Unlock 결과 분리.

### Warning / VSS / Availability
- CIS 후방 거리/품질/기능 status, BCM door protection event -> warning.
- one-shot VSS 알림 재전송에 새 occurrence 생성 금지.
- warning ack는 현재 occurrence/boot 대상으로만 적용.
- 원 입력 quality invalid/unknown을 정상 alarm-clear로 만들지 않음.
- WINDOW 고장 미연결을 현재 운영중 fault로 보고하지 않음.

## 4. 빌드 및 실제 보드 절차

- D0에서는 파일 변경하지 않고 기존 S32DS Eclipse 프로젝트(.project/.cproject, SDK/RTD, linker, startup, clock, pins, FS26 watch-dog 등)를 확인.
- D1에서 기존 정상 `main` 경로에 app initialization **한 번만** 등록. 네트워크 초기화 누락/이중 수행 금지.
- S32DS 실제 환경에서 clean build/Debug configuration 확인. 호스트 유닛 테스트는 순수 C 함수를 대상으로 분리.
- UART/CAN 보드 bring-up/전기 설정은 Network 담당자 책임. 보드 통합 전에 GPIO 및 실제 트랜시버 연결 실측 결과를 상호 확인.
- 빌드 성공, Debug main 진입, mock 테스트, 실제 ECU 통합 성공은 각각 다른 Gate로 보고.

## 5. 미확정 사항의 처리

| OPEN | 작업 방침 |
|---|---|
| Domain C2 Network 구현체·Port 연결 | 헤더 기반 Mock 테스트, 실제 Network API 제공 시 연결 |
| UART/CAN Wire candidate | Application 기능 로직은 독립 구현, private codec 직접 개발 금지 |
| O03 CAN command accept 200ms vs 1초 | 환경 값/정책 제한으로 관리, 임의 deadline 확정 금지 |
| O05 Permission shared source quality | 불명 원 power/age는 미허용, 합성 Network 책임 중복 금지 |
| O24 Source CAN↔snapshot join | 원증거 미연결 상태 모바일 projection local NOT_READY/부분 결과 |
| O02 unsupported Wire enums | `UNSUPPORTED`/`UNCONFIRMED`를 정상/FAILED로 강제 cast 금지 |
| DigitalKey far_run evidence | Mock policy 가능, 실제 auto Unlock 비활성 |
| WINDOW | 설계만 유지, 현재 기능/시험 범위 제외 |

## 6. 개발 작업 PR 템플릿

- 변경 범위: Domain Application 1~2 모듈 / 기존 `DomainIf_*` API 변경 여부: 없음.
- 근거: 정확한 문서 파일·절·API 함수.
- 테스트: `host test` 결과 + S32DS build 결과 + board test 결과(미실행이면 명시).
- OPEN: 미확정 의미/안전 제한/팀 결정 필요 항목.

## 7. Definition of Done

- [ ] C2 API 원형 변경 없이 Domain App이 초기화·실행됨
- [ ] 모든 활성 RX가 state/policy 소비자와 연결됨
- [ ] BCM door/climate/interior light Command와 확인된 Result 처리
- [ ] Mobile Request 수용/거부/중복/UNKNOWN/Result 및 Settings
- [ ] QueryGet 18종 기능/보류 상태 포함 일관된 snapshot 반환
- [ ] CIS 후방·환경/탑승자 판단 및 Warning/VSS 연계
- [ ] Digital Key 등록·활성·근접 신뢰도 검증, 실제 auto Unlock은 승인 후 Gate
- [ ] stale/invalid/boot change/network failure시 보수적 처리
- [ ] S32DS build 및 mock tests 보고
- [ ] 실제 Network 구현과 S32K344 ECU 통합 결과를 별도 기록
