# ESP32 Read-only Monitoring

- 개인 기준: `da982d6cc512302f74fc3bfae1a7724b284c7c53`
- 팀 기준: `develop@abf12cd29229f98abe6a987dd73e568072bd35b3`
- 관련 Issue: #116 / Domain 의존성: #115
- 기존 Open PR #83/#89와 기준선이 겹친다. 이번 PR은 최신 전체 ESP32 소프트웨어 스냅샷이며, 리뷰에서 기존 PR과 통합 순서/대체 여부를 결정해야 한다. 기존 PR을 자동 수정·종료하지 않는다.

## 구현 범위

`main/app_main.c` → P6 Registration/Session → GatewayTask → 검증된 M_CONTEXT 후보 → guarded UART Queue.
Domain 상태는 기존 Parser/Typed Codec → GatewayTask/Core/Mapper → BLE Vehicle Data Notify로 전달한다.

- NimBLE Host에서 실제 연결 descriptor/resolved identity/Bond Store를 재관측한다.
- Timer 실행이나 반복 송신만으로 Source Sequence/AGE를 갱신하지 않는다.
- 기본 빌드는 Monitoring 비활성. 명시적 Monitoring profile만 제한적인 read-only M_CONTEXT 송신을 연결한다.
- M_CONTEXT 갱신/재관측 목표 200ms, Registration/Connection Source TTL 600ms, APP_STATE TTL 1000ms, Domain lease 750ms.
- Queue 대기 AGE 보정, 만료·Session/Generation/Epoch/Boot/Link 변경 시 미제출 후보 폐기.
- 이미 Driver에 제출된 바이트는 취소할 수 없다. UART enqueue/Driver 수용/Domain 수용/Android 수신은 별도 결과다.
- Proximity UNKNOWN/NO_DATA, ESP SessionReady=false 유지.
- P6_PRODUCTION_BINDING_ENABLED=0 및 GATEWAY_CORE_PRODUCTION_ROUTING_ENABLED=0 유지.
- M_REQUEST/M_QUERY/Warning ACK, 차량 제어, Smart Access/Auto Unlock은 활성화하지 않는다.
- 물리 UART 위조 방어가 없는 저전압 시연용 단일 ESP32 신뢰 모델이다.
- Registration/NVS 오류 및 Reset UNKNOWN의 영속성 한계는 `docs/P6_REGISTRATION_SESSION_DESIGN.md`, `docs/OPEN_ISSUES.md` 참조.

## 빌드

ESP-IDF v5.5.5 환경에서 이 디렉터리를 프로젝트 root로 사용한다.

```powershell
# 기본 설정. 기존 사용자 sdkconfig는 덮어쓰지 않는다.
idf.py -B build_default -D SDKCONFIG=sdkconfig.team.default build

# 명시적 Monitoring profile. 보드 Flash는 별도 승인 및 전기/파티션 확인 후 수행한다.
idf.py -B build_monitoring -D SDKCONFIG=sdkconfig.team.monitoring -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.p6_monitoring.defaults" build
```

`sdkconfig.defaults`와 `sdkconfig.p6_monitoring.defaults`는 재현 입력이다.
개인 sdkconfig, build/cache, BIN, NVS/Bond 백업은 저장소에 포함하지 않는다.
기본 Partition/NVS와 보드 Flash 크기의 호환성은 실제 Flash 전 별도 검증한다.
개발 승인/NVS 시험 profile은 기본 비활성이며 일반 Monitoring 운용에 사용하지 않는다.

## Host 검증

CMake/Ninja와 C11 지원 compiler를 사용한다. Windows MinGW의 긴 경로에는
`-DCMAKE_OBJECT_PATH_MAX=200`을 사용하거나 짧은 로컬 build 경로를 선택한다.

```powershell
cmake -S tests/gateway_adapter -B build_host_gateway -G Ninja -DCMAKE_OBJECT_PATH_MAX=200
cmake --build build_host_gateway
ctest --test-dir build_host_gateway --output-on-failure

# DOMAIN_PROJECT: 검토된 Domain 프로젝트 root (src/, network/ 포함)
# DOMAIN_FIXTURE_DIR: 기존 Domain tests/host 디렉터리. 외부 프로젝트는 읽기 전용.
cmake -S tests/monitoring_interop -B build_host_monitoring -G Ninja -DCMAKE_OBJECT_PATH_MAX=200 "-DDOMAIN_PROJECT=$env:DOMAIN_PROJECT" "-DDOMAIN_FIXTURE_DIR=$env:DOMAIN_FIXTURE_DIR"
cmake --build build_host_monitoring
ctest --test-dir build_host_monitoring -R '^monitoring_runtime_interop$' --output-on-failure
```

Monitoring CMake는 Gateway fixture를 EXCLUDE_FROM_ALL로 재사용한다.
그 fixture의 미빌드 보조 테스트까지 실행하지 않도록 위 단일 타깃 필터를 사용한다.
Domain #115의 승인된 소스/fixture를 준비해야 하며 개인 PC 경로는 요구하지 않는다.

## 검증 기록 — 2026-11-25

- 이번 팀 레이아웃 실행: Gateway Host CTest 6/6 PASS, Monitoring Host E2E 1/1 PASS. 최종 compiler warning/error 0.
- 단일 E2E: P6 fixture → 실제 UART Queue/AGE/Encoder → 실제 Domain Decoder/Monitoring Provider → CIS Environment/Rear Publisher → Network Encoder → ESP32 Parser/Core/Mapper → BLE Notify Mock.
- 실제 센서·P6/Bond/NVS/시계 입력과 물리 Driver/Notify는 Host fixture다. 실제 Android 수신 성공이 아니다.
- E2E 외부 소스는 기존 로컬 Domain Production과 Host fixture를 읽기 전용으로 사용했다. #115 원격 트리 자체를 checkout하여 실행한 것은 아니다.
- ESP-IDF v5.5.5/esp32 Monitoring build PASS는 기존 최종 검증 결과 재사용. 해당 검증 Worktree의 소스·테스트·설정 108개가 이번 개인 main과 줄바꿈을 제외하고 일치한다.
- 기존 앱 크기 0x89400 / app partition 0x100000. 팀 경로에서 ESP-IDF를 새로 빌드하거나 Flash하지 않았다.
- Android `function-mobile@8054b69` MessageCodec와 CIS Occupant 45B의 offset 36/38–43 정적 호환 확인. offset 44 function status는 기존 앱이 읽지 않는다. Android 실제 수신/파싱은 NOT_VERIFIED.
- Android VdcsConfig의 heartbeatMs 기본값 1000ms는 APP_STATE TTL 경계와 충돌한다. 앱 담당자는 실제 foreground 기반 500ms 목표 및 적절한 tick을 적용하고 지연/중단 시 만료 동작을 검증해야 한다. ESP32가 화면 상태를 추정하지 않는다.
- 실제 UART/BLE E2E 및 보드 전기·레벨 변환·재시작·파티션 호환성: BOARD_NOT_TESTED.

Domain #115 Provider/Owner/Hardware Gate 준비, Android heartbeat 및 실제 Notify 표시,
물리 UART 전압·배선과 양단 Flash/통신 검증은 별도 통합 Gate다. 이 PR은 자동 Merge하지 않는다.
