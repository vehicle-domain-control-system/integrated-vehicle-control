# STM0 Tick Reader 통합 조건

이 문서는 공개 저장소에 포함되는 `DomainStm0TickReader`의 보드 통합 선행조건과 검증 범위를 기록한다. NXP RTD 구현 및 Configuration Tools 생성 파일은 이 저장소 변경에 포함하지 않는다.

## 보드 빌드 선행조건

- S32K3 RTD 7.0.1이 설치된 S32DS 환경을 사용한다.
- NXP S32 Configuration Tools에서 STM0 설정을 공식 생성한다.
- STM0 입력 Clock은 FIRC 48 MHz이고 Divider는 1이어야 한다.
- STM0 Prescaler 설정값은 0이며, 결과 분주율은 `Prescaler + 1 = 1`이다.
- CH0는 Continuous/Polling으로 설정하고 ISR을 비활성화하며 Notification은 `NULL_PTR`로 둔다.
- OSIF의 Core ID 조회 설정과 RTD가 요구하는 프로젝트 include path를 준비한다.
- `Clock_Ip_Init()`가 성공적으로 끝난 뒤 Tick Reader를 초기화한다.
- 공식 생성 설정과 RTD 소스는 적법한 로컬 설치 또는 별도 제공물로 준비한다.

공개 저장소의 자체 작성 파일만으로는 S32DS 타깃 전체 빌드를 재현하지 못할 수 있다. 다음 파일은 공개 변경에 포함하지 않는다.

- `generate/include/Stm_Ip_*.h`
- `generate/src/Stm_Ip_SA_PBcfg.c`
- OSIF 생성 설정
- 프로젝트 내부 `RTD/`
- `.mex` 및 `.cproject`

## 시간 특성

48 MHz, 분주율 1에서 Counter는 48,000,000 tick/s이고 1 ms는 48,000 tick이다. 32-bit Counter는 약 89.48초마다 rollover한다. `DomainBoardTimebase`의 누적 계산이 모호해지지 않도록 Reader 호출 간격은 한 번의 전체 Counter 주기보다 짧아야 한다.

RTD의 초기화 및 Start API가 `void`이므로 `DomainStm0TickReader_Init()`의 성공은 소프트웨어 선행조건 확인과 RTD 호출 완료만 뜻한다. 실제 Counter 진행, Clock 정확도와 장시간 단조성은 보드에서 별도로 측정해야 한다.

## 후속 Runtime 초기화 순서

1. Clock 초기화
2. STM0 Tick Reader 초기화
3. DomainBoardTimebase 초기화
4. DomainRuntime 초기화
5. 단일 non-ISR owner의 Superloop Poll

`main.c`, Watchdog 서비스, Poll 주기, 초기화 실패 시 안전 상태, Network 초기화 순서는 후속 통합 범위다. 이 변경에서는 Board Flash와 실제 Counter 측정을 수행하지 않았다.
