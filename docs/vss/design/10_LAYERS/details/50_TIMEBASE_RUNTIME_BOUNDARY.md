# Timebase / Runtime — Layer 하위 공통 Boundary 상세

> R1 분류: INFRA Layer 공통 Boundary. 수치·정밀도·처리 예산·실행 문맥은 기존 TBD를 유지한다.

## 1. 공통 근거와 시간 문맥

| 공통 근거 | 소비하는 책임 |
| --- | --- |
| 원본과 비교 가능한 시간·연속성 | STORE age/Hold, PLAYBACK start 결과/정리/정책, Backend·HEALTH 각 기한 |
| 해당 요청·출력 시도에 연결된 사실·후속 처리 알림 | FLOW 처리 기회, Service/Driver의 해당 요청·출력 시도에 귀속된 결과 적용 |
| coalesce/overflow·귀속/연속성 손실 | 해당 owner가 성공·미시작·종료를 추정하지 않도록 손실 전달 |

시간 문맥·연속성·발생/포착/처리 시각은 [Timing §1](../../50_CONTRACTS/30_TIMING_FRESHNESS.md#time-context)을 따른다. Runtime은 비교 가능한 시간/연속성과 불확실성을 제공하며 도메인 age·복구/재생 적법성을 대신 판단하지 않는다.

## 2. 호출과 주요 진행

필요한 APP/SERVICE/DRIVER/HAL이 기존 공통 공개 경계를 사용한다. 새 RUNTIME getter/measurement wrapper를 핵심 함수로 늘리지 않는다. RX 없는 bounded 진행은 [FLOW](../../20_MODULES/10_FLOW_MODULE.md), ISR에서 요청 전에 저장한 귀속 정보로 사실을 포착하는 처리는 [HAL/BSP](40_HAL_BSP_BOUNDARY.md)를 따른다.

알림 자체는 실제 사실이 아니다. 중요한 actual/consume/error가 coalesce/용량 제한으로 사라지면 손실을 관측 가능하게 남긴다. 기존 실행 문맥/도구를 우선하며 새 Queue/Mutex/Task/Cache를 먼저 선택하지 않는다.

<a id="measurement"></a>
## 3. 기존 측정 marker

| 구간 | 사실을 생성한 주체가 남긴 측정 표시(marker) | 연결할 지연 |
| --- | --- | --- |
| 시작 | T0: STORE accepted / PLAYBACK 새 output attempt 채택 중 관측 사실 구별. T1: ASSET lookup/read 시작. T2: AUDIO STREAM(C09)의 첫 decode/PCM. T3: 초기 A/B prime. T4: TX 수락+실제 전송/출력을 위한 장치 활성화(arm). T5: 원래 출력 시도의 장치 활성화 이후 새로 관측한 실제 출력 시작 | T5−T0와 하위 비용 |
| 선점 | P0: SELECT 높은 후보 / PLAYBACK old stop intent의 관측 경계. P1: old 전체 retirement. P2: 새 수락+장치 활성화. P3: 새 actual start | P3−P0 |
| CLEAR | C0: STORE valid CLEAR / PLAYBACK stop intent의 관측 경계. C1: old DMA/SAI 정리. C2: 전체 output termination | C2−C0 |

FLOW는 해당 operation의 marker를 연관할 뿐 처리 현재 시각으로 하위 사실을 재생산하지 않는다. 기존 성능 요구사항(PER-001~010)과 수치/후보 상태를 상속하며, 이 표는 성능 PASS가 아니다.

## 4. 대표 예외와 TBD

시간 비교/귀속·필수 사실 저장 손실은 owner에 그대로 전달한다. timeout 자체는 no-start/end 증명이 아니다. 실제 timebase·포착 지점/정밀도·직렬화/처리 예산과 보호 용량은 [실행 TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md#execution), 물리 출력 상관은 [HW TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md#hardware)를 확인한다.

## 문서 연결과 후속 범위

[INFRA Layer](../50_INFRA_LAYER.md) · [시간 계약 위치](../../50_CONTRACTS/00_CONTRACT_OVERVIEW.md#timing) · [TBD](../../90_BINDING/91_IMPLEMENTATION_TBD.md)

기존 성능 요구사항(PER-001~010)과 marker 의미를 보존했다. 보드 측정이나 성능 PASS를 새로 선언하지 않았다.
